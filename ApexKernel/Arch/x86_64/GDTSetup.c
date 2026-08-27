// ApexKernel/Arch/x86_64/GDTSetup.c
// ApexOS -- Global Descriptor Table

#include "Arch/x86_64/GDTSetup.h"
#include "Arch/x86_64/GDTInternal.h"

uint64_t gdt[7];
gdt_descriptor_t gdt_desc;

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
