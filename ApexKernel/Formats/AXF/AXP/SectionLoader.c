// ApexKernel/Formats/AXF/AXP/SectionLoader.c
// ApexOS -- AXP section mapping

#include "Formats/AXF/AXP/SectionLoader.h"
#include "Core/Userspace.h"
#include "Core/Memory/Physical/PageBitmap.h"
#include "Core/Memory/Virtual/PageTables.h"
#include "Formats/AXF/SafeRange/SafeRange.h"
#include "Logs/Log.h"

bool axp_load_sections(const axf_header_t *header, const uint8_t *filebuf,
                       uint64_t read_bytes, bool *entry_point_valid)
{
    *entry_point_valid = false;

    // header->section_count is attacker-controlled (read straight
    // from the file) -- axf_array_fits() checks the section_count *
    // sizeof(axf_section_t) multiplication for overflow before ever
    // trusting it, instead of the raw addition/multiplication this
    // used to do directly (which could wrap to a small value for a
    // large enough section_count, passing this check while actually
    // pointing past the file).
    if (!axf_array_fits(header->section_table_offset, header->section_count, sizeof(axf_section_t), read_bytes))
    {
        klog_error("axf_load: section table extends beyond file");
        return false;
    }

    // Prepares the code page (same approach as `ring3`: a single
    // page, zero-filled, sections copied into it).
    void *code_phys = pmm_alloc_page();
    if (!code_phys)
    {
        klog_error("axf_load: out of physical memory");
        return false;
    }
    vmm_map(USER_CODE_ADDR, (uint64_t)code_phys, VMM_USER_RW);

    uint8_t *dst_page = (uint8_t *)USER_CODE_ADDR;
    for (uint32_t i = 0; i < 4096; i++)
        dst_page[i] = 0;

    uint64_t entry_point = header->entry_point;

    for (uint32_t s = 0; s < header->section_count; s++)
    {
        axf_section_t sec;
        uint64_t sec_offset = header->section_table_offset + s * sizeof(axf_section_t);
        // Per-entry check before reading, on top of the aggregate
        // axf_array_fits() check above -- see HeaderValidation.c's
        // identical addition for why relying on the aggregate check
        // alone isn't enough by itself.
        if (!axf_range_fits(sec_offset, sizeof(sec), read_bytes))
        {
            klog_error("axf_load: section table entry extends beyond file");
            return false;
        }
        const uint8_t *sb = filebuf + sec_offset;
        uint8_t *dst = (uint8_t *)&sec;
        for (uint32_t i = 0; i < sizeof(sec); i++)
            dst[i] = sb[i];

        // V1: every section must fit inside the single page allocated
        // above, within [USER_CODE_ADDR, USER_CODE_ADDR+4096). No
        // multi-page support yet. Checked via axf_range_fits() rather
        // than the raw `sec.vaddr + sec.mem_size > ...` comparison --
        // sec.mem_size is attacker-controlled and a large enough value
        // could otherwise wrap the addition past UINT64_MAX back into
        // a small, seemingly in-range address.
        if (sec.vaddr < USER_CODE_ADDR || !axf_range_fits(sec.vaddr - USER_CODE_ADDR, sec.mem_size, 4096))
        {
            klog_error("axf_load: section out of supported range (v1: single 4KB page)");
            return false;
        }

        uint64_t page_offset = sec.vaddr - USER_CODE_ADDR;

        if (sec.file_size > 0)
        {
            // sec.file_offset/file_size are attacker-controlled --
            // same overflow hazard as the section table check above.
            if (!axf_range_fits(sec.file_offset, sec.file_size, read_bytes))
            {
                klog_error("axf_load: section data extends beyond file");
                return false;
            }
            for (uint64_t i = 0; i < sec.file_size; i++)
                dst_page[page_offset + i] = filebuf[sec.file_offset + i];
        }
        // The rest of mem_size beyond file_size (typically .bss, or
        // padding) stays zero -- already handled by zero-filling the
        // whole page above.

        // Same overflow hazard as the range checks above: sec.vaddr +
        // sec.mem_size could wrap for an attacker-chosen mem_size, but
        // by this point axf_range_fits() above already proved
        // sec.vaddr - USER_CODE_ADDR + sec.mem_size <= 4096 without
        // overflowing, so sec.vaddr + sec.mem_size is now known safe
        // to compute directly.
        if (entry_point >= sec.vaddr && entry_point < sec.vaddr + sec.mem_size)
            *entry_point_valid = true;
    }

    if (!*entry_point_valid)
    {
        klog_error("axf_load: entry_point is not inside any loaded section");
        return false;
    }

    return true;
}
