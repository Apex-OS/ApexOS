// ApexKernel/Filesystem/AXFS/Allocation/DataBlockAllocation.c
// ApexOS -- AXFS data block allocation/free

#include "Filesystem/AXFS/Allocation/AllocationInternal.h"

uint64_t alloc_data_block(axfs_volume_t *vol, uint32_t transaction_id)
{
    uint64_t data_blocks = vol->sb.total_blocks - vol->sb.data_start_block;

    for (uint64_t i = 0; i < data_blocks; i++)
    {
        if (!bitmap_test_bit(vol, vol->sb.block_bitmap_block, i))
        {
            bitmap_set_bit(vol, transaction_id, vol->sb.block_bitmap_block, i, true);
            vol->sb.free_blocks--;
            write_superblock(vol, transaction_id);
            return vol->sb.data_start_block + i;
        }
    }

    return 0; // 0 is never a valid data block number (block 0 is the superblock)
}

void free_data_block(axfs_volume_t *vol, uint32_t transaction_id, uint64_t block_num)
{
    if (block_num < vol->sb.data_start_block)
        return; // never free the superblock/bitmaps/inode table by mistake

    uint64_t bit_index = block_num - vol->sb.data_start_block;
    bitmap_set_bit(vol, transaction_id, vol->sb.block_bitmap_block, bit_index, false);
    vol->sb.free_blocks++;
    write_superblock(vol, transaction_id);
}
