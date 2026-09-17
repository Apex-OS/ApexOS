// ApexKernel/Filesystem/AXFS/Disk/AHCIBackend.c
// ApexOS -- AXFS AHCI (real disk) backend

#include "Filesystem/AXFS/Disk/AHCIBackend.h"
#include "Filesystem/AXFS/AXFS.h"
#include "Drivers/Storage/AHCI/AHCI.h"
#include "Drivers/Display/VGA.h"
#include "Core/Boot/Log/BootLog.h"

// 512-byte sectors per AXFS block -- fixed by AXFS_BLOCK_SIZE (4096)
// vs the disk's fixed 512-byte sector size, not configurable per
// volume.
#define SECTORS_PER_BLOCK (AXFS_BLOCK_SIZE / 512)

bool axfs_disk_init_ahci(axfs_disk_t *disk, uint64_t start_sector, uint64_t size_bytes)
{
    ahci_drive_info_t drive = ahci_drive_info();
    if (!drive.ready)
    {
        boot_log_error("axfs_disk_init_ahci: no AHCI drive ready");
        return false;
    }

    uint64_t blocks = (size_bytes + AXFS_BLOCK_SIZE - 1) / AXFS_BLOCK_SIZE;
    uint64_t sectors_needed = blocks * SECTORS_PER_BLOCK;

    if (start_sector + sectors_needed > drive.sector_count)
    {
        boot_log_error("axfs_disk_init_ahci: requested range exceeds drive size");
        return false;
    }

    disk->kind = AXFS_DISK_BACKEND_AHCI;
    disk->ram_buffer = NULL;
    disk->ram_size_bytes = 0;
    disk->ahci_start_lba = start_sector;
    disk->ahci_total_blocks = blocks;

    boot_log_start();
    boot_log_str("AXFS AHCI disk initialized (");
    boot_log_dec(blocks);
    boot_log_str(" blocks, starting at sector ");
    boot_log_dec(start_sector);
    boot_log_str(")");
    boot_log_end_ok();

    return true;
}
