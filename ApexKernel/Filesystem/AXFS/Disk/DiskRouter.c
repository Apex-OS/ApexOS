// ApexKernel/Filesystem/AXFS/Disk/DiskRouter.c
// ApexOS -- AXFS disk abstraction layer (see DiskRouter.h)

#include "Filesystem/AXFS/Disk/DiskRouter.h"
#include "Filesystem/AXFS/AXFS.h"
#include "Drivers/Storage/AHCI/AHCI.h"

#define SECTORS_PER_BLOCK (AXFS_BLOCK_SIZE / 512)

bool axfs_disk_read_block(axfs_disk_t *disk, uint64_t block_num, void *buf)
{
    if (disk->kind == AXFS_DISK_BACKEND_RAM)
    {
        if (!disk->ram_buffer || block_num * AXFS_BLOCK_SIZE >= disk->ram_size_bytes)
            return false;

        uint8_t *src = disk->ram_buffer + block_num * AXFS_BLOCK_SIZE;
        uint8_t *dst = (uint8_t *)buf;
        for (uint32_t i = 0; i < AXFS_BLOCK_SIZE; i++)
            dst[i] = src[i];

        return true;
    }

    if (disk->kind == AXFS_DISK_BACKEND_AHCI)
    {
        if (block_num >= disk->ahci_total_blocks)
            return false;

        uint64_t lba = disk->ahci_start_lba + block_num * SECTORS_PER_BLOCK;
        return ahci_read_sectors(lba, SECTORS_PER_BLOCK, buf);
    }

    return false; // AXFS_DISK_BACKEND_NONE -- never initialized
}

bool axfs_disk_write_block(axfs_disk_t *disk, uint64_t block_num, const void *buf)
{
    if (disk->kind == AXFS_DISK_BACKEND_RAM)
    {
        if (!disk->ram_buffer || block_num * AXFS_BLOCK_SIZE >= disk->ram_size_bytes)
            return false;

        uint8_t *dst = disk->ram_buffer + block_num * AXFS_BLOCK_SIZE;
        const uint8_t *src = (const uint8_t *)buf;
        for (uint32_t i = 0; i < AXFS_BLOCK_SIZE; i++)
            dst[i] = src[i];

        return true;
    }

    if (disk->kind == AXFS_DISK_BACKEND_AHCI)
    {
        if (block_num >= disk->ahci_total_blocks)
            return false;

        uint64_t lba = disk->ahci_start_lba + block_num * SECTORS_PER_BLOCK;
        return ahci_write_sectors(lba, SECTORS_PER_BLOCK, buf);
    }

    return false;
}

uint64_t axfs_disk_total_blocks(const axfs_disk_t *disk)
{
    if (disk->kind == AXFS_DISK_BACKEND_RAM)
        return disk->ram_size_bytes / AXFS_BLOCK_SIZE;
    if (disk->kind == AXFS_DISK_BACKEND_AHCI)
        return disk->ahci_total_blocks;
    return 0;
}
