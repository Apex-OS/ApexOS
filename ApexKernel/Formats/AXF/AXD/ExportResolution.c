// ApexKernel/Formats/AXF/AXD/ExportResolution.c
// ApexOS -- AXD export table resolution (internal to axd_load)

#include "Formats/AXF/AXD/ExportResolution.h"
#include "ApexLib/Logs/Log.h"

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
    if (header->export_table_offset + header->export_count * sizeof(axf_export_t) > read_bytes)
    {
        klog_error("axd_load: export table extends beyond file");
        return 0;
    }

    uint64_t init_addr = 0;

    for (uint32_t e = 0; e < header->export_count; e++)
    {
        axf_export_t exp;
        uint64_t exp_offset = header->export_table_offset + e * sizeof(axf_export_t);
        const uint8_t *eb = filebuf + exp_offset;
        uint8_t *dst = (uint8_t *)&exp;
        for (uint32_t i = 0; i < sizeof(exp); i++)
            dst[i] = eb[i];

        uint64_t abs_addr = drv->base_addr + exp.offset;
        drv->export_offsets[e] = abs_addr;

        if (str_eq(exp.name, AXD_INIT_EXPORT_NAME))
            init_addr = abs_addr;
    }

    return init_addr;
}
