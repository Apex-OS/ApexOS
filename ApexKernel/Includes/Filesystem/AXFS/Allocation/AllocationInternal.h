// ApexKernel/Includes/Filesystem/AXFS/Allocation/AllocationInternal.h
// ApexOS -- Shared internal helpers for the Allocation module
//
// bitmap_test_bit() and write_superblock() are defined once in
// BitOperations.c/SuperblockSync.c respectively and shared via extern
// declarations here with DataBlockAllocation.c and InodeAllocation.c.
// Not part of AXFS.h's or Internal.h's public/shared API.

#ifndef APEX_FILESYSTEM_AXFS_ALLOCATION_INTERNAL_H
#define APEX_FILESYSTEM_AXFS_ALLOCATION_INTERNAL_H

#include "Filesystem/AXFS/Internal.h"

bool bitmap_test_bit(axfs_volume_t *vol, uint64_t bitmap_start_block, uint64_t bit_index);

// Serializes vol->sb (the in-RAM cached superblock) back to disk block
// 0. Called after every change to a field that lives in the
// superblock (free_blocks/free_inodes) so the on-disk copy never
// drifts from what's cached in RAM.
void write_superblock(axfs_volume_t *vol);

#endif // APEX_FILESYSTEM_AXFS_ALLOCATION_INTERNAL_H
