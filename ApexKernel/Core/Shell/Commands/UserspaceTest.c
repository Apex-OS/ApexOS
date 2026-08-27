// ApexKernel/Core/Shell/Commands/UserspaceTest.c
// ApexOS -- Ring 3 smoke-test command

#include "Core/Shell/Commands/UserspaceTest.h"
#include "Core/Userspace.h"
#include "Core/Memory/Physical/PageBitmap.h"
#include "Core/Memory/Virtual/PageTables.h"
#include "Core/Syscalls/Handlers/SysExit.h"
#include "Arch/x86_64/Userspace.h"
#include "Drivers/Display/VGA.h"

// Defined in SysExit.c, set by the naked syscall_set_shell_return_point()
// / shell_return_jump() pair (see cmd_ring3() below and
// ApexKernel/Formats/AXF/AXP/ProcessLaunch.c, which relies on the
// exact same mechanism).
extern volatile uint64_t shell_return_bypass;

void cmd_ring3(void)
{
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("Jumping to Ring 3...\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    uint64_t user_main_size = (uint64_t)user_main_end - (uint64_t)user_main_start;
    if (user_main_size == 0 || user_main_size > 4096)
    {
        vga_puts("ring3: user_main too large or size=0 -- aborting\n");
        return;
    }

    void *code_phys = pmm_alloc_page();
    vmm_map(USER_CODE_ADDR, (uint64_t)code_phys, VMM_USER_RW);

    uint8_t *src = (uint8_t *)user_main_start;
    uint8_t *dst = (uint8_t *)USER_CODE_ADDR;
    for (uint64_t i = 0; i < 4096; i++)
        dst[i] = (i < user_main_size) ? src[i] : 0;

    for (uint64_t addr = USER_STACK_ADDR;
         addr < USER_STACK_ADDR + USER_STACK_SIZE;
         addr += 4096)
    {
        void *phys = pmm_alloc_page();
        vmm_map(addr, (uint64_t)phys, VMM_USER_RW);
    }

    uint64_t stack_top = USER_STACK_ADDR + USER_STACK_SIZE;

    syscall_set_shell_return_point();
    syscall_mark_shell_return_point_ready();

    if (shell_return_bypass == 0)
    {
        // First pass: actually jump.
        userspace_jump(USER_CODE_ADDR, stack_top);
    }

    // Reached only after SYS_EXIT's return jump (shell_return_bypass
    // == 1 at this point, see SysExit.c). The syscall handler leaves
    // interrupts disabled (cli) right before the jump back here --
    // `sti` must stay at the very end of shell_return_jump(), never
    // before the RSP restore (a race condition here was fixed once
    // already: an interrupt firing between `sti` and the RSP restore
    // corrupts the stack).
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("Back in kernel shell.\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
}
