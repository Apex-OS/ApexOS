// ApexKernel/Core/SMP/Startup/AppProcessorStartup.c
// ApexOS -- SMP bring-up: waking APs (see AppProcessorStartup.h)

#include "Core/SMP/Startup/AppProcessorStartup.h"
#include "Core/Acpi/Madt.h"
#include "Core/SMP/APIC/Lapic.h"
#include "Core/SMP/PerCPU/PerCpu.h"
#include "Core/Interrupts/IDT/IDT.h"
#include "Core/Synchronization/Atomic/Atomic.h"
#include "Core/Synchronization/Mutex/Mutex.h"
#include "Core/Synchronization/Semaphore/Semaphore.h"
#include "Core/Memory/Heap/Heap.h"
#include "Core/Time/PITCounter.h"
#include "Core/Boot/Log/BootLog.h"

// Exported by ApBootTrampoline.asm -- the raw bytes between these two
// labels are exactly what gets memcpy()'d to AP_TRAMPOLINE_ADDR below,
// nothing more. Declared extern uint8_t (not a function pointer) since
// this kernel never calls through these symbols directly -- only ever
// takes their addresses to compute a byte count and a source pointer
// for the copy, see ap_startup_begin().
extern uint8_t ap_trampoline_start[];
extern uint8_t ap_trampoline_end[];

// Defined in Arch/x86_64/ContextSwitch.asm -- jumps directly into a
// process via iretq, no prior context to save. Used below by
// ap_c_entry() to launch a process the BSP assigned to this AP via
// Core/SMP/IPI/Ipi.h's ipi_send_process() -- see that file's own
// comment for the full Phase 6.3 mechanism.
extern void proc_iretq_start(cpu_context_t *ctx);

// Each AP gets its own dedicated stack, allocated from the heap (see
// ap_startup_wake_one()) -- this is the size of that allocation.
// 16KB matches Core/Tasking/Process/Process.c's own per-process kernel
// stack size, which has proven sufficient for every kernel-side call
// depth this codebase reaches (interrupt handlers, AXFS/AHCI/network
// stacks all nested at once, worst case) -- an AP's own idle/scheduler
// stack has no reason to need more than any of those already do.
#define AP_STACK_SIZE (16 * 1024)

// Phase 6a validation (Core/Synchronization/Atomic, Core/Synchronization/
// Mutex): every core -- BSP included, see Init.c -- increments this
// shared counter via atomic_inc32() and briefly holds this shared
// mutex, proving both actually serialize/synchronize correctly across
// real concurrent CPUs rather than just compiling. See
// ap_c_entry()'s own use of these below.
uint32_t g_smp_test_counter = 0;
mutex_t g_smp_test_mutex = MUTEX_INIT;

// Phase 6b validation (Core/Synchronization/Semaphore): initialized to
// 2 available units in Init.c (sem_init() call, must run before
// ap_startup_begin() -- see that file). With 4 real CPUs (BSP + 3 AP)
// all racing to claim it via sem_try_wait() in ap_c_entry() below and
// Init.c's own BSP-side attempt, exactly 2 across the whole system
// should succeed and 2 should fail, regardless of which specific CPUs
// win.
semaphore_t g_smp_test_semaphore;

// Sends one IPI and spins until the LAPIC hardware confirms it has
// actually gone out (see Lapic.h's lapic_wait_ipi_sent()) before
// returning -- every single IPI in the INIT-SIPI-SIPI sequence below
// goes through this, never a bare lapic_send_ipi() on its own.
static void send_ipi_and_wait(uint8_t dest_apic_id, uint32_t command_bits)
{
    lapic_send_ipi(dest_apic_id, command_bits);
    lapic_wait_ipi_sent();
}

// Busy-waits for approximately `ms` milliseconds using the PIT tick
// counter (Core/Time/PITCounter.c, 100 ticks/sec once pit_init() has
// run -- see Init.c, which calls it well before ap_startup_begin()).
// Local to this file rather than a general-purpose kernel delay
// function: the INIT-SIPI-SIPI timing below is the only place in this
// codebase that needs a plain millisecond busy-wait rather than
// event-driven waiting (contrast Drivers/Storage/AHCI/PortControl.c's
// timeout loops, which poll a condition rather than waiting a fixed
// duration unconditionally).
static void busy_wait_ms(uint32_t ms)
{
    uint64_t start = pit_get_ticks();
    uint64_t target_ticks = ((uint64_t)ms * PIT_TARGET_FREQ) / 1000;
    if (target_ticks == 0)
        target_ticks = 1; // Always wait at least one tick (10ms) rather than a no-op for very small ms values.

    while (pit_get_ticks() - start < target_ticks)
        __asm__ volatile("pause");
}

