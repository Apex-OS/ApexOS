// ApexKernel/Boot/UEFI/MemoryMapConvert.c
// ApexOS -- UEFI bootloader: memory map conversion to kernel E820
// format (see MemoryMapConvert.h)

#include "Boot/UEFI/MemoryMapConvert.h"

void convert_memory_map(EFI_MEMORY_DESCRIPTOR *map, size_t map_size, size_t descriptor_size)
{
    mmap_entry_t *out = (mmap_entry_t *)(uint64_t)MMAP_ADDR;
    uint32_t *out_count = (uint32_t *)(uint64_t)MMAP_COUNT_ADDR;

    uint32_t count = 0;
    size_t entries = map_size / descriptor_size;

    for (size_t i = 0; i < entries && count < MAX_MMAP_ENTRIES; i++)
    {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)map + i * descriptor_size);

        out[count].base = desc->physical_start;
        out[count].length = desc->number_of_pages * 4096ULL;
        out[count].acpi_attrs = 0;

        if (desc->type == EfiConventionalMemory ||
            desc->type == EfiBootServicesCode ||
            desc->type == EfiBootServicesData)
            out[count].type = MMAP_AVAILABLE;
        else
            out[count].type = MMAP_RESERVED;

        count++;
    }

    *out_count = count;
}
