// apex-kernel/fs/axfs.h
// ApexOS -- AXFS (Apex File System) v1
//
// On-disk format inspired by ext2 (allocation bitmap + inode table +
// direct/indirect blocks), simplified for a working V1. No journaling
// yet (planned for v2) -- a power loss during a write can corrupt the
// volume.
//
// Disk layout (in AXFS_BLOCK_SIZE blocks):
//
//   Block 0            : Superblock
//   Block 1..N         : Inode bitmap (1 bit per inode)
//   Block N+1..M       : Data block bitmap (1 bit per block)
//   Block M+1..K       : Inode table (32 inodes per 4KB block)
//   Block K+1..end     : Data area (file/directory contents)
//
// The exact bounds of each zone are computed in axfs_format() and
// stored in the superblock -- nothing is hardcoded on the driver side.

#ifndef APEX_AXFS_H
#define APEX_AXFS_H

#include "../include/apex/types.h"

#define AXFS_MAGIC 0x41585653u // "AXVS" little-endian on disk
#define AXFS_VERSION 1
#define AXFS_BLOCK_SIZE 4096u
#define AXFS_INODE_SIZE 256u
#define AXFS_INODES_PER_BLOCK (AXFS_BLOCK_SIZE / AXFS_INODE_SIZE) // 16

#define AXFS_ROOT_INODE 1 // First inode ever allocated = "/"

#define AXFS_MAX_NAME_LEN 255
#define AXFS_DIRECT_BLOCKS 12

// Number of block pointers that fit in one indirect block (pointer =
// uint64_t = 8 bytes -> 4096/8 = 512 pointers).
#define AXFS_PTRS_PER_BLOCK (AXFS_BLOCK_SIZE / sizeof(uint64_t))

// Max file size with direct + indirect + double-indirect:
//   direct          : 12 * 4096                                = 48 KB
//   indirect        : 512 * 4096                                = 2 MB
//   double-indirect : 512 * 512 * 4096                          = 1 GB
// (plenty for a V1 -- no triple-indirect for now)

typedef enum
{
    AXFS_TYPE_FREE = 0, // Free / unallocated inode
    AXFS_TYPE_FILE = 1,
    AXFS_TYPE_DIR = 2,
} axfs_inode_type_t;

// Superblock (occupies exactly one block).
typedef struct __attribute__((packed))
{
    uint32_t magic;   // AXFS_MAGIC -- checked on mount
    uint32_t version; // AXFS_VERSION
    uint32_t block_size;
    uint64_t total_blocks; // Total volume size in blocks
    uint64_t free_blocks;

    uint64_t inode_bitmap_block; // Start of the inode bitmap
    uint64_t block_bitmap_block; // Start of the data block bitmap
    uint64_t inode_table_block;  // Start of the inode table
    uint64_t data_start_block;   // Start of the data area

    uint64_t total_inodes;
    uint64_t free_inodes;
    uint64_t root_inode; // = AXFS_ROOT_INODE

    char volume_label[32]; // e.g. "SysDisk"

    uint8_t reserved[4096 - 4 - 4 - 4 - 8 - 8 - 8 - 8 - 8 - 8 - 8 - 8 - 8 - 32];
} axfs_superblock_t;

// Inode (256 bytes -- 16 per 4KB block).
typedef struct __attribute__((packed))
{
    uint16_t type;        // axfs_inode_type_t
    uint16_t permissions; // reserved, not enforced in v1 (default 0777)
    uint32_t owner_id;    // reserved, not enforced in v1 (default 0 = admin)
    uint64_t size_bytes;  // actual content size

    uint64_t created_at; // timestamp (PIT ticks for now)
    uint64_t modified_at;
    uint32_t links_count; // reserved for future hardlinks (default 1)
    uint32_t _pad0;

    uint64_t direct_blocks[AXFS_DIRECT_BLOCKS];
    uint64_t indirect_block;        // 0 = unused
    uint64_t double_indirect_block; // 0 = unused

    // 104 bytes of headroom for future extensions (ACLs, real
    // hardlinks, triple-indirect, etc.) without ever having to change
    // the on-disk inode size (which would break every existing
    // volume).
    uint8_t reserved[256 - 2 - 2 - 4 - 8 - 8 - 8 - 4 - 4 - (8 * AXFS_DIRECT_BLOCKS) - 8 - 8];
} axfs_inode_t;

