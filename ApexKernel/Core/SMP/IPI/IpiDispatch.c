// ApexKernel/Core/SMP/IPI/IpiDispatch.c
// ApexOS -- Generic IPI send/dispatch (see Ipi.h)

#include "Core/SMP/IPI/Ipi.h"
#include "Core/SMP/APIC/Lapic.h"
#include "Core/SMP/PerCPU/PerCpu.h"
#include "Core/Acpi/Madt.h"
#include "Core/Tasking/Process/Process.h"
#include "Core/Tasking/Scheduler/Scheduler.h"
#include "Logs/Log.h"

// Defined in Arch/x86_64/ContextSwitch.asm -- jumps directly into a
// process via iretq, no prior context to save (see that file's own
// header comment on why this is the cooperative-switch-agnostic half
// of that file, safe to call from anywhere including here). Used
// below by ipi_dispatch() to launch a process the BSP assigned to
// this AP via ipi_send_process() -- NOT declared in a shared header
// since ProcessLauncher.c's own use of the identical extern is
// entirely unrelated to this file (that one launches the very first
// process ever, from the BSP's normal boot path; this one launches
// whatever process Phase 6.3's ipi_send_process() most recently
// assigned to an AP) -- two independent callers of the same low-level
// primitive, not a shared abstraction worth centralizing.
extern void proc_iretq_start(cpu_context_t *ctx);

void ipi_send(uint32_t target_cpu_index)
{
    if (!g_madt_info.valid || target_cpu_index >= g_madt_info.cpu_count)
    {
        klog_error_n("ipi_send: invalid target_cpu_index", target_cpu_index);
        return;
    }

    // Only warns rather than refusing outright -- an IPI sent to a
    // CPU whose percpu_t.online is still false (either genuinely not
    // woken yet, or Core/SMP/Startup/AppProcessorStartup.c's
    // ap_startup_begin() was never called at all, e.g. the single-CPU
    // fallback path) has no handler running there to receive it, but
    // that's a caller logic error to fix, not a condition this
    // function needs to actively prevent -- the LAPIC send itself is
    // harmless either way (Intel SDM defines no fault for targeting
    // an APIC ID with nothing listening), it would simply never be
    // observed as received.
    percpu_t *target = percpu_get_by_index(target_cpu_index);
    if (!target || !target->online)
        klog_warn("ipi_send: target CPU not marked online, sending anyway");

    uint8_t dest_apic_id = g_madt_info.cpu_apic_ids[target_cpu_index];
    lapic_send_ipi(dest_apic_id, IPI_VECTOR);
    lapic_wait_ipi_sent();
}

void ipi_send_process(uint32_t target_cpu_index, process_t *proc)
{
    if (!g_madt_info.valid || target_cpu_index >= g_madt_info.cpu_count)
    {
        klog_error_n("ipi_send_process: invalid target_cpu_index", target_cpu_index);
        return;
    }

    percpu_t *target = percpu_get_by_index(target_cpu_index);
    if (!target)
    {
        klog_error_n("ipi_send_process: no percpu slot for cpu_index", target_cpu_index);
        return;
    }

    // Assigns the process to the TARGET CPU's own slot (via
    // percpu_get_by_index(), not this CPU's own percpu_get()) BEFORE
    // sending the IPI that tells it to look -- ipi_dispatch() on the
    // receiving end reads its own current_proc (percpu_get(), which
    // always resolves to whichever CPU is executing it) expecting
    // this write to already be visible by the time its handler runs.
    // Ordering this way (write, then send) rather than the reverse is
    // what guarantees that: by the time the target CPU's LAPIC
    // actually delivers the interrupt (necessarily strictly after
    // this function's lapic_send_ipi() call below), this store has
    // already completed from this CPU's perspective, and x86's own
    // memory ordering guarantees don't allow the target to observe
    // the interrupt before observing a write that happened-before it
    // on the sending side.
    target->current_proc = proc;
    proc->state = PROCESS_RUNNING;

    uint8_t dest_apic_id = g_madt_info.cpu_apic_ids[target_cpu_index];
    lapic_send_ipi(dest_apic_id, IPI_VECTOR);
    lapic_wait_ipi_sent();
}

