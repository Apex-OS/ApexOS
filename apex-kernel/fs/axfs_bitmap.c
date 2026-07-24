// apex-kernel/fs/axfs_bitmap.c
// ApexOS -- AXFS bitmap-based block and inode allocation

#include "axfs_internal.h"

static bool bitmap_test_bit(uint64_t bitmap_start_block, uint64_t bit_index)
{
    uint64_t byte_index = bit_index / 8;
    uint64_t block_offset = byte_index / AXFS_BLOCK_SIZE;
    uint64_t byte_in_block = byte_index % AXFS_BLOCK_SIZE;
    uint8_t bit_in_byte = bit_index % 8;

    uint8_t buf[AXFS_BLOCK_SIZE];
    if (!axfs_disk_read_block(bitmap_start_block + block_offset, buf))
        return false;

    return (buf[byte_in_block] & (1 << bit_in_byte)) != 0;
}

void bitmap_set_bit(uint64_t bitmap_start_block, uint64_t bit_index, bool value)
{
    uint64_t byte_index = bit_index / 8;
    uint64_t block_offset = byte_index / AXFS_BLOCK_SIZE;
    uint64_t byte_in_block = byte_index % AXFS_BLOCK_SIZE;
    uint8_t bit_in_byte = bit_index % 8;

    uint8_t buf[AXFS_BLOCK_SIZE];
    if (!axfs_disk_read_block(bitmap_start_block + block_offset, buf))
        return;

    if (value)
        buf[byte_in_block] |= (1 << bit_in_byte);
    else
        buf[byte_in_block] &= ~(1 << bit_in_byte);

    axfs_disk_write_block(bitmap_start_block + block_offset, buf);
}

static uint64_t bitmap_find_free(uint64_t bitmap_start_block, uint64_t total_items)
{
    for (uint64_t i = 0; i < total_items; i++)
    {
        if (!bitmap_test_bit(bitmap_start_block, i))
            return i;
    }
    return (uint64_t)-1;
}

uint64_t alloc_data_block(void)
{
    uint64_t data_blocks_total = g_sb.total_blocks - g_sb.data_start_block;
    uint64_t free_idx = bitmap_find_free(g_sb.block_bitmap_block, data_blocks_total);
    if (free_idx == (uint64_t)-1)
        return 0;

    bitmap_set_bit(g_sb.block_bitmap_block, free_idx, true);
    g_sb.free_blocks--;

    uint8_t zero_buf[AXFS_BLOCK_SIZE];
    for (uint32_t i = 0; i < AXFS_BLOCK_SIZE; i++)
        zero_buf[i] = 0;
    axfs_disk_write_block(g_sb.data_start_block + free_idx, zero_buf);

    return g_sb.data_start_block + free_idx;
}

void free_data_block(uint64_t block_num)
{
    if (block_num < g_sb.data_start_block)
        return;
    uint64_t idx = block_num - g_sb.data_start_block;
    bitmap_set_bit(g_sb.block_bitmap_block, idx, false);
    g_sb.free_blocks++;
}

uint64_t alloc_inode_num(void)
{
    uint64_t free_idx = bitmap_find_free(g_sb.inode_bitmap_block, g_sb.total_inodes);
    if (free_idx == (uint64_t)-1)
        return 0;

    bitmap_set_bit(g_sb.inode_bitmap_block, free_idx, true);
    g_sb.free_inodes--;

    return free_idx + 1;
}