// Runs on the AP itself, in 64-bit mode, with its own dedicated stack
// -- the very first C code this AP ever executes (see
// ApBootTrampoline.asm's ap_long_mode_entry:, which calls this via
// ap_trampoline_data_t.c_entry_addr). `cpu_index` matches
// g_madt_info.cpu_apic_ids[]'s indexing (see AppProcessorStartup.h).
//
// After the Phase 5/6a/6b validation calls below, this AP idles in a
// hlt loop -- UNLESS Core/SMP/IPI/Ipi.h's ipi_send_process() later
// assigns it a real process (Phase 6.3): see the IPI handler this
// idle loop is interrupted by, IpiDispatch.c's ipi_dispatch(), which
// checks this CPU's own current_proc on every IPI receipt and jumps
// into it via proc_iretq_start() if one is present.
static void ap_c_entry(uint32_t cpu_index)
{
    // This AP's own LAPIC -- same function, same address, different
    // physical hardware than the BSP's call to it in Init.c (see
    // Lapic.h's own header comment on why this is always safe: each
    // core's LAPIC MMIO access always targets that core's own LAPIC).
    lapic_init();

    // Sets up this AP's own slot in the shared per-CPU array
    // percpu_init_array() (Init.c, called by the BSP before any AP
    // was ever woken) already allocated -- see PerCpu.h's own
    // comment on why every core (BSP included) calls this for
    // itself rather than the BSP filling in every slot on every
    // AP's behalf.
    percpu_init_this_cpu(cpu_index);

    // Loads the same IDT the BSP already built (Init.c's idt_init())
    // -- without this, IDTR is left at whatever invalid/empty state
    // it had immediately after SIPI, and any interrupt (including a
    // generic IPI, Core/SMP/IPI/Ipi.h) delivered to this AP before
    // now would fault immediately with nowhere valid to vector to.
    idt_load_on_this_cpu();

    // Clears the interrupt flag's mask -- an AP starts with IF=0
    // (interrupts disabled) same as the BSP does immediately after
    // Boot/BIOS/Transition/LongModeEntry.asm's own long-mode entry.
    // Without this, IDTR being valid still isn't enough: the CPU
    // simply never delivers anything, IPI included, until IF is set.
    __asm__ volatile("sti");

    boot_log_ok_n("ap_c_entry: AP online, cpu_index", cpu_index);
    boot_log_ok_n("ap_c_entry: AP local APIC ID", lapic_get_id());

    // Confirms the GS-based mechanism actually works on THIS specific
    // core, not just that percpu_init_this_cpu() ran without crashing
    // -- percpu_get() re-reads GS_BASE from scratch and dereferences
    // it, so if this ever logs a cpu_index that doesn't match the
    // parameter above, GS_BASE isn't pointing where it should.
    percpu_t *self = percpu_get();
    boot_log_ok_n("ap_c_entry: percpu_get() cpu_index", self->cpu_index);

    // Phase 6a validation: atomic_inc32() must correctly serialize
    // across every real concurrent CPU touching this same shared
    // counter -- see this file's own comment on g_smp_test_counter.
    // If this were a plain (non-atomic) `g_smp_test_counter++`
    // instead, two APs incrementing at the same instant could both
    // read the same pre-increment value and each write back the same
    // result, silently losing one of the two increments -- exactly
    // the kind of bug atomic_inc32()'s single LOCK-prefixed
    // instruction exists to make impossible.
    uint32_t new_count = atomic_inc32(&g_smp_test_counter);
    boot_log_ok_n("ap_c_entry: atomic_inc32(g_smp_test_counter) ->", new_count);

    // Phase 6a validation: mutex_lock()/mutex_unlock() must actually
    // exclude other CPUs for the duration this AP holds it, and
    // mutex_unlock() must record THIS AP as the correct owner (see
    // Mutex.h's own header comment on why ownership tracking is the
    // whole point of a mutex over a bare spinlock).
    mutex_lock(&g_smp_test_mutex);
    boot_log_ok_n("ap_c_entry: mutex acquired, owner_cpu_index", (uint64_t)g_smp_test_mutex.owner_cpu_index);
    mutex_unlock(&g_smp_test_mutex);

    // Phase 6b validation: with only 2 units available
    // (g_smp_test_semaphore, Init.c's sem_init() call) and 4 real
    // CPUs (BSP + 3 AP) all reaching this same sem_try_wait() call,
    // exactly 2 across the whole system should succeed and 2 should
    // fail -- regardless of WHICH specific CPUs end up in each group
    // (that's a race, intentionally; the count is what's guaranteed,
    // not the winners).
    if (sem_try_wait(&g_smp_test_semaphore))
        boot_log_ok_n("ap_c_entry: semaphore unit claimed, cpu_index", cpu_index);
    else
    {
        boot_log_start();
        boot_log_str("ap_c_entry: semaphore empty, cpu_index ");
        boot_log_dec(cpu_index);
        boot_log_end_warn();
    }

    // Signals the BSP (spinning in ap_startup_wake_one() below) that
    // this AP has finished everything it needs from shared state --
    // in particular, that it's done reading ap_trampoline_data_t, so
    // the BSP can safely overwrite it for the next AP. Must be the
    // LAST write this function makes to anything shared before
    // idling -- see ap_startup_wake_one()'s own comment on why this
    // handoff is strictly one-AP-at-a-time.
    ap_trampoline_data_t *data = (ap_trampoline_data_t *)(uint64_t)(AP_TRAMPOLINE_ADDR + AP_TRAMPOLINE_DATA_OFFSET);
    data->ap_ready = 1;

    // Idles until Core/SMP/IPI/Ipi.h's ipi_send_process() (Phase 6.3)
    // assigns this CPU a real process and sends the IPI that
    // interrupts this very hlt -- IpiDispatch.c's ipi_dispatch() reads
    // this CPU's own current_proc on every IPI receipt (both this
    // one's own Phase 5 validation IPI, already delivered by the time
    // this loop starts, and any future one) and jumps into it via
    // proc_iretq_start() if one is present, never returning to this
    // loop once that happens.
    for (;;)
        __asm__ volatile("hlt");
}

