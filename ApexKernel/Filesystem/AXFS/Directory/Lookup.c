// ApexKernel/Filesystem/AXFS/Directory/Lookup.c
// ApexOS -- AXFS directory entry lookup by name

#include "Filesystem/AXFS/AXFS.h"
#include "Filesystem/AXFS/Internal.h"

uint64_t axfs_lookup(axfs_volume_t *vol, uint64_t dir_inode, const char *name)
{
    if (!vol->mounted)
        return 0;

    axfs_inode_t dir;
    if (!read_inode(vol, dir_inode, &dir))
        return 0;
    if (dir.type != AXFS_TYPE_DIR)
        return 0;

    return dir_find_entry(vol, &dir, name, NULL, NULL);
}
