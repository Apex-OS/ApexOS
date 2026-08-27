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

    g_old_ctx = NULL;
    g_new_ctx = NULL;

    // Ticks the current process even when it's running alone.
    if (current_proc)
        current_proc->ticks++;

    // No switch while inside a syscall.
    if (in_syscall)
        return;

    if (!proc_list || proc_count < 2)
        return;

    process_t *old = current_proc;
    if (!old)
    {
        current_proc = proc_list;
        current_proc->state = PROCESS_RUNNING;
        return;
    }

    process_t *next = current_proc->next;
    if (!next)
        next = proc_list;
    if (next == current_proc)
        return;

    old->state = PROCESS_READY;
    current_proc = next;
    current_proc->state = PROCESS_RUNNING;
    // ticks already incremented above.

    g_old_ctx = &old->context;
    g_new_ctx = &current_proc->context;
}
