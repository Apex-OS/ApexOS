// ApexKernel/Core/Memory/Physical/PageBitmap.c
// ApexOS -- Physical Memory Manager (bitmap allocator)

#include "Core/Memory/Physical/PageBitmap.h"
#include "Core/Memory/Physical/PhysicalInternal.h"

uint8_t *pmm_bitmap = NULL;
uint64_t pmm_total = 0;       // Total page count
uint64_t pmm_used = 0;        // Pages currently in use
uint64_t pmm_bitmap_size = 0; // Bitmap size in bytes

void *pmm_alloc_page(void)
{
    for (uint64_t p = 0; p < pmm_total; p++)
    {
        if (!bitmap_test(p))
        {
            bitmap_set(p);
            pmm_used++;
            return (void *)PAGE_TO_ADDR(p);
        }
    }
    return NULL; // Out of physical memory.
}

void pmm_free_page(void *addr)
{
    uint64_t page = ADDR_TO_PAGE((uint64_t)addr);
    if (bitmap_test(page))
    {
        bitmap_clear(page);
        pmm_used--;
    }
}

void pmm_stats(pmm_stats_t *stats)
{
    stats->total_pages = pmm_total;
    stats->used_pages = pmm_used;
    stats->free_pages = pmm_total - pmm_used;
}

uint64_t pmm_get_total_bytes(void)
{
    return pmm_total * PAGE_SIZE;
}
