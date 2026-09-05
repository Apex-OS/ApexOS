// ApexKernel/Includes/Filesystem/AXFS/AXFS.h
// ApexOS -- AXFS (Apex File System) v1
//
// On-disk format inspired by ext2 (allocation bitmap + inode table +
// direct/indirect blocks), simplified for a working V1. Full write-
// ahead journaling (metadata AND file content) protects every write
// against a crash/power loss mid-operation -- see Journal/Journal.h
// for the format and recovery model.
//
// Disk layout (in AXFS_BLOCK_SIZE blocks):
//
//   Block 0            : Superblock
//   Block 1..J         : Journal (AXFS_JOURNAL_SLOT_COUNT slots, see Journal/Journal.h)
//   Block J+1..N       : Inode bitmap (1 bit per inode)
//   Block N+1..M       : Data block bitmap (1 bit per block)
//   Block M+1..K       : Inode table (32 inodes per 4KB block)
//   Block K+1..end     : Data area (file/directory contents)
//
// The exact bounds of each zone are computed in Volume/FormatWrite.c
// and stored in the superblock -- nothing is hardcoded on the driver
// side.
//
// Multi-volume: every function below takes an axfs_volume_t* as its
// first argument -- there is no hidden global "the mounted volume"
// anymore. A volume owns its own superblock (cached in RAM after
// mount) and its own disk backend (RAM or AHCI, see
// Disk/DiskRouter.h). Callers manage the axfs_volume_t's lifetime and
// storage (typically a static instance per volume, e.g. one for
// @SysDisk) -- nothing in this module allocates one itself.

#ifndef APEX_FILESYSTEM_AXFS_H
#define APEX_FILESYSTEM_AXFS_H

#include "Core/Types.h"
#include "Filesystem/AXFS/Disk/DiskRouter.h"
#include "Filesystem/AXFS/Journal/Journal.h"

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

    uint64_t journal_start_block; // Start of the write-ahead journal region
                                  // (AXFS_JOURNAL_SLOT_COUNT * AXFS_JOURNAL_SLOT_BLOCKS
                                  // blocks -- see Journal/Journal.h). Sits right after
                                  // the superblock, before the inode bitmap (see this
                                  // file's disk layout comment).

    char volume_label[32]; // e.g. "SysDisk"

    uint8_t reserved[4096 - 4 - 4 - 4 - 8 - 8 - 8 - 8 - 8 - 8 - 8 - 8 - 8 - 8 - 32];
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

// ── Volume context ─────────────────────────────────────────────────────
// One mounted (or mountable) AXFS volume. mounted=false means
// disk/superblock aren't valid yet -- Volume/FormatWrite.c's
// axfs_format() / Volume/Mount.c's axfs_mount() are the only calls
// safe before that.
struct axfs_volume_s
{
    axfs_disk_t disk;
    axfs_superblock_t sb;
    bool mounted;
    char label[32]; // matches sb.volume_label once mounted; set before that by whoever inits the volume

    // Journal cursor (see Journal/Journal.h) -- in-memory only, never
    // persisted. Set by axfs_journal_init() (fresh format) or
    // axfs_journal_recover() (existing volume, run during
    // axfs_mount()); every axfs_disk_write_block_journaled() call
    // advances journal_next_slot and axfs_journal_begin_transaction()
    // advances journal_next_transaction_id.
    uint32_t journal_next_slot;
    uint32_t journal_next_transaction_id;
};
typedef struct axfs_volume_s axfs_volume_t;

// ── Public driver API ───────────────────────────────────────────────────
// Every function takes the volume to operate on explicitly -- see the
// axfs_volume_t comment above. `vol` must have a disk backend already
// set up (axfs_disk_init_ram()/axfs_disk_init_ahci(), see
// Disk/DiskRouter.h) before calling axfs_format() or axfs_mount() on
// it.

// Formats a blank volume: writes the superblock, bitmaps, and root
// inode. `vol->disk` must already be initialized. (Volume/FormatWrite.c)
bool axfs_format(axfs_volume_t *vol, const char *volume_label);

