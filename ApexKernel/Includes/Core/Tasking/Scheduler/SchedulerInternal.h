// ApexKernel/Includes/Core/Tasking/Scheduler/SchedulerInternal.h
// ApexOS -- Shared internal state for the Scheduler module
//
// proc_list/proc_tail/proc_count are defined once in Scheduler.c and
// shared via extern with SchedulerTick.c and SchedulerStats.c. Not
// part of the public Scheduler.h API.
//
// current_proc (Phase 6.2): no longer a plain global variable.
// Redefined below as a macro expanding to percpu_get()->current_proc
// (Core/SMP/PerCPU/PerCpu.h) -- every existing read/write site
// (SchedulerTick.c, Scheduler.c's own scheduler_current(),
// ProcessLauncher.c's current_proc_start()) continues to compile
// completely unchanged: `current_proc = x`, `current_proc->field`,
// `if (current_proc)` all still work exactly as written, but now
// transparently read/write THIS CPU's own percpu_t slot instead of
// one kernel-wide global.
//
// SMP round-robin (post-Phase-6.3): scheduler_tick_asm() no longer
// advances through the process list by following its own
// current_proc->next -- that only ever worked because exactly one
// CPU (the BSP) was doing the advancing. It now calls
// scheduler_pick_next_locked() below, the same shared, lock-protected
// round-robin search every CPU (BSP tick or otherwise) draws from --
// see Scheduler.c's own comment on proc_rotation_cursor for why a
// single shared cursor, not a per-CPU one, is what makes "round-robin"
// mean the same thing regardless of which CPU is asking.

#ifndef APEX_CORE_TASKING_SCHEDULER_INTERNAL_H
#define APEX_CORE_TASKING_SCHEDULER_INTERNAL_H

#include "Core/Types.h"
#include "Core/Synchronization/Spinlock/Spinlock.h"
#include "Core/SMP/PerCPU/PerCpu.h"

#include "Core/Tasking/Scheduler/Scheduler.h"

extern process_t *proc_list;
extern process_t *proc_tail;
extern uint32_t proc_count;

// See this file's own header comment for the full explanation of why
// this is a macro rather than a variable declaration as of Phase 6.2.
#define current_proc (percpu_get()->current_proc)

// Protects proc_list/proc_tail/proc_count (and the rotation cursor,
// see Scheduler.c) against concurrent access from more than one CPU.
// Added as part of the project's SMP roadmap (Phase 6.1, scheduler
// SMP-safety) -- before this, scheduler_add()/scheduler_remove()
// (Scheduler.c) and scheduler_tick_asm()'s own read of proc_list/
// proc_count (SchedulerTick.c) had no protection at all.
//
// Deliberately does NOT cover current_proc: current_proc is a THIS-
// CPU-ONLY value (via the GS-based percpu_get()), so there is no
// cross-CPU race on it to protect against -- each CPU only ever reads/
// writes its own slot.
extern spinlock_t proc_list_lock;

// Lock-free core of the public scheduler_pick_next() (Scheduler.h) --
// caller MUST already hold proc_list_lock. Exists specifically for
// SchedulerTick.c's scheduler_tick_asm(), which acquires proc_list_lock
// itself before this point (for its own tick-increment/in_syscall
// checks) and would deadlock calling the lock-acquiring public
// version from inside that already-held section -- see Scheduler.c's
// own comment on this function for the full reasoning.
process_t *scheduler_pick_next_locked(void);

#endif // APEX_CORE_TASKING_SCHEDULER_INTERNAL_H
