// ApexKernel Tests/Kernel/PMMTest.c
// ApexOS -- PMM self-test

#include "PMMTest.h"
#include "TestRunner.h"
#include "Drivers/Display/VGA.h"
#include "Core/Memory/Physical/PageBitmap.h"

void test_pmm(void)
{
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("PMM:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    pmm_stats_t s;
    pmm_stats(&s);

    TEST("Total pages > 0", s.total_pages > 0, "no pages");
    TEST("Free pages > 0", s.free_pages > 0, "no free pages");
    TEST("RAM > 64MB", s.total_pages * 4096 > 64 * 1024 * 1024, "less than 64MB");

    uint64_t used_before = s.used_pages;
    void *p1 = pmm_alloc_page();
    void *p2 = pmm_alloc_page();
    TEST("pmm_alloc_page != NULL", p1 != NULL && p2 != NULL, "alloc failed");
    TEST("pages different", p1 != p2, "same page allocated twice");

    pmm_stats(&s);
    TEST("used_pages increased", s.used_pages > used_before, "used not updated");

    pmm_free_page(p1);
    pmm_free_page(p2);
    pmm_stats(&s);
    TEST("used_pages decreased after free", s.used_pages == used_before, "free failed");
}
