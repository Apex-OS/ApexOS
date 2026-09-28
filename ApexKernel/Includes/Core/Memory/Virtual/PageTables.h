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
// Same as above, but with the page marked non-executable (requires
// EFER.NXE, set by Core/Syscalls/ABI/SyscallSetup.c's syscall_init()
// -- without it the CPU ignores VMM_NO_EXEC entirely and every page
// stays executable regardless). Use these for any page that holds
// data a Ring 3 process (or a syscall handler acting on its behalf)
// can write into but should never be able to jump to: user stacks,
// heap-backed buffers, and any buffer a syscall like SYS_READ writes
// into (Core/Syscalls/Validation/UserPointer.h's is_valid_user_range()
// callers). Still not a full W^X policy on its own -- code pages
// (USER_CODE_ADDR, USER_LIB_ADDR, AXD_BASE_ADDR) are mapped
// executable-and-writable today so this kernel's own file loaders can
// write program bytes into them before ever running them -- but it
// closes the specific "write-anything, execute-anything" path a
// corrupted or attacker-controlled data pointer used to have.
#define VMM_KERNEL_RW_NX (VMM_PRESENT | VMM_WRITABLE | VMM_NO_EXEC)
#define VMM_USER_RW_NX (VMM_PRESENT | VMM_WRITABLE | VMM_USER | VMM_NO_EXEC)

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

// Identical to vmm_map() above, but additionally broadcasts a TLB
// shootdown (Core/SMP/TLB/TlbShootdown.h) to every other online CPU
// afterward -- required whenever a mapping is created/changed AFTER
// APs may already be running real code that could hold a stale TLB
// entry for the same virtual address (or for a page table slot whose
// shape just changed, e.g. a fresh PD/PT page vmm_get_or_create()
// allocated for this new mapping). vmm_map() itself only ever
// invalidates the CALLING CPU's own TLB (a single `invlpg`) -- see
// this function's own discovery story in TlbShootdown.h's header
// comment for why that silently corrupted an unrelated AP's execution
// the first time this kernel ever called vmm_map() again after APs
// were already active (Core/SMP/IOAPIC/IoApic.h's ioapic_init()).
//
// Deliberately a SEPARATE function rather than folding the shootdown
// into vmm_map() itself: the overwhelming majority of this kernel's
// vmm_map() calls happen early in boot, before any AP exists (the
// entire identity map, Core/SMP/APIC/Lapic.h's lapic_init(), and
// dozens of other one-time driver MMIO mappings) -- broadcasting a
// shootdown IPI and spinning for acks on every one of those would add
// real overhead for zero benefit (there's no other CPU yet whose TLB
// could possibly be stale). Callers that run after APs are active use
// this instead; everything else keeps calling plain vmm_map().
void vmm_map_and_shootdown(uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_unmap(uint64_t virt);
uint64_t vmm_get_phys(uint64_t virt);

#endif // APEX_CORE_MEMORY_VIRTUAL_PAGE_TABLES_H
