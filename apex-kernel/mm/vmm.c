// apex-kernel/mm/vmm.c
// ApexOS -- Virtual Memory Manager

#include "vmm.h"
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
    // map up to 64MB for headroom -- 16MB previously caused page
    // faults once the kernel started touching memory past that point,
    // see mm/heap.h (HEAP_START=8MB) and core/axf/axd.c (AXD_BASE_ADDR
    // =13MB) for allocations that live in this range.
    for (uint64_t addr = 0x400000; addr < 0x4000000; addr += PAGE_SIZE)
        vmm_map(addr, addr, VMM_KERNEL_RW);
}

void vmm_print_stats(void)
{
    klog_ok_prefix();
    vga_puts("VMM initialized - Identity map 0x400000 to 0x4000000 (4MB-64MB)\n");
}