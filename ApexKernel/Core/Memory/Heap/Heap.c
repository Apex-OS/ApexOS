// ApexKernel/Core/Memory/Heap/Heap.c
// ApexOS -- Kernel heap allocator (kmalloc / kfree)

#include "Core/Memory/Heap/Heap.h"
#include "Core/Memory/Heap/HeapInternal.h"
#include "Core/Synchronization/Spinlock/Spinlock.h"

heap_block_t *heap_head = NULL;
size_t heap_size = 0;

// Protects heap_head's linked list (block->next/prev/size/free) across
// every kmalloc()/kcalloc()/kfree() call. Without this, two CPUs
// calling into the allocator at the same time (this kernel's SMP is
// active from boot -- Core/SMP/Startup/AppProcessorStartup.c's
// ap_startup_wake_one() itself calls kmalloc() for each AP's stack)
// could interleave their reads/writes of the same block's fields --
// e.g. both see the same free block as available and both split/claim
// it, corrupting the list for everyone after. Uses the IRQ-safe
// variant (see Spinlock.h's own comment on why proc_list_lock needs
// it): nothing in this kernel currently calls kmalloc()/kfree() from
// inside an interrupt handler, but the cost of the IRQ-safe variant is
// negligible and it removes the risk entirely if that ever changes,
// rather than relying on it staying true forever.
static spinlock_t heap_lock = SPINLOCK_INIT;

void *kmalloc(size_t size)
{
    if (!size)
        return NULL;

    size = (size + 7) & ~7; // 8-byte alignment

    uint64_t flags;
    spinlock_acquire_irqsave(&heap_lock, &flags);

    heap_block_t *block = heap_head;

    while (block)
    {
        if (block->magic != HEAP_MAGIC)
        {
            spinlock_release_irqrestore(&heap_lock, flags);
            return NULL; // Corruption detected.
        }

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
            spinlock_release_irqrestore(&heap_lock, flags);
            return (void *)((uint8_t *)block + sizeof(heap_block_t));
        }

        block = block->next;
    }

    spinlock_release_irqrestore(&heap_lock, flags);
    return NULL; // Heap exhausted.
}

void *kcalloc(size_t count, size_t size)
{
    // count/size can both come from anywhere that eventually calls
    // kcalloc() with an unchecked value -- a large enough count for a
    // given size overflows count * size back to a small value, so
    // kmalloc() would allocate far less than the caller thinks it got,
    // and the very first write into what the caller believes is a
    // count-by-size buffer becomes a heap overflow. Checked as a
    // division instead of comparing the multiplication's own
    // (possibly already-wrapped) result.
    if (size != 0 && count > (size_t)-1 / size)
        return NULL;

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

    uint64_t flags;
    spinlock_acquire_irqsave(&heap_lock, &flags);

    if (block->magic != HEAP_MAGIC)
    {
        spinlock_release_irqrestore(&heap_lock, flags);
        return;
    }

    // Double-free guard: without this, a second kfree() on the same
    // pointer would coalesce this block with its neighbors a second
    // time, corrupting next/prev/size across the list (double-counted
    // sizes, pointers left dangling into memory that may since have
    // been reallocated to something else entirely) -- a classic
    // allocator double-free, one of the more reliable primitives for
    // turning a use-after-free bug elsewhere into arbitrary memory
    // corruption.
    if (block->free)
    {
        spinlock_release_irqrestore(&heap_lock, flags);
        return;
    }

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

    spinlock_release_irqrestore(&heap_lock, flags);
}
