// apex-kernel/core/usertest.c
// ApexOS -- First userspace program

#include "../include/apex/types.h"

// Inline syscall -- no external call.
static __attribute__((always_inline)) inline uint64_t do_syscall(uint64_t num, uint64_t arg1,
                                                                 uint64_t arg2, uint64_t arg3)
{
    uint64_t ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
        : "rcx", "r11", "memory");
    return ret;
}

// Userspace entry point -- fully inline, no external call.
//
// IMPORTANT: the function is bracketed with asm labels
// (user_main_start / user_main_end) placed in the SAME .text section,
// right before and right after the function body. This gives its
// EXACT size at link time, via (user_main_end - user_main_start),
// instead of guessing "4096 bytes" and accidentally copying
// neighboring kernel code -- that was the actual bug.
__asm__(".global user_main_start\nuser_main_start:");

void __attribute__((noinline, aligned(16))) user_main(void)
{
    const char msg[] = "Hello from userspace! Ring 3 works!\n";

    // SYS_WRITE = 1, fd=1, buf=msg, len=37 (actual message size)
    do_syscall(1, 1, (uint64_t)msg, sizeof(msg) - 1);

    // SYS_EXIT = 0
    do_syscall(0, 0, 0, 0);

    for (;;)
        ;
}

__asm__(".global user_main_end\nuser_main_end:");