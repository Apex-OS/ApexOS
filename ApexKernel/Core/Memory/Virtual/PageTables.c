// ApexKernel/Core/Memory/Virtual/PageTables.c
// ApexOS -- Virtual Memory Manager

#include "Core/Memory/Virtual/PageTables.h"
#include "Core/Memory/Physical/PageBitmap.h"
#include "Core/SMP/TLB/TlbShootdown.h"

// Kernel PML4, set up by stage2 at boot (see
// ApexKernel/Boot/BIOS/Stage2.asm). During vmm_init()'s initial
// identity-map build, this is redirected to SCRATCH_PML4 instead --
// see building_scratch_pml4's declaration in PageTables.h and
// vmm_init()'s comment in IdentityMap.c for why.
static uint64_t *pml4 = (uint64_t *)KERNEL_PML4;
bool building_scratch_pml4 = false;

// Returns whichever PML4 is currently the target for new mappings --
// KERNEL_PML4 normally, or SCRATCH_PML4 while vmm_init() is building
// the initial map into it (see building_scratch_pml4's declaration).
static inline uint64_t *active_pml4(void)
{
    return building_scratch_pml4 ? (uint64_t *)SCRATCH_PML4 : pml4;
}

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

    uint64_t *pdpt = vmm_get_or_create(active_pml4(), PML4_IDX(virt), table_flags);
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

void vmm_map_and_shootdown(uint64_t virt, uint64_t phys, uint64_t flags)
{
    vmm_map(virt, phys, flags);

    // See PageTables.h's own comment on vmm_map_and_shootdown() for
    // why this broadcast is needed here specifically -- vmm_map()
    // above already invalidated THIS CPU's own TLB, this covers
    // every other one.
    tlb_shootdown(virt);
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
