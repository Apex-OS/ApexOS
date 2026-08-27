// ApexKernel/Includes/Filesystem/VFS/FilesystemRouter.h
// ApexOS -- VFS path resolution routing (dispatches to the concrete filesystem)

#ifndef APEX_FILESYSTEM_VFS_FILESYSTEM_ROUTER_H
#define APEX_FILESYSTEM_VFS_FILESYSTEM_ROUTER_H

#include "Filesystem/AXFS/AXFS.h"

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

// Returns the axfs_volume_t backing the volume named in `path`
// ("/@SysDisk/..." -> the SysDisk volume), or NULL if that volume
// doesn't exist or isn't an AXFS volume. Callers that need to call an
// axfs_* function directly (axfs_read/axfs_write/axfs_list_dir/etc --
// anything vfs_resolve()/vfs_create() don't already wrap) need this to
// get the axfs_volume_t* those functions require.
axfs_volume_t *vfs_get_volume(const char *path);

#endif // APEX_FILESYSTEM_VFS_FILESYSTEM_ROUTER_H
