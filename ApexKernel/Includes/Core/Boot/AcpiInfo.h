// ApexKernel/Includes/Core/Boot/AcpiInfo.h
// ApexOS -- Boot-time ACPI RSDP info
//
// Same two-path split as DiskLayout.h, for the same underlying
// reason: UEFI and BIOS learn the RSDP's location completely
// differently, and this struct is the format both sides agree to
// leave it in for Core/Acpi/Madt.c (which parses RSDT/XSDT -> MADT
// from it for SMP CPU discovery) to read, regardless of which path
// actually wrote it.
//
// UEFI path: Boot/UEFI/Main.c calls AcpiFind.c to read the RSDP
// address straight out of the firmware's configuration table (no
// scanning needed or possible -- UEFI doesn't guarantee it sits at
// any particular physical address) and writes it here before jumping
// to the kernel.
//
// BIOS path: Boot/BIOS/ does no ACPI work of its own (same reasoning
// DiskLayout.h gives for GPT: this bootloader keeps its own boot
// sequence minimal and pushes work to the kernel where it's easier to
// get right in C than in 16-bit real-mode assembly). Instead
// Core/Acpi/RsdpFind.c scans the fixed, spec-guaranteed BIOS search
// area itself once the kernel is running in long mode with the full
// identity map already up (see Core/Memory/Virtual/IdentityMap.c --
// low memory, where the RSDP lives on a BIOS boot, is covered by that
// map like everything else), and fills this same struct in-place
// rather than relying on anything the bootloader wrote.
//
// `valid == 0` (the default if this address is never written, e.g. a
// UEFI boot whose firmware publishes neither ACPI GUID) means
// Madt.c falls back to treating this as a single-CPU system -- no
// APs discovered, no APIC/IPI/per-CPU setup attempted. Degraded but
// safe: the boot processor keeps running exactly as it does today.

#ifndef APEX_CORE_BOOT_ACPI_INFO_H
#define APEX_CORE_BOOT_ACPI_INFO_H

#include "Core/Types.h"

// Placed in the gap between DISK_LAYOUT_INFO_ADDR (0x9400, 9 bytes)
// and MMAP_ADDR (0x9600) -- same proven-safe low-memory scratch
// region DiskLayout.h uses, same reasoning: 0xA000+ overlaps the EBDA
// and has already corrupted a BIOS boot once when a boot-info address
// strayed into it (see DiskLayout.h's own history of that exact bug).
#define ACPI_INFO_ADDR 0x9500

typedef struct __attribute__((packed))
{
    uint64_t rsdp_addr; // Physical address of the RSDP -- see Core/Acpi/Rsdp.h for its layout
    uint8_t valid;      // 0 = not written / not found -- see file header for the fallback
} acpi_info_t;

#endif // APEX_CORE_BOOT_ACPI_INFO_H
