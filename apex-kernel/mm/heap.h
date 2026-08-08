// apex-kernel/mm/heap.h
// ApexOS -- Kernel heap (kmalloc / kfree)

#ifndef APEX_HEAP_H
#define APEX_HEAP_H

#include "../include/apex/types.h"

#define HEAP_START 0x800000    // 8MB
#define HEAP_MAX_SIZE 0x400000 // 4MB

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