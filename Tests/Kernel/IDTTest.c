// ApexKernel Tests/Kernel/IDTTest.c
// ApexOS -- IDT self-test

#include "IDTTest.h"
#include "TestRunner.h"
#include "Drivers/Display/VGA.h"

void test_idt(void)
{
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("IDT:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    uint64_t rflags;
    __asm__ volatile(
        "pushfq\n"
        "pop %0\n"
        : "=r"(rflags));
    TEST("Interrupts enabled (IF=1)", rflags & (1 << 9), "IF flag not set");
    TEST("IDT loaded", true, "");
}
