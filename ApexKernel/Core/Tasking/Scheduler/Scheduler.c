// ApexKernel/Core/Tasking/Scheduler/Scheduler.c
// ApexOS -- Round-robin scheduler (process list management)

#include "Core/Tasking/Scheduler/Scheduler.h"
#include "Core/Tasking/Scheduler/SchedulerInternal.h"

process_t *proc_list = NULL;
process_t *proc_tail = NULL;
process_t *current_proc = NULL;
uint32_t proc_count = 0;

void scheduler_init(void)
{
    proc_list = NULL;
    proc_tail = NULL;
    current_proc = NULL;
    proc_count = 0;
}

void scheduler_add(process_t *proc)
{
    if (!proc)
        return;
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
}

void scheduler_remove(uint32_t pid)
{
    if (!proc_list)
        return;
    if (proc_list->pid == pid)
    {
        proc_list = proc_list->next;
        if (!proc_list)
            proc_tail = NULL;
        proc_count--;
        return;
    }
    process_t *cur = proc_list;
    while (cur->next)
    {
        if (cur->next->pid == pid)
        {
            if (cur->next == proc_tail)
                proc_tail = cur;
            cur->next = cur->next->next;
            proc_count--;
            return;
        }
        cur = cur->next;
    }
}

process_t *scheduler_current(void)
{
    return current_proc;
}
