// ApexKernel/Core/Tasking/Scheduler/SchedulerStats.c
// ApexOS -- Scheduler diagnostics

#include "Core/Tasking/Scheduler/SchedulerStats.h"
#include "Core/Tasking/Scheduler/SchedulerInternal.h"
#include "Core/Boot/Log/BootLog.h"

void scheduler_print_stats(void)
{
    boot_log_ok_n("Scheduler initialized - Round-robin, processes ready", proc_count);
}
