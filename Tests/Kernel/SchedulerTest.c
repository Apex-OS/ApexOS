// ApexKernel Tests/Kernel/SchedulerTest.c
// ApexOS -- Scheduler self-test

#include "SchedulerTest.h"
#include "TestRunner.h"
#include "Drivers/Display/VGA.h"
#include "Core/Tasking/Scheduler/Scheduler.h"
#include "Core/Time/PITCounter.h"

void test_scheduler(void)
{
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("Scheduler:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    process_t *cur = scheduler_current();
    TEST("current process != NULL", cur != NULL, "no current process");
    TEST("current process RUNNING", cur && cur->state == PROCESS_RUNNING, "not running");
    TEST("current process ticks > 0", cur && cur->ticks > 0, "no ticks");

    uint64_t t = pit_get_ticks();
    TEST("PIT ticks > 0", t > 0, "no ticks");
}
