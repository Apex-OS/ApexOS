// ApexKernel/Includes/Core/Tasking/Scheduler/SchedulerInternal.h
// ApexOS -- Shared internal state for the Scheduler module
//
// proc_list/proc_tail/current_proc/proc_count are defined once in
// Scheduler.c and shared via extern with SchedulerTick.c and
// SchedulerStats.c. Not part of the public Scheduler.h API.

#ifndef APEX_CORE_TASKING_SCHEDULER_INTERNAL_H
#define APEX_CORE_TASKING_SCHEDULER_INTERNAL_H

#include "Core/Types.h"

#include "Core/Tasking/Scheduler/Scheduler.h"

extern process_t *proc_list;
extern process_t *proc_tail;
extern process_t *current_proc;
extern uint32_t proc_count;

#endif // APEX_CORE_TASKING_SCHEDULER_INTERNAL_H
