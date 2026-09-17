// ApexKernel/Core/Tasking/Scheduler/SchedulerTick.c
// ApexOS -- Preemptive scheduler tick (called from IRQ0 asm)

#include "Core/Tasking/Scheduler/SchedulerTick.h"
#include "Core/Tasking/Scheduler/SchedulerInternal.h"
#include "Core/Time/PITCounter.h"

// 1 while inside a syscall -- no context switch happens during that
// window (see below).
volatile int in_syscall = 0;

// Called from IRQ0 (asm). Must return old_ctx in RAX, new_ctx in RDX.
cpu_context_t *g_old_ctx = NULL;
cpu_context_t *g_new_ctx = NULL;

void __attribute__((noinline)) scheduler_tick_asm(void)
{
    pit_tick();

    // Guards against a timer tick firing before Init.c's own
    // percpu_init_this_cpu(0) call has run -- interrupts are enabled
    // (Init.c's `sti`) well before that call, to get real boot-log
    // timestamps flowing early. Any tick in that window would
    // otherwise dereference current_proc (this file's own macro over
    // Core/SMP/PerCPU/PerCpu.h's percpu_get()) while GS_BASE is still
    // unset -- confirmed via -d int as the exact cause of a General
    // Protection Fault the very first time this code path was
    // exercised with per-CPU storage. See PerCpu.h's own comment on
    // g_percpu_ready for the full explanation.
    if (!g_percpu_ready)
    {
        g_old_ctx = NULL;
        g_new_ctx = NULL;
        return;
    }

    g_old_ctx = NULL;
    g_new_ctx = NULL;

    // Plain spinlock_acquire() here, NOT the _irqsave variant
    // Scheduler.c's scheduler_add()/scheduler_remove() use -- this
    // function is only ever reached from the IRQ0 handler
    // (TimerInterrupt.asm), which the CPU has already entered with
    // interrupts disabled (standard x86 IRQ entry behavior, not
    // something this code arranges itself) and keeps that way for
    // this function's entire duration -- there is no further nested
    // interrupt this specific CPU could take before returning, so
    // there is nothing for an _irqsave variant to additionally
    // protect against here that plain cli-on-entry hasn't already
    // covered. See SchedulerInternal.h's own comment on proc_list_lock
    // for why this lock exists in the first place (protecting against
    // scheduler_add()/scheduler_remove() running concurrently on a
    // DIFFERENT CPU, not against nested interrupts on this one).
    spinlock_acquire(&proc_list_lock);

    // Ticks the current process even when it's running alone.
    if (current_proc)
        current_proc->ticks++;

    // No switch while inside a syscall.
    if (in_syscall)
    {
        spinlock_release(&proc_list_lock);
        return;
    }

    if (!proc_list || proc_count < 2)
    {
        spinlock_release(&proc_list_lock);
        return;
    }

    process_t *old = current_proc;

    // SMP round-robin: scheduler_pick_next_locked() (Scheduler.c) is
    // the single shared search every CPU's tick draws from -- see
    // SchedulerInternal.h's own comment on why this replaced following
    // current_proc->next directly. Marks whatever it finds
    // PROCESS_RUNNING itself; this function only needs to decide what
    // to do with `old` (the process THIS CPU was running before this
    // tick, if any) around that pick.
    process_t *next = scheduler_pick_next_locked();

    if (!next)
    {
        // Nothing else is PROCESS_READY right now -- every other
        // process is already RUNNING (elsewhere), BLOCKED, or DEAD.
        // `old` (if any) keeps running uninterrupted; no switch this
        // tick. Distinct from the !old case just below: this is "I
        // have a process and nothing better is available", not "I
        // have nothing running yet".
        spinlock_release(&proc_list_lock);
        return;
    }

    if (!old)
    {
        // This CPU had nothing running -- just start `next` outright,
        // no outgoing context to save.
        current_proc = next;
        spinlock_release(&proc_list_lock);
        return;
    }

    if (next == old)
    {
        // Defensive only -- should be unreachable in practice.
        // scheduler_pick_next_locked() only ever returns a process it
        // found with state == PROCESS_READY, and `old` is still
        // PROCESS_RUNNING at this point (its state hasn't been touched
        // since it was set RUNNING by whatever pick put it there) --
        // so the search performed by the call above could not have
        // matched it. Kept as a guard rather than removed: if a future
        // change to process state transitions ever made this
        // reachable, this avoids the alternative of context-switching
        // a process into itself (which the save/restore sequence below
        // was never designed to handle -- g_old_ctx and g_new_ctx would
        // both point at the exact same cpu_context_t, and
        // ContextSwitch.asm's save-then-restore would read back
        // whatever it just wrote, not necessarily preserving execution
        // correctly through the switch).
        spinlock_release(&proc_list_lock);
        return;
    }

    old->state = PROCESS_READY;
    current_proc = next;
    // ticks already incremented above.

    g_old_ctx = &old->context;
    g_new_ctx = &current_proc->context;

    spinlock_release(&proc_list_lock);
}
