// ApexKernel/Core/Acpi/Rsdp.c
// ApexOS -- BIOS-path RSDP discovery (see Rsdp.h)

#include "Core/Acpi/Rsdp.h"

// Sums every byte over `len` starting at `ptr` and checks the total
// is 0 mod 256 -- the ACPI checksum scheme used by every table in
// this file and Rsdt.c/Madt.c, not just the RSDP: a valid table's
// bytes (including its own checksum byte, chosen by whatever built
// the table specifically to make this true) always sum to exactly
// 0 mod 256. Deliberately returns bool rather than the sum itself --
// no caller needs the actual value, only whether it validates.
static bool checksum_valid(const uint8_t *ptr, uint32_t len)
{
    uint8_t sum = 0;
    for (uint32_t i = 0; i < len; i++)
        sum = (uint8_t)(sum + ptr[i]);
    return sum == 0;
}

static bool signature_matches(const uint8_t *ptr)
{
    const char *sig = RSDP_SIGNATURE;
    for (int i = 0; i < 8; i++)
        if (ptr[i] != (uint8_t)sig[i])
            return false;
    return true;
}

// Checks one candidate address for a valid RSDP -- shared by both
// regions rsdp_find() scans below. A v1 RSDP is only 20 bytes and its
// checksum only covers those 20; a v2 RSDP additionally carries its
// own `length` field and a second checksum over the whole extended
// structure (see Rsdp.h's rsdp_v2_t comment) -- checked in that order
// (signature, then v1 checksum, then only if revision >= 2 the
// extended checksum) so a genuine v1 RSDP is never rejected for
// lacking fields it was never supposed to have, and a corrupt
// (signature-only-matching garbage) region is rejected before any
// attempt to read fields past the guaranteed 20 bytes.
static void *check_candidate(uint8_t *ptr)
{
    if (!signature_matches(ptr))
        return NULL;

    if (!checksum_valid(ptr, sizeof(rsdp_v1_t)))
        return NULL;

    rsdp_v1_t *v1 = (rsdp_v1_t *)ptr;
    if (v1->revision >= 2)
    {
        rsdp_v2_t *v2 = (rsdp_v2_t *)ptr;
        if (!checksum_valid(ptr, v2->length))
            return NULL;
    }

    return ptr;
}

void *rsdp_find(void)
{
    // EBDA segment lives as a 16-bit real-mode segment value at
    // physical 0x40E (part of the BIOS Data Area, a convention
    // predating ACPI itself) -- shifted left 4 to get its actual
    // physical byte address, standard real-mode segment:offset math
    // with offset 0. Only the first 1KB of the EBDA is searched, per
    // the ACPI spec's own guarantee of where the RSDP will be within
    // it, in 16-byte (paragraph) steps -- the RSDP is always
    // paragraph-aligned.
    uint16_t ebda_segment = *(uint16_t *)(uint64_t)0x40E;
    uint64_t ebda_addr = (uint64_t)ebda_segment << 4;

    // A few BIOSes report ebda_segment as 0 (or another clearly
    // bogus value) when they don't actually relocate the EBDA --
    // skip the EBDA search entirely rather than scanning from
    // physical address 0, which is mapped (see IdentityMap.c) but
    // guaranteed to not hold anything ACPI-related and wastes 1KB of
    // scanning for nothing.
    if (ebda_addr != 0)
    {
        for (uint64_t addr = ebda_addr; addr < ebda_addr + 1024; addr += 16)
        {
            void *found = check_candidate((uint8_t *)addr);
            if (found)
                return found;
        }
    }

    // Main BIOS read-only memory area -- the ACPI spec's other
    // guaranteed search region, independent of the EBDA. Some
    // firmware puts the RSDP here instead of (or as well as) the
    // EBDA, so both are always searched rather than stopping after
    // the EBDA comes up empty.
    for (uint64_t addr = 0xE0000; addr < 0x100000; addr += 16)
    {
        void *found = check_candidate((uint8_t *)addr);
        if (found)
            return found;
    }

    return NULL;
}
