// ApexKernel/Formats/AXF/AXD/ExportResolution.c
// ApexOS -- AXD export table resolution (internal to axd_load)

#include "Formats/AXF/AXD/ExportResolution.h"
#include "Formats/AXF/SafeRange/SafeRange.h"
#include "Logs/Log.h"

// Single page (pmm_alloc_page(), see HeaderValidation.c's axd_load())
// mapped at AXD_BASE_ADDR for the driver's own code/data -- every
// export offset resolved below must stay inside it. Duplicated here
// rather than pulled from a shared header because HeaderValidation.c's
// AXD_BASE_ADDR is a base address, not a size -- this constant is
// this file's own bound check, independent of that one.
#define AXD_DRIVER_PAGE_SIZE 4096ull

static bool str_eq(const char *a, const char *b)
{
    while (*a && *b)
    {
        if (*a != *b)
            return false;
        a++;
        b++;
    }
    return *a == *b;
}

uint64_t axd_resolve_exports(const axf_header_t *header, const uint8_t *filebuf,
                             uint64_t read_bytes, axd_driver_t *drv)
{
    // header->export_count is attacker-controlled -- checked via
    // axf_array_fits() rather than the raw addition/multiplication,
    // which could otherwise wrap for a large enough export_count.
    if (!axf_array_fits(header->export_table_offset, header->export_count, sizeof(axf_export_t), read_bytes))
    {
        klog_error("axd_load: export table extends beyond file");
        return 0;
    }

    uint64_t init_addr = 0;

    for (uint32_t e = 0; e < header->export_count; e++)
    {
        axf_export_t exp;
        uint64_t exp_offset = header->export_table_offset + e * sizeof(axf_export_t);
        // Per-entry check before reading, on top of the aggregate
        // axf_array_fits() check above the loop.
        if (!axf_range_fits(exp_offset, sizeof(exp), read_bytes))
        {
            klog_error("axd_load: export table entry extends beyond file");
            return 0;
        }
        const uint8_t *eb = filebuf + exp_offset;
        uint8_t *dst = (uint8_t *)&exp;
        for (uint32_t i = 0; i < sizeof(exp); i++)
            dst[i] = eb[i];

        // exp.offset is a raw, attacker-controlled value straight from
        // the .axd file -- with nothing checked here, a malicious
        // driver could point it at ANY kernel address, which
        // HeaderValidation.c then calls through in Ring 0 with full
        // kernel privileges (no syscall gate, unlike .axp/.axl). Must
        // land inside the single page actually allocated for this
        // driver's code/data (see AXD_DRIVER_PAGE_SIZE above) -- this
        // is the one check standing between a crafted .axd file and
        // arbitrary Ring 0 code execution.
        if (exp.offset >= AXD_DRIVER_PAGE_SIZE)
        {
            klog_error("axd_load: export offset out of page bounds");
            return 0;
        }

        uint64_t abs_addr = drv->base_addr + exp.offset;
        drv->export_offsets[e] = abs_addr;

        if (str_eq(exp.name, AXD_INIT_EXPORT_NAME))
            init_addr = abs_addr;
    }

    return init_addr;
}
