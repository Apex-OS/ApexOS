// ApexKernel/Includes/Arch/x86_64/MTRR.h
// ApexOS -- Memory Type Range Registers (variable-range MTRR setup)
//
// Used to mark the VBE linear framebuffer's physical address range as
// Write-Combining (WC) instead of the default Write-Back/Uncacheable
// type the BIOS/firmware leaves it in. WC lets the CPU buffer and
// coalesce consecutive writes to VRAM into larger burst transfers
// instead of individual bus cycles per write -- framebuffer writes are
// almost always sequential (fb_present()/fb_scroll_up() walk the
// buffer linearly) and never read back before the next write, which
// is exactly the access pattern WC is designed for.

#ifndef APEX_ARCH_X86_64_MTRR_H
#define APEX_ARCH_X86_64_MTRR_H

#include "Core/Types.h"

// IA32_MTRR_DEF_TYPE.Type field encoding (Intel SDM Vol 3, Table
// 11-8) -- also the encoding used in each PhysBase's low 8 bits.
#define MTRR_TYPE_UC 0x00 // Uncacheable
#define MTRR_TYPE_WC 0x01 // Write-Combining
#define MTRR_TYPE_WT 0x04 // Write-Through
#define MTRR_TYPE_WP 0x05 // Write-Protect
#define MTRR_TYPE_WB 0x06 // Write-Back

// Marks the physical range [phys_base, phys_base + size) as the given
// MTRR type, using one variable-range MTRR pair. size is rounded up
// internally to the next power of two (variable MTRR ranges must be a
// power of two in size and naturally aligned -- see
// mtrr_set_range()'s comment for why). Returns false without writing
// any MSR if the CPU doesn't support MTRRs, doesn't support the
// requested type (checked against IA32_MTRRCAP for WC specifically),
// has no free variable-range register pairs, or if phys_base/size
// can't be expressed as a single aligned power-of-two range small
// enough to fit the variable MTRRs' mask width. Safe to call at
// runtime (not just at boot) -- follows the Intel-recommended
// programming sequence (disable caching, flush, write, flush,
// re-enable) so it doesn't corrupt already-cached data.
bool mtrr_set_range(uint64_t phys_base, uint64_t size, uint8_t type);

#endif // APEX_ARCH_X86_64_MTRR_H
