// ApexKernel/Core/Memory/Virtual/IdentityMap.c
// ApexOS -- VMM bootstrap: identity-maps all physical memory

#include "Core/Memory/Virtual/IdentityMap.h"
#include "Core/Memory/Virtual/PageTables.h"
#include "Core/Memory/Physical/PageBitmap.h"

static inline void write_cr3(uint64_t val)
{
    __asm__ volatile("mov %0, %%cr3" ::"r"(val) : "memory");
}

void vmm_init(void)
{
    // Every other function in this file (vmm_map/vmm_unmap/
    // vmm_get_phys, see PageTables.c) assumes the active PML4 --
    // whatever CR3 actually points at -- lives at the fixed physical
    // address KERNEL_PML4 (0x1000). True on a BIOS boot: Stage2.asm
    // builds its page tables there itself and loads CR3 with that
    // exact address before ever jumping to the kernel. Never true on
    // a UEFI boot: Boot/UEFI/Main.c intentionally does no page table
    // setup of its own (see that file's header comment, step 6) and
    // leaves CR3 pointing at whatever page table structure the UEFI
    // firmware itself was using -- some real physical address, but
    // essentially never 0x1000 in practice.
    //
    // A first attempt at fixing this copied the firmware's existing
    // PML4 entries into KERNEL_PML4 and pointed CR3 there -- wrong:
    // those entries still point at the firmware's own PDPT/PD/PT
    // sub-tables, wherever in physical memory the firmware happened
    // to put them (confirmed via -d int: a page fault at CR2 just
    // past the original CR3's own address, i.e. inside one of those
    // inherited sub-tables). Nothing stops pmm_alloc_page() from
    // handing out that same physical memory for something else the
    // moment this kernel starts allocating -- the firmware's memory
    // map says it's ordinary conventional memory once
    // ExitBootServices() has run, not a reserved page table.
    // Whatever legitimately used it next silently corrupts a
    // still-referenced page table out from under the CPU.
    //
    // A second attempt zero-initialized KERNEL_PML4 (0x1000) directly,
    // in place, before building the map -- wrong in the opposite
    // direction: on a BIOS boot CR3 already IS 0x1000 by this point
    // (Stage2.asm loaded it before jumping here), so zeroing it live
    // wipes the very table the CPU is walking to fetch the next
    // instruction, including the entry that maps this code's own
    // current address -- an immediate fault with no handler installed
    // yet this early, seen as the kernel hanging/going dark right
    // around vmm_init() on a BIOS boot specifically (UEFI was fine,
    // since there CR3 was never 0x1000 to begin with -- this was a
    // regression introduced while fixing the UEFI case, not caught
    // because it was only tested on UEFI).
    //
    // Fixed properly: the new table is built from scratch at a
    // temporary scratch address (SCRATCH_PML4, 0x7000 -- unused by
    // either boot path, within the low-1MB range
    // PhysicalMemoryDetection.c's pmm_init() already reserves
    // unconditionally) via vmm_map() below, which is taught to build
    // into that scratch address rather than KERNEL_PML4 while this
    // flag is set. Once the full map is complete there, it's copied
    // into KERNEL_PML4 (0x1000) in one shot -- overwriting whatever
    // was there (BIOS's now-superseded table, or nothing in
    // particular on UEFI) is safe at this point because it happens
    // through the *old*, still-active mapping, not the new one, and
    // the copy itself doesn't change what code is currently
    // executing -- and only then is CR3 finally pointed at 0x1000.
    // This works identically on both boot paths: neither one's
    // currently-active table is ever touched until the switch itself,
    // and vmm_map() only ever writes into memory nothing is walking
    // yet.
    building_scratch_pml4 = true;
    uint64_t *scratch_pml4 = (uint64_t *)SCRATCH_PML4;
    for (int i = 0; i < 512; i++)
        scratch_pml4[i] = 0;

    // Identity-maps ALL physical memory the PMM knows about
    // (pmm_get_total_bytes(), valid here -- pmm_init() runs before
    // vmm_init() in kmain()), starting from address 0. Every physical
    // page pmm_alloc_page() can ever hand out -- heap, framebuffer,
    // page tables, stack pages -- needs to already be identity-mapped
    // by the time anything uses it, regardless of allocation order or
    // which boot path got here.
    uint64_t identity_end = pmm_get_total_bytes();
    for (uint64_t addr = 0x0; addr < identity_end; addr += PAGE_SIZE)
        vmm_map(addr, addr, VMM_KERNEL_RW);
    building_scratch_pml4 = false;

    // Copy the finished scratch table into KERNEL_PML4. This runs
    // under the boot path's original CR3 (BIOS: Stage2.asm's table at
    // 0x1000 -- about to be overwritten here, which is fine since
    // nothing walks it again after the switch below; UEFI: the
    // firmware's own table, untouched by any of this), so nothing
    // executing right now becomes unmapped by this write.
    uint64_t *fixed_pml4 = (uint64_t *)KERNEL_PML4;
    for (int i = 0; i < 512; i++)
        fixed_pml4[i] = scratch_pml4[i];

    // Only now, with a complete identity map already built and copied
    // into it, is it safe to make KERNEL_PML4 the table the CPU
    // actually walks -- the code currently executing (identity-mapped,
    // being physical low memory) and everything else touched between
    // here and kmain() returning stays reachable.
    write_cr3(KERNEL_PML4);
}
