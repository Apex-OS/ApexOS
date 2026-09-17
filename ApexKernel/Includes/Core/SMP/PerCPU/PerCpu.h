// ApexKernel/Includes/Core/SMP/PerCPU/PerCpu.h
// ApexOS -- Per-CPU data (SMP Phase 4, extended in Phase 6.2)
//
// Every core needs a way to find data that's specific to itself --
// which CPU index it is, its own APIC ID, and (as of Phase 6.2) its
// own current_proc -- without needing to know in advance, from
// arbitrary code, which physical core is currently running it.
//
// The x86-64 mechanism for this is the GS segment: each CPU's own
// IA32_GS_BASE MSR points to a different physical address, so the
// exact same instruction (`mov rax, gs:[offset]`) run on different
// cores reads different memory -- see percpu_init_this_cpu() below,
// which is what makes each core's GS_BASE point at its own
// percpu_t rather than another core's.

#ifndef APEX_CORE_SMP_PERCPU_PERCPU_H
#define APEX_CORE_SMP_PERCPU_PERCPU_H

#include "Core/Types.h"
#include "Core/Tasking/Process/Process.h"

// One entry per logical CPU this machine has (see Core/Acpi/Madt.h --
// indexed identically to g_madt_info.cpu_apic_ids[], same convention:
// index 0 is always the BSP). Deliberately NOT allocated with
// kmalloc() per-CPU as each core starts up -- percpu_init_array()
// below allocates the whole array up front, from the BSP, before any
// AP is ever woken (Core/SMP/Startup/AppProcessorStartup.c's
// ap_startup_begin() runs after this in Init.c) specifically so that
// by the time an AP's own ap_c_entry() calls percpu_init_this_cpu(),
// the array (and this AP's own slot within it) already exists --
// no AP ever allocates its own slot, only fills in the one the BSP
// already reserved for it.
typedef struct
{
    uint32_t cpu_index;   // This CPU's index into g_madt_info.cpu_apic_ids[] -- 0 for the BSP
    uint32_t apic_id;     // This CPU's own Local APIC ID (Core/SMP/APIC/Lapic.h's lapic_get_id(), cached here so reading it doesn't require an MMIO round-trip every time)
    volatile bool online; // Set true by percpu_init_this_cpu() once this slot is safe to read -- see that function's own comment on why this matters for the BSP's own slot specifically

    // Incremented by Core/SMP/IPI/IpiDispatch.c's ipi_dispatch() every
    // time THIS core receives a generic IPI (Core/SMP/IPI/Ipi.h) --
    // exists purely to give Phase 5 something concrete to verify (a
    // sender's ipi_send() call should visibly bump the specific
    // target CPU's own counter and no other CPU's) ahead of any real
    // IPI use case being wired up.
    volatile uint64_t ipi_received_count;

    // This CPU's own currently-running process, or NULL if none (the
    // pre-Phase-6.2 state every CPU implicitly had before any process
    // was ever scheduled onto it). Replaces Core/Tasking/Scheduler/
    // Scheduler.c's own current_proc, which was a single kernel-wide
    // global through Phase 6.1 -- see SchedulerInternal.h for the
    // full migration story and why current_proc itself still exists
    // as a compatibility accessor for the BSP's own slot rather than
    // being deleted outright.
    process_t *current_proc;
} percpu_t;

// Allocates the shared percpu_t array (see percpu_t's own comment on
// why this happens once, up front, from the BSP). Must run after
// Core/Acpi/Madt.c's madt_discover_cpus() (needs g_madt_info.cpu_count
// to know how many slots to allocate) and after heap_init() (this is
// a kmalloc() allocation) -- see Init.c for the exact ordering this
// requires. Must ALSO run before Core/SMP/Startup/AppProcessorStartup.c's
// ap_startup_begin(), since every AP's own ap_c_entry() calls
// percpu_init_this_cpu() below, which requires this array to already
// exist.
void percpu_init_array(void);

// Called once by EVERY core (BSP included, from Init.c; each AP from
// its own ap_c_entry(), Core/SMP/Startup/AppProcessorStartup.c) --
// sets IA32_GS_BASE to point at this core's own slot in the array
// percpu_init_array() allocated, then fills in that slot's
// cpu_index/apic_id and marks it online. `cpu_index` must be this
// specific call's own core's index (0 for the BSP call; whatever
// AppProcessorStartup.c's ap_c_entry() was itself called with for
// every other core) -- there is no way for this function to
// determine that on its own, since lapic_get_id() alone only proves
// which APIC ID is running, not which g_madt_info.cpu_apic_ids[]
// slot that corresponds to (technically derivable by searching that
// array for a match, but the caller already knows its own index
// for free in every real call site, so doing that search here would
// be redundant work for no benefit).
void percpu_init_this_cpu(uint32_t cpu_index);

// Returns a pointer to the CURRENTLY EXECUTING core's own percpu_t --
// reads IA32_GS_BASE (whatever percpu_init_this_cpu() set it to on
// THIS core) rather than indexing the array by some other means, so
// this is always correct regardless of which core calls it, with no
// locking needed (each core only ever reads/writes its own slot
// through this function -- see percpu_t's own fields, none of which
// any other core has a legitimate reason to touch).
percpu_t *percpu_get(void);

// Returns a pointer to `cpu_index`'s own percpu_t slot, regardless of
// which core calls this -- unlike percpu_get() above (which always
// reads the CALLING core's own slot via GS_BASE), this is how one CPU
// inspects another's per-CPU state, e.g. Core/SMP/IPI/Ipi.h's
// ipi_get_received_count() confirming a specific target CPU's counter
// actually moved after ipi_send() targeted it. Returns NULL if
// cpu_index is out of range or the array failed to allocate (see
// percpu_init_array()) -- callers must check before dereferencing,
// same as any other fallible lookup in this kernel.
percpu_t *percpu_get_by_index(uint32_t cpu_index);

// True once percpu_init_this_cpu() has successfully run for the BSP
// (Init.c calls it with cpu_index == 0 right after percpu_init_array())
// -- exists specifically because interrupts are enabled (Init.c's own
// `sti`) BEFORE that call happens, in order to get real boot-log
// timestamps flowing early (see Init.c's own comment on pit_init()'s
// placement). Any interrupt that fires in that window -- most notably
// the timer tick, IRQ0, Core/Tasking/Scheduler/SchedulerTick.c's
// scheduler_tick_asm() -- runs before GS_BASE has ever been set on
// this CPU, so percpu_get() would return garbage (whatever was in the
// GS_BASE MSR at power-on) and any dereference of it is undefined --
// confirmed via -d int: a General Protection Fault reading MSR 0xC0000101
// (IA32_GS_BASE) territory the instant such an early tick tried to
// touch current_proc (SchedulerInternal.h's macro over percpu_get()).
// Code that might run before Init.c's own percpu_init_this_cpu(0) call
// -- currently just scheduler_tick_asm() -- must check this before
// calling percpu_get() at all.
extern volatile bool g_percpu_ready;

#endif // APEX_CORE_SMP_PERCPU_PERCPU_H
