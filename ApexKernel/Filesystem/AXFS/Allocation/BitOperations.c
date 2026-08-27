// ApexKernel/Filesystem/AXFS/Allocation/BitOperations.c
// ApexOS -- AXFS raw bitmap bit set operation

#include "Filesystem/AXFS/Internal.h"

void bitmap_set_bit(axfs_volume_t *vol, uint64_t bitmap_start_block, uint64_t bit_index, bool value)
{
    uint64_t block_offset = bit_index / (AXFS_BLOCK_SIZE * 8);
    uint64_t byte_offset = (bit_index / 8) % AXFS_BLOCK_SIZE;
    uint8_t bit_offset = (uint8_t)(bit_index % 8);

    uint8_t block[AXFS_BLOCK_SIZE];
    axfs_disk_read_block(&vol->disk, bitmap_start_block + block_offset, block);

    if (value)
        block[byte_offset] |= (uint8_t)(1u << bit_offset);
    else
        block[byte_offset] &= (uint8_t)~(1u << bit_offset);

    axfs_disk_write_block(&vol->disk, bitmap_start_block + block_offset, block);
}

bool bitmap_test_bit(axfs_volume_t *vol, uint64_t bitmap_start_block, uint64_t bit_index)
{
    uint64_t block_offset = bit_index / (AXFS_BLOCK_SIZE * 8);
    uint64_t byte_offset = (bit_index / 8) % AXFS_BLOCK_SIZE;
    uint8_t bit_offset = (uint8_t)(bit_index % 8);

    uint8_t block[AXFS_BLOCK_SIZE];
    axfs_disk_read_block(&vol->disk, bitmap_start_block + block_offset, block);

    return (block[byte_offset] & (1u << bit_offset)) != 0;
}
