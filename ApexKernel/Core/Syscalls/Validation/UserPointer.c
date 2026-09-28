// ApexKernel/Core/Syscalls/Validation/UserPointer.c
// ApexOS -- Userspace pointer/length validation (see UserPointer.h)

#include "Core/Syscalls/Validation/UserPointer.h"
#include "Core/Userspace.h"
#include "Formats/AXF/AXFFormat.h"

#define USER_CODE_PAGE_SIZE 4096ull    // 1 page -- see Core/Userspace.h's USER_CODE_ADDR comment
#define USER_LIB_PAGE_SIZE 4096ull     // 1 page -- see Formats/AXF/AXL/LibraryLoader.c's vmm_map() call
#define IMPORT_TABLE_PAGE_SIZE 4096ull // 1 page -- see AXFFormat.h's AXF_IMPORT_TABLE_ADDR comment

// True if [addr, addr+len) falls entirely within [region_start,
// region_start+region_size). Assumes addr+len has already been
// checked not to overflow (is_valid_user_range() below does this
// once, up front, rather than in every call to this helper).
static bool range_within(uint64_t addr, uint64_t len, uint64_t region_start, uint64_t region_size)
{
    if (addr < region_start)
        return false;
    if (len > region_size)
        return false;
    // addr - region_start and region_size are both known not to
    // overflow here (region_size is one of this file's small fixed
    // constants), so this comparison is safe even though addr itself
    // could be close to UINT64_MAX for an out-of-region address --
    // that case is already excluded by the addr < region_start check
    // above whenever region_start is small, which it always is here.
    return (addr - region_start) <= (region_size - len);
}

bool is_valid_user_range(uint64_t addr, uint64_t len)
{
    if (len == 0)
        return true; // an empty range trivially can't read/write anything

    // Rejects the overflow case up front: a malicious addr near
    // UINT64_MAX combined with a nonzero len would otherwise wrap
    // addr+len back into a small, seemingly-valid-looking value in
    // every check below.
    if (addr + len < addr)
        return false;

    if (range_within(addr, len, USER_CODE_ADDR, USER_CODE_PAGE_SIZE))
        return true;
    if (range_within(addr, len, AXF_IMPORT_TABLE_ADDR, IMPORT_TABLE_PAGE_SIZE))
        return true;
    if (range_within(addr, len, USER_LIB_ADDR, USER_LIB_PAGE_SIZE))
        return true;
    if (range_within(addr, len, USER_STACK_ADDR, USER_STACK_SIZE))
        return true;

    return false;
}
