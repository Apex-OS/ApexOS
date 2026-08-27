// ApexKernel/Filesystem/AXFS/Directory/DirectoryEntrySearch.c
// ApexOS -- AXFS raw directory-block entry search (internal helper)

#include "Filesystem/AXFS/Internal.h"

uint64_t dir_find_entry(axfs_volume_t *vol, const axfs_inode_t *dir, const char *name,
                        uint64_t *out_block, uint32_t *out_offset)
{
    uint32_t name_len = str_len(name);
    if (name_len > AXFS_MAX_NAME_LEN)
        return 0;

    for (int b = 0; b < AXFS_DIRECT_BLOCKS; b++)
    {
        if (dir->direct_blocks[b] == 0)
            continue;

        uint8_t buf[AXFS_BLOCK_SIZE];
        if (!axfs_disk_read_block(&vol->disk, dir->direct_blocks[b], buf))
            continue;

        uint32_t offset = 0;
        while (offset < AXFS_BLOCK_SIZE)
        {
            axfs_dirent_t *ent = (axfs_dirent_t *)(buf + offset);
            if (ent->entry_size == 0)
                break; // no more entries in this block

            if (ent->inode_num != 0 && ent->name_len == name_len &&
                str_eq_n(ent->name, name, name_len))
            {
                if (out_block)
                    *out_block = dir->direct_blocks[b];
                if (out_offset)
                    *out_offset = offset;
                return ent->inode_num;
            }

            offset += ent->entry_size;
        }
    }

    return 0;
}