// Mounts the volume (checks the magic, loads the superblock into
// memory). `vol->disk` must already be initialized. (Volume/Mount.c)
bool axfs_mount(axfs_volume_t *vol);

// Creates an empty file or directory inside `parent_inode`. Returns
// the new inode number, or 0 on failure. (Entry/InodeCreation.c +
// Entry/DirectoryInsertion.c)
uint64_t axfs_create(axfs_volume_t *vol, uint64_t parent_inode, const char *name, axfs_inode_type_t type);

// Writes data to a file (overwrites existing content from the given
// offset, allocating blocks as needed). (Content/FileWrite.c)
bool axfs_write(axfs_volume_t *vol, uint64_t inode_num, const void *data, uint64_t size, uint64_t offset);

// Reads data from a file. (Content/FileRead.c)
uint64_t axfs_read(axfs_volume_t *vol, uint64_t inode_num, void *buf, uint64_t size, uint64_t offset);

// Looks up an entry by name in a directory. Returns the inode number,
// or 0 if not found. (Directory/Lookup.c)
uint64_t axfs_lookup(axfs_volume_t *vol, uint64_t dir_inode, const char *name);

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
// (Directory/Listing.c)
uint32_t axfs_list_dir(axfs_volume_t *vol, uint64_t dir_inode, axfs_dirent_info_t *out, uint32_t max_entries);

// Returns an inode's type (AXFS_TYPE_FILE/AXFS_TYPE_DIR), or
// AXFS_TYPE_FREE (0) if the inode doesn't exist / isn't allocated.
// (Directory/TypeQuery.c)
axfs_inode_type_t axfs_get_type(axfs_volume_t *vol, uint64_t inode_num);

// Removes an entry from a directory (doesn't necessarily free the
// inode if links_count > 1 -- for now links_count is always 1, so
// this is equivalent to a full delete). (Entry/Unlink.c)
bool axfs_unlink(axfs_volume_t *vol, uint64_t dir_inode, const char *name);

// ── Nested path resolution ────────────────────────────────────────────
// An "AXFS path" here is relative to a starting inode (typically
// AXFS_ROOT_INODE), without the volume prefix ("@SysDisk" etc.) --
// that prefix is handled by the VFS layer above (see
// Filesystem/VFS/), not here. Expected format: segments separated by
// '/', e.g. "Home/Users/Zeemoud". An empty path or "." returns
// start_inode unchanged.

// Walks the path from start_inode and returns the final inode, or 0
// if an intermediate segment doesn't exist or isn't a directory.
// (Path/PathResolve.c)
uint64_t axfs_resolve_path(axfs_volume_t *vol, uint64_t start_inode, const char *path);

// Like axfs_resolve_path, but CREATES missing intermediate
// directories along the way (like "mkdir -p"). The last segment is
// created with the requested type (`final_type`) -- useful for
// building a full file or directory path in one call. Returns the
// final inode (existing or newly created), or 0 on failure (e.g. an
// intermediate segment already exists but isn't a directory).
// (Path/PathCreate.c)
uint64_t axfs_create_path(axfs_volume_t *vol, uint64_t start_inode, const char *path, axfs_inode_type_t final_type);

// Creates the standard ApexOS directory structure (Home/Admin, System/
// SysCore/... -- see the full list in Tree/DefaultTreeData.c) on
// `vol`. Call once, right after axfs_format() on a fresh volume --
// calling this on a volume that already has a populated tree (e.g.
// remounting a persistent disk on a later boot) will fail on every
// already-existing entry's axfs_create() but is otherwise harmless;
// callers that persist across boots should check for an existing tree
// first (see Core/Startup/SysDiskSetup.c) rather than calling this
// unconditionally. (Tree/DefaultTreeBuilder.c)
bool axfs_create_default_tree(axfs_volume_t *vol);

#endif // APEX_FILESYSTEM_AXFS_H
