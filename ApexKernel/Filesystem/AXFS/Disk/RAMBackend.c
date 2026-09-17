// ApexKernel/Filesystem/AXFS/Disk/RAMBackend.c
// ApexOS -- AXFS RAM disk backend

#include "Filesystem/AXFS/Disk/RAMBackend.h"
#include "Filesystem/AXFS/AXFS.h"
#include "Core/Memory/Heap/Heap.h"
#include "Drivers/Display/VGA.h"
#include "Core/Boot/Log/BootLog.h"

bool axfs_disk_init_ram(axfs_disk_t *disk, uint64_t size_bytes)
{
    uint64_t blocks = (size_bytes + AXFS_BLOCK_SIZE - 1) / AXFS_BLOCK_SIZE;
    uint64_t total_bytes = blocks * AXFS_BLOCK_SIZE;

    uint8_t *buffer = (uint8_t *)kmalloc(total_bytes);
    if (!buffer)
    {
        boot_log_error("axfs_disk_init_ram: kmalloc failed");
        return false;
    }

    // Zeroes the whole disk -- ensures bitmaps/inodes start from a
    // clean, predictable state instead of leftover heap contents from
    // whatever used this memory before.
    for (uint64_t i = 0; i < total_bytes; i++)
        buffer[i] = 0;

    disk->kind = AXFS_DISK_BACKEND_RAM;
    disk->ram_buffer = buffer;
    disk->ram_size_bytes = total_bytes;
    disk->ahci_start_lba = 0;
    disk->ahci_total_blocks = 0;

    boot_log_start();
    boot_log_str("AXFS RAM disk initialized (");
    boot_log_dec(total_bytes / (1024 * 1024));
    boot_log_str(" MB, ");
    boot_log_dec(blocks);
    boot_log_str(" blocks)");
    boot_log_end_ok();

    return true;
}
