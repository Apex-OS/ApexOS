// ApexKernel/Includes/Core/Memory/Physical/PageBitmap.h
// ApexOS -- Physical Memory Manager (bitmap allocator)

#ifndef APEX_CORE_MEMORY_PHYSICAL_PAGE_BITMAP_H
#define APEX_CORE_MEMORY_PHYSICAL_PAGE_BITMAP_H

#include "Core/Types.h"

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

#define ADDR_TO_PAGE(a) ((a) >> PAGE_SHIFT)
#define PAGE_TO_ADDR(p) ((p) << PAGE_SHIFT)

typedef struct
{
    uint64_t total_pages;
    uint64_t used_pages;
    uint64_t free_pages;
} pmm_stats_t;

void *pmm_alloc_page(void);
void pmm_free_page(void *addr);
void pmm_stats(pmm_stats_t *stats);

// Highest physical address (exclusive) the PMM knows about. Used by
// vmm_init() to size the identity map so it covers all physical
// memory pmm_alloc_page() can ever hand out, not just an arbitrary
// fixed cutoff -- a cutoff smaller than this lets pmm_alloc_page()
// hand out pages that were never mapped anywhere, including pages
// vmm_get_or_create() itself allocates for new PDPT/PD/PT tables.
uint64_t pmm_get_total_bytes(void);

#endif // APEX_CORE_MEMORY_PHYSICAL_PAGE_BITMAP_H
