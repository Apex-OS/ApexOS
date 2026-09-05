// ApexKernel/Filesystem/AXFS/Entry/InodeCreation.c
// ApexOS -- AXFS new-inode construction (internal helper for axfs_create)

#include "Filesystem/AXFS/Entry/InodeCreation.h"
#include "Filesystem/AXFS/Internal.h"
#include "Core/Time/PITCounter.h"

uint64_t axfs_create_inode(axfs_volume_t *vol, uint32_t transaction_id, uint64_t parent_inode, axfs_inode_type_t type)
{
    uint64_t new_inode_num = alloc_inode_num(vol, transaction_id);
    if (new_inode_num == 0)
        return 0;

    axfs_inode_t new_inode;
    uint8_t *nb = (uint8_t *)&new_inode;
    for (uint32_t i = 0; i < sizeof(new_inode); i++)
        nb[i] = 0;

    new_inode.type = (uint16_t)type;
    new_inode.permissions = 0777;
    new_inode.owner_id = 0;
    new_inode.size_bytes = 0;
    new_inode.created_at = pit_get_ticks();
    new_inode.modified_at = new_inode.created_at;
    new_inode.links_count = 1;

    if (type == AXFS_TYPE_DIR)
    {
        uint64_t dir_block = alloc_data_block(vol, transaction_id);
        if (dir_block == 0)
        {
            bitmap_set_bit(vol, transaction_id, vol->sb.inode_bitmap_block, new_inode_num - 1, false);
            vol->sb.free_inodes++;
            return 0;
        }
        new_inode.direct_blocks[0] = dir_block;

        uint8_t dir_buf[AXFS_BLOCK_SIZE];
        for (uint32_t i = 0; i < AXFS_BLOCK_SIZE; i++)
            dir_buf[i] = 0;

        uint32_t off = 0;
        axfs_dirent_t *dot = (axfs_dirent_t *)(dir_buf + off);
        dot->inode_num = new_inode_num;
        dot->name_len = 1;
        dot->file_type = AXFS_TYPE_DIR;
        dot->name[0] = '.';
        dot->entry_size = AXFS_DIRENT_HEADER_SIZE + dot->name_len;
        off += dot->entry_size;

        axfs_dirent_t *dotdot = (axfs_dirent_t *)(dir_buf + off);
        dotdot->inode_num = parent_inode;
        dotdot->name_len = 2;
        dotdot->file_type = AXFS_TYPE_DIR;
        dotdot->name[0] = '.';
        dotdot->name[1] = '.';
        dotdot->entry_size = AXFS_DIRENT_HEADER_SIZE + dotdot->name_len;
        off += dotdot->entry_size;

        axfs_disk_write_block_journaled(vol, transaction_id, dir_block, dir_buf);
        new_inode.size_bytes = off;
    }

    write_inode(vol, transaction_id, new_inode_num, &new_inode);

    return new_inode_num;
}
