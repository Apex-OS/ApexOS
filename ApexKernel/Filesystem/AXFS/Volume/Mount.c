// ApexKernel/Filesystem/AXFS/Volume/Mount.c
// ApexOS -- AXFS volume mounting

#include "Filesystem/AXFS/AXFS.h"
#include "Drivers/Display/VGA.h"
#include "Logs/Log.h"

bool axfs_mount(axfs_volume_t *vol)
{
    axfs_superblock_t sb;
    if (!axfs_disk_read_block(&vol->disk, 0, &sb))
    {
        klog_error("axfs_mount: could not read superblock");
        return false;
    }

    if (sb.magic != AXFS_MAGIC)
    {
        // No log here on purpose -- this is the expected first read on
        // a fresh disk/RAM disk that's never been formatted, not a
        // real problem axfs_mount() should be announcing. The caller
        // (see Core/Startup/SysDiskSetup.c) already knows what a
        // failed axfs_mount() means in this context (format, then
        // mount again) and prints its own message explaining that,
        // with real context ("no filesystem found, formatting now")
        // instead of leaving something like "bad magic (volume not
        // formatted?)" for whoever's reading the boot log to puzzle
        // out on their own.
        return false;
    }

    if (sb.version != AXFS_VERSION)
    {
        klog_error("axfs_mount: unsupported version");
        return false;
    }

    vol->sb = sb;
    vol->mounted = true;

    klog_ok_prefix();
    vga_puts("AXFS mounted: \"");
    vga_puts(sb.volume_label);
    vga_puts("\" (");
    vga_putdec(sb.free_inodes);
    vga_puts("/");
    vga_putdec(sb.total_inodes);
    vga_puts(" inodes free, ");
    vga_putdec(sb.free_blocks);
    vga_puts(" data blocks free)\n");

    return true;
}
