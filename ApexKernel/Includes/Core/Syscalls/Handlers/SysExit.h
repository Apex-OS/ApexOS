// ApexKernel/Includes/Core/Syscalls/Handlers/SysExit.h
// ApexOS -- SYS_EXIT handler and the shell-return jump mechanism

#ifndef APEX_CORE_SYSCALLS_HANDLERS_SYS_EXIT_H
#define APEX_CORE_SYSCALLS_HANDLERS_SYS_EXIT_H

#include "Core/Types.h"

uint64_t sys_exit(uint64_t arg1);

// Records the return point (pure-asm RSP save) that SYS_EXIT must
// jump back to after a user process. Call right before any
// userspace_jump(), and IMMEDIATELY followed by
// syscall_mark_shell_return_point_ready() (see
// ApexKernel/Core/Shell/Commands/UserspaceTest.c) -- the two calls are
// separate because the first is a "naked" function that can't execute
// any additional C code.
void syscall_set_shell_return_point(void);
void syscall_mark_shell_return_point_ready(void);

#endif // APEX_CORE_SYSCALLS_HANDLERS_SYS_EXIT_H
