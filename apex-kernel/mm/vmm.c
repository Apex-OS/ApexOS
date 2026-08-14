// apex-kernel/mm/vmm.c
// ApexOS -- Virtual Memory Manager

#include "vmm.h"
#include "pmm.h"
#include "../drivers/vga.h"
#include "../core/klog.h"

// Kernel PML4, set up by stage2 at boot (see arch/x86_64/boot/stage2.asm).
static uint64_t *pml4 = (uint64_t *)KERNEL_PML4;

// Returns the child table at `table[idx]`, allocating and
// zero-initializing it first if not already present.
static uint64_t *vmm_get_or_create(uint64_t *table, uint64_t idx,
                                   uint64_t flags)
{
    if (!(table[idx] & VMM_PRESENT))
    {
        void *page = pmm_alloc_page();
        if (!page)
            return NULL;

        uint64_t *t = (uint64_t *)page;
        for (int i = 0; i < 512; i++)
            t[i] = 0;

        table[idx] = (uint64_t)page | flags;
    }
    else
    {
        // Merges in any new flags (e.g. VMM_USER) on an existing entry.
        table[idx] |= flags;
    }
    return (uint64_t *)PAGE_ADDR(table[idx]);
}

void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags)
{
    // Intermediate tables need Present + Writable (+ User, if the leaf
    // mapping is user-accessible) regardless of the leaf page's own
    // flags, or the CPU will fault walking the page tables.
    uint64_t table_flags = VMM_PRESENT | VMM_WRITABLE;
    if (flags & VMM_USER)
        table_flags |= VMM_USER;

    uint64_t *pdpt = vmm_get_or_create(pml4, PML4_IDX(virt), table_flags);
    if (!pdpt)
        return;

    uint64_t *pd = vmm_get_or_create(pdpt, PDPT_IDX(virt), table_flags);
    if (!pd)
        return;

    uint64_t *pt = vmm_get_or_create(pd, PD_IDX(virt), table_flags);
    if (!pt)
        return;

    pt[PT_IDX(virt)] = (phys & 0x000FFFFFFFFFF000ULL) | flags;

    __asm__ volatile("invlpg (%0)" ::"r"(virt) : "memory");
}

void vmm_unmap(uint64_t virt)
{
    uint64_t *pdpt = (uint64_t *)PAGE_ADDR(pml4[PML4_IDX(virt)]);
    if (!pdpt)
        return;

    uint64_t *pd = (uint64_t *)PAGE_ADDR(pdpt[PDPT_IDX(virt)]);
    if (!pd)
        return;

    uint64_t *pt = (uint64_t *)PAGE_ADDR(pd[PD_IDX(virt)]);
    if (!pt)
        return;

    pt[PT_IDX(virt)] = 0;
    __asm__ volatile("invlpg (%0)" ::"r"(virt) : "memory");
}

uint64_t vmm_get_phys(uint64_t virt)
{
    if (!(pml4[PML4_IDX(virt)] & VMM_PRESENT))
        return 0;
    uint64_t *pdpt = (uint64_t *)PAGE_ADDR(pml4[PML4_IDX(virt)]);

    if (!(pdpt[PDPT_IDX(virt)] & VMM_PRESENT))
        return 0;
    uint64_t *pd = (uint64_t *)PAGE_ADDR(pdpt[PDPT_IDX(virt)]);

    if (!(pd[PD_IDX(virt)] & VMM_PRESENT))
        return 0;
    uint64_t *pt = (uint64_t *)PAGE_ADDR(pd[PD_IDX(virt)]);

    if (!(pt[PT_IDX(virt)] & VMM_PRESENT))
        return 0;
    return PAGE_ADDR(pt[PT_IDX(virt)]);
}

void vmm_init(void)
{
    // The PML4 already exists (set up by stage2); the 0-4MB range is
    // already identity-mapped there too. This extends the identity
    // map up through ALL physical memory the PMM knows about
    // (pmm_get_total_bytes(), valid here -- pmm_init() runs before
    // vmm_init() in kmain()), not just a fixed cutoff.
    //
    // This used to stop at a fixed 64MB. That missed the real
    // requirement: pmm_alloc_page() can hand out ANY physical page up
    // to pmm_total, including ones vmm_get_or_create() allocates for
    // brand new PDPT/PD/PT tables while mapping addresses above the
    // cutoff -- e.g. the VRAM framebuffer around 0xfd000000 (see
    // drivers/framebuffer.c). Confirmed by bisecting HEAP_MAX_SIZE:
    // whether the VRAM mapping survives depends on exactly how much
    // heap the kernel allocates before fb_init() runs, because that
    // shifts where pmm_alloc_page() happens to place fb_init()'s own
    // page tables -- sometimes landing them inside memory the heap
    // (or something else) claims moments later. A table page that
    // isn't identity-mapped can silently get handed out again as
    // plain data, corrupting whatever mapping depended on it.
    //
    // Mapping all of physical memory up front removes that ordering
    // dependency entirely -- every page pmm_alloc_page() can ever
    // return is guaranteed to already be identity-mapped by the time
    // anything tries to use it, whether as heap/framebuffer data or
    // as a fresh page table, regardless of allocation order or size.
    uint64_t identity_end = pmm_get_total_bytes();
    for (uint64_t addr = 0x400000; addr < identity_end; addr += PAGE_SIZE)
        vmm_map(addr, addr, VMM_KERNEL_RW);
}

void vmm_print_stats(void)
{
    klog_ok_prefix();
    vga_puts("VMM initialized - Identity map 0x400000 to end of RAM\n");
}