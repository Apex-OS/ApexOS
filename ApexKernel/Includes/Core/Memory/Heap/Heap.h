// ApexKernel/Includes/Core/Memory/Heap/Heap.h
// ApexOS -- Kernel heap allocator (kmalloc / kfree)
//
// Owns heap_block_t and HEAP_MAGIC: the shared block-header type used
// by every file in Core/Memory/Heap/ (Heap.c, HeapSetup.c,
// HeapStats.c). Satellite files #include this header rather than
// redeclaring the type.

#ifndef APEX_CORE_MEMORY_HEAP_H
#define APEX_CORE_MEMORY_HEAP_H

#include "Core/Types.h"

typedef struct heap_block
{
    size_t size; // Payload size (excludes this header)
    bool free;
    struct heap_block *next;
    struct heap_block *prev;
    uint32_t magic; // Corruption check
} heap_block_t;

#define HEAP_MAGIC 0xDEADB00F

void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t size);
void kfree(void *ptr);

#endif // APEX_CORE_MEMORY_HEAP_H
