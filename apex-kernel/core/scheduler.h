// apex-kernel/core/scheduler.h
// ApexOS -- Round-robin scheduler

#ifndef APEX_SCHEDULER_H
#define APEX_SCHEDULER_H

#include "process.h"

void scheduler_init(void);
void scheduler_add(process_t *proc);
void scheduler_remove(uint32_t pid);
process_t *scheduler_current(void);
void scheduler_print_stats(void);
// Starts the very first process via iretq.
void current_proc_start(process_t *proc);

// Called by IRQ0 in assembly (see arch/x86_64/idt.asm): does the
// actual scheduler tick work (advances current_proc, sets
// g_old_ctx/g_new_ctx if a switch must happen). The switch itself is
// done inline in irq0's asm, not via a C context_switch() call.
void scheduler_tick_asm(void);

// ── Context switch (assembly, arch/x86_64/context_switch.asm) ────────────
// proc_iretq_start: used by current_proc_start() to launch the very
// first process (via iretq rather than a real context switch).
extern void proc_iretq_start(cpu_context_t *ctx);

// g_old_ctx / g_new_ctx / in_syscall: shared with irq0 (idt.asm) for
// the inline context switch mechanism described above.
extern cpu_context_t *g_old_ctx;
extern cpu_context_t *g_new_ctx;
extern volatile int in_syscall;

#endif // APEX_SCHEDULER_H