// ApexKernel/Core/Syscalls/Dispatcher/SyscallDispatcher.c
// ApexOS -- Syscall dispatch table

#include "Core/Syscalls/Dispatcher/SyscallDispatcher.h"
#include "Core/Syscalls/ABI/SyscallSetup.h"
#include "Core/Syscalls/Handlers/SysExit.h"
#include "Core/Syscalls/Handlers/SysWrite.h"
#include "Core/Syscalls/Handlers/SysRead.h"
#include "Core/Syscalls/Handlers/SysGetPID.h"
#include "Core/Syscalls/Handlers/SysSleep.h"
#include "Core/Syscalls/Handlers/SysDriverCall.h"
#include "Logs/Log.h"

uint64_t syscall_handler(uint64_t num, uint64_t arg1,
                         uint64_t arg2, uint64_t arg3)
{
    switch (num)
    {
    case SYS_EXIT:
        return sys_exit(arg1);

    case SYS_WRITE:
        return sys_write(arg1, arg2, arg3);

    case SYS_READ:
        return sys_read(arg1, arg2, arg3);

    case SYS_GETPID:
        return sys_getpid();

    case SYS_SLEEP:
        return sys_sleep(arg1);

    case SYS_DRIVER_CALL:
        return sys_driver_call(arg1, arg2, arg3);

    default:
        klog_error_n("[SYSCALL] Unknown", num);
        return (uint64_t)-1;
    }
}
