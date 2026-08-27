// ApexKernel/Filesystem/AXFS/Content/FileRead.c
// ApexOS -- AXFS file content read

#include "Filesystem/AXFS/AXFS.h"
#include "Filesystem/AXFS/Internal.h"

uint64_t axfs_read(axfs_volume_t *vol, uint64_t inode_num, void *buf, uint64_t size, uint64_t offset)
{
    if (!vol->mounted)
        return 0;

    axfs_inode_t inode;
    if (!read_inode(vol, inode_num, &inode))
        return 0;
    if (inode.type != AXFS_TYPE_FILE)
        return 0;

    if (offset >= inode.size_bytes)
        return 0;

    uint64_t available = inode.size_bytes - offset;
    if (size > available)
        size = available;

    uint8_t *dst = (uint8_t *)buf;
    uint64_t total_read = 0;

    while (total_read < size)
    {
        uint64_t file_pos = offset + total_read;
        uint64_t block_index = file_pos / AXFS_BLOCK_SIZE;
        uint64_t block_offset = file_pos % AXFS_BLOCK_SIZE;

        if (block_index >= AXFS_DIRECT_BLOCKS || inode.direct_blocks[block_index] == 0)
            break; // hole in the file (V1: treated as end of readable data)

        uint8_t block_buf[AXFS_BLOCK_SIZE];
        if (!axfs_disk_read_block(&vol->disk, inode.direct_blocks[block_index], block_buf))
            break;

        uint64_t chunk = AXFS_BLOCK_SIZE - block_offset;
        if (chunk > size - total_read)
            chunk = size - total_read;

        for (uint64_t i = 0; i < chunk; i++)
            dst[total_read + i] = block_buf[block_offset + i];

        total_read += chunk;
    }

    return total_read;
}
