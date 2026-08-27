// ApexKernel/Core/Tasking/Process/Process.c
// ApexOS -- Process management

#include "Core/Tasking/Process/Process.h"
#include "Core/Memory/Heap/Heap.h"

static uint32_t next_pid = 1;

static void copy_name(char *dst, const char *src, size_t max)
{
    size_t i = 0;
    while (i < max - 1 && src[i])
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

process_t *process_create(const char *name, void (*entry)(void))
{
    process_t *proc = (process_t *)kmalloc(sizeof(process_t));
    if (!proc)
        return NULL;

    uint8_t *p = (uint8_t *)proc;
    for (size_t i = 0; i < sizeof(process_t); i++)
        p[i] = 0;

    proc->pid = next_pid++;
    copy_name(proc->name, name, 32);
    proc->state = PROCESS_READY;
    proc->ticks = 0;
    proc->next = NULL;

    proc->stack_base = (uint64_t)kmalloc(PROCESS_STACK_SIZE);
    if (!proc->stack_base)
    {
        kfree(proc);
        return NULL;
    }
    proc->stack_top = proc->stack_base + PROCESS_STACK_SIZE;

    // CPU context set up AFTER the zero-init above.
    proc->context.rip = (uint64_t)entry;
    proc->context.rsp = proc->stack_top - 8;
    proc->context.cs = 0x08;
    proc->context.ss = 0x10;
    proc->context.rflags = 0x202; // IF=1

    return proc;
}

void process_destroy(process_t *proc)
{
    if (!proc)
        return;
    if (proc->stack_base)
        kfree((void *)proc->stack_base);
    kfree(proc);
}
