// ApexKernel/Core/Syscalls/Handlers/SysDriverCall.c
// ApexOS -- SYS_DRIVER_CALL handler

#include "Core/Syscalls/Handlers/SysDriverCall.h"
#include "Formats/AXF/AXD/DriverInvocation.h"

uint64_t sys_driver_call(uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    // arg1 = driver_id, arg2 = func_id, arg3 = generic arg
    return axd_call((int)arg1, (int)arg2, arg3);
}
