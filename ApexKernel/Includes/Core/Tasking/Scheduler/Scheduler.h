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

#endif // APEX_CORE_TASKING_SCHEDULER_H
