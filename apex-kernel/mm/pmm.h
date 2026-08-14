// apex-kernel/mm/pmm.h
// ApexOS -- Physical Memory Manager (bitmap allocator)

#ifndef APEX_PMM_H
#define APEX_PMM_H

#include "../include/apex/types.h"
#include "../include/apex/mmap.h"

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

#define ADDR_TO_PAGE(a) ((a) >> PAGE_SHIFT)
#define PAGE_TO_ADDR(p) ((p) << PAGE_SHIFT)

// Bitmap location: right after the kernel image. __kernel_end is
// defined by linker.ld.
extern uint64_t __kernel_end;

typedef struct
{
    uint64_t total_pages;
    uint64_t used_pages;
    uint64_t free_pages;
} pmm_stats_t;

void pmm_init(void);
void *pmm_alloc_page(void);
void pmm_free_page(void *addr);
void pmm_stats(pmm_stats_t *stats);
void pmm_print_stats(void);

// Highest physical address (exclusive) the PMM knows about. Used by
// vmm_init() to size the identity map so it covers all physical
// memory pmm_alloc_page() can ever hand out, not just an arbitrary
// fixed cutoff -- a cutoff smaller than this lets pmm_alloc_page()
// hand out pages that were never mapped anywhere, including pages
// vmm_get_or_create() itself allocates for new PDPT/PD/PT tables.
uint64_t pmm_get_total_bytes(void);

#endif // APEX_PMM_H