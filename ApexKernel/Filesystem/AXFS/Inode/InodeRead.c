// ApexKernel/Filesystem/AXFS/Inode/InodeRead.c
// ApexOS -- AXFS inode read from the on-disk inode table

#include "Filesystem/AXFS/Internal.h"

bool read_inode(axfs_volume_t *vol, uint64_t inode_num, axfs_inode_t *out)
{
    if (inode_num == 0 || inode_num > vol->sb.total_inodes)
        return false;

    uint64_t index = inode_num - 1; // inode_num is 1-based
    uint64_t block = vol->sb.inode_table_block + (index / AXFS_INODES_PER_BLOCK);
    uint64_t offset_in_block = (index % AXFS_INODES_PER_BLOCK) * AXFS_INODE_SIZE;

    uint8_t buf[AXFS_BLOCK_SIZE];
    if (!axfs_disk_read_block(&vol->disk, block, buf))
        return false;

    axfs_inode_t *inode = (axfs_inode_t *)(buf + offset_in_block);
    if (inode->type == AXFS_TYPE_FREE)
        return false;

    *out = *inode;
    return true;
}
