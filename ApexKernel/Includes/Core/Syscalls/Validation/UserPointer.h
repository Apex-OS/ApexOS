// ApexKernel/Includes/Core/Syscalls/Validation/UserPointer.h
// ApexOS -- Validates that a (pointer, length) pair a Ring 3 process
// handed to a syscall actually falls inside memory that process is
// allowed to touch, before any syscall handler dereferences it.
//
// Every syscall argument that names a userspace buffer (SysWrite.c's
// buf/len, SysRead.c's buf/len) arrives completely unvalidated from
// Ring 3 -- SyscallEntry.asm just forwards whatever the process put
// in its registers. Without this check, a process can pass any
// address at all: a kernel address to leak kernel memory via
// SYS_WRITE, or a kernel address to CORRUPT via SYS_READ (which
// writes into it) -- see this header's is_valid_user_range() callers
// for the exact fix each of those two applies.
#ifndef APEX_CORE_SYSCALLS_VALIDATION_USER_POINTER_H
#define APEX_CORE_SYSCALLS_VALIDATION_USER_POINTER_H

#include "Core/Types.h"

// True if [addr, addr+len) falls entirely within one of this kernel's
// known Ring 3-accessible regions (Core/Userspace.h's USER_CODE_ADDR/
// USER_LIB_ADDR/USER_STACK_ADDR, and AXFFormat.h's
// AXF_IMPORT_TABLE_ADDR) and does not overflow when computing
// addr+len. V1 -- this kernel runs one Ring 3 process at a time at
// fixed addresses (see Core/Userspace.h's own header comment), so a
// fixed allowlist of regions is precise and sufficient; a future
// multi-process kernel with per-process page tables would instead
// walk that process's own tables to confirm the range is both present
// and user-accessible, rather than checking against a fixed list.
bool is_valid_user_range(uint64_t addr, uint64_t len);

#endif // APEX_CORE_SYSCALLS_VALIDATION_USER_POINTER_H
