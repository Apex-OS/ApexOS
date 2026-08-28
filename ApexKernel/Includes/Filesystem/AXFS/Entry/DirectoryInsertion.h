// ApexKernel/Includes/Filesystem/AXFS/Entry/DirectoryInsertion.h
// ApexOS -- AXFS directory entry insertion (internal helper for axfs_create)

#ifndef APEX_FILESYSTEM_AXFS_DIRECTORY_INSERTION_H
#define APEX_FILESYSTEM_AXFS_DIRECTORY_INSERTION_H

#include "Core/Types.h"

#include "Filesystem/AXFS/AXFS.h"

// Inserts a new directory entry (name -> inode_num, type) into
// `parent`'s data blocks, allocating a new block if none of the
// existing ones has room. `parent` is read/modified in place (its
// direct_blocks[] may gain a new entry) -- caller is responsible for
// write_inode()'ing it back afterward. Returns false if no room could
// be found/allocated -- caller should treat this as a full rollback
// signal (see axfs_create() in EntryLifecycle.c).
bool axfs_insert_dirent(axfs_volume_t *vol, axfs_inode_t *parent, const char *name,
                        uint32_t name_len, uint64_t inode_num, axfs_inode_type_t type);

#endif // APEX_FILESYSTEM_AXFS_DIRECTORY_INSERTION_H
