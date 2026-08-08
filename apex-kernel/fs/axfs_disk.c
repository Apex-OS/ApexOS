// apex-kernel/fs/axfs_disk.c
// ApexOS -- AXFS disk layer, RAM disk implementation
//
// V1: the "disk" is an array of physical pages allocated via the PMM,
// mapped contiguously in virtual memory. This validates the entire
// AXFS logic without depending on an ATA/AHCI driver -- a real driver
// will later replace ONLY this file (axfs.c has no idea the "disk" is
// in RAM; it only knows the axfs_disk.h interface).

#include "axfs_disk.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"
#include "../drivers/vga.h"
#include "../core/klog.h"

// Virtual address where the RAM disk is mapped. Chosen away from
// everything else (kernel heap = 8-12MB, USER_CODE/STACK = 4-7MB) to
// avoid any collision. See mm/heap.h and core/userspace.h for zones
// already in use.
#define AXFS_RAMDISK_VADDR 0x2000000 // 32MB

static uint8_t *ramdisk_base = NULL;
static uint64_t ramdisk_total_blocks = 0;

bool axfs_disk_init(uint64_t size_bytes)
{
    // Rounds up to a multiple of AXFS_BLOCK_SIZE.
    uint64_t blocks = (size_bytes + AXFS_BLOCK_SIZE - 1) / AXFS_BLOCK_SIZE;
    uint64_t total_bytes = blocks * AXFS_BLOCK_SIZE;
    uint64_t pages_needed = (total_bytes + 4095) / 4096;

    // Allocates and maps each physical page to a contiguous virtual
    // address, so the rest of axfs_disk can treat the "disk" as a
    // plain in-memory byte array without worrying about underlying
    // physical fragmentation (the VMM handles that).
    for (uint64_t i = 0; i < pages_needed; i++)
    {
        void *phys = pmm_alloc_page();
        if (!phys)
        {
            klog_error_n("axfs_disk_init: out of physical memory at i", i);
            return false;
        }
        vmm_map(AXFS_RAMDISK_VADDR + i * 4096, (uint64_t)phys, VMM_KERNEL_RW);
    }

    ramdisk_base = (uint8_t *)AXFS_RAMDISK_VADDR;
    ramdisk_total_blocks = blocks;

    // Zeroes the whole disk -- ensures bitmaps/inodes start from a
    // clean, predictable state instead of leftover physical garbage
    // from a previous use of these pages.
    for (uint64_t i = 0; i < total_bytes; i++)
        ramdisk_base[i] = 0;

    klog_ok_prefix();
    vga_puts("AXFS RAM disk initialized (");
    vga_putdec(total_bytes / (1024 * 1024));
    vga_puts(" MB, ");
    vga_putdec(blocks);
    vga_puts(" blocks)\n");

    return true;
}

bool axfs_disk_read_block(uint64_t block_num, void *buf)
{
    if (!ramdisk_base || block_num >= ramdisk_total_blocks)
        return false;

    uint8_t *src = ramdisk_base + block_num * AXFS_BLOCK_SIZE;
    uint8_t *dst = (uint8_t *)buf;
    for (uint32_t i = 0; i < AXFS_BLOCK_SIZE; i++)
        dst[i] = src[i];

    return true;
}

bool axfs_disk_write_block(uint64_t block_num, const void *buf)
{
    if (!ramdisk_base || block_num >= ramdisk_total_blocks)
        return false;

    uint8_t *dst = ramdisk_base + block_num * AXFS_BLOCK_SIZE;
    const uint8_t *src = (const uint8_t *)buf;
    for (uint32_t i = 0; i < AXFS_BLOCK_SIZE; i++)
        dst[i] = src[i];

    return true;
}

uint64_t axfs_disk_total_blocks(void)
{
    return ramdisk_total_blocks;
}