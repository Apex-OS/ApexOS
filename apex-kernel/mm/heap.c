// apex-kernel/mm/heap.c
// ApexOS -- Kernel heap (kmalloc / kfree)

#include "heap.h"
#include "vmm.h"
#include "pmm.h"
#include "../drivers/vga.h"
#include "../core/klog.h"

static heap_block_t *heap_head = NULL;
static size_t heap_size = 0;

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

void *kmalloc(size_t size)
{
    if (!size)
        return NULL;

    size = (size + 7) & ~7; // 8-byte alignment

    heap_block_t *block = heap_head;

    while (block)
    {
        if (block->magic != HEAP_MAGIC)
            return NULL; // Corruption detected.

        if (block->free && block->size >= size)
        {
            // Splits the block if there's enough room left over to be
            // useful as a separate free block.
            if (block->size >= size + sizeof(heap_block_t) + 8)
            {
                heap_block_t *new_block = (heap_block_t *)((uint8_t *)block + sizeof(heap_block_t) + size);
                new_block->size = block->size - size - sizeof(heap_block_t);
                new_block->free = true;
                new_block->next = block->next;
                new_block->prev = block;
                new_block->magic = HEAP_MAGIC;

                if (block->next)
                    block->next->prev = new_block;

                block->next = new_block;
                block->size = size;
            }

            block->free = false;
            return (void *)((uint8_t *)block + sizeof(heap_block_t));
        }

        block = block->next;
    }

    return NULL; // Heap exhausted.
}

void *kcalloc(size_t count, size_t size)
{
    size_t total = count * size;
    void *ptr = kmalloc(total);
    if (!ptr)
        return NULL;

    uint8_t *p = (uint8_t *)ptr;
    for (size_t i = 0; i < total; i++)
        p[i] = 0;

    return ptr;
}

void kfree(void *ptr)
{
    if (!ptr)
        return;

    heap_block_t *block = (heap_block_t *)((uint8_t *)ptr - sizeof(heap_block_t));

    if (block->magic != HEAP_MAGIC)
        return;

    block->free = true;

    // Coalesces with the next block if it's also free.
    if (block->next && block->next->free)
    {
        block->size += sizeof(heap_block_t) + block->next->size;
        block->next = block->next->next;
        if (block->next)
            block->next->prev = block;
    }

    // Coalesces with the previous block if it's also free.
    if (block->prev && block->prev->free)
    {
        block->prev->size += sizeof(heap_block_t) + block->size;
        block->prev->next = block->next;
        if (block->next)
            block->next->prev = block->prev;
    }
}

void heap_print_stats(void)
{
    size_t free_bytes = 0;
    size_t used_bytes = 0;
    size_t blocks = 0;

    heap_block_t *block = heap_head;
    while (block)
    {
        if (block->free)
            free_bytes += block->size;
        else
            used_bytes += block->size;
        blocks++;
        block = block->next;
    }
    klog_ok_prefix();
    vga_puts("Heap initialized - Size: ");
    vga_putdec(heap_size / 1024);
    vga_puts("KB  Free: ");
    vga_putdec(free_bytes / 1024);
    vga_puts("KB  Used: ");
    vga_putdec(used_bytes / 1024);
    vga_puts("KB  Blocks: ");
    vga_putdec(blocks);
    vga_puts("\n");
}