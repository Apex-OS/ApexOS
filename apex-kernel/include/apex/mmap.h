// apex-kernel/include/apex/mmap.h
// ApexOS -- E820 memory map

#ifndef APEX_MMAP_H
#define APEX_MMAP_H

#include "types.h"

// Fixed addresses (populated by stage2 at boot).
#define MMAP_ADDR 0x500
#define MMAP_COUNT_ADDR 0x4FC

// E820 entry types.
#define MMAP_AVAILABLE 1    // Usable RAM
#define MMAP_RESERVED 2     // Reserved
#define MMAP_ACPI_RECLAIM 3 // ACPI reclaimable
#define MMAP_ACPI_NVS 4     // ACPI non-volatile
#define MMAP_BAD 5          // Defective memory

// One E820 entry (24 bytes).
typedef struct __attribute__((packed))
{
    uint64_t base;
    uint64_t length;
    uint32_t type;       // See MMAP_* defines above
    uint32_t acpi_attrs; // Extended ACPI attributes
} mmap_entry_t;

mmap_entry_t *mmap_get_entries(void);
uint32_t mmap_get_count(void);
uint64_t mmap_get_total_ram(void);
void mmap_print(void);

#endif // APEX_MMAP_H