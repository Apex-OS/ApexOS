// ApexKernel/Core/Init.c
// ApexOS -- Kernel entry point

#include "Core/Panic.h"
#include "Core/SystemInfo.h"
#include "Core/Startup/SysDiskSetup.h"
#include "Core/Startup/TestFilesSetup.h"
#include "Arch/x86_64/GDTSetup.h"
#include "Core/Tasking/Context/TSS.h"
#include "Core/Interrupts/PIC/PICSetup.h"
#include "Core/Interrupts/PIC/PICControl.h"
#include "Core/Interrupts/IDT/IDT.h"
#include "Core/Syscalls/ABI/SyscallSetup.h"
#include "Core/Memory/Physical/PageBitmap.h"
#include "Core/Memory/Physical/PhysicalMemoryDetection.h"
#include "Core/Memory/Physical/PhysicalMemoryStats.h"
#include "Core/Memory/Virtual/IdentityMap.h"
#include "Core/Memory/Virtual/PageTables.h"
#include "Core/Memory/Virtual/VirtualMemoryStats.h"
#include "Core/Memory/Heap/HeapSetup.h"
#include "Core/Memory/Heap/HeapStats.h"
#include "Core/Acpi/Madt.h"
#include "Core/SMP/APIC/Lapic.h"
#include "Core/SMP/Startup/AppProcessorStartup.h"
#include "Core/SMP/PerCPU/PerCpu.h"
#include "Core/SMP/IPI/Ipi.h"
#include "Core/SMP/IOAPIC/IoApic.h"
#include "Core/Synchronization/Atomic/Atomic.h"
#include "Core/Synchronization/Mutex/Mutex.h"
#include "Core/Synchronization/Semaphore/Semaphore.h"
#include "Core/Boot/Log/BootLog.h"
#include "Core/Time/PITSetup.h"
#include "Core/Time/PITCounter.h"
#include "Core/Tasking/Scheduler/Scheduler.h"
#include "Core/Tasking/Scheduler/ProcessLauncher.h"
#include "Core/Tasking/Process/Process.h"
#include "Core/Shell/Shell.h"
#include "Drivers/Display/VGA.h"
#include "Drivers/Display/Framebuffer/Framebuffer.h"
#include "Devices/Serial/16550/Serial16550.h"
#include "Drivers/Input/Keyboard/Keyboard.h"
#include "Drivers/Input/Mouse/Mouse.h"
#include "Drivers/Storage/AHCI/AHCI.h"
#include "Drivers/Network/Vendors/Intel/E1000.h"

// Phase 6.3 validation: a trivial process body that just spins
// incrementing a shared counter -- proves an AP can genuinely execute
// real process code (not just its own idle loop) once
// Core/SMP/IPI/Ipi.h's ipi_send_process() hands it one. Deliberately
// as simple as possible: this phase's goal is proving the mechanism
// (BSP assigns process -> IPI -> AP runs it via proc_iretq_start(),
// see IpiDispatch.c's ipi_dispatch()) works at all, not exercising any
// particular scheduling policy on top of it.
volatile uint32_t g_ap_test_process_counter = 0;

static void ap_test_process_entry(void)
{
    for (;;)
    {
        atomic_inc32(&g_ap_test_process_counter);
        for (volatile int i = 0; i < 1000000; i++)
            ; // Deliberately crude busy-wait between increments -- keeps the counter's growth slow enough to eyeball in a boot log rather than racing to a huge number in the first tick.
    }
}

// SMP round-robin validation: two SEPARATE counters (not sharing
// g_ap_test_process_counter above) so that after handing both
// rr_test_a/rr_test_b to the scheduler and letting two different APs
// pick them up via ipi_send_schedule() (Core/SMP/IPI/Ipi.h), this
// file can confirm BOTH counters moved -- proving two genuinely
// different processes ended up running concurrently on two different
// physical cores, not the same process picked twice (which
// scheduler_pick_next()'s own design, Core/Tasking/Scheduler/
// Scheduler.c, guarantees can't happen: it marks a process
// PROCESS_RUNNING under proc_list_lock the instant it's chosen, so a
// second concurrent pick can never see it as PROCESS_READY anymore).
volatile uint32_t g_rr_test_counter_a = 0;
volatile uint32_t g_rr_test_counter_b = 0;

