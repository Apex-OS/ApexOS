// ApexKernel/Includes/Formats/AXF/SafeRange/SafeRange.h
// ApexOS -- Overflow-safe bounds checking for AXF file parsing
//
// Every AXF loader (Formats/AXF/AXP/, AXF/AXL/, AXF/AXD/) repeats the
// same bounds-check pattern before trusting an offset/count read from
// the file itself:
//
//   if (header->section_table_offset + header->section_count * sizeof(axf_section_t) > read_bytes)
//       return false; // "extends beyond file"
//
// Every field on the left (offset, count) is attacker-controlled --
// it comes straight from the .axp/.axl/.axd file. For a large enough
// count or offset, the addition or multiplication above silently
// wraps around a uint64_t, producing a small value that passes the
// check even though the real range extends far past the file --
// letting a later loop read (or, worse, copy into a freshly mapped
// page) memory beyond the actual file buffer.
#ifndef APEX_FORMATS_AXF_SAFE_RANGE_H
#define APEX_FORMATS_AXF_SAFE_RANGE_H

#include "Core/Types.h"

// True if a file region of `size` bytes at `offset` fits entirely
// within a buffer of `limit` bytes, with no integer overflow along
// the way. Use this instead of the raw `offset + size > limit`
// pattern anywhere size/offset come from the file being parsed.
static inline bool axf_range_fits(uint64_t offset, uint64_t size, uint64_t limit)
{
    if (size > limit)
        return false;
    return offset <= limit - size;
}

// True if `count * elem_size` (an array of `count` elements read from
// the file, each `elem_size` bytes, e.g. header->section_count *
// sizeof(axf_section_t)) does not overflow a uint64_t, AND the
// resulting byte range at `offset` fits within `limit` bytes.
// Checks the multiplication as a division (count > limit / elem_size)
// rather than computing count * elem_size directly and comparing
// after the fact -- by the time an overflowing multiplication has
// already happened, the wrapped result can no longer be trusted to
// reveal that it overflowed.
static inline bool axf_array_fits(uint64_t offset, uint64_t count, uint64_t elem_size, uint64_t limit)
{
    if (elem_size == 0)
        return offset <= limit; // a zero-size element array reads nothing
    if (count > limit / elem_size)
        return false;
    return axf_range_fits(offset, count * elem_size, limit);
}

#endif // APEX_FORMATS_AXF_SAFE_RANGE_H
