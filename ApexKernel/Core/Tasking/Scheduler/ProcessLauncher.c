// ApexKernel/Core/Tasking/Scheduler/ProcessLauncher.c
// ApexOS -- Launches the very first process (via iretq)

#include "Core/Tasking/Scheduler/ProcessLauncher.h"
#include "Core/Tasking/Scheduler/SchedulerInternal.h"

// proc_iretq_start: used by current_proc_start() to launch the very
// first process (via iretq rather than a real context switch). See
// ApexKernel/Arch/x86_64/ContextSwitch.asm.
extern void proc_iretq_start(cpu_context_t *ctx);

void current_proc_start(process_t *proc)
{
    current_proc = proc;
    current_proc->state = PROCESS_RUNNING;

    // Disables interrupts -- iretq re-enables them via RFLAGS=0x202.
    __asm__ volatile("cli");
    proc_iretq_start(&proc->context);
}
