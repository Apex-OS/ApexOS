// ApexKernel/Includes/Boot/UEFI/MemoryMapConvert.h
// ApexOS -- UEFI bootloader: memory map conversion to kernel E820 format

#ifndef APEX_BOOT_UEFI_MEMORY_MAP_CONVERT_H
#define APEX_BOOT_UEFI_MEMORY_MAP_CONVERT_H

#include "Boot/UEFI/Efi.h"

#define MMAP_ADDR 0x9600
#define MMAP_COUNT_ADDR 0x95FC

// Mirrors Core/Memory/MemoryMap/MemoryMap.h's mmap_entry_t exactly --
// same duplication reasoning as GraphicsSetup.h's fb_boot_info_t.
#define MMAP_AVAILABLE 1
#define MMAP_RESERVED 2

typedef struct __attribute__((packed))
{
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi_attrs;
} mmap_entry_t;

#define MAX_MMAP_ENTRIES 128 // generous -- BIOS E820 maps are typically under 20 entries;
                             // UEFI's tend to be more fragmented but still comfortably under this

// Converts one UEFI memory map into this kernel's existing E820-style
// mmap_entry_t array at MMAP_ADDR, writing the count to
// MMAP_COUNT_ADDR. Every UEFI descriptor becomes one E820 entry; type
// mapping is coarse (EfiConventionalMemory/EfiBootServicesCode/
// EfiBootServicesData -> MMAP_AVAILABLE, everything else ->
// MMAP_RESERVED) since this kernel's PMM (Core/Memory/Physical/
// PageBitmap.c) only distinguishes "usable RAM" from "not usable RAM"
// at boot -- see MemoryMap.h's MMAP_* constants for the finer-grained
// types this simplification deliberately doesn't populate.
// Boot/BootServicesCode/Data are counted as available because
// EfiBootServicesCode/Data are freed for general use once
// ExitBootServices() has run (see Main.c: efi_main, called right
// before that), matching what the UEFI spec says about that memory's
// fate.
void convert_memory_map(EFI_MEMORY_DESCRIPTOR *map, size_t map_size, size_t descriptor_size);

#endif // APEX_BOOT_UEFI_MEMORY_MAP_CONVERT_H
