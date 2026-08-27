// ApexKernel/Core/Syscalls/Handlers/SysSleep.c
// ApexOS -- SYS_SLEEP handler

#include "Core/Syscalls/Handlers/SysSleep.h"

uint64_t sys_sleep(uint64_t arg1)
{
    // arg1 = ms
    // Basic busy-wait for now.
    (void)arg1;
    return 0;
}
