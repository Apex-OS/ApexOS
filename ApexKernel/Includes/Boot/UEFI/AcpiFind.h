// ApexKernel/Includes/Boot/UEFI/AcpiFind.h
// ApexOS -- UEFI bootloader: locate the ACPI RSDP via the firmware's
// configuration table
//
// Mirrors GptReader.h's approach exactly: a shared fixed-address
// struct both this bootloader and the kernel agree on
// (Core/Boot/AcpiInfo.h holds the kernel-side copy of this same
// layout), non-fatal on failure (SMP with a single detected CPU --
// this one, the boot processor -- is a safe degraded fallback; see
// Core/Acpi/Madt.c).

#ifndef APEX_BOOT_UEFI_ACPI_FIND_H
#define APEX_BOOT_UEFI_ACPI_FIND_H

#include "Boot/UEFI/Efi.h"

typedef struct __attribute__((packed))
{
    uint64_t rsdp_addr; // physical address of the RSDP, 0 if not found
    uint8_t valid;
} acpi_info_t;

#define ACPI_INFO_ADDR 0x9500

// Walks system_table->configuration_table looking for the ACPI 2.0
// GUID first, falling back to the ACPI 1.0 GUID if 2.0 isn't present
// (some older/lighter firmware, and most notably OVMF configurations
// without a modern QEMU ACPI table build, may only publish 1.0) --
// see AcpiFind.c for why 2.0 is strictly preferred when both exist
// (XSDT vs RSDT, see Core/Acpi/Madt.c). Returns FALSE (out left
// unwritten) if neither GUID is found in the table.
BOOLEAN find_acpi_rsdp(EFI_SYSTEM_TABLE *st, uint64_t *out_rsdp_addr);

#endif // APEX_BOOT_UEFI_ACPI_FIND_H