uint64_t ipi_get_received_count(void)
{
    return percpu_get()->ipi_received_count;
}

void ipi_send_schedule(uint32_t target_cpu_index)
{
    if (!g_madt_info.valid || target_cpu_index >= g_madt_info.cpu_count)
    {
        klog_error_n("ipi_send_schedule: invalid target_cpu_index", target_cpu_index);
        return;
    }

    // Deliberately does NOT touch the target's current_proc -- that's
    // the entire difference from ipi_send_process() (see this
    // function's own header comment in Ipi.h). ipi_dispatch() on the
    // receiving end is what decides what to run, by calling
    // scheduler_pick_next() itself once this IPI arrives.
    uint8_t dest_apic_id = g_madt_info.cpu_apic_ids[target_cpu_index];
    lapic_send_ipi(dest_apic_id, IPI_VECTOR);
    lapic_wait_ipi_sent();
}

void ipi_dispatch(interrupt_frame_t *frame)
{
    (void)frame; // Unused -- a generic IPI carries no per-delivery data of its own, unlike an IRQ (device state) or exception (fault details) frame.

    percpu_get()->ipi_received_count++;

    process_t *proc = percpu_get()->current_proc;

    // SMP round-robin: no process was pre-assigned (ipi_send_process()
    // was never called for this CPU, or this CPU already finished
    // whatever it was running and went back to NULL -- see
    // ProcessLauncher.c/process lifecycle) -- try picking one from the
    // shared run queue ourselves. See Core/Tasking/Scheduler/
    // Scheduler.h's own comment on scheduler_pick_next() for why this
    // is safe to call from any CPU concurrently: two CPUs racing to
    // this exact line at the same time are guaranteed to get two
    // different processes (or one gets NULL), never the same one.
    //
    // Distinguishes ipi_send_schedule() (Ipi.h) from ipi_send_process():
    // the latter already wrote current_proc before sending, so `proc`
    // above is non-NULL and this branch is skipped entirely -- the
    // process to run was already decided by whoever called
    // ipi_send_process(), this CPU doesn't second-guess that choice by
    // also trying to pick one of its own.
    if (!proc)
    {
        proc = scheduler_pick_next();
        if (proc)
            percpu_get()->current_proc = proc;
    }

    // LAPIC EOI sent here UNCONDITIONALLY, before either branch below
    // -- critical when proc is non-NULL: proc_iretq_start() (called
    // below) never returns to this function, so anything after it
    // would simply never execute. Acknowledging the interrupt before
    // jumping away, rather than after, ensures the LAPIC always
    // considers this IPI serviced regardless of which path is taken --
    // an un-acknowledged LAPIC interrupt can block further interrupt
    // delivery to this CPU, which would be an easy way to silently
    // strand an AP running its assigned process with interrupts
    // effectively wedged.
    lapic_send_eoi();

    if (proc)
    {
        // Phase 6.3 / SMP round-robin: this AP has a process to run --
        // either pre-assigned by ipi_send_process(), or just claimed
        // above via scheduler_pick_next() -- launches it directly via
        // iretq. Does NOT return to the caller (Core/Interrupts/
        // Handlers/InterruptDispatch.c's interrupt_handler(), itself
        // called from Interrupts.asm's isr240 stub) -- the interrupt
        // frame that stub pushed, and the isr_common tail it would
        // otherwise fall through to (register restore + its own
        // iretq), are simply abandoned rather than unwound. This is
        // safe specifically because the code this interrupted was
        // ap_c_entry()'s own `for (;;) hlt` idle loop (Core/SMP/
        // Startup/AppProcessorStartup.c) -- there is nothing of value
        // on that abandoned stack to ever return to, unlike
        // interrupting genuine in-progress work would be.
        proc_iretq_start(&proc->context);
    }

    // Reached only when this CPU had no process assigned AND
    // scheduler_pick_next() found nothing PROCESS_READY either -- the
    // original Phase 5 validation behavior (Core/SMP/Startup/
    // AppProcessorStartup.c's own IPI test), unchanged.
}
