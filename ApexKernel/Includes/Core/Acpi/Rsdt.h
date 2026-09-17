// ApexKernel/Includes/Core/Acpi/Rsdt.h
// ApexOS -- ACPI RSDT/XSDT (Root/Extended System Description Table)
//
// Once Rsdp.h's structure is found, its rsdt_addr (or xsdt_addr on a
// v2 RSDP) points to one of these -- an array of pointers to every
// other ACPI table the firmware publishes (MADT among them, see
// Madt.h, which is the only one this kernel currently cares about).
// RSDT and XSDT share this exact same layout except for pointer
// width (32-bit vs 64-bit entries) -- acpi_find_table() below is
// written once and handles both via a width flag, rather than two
// near-duplicate functions.

#ifndef APEX_CORE_ACPI_RSDT_H
#define APEX_CORE_ACPI_RSDT_H

#include "Core/Types.h"

// Common header every ACPI table starts with (RSDT/XSDT/MADT/all the
// others this kernel doesn't parse yet) -- defined once here rather
// than duplicated in Madt.h, since acpi_find_table() below needs to
// read a candidate table's signature/length before knowing which
// specific table type it turned out to be.
typedef struct __attribute__((packed))
{
    char signature[4];
    uint32_t length; // Total table size in bytes, including this header -- covers the whole checksum region
    uint8_t revision;
    uint8_t checksum; // Sum of all `length` bytes must be 0 mod 256, same scheme as Rsdp.c's checksum_valid()
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} acpi_sdt_header_t;

// Searches the RSDT/XSDT pointed to by rsdt_addr for a table matching
// `signature` (e.g. "APIC" for MADT -- see Madt.h) and returns its
// address, or NULL if no such table is published. `is_xsdt` selects
// whether the entries following the header are read as uint32_t
// (RSDT) or uint64_t (XSDT) -- see Rsdp.h: a v2 RSDP's xsdt_addr
// should always be preferred when available (wider pointers, no
// practical limitation), falling back to a v1 RSDP's rsdt_addr only
// when no XSDT exists. Callers (Madt.c) don't validate rsdt_addr's own
// checksum before calling this -- done internally here instead, since
// every caller would otherwise need to repeat it.
void *acpi_find_table(uint64_t rsdt_addr, bool is_xsdt, const char signature[4]);

#endif // APEX_CORE_ACPI_RSDT_H
