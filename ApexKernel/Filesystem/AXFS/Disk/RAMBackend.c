// ApexKernel/Filesystem/AXFS/Disk/RAMBackend.c
// ApexOS -- AXFS RAM disk backend

#include "Filesystem/AXFS/Disk/RAMBackend.h"
#include "Filesystem/AXFS/AXFS.h"
#include "Core/Memory/Heap/Heap.h"
#include "Drivers/Display/VGA.h"
#include "Logs/Log.h"

bool axfs_disk_init_ram(axfs_disk_t *disk, uint64_t size_bytes)
{
    uint64_t blocks = (size_bytes + AXFS_BLOCK_SIZE - 1) / AXFS_BLOCK_SIZE;
    uint64_t total_bytes = blocks * AXFS_BLOCK_SIZE;

    uint8_t *buffer = (uint8_t *)kmalloc(total_bytes);
    if (!buffer)
    {
        klog_error("axfs_disk_init_ram: kmalloc failed");
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

    klog_ok_prefix();
    vga_puts("AXFS RAM disk initialized (");
    vga_putdec(total_bytes / (1024 * 1024));
    vga_puts(" MB, ");
    vga_putdec(blocks);
    vga_puts(" blocks)\n");

    return true;
}
