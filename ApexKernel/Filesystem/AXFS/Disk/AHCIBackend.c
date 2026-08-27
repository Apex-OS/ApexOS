// ApexKernel/Filesystem/AXFS/Disk/AHCIBackend.c
// ApexOS -- AXFS AHCI (real disk) backend

#include "Filesystem/AXFS/Disk/AHCIBackend.h"
#include "Filesystem/AXFS/AXFS.h"
#include "Drivers/Storage/AHCI/AHCI.h"
#include "Drivers/Display/VGA.h"
#include "ApexLib/Logs/Log.h"

// 512-byte sectors per AXFS block -- fixed by AXFS_BLOCK_SIZE (4096)
// vs the disk's fixed 512-byte sector size, not configurable per
// volume.
#define SECTORS_PER_BLOCK (AXFS_BLOCK_SIZE / 512)

bool axfs_disk_init_ahci(axfs_disk_t *disk, uint64_t start_sector, uint64_t size_bytes)
{
    ahci_drive_info_t drive = ahci_drive_info();
    if (!drive.ready)
    {
        klog_error("axfs_disk_init_ahci: no AHCI drive ready");
        return false;
    }

    uint64_t blocks = (size_bytes + AXFS_BLOCK_SIZE - 1) / AXFS_BLOCK_SIZE;
    uint64_t sectors_needed = blocks * SECTORS_PER_BLOCK;

    if (start_sector + sectors_needed > drive.sector_count)
    {
        klog_error("axfs_disk_init_ahci: requested range exceeds drive size");
        return false;
    }

    disk->kind = AXFS_DISK_BACKEND_AHCI;
    disk->ram_buffer = NULL;
    disk->ram_size_bytes = 0;
    disk->ahci_start_lba = start_sector;
    disk->ahci_total_blocks = blocks;

    klog_ok_prefix();
    vga_puts("AXFS AHCI disk initialized (");
    vga_putdec(blocks);
    vga_puts(" blocks, starting at sector ");
    vga_putdec(start_sector);
    vga_puts(")\n");

    return true;
}
