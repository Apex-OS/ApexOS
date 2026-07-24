// apex-kernel/core/gdt.c
// ApexOS -- Global Descriptor Table

#include "gdt.h"

extern void gdt_flush(uint64_t gdt_descriptor);

// Extended GDT -- 7 entries (5 normal + 2 for the TSS).
static uint64_t gdt[7];
static gdt_descriptor_t gdt_desc;

static void gdt_set_entry(int idx, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t granularity)
{
    uint8_t *entry = (uint8_t *)&gdt[idx];
    entry[0] = limit & 0xFF;
    entry[1] = (limit >> 8) & 0xFF;
    entry[2] = base & 0xFF;
    entry[3] = (base >> 8) & 0xFF;
    entry[4] = (base >> 16) & 0xFF;
    entry[5] = access;
    entry[6] = (granularity & 0xF0) | ((limit >> 16) & 0x0F);
    entry[7] = (base >> 24) & 0xFF;
}

// Installs the TSS descriptor (occupies 2 GDT entries in 64-bit mode).
uint8_t gdt_install_tss(uint64_t base, uint32_t limit)
{
    // Entry 5 (0x28) -- low half of the TSS descriptor.
    uint8_t *entry = (uint8_t *)&gdt[5];
    entry[0] = limit & 0xFF;
    entry[1] = (limit >> 8) & 0xFF;
    entry[2] = base & 0xFF;
    entry[3] = (base >> 8) & 0xFF;
    entry[4] = (base >> 16) & 0xFF;
    entry[5] = 0x89; // Present, Ring 0, TSS Available
    entry[6] = 0x00;
    entry[7] = (base >> 24) & 0xFF;

    // Entry 6 (0x30) -- high half (base[63:32]).
    uint8_t *entry2 = (uint8_t *)&gdt[6];
    entry2[0] = (base >> 32) & 0xFF;
    entry2[1] = (base >> 40) & 0xFF;
    entry2[2] = (base >> 48) & 0xFF;
    entry2[3] = (base >> 56) & 0xFF;
    entry2[4] = 0;
    entry2[5] = 0;
    entry2[6] = 0;
    entry2[7] = 0;

    // Reloads the GDT with its new size.
    gdt_desc.limit = sizeof(gdt) - 1;
    gdt_desc.base = (uint64_t)&gdt;
    gdt_flush((uint64_t)&gdt_desc);

    return 0x28;
}

void gdt_init(void)
{
    // 0 -- Null descriptor.
    gdt[0] = 0;

    // 1 -- Kernel Code (0x08).
    gdt_set_entry(1, 0, 0xFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_CODE | GDT_ACCESS_RW,
                  GDT_FLAG_LONG | GDT_FLAG_GRAN);

    // 2 -- Kernel Data (0x10).
    gdt_set_entry(2, 0, 0xFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_DATA | GDT_ACCESS_RW,
                  GDT_FLAG_GRAN);

    // 3 -- User Code (0x18).
    gdt_set_entry(3, 0, 0xFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_CODE | GDT_ACCESS_RW,
                  GDT_FLAG_LONG | GDT_FLAG_GRAN);

    // 4 -- User Data (0x20).
    gdt_set_entry(4, 0, 0xFFFFF,
                  GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_DATA | GDT_ACCESS_RW,
                  GDT_FLAG_GRAN);

    // 5 & 6 -- TSS (filled in by tss_init).
    gdt[5] = 0;
    gdt[6] = 0;

    gdt_desc.limit = sizeof(gdt) - 1;
    gdt_desc.base = (uint64_t)&gdt;
    gdt_flush((uint64_t)&gdt_desc);
}