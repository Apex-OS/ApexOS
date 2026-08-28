// ApexKernel/Includes/Filesystem/AXFS/Internal.h
// ApexOS -- AXFS private state and helpers shared between AXFS files
//
// NOT part of the public API (see AXFS.h for that) -- included only
// by the AXFS implementation files themselves. Lets low-level
// block/inode helpers be shared across Allocation/, Inode/,
// Directory/, Entry/, Path/, and Tree/ without duplicating them.
//
// Multi-volume: there is no mounted-volume global state here (no
// g_sb/g_mounted) -- every helper below takes the axfs_volume_t it
// operates on explicitly, same as the public API in AXFS.h. See
// AXFS.h's axfs_volume_t comment for the volume lifetime model.

#ifndef APEX_FILESYSTEM_AXFS_INTERNAL_H
#define APEX_FILESYSTEM_AXFS_INTERNAL_H

#include "Core/Types.h"

#include "Filesystem/AXFS/AXFS.h"
#include "Filesystem/AXFS/Disk/DiskRouter.h"

// ── Bitmap helpers (Allocation/) ────────────────────────────────────────
// bitmap_set_bit() is exposed here (rather than kept static) because
// several files use it directly: marking the root inode used at
// format time, and rolling back an inode/data block on axfs_create()
// failure or axfs_unlink(). Prefer alloc_data_block()/
// alloc_inode_num()/free_data_block() where possible -- they also keep
// vol->sb.free_blocks/free_inodes in sync, which a raw bitmap_set_bit()
// call does not.
void bitmap_set_bit(axfs_volume_t *vol, uint64_t bitmap_start_block, uint64_t bit_index, bool value);
uint64_t alloc_data_block(axfs_volume_t *vol);
void free_data_block(axfs_volume_t *vol, uint64_t block_num);
uint64_t alloc_inode_num(axfs_volume_t *vol);

// ── Inode I/O and directory lookup (Inode/, Directory/) ─────────────────
bool read_inode(axfs_volume_t *vol, uint64_t inode_num, axfs_inode_t *out);
bool write_inode(axfs_volume_t *vol, uint64_t inode_num, const axfs_inode_t *in);
uint64_t dir_find_entry(axfs_volume_t *vol, const axfs_inode_t *dir, const char *name,
                        uint64_t *out_block, uint32_t *out_offset);

// ── String helpers (no libc in freestanding) ──────────────────────────────
// TODO: duplicated across several modules (Filesystem/VFS/,
// Formats/AXF/) -- candidate for centralizing in ApexLib/String/ once
// that's populated.
uint32_t str_len(const char *s);
bool str_eq_n(const char *a, const char *b, uint32_t len);
void str_copy_n(char *dst, const char *src, uint32_t len);

#endif // APEX_FILESYSTEM_AXFS_INTERNAL_H
