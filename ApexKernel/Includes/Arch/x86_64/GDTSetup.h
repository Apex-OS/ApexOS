// ApexKernel/Includes/Arch/x86_64/GDTSetup.h
// ApexOS -- Global Descriptor Table
//
// Owns every type/constant shared across the GDT module and used
// elsewhere in the kernel (IDT.c uses GDT_KERNEL_CODE, TSS.c uses
// GDT_TSS, GDTExtension.c uses gdt_entry_t, etc).

#ifndef APEX_ARCH_X86_64_GDT_SETUP_H
#define APEX_ARCH_X86_64_GDT_SETUP_H

#include "Core/Types.h"

// One GDT descriptor (8 bytes).
typedef struct __attribute__((packed))
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} gdt_entry_t;

// GDTR register layout.
typedef struct __attribute__((packed))
{
    uint16_t limit; // GDT size - 1
    uint64_t base;  // GDT address
} gdt_descriptor_t;

// Segment selectors.
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE 0x18
#define GDT_USER_DATA 0x20

// Access byte flags.
#define GDT_ACCESS_PRESENT (1 << 7)
#define GDT_ACCESS_RING0 (0 << 5)
#define GDT_ACCESS_RING3 (3 << 5)
#define GDT_ACCESS_CODE (1 << 4) | (1 << 3)
#define GDT_ACCESS_DATA (1 << 4)
#define GDT_ACCESS_RW (1 << 1)

// Granularity flags.
#define GDT_FLAG_LONG (1 << 5) // 64-bit segment
#define GDT_FLAG_GRAN (1 << 7) // 4KB granularity

#define GDT_TSS 0x28

// User selectors with RPL=3.
#define GDT_USER_CODE_RPL3 (0x18 | 3)
#define GDT_USER_DATA_RPL3 (0x20 | 3)

void gdt_init(void);

#endif // APEX_ARCH_X86_64_GDT_SETUP_H
