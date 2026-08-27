// ApexKernel/Filesystem/AXFS/Entry/Unlink.c
// ApexOS -- AXFS entry removal

#include "Filesystem/AXFS/AXFS.h"
#include "Filesystem/AXFS/Internal.h"

bool axfs_unlink(axfs_volume_t *vol, uint64_t dir_inode, const char *name)
{
    if (!vol->mounted)
        return false;

    axfs_inode_t dir;
    if (!read_inode(vol, dir_inode, &dir))
        return false;
    if (dir.type != AXFS_TYPE_DIR)
        return false;

    uint64_t target_block = 0;
    uint32_t target_offset = 0;
    uint64_t target_inode_num = dir_find_entry(vol, &dir, name, &target_block, &target_offset);
    if (target_inode_num == 0)
        return false;

    uint32_t name_len = str_len(name);
    if ((name_len == 1 && name[0] == '.') ||
        (name_len == 2 && name[0] == '.' && name[1] == '.'))
        return false;

    axfs_inode_t target;
    if (!read_inode(vol, target_inode_num, &target))
        return false;

    if (target.type == AXFS_TYPE_DIR && target.size_bytes > AXFS_DIRENT_HEADER_SIZE * 2 + 3)
        return false; // directory not empty (only "." and ".." left)

    uint8_t buf[AXFS_BLOCK_SIZE];
    if (!axfs_disk_read_block(&vol->disk, target_block, buf))
        return false;
    axfs_dirent_t *ent = (axfs_dirent_t *)(buf + target_offset);
    ent->inode_num = 0; // tombstone
    axfs_disk_write_block(&vol->disk, target_block, buf);

    for (int b = 0; b < AXFS_DIRECT_BLOCKS; b++)
    {
        if (target.direct_blocks[b] != 0)
            free_data_block(vol, target.direct_blocks[b]);
    }

    bitmap_set_bit(vol, vol->sb.inode_bitmap_block, target_inode_num - 1, false);
    vol->sb.free_inodes++;

    axfs_disk_write_block(&vol->disk, 0, &vol->sb);

    return true;
}
