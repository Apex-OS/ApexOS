// apex-kernel/fs/vfs.h
// ApexOS -- VFS (Virtual File System)
//
// Abstraction layer above concrete filesystems (AXFS for now,
// potentially FAT32/ISO9660 later for reading non-ApexOS USB/CD/DVD
// disks). Deals in full ABSOLUTE paths, prefixed with a volume name:
//
//   /@SysDisk/Home/Users/Zeemoud/Documents/photo.png
//   ^          ^
//   |          `-- rest of the path, delegated to the volume's driver
//   `-- volume prefix ("@VolumeName"), always first
//
// The leading '/' marks an absolute path (the only form supported for
// now -- no path relative to a "current directory" at the VFS level
// yet; shell.c handles its own navigation, tracking the current
// directory and building absolute paths before calling into the
// VFS).
//
// V1: a single real volume (SysDisk, backed by AXFS). The volume
// table is designed to support several from the start (USB, DataDisk,
// etc. later) without needing a redesign.

#ifndef APEX_VFS_H
#define APEX_VFS_H

#include "../include/apex/types.h"
#include "axfs.h"

#define VFS_MAX_VOLUMES 8
#define VFS_MAX_VOLUME_NAME 32

typedef enum
{
    VFS_FS_NONE = 0,
    VFS_FS_AXFS = 1,
    // VFS_FS_FAT32, VFS_FS_ISO9660... will come later for non-ApexOS
    // removable media.
} vfs_fs_type_t;

// Registers a volume in the VFS table. `name` is what appears after
// the '@' in paths (e.g. "SysDisk"). Only VFS_FS_AXFS is actually
// supported for now -- the underlying AXFS volume must already be
// mounted (axfs_mount()) before this call.
bool vfs_register_volume(const char *name, vfs_fs_type_t type);

// Resolves a full ABSOLUTE path ("/@SysDisk/Home/...") and returns
// the final inode number on the concrete filesystem (AXFS for now),
// or 0 if the volume doesn't exist or the path can't be found.
//
// IMPORTANT: the returned inode number is an identifier SPECIFIC TO
// THE VOLUME'S FILESYSTEM -- not a global VFS identifier. As long as
// only one filesystem type (AXFS) exists this isn't ambiguous, but
// once several types coexist a real vfs_node_t pairing (volume,
// inode) will likely be needed. Note for a V2.
uint64_t vfs_resolve(const char *path);

// Like vfs_resolve, but creates missing directories/file along the
// way (equivalent to axfs_create_path, but with the volume prefix).
uint64_t vfs_create(const char *path, axfs_inode_type_t type);

// Extracts the volume name from an absolute path ("/@SysDisk/Home" ->
// "SysDisk"). Written into `out` (max size out_size). Returns false
// if the path doesn't match the expected format (must start with
// "/@").
bool vfs_parse_volume(const char *path, char *out, uint32_t out_size);

// Returns a pointer to the rest of the path AFTER the volume name
// (i.e. what should be passed to axfs_resolve_path/axfs_create_path).
// E.g. "/@SysDisk/Home/Users" -> pointer to "Home/Users". Returns NULL
// if the path doesn't match the expected format.
const char *vfs_path_after_volume(const char *path);

#endif // APEX_VFS_H