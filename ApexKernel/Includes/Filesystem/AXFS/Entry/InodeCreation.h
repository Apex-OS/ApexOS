// ApexKernel/Includes/Filesystem/AXFS/Entry/InodeCreation.h
// ApexOS -- AXFS new-inode construction (internal helper for axfs_create)

#ifndef APEX_FILESYSTEM_AXFS_INODE_CREATION_H
#define APEX_FILESYSTEM_AXFS_INODE_CREATION_H

#include "Core/Types.h"

#include "Filesystem/AXFS/AXFS.h"

// Allocates a new inode number and builds the inode's initial content
// (for a directory: allocates its data block and writes "."/".."
// entries). Writes the new inode to disk. Returns the new inode
// number on success, or 0 on failure (out of inodes/blocks) -- on
// failure, any partial allocation this call made is already rolled
// back before returning. `parent_inode` is only needed for a
// directory's ".." entry.
uint64_t axfs_create_inode(axfs_volume_t *vol, uint64_t parent_inode, axfs_inode_type_t type);

#endif // APEX_FILESYSTEM_AXFS_INODE_CREATION_H
