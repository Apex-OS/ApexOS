// apex-kernel/mm/heap.h
// ApexOS -- Kernel heap (kmalloc / kfree)

#ifndef APEX_HEAP_H
#define APEX_HEAP_H

#include "../include/apex/types.h"

// HEAP_START used to be 0x800000 (8MB). Moved to 64MB: the heap's
// virtual address range was colliding with the kernel's low-memory
// identity map (vmm_init() maps 0x400000 upward, virt==phys).
// vmm_get_or_create() allocates fresh PDPT/PD/PT pages via
// pmm_alloc_page() and zero-initializes them by writing straight to
// the returned physical address, assuming it's identity-mapped
// (virt==phys) -- true right after vmm_init() runs, but heap_init()
// runs next and remaps some of those same virtual addresses (the
// heap's own range, e.g. 0x800000+) to unrelated physical pages for
// its own data. If fb_init() later needs a new page table and
// pmm_alloc_page() happens to hand back a physical page that falls
// inside the heap's own virtual range, vmm_get_or_create() writes the
// new table through that now-repurposed virtual address -- landing
// in the middle of live heap data instead of the table it thought it
// was initializing. Confirmed via GDB + QEMU's gva2gpa: at
// HEAP_MAX_SIZE=6MB this happened for the VRAM framebuffer's page
// table specifically, silently corrupting it and producing a page
// fault the first time fb_present() tried to use the mapping -- see
// git history/session notes. Whether it triggers at all depends on
// exact allocation order/timing, which is why it only showed up past
// a specific heap size instead of consistently.
//
// Moving the heap well above the low region vmm_get_or_create() tends
// to draw fresh page-table pages from removes the overlap entirely:
// the heap's virtual range and the physical pages used as page tables
// no longer share the same address space.
#define HEAP_START 0x4000000   // 64MB
#define HEAP_MAX_SIZE 0x600000 // 6MB

typedef struct heap_block
{
    size_t size; // Payload size (excludes this header)
    bool free;
    struct heap_block *next;
    struct heap_block *prev;
    uint32_t magic; // Corruption check
} heap_block_t;

#define HEAP_MAGIC 0xDEADB00F

void heap_init(void);
void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t size);
void kfree(void *ptr);
void heap_print_stats(void);

#endif // APEX_HEAP_H