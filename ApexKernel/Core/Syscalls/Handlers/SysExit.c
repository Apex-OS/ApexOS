// ApexKernel/Core/Syscalls/Handlers/SysExit.c
// ApexOS -- SYS_EXIT handler and the shell-return jump mechanism

#include "Core/Syscalls/Handlers/SysExit.h"
#include "Core/Tasking/Scheduler/SchedulerTick.h"
#include "Logs/Log.h"

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
// SyscallEntry.asm -> kernel_syscall_stack -> shell return.
//
// v3 (current): RSP is saved/restored by hand, in PURE assembly
// (naked function, no GCC-generated prologue/epilogue that could move
// RSP before it's captured). Same principle as proc_iretq_start in
// ContextSwitch.asm, just applied to a normal function return (ret)
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
// was at the moment of the `call` from Shell.c, before any prologue
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

// Call from Shell.c right after syscall_set_shell_return_point().
// Kept separate because syscall_set_shell_return_point() is "naked"
// (no C instructions can be generated in its body, only the
// hand-written asm) -- this small setter does the normal C-side work
// instead.
void syscall_mark_shell_return_point_ready(void)
{
    shell_return_point_set = 1;
}

uint64_t sys_exit(uint64_t arg1)
{
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

        // 4. Reloads the KERNEL data segments -- gs is deliberately
        // NOT included here, unlike ds/es/fs. IA32_GS_BASE is set
        // per-CPU via wrmsr() by Core/SMP/PerCPU/PerCpu.c's
        // percpu_init_this_cpu() (see that file), completely
        // independent of the gs SELECTOR's own GDT descriptor -- but
        // explicitly loading the gs selector with `mov %ax, %gs`
        // reloads its hidden base from that descriptor as a side
        // effect on real hardware, silently clobbering the per-CPU
        // GS_BASE this kernel actually relies on (percpu_get(),
        // called constantly by the SMP code active since Init.c's own
        // SMP bring-up) back to whatever flat base the GDT's data
        // descriptor defines (0) -- breaking percpu_get() for this
        // CPU for the rest of the kernel's life the moment any syscall
        // returns via sys_exit(). ds/es/fs carry no such hazard: only
        // gs's base is ever read via `mov gs:offset` addressing
        // anywhere in this kernel.
        __asm__ volatile(
            "mov $0x10, %%ax\n"
            "mov %%ax, %%ds\n"
            "mov %%ax, %%es\n"
            "mov %%ax, %%fs\n"
            :
            :
            : "ax");

        // 5. Returns to the shell.
        shell_return_jump();
    }

    // DIAGNOSTIC -- si ce log apparait jamais, shell_return_point_set
    // etait faux au moment ou sys_exit a tourne (donc `ring3`/`run` n'a
    // jamais appele syscall_set_shell_return_point()/
    // syscall_mark_shell_return_point_ready() avant de lancer le
    // programme -- chercher un chemin de code qui saute cette etape).
    klog_error("sys_exit: shell_return_point_set was FALSE -- halting forever");
    __asm__ volatile("cli; hlt");
    return 0;
}
