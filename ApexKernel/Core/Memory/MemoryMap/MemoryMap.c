// ApexKernel/Core/Memory/MemoryMap/MemoryMap.c
// ApexOS -- E820 memory map parser

#include "Core/Memory/MemoryMap/MemoryMap.h"

mmap_entry_t *mmap_get_entries(void)
{
    return (mmap_entry_t *)MMAP_ADDR;
}

uint32_t mmap_get_count(void)
{
    return *(uint32_t *)MMAP_COUNT_ADDR;
}

uint64_t mmap_get_total_ram(void)
{
    mmap_entry_t *entries = mmap_get_entries();
    uint32_t count = mmap_get_count();
    uint64_t total = 0;

    for (uint32_t i = 0; i < count; i++)
    {
        if (entries[i].type == MMAP_AVAILABLE)
            total += entries[i].length;
    }
    return total;
}
