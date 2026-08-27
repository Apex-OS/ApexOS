// ApexKernel/Filesystem/AXFS/Tree/DefaultTreeBuilder.c
// ApexOS -- Default directory tree builder

#include "Filesystem/AXFS/AXFS.h"
#include "Filesystem/AXFS/Tree/DefaultTreeData.h"
#include "Drivers/Display/VGA.h"
#include "ApexLib/Logs/Log.h"

bool axfs_create_default_tree(axfs_volume_t *vol)
{
    if (!vol->mounted)
        return false;

    uint32_t created = 0;
    for (uint32_t i = 0; i < AXFS_DEFAULT_TREE_COUNT; i++)
    {
        uint64_t inode = axfs_create_path(vol, AXFS_ROOT_INODE, AXFS_DEFAULT_TREE[i], AXFS_TYPE_DIR);
        if (inode == 0)
        {
            klog_error_prefix();
            vga_puts("axfs_create_default_tree: failed at '");
            vga_puts(AXFS_DEFAULT_TREE[i]);
            vga_puts("'\n");
            return false;
        }
        created++;
    }

    klog_ok_prefix();
    vga_puts("Default filesystem tree created (");
    vga_putdec(created);
    vga_puts(" directories)\n");

    return true;
}
