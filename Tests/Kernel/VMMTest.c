// ApexKernel Tests/Kernel/VMMTest.c
// ApexOS -- VMM self-test

#include "VMMTest.h"
#include "TestRunner.h"
#include "Drivers/Display/VGA.h"
#include "Core/Memory/Physical/PageBitmap.h"
#include "Core/Memory/Virtual/PageTables.h"

void test_vmm(void)
{
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("VMM:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    uint64_t phys = vmm_get_phys(0x100000);
    TEST("vmm_get_phys(0x100000) = 0x100000", phys == 0x100000, "wrong translation");

    void *phys_page = pmm_alloc_page();
    TEST("alloc phys page for VMM test", phys_page != NULL, "alloc failed");

    vmm_map(0x500000, (uint64_t)phys_page, VMM_KERNEL_RW);
    uint64_t translated = vmm_get_phys(0x500000);
    TEST("vmm_map + vmm_get_phys", translated == (uint64_t)phys_page, "map failed");

    uint64_t *test_ptr = (uint64_t *)0x500000;
    *test_ptr = 0xDEADBEEFCAFEBABE;
    TEST("write/read mapped page", *test_ptr == 0xDEADBEEFCAFEBABE, "data mismatch");

    vmm_unmap(0x500000);
    pmm_free_page(phys_page);
    TEST("vmm_unmap done", true, "");
}
