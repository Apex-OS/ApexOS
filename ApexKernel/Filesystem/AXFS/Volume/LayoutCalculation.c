// ApexKernel/Filesystem/AXFS/Volume/LayoutCalculation.c
// ApexOS -- AXFS on-disk layout calculation (used only by FormatWrite.c)

#include "Filesystem/AXFS/Volume/LayoutCalculation.h"

#define AXFS_BYTES_PER_INODE 16384u

bool axfs_calculate_layout(const axfs_disk_t *disk, axfs_volume_layout_t *out)
{
    uint64_t total_blocks = axfs_disk_total_blocks(disk);
    if (total_blocks < 16)
        return false;

    uint64_t total_bytes = total_blocks * AXFS_BLOCK_SIZE;
    uint64_t total_inodes = total_bytes / AXFS_BYTES_PER_INODE;
    if (total_inodes < 16)
        total_inodes = 16;

    uint64_t inode_table_blocks = (total_inodes + AXFS_INODES_PER_BLOCK - 1) / AXFS_INODES_PER_BLOCK;

    uint64_t inode_bitmap_bytes = (total_inodes + 7) / 8;
    uint64_t inode_bitmap_blocks = (inode_bitmap_bytes + AXFS_BLOCK_SIZE - 1) / AXFS_BLOCK_SIZE;
    if (inode_bitmap_blocks == 0)
        inode_bitmap_blocks = 1;

    uint64_t block_bitmap_bytes = (total_blocks + 7) / 8;
    uint64_t block_bitmap_blocks = (block_bitmap_bytes + AXFS_BLOCK_SIZE - 1) / AXFS_BLOCK_SIZE;
    if (block_bitmap_blocks == 0)
        block_bitmap_blocks = 1;

    uint64_t journal_start = 1; // right after the superblock (block 0)
    uint64_t journal_blocks = AXFS_JOURNAL_SLOT_COUNT * AXFS_JOURNAL_SLOT_BLOCKS;

    uint64_t inode_bitmap_start = journal_start + journal_blocks;
    uint64_t block_bitmap_start = inode_bitmap_start + inode_bitmap_blocks;
    uint64_t inode_table_start = block_bitmap_start + block_bitmap_blocks;
    uint64_t data_start = inode_table_start + inode_table_blocks;

    if (data_start >= total_blocks)
        return false;

    out->total_blocks = total_blocks;
    out->total_inodes = total_inodes;
    out->journal_start = journal_start;
    out->inode_bitmap_start = inode_bitmap_start;
    out->inode_bitmap_blocks = inode_bitmap_blocks;
    out->block_bitmap_start = block_bitmap_start;
    out->block_bitmap_blocks = block_bitmap_blocks;
    out->inode_table_start = inode_table_start;
    out->inode_table_blocks = inode_table_blocks;
    out->data_start = data_start;

    return true;
}
