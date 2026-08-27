// ApexKernel/Filesystem/AXFS/Directory/TypeQuery.c
// ApexOS -- AXFS inode type query

#include "Filesystem/AXFS/AXFS.h"
#include "Filesystem/AXFS/Internal.h"

axfs_inode_type_t axfs_get_type(axfs_volume_t *vol, uint64_t inode_num)
{
    if (!vol->mounted)
        return AXFS_TYPE_FREE;

    axfs_inode_t inode;
    if (!read_inode(vol, inode_num, &inode))
        return AXFS_TYPE_FREE;

    return (axfs_inode_type_t)inode.type;
}
