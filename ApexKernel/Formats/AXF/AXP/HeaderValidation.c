// ApexKernel/Formats/AXF/AXP/HeaderValidation.c
// ApexOS -- AXP (.axp executable) header validation

#include "Formats/AXF/AXP/HeaderValidation.h"
#include "Core/Userspace.h"
#include "ApexLib/Logs/Log.h"

bool axp_validate_header(const axf_header_t *header)
{
    if (header->magic != AXF_MAGIC)
    {
        klog_error("axf_load: bad magic (not a valid AXF file)");
        return false;
    }

    if (header->version != AXF_VERSION)
    {
        klog_error_n("axf_load: unsupported AXF version", header->version);
        return false;
    }

    if (header->type != AXF_TYPE_AXP)
    {
        // V1: only .axp files (standalone programs) are directly
        // executable. .axl/.axd need different handling (loaded as a
        // library / driver instead), see Formats/AXF/AXD/ for .axd.
        klog_error("axf_load: only .axp files can be run directly (v1)");
        return false;
    }

    if (header->section_count == 0 || header->section_count > 8)
    {
        // Guard: no sections, or an unreasonable count (the file is
        // likely corrupted or malicious).
        klog_error_n("axf_load: invalid section_count", header->section_count);
        return false;
    }

    uint64_t base_addr = header->base_addr ? header->base_addr : AXF_DEFAULT_BASE_ADDR;

    // V1: only accepts the default base (USER_CODE_ADDR) -- an
    // arbitrary base address would require dynamically choosing where
    // to map pages, which the current ring3 return mechanism doesn't
    // support yet (USER_CODE_ADDR/USER_STACK_ADDR are fixed constants
    // shared with `ring3`).
    if (base_addr != USER_CODE_ADDR)
    {
        klog_error("axf_load: only the default base address is supported (v1)");
        return false;
    }

    return true;
}
