// ApexKernel/Core/Panic.c
// ApexOS -- Kernel panic

#include "Core/Panic.h"
#include "Drivers/Display/VGA.h"

void kpanic(const char *msg)
{
    vga_set_color(VGA_WHITE, VGA_RED);
    vga_puts("\n!! KERNEL PANIC !!\n  ");
    vga_puts(msg);
    vga_puts("\n  System halted.\n");
    __asm__ volatile("cli");
    for (;;)
        __asm__ volatile("hlt");
}
