// ApexKernel/Formats/AXF/AXP/ImportResolution.c
// ApexOS -- AXP import table resolution (loads needed .axl libraries)

#include "Formats/AXF/AXP/ImportResolution.h"
#include "Formats/AXF/AXL/LibraryLoader.h"
#include "Core/Memory/Physical/PageBitmap.h"
#include "Core/Memory/Virtual/PageTables.h"
#include "Logs/Log.h"

bool axp_resolve_imports(axfs_volume_t *vol, uint64_t dir_inode, const axf_header_t *header,
                         const uint8_t *filebuf, uint64_t read_bytes)
{
    if (header->import_count == 0)
        return true; // nothing to resolve -- standalone binary

    if (header->import_table_offset + header->import_count * sizeof(axf_import_t) > read_bytes)
    {
        klog_error("axf_load: import table extends beyond file");
        return false;
    }

    uint64_t resolved[64]; // matches AXF_IMPORT_TABLE_ADDR -- 64 imports max in V1 (plenty for now)
    if (header->import_count > 64)
    {
        klog_error("axf_load: too many imports (max 64 in v1)");
        return false;
    }

    for (uint32_t i = 0; i < header->import_count; i++)
    {
        axf_import_t imp;
        uint64_t imp_offset = header->import_table_offset + i * sizeof(axf_import_t);
        const uint8_t *ib = filebuf + imp_offset;
        uint8_t *dst = (uint8_t *)&imp;
        for (uint32_t k = 0; k < sizeof(imp); k++)
            dst[k] = ib[k];

        uint64_t addr = load_axl_and_resolve(vol, dir_inode, imp.lib_name, imp.func_name);
        if (addr == 0)
        {
            // load_axl_and_resolve() already printed a precise error
            // (library not found / function missing / etc.) -- no
            // need to add another one here.
            return false;
        }
        resolved[i] = addr;
    }

    // Writes the resolved import table into memory, at the fixed
    // address the .axp will read from at startup (see AXFFormat.h:
    // AXF_IMPORT_TABLE_ADDR). Needs its own page -- separate from the
    // .axp's code page, even though the chosen address
    // (USER_CODE_ADDR + 4096) makes it logically "adjacent".
    void *import_table_phys = pmm_alloc_page();
    if (!import_table_phys)
    {
        klog_error("axf_load: out of physical memory (import table)");
        return false;
    }
    vmm_map(AXF_IMPORT_TABLE_ADDR, (uint64_t)import_table_phys, VMM_USER_RW);

    uint64_t *import_table = (uint64_t *)AXF_IMPORT_TABLE_ADDR;
    for (uint32_t i = 0; i < header->import_count; i++)
        import_table[i] = resolved[i];

    return true;
}
