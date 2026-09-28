// ApexKernel/Core/Syscalls/ABI/SyscallSetup.c
// ApexOS -- Syscall MSR configuration (syscall/sysret enablement)

#include "Core/Syscalls/ABI/SyscallSetup.h"
#include "Core/Boot/Log/BootLog.h"

#define MSR_EFER 0xC0000080
#define MSR_STAR 0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_SFMASK 0xC0000084

static void wrmsr(uint32_t msr, uint64_t val)
{
    uint32_t lo = val & 0xFFFFFFFF;
    uint32_t hi = val >> 32;
    __asm__ volatile("wrmsr" ::"c"(msr), "a"(lo), "d"(hi));
}

static uint64_t rdmsr(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

void syscall_init(void)
{
    // Enables SCE (Syscall Enable) and NXE (No-Execute Enable) in
    // EFER. NXE lets page table entries mark a page non-executable
    // (VMM_NX, PageTables.h) -- without it, the NX bit in every page
    // table entry is simply ignored by the CPU and every mapped page
    // stays executable no matter what flags vmm_map() was given,
    // which is what let a bug elsewhere (an unvalidated Ring 3 SYS_READ
    // pointer, since fixed -- see Core/Syscalls/Validation/
    // UserPointer.h) turn into a straightforward path to arbitrary
    // Ring 0 code execution: kernel memory a process could get
    // SYS_READ to write into was also memory the CPU would happily
    // execute if jumped to. NXE alone doesn't fix that -- it only
    // makes the NX bit meaningful once set; see IdentityMap.c and
    // Process.c for where VMM_NX is actually applied to real
    // data/stack pages now that this makes doing so worth anything.
    uint64_t efer = rdmsr(MSR_EFER);
    efer |= 1;         // SCE
    efer |= (1 << 11); // NXE
    wrmsr(MSR_EFER, efer);

    // STAR: segments for syscall/sysret.
    // [63:48] = User CS-8 (sysret CS = STAR[63:48] + 16, SS = STAR[63:48] + 8)
    // [47:32] = Kernel CS (syscall CS = STAR[47:32], SS = STAR[47:32] + 8)
    uint64_t star = ((uint64_t)0x0018 << 48) | ((uint64_t)0x0008 << 32);
    wrmsr(MSR_STAR, star);

    // LSTAR: address of the syscall handler.
    extern void syscall_entry(void);
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);

    // SFMASK: RFLAGS mask applied on syscall entry (clears IF).
    wrmsr(MSR_SFMASK, (1 << 9)); // masks IF

    boot_log_ok("Syscalls initialized (syscall/sysret)");
}
