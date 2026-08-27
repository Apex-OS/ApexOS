// ApexKernel/Filesystem/AXFS/Directory/Listing.c
// ApexOS -- AXFS directory listing

#include "Filesystem/AXFS/AXFS.h"
#include "Filesystem/AXFS/Internal.h"

uint32_t axfs_list_dir(axfs_volume_t *vol, uint64_t dir_inode, axfs_dirent_info_t *out, uint32_t max_entries)
{
    if (!vol->mounted || !out || max_entries == 0)
        return 0;

    axfs_inode_t dir;
    if (!read_inode(vol, dir_inode, &dir))
        return 0;
    if (dir.type != AXFS_TYPE_DIR)
        return 0;

    uint32_t count = 0;

    for (int b = 0; b < AXFS_DIRECT_BLOCKS && count < max_entries; b++)
    {
        if (dir.direct_blocks[b] == 0)
            continue;

        uint8_t buf[AXFS_BLOCK_SIZE];
        if (!axfs_disk_read_block(&vol->disk, dir.direct_blocks[b], buf))
            continue;

        uint32_t offset = 0;
        while (offset < AXFS_BLOCK_SIZE && count < max_entries)
        {
            axfs_dirent_t *ent = (axfs_dirent_t *)(buf + offset);
            if (ent->entry_size == 0)
                break;

            // Skips "tombstone" entries (deleted, inode_num == 0)
            if (ent->inode_num != 0)
            {
                out[count].inode_num = ent->inode_num;
                str_copy_n(out[count].name, ent->name, ent->name_len);
                out[count].name[ent->name_len] = '\0';
                out[count].type = ent->file_type;
                count++;
            }

            offset += ent->entry_size;
        }
    }

    return count;
}
