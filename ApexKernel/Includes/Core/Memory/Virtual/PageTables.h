// ApexKernel/Includes/Core/Memory/Virtual/PageTables.h
// ApexOS -- Virtual Memory Manager (4-level paging, x86-64)

#ifndef APEX_CORE_MEMORY_VIRTUAL_PAGE_TABLES_H
#define APEX_CORE_MEMORY_VIRTUAL_PAGE_TABLES_H

#include "Core/Types.h"

// Page table entry flags.
#define VMM_PRESENT (1ULL << 0)
#define VMM_WRITABLE (1ULL << 1)
#define VMM_USER (1ULL << 2)
#define VMM_WRITETHRU (1ULL << 3)
#define VMM_NO_CACHE (1ULL << 4)
#define VMM_ACCESSED (1ULL << 5)
#define VMM_DIRTY (1ULL << 6)
#define VMM_HUGE (1ULL << 7)
#define VMM_NO_EXEC (1ULL << 63)

// Common flag combinations.
#define VMM_KERNEL_RW (VMM_PRESENT | VMM_WRITABLE)
#define VMM_USER_RW (VMM_PRESENT | VMM_WRITABLE | VMM_USER)

// Physical address of the kernel PML4 (set up by stage2 at boot).
#define KERNEL_PML4 0x1000

// Scratch address used transiently by vmm_init() (IdentityMap.c) to
// build the initial identity map before it's known safe to write into
// KERNEL_PML4 directly -- see that function's header comment. Unused
// by both boot paths and within the low-1MB range
// PhysicalMemoryDetection.c's pmm_init() already reserves
// unconditionally, so nothing else can ever claim it either.
#define SCRATCH_PML4 0x7000

// When true, vmm_map()/vmm_get_or_create() (PageTables.c) build into
// SCRATCH_PML4 instead of KERNEL_PML4. Set only by vmm_init() while
// it builds the initial identity map; false the rest of the kernel's
// life, when KERNEL_PML4 is the live table again and every other
// vmm_map() call (heap growth, Ring3 mappings, AHCI DMA buffers, ...)
// must target it as normal.
extern bool building_scratch_pml4;

// Extracts each page-table index from a virtual address.
#define PML4_IDX(a) (((a) >> 39) & 0x1FF)
#define PDPT_IDX(a) (((a) >> 30) & 0x1FF)
#define PD_IDX(a) (((a) >> 21) & 0x1FF)
#define PT_IDX(a) (((a) >> 12) & 0x1FF)

// Masks the physical address out of a page table entry.
#define PAGE_ADDR(e) ((e) & 0x000FFFFFFFFFF000ULL)

void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_unmap(uint64_t virt);
uint64_t vmm_get_phys(uint64_t virt);

#endif // APEX_CORE_MEMORY_VIRTUAL_PAGE_TABLES_H
