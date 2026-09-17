// ApexKernel/Core/Tasking/Scheduler/Scheduler.c
// ApexOS -- Round-robin scheduler (process list management)

#include "Core/Tasking/Scheduler/Scheduler.h"
#include "Core/Tasking/Scheduler/SchedulerInternal.h"

process_t *proc_list = NULL;
process_t *proc_tail = NULL;
uint32_t proc_count = 0;
spinlock_t proc_list_lock = SPINLOCK_INIT;

// Round-robin rotation cursor -- where the next scheduler_pick_next()
// call resumes searching from, shared across every CPU rather than
// each CPU tracking its own position (which is what following
// current_proc->next effectively did before this file's SMP
// round-robin migration: correct only because exactly one CPU, the
// BSP, was ever the one advancing it). Always protected by
// proc_list_lock alongside proc_list itself -- see
// scheduler_pick_next()'s own comment on why using a plain global
// here, rather than something per-CPU, is the correct choice for a
// cursor that's supposed to represent one shared rotation order all
// CPUs pull from together.
static process_t *proc_rotation_cursor = NULL;

void scheduler_init(void)
{
    uint64_t flags;
    spinlock_acquire_irqsave(&proc_list_lock, &flags);
    proc_list = NULL;
    proc_tail = NULL;
    current_proc = NULL;
    proc_count = 0;
    proc_rotation_cursor = NULL;
    spinlock_release_irqrestore(&proc_list_lock, flags);
}

void scheduler_add(process_t *proc)
{
    if (!proc)
        return;

    uint64_t flags;
    spinlock_acquire_irqsave(&proc_list_lock, &flags);
    proc->next = NULL;
    proc->state = PROCESS_READY;
    if (!proc_list)
    {
        proc_list = proc;
        proc_tail = proc;
    }
    else
    {
        proc_tail->next = proc;
        proc_tail = proc;
    }
    proc_count++;
    spinlock_release_irqrestore(&proc_list_lock, flags);
}

void scheduler_remove(uint32_t pid)
{
    uint64_t flags;
    spinlock_acquire_irqsave(&proc_list_lock, &flags);

    if (!proc_list)
    {
        spinlock_release_irqrestore(&proc_list_lock, flags);
        return;
    }

    if (proc_list->pid == pid)
    {
        // If the rotation cursor was pointing at the process being
        // removed, advance it first -- otherwise scheduler_pick_next()
        // would resume searching from a pointer to memory that's about
        // to become invalid (process_destroy() typically frees `proc`
        // shortly after this call returns, see ProcessLauncher.c/
        // whatever caller invoked scheduler_remove()). Advancing to
        // proc_list->next mirrors the natural next stop in the
        // rotation anyway -- the removed process was going to be
        // skipped on the next pick regardless, this just does it a
        // step early rather than leaving a dangling reference in the
        // meantime.
        if (proc_rotation_cursor == proc_list)
            proc_rotation_cursor = proc_list->next;

        proc_list = proc_list->next;
        if (!proc_list)
            proc_tail = NULL;
        proc_count--;
        spinlock_release_irqrestore(&proc_list_lock, flags);
        return;
    }

    process_t *cur = proc_list;
    while (cur->next)
    {
        if (cur->next->pid == pid)
        {
            if (proc_rotation_cursor == cur->next)
                proc_rotation_cursor = cur->next->next;

            if (cur->next == proc_tail)
                proc_tail = cur;
            cur->next = cur->next->next;
            proc_count--;
            spinlock_release_irqrestore(&proc_list_lock, flags);
            return;
        }
        cur = cur->next;
    }

    spinlock_release_irqrestore(&proc_list_lock, flags);
}

process_t *scheduler_current(void)
{
    return current_proc;
}

// Lock-free core of scheduler_pick_next() -- caller MUST already hold
// proc_list_lock. Split out specifically so Core/Tasking/Scheduler/
// SchedulerTick.c's scheduler_tick_asm() (which acquires proc_list_lock
// itself, for the tick-increment/in_syscall checks that come before
// any pick attempt) can reuse this exact same search-and-mark logic
// without a nested spinlock_acquire() on a lock it's already holding
// -- spinlocks are not reentrant, and calling the public
// scheduler_pick_next() (which acquires the lock itself) from inside
// an already-locked section would deadlock immediately, the same
// class of bug Spinlock.h's own _irqsave variants exist to prevent
// for the interrupt-handler case specifically.
process_t *scheduler_pick_next_locked(void)
{
    if (!proc_list)
        return NULL;

    process_t *start = proc_rotation_cursor ? proc_rotation_cursor->next : NULL;
    if (!start)
        start = proc_list;

    process_t *candidate = start;
    do
    {
        if (candidate->state == PROCESS_READY)
        {
            candidate->state = PROCESS_RUNNING;
            proc_rotation_cursor = candidate;
            return candidate;
        }
        candidate = candidate->next ? candidate->next : proc_list;
    } while (candidate != start);

    return NULL;
}

process_t *scheduler_pick_next(void)
{
    uint64_t flags;
    spinlock_acquire_irqsave(&proc_list_lock, &flags);
    process_t *result = scheduler_pick_next_locked();
    spinlock_release_irqrestore(&proc_list_lock, flags);
    return result;
}