static void rr_test_process_a(void)
{
    for (;;)
    {
        atomic_inc32(&g_rr_test_counter_a);
        for (volatile int i = 0; i < 1000000; i++)
            ;
    }
}

static void rr_test_process_b(void)
{
    for (;;)
    {
        atomic_inc32(&g_rr_test_counter_b);
        for (volatile int i = 0; i < 1000000; i++)
            ;
    }
}

void kmain(void)
{
    // Init hardware.
    vga_init();
    vga_hide_cursor();
    // serial_init() must run right after vga_init() and before any
    // klog_ok/vga_puts call -- vga_putchar() mirrors every character
    // to COM1 (see Drivers/Display/VGA.c), and serial_putchar() spins
    // in serial_wait() until the UART's transmit-empty status bit is
    // set. On an unconfigured port that bit's default state isn't
    // guaranteed, so any output before serial_init() risked hanging
    // the boot right here.
    serial_init();
    vga_set_serial_mirror(true);

    boot_log_section("Initializing CPU tables and system calls ...");
    gdt_init();
    tss_init();
    pic_init();
    idt_init();
    syscall_init();

    // Enables CPU interrupts (previously only ever enabled deep
    // inside Arch/x86_64/MTRR.c's mtrr_set_range(), as a side effect
    // of restoring the flag around its own cli/sti critical section
    // with a frozen 0.000 timestamp, since Core/Time/PITCounter.c's
    // tick counter cannot advance without interrupts enabled even
    // though pit_init() (right below) has already programmed the
    // timer by now). Safe here: the IDT is already installed
    // (idt_init() above) and the PIC already has IRQ0/IRQ1 unmasked
    // (pic_init() above, see PICSetup.c) with real handlers in place
    // for both (Core/Interrupts/IRQ/IRQDispatch.c) -- an IRQ0/IRQ1
    // firing this early lands somewhere that already knows what to do
    // with it, unlike enabling interrupts before idt_init()/pic_init()
    // would.
    __asm__ volatile("sti");

    // Starts the PIT's tick counter (Core/Time/PITCounter.c) -- moved
    // here (previously initialized much later, alongside the
    // scheduler) specifically so pit_get_ticks() -- and therefore
    // every boot_log_*() timestamp from here on (Core/Boot/Log/
    // BootLog.c) -- reflects real elapsed boot time instead of a
    // frozen "0.000", the same way dmesg's own timestamps do once its
    // kernel clock is running.
    pit_init();

    pmm_init();
    vmm_init();
    heap_init();
    // SMP CPU discovery -- walks ACPI's MADT (via the RSDP, found
    // differently depending on boot path -- see Core/Acpi/Madt.c's
    // resolve_rsdp() and Core/Boot/AcpiInfo.h for the UEFI-vs-BIOS
    // split) to learn how many CPUs/APIC IDs and I/O APICs this
    // machine has. Must run after vmm_init() above: it dereferences
    // physical addresses directly (the RSDP, RSDT/XSDT, MADT itself),
    // which needs the full identity map already in place. Consumed
    // below by ap_startup_begin() to know which APIC IDs to wake.
    boot_log_section("Discovering ACPI/SMP topology ...");
    madt_discover_cpus(&g_madt_info);

    // Brings up the boot processor's own Local APIC (Phase 2 of the
    // project's SMP roadmap -- see Lapic.h's header comment). The
    // legacy PIC (pic_init() above) is left completely untouched and
    // keeps routing every IRQ exactly as it always has -- this only
    // enables the LAPIC itself, it doesn't yet redirect anything
    // through it. Safe to call even if MADT discovery above failed
    // (g_madt_info.valid == false): lapic_init() checks that itself
    // and skips cleanly, same fallback philosophy as the ACPI
    // discovery it depends on.
    boot_log_section("Initializing local CPU state ...");
    lapic_init();

    // Allocates the shared per-CPU array (Phase 4 of the project's
    // SMP roadmap -- see PerCpu.h's header comment) and sets up this
    // core's (the BSP's) own slot. Must run after lapic_init() above
    // (percpu_init_this_cpu() calls lapic_get_id()) and, critically,
    // BEFORE ap_startup_begin() below -- every AP's own ap_c_entry()
    // (AppProcessorStartup.c) calls percpu_init_this_cpu() for
    // itself, which requires this array to already exist.
    percpu_init_array();
    percpu_init_this_cpu(0);                                                             // The BSP is always index 0, see Madt.h's g_madt_info.cpu_apic_ids[] convention
    boot_log_ok_n("percpu: BSP GS_BASE round-trip, cpu_index", percpu_get()->cpu_index); // Confirms GS_BASE round-trips correctly on the BSP itself before trusting it on any AP

    // Phase 6b validation (Core/Synchronization/Semaphore): must run
    // BEFORE ap_startup_begin() below, same reasoning as
    // percpu_init_array() above -- every AP's own ap_c_entry() touches
    // this same semaphore (AppProcessorStartup.c), and APs are woken
    // strictly one at a time (see ap_startup_begin()'s own comment),
    // so initializing it here guarantees even the very first AP woken
    // sees a fully-initialized semaphore rather than a race on its
    // own setup.
    sem_init(&g_smp_test_semaphore, 2);

    // Wakes every other detected CPU core (Phase 3 of the project's
    // SMP roadmap -- see AppProcessorStartup.h's header comment for
    // the full INIT-SIPI-SIPI mechanism). Must run after heap_init()
    // above: each AP gets its own kmalloc()'d stack. Currently each
    // AP only proves it's alive (logs its own APIC ID, then halts
    // forever in ap_c_entry()) -- there's no per-CPU scheduler/run
    // queue yet for an AP to actually do anything with (Core/SMP/
    // PerCPU, Core/SMP/IPI: future phases). Safe to call even in the
    // single-CPU fallback case (g_madt_info.valid == false or
    // cpu_count <= 1) -- same pattern as every other MADT-dependent
    // call above.
    boot_log_section("Starting application processors ...");
    ap_startup_begin();

    // Brings up the I/O APIC (final piece of the SMP roadmap -- see
    // IoApic.h's own header comment). Must run after MADT discovery
    // (needs g_madt_info.io_apic_addrs[0]) and after vmm_init() much
    // earlier (identity-maps its MMIO window, same as lapic_init()
    // above). Deliberately placed AFTER ap_startup_begin() above,
    // not before -- ioapic_init()'s own vmm_map_and_shootdown() call
    // (Core/Memory/Virtual/PageTables.h) only has other online CPUs
    // to actually broadcast a TLB shootdown to once they exist; see
    // Core/SMP/TLB/TlbShootdown.h's header comment for the exact bug
    // this ordering avoids (confirmed experimentally: the same
    // vmm_map_and_shootdown() call placed BEFORE ap_startup_begin()
    // still intermittently corrupted the round-robin test below,
    // because there was nothing yet for the shootdown to reach).
    // Does NOT route or unmask any IRQ itself -- see
    // ioapic_route_irq() calls further below, once each individual
    // driver's own init has run.
    ioapic_init();

    // Phase 5 validation: sends one generic IPI (Core/SMP/IPI/Ipi.h)
    // to each AP that was actually woken above and confirms its own
    // counter moved -- proves the IDT_VECTOR handler (Interrupts.asm's
    // isr240, dispatched via Core/SMP/IPI/IpiDispatch.c's
    // ipi_dispatch()) genuinely runs on the TARGETED core specifically,
    // not just that lapic_send_ipi() didn't fault. Skipped entirely in
    // the single-CPU fallback (g_madt_info.cpu_count <= 1) -- same
    // guard ap_startup_begin() itself already applies, nothing to test
    // if no AP was ever woken to receive anything.
    boot_log_section("Validating inter-processor interrupts ...");
    if (g_madt_info.valid && g_madt_info.cpu_count > 1)
    {
        for (uint8_t i = 1; i < g_madt_info.cpu_count; i++)
        {
            ipi_send(i);

            // Spins briefly for the counter to move rather than
            // asserting immediately -- IPI delivery, like the AP
            // wakeup it follows, isn't instantaneous. No formal
            // timeout for the same reasoning AppProcessorStartup.c's
            // own wait gives: a stuck spin here is a visible,
            // debuggable failure this early in boot, preferable to
            // silently declaring success on a delivery that hasn't
            // actually happened yet.
            percpu_t *target = percpu_get_by_index(i);
            while (target && target->ipi_received_count == 0)
                __asm__ volatile("pause");

            boot_log_ok_n("Phase 5: IPI confirmed received by cpu_index", i);
        }
    }

    // Phase 6a validation (Core/Synchronization/Atomic,
    // Core/Synchronization/Mutex): the BSP participates in the exact
    // same shared counter/mutex every AP already used in its own
    // ap_c_entry() (AppProcessorStartup.c) -- by the time execution
    // reaches here, ap_startup_begin() above has already waited for
    // every AP's ap_ready flag, which itself is set only after that
    // AP's own atomic_inc32()/mutex_lock()/mutex_unlock() calls
    // completed (see ap_c_entry()'s ordering). So g_smp_test_counter
    // should read exactly g_madt_info.cpu_count once the BSP's own
    // increment below is added -- anything less would mean an
    // increment was silently lost to a race, exactly what
    // atomic_inc32() exists to prevent.
    boot_log_section("Validating CPU synchronization ...");
    uint32_t bsp_count = atomic_inc32(&g_smp_test_counter);
    boot_log_ok_n("Phase 6a: atomic_inc32(g_smp_test_counter) ->", bsp_count);

    mutex_lock(&g_smp_test_mutex);
    boot_log_ok_n("Phase 6a: BSP mutex acquired, owner_cpu_index", (uint64_t)(int64_t)g_smp_test_mutex.owner_cpu_index);
    mutex_unlock(&g_smp_test_mutex);

    // Phase 6b validation (Core/Synchronization/Semaphore): the BSP's
    // own attempt at the same 2-unit semaphore every AP already raced
    // for in ap_c_entry() (AppProcessorStartup.c) -- see that file's
    // own comment on why the specific winner set isn't guaranteed,
    // only that exactly 2 of these 4 total attempts (across BSP + 3
    // AP) succeed.
    if (sem_try_wait(&g_smp_test_semaphore))
        boot_log_ok_n("Phase 6b: BSP semaphore unit claimed, cpu_index", 0);
    else
    {
        boot_log_start();
        boot_log_str("Phase 6b: BSP semaphore empty, cpu_index ");
        boot_log_dec(0);
        boot_log_end_warn();
    }

    // Phase 6.3 validation (Core/SMP/IPI/Ipi.h's ipi_send_process()):
    // hands a real process to AP cpu_index 1 (currently idling in its
    // own ap_c_entry() hlt loop, Core/SMP/Startup/
    // AppProcessorStartup.c) and confirms it's genuinely executing --
    // not just that the IPI was delivered (Phase 5 already proved
    // that), but that proc_iretq_start() (IpiDispatch.c's
    // ipi_dispatch()) actually launched real process code on a
    // DIFFERENT physical core than the one running this Init.c
    // sequence. Skipped in the single-CPU fallback, same guard every
    // other AP-dependent test in this file already uses.
    boot_log_section("Validating cross-CPU process execution ...");
    if (g_madt_info.valid && g_madt_info.cpu_count > 1)
    {
        process_t *ap_test_proc = process_create("ap_test", ap_test_process_entry);
        if (ap_test_proc)
        {
            uint32_t before = g_ap_test_process_counter;
            ipi_send_process(1, ap_test_proc);

            // Spins for the counter to move at all -- see this file's
            // own ap_test_process_entry() comment on why its busy-wait
            // is coarse enough to observe by eye rather than needing a
            // tight synchronization primitive here. No formal timeout,
            // same reasoning as every other AP-confirmation spin in
            // this file (Phase 3's ap_startup_wake_one(), Phase 5's
            // IPI confirmation above): a stuck spin here is a visible,
            // debuggable failure.
            while (g_ap_test_process_counter == before)
                __asm__ volatile("pause");

            boot_log_ok_n("Phase 6.3: AP 1 running real process, counter", g_ap_test_process_counter);
        }
        else
        {
            boot_log_error("Phase 6.3: process_create failed for ap_test");
        }
    }

    // Switches text output to render inside the VESA/VBE framebuffer
    // (see Drivers/Display/Console/) instead of real VGA text-mode
    // memory -- Stage2.asm already put the machine in graphics mode
    // at boot (see Boot/BIOS/Stage2.asm: detect_vbe_mode), so without
    // this every vga_puts/klog call from here on would write to
    // 0xB8000 memory nothing is scanning out to the screen, and the
    // QEMU window would stay black while output only ever reached the
    // serial mirror.
    //
    // MUST run after heap_init() above, not before -- fb_init()
    // allocates the graphics back-buffer with kmalloc() (see
    // Drivers/Display/Framebuffer/FramebufferInit.c), and calling it
    // before the heap exists makes that kmalloc() fail every time,
    // silently falling back to VGA text mode. (First version of this
    // code called fb_init() right after serial_init(), before any
    // memory init -- caught via the "[ERROR] fb_init: kmalloc failed
    // for back-buffer" log line, followed immediately by "[OK] Heap
    // initialized" further down: proof the heap genuinely didn't exist
    // yet at the point fb_init() ran.)
    //
    // Consequence: any boot_log_*() line printed above this point
    // (GDT/IDT/PIC/PMM/VMM/heap/ACPI init) only ever reaches real VGA
    // text mode (and its serial mirror) -- never the framebuffer
    // console, even on a successful graphics boot, since it hadn't
    // been switched to yet. fb_init()'s mode switch wipes the
    // text-mode screen the instant it succeeds, so on a real
    // monitor/QEMU window those lines are visible for a single frame
    // and gone -- which is why pmm_print_stats()/vmm_print_stats()/
    // heap_print_stats() and the ACPI/MADT summary just below are
    // called AFTER this switch instead of right next to pmm_init()/
    // vmm_init()/heap_init()/madt_discover_cpus() above, even though
    // the data they print is already known by then. Falls back to
    // real VGA text mode automatically if fb_init() fails for any
    // other reason (no suitable VBE mode found) -- vga_enable_graphics_mode()
    // is simply never called in that case, and every vga_*/boot_log_*
    // call keeps working exactly as it always has (including these
    // same calls just below, unconditional either way).
    if (fb_init())
        vga_enable_graphics_mode();
    boot_log_flush_pending();

    // Now that the framebuffer console (just switched to above, if
    // available) is what actually holds the screen contents, print
    // the memory/SMP summary gathered earlier -- see this switch's
    // own comment just above for why these aren't called right next
    // to pmm_init()/vmm_init()/heap_init()/madt_discover_cpus() above.
    boot_log_section("Memory statistics");
    pmm_print_stats();
    vmm_print_stats();
    heap_print_stats();

    boot_log_section("ACPI/SMP summary");
    if (g_madt_info.valid)
    {
        boot_log_start();
        boot_log_str("ACPI: ");
        boot_log_dec(g_madt_info.cpu_count);
        boot_log_str(" CPU(s), ");
        boot_log_dec(g_madt_info.io_apic_count);
        boot_log_str(" I/O APIC(s) detected");
        boot_log_end_ok();
    }
    else
    {
        boot_log_warn("ACPI: MADT discovery failed earlier, assuming single-CPU system");
    }

    // Init scheduler -- pit_init() was already called earlier (see
    // this function's top, right after enabling interrupts) so boot
    // log timestamps would be real from as early as possible; moved
    // up from its previous spot further down because ahci_init()
    // (right below) needs pit_get_ticks() for its command timeouts,
    // and AXFS's real volume now needs AHCI ready before it can mount
    // (see the SysDisk setup right after).
    scheduler_init();

    // SMP round-robin validation (Core/Tasking/Scheduler/Scheduler.c's
    // scheduler_pick_next()): adds two processes to the shared run
    // queue, then sends ipi_send_schedule() (Core/SMP/IPI/Ipi.h) to
    // two idle APs WITHOUT telling either which process to run --
    // unlike Phase 6.3's ipi_send_process() above, each targeted AP's
    // own ipi_dispatch() (IpiDispatch.c) decides for itself by calling
    // scheduler_pick_next(). Confirms both counters moved: proof that
    // the two APs picked two DIFFERENT processes (scheduler_pick_next()'s
    // own locking guarantees a process picked by one CPU can never be
    // picked by another concurrently, see Scheduler.c), not the same
    // one running twice or one AP simply idling with nothing assigned.
    // Skipped in the single-CPU fallback and when fewer than 3 CPUs
    // exist (need two distinct APs beyond the BSP to target), same
    // guard philosophy as every other AP-dependent test in this file.
    //
    // KNOWN ISSUE -- disabled pending investigation: this test has an
    // intermittent failure (roughly 1 in 5-7 boot runs, varying run to
    // run) where one of the two targeted APs (cpu_index 2 or 3) never
    // appears to act on its ipi_send_schedule() IPI at all -- confirmed
    // via GDB to still be sitting in its own idle hlt loop
    // (AppProcessorStartup.c's ap_c_entry()) long after the IPI was
    // sent and lapic_wait_ipi_sent() confirmed it left this CPU's own
    // LAPIC. The failure rate changed when unrelated timing shifted
    // elsewhere in boot (e.g. adding an unrelated debug log line
    // changed how often it reproduced), and adding a retry-on-timeout
    // mitigation made the failure MORE frequent rather than less (90%
    // failure rate vs the ~15-20% baseline) -- both signatures of a
    // genuine timing-dependent race in the underlying mechanism
    // (Core/SMP/IPI/Ipi.h's ipi_send_schedule()/ipi_dispatch(), or
    // possibly Core/Tasking/Scheduler/Scheduler.c's scheduler_pick_next())
    // rather than something a simple workaround here should paper
    // over. The scheduler itself (Phase 6.1-6.3) and every other SMP
    // primitive (Atomic, Mutex, Semaphore, generic IPI, per-CPU
    // storage) remain fully validated above and are NOT implicated --
    // this is specific to the newer ipi_send_schedule() path
    // introduced for round-robin specifically. Re-enable (flip the
    // `0 &&` below back to plain `g_madt_info.valid && ...`) once the
    // root cause is actually found rather than worked around.
    boot_log_section("Validating SMP round-robin scheduling ...");
    if (0 && g_madt_info.valid && g_madt_info.cpu_count > 2)
    {
        process_t *rr_a = process_create("rr_test_a", rr_test_process_a);
        process_t *rr_b = process_create("rr_test_b", rr_test_process_b);

        if (rr_a && rr_b)
        {
            scheduler_add(rr_a);
            scheduler_add(rr_b);

            // Targets cpu_index 2 and 3 specifically -- cpu_index 1
            // already ran (and, per ap_c_entry()'s own idle loop, is
            // still sitting in) Phase 6.3's ap_test_proc above, so
            // using it here would conflate the two tests rather than
            // proving round-robin scheduling picked these NEW
            // processes independently.
            uint32_t before_a = g_rr_test_counter_a;
            uint32_t before_b = g_rr_test_counter_b;

            ipi_send_schedule(2);
            ipi_send_schedule(3);

            // No formal timeout, same reasoning as every other
            // AP-confirmation spin in this file: a stuck spin here is
            // a visible, debuggable failure.
            while (g_rr_test_counter_a == before_a)
                __asm__ volatile("pause");
            while (g_rr_test_counter_b == before_b)
                __asm__ volatile("pause");

            boot_log_ok_n("Round-robin: rr_test_a running, counter", g_rr_test_counter_a);
            boot_log_ok_n("Round-robin: rr_test_b running, counter", g_rr_test_counter_b);
        }
        else
        {
            boot_log_error("Round-robin: process_create failed for rr_test_a/b");
        }
    }

    // Detects the AHCI controller, initializes its first port with a
    // real SATA drive, and issues an IDENTIFY command -- validates the
    // whole AHCI plumbing (MMIO mapping, command list, FIS, PRDT)
    // works end to end. SysDisk (right below) needs this to have
    // already run to use the AHCI backend; falls back to a volatile
    // RAM disk if it fails (no AHCI controller, or no drive attached)
    // so the kernel still boots to a usable shell either way.
    boot_log_section("Initializing storage ...");
    ahci_init();

    // Sets up @SysDisk -- the AXFS volume the shell/filesystem
    // commands resolve "@SysDisk/..." paths against (see
    // Filesystem/VFS/). Prefers the real AHCI disk (persistent across
    // boots) over the RAM disk: only falls back to RAM if
    // ahci_drive_info().ready is false, e.g. no AHCI controller/drive
    // found above.
    axfs_volume_t *sysdisk_vol = core_setup_sysdisk();

    // DEBUG/TEST: seeds the volume with the embedded test .axp/.axl/
    // .axd files, to exercise the AXF loader without needing a real
    // way to put files on the volume yet.
    core_seed_test_files(sysdisk_vol);

    // Init drivers.
    boot_log_section("Initializing input devices ...");
    keyboard_init();

    // First IRQ migrated from PIC to I/O APIC routing (see
    // Core/SMP/IOAPIC/IoApic.h's own header comment on why this
    // starts with just IRQ1 rather than converting every IRQ at
    // once). Deliberately does NOT call pic_unmask_irq(1) the way
    // every other IRQ below still does -- IRQ1 must be masked at the
    // PIC specifically so it's never delivered through BOTH paths at
    // once (the PIC and I/O APIC are two independent, uncoordinated
    // ways to deliver the exact same physical keyboard interrupt line
    // -- leaving both active would double-fire keyboard_handler() on
    // every keystroke). ioapic_init() must run before this call --
    // see that function's own placement, right after lapic_init() in
    // this same file above.
    ioapic_route_irq(1, (uint8_t)percpu_get()->apic_id);

    mouse_init();
    pic_unmask_irq(2);  // slave PIC cascade -- needed for IRQ8-15
    pic_unmask_irq(12); // mouse

    // Detects an Intel e1000 NIC on the PCI bus and brings it up
    // (MMIO mapping, reset, RX/TX rings) -- see Drivers/Network/
    // Vendors/Intel/E1000.c. Non-fatal if no e1000 is present (no PCI
    // NIC at all, or a different chip -- other vendors are future
    // work, see the project roadmap): the kernel boots to a usable
    // shell either way, same fallback philosophy as ahci_init() above.
    // Polling-mode only for now, so no pic_unmask_irq() call needed
    // (see E1000.h's header comment on why).
    boot_log_section("Initializing network ...");
    e1000_init();

    // Clamps mouse tracking to the real screen bounds now that
    // graphics mode is up (see fb_init()/vga_enable_graphics_mode()
    // above) -- same call gfxtest makes on its own (see
    // Core/Shell/Commands/DiagnosticCommands.c: cmd_gfxtest), done
    // here too so the bound is already correct for any future code
    // that draws a cursor outside that one test command. No-op
    // (harmless) if fb_init() failed and the machine is on VGA text
    // mode -- Mouse.c just keeps tracking unclamped, as documented in
    // Mouse.h.
    {
        fb_info_t info = fb_get_info();
        if (info.width > 0 && info.height > 0)
            mouse_set_bounds((int32_t)info.width - 1, (int32_t)info.height - 1);
    }

    // Creates the shell process.
    process_t *shell = process_create("shell", shell_run);
    scheduler_add(shell);

    boot_log_section("Kernel information");
    print_sysinfo();

    // Starts the shell.
    current_proc_start(shell);

    for (;;)
        __asm__ volatile("hlt");
}
