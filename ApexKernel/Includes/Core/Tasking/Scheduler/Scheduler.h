// ApexKernel/Includes/Core/Tasking/Scheduler/Scheduler.h
// ApexOS -- Round-robin scheduler (process list management)

#ifndef APEX_CORE_TASKING_SCHEDULER_H
#define APEX_CORE_TASKING_SCHEDULER_H

#include "Core/Types.h"

#include "Core/Tasking/Process/Process.h"

void scheduler_init(void);
void scheduler_add(process_t *proc);
void scheduler_remove(uint32_t pid);
process_t *scheduler_current(void);

// Picks the next PROCESS_READY process in round-robin order and marks
// it PROCESS_RUNNING, or returns NULL if none is ready. SMP-safe:
// unlike the pre-round-robin migration approach of following
// current_proc->next (which assumed one single scheduling cursor
// shared by whichever CPU happened to be ticking), this walks from a
// shared rotation cursor (Scheduler.c's own proc_rotation_cursor) that
// any CPU can safely call this from -- two CPUs calling this
// concurrently are guaranteed to pick two DIFFERENT processes (or one
// gets NULL if only one was ready), never the same one, because the
// whole search-and-mark-RUNNING sequence happens under
// proc_list_lock. Does NOT change the caller's own current_proc or
// touch the process this call's caller was previously running --
// purely "find me a ready process and reserve it", leaving what the
// caller does with the result (assign to current_proc, save/restore a
// context, ignore it) entirely up to them.
process_t *scheduler_pick_next(void);

#endif // APEX_CORE_TASKING_SCHEDULER_H
