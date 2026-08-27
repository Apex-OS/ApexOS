// ApexKernel Tests/Kernel/PITTest.c
// ApexOS -- PIT self-test

#include "PITTest.h"
#include "TestRunner.h"
#include "Drivers/Display/VGA.h"
#include "Core/Time/PITCounter.h"

void test_pit(void)
{
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("PIT:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    uint64_t t1 = pit_get_ticks();
    uint64_t wait = 0;
    while (pit_get_ticks() == t1 && wait < 10000000)
        wait++;
    uint64_t t2 = pit_get_ticks();

    TEST("PIT ticking", t2 > t1, "no tick detected");
    TEST("PIT ticks > 0", t2 > 0, "ticks = 0");
}
