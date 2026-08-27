// ApexKernel/Core/Syscalls/Handlers/SysGetPID.c
// ApexOS -- SYS_GETPID handler

#include "Core/Syscalls/Handlers/SysGetPID.h"
#include "Core/Tasking/Scheduler/Scheduler.h"

uint64_t sys_getpid(void)
{
    process_t *cur = scheduler_current();
    return cur ? cur->pid : 0;
}
