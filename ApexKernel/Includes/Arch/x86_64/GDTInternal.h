// ApexKernel/Includes/Arch/x86_64/GDTInternal.h
// ApexOS -- Shared internal state for the GDT module
//
// gdt[] and gdt_desc are defined once in GDTSetup.c and shared via
// extern with GDTExtension.c. Not part of the public GDTSetup.h API.

#ifndef APEX_ARCH_X86_64_GDT_INTERNAL_H
#define APEX_ARCH_X86_64_GDT_INTERNAL_H

#include "Arch/x86_64/GDTSetup.h"

// Extended GDT -- 7 entries (5 normal + 2 for the TSS).
extern uint64_t gdt[7];
extern gdt_descriptor_t gdt_desc;

extern void gdt_flush(uint64_t gdt_descriptor);

#endif // APEX_ARCH_X86_64_GDT_INTERNAL_H
