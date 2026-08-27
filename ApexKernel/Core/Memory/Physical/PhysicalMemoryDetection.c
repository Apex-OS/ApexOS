// ApexKernel/Core/Memory/Physical/PhysicalMemoryDetection.c
// ApexOS -- PMM bootstrap: builds the page bitmap from the E820 map

#include "Core/Memory/Physical/PhysicalMemoryDetection.h"
#include "Core/Memory/Physical/PhysicalInternal.h"
#include "Core/Memory/MemoryMap/MemoryMap.h"

// Bitmap location: right after the kernel image. __kernel_end is
// defined by Linker.ld.
extern uint64_t __kernel_end;

void pmm_init(void)
{
    mmap_entry_t *entries = mmap_get_entries();
    uint32_t count = mmap_get_count();

    // Finds the highest USABLE address only (type == Available).
    uint64_t max_addr = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        if (entries[i].type != MMAP_AVAILABLE)
            continue;
        uint64_t end = entries[i].base + entries[i].length;
        if (end > max_addr)
            max_addr = end;
    }

    pmm_total = ADDR_TO_PAGE(max_addr);
    pmm_bitmap_size = (pmm_total + 7) / 8;

    // Places the bitmap right after the kernel image.
    pmm_bitmap = (uint8_t *)((uint64_t)&__kernel_end);

    // Marks everything used by default; available E820 ranges are
    // freed below.
    for (uint64_t i = 0; i < pmm_bitmap_size; i++)
        pmm_bitmap[i] = 0xFF;

    pmm_used = pmm_total;

    for (uint32_t i = 0; i < count; i++)
    {
        if (entries[i].type != MMAP_AVAILABLE)
            continue;

        uint64_t base = ADDR_TO_PAGE(entries[i].base);
        uint64_t pages = entries[i].length / PAGE_SIZE;

        for (uint64_t p = base; p < base + pages; p++)
        {
            if (p < pmm_total && bitmap_test(p))
            {
                bitmap_clear(p);
                pmm_used--;
            }
        }
    }

    // Re-reserves the low 1MB (BIOS/boot data, real-mode IVT, etc.).
    uint64_t low_pages = ADDR_TO_PAGE(0x100000);
    for (uint64_t p = 0; p < low_pages && p < pmm_total; p++)
    {
        if (!bitmap_test(p))
        {
            bitmap_set(p);
            pmm_used++;
        }
    }

    // Re-reserves the kernel image plus the bitmap that follows it.
    uint64_t kernel_start_page = ADDR_TO_PAGE(0x100000);
    uint64_t bitmap_end = (uint64_t)&__kernel_end + pmm_bitmap_size;
    uint64_t kernel_end_page = ADDR_TO_PAGE(
        (bitmap_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));

    for (uint64_t p = kernel_start_page; p < kernel_end_page && p < pmm_total; p++)
    {
        if (!bitmap_test(p))
        {
            bitmap_set(p);
            pmm_used++;
        }
    }

    // Re-reserves the kernel's initial stack -- stage2.asm sets
    // rsp=0x1F0000 (see ApexKernel/Boot/BIOS/Stage2.asm: lm64_entry)
    // right before jumping into kmain(), and the stack lives BELOW
    // that address, growing down. Nothing before this point ever
    // marked this range as used: it isn't the low-1MB BIOS region
    // (it's at ~2MB) and it isn't part of the kernel image either
    // (which ends well before 2MB in practice, but that boundary
    // depends on kernel size, not on where stage2 happened to put the
    // stack). Without this, pmm_alloc_page() could -- and, confirmed
    // via GDB plus QEMU's "-d int" exception trace, DID -- hand out a
    // physical page that's actually backing the live stack to
    // something else entirely (heap_init(), in the case that
    // motivated this fix), creating two completely different owners
    // for the same physical memory: whichever one wrote to it last
    // would silently corrupt the other, eventually crashing in a way
    // that looked -- through several increasingly deep debugging
    // sessions -- like a totally unrelated framebuffer/VBE MMIO
    // mapping failure.
    //
    // 1MB of headroom below the stack pointer is a deliberately
    // generous margin for a kernel this size -- cheap to reserve, and
    // it directly avoids re-discovering this exact bug the next time
    // the stack's actual usage grows deeper than whoever last sized
    // this margin expected.
    uint64_t stack_reserve_start = ADDR_TO_PAGE(0x100000);
    uint64_t stack_reserve_end = ADDR_TO_PAGE(0x200000);
    for (uint64_t p = stack_reserve_start; p < stack_reserve_end && p < pmm_total; p++)
    {
        if (!bitmap_test(p))
        {
            bitmap_set(p);
            pmm_used++;
        }
    }
}
