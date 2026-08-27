// ApexKernel/Includes/Core/Syscalls/ABI/SyscallSetup.h
// ApexOS -- Syscall MSR configuration (syscall/sysret enablement)
//
// Owns the syscall number constants, shared across every file in
// Core/Syscalls/.

#ifndef APEX_CORE_SYSCALLS_ABI_SETUP_H
#define APEX_CORE_SYSCALLS_ABI_SETUP_H

#include "Core/Types.h"

#define SYS_EXIT 0
#define SYS_WRITE 1
#define SYS_READ 2
#define SYS_GETPID 3
#define SYS_SLEEP 4
#define SYS_DRIVER_CALL 5 // arg1=driver_id, arg2=func_id, arg3=generic arg (see AXD.h)

void syscall_init(void);

#endif // APEX_CORE_SYSCALLS_ABI_SETUP_H
