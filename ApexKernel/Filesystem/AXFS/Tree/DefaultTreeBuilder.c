// ApexKernel/Filesystem/AXFS/Tree/DefaultTreeBuilder.c
// ApexOS -- Default directory tree builder

#include "Filesystem/AXFS/AXFS.h"
#include "Filesystem/AXFS/Tree/DefaultTreeData.h"
#include "Drivers/Display/VGA.h"
#include "Core/Boot/Log/BootLog.h"

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
            boot_log_start();
            boot_log_str("axfs_create_default_tree: failed at '");
            boot_log_str(AXFS_DEFAULT_TREE[i]);
            boot_log_str("'");
            boot_log_end_error();
            return false;
        }
        created++;
    }

    boot_log_start();
    boot_log_str("Default filesystem tree created (");
    boot_log_dec(created);
    boot_log_str(" directories)");
    boot_log_end_ok();

    return true;
}
