// ApexKernel/Core/Memory/Heap/HeapSetup.c
// ApexOS -- Heap bootstrap (one-time construction at boot)

#include "Core/Memory/Heap/HeapSetup.h"
#include "Core/Memory/Heap/HeapInternal.h"
#include "Core/Memory/Virtual/PageTables.h"
#include "Core/Memory/Physical/PageBitmap.h"

void heap_init(void)
{
    for (uint64_t addr = HEAP_START;
         addr < HEAP_START + HEAP_MAX_SIZE;
         addr += PAGE_SIZE)
    {
        void *phys = pmm_alloc_page();
        if (!phys)
            break;
        vmm_map(addr, (uint64_t)phys, VMM_KERNEL_RW);
        heap_size += PAGE_SIZE;
    }

    // The whole heap starts out as one large free block.
    heap_head = (heap_block_t *)HEAP_START;
    heap_head->size = heap_size - sizeof(heap_block_t);
    heap_head->free = true;
    heap_head->next = NULL;
    heap_head->prev = NULL;
    heap_head->magic = HEAP_MAGIC;
}