// Wakes exactly one AP (INIT-SIPI-SIPI, Intel SDM Vol. 3A 8.4.4) and
// spins until it confirms it's alive before returning. Called once
// per AP, strictly one at a time (see ap_startup_begin()'s loop) --
// NOT fired off in parallel for every AP up front, because every AP
// this sequence wakes runs the exact same trampoline PAGE (physical
// AP_TRAMPOLINE_ADDR, not a per-AP copy) with the exact same shared
// ap_trampoline_data_t inside it. Two APs racing through that page
// concurrently would each be reading stack_top/c_entry_addr/cpu_index
// the BSP intended for a DIFFERENT AP, or reading them mid-write --
// waiting for ap_ready before touching that memory again for the next
// AP is what makes reusing a single shared page safe at all.
static void ap_startup_wake_one(uint8_t apic_id, uint32_t cpu_index)
{
    void *stack = kmalloc(AP_STACK_SIZE);
    if (!stack)
    {
        boot_log_start();
        boot_log_str("ap_startup_wake_one: stack allocation failed for cpu_index ");
        boot_log_dec(cpu_index);
        boot_log_end_error();
        return;
    }

    ap_trampoline_data_t *data = (ap_trampoline_data_t *)(uint64_t)(AP_TRAMPOLINE_ADDR + AP_TRAMPOLINE_DATA_OFFSET);
    // Stack grows down -- stack_top is the highest address in the
    // allocation, same convention every other stack setup in this
    // kernel uses (e.g. Core/Tasking/Process/Process.c's own kernel
    // stack setup).
    data->stack_top = (uint64_t)stack + AP_STACK_SIZE;
    data->c_entry_addr = (uint64_t)ap_c_entry;
    data->cpu_index = cpu_index;
    data->ap_ready = 0;

    // ── INIT IPI: assert then deassert ──────────────────────────────
    // Per the original MP Specification (and every real BIOS/chipset
    // built to it since), INIT is sent as a level-triggered
    // assert followed by an explicit deassert -- not simply sent once
    // -- to correctly reset the target AP's state. The deassert's
    // destination shorthand/level bits below (level-triggered,
    // deassert = level bit set but assert bit clear) target "all
    // APs" per spec convention for this specific second half of the
    // sequence, which is harmless here since only one AP is
    // mid-wakeup at a time (see this function's own header comment).
    send_ipi_and_wait(apic_id, LAPIC_ICR_DELIVERY_INIT | LAPIC_ICR_LEVEL_ASSERT | LAPIC_ICR_TRIGGER_LEVEL);
    send_ipi_and_wait(apic_id, LAPIC_ICR_DELIVERY_INIT | LAPIC_ICR_TRIGGER_LEVEL);

    // 10ms is the wait Intel's own MP initialization algorithm
    // (SDM Vol. 3A, 8.4.4.1) specifies between INIT and the first
    // SIPI -- not an arbitrary/conservative margin, this exact
    // duration is part of the documented sequence.
    busy_wait_ms(10);

    // ── SIPI, sent twice ─────────────────────────────────────────────
    // Also per Intel's documented algorithm: some older/real hardware
    // needs a second SIPI to reliably start (the first can be missed
    // if the AP wasn't fully ready), so both are always sent
    // unconditionally rather than sending one and checking. The 200us
    // gap the SDM specifies between them is far shorter than this
    // busy_wait_ms()'s 10ms granularity can express -- rounds up to
    // one full tick, which is harmless: waiting longer than the
    // minimum between the two SIPIs has no downside, only sending them
    // too close together (or the first SIPI too soon after INIT) does.
    uint32_t sipi_command = LAPIC_ICR_DELIVERY_STARTUP | AP_TRAMPOLINE_VECTOR;
    send_ipi_and_wait(apic_id, sipi_command);
    busy_wait_ms(1);
    send_ipi_and_wait(apic_id, sipi_command);

    // Spins for this AP's ap_ready flag rather than a fixed delay --
    // see ap_c_entry()'s own comment on why this specific flag (not
    // just "some time has passed") is what makes it safe to reuse the
    // trampoline page for the next AP. No timeout: on real/QEMU
    // hardware behaving per spec this always completes in well under
    // a second; if an AP genuinely never comes up (broken/absent
    // core, despite MADT reporting it enabled) this spins forever
    // rather than silently proceeding to hand the SAME trampoline
    // page to the next AP while the stuck one might still read from
    // it later and corrupt that next AP's boot -- a hang here is a
    // visible, debuggable failure; silent corruption of a different,
    // unrelated AP would not be.
    while (!data->ap_ready)
        __asm__ volatile("pause");

    boot_log_ok_n("ap_startup_wake_one: AP confirmed online, APIC ID", apic_id);
}

