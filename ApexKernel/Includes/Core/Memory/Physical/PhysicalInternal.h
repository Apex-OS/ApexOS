// ApexKernel/Includes/Core/Memory/Physical/PhysicalInternal.h
// ApexOS -- Shared internal state for the Physical Memory Manager
//
// pmm_bitmap/pmm_total/pmm_used/pmm_bitmap_size are defined once in
// PageBitmap.c and shared via extern with PhysicalMemoryDetection.c
// and PhysicalMemoryStats.c. The bitmap_set/clear/test inline helpers
// live here too since every file in Physical/ needs them. Not part of
// the public PageBitmap.h API: nothing outside Core/Memory/Physical/
// should touch these directly.

#ifndef APEX_CORE_MEMORY_PHYSICAL_INTERNAL_H
#define APEX_CORE_MEMORY_PHYSICAL_INTERNAL_H

#include "Core/Types.h"

#include "Core/Memory/Physical/PageBitmap.h"

extern uint8_t *pmm_bitmap;
extern uint64_t pmm_total;       // Total page count
extern uint64_t pmm_used;        // Pages currently in use
extern uint64_t pmm_bitmap_size; // Bitmap size in bytes

static inline void bitmap_set(uint64_t page)
{
    pmm_bitmap[page / 8] |= (1 << (page % 8));
}

static inline void bitmap_clear(uint64_t page)
{
    pmm_bitmap[page / 8] &= ~(1 << (page % 8));
}

static inline bool bitmap_test(uint64_t page)
{
    return (pmm_bitmap[page / 8] >> (page % 8)) & 1;
}

#endif // APEX_CORE_MEMORY_PHYSICAL_INTERNAL_H
