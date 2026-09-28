// ApexKernel/Includes/Core/Syscalls/Handlers/SysRead.h
// ApexOS -- SYS_READ handler

#ifndef APEX_CORE_SYSCALLS_HANDLERS_SYS_READ_H
#define APEX_CORE_SYSCALLS_HANDLERS_SYS_READ_H

#include "Core/Types.h"

// arg1 = fd (only 0/stdin supported in V1 -- reading from a real file
// via this syscall would need a file descriptor table this kernel
// doesn't have yet, see Core/Tasking/Process/Process.h; every .axp
// wanting file I/O today goes through Formats/AXF/AXD/ instead),
// arg2 = buf (userspace pointer to write bytes into), arg3 = len
// (max bytes to write into buf).
//
// Blocks (via Drivers/Input/Keyboard/KeyBuffer.h's keyboard_getchar(),
// itself a busy-wait) until at least one byte is available, then
// drains whatever is ALREADY buffered up to `len` bytes without
// blocking further -- this is a "read what's ready" syscall (like a
// real read() on a pty in canonical-off mode), not a "wait for a full
// line" one: no line editing, backspace handling, or Enter-terminated
// buffering happens here or anywhere below this call. A calculator
// reading one key at a time (Core/Syscalls/Handlers/SysRead.h's own
// caller, e.g. an .axp built against this syscall) is expected to
// call this in a loop, one key at a time (len=1), and do its own
// echo/backspace/editing in userspace if it wants any -- V1 keeps the
// kernel side of this deliberately minimal.
uint64_t sys_read(uint64_t arg1, uint64_t arg2, uint64_t arg3);

#endif // APEX_CORE_SYSCALLS_HANDLERS_SYS_READ_H
