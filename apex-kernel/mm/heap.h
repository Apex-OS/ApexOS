// apex-kernel/mm/heap.h
// ApexOS -- Kernel heap (kmalloc / kfree)

#ifndef APEX_HEAP_H
#define APEX_HEAP_H

#include "../include/apex/types.h"

#define HEAP_START 0x800000    // 8MB
#define HEAP_MAX_SIZE 0x800000 // 8MB -- grown from 4MB to fit the graphics
                               // back-buffer (drivers/framebuffer.c), sized
                               // off the VBE mode picked at boot (capped at
                               // 1600x900x4 bytes = ~5.76MB, see stage2.asm:
                               // MAX_FB_WIDTH/MAX_FB_HEIGHT). Heap now spans
                               // [0x800000, 0x1000000) -- 8MB to 16MB. If you
                               // change this again, AXD_BASE_ADDR
                               // (core/axf/axd.c) MUST be re-checked for
                               // overlap -- it was moved past this range for
                               // exactly that reason.

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