void ap_startup_begin(void)
{
    if (!g_madt_info.valid || g_madt_info.cpu_count <= 1)
    {
        boot_log_warn("ap_startup_begin: single-CPU system (or no MADT), skipping AP wakeup");
        return;
    }

    // Copies the trampoline's raw bytes to the fixed physical address
    // every SIPI in this function targets -- must happen before any
    // AP is sent a SIPI, obviously, but also (see AP_TRAMPOLINE_ADDR's
    // own comment in AppProcessorStartup.h) is safe to do this late in
    // boot: nothing else in this kernel ever claims that address for
    // anything, at any point.
    uint8_t *trampoline_dest = (uint8_t *)(uint64_t)AP_TRAMPOLINE_ADDR;
    uint64_t trampoline_size = (uint64_t)(ap_trampoline_end - ap_trampoline_start);
    for (uint64_t i = 0; i < trampoline_size; i++)
        trampoline_dest[i] = ap_trampoline_start[i];

    boot_log_ok_n("ap_startup_begin: trampoline copied, bytes", trampoline_size);

    // Index 0 is always the BSP itself by this kernel's own convention
    // (Core/Acpi/Madt.h's g_madt_info.cpu_apic_ids[] header comment --
    // not something ACPI itself guarantees, but true for every MADT
    // this kernel has been tested against and consistent with how the
    // BSP's own APIC ID was read in Init.c's lapic_init() call) --
    // starting this loop at 1 skips ever sending the BSP a SIPI to
    // wake itself, which the SDM doesn't define sensible behavior for
    // in the first place.
    for (uint8_t i = 1; i < g_madt_info.cpu_count; i++)
        ap_startup_wake_one(g_madt_info.cpu_apic_ids[i], i);

    boot_log_ok_n("ap_startup_begin: all APs woken, total CPUs online", g_madt_info.cpu_count);
}
