// ApexKernel/Core/Syscalls/ABI/SyscallSetup.c
// ApexOS -- Syscall MSR configuration (syscall/sysret enablement)

#include "Core/Syscalls/ABI/SyscallSetup.h"
#include "Logs/Log.h"

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
    // Enables SCE (Syscall Enable) in EFER.
    uint64_t efer = rdmsr(MSR_EFER);
    efer |= 1;          // SCE
    efer &= ~(1 << 11); // NXE = 0 -- disables No-Execute
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

    klog_ok("Syscalls initialized (syscall/sysret)");
}
