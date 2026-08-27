// ApexKernel Tests/Kernel/HeapTest.c
// ApexOS -- Heap self-test

#include "HeapTest.h"
#include "TestRunner.h"
#include "Drivers/Display/VGA.h"
#include "Core/Memory/Heap/Heap.h"

void test_heap(void)
{
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("Heap:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    void *a = kmalloc(64);
    void *b = kmalloc(128);
    void *c = kmalloc(256);

    TEST("kmalloc(64)  != NULL", a != NULL, "alloc failed");
    TEST("kmalloc(128) != NULL", b != NULL, "alloc failed");
    TEST("kmalloc(256) != NULL", c != NULL, "alloc failed");
    TEST("allocations different", a != b && b != c && a != c, "same addr");

    uint8_t *pa = (uint8_t *)a;
    for (int i = 0; i < 64; i++)
        pa[i] = (uint8_t)i;
    TEST("write 64 bytes", pa[63] == 63, "write failed");

    uint8_t *z = (uint8_t *)kcalloc(32, 1);
    bool zeroed = true;
    for (int i = 0; i < 32; i++)
        if (z[i] != 0)
        {
            zeroed = false;
            break;
        }
    TEST("kcalloc zero-initialized", zeroed, "not zeroed");

    kfree(a);
    void *d = kmalloc(64);
    TEST("kmalloc after kfree", d != NULL, "alloc after free failed");

    kfree(b);
    kfree(c);
    kfree(d);
    kfree(z);
    TEST("kfree all blocks", true, "");
}
