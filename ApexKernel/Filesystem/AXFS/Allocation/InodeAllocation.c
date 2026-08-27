// ApexKernel/Filesystem/AXFS/Allocation/InodeAllocation.c
// ApexOS -- AXFS inode number allocation

#include "Filesystem/AXFS/Allocation/AllocationInternal.h"

uint64_t alloc_inode_num(axfs_volume_t *vol)
{
    for (uint64_t i = 0; i < vol->sb.total_inodes; i++)
    {
        if (!bitmap_test_bit(vol, vol->sb.inode_bitmap_block, i))
        {
            bitmap_set_bit(vol, vol->sb.inode_bitmap_block, i, true);
            vol->sb.free_inodes--;
            write_superblock(vol);
            return i + 1; // inode numbers are 1-based (0 = "no inode")
        }
    }

    return 0;
}
