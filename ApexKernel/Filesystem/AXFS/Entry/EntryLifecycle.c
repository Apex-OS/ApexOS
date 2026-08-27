// ApexKernel/Filesystem/AXFS/Entry/EntryLifecycle.c
// ApexOS -- AXFS entry creation orchestration

#include "Filesystem/AXFS/Entry/EntryLifecycle.h"
#include "Filesystem/AXFS/Entry/InodeCreation.h"
#include "Filesystem/AXFS/Entry/DirectoryInsertion.h"
#include "Filesystem/AXFS/Internal.h"
#include "Core/Time/PITCounter.h"

uint64_t axfs_create(axfs_volume_t *vol, uint64_t parent_inode, const char *name, axfs_inode_type_t type)
{
    if (!vol->mounted)
        return 0;

    axfs_inode_t parent;
    if (!read_inode(vol, parent_inode, &parent))
        return 0;
    if (parent.type != AXFS_TYPE_DIR)
        return 0;

    if (dir_find_entry(vol, &parent, name, NULL, NULL) != 0)
        return 0; // already exists

    uint32_t name_len = str_len(name);
    if (name_len == 0 || name_len > AXFS_MAX_NAME_LEN)
        return 0;

    uint64_t new_inode_num = axfs_create_inode(vol, parent_inode, type);
    if (new_inode_num == 0)
        return 0;

    if (!axfs_insert_dirent(vol, &parent, name, name_len, new_inode_num, type))
    {
        // Rollback: the inode was created but couldn't be linked into
        // the parent -- free everything axfs_create_inode() allocated
        // rather than leaving an orphaned inode with no directory
        // entry pointing to it.
        axfs_inode_t orphan;
        if (read_inode(vol, new_inode_num, &orphan) && type == AXFS_TYPE_DIR && orphan.direct_blocks[0] != 0)
            free_data_block(vol, orphan.direct_blocks[0]);

        bitmap_set_bit(vol, vol->sb.inode_bitmap_block, new_inode_num - 1, false);
        vol->sb.free_inodes++;
        return 0;
    }

    parent.modified_at = pit_get_ticks();
    write_inode(vol, parent_inode, &parent);

    axfs_disk_write_block(&vol->disk, 0, &vol->sb);

    return new_inode_num;
}
