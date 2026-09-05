// ApexKernel/Filesystem/AXFS/Allocation/SuperblockSync.c
// ApexOS -- AXFS superblock write-back

#include "Filesystem/AXFS/Allocation/AllocationInternal.h"

void write_superblock(axfs_volume_t *vol, uint32_t transaction_id)
{
    uint8_t sb_block[AXFS_BLOCK_SIZE];
    for (uint32_t b = 0; b < AXFS_BLOCK_SIZE; b++)
        sb_block[b] = 0;
    for (uint32_t b = 0; b < sizeof(axfs_superblock_t); b++)
        sb_block[b] = ((uint8_t *)&vol->sb)[b];
    axfs_disk_write_block_journaled(vol, transaction_id, 0, sb_block);
}
