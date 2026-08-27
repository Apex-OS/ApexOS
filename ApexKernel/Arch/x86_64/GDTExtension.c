// ApexKernel/Arch/x86_64/GDTExtension.c
// ApexOS -- GDT extension for the TSS descriptor

#include "Arch/x86_64/GDTExtension.h"
#include "Arch/x86_64/GDTInternal.h"

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
