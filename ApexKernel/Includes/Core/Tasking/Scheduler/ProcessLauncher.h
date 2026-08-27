// ApexKernel/Includes/Core/Tasking/Scheduler/ProcessLauncher.h
// ApexOS -- Launches the very first process (via iretq)

#ifndef APEX_CORE_TASKING_PROCESS_LAUNCHER_H
#define APEX_CORE_TASKING_PROCESS_LAUNCHER_H

#include "Core/Tasking/Process/Process.h"

// Starts the very first process via iretq.
void current_proc_start(process_t *proc);

#endif // APEX_CORE_TASKING_PROCESS_LAUNCHER_H
