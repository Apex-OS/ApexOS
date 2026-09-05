// ApexKernel/Filesystem/AXFS/Entry/EntryLifecycle.c
// ApexOS -- AXFS entry creation orchestration

#include "Filesystem/AXFS/Entry/EntryLifecycle.h"
#include "Filesystem/AXFS/Entry/InodeCreation.h"
#include "Filesystem/AXFS/Entry/DirectoryInsertion.h"
#include "Filesystem/AXFS/Internal.h"
#include "Filesystem/AXFS/Allocation/AllocationInternal.h"
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

    // Every write below (inode bitmap, block bitmap, the new inode's
    // data block, the new inode itself, the parent's data block, the
    // parent inode, the superblock) is part of one logical operation
    // -- "create this entry" either fully lands on disk or, if
    // interrupted, gets replayed to completion on the next mount (see
    // Journal/Journal.h). Without this, a crash midway through could
    // leave an inode allocated with no directory entry pointing to it,
    // or a superblock free-count that doesn't match the bitmaps.
    uint32_t txn = axfs_journal_begin_transaction(vol);

    uint64_t new_inode_num = axfs_create_inode(vol, txn, parent_inode, type);
    if (new_inode_num == 0)
        return 0;

    if (!axfs_insert_dirent(vol, txn, &parent, name, name_len, new_inode_num, type))
    {
        // Rollback: the inode was created but couldn't be linked into
        // the parent -- free everything axfs_create_inode() allocated
        // rather than leaving an orphaned inode with no directory
        // entry pointing to it. Still part of the same transaction --
        // these writes need the same crash protection as the ones
        // that led to needing a rollback in the first place.
        axfs_inode_t orphan;
        if (read_inode(vol, new_inode_num, &orphan) && type == AXFS_TYPE_DIR && orphan.direct_blocks[0] != 0)
            free_data_block(vol, txn, orphan.direct_blocks[0]);

        bitmap_set_bit(vol, txn, vol->sb.inode_bitmap_block, new_inode_num - 1, false);
        vol->sb.free_inodes++;
        write_superblock(vol, txn);
        axfs_journal_commit_transaction(vol, txn);
        return 0;
    }

    parent.modified_at = pit_get_ticks();
    write_inode(vol, txn, parent_inode, &parent);

    write_superblock(vol, txn);

    axfs_journal_commit_transaction(vol, txn);

    return new_inode_num;
}
