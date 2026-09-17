// ApexKernel/Includes/Core/Acpi/Rsdp.h
// ApexOS -- ACPI RSDP (Root System Description Pointer) structure and
// BIOS-path discovery
//
// The RSDP is ACPI's single entry point: everything else (RSDT/XSDT,
// then MADT within it -- see Rsdt.h/Madt.h) is only reachable by
// first finding this structure. Two ways to find it exist, matching
// the two-path split already established for boot info generally
// (see Core/Boot/AcpiInfo.h): UEFI publishes its address directly
// (Boot/UEFI/AcpiFind.c already handled that side); BIOS instead
// guarantees it lives in one of two fixed, scannable physical memory
// regions, which is what rsdp_find() below searches.

#ifndef APEX_CORE_ACPI_RSDP_H
#define APEX_CORE_ACPI_RSDP_H

#include "Core/Types.h"

// ACPI 1.0 RSDP -- 20 bytes. Every RSDP, even a 2.0+ one, starts with
// exactly this layout (2.0 just appends more fields after it, see
// rsdp_v2_t below) -- so this struct alone is always safe to read the
// signature/checksum/revision from, regardless of which version is
// actually present.
typedef struct __attribute__((packed))
{
    char signature[8]; // "RSD PTR " (note the trailing space -- 8 bytes exactly, not NUL-terminated)
    uint8_t checksum;  // Sum of all 20 bytes (this struct) must be 0 mod 256
    char oem_id[6];
    uint8_t revision;   // 0 = ACPI 1.0 (RSDT only), 2 = ACPI 2.0+ (adds XSDT, see rsdp_v2_t)
    uint32_t rsdt_addr; // Physical address of the RSDT -- 32-bit, always present even in a v2 RSDP
} rsdp_v1_t;

// ACPI 2.0+ RSDP -- 36 bytes total (v1's 20 + these 16). Only valid to
// read the fields below rsdp_v1_t's `length` if revision >= 2 --
// checked by rsdp_find() before this struct is ever cast onto a found
// RSDP, since a genuine 1.0 RSDP is only 20 bytes long and reading
// past it reads whatever memory happens to follow.
typedef struct __attribute__((packed))
{
    rsdp_v1_t v1;
    uint32_t length;           // Total size of this structure, including the v1 portion
    uint64_t xsdt_addr;        // Physical address of the XSDT (64-bit table pointers) -- see Rsdt.h
    uint8_t extended_checksum; // Sum of all `length` bytes (not just these extra 16) must be 0 mod 256
    uint8_t reserved[3];
} rsdp_v2_t;

#define RSDP_SIGNATURE "RSD PTR "

// Scans the two BIOS-guaranteed regions for a valid "RSD PTR "
// signature + checksum: the first 1KB of the Extended BIOS Data Area
// (EBDA -- its segment is itself found via the BIOS data area at
// physical 0x40E, a real-mode-era convention that still holds on
// every BIOS this kernel has been tested against) and the main BIOS
// read-only memory area 0xE0000-0xFFFFF. Both regions are already
// covered by the kernel's full-RAM identity map by the time this
// runs (see Core/Memory/Virtual/IdentityMap.c -- vmm_init() must have
// already been called), so these are ordinary physical addresses to
// read directly, no special-casing needed.
//
// Only meaningful on a BIOS boot -- on UEFI, Boot/UEFI/AcpiFind.c
// already found the RSDP via the firmware's configuration table
// (which is the only correct way on UEFI: nothing guarantees it sits
// in either of the two regions searched here), so Madt.c checks
// Core/Boot/AcpiInfo.h's acpi_info_t.valid first and only falls back
// to calling this when that's unset.
//
// Returns NULL if no valid signature+checksum is found in either
// region -- callers treat that the same as UEFI publishing neither
// ACPI GUID (see AcpiInfo.h's file header: single-CPU fallback, no
// SMP).
void *rsdp_find(void);

#endif // APEX_CORE_ACPI_RSDP_H
