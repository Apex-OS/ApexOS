// ApexKernel/Core/Memory/Virtual/IdentityMap.c
// ApexOS -- VMM bootstrap: identity-maps all physical memory

#include "Core/Memory/Virtual/IdentityMap.h"
#include "Core/Memory/Virtual/PageTables.h"
#include "Core/Memory/Physical/PageBitmap.h"

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
    // ApexKernel/Drivers/Display/Framebuffer/FramebufferInit.c).
    // Confirmed by bisecting HEAP_MAX_SIZE: whether the VRAM mapping
    // survives depends on exactly how much heap the kernel allocates
    // before fb_init() runs, because that shifts where
    // pmm_alloc_page() happens to place fb_init()'s own page tables
    // -- sometimes landing them inside memory the heap (or something
    // else) claims moments later. A table page that isn't
    // identity-mapped can silently get handed out again as plain
    // data, corrupting whatever mapping depended on it.
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
