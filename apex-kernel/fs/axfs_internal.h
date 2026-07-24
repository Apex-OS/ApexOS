// apex-kernel/fs/axfs_internal.h
// ApexOS -- AXFS private state and helpers shared between axfs_*.c files
//
// NOT part of the public API (see axfs.h for that) -- included only by
// the AXFS implementation files themselves. Split out of axfs.c during
// the 0.1.0 refactor to let the mounted-volume state and low-level
// block/inode helpers be shared across axfs_bitmap.c, axfs_inode.c,
// axfs_path.c, axfs_tree.c, and axfs.c without duplicating them.

#ifndef APEX_AXFS_INTERNAL_H
#define APEX_AXFS_INTERNAL_H

#include "axfs.h"
#include "axfs_disk.h"

// ── Mounted volume state ──────────────────────────────────────────────────
// Defined in axfs.c, read/written by every axfs_*.c file.
extern axfs_superblock_t g_sb;
extern bool g_mounted;

// ── Bitmap helpers (axfs_bitmap.c) ────────────────────────────────────────
// bitmap_set_bit() is exposed here (rather than kept static) because
// axfs.c still uses it directly in three places: marking the root
// inode used at format time, and rolling back an inode/data block on
// axfs_create() failure or axfs_unlink(). Prefer alloc_data_block()/
// alloc_inode_num()/free_data_block() where possible -- they also keep
// g_sb.free_blocks/free_inodes in sync, which a raw bitmap_set_bit()
// call does not.
void bitmap_set_bit(uint64_t bitmap_start_block, uint64_t bit_index, bool value);
uint64_t alloc_data_block(void);
void free_data_block(uint64_t block_num);
uint64_t alloc_inode_num(void);

// ── Inode I/O and directory lookup (axfs_inode.c) ─────────────────────────
bool read_inode(uint64_t inode_num, axfs_inode_t *out);
bool write_inode(uint64_t inode_num, const axfs_inode_t *in);
uint64_t dir_find_entry(const axfs_inode_t *dir, const char *name,
                        uint64_t *out_block, uint32_t *out_offset);

// ── String helpers (no libc in freestanding) ──────────────────────────────
uint32_t str_len(const char *s);
bool str_eq_n(const char *a, const char *b, uint32_t len);
void str_copy_n(char *dst, const char *src, uint32_t len);

#endif // APEX_AXFS_INTERNAL_H