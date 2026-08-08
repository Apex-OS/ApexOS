// apex-kernel/core/syscall.h
// ApexOS -- Syscall interface

#ifndef APEX_SYSCALL_H
#define APEX_SYSCALL_H

#include "../include/apex/types.h"

// ── Syscall numbers ────────────────────────────────────────────────────────
#define SYS_EXIT 0
#define SYS_WRITE 1
#define SYS_READ 2
#define SYS_GETPID 3
#define SYS_SLEEP 4
#define SYS_DRIVER_CALL 5 // arg1=driver_id, arg2=func_id, arg3=generic arg (see axd.h)

// ── Public API ─────────────────────────────────────────────────────────────
void syscall_init(void);

// Records the return point (pure-asm RSP save) that SYS_EXIT must
// jump back to after a user process (see syscall.c for the details).
// Call right before any userspace_jump(), and IMMEDIATELY followed by
// syscall_mark_shell_return_point_ready() (see shell.c) -- the two
// calls are separate because the first is a "naked" function that
// can't execute any additional C code.
void syscall_set_shell_return_point(void);
void syscall_mark_shell_return_point_ready(void);

#endif // APEX_SYSCALL_H