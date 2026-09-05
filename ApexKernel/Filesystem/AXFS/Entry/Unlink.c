// ApexKernel/Filesystem/AXFS/Entry/Unlink.c
// ApexOS -- AXFS entry removal

#include "Filesystem/AXFS/AXFS.h"
#include "Filesystem/AXFS/Internal.h"
#include "Filesystem/AXFS/Allocation/AllocationInternal.h"

bool axfs_unlink(axfs_volume_t *vol, uint64_t dir_inode, const char *name)
{
    if (!vol->mounted)
        return false;

    axfs_inode_t dir;
    if (!read_inode(vol, dir_inode, &dir))
        return false;
    if (dir.type != AXFS_TYPE_DIR)
        return false;

    uint64_t target_block = 0;
    uint32_t target_offset = 0;
    uint64_t target_inode_num = dir_find_entry(vol, &dir, name, &target_block, &target_offset);
    if (target_inode_num == 0)
        return false;

    uint32_t name_len = str_len(name);
    if ((name_len == 1 && name[0] == '.') ||
        (name_len == 2 && name[0] == '.' && name[1] == '.'))
        return false;

    axfs_inode_t target;
    if (!read_inode(vol, target_inode_num, &target))
        return false;

    if (target.type == AXFS_TYPE_DIR && target.size_bytes > AXFS_DIRENT_HEADER_SIZE * 2 + 3)
        return false; // directory not empty (only "." and ".." left)

    uint8_t buf[AXFS_BLOCK_SIZE];
    if (!axfs_disk_read_block(&vol->disk, target_block, buf))
        return false;
    axfs_dirent_t *ent = (axfs_dirent_t *)(buf + target_offset);
    ent->inode_num = 0; // tombstone

    // Same reasoning as axfs_create() (see EntryLifecycle.c): the
    // tombstone write, every freed data block's bitmap update, the
    // inode bitmap update, and the superblock update all need to
    // land together -- a crash partway through could otherwise free
    // blocks the bitmap still shows as used (a leak, recoverable by a
    // future fsck) or, worse, mark the inode free while a directory
    // entry still points to it (a future axfs_create() could then
    // reallocate that inode number while the stale dirent still
    // resolves to it).
    uint32_t txn = axfs_journal_begin_transaction(vol);

    axfs_disk_write_block_journaled(vol, txn, target_block, buf);

    for (int b = 0; b < AXFS_DIRECT_BLOCKS; b++)
    {
        if (target.direct_blocks[b] != 0)
            free_data_block(vol, txn, target.direct_blocks[b]);
    }

    bitmap_set_bit(vol, txn, vol->sb.inode_bitmap_block, target_inode_num - 1, false);
    vol->sb.free_inodes++;

    write_superblock(vol, txn);

    axfs_journal_commit_transaction(vol, txn);

    return true;
}
