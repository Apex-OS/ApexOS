// ApexKernel/Includes/Arch/x86_64/GDTExtension.h
// ApexOS -- GDT extension for the TSS descriptor

#ifndef APEX_ARCH_X86_64_GDT_EXTENSION_H
#define APEX_ARCH_X86_64_GDT_EXTENSION_H

#include "Core/Types.h"

// Installs the TSS descriptor into the GDT.
uint8_t gdt_install_tss(uint64_t base, uint32_t limit);

#endif // APEX_ARCH_X86_64_GDT_EXTENSION_H
