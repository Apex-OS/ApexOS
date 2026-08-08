// apex-kernel/core/syscall.c
// ApexOS -- Syscall handler

#include "syscall.h"
#include "scheduler.h"
#include "tss.h"
#include "klog.h"
#include "axf/axd.h"
#include "../drivers/vga.h"

// ── Return to the shell after a user process (SYS_EXIT) ──────────────────
// No real process_exit()/scheduler handoff back to the calling process
// yet: there's only one "real" process (the shell), and user programs
// are launched synchronously from it (e.g. `ring3`).
//
// BEFORE (v1): SYS_EXIT did "cli; hlt" forever -- the shell never got
// control back, the machine stayed halted.
//
// BEFORE (v2): GCC's __builtin_setjmp/__builtin_longjmp were used. The
// "Back in kernel shell" message printed fine, but a #GP (General
// Protection Fault) followed right after, with RSP/RBP completely
// corrupted. __builtin_setjmp/longjmp are meant for a restricted use
// case and aren't reliable here for restoring a context across
// syscall_entry.asm -> kernel_syscall_stack -> shell return.
//
// v3 (current): RSP is saved/restored by hand, in PURE assembly
// (naked function, no GCC-generated prologue/epilogue that could move
// RSP before it's captured). Same principle as proc_iretq_start in
// context_switch.asm, just applied to a normal function return (ret)
// instead of an iretq.
static uint64_t __attribute__((used)) shell_return_rsp = 0;
static uint64_t __attribute__((used)) shell_return_rbp = 0;
static uint64_t __attribute__((used)) shell_return_rbx = 0;
static uint64_t __attribute__((used)) shell_return_r12 = 0;
static uint64_t __attribute__((used)) shell_return_r13 = 0;
static uint64_t __attribute__((used)) shell_return_r14 = 0;
static uint64_t __attribute__((used)) shell_return_r15 = 0;

volatile uint64_t shell_return_bypass = 0;

// naked: NO GCC prologue/epilogue (no push rbp, no callee-saved
// register spills) -- the function body is EXACTLY the assembly
// written here, nothing else. Needed to capture the real RSP as it
// was at the moment of the `call` from shell.c, before any prologue
// could have already changed it.
__attribute__((naked)) void syscall_set_shell_return_point(void)
{
    __asm__ volatile(
        "movq $0, shell_return_bypass\n\t" // 0 = first pass
        "movq %%rsp, shell_return_rsp\n\t"
        "movq %%rbp, shell_return_rbp\n\t"
        "movq %%rbx, shell_return_rbx\n\t"
        "movq %%r12, shell_return_r12\n\t"
        "movq %%r13, shell_return_r13\n\t"
        "movq %%r14, shell_return_r14\n\t"
        "movq %%r15, shell_return_r15\n\t"
        "ret\n\t"
        :
        :
        : "memory");
}

// IMPORTANT -- `sti` is done HERE, AFTER RSP has been restored, not
// before calling this function. BEFORE this fix, `sti` ran in
// syscall_handler right before calling shell_return_jump() -- between
// that sti and the actual RSP restore below, the PIT could fire while
// RSP still pointed at kernel_syscall_stack (syscall_handler's own
// stack), not the shell's real stack. irq0 then saw CS=0x08 (already
// back in Ring 0, so classified as Ring0->Ring0) but with an invalid
// RSP for that path, which corrupted the stack and ended in a #GP
// (13) on the next iretq. Doing `sti` only once RSP is restored
// closes that window entirely -- there's no longer a point where an
// interrupt can land with an inconsistent RSP.
__attribute__((naked, noreturn)) static void shell_return_jump(void)
{
    __asm__ volatile(
        "movq $1, shell_return_bypass\n\t" // 1 = returning from SYS_EXIT
        "movq shell_return_rsp, %%rsp\n\t" // restores the clean stack
        "movq shell_return_rbp, %%rbp\n\t" // restores a valid C frame pointer!
        "movq shell_return_rbx, %%rbx\n\t"
        "movq shell_return_r12, %%r12\n\t"
        "movq shell_return_r13, %%r13\n\t"
        "movq shell_return_r14, %%r14\n\t"
        "movq shell_return_r15, %%r15\n\t"
        "sti\n\t"
        "ret\n\t"
        :
        :
        : "memory");
}

static volatile int shell_return_point_set = 0;

// Call from shell.c right after syscall_set_shell_return_point().
// Kept separate because syscall_set_shell_return_point() is "naked"
// (no C instructions can be generated in its body, only the
// hand-written asm) -- this small setter does the normal C-side work
// instead.
void syscall_mark_shell_return_point_ready(void)
{
    shell_return_point_set = 1;
}

// ── MSR addresses ─────────────────────────────────────────────────────────
#define MSR_EFER 0xC0000080
#define MSR_STAR 0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_SFMASK 0xC0000084

// ── MSR helpers ────────────────────────────────────────────────────────────
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

// ── C syscall handler ─────────────────────────────────────────────────────
// Called from syscall_entry.asm.
// RAX = syscall number
// RDI = arg1, RSI = arg2, RDX = arg3
uint64_t syscall_handler(uint64_t num, uint64_t arg1,
                         uint64_t arg2, uint64_t arg3)
{
    switch (num)
    {
    case SYS_EXIT:
        // Terminates the current process.
        klog_info_n("[SYSCALL] exit", arg1);

        if (shell_return_point_set)
        {
            // 1. Disable interrupts immediately.
            __asm__ volatile("cli");

            // 2. Signal that we're leaving syscall mode.
            in_syscall = 0;

            // 3. Clears any pending context-switch request -- there is
            // no process to switch away from anymore, we're jumping
            // straight back to the caller's C frame.
            extern cpu_context_t *g_old_ctx;
            extern cpu_context_t *g_new_ctx;
            g_old_ctx = NULL;
            g_new_ctx = NULL;

            // 4. Reloads the KERNEL data segments.
            __asm__ volatile(
                "mov $0x10, %%ax\n"
                "mov %%ax, %%ds\n"
                "mov %%ax, %%es\n"
                "mov %%ax, %%fs\n"
                "mov %%ax, %%gs\n"
                :
                :
                : "ax");

            // 5. Returns to the shell.
            shell_return_jump();
        }

        __asm__ volatile("cli; hlt");
        return 0;

    case SYS_WRITE:
        // arg1 = fd, arg2 = buf, arg3 = len
        if (arg1 == 1 || arg1 == 2)
        { // stdout or stderr
            const char *buf = (const char *)arg2;
            uint64_t len = arg3;
            for (uint64_t i = 0; i < len; i++)
                vga_putchar(buf[i]);
            return len;
        }
        return (uint64_t)-1;

    case SYS_GETPID:
    {
        process_t *cur = scheduler_current();
        return cur ? cur->pid : 0;
    }

    case SYS_SLEEP:
        // arg1 = ms
        // Basic busy-wait for now.
        return 0;

    case SYS_DRIVER_CALL:
        // arg1 = driver_id, arg2 = func_id, arg3 = generic arg
        return axd_call((int)arg1, (int)arg2, arg3);

    default:
        klog_error_n("[SYSCALL] Unknown", num);
        return (uint64_t)-1;
    }
}

// ── Syscall init ───────────────────────────────────────────────────────────
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