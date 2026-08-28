// ApexKernel/Formats/AXF/AXL/LibraryLoader.c
// ApexOS -- AXL (.axl library) loading and function resolution

#include "Formats/AXF/AXL/LibraryLoader.h"
#include "Formats/AXF/AXFFormat.h"
#include "Core/Userspace.h"
#include "Core/Memory/Physical/PageBitmap.h"
#include "Core/Memory/Virtual/PageTables.h"
#include "Drivers/Display/VGA.h"
#include "Logs/Log.h"

#define AXF_MAX_FILE_SIZE 4096u

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

uint64_t load_axl_and_resolve(axfs_volume_t *vol, uint64_t dir_inode, const char *lib_name,
                              const char *func_name)
{
    uint64_t lib_inode = axfs_lookup(vol, dir_inode, lib_name);
    if (lib_inode == 0)
    {
        klog_error_prefix();
        vga_puts("axf_load: import: library '");
        vga_puts(lib_name);
        vga_puts("' not found\n");
        return 0;
    }

    static uint8_t filebuf[AXF_MAX_FILE_SIZE];
    uint64_t read_bytes = axfs_read(vol, lib_inode, filebuf, sizeof(filebuf), 0);
    if (read_bytes < sizeof(axf_header_t))
    {
        klog_error("axf_load: import: library file too small");
        return 0;
    }

    axf_header_t header;
    uint8_t *hb = (uint8_t *)&header;
    for (uint32_t i = 0; i < sizeof(header); i++)
        hb[i] = filebuf[i];

    if (header.magic != AXF_MAGIC || header.version != AXF_VERSION)
    {
        klog_error("axf_load: import: invalid library file");
        return 0;
    }

    if (header.type != AXF_TYPE_AXL)
    {
        klog_error_prefix();
        vga_puts("axf_load: import: '");
        vga_puts(lib_name);
        vga_puts("' is not a .axl file\n");
        return 0;
    }

    if (header.section_count == 0 || header.section_count > 8)
    {
        klog_error("axf_load: import: invalid section_count in library");
        return 0;
    }

    // Maps the .axl at USER_LIB_ADDR (same logic as a .axp, see
    // Formats/AXF/AXP/SectionLoader.c -- a single 4KB page in V1).
    void *lib_phys = pmm_alloc_page();
    if (!lib_phys)
    {
        klog_error("axf_load: import: out of physical memory");
        return 0;
    }
    vmm_map(USER_LIB_ADDR, (uint64_t)lib_phys, VMM_USER_RW);

    uint8_t *dst_page = (uint8_t *)USER_LIB_ADDR;
    for (uint32_t i = 0; i < 4096; i++)
        dst_page[i] = 0;

    for (uint32_t s = 0; s < header.section_count; s++)
    {
        axf_section_t sec;
        uint64_t sec_offset = header.section_table_offset + s * sizeof(axf_section_t);
        if (sec_offset + sizeof(sec) > read_bytes)
        {
            klog_error("axf_load: import: section table extends beyond file");
            return 0;
        }
        uint8_t *sb = (uint8_t *)&sec;
        for (uint32_t i = 0; i < sizeof(sec); i++)
            sb[i] = filebuf[sec_offset + i];

        // .axl files are compiled to be loaded at USER_CODE_ADDR (same
        // convention as .axp -- a single "standard" base address in
        // V1) but physically MAPPED at USER_LIB_ADDR. Each section's
        // offset is therefore computed relative to the file's own
        // base address (header.base_addr, or AXF_DEFAULT_BASE_ADDR),
        // not relative to USER_LIB_ADDR directly -- this lets the same
        // generator tool produce a .axp or a .axl without an address
        // distinction.
        uint64_t lib_base = header.base_addr ? header.base_addr : AXF_DEFAULT_BASE_ADDR;

        if (sec.vaddr < lib_base || sec.vaddr + sec.mem_size > lib_base + 4096)
        {
            klog_error("axf_load: import: library section out of supported range");
            return 0;
        }

        uint64_t page_offset = sec.vaddr - lib_base;

        if (sec.file_size > 0)
        {
            if (sec.file_offset + sec.file_size > read_bytes)
            {
                klog_error("axf_load: import: section data extends beyond file");
                return 0;
            }
            for (uint64_t i = 0; i < sec.file_size; i++)
                dst_page[page_offset + i] = filebuf[sec.file_offset + i];
        }
    }

    // Looks up the requested function in the export table.
    if (header.export_count == 0 || header.export_table_offset + header.export_count * sizeof(axf_export_t) > read_bytes)
    {
        klog_error("axf_load: import: library has no valid export table");
        return 0;
    }

    for (uint32_t e = 0; e < header.export_count; e++)
    {
        axf_export_t exp;
        uint64_t exp_offset = header.export_table_offset + e * sizeof(axf_export_t);
        uint8_t *eb = (uint8_t *)&exp;
        for (uint32_t i = 0; i < sizeof(exp); i++)
            eb[i] = filebuf[exp_offset + i];

        if (str_eq(exp.name, func_name))
        {
            // Absolute address = where the function actually lands in
            // memory (USER_LIB_ADDR), not the file's "logical" base
            // address (lib_base) -- exp.offset is relative to the
            // start of the file/section, so it's directly usable here.
            return USER_LIB_ADDR + exp.offset;
        }
    }

    klog_error_prefix();
    vga_puts("axf_load: import: function '");
    vga_puts(func_name);
    vga_puts("' not found in '");
    vga_puts(lib_name);
    vga_puts("'\n");
    return 0;
}
