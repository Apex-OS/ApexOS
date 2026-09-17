// ApexKernel/Boot/UEFI/AcpiFind.c
// ApexOS -- UEFI bootloader: locate the ACPI RSDP (see AcpiFind.h)

#include "Boot/UEFI/AcpiFind.h"

static BOOLEAN guid_eq(const EFI_GUID *a, const EFI_GUID *b)
{
    if (a->data1 != b->data1 || a->data2 != b->data2 || a->data3 != b->data3)
        return FALSE;
    for (int i = 0; i < 8; i++)
        if (a->data4[i] != b->data4[i])
            return FALSE;
    return TRUE;
}

BOOLEAN find_acpi_rsdp(EFI_SYSTEM_TABLE *st, uint64_t *out_rsdp_addr)
{
    const EFI_GUID acpi20 = EFI_ACPI_2_0_TABLE_GUID;
    const EFI_GUID acpi10 = EFI_ACPI_1_0_TABLE_GUID;

    // Two passes rather than one: a firmware that publishes both
    // GUIDs (common -- some also keep a 1.0 entry for compatibility)
    // must resolve to the 2.0 RSDP, which has the XSDT pointer
    // Madt.c's table walk prefers. Scanning once and taking whichever
    // GUID happens to appear first in the array would occasionally
    // pick the weaker 1.0 pointer purely by table ordering, which is
    // firmware-defined and not something to depend on.
    for (size_t i = 0; i < st->number_of_table_entries; i++)
    {
        if (guid_eq(&st->configuration_table[i].vendor_guid, &acpi20))
        {
            *out_rsdp_addr = (uint64_t)st->configuration_table[i].vendor_table;
            return TRUE;
        }
    }

    for (size_t i = 0; i < st->number_of_table_entries; i++)
    {
        if (guid_eq(&st->configuration_table[i].vendor_guid, &acpi10))
        {
            *out_rsdp_addr = (uint64_t)st->configuration_table[i].vendor_table;
            return TRUE;
        }
    }

    return FALSE;
}