// Directory entry, stored in the data blocks of an AXFS_TYPE_DIR
// inode. Variable size (entry_size) -- avoids wasting space on short
// names while still supporting up to 255 characters.
typedef struct __attribute__((packed))
{
    uint64_t inode_num;  // 0 = free/deleted entry (tombstone)
    uint16_t entry_size; // total size of this entry in bytes (for sequential scans)
    uint8_t name_len;    // actual name length (excludes the trailing \0)
    uint8_t file_type;   // duplicates axfs_inode_type_t to avoid an inode read
    char name[AXFS_MAX_NAME_LEN];
} axfs_dirent_t;

// Minimum real size of an entry (header without the full name) --
// used to compute entry_size = AXFS_DIRENT_HEADER_SIZE + name_len.
#define AXFS_DIRENT_HEADER_SIZE (8 + 2 + 1 + 1)

// ── Public driver API (see axfs.c) ────────────────────────────────────────
// NOTE: this V1 operates directly on an in-memory buffer standing in
// for a disk (see axfs_disk.h) -- wiring up a real ATA/AHCI disk comes
// in a later step, once the filesystem logic itself is validated.

// Initializes the underlying disk layer (RAM disk for now). Call once
// at startup, before axfs_format()/axfs_mount().
bool axfs_init(void);

// Formats a blank volume: writes the superblock, bitmaps, and root
// inode.
bool axfs_format(const char *volume_label);

// Mounts the volume (checks the magic, loads the superblock into
// memory).
bool axfs_mount(void);

// Creates an empty file or directory inside `parent_inode`. Returns
// the new inode number, or 0 on failure.
uint64_t axfs_create(uint64_t parent_inode, const char *name, axfs_inode_type_t type);

// Writes data to a file (overwrites existing content from the given
// offset, allocating blocks as needed).
bool axfs_write(uint64_t inode_num, const void *data, uint64_t size, uint64_t offset);

// Reads data from a file.
uint64_t axfs_read(uint64_t inode_num, void *buf, uint64_t size, uint64_t offset);

// Looks up an entry by name in a directory. Returns the inode number,
// or 0 if not found.
uint64_t axfs_lookup(uint64_t dir_inode, const char *name);

// ── Directory listing ──────────────────────────────────────────────────
// Simplified view of a directory entry, for the public API (only what
// an external caller needs -- not the raw on-disk entry).
typedef struct
{
    uint64_t inode_num;
    char name[AXFS_MAX_NAME_LEN + 1]; // +1 for the trailing '\0'
    uint8_t type;                     // axfs_inode_type_t
} axfs_dirent_info_t;

// Lists a directory's entries into `out` (up to `max_entries`).
// Returns the number of entries actually written to `out` (includes
// "." and ".." -- the caller filters those out if needed).
uint32_t axfs_list_dir(uint64_t dir_inode, axfs_dirent_info_t *out, uint32_t max_entries);

// Returns an inode's type (AXFS_TYPE_FILE/AXFS_TYPE_DIR), or
// AXFS_TYPE_FREE (0) if the inode doesn't exist / isn't allocated.
axfs_inode_type_t axfs_get_type(uint64_t inode_num);

// Removes an entry from a directory (doesn't necessarily free the
// inode if links_count > 1 -- for now links_count is always 1, so
// this is equivalent to a full delete).
bool axfs_unlink(uint64_t dir_inode, const char *name);

// ── Nested path resolution ────────────────────────────────────────────
// An "AXFS path" here is relative to a starting inode (typically
// AXFS_ROOT_INODE), without the volume prefix ("@SysDisk" etc.) --
// that prefix is handled by the VFS layer above (see vfs.h), not
// here. Expected format: segments separated by '/', e.g.
// "Home/Users/Zeemoud". An empty path or "." returns start_inode
// unchanged.

// Walks the path from start_inode and returns the final inode, or 0
// if an intermediate segment doesn't exist or isn't a directory.
uint64_t axfs_resolve_path(uint64_t start_inode, const char *path);

// Like axfs_resolve_path, but CREATES missing intermediate
// directories along the way (like "mkdir -p"). The last segment is
// created with the requested type (`final_type`) -- useful for
// building a full file or directory path in one call. Returns the
// final inode (existing or newly created), or 0 on failure (e.g. an
// intermediate segment already exists but isn't a directory).
uint64_t axfs_create_path(uint64_t start_inode, const char *path, axfs_inode_type_t final_type);

// Creates the standard ApexOS directory structure (Home/Admin, System/
// SysCore/... -- see the full list in axfs_tree.c) on the currently
// mounted volume. Call once, right after axfs_format().
bool axfs_create_default_tree(void);

#endif // APEX_AXFS_H