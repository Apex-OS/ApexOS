// ApexKernel/Includes/Core/Syscalls/Dispatcher/SyscallDispatcher.h
// ApexOS -- Syscall dispatch table

#ifndef APEX_CORE_SYSCALLS_DISPATCHER_H
#define APEX_CORE_SYSCALLS_DISPATCHER_H

#include "Core/Types.h"

// Called from ApexKernel/Arch/x86_64/SyscallEntry.asm.
// RAX = syscall number, RDI = arg1, RSI = arg2, RDX = arg3.
uint64_t syscall_handler(uint64_t num, uint64_t arg1,
                         uint64_t arg2, uint64_t arg3);

#endif // APEX_CORE_SYSCALLS_DISPATCHER_H
