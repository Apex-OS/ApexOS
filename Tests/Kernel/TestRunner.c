// ApexKernel Tests/Kernel/TestRunner.c
// ApexOS -- Kernel self-test infrastructure and orchestrator

#include "TestRunner.h"
#include "GDTTest.h"
#include "IDTTest.h"
#include "PITTest.h"
#include "PMMTest.h"
#include "VMMTest.h"
#include "HeapTest.h"
#include "SchedulerTest.h"
#include "Drivers/Display/VGA.h"
#include "ApexLib/Logs/Log.h"

// Tests/Filesystem/'s test_axfs/test_axfs_cleanup/test_vfs -- declared
// here rather than included via a header path across directories,
// since Tests/ has no shared Includes/ (see project notes: test files
// are self-contained).
extern void test_axfs(void);
extern void test_axfs_cleanup(void);
extern void test_vfs(void);

static uint32_t tests_passed = 0;
static uint32_t tests_failed = 0;

void test_pass(const char *name)
{
    vga_puts("  ");
    klog_test_pass(name);
    tests_passed++;
}

void test_fail(const char *name, const char *reason)
{
    vga_puts("  ");
    klog_test_fail(name, reason);
    tests_failed++;
}

void ktest_run_all(void)
{
    tests_passed = 0;
    tests_failed = 0;

    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("\n=== ApexOS Kernel Self-Test ===\n\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    test_gdt();
    vga_set_color(VGA_WHITE, VGA_BLACK);
    test_idt();
    vga_set_color(VGA_WHITE, VGA_BLACK);
    test_pit();
    vga_set_color(VGA_WHITE, VGA_BLACK);
    test_pmm();
    vga_set_color(VGA_WHITE, VGA_BLACK);
    test_vmm();
    vga_set_color(VGA_WHITE, VGA_BLACK);
    test_heap();
    vga_set_color(VGA_WHITE, VGA_BLACK);
    test_scheduler();
    vga_set_color(VGA_WHITE, VGA_BLACK);
    test_axfs();
    vga_set_color(VGA_WHITE, VGA_BLACK);
    test_vfs();
    vga_set_color(VGA_WHITE, VGA_BLACK);
    test_axfs_cleanup();

    vga_puts("\n");
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("Results: ");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_putdec(tests_passed);
    vga_puts(" passed  ");
    if (tests_failed > 0)
        vga_set_color(VGA_RED, VGA_BLACK);
    vga_putdec(tests_failed);
    vga_puts(" failed\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("===============================\n");
}
