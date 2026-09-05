// ApexKernel/Filesystem/AXFS/Entry/DirectoryInsertion.c
// ApexOS -- AXFS directory entry insertion (internal helper for axfs_create)

#include "Filesystem/AXFS/Entry/DirectoryInsertion.h"
#include "Filesystem/AXFS/Internal.h"

bool axfs_insert_dirent(axfs_volume_t *vol, uint32_t transaction_id, axfs_inode_t *parent, const char *name,
                        uint32_t name_len, uint64_t inode_num, axfs_inode_type_t type)
{
    uint16_t new_entry_size = AXFS_DIRENT_HEADER_SIZE + (uint16_t)name_len;

    for (int b = 0; b < AXFS_DIRECT_BLOCKS; b++)
    {
        if (parent->direct_blocks[b] == 0)
        {
            uint64_t new_block = alloc_data_block(vol, transaction_id);
            if (new_block == 0)
                break;
            parent->direct_blocks[b] = new_block;
        }

        uint8_t buf[AXFS_BLOCK_SIZE];
        if (!axfs_disk_read_block(&vol->disk, parent->direct_blocks[b], buf))
            continue;

        uint32_t offset = 0;
        while (offset + new_entry_size <= AXFS_BLOCK_SIZE)
        {
            axfs_dirent_t *ent = (axfs_dirent_t *)(buf + offset);

            if (ent->entry_size == 0)
            {
                ent->inode_num = inode_num;
                ent->entry_size = new_entry_size;
                ent->name_len = (uint8_t)name_len;
                ent->file_type = (uint8_t)type;
                str_copy_n(ent->name, name, name_len);

                axfs_disk_write_block_journaled(vol, transaction_id, parent->direct_blocks[b], buf);
                return true;
            }

            offset += ent->entry_size;
        }
    }

    return false;
}
