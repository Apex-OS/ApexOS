// ApexKernel/Core/Acpi/Rsdt.c
// ApexOS -- ACPI RSDT/XSDT table lookup (see Rsdt.h)

#include "Core/Acpi/Rsdt.h"

// Same checksum scheme as Rsdp.c's checksum_valid() -- duplicated
// rather than shared across the two .c files since it's a 4-line
// static helper and neither file has any other reason to depend on
// the other (see the project's Internal.h pattern for when sharing
// would be warranted -- this doesn't rise to that: it's not shared
// mutable state, just the same tiny well-known algorithm).
static bool checksum_valid(const uint8_t *ptr, uint32_t len)
{
    uint8_t sum = 0;
    for (uint32_t i = 0; i < len; i++)
        sum = (uint8_t)(sum + ptr[i]);
    return sum == 0;
}

static bool signature_matches(const acpi_sdt_header_t *hdr, const char signature[4])
{
    for (int i = 0; i < 4; i++)
        if (hdr->signature[i] != signature[i])
            return false;
    return true;
}

void *acpi_find_table(uint64_t rsdt_addr, bool is_xsdt, const char signature[4])
{
    acpi_sdt_header_t *rsdt = (acpi_sdt_header_t *)rsdt_addr;

    if (!checksum_valid((uint8_t *)rsdt, rsdt->length))
        return NULL;

    // Entries start immediately after the header -- their count isn't
    // stored anywhere explicit, it's derived from how many pointer-
    // sized slots fit in the remaining (length - header size) bytes.
    // This is why is_xsdt matters here specifically: an XSDT's entries
    // are twice as wide, so the same remaining byte count yields half
    // as many actual table pointers.
    uint32_t entry_size = is_xsdt ? sizeof(uint64_t) : sizeof(uint32_t);
    uint32_t entries_bytes = rsdt->length - sizeof(acpi_sdt_header_t);
    uint32_t entry_count = entries_bytes / entry_size;

    uint8_t *entries_start = (uint8_t *)rsdt + sizeof(acpi_sdt_header_t);

    for (uint32_t i = 0; i < entry_count; i++)
    {
        uint64_t table_addr;
        if (is_xsdt)
            table_addr = ((uint64_t *)entries_start)[i];
        else
            table_addr = ((uint32_t *)entries_start)[i];

        acpi_sdt_header_t *table = (acpi_sdt_header_t *)table_addr;
        if (signature_matches(table, signature) && checksum_valid((uint8_t *)table, table->length))
            return table;
    }

    return NULL;
}
