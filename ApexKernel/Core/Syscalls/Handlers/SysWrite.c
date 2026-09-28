// ApexKernel/Core/Syscalls/Handlers/SysWrite.c
// ApexOS -- SYS_WRITE handler

#include "Core/Syscalls/Handlers/SysWrite.h"
#include "Core/Syscalls/Validation/UserPointer.h"
#include "Drivers/Display/VGA.h"

uint64_t sys_write(uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    // arg1 = fd, arg2 = buf, arg3 = len
    if (arg1 == 1 || arg1 == 2)
    { // stdout or stderr
        // arg2/arg3 come straight from the calling Ring 3 process,
        // completely unvalidated by SyscallEntry.asm -- without this
        // check, a process could point buf at kernel memory and read
        // it out through this syscall's own vga_putchar() loop below,
        // an arbitrary kernel-memory-read primitive. See
        // UserPointer.h for what counts as a valid range.
        if (!is_valid_user_range(arg2, arg3))
            return (uint64_t)-1;

        const char *buf = (const char *)arg2;
        uint64_t len = arg3;
        for (uint64_t i = 0; i < len; i++)
            vga_putchar(buf[i]);
        return len;
    }
    return (uint64_t)-1;
}
