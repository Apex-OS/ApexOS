// ApexKernel Tests/Kernel/GDTTest.c
// ApexOS -- GDT self-test

#include "GDTTest.h"
#include "TestRunner.h"
#include "Drivers/Display/VGA.h"

void test_gdt(void)
{
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("GDT:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    uint16_t cs, ds, ss;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    __asm__ volatile("mov %%ds, %0" : "=r"(ds));
    __asm__ volatile("mov %%ss, %0" : "=r"(ss));

    TEST("CS = 0x08", cs == 0x08, "wrong code segment");
    TEST("DS = 0x10", ds == 0x10, "wrong data segment");
    TEST("SS = 0x10", ss == 0x10, "wrong stack segment");
}
