// ApexKernel/Filesystem/AXFS/Content/FileWrite.c
// ApexOS -- AXFS file content write

#include "Filesystem/AXFS/AXFS.h"
#include "Filesystem/AXFS/Internal.h"
#include "Core/Time/PITCounter.h"

bool axfs_write(axfs_volume_t *vol, uint64_t inode_num, const void *data, uint64_t size, uint64_t offset)
{
    if (!vol->mounted)
        return false;

    axfs_inode_t inode;
    if (!read_inode(vol, inode_num, &inode))
        return false;
    if (inode.type != AXFS_TYPE_FILE)
        return false;

    // V1: direct blocks only (no indirect/double-indirect writing
    // yet) -- max file size is AXFS_DIRECT_BLOCKS * AXFS_BLOCK_SIZE.
    uint64_t max_size = (uint64_t)AXFS_DIRECT_BLOCKS * AXFS_BLOCK_SIZE;
    if (offset + size > max_size)
        return false;

    const uint8_t *src = (const uint8_t *)data;
    uint64_t written = 0;

    while (written < size)
    {
        uint64_t file_pos = offset + written;
        uint64_t block_index = file_pos / AXFS_BLOCK_SIZE;
        uint64_t block_offset = file_pos % AXFS_BLOCK_SIZE;

        if (inode.direct_blocks[block_index] == 0)
        {
            uint64_t new_block = alloc_data_block(vol);
            if (new_block == 0)
                return false; // out of space -- partial write already applied, caller should treat as failure
            inode.direct_blocks[block_index] = new_block;
        }

        uint8_t buf[AXFS_BLOCK_SIZE];
        if (!axfs_disk_read_block(&vol->disk, inode.direct_blocks[block_index], buf))
            return false;

        uint64_t chunk = AXFS_BLOCK_SIZE - block_offset;
        if (chunk > size - written)
            chunk = size - written;

        for (uint64_t i = 0; i < chunk; i++)
            buf[block_offset + i] = src[written + i];

        if (!axfs_disk_write_block(&vol->disk, inode.direct_blocks[block_index], buf))
            return false;

        written += chunk;
    }

    if (offset + size > inode.size_bytes)
        inode.size_bytes = offset + size;
    inode.modified_at = pit_get_ticks();

    return write_inode(vol, inode_num, &inode);
}
