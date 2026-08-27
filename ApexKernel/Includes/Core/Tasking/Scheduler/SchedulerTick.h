// ApexKernel/Includes/Core/Tasking/Scheduler/SchedulerTick.h
// ApexOS -- Preemptive scheduler tick (called from IRQ0 asm)

#ifndef APEX_CORE_TASKING_SCHEDULER_TICK_H
#define APEX_CORE_TASKING_SCHEDULER_TICK_H

#include "Core/Tasking/Process/Process.h"

// Called by IRQ0 in assembly (see
// ApexKernel/Arch/x86_64/Interrupts/TimerInterrupt.asm): does the
// actual scheduler tick work (advances current_proc, sets
// g_old_ctx/g_new_ctx if a switch must happen). The switch itself is
// done inline in irq0's asm, not via a C context_switch() call.
void scheduler_tick_asm(void);

// g_old_ctx / g_new_ctx / in_syscall: shared with irq0
// (TimerInterrupt.asm) for the inline context switch mechanism
// described above. Also referenced from SysExit.c (cleared on
// SYS_EXIT -- no process to switch away from anymore).
extern cpu_context_t *g_old_ctx;
extern cpu_context_t *g_new_ctx;
extern volatile int in_syscall;

#endif // APEX_CORE_TASKING_SCHEDULER_TICK_H
