// apex-kernel/core/scheduler.c
// ApexOS -- Round-robin scheduler

#include "scheduler.h"
#include "klog.h"
#include "../drivers/vga.h"
#include "../drivers/pit.h"

static process_t *proc_list = NULL;
static process_t *proc_tail = NULL;
static process_t *current_proc = NULL;
static uint32_t proc_count = 0;

// 1 while inside a syscall -- no context switch happens during that
// window (see scheduler_tick_asm below).
volatile int in_syscall = 0;

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

void scheduler_print_stats(void)
{
    klog_ok_n("Scheduler initialized - Round-robin, processes ready", proc_count);
}

void current_proc_start(process_t *proc)
{
    current_proc = proc;
    current_proc->state = PROCESS_RUNNING;

    // Disables interrupts -- iretq re-enables them via RFLAGS=0x202.
    __asm__ volatile("cli");
    proc_iretq_start(&proc->context);
}

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