// ApexKernel/Includes/Formats/AXF/AXP/SectionLoader.h
// ApexOS -- AXP section mapping (loads .text/.data/.bss into the process's code page)

#ifndef APEX_FORMATS_AXF_AXP_SECTION_LOADER_H
#define APEX_FORMATS_AXF_AXP_SECTION_LOADER_H

#include "Formats/AXF/AXFFormat.h"
#include "Core/Types.h"

// Allocates and maps the .axp's single code page at USER_CODE_ADDR,
// then copies every section from `filebuf` into it. Returns true and
// sets `entry_point_valid` if header->entry_point falls inside one of
// the loaded sections -- false on any failure (out of memory, section
// out of range, entry point not covered by any section).
bool axp_load_sections(const axf_header_t *header, const uint8_t *filebuf,
                       uint64_t read_bytes, bool *entry_point_valid);

#endif // APEX_FORMATS_AXF_AXP_SECTION_LOADER_H
