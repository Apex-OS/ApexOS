// ApexKernel/Core/Memory/Heap/Heap.c
// ApexOS -- Kernel heap allocator (kmalloc / kfree)

#include "Core/Memory/Heap/Heap.h"
#include "Core/Memory/Heap/HeapInternal.h"

heap_block_t *heap_head = NULL;
size_t heap_size = 0;

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
