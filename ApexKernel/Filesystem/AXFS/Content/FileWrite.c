// ApexKernel/Filesystem/AXFS/Content/FileWrite.c
// ApexOS -- AXFS file content write

#include "Filesystem/AXFS/AXFS.h"
#include "Filesystem/AXFS/Internal.h"
#include "Core/Time/PITCounter.h"
#include "Formats/AXF/SafeRange/SafeRange.h"

bool axfs_write(axfs_volume_t *vol, uint64_t inode_num, const void *data, uint64_t size, uint64_t offset)
{
    if (!vol->mounted)
        return false;

    axfs_inode_t inode;
    if (!read_inode(vol, inode_num, &inode))
        return false;
    if (inode.type != AXFS_TYPE_FILE)
        return false;

    // V1: direct blocks only (no indirect/double-indirect writing
    // yet) -- max file size is AXFS_DIRECT_BLOCKS * AXFS_BLOCK_SIZE.
    // offset/size are not attacker-file-controlled the way an AXF
    // section's fields are, but they can still come from anywhere
    // that eventually calls axfs_write() with an unchecked value (a
    // syscall handler, a bug elsewhere in the kernel) -- checked via
    // axf_range_fits() rather than the raw `offset + size > max_size`
    // comparison, which could otherwise wrap for a large enough size
    // and let a write starting past the end of the file's addressable
    // range through, silently corrupting whatever
    // block_index/block_offset that wrapped offset then computes
    // into. This is the one occurrence of this pattern in the whole
    // codebase NOT bounded by AXF_MAX_FILE_SIZE (4096) the way every
    // AXF file loader's own checks are -- a wrapped offset/size here
    // has the whole disk, not a 4KB static buffer, to write beyond.
    uint64_t max_size = (uint64_t)AXFS_DIRECT_BLOCKS * AXFS_BLOCK_SIZE;
    if (!axf_range_fits(offset, size, max_size))
        return false;

    const uint8_t *src = (const uint8_t *)data;
    uint64_t written = 0;

    // Full journaling (not metadata-only) means file content itself
    // is covered here too, not just the inode/bitmap bookkeeping --
    // every block of actual data this loop writes, plus the inode
    // update at the end, is one transaction. Without this, a crash
    // partway through a multi-block write leaves the file with some
    // blocks updated and others still holding old data, with
    // size_bytes on disk possibly not matching either state.
    uint32_t txn = axfs_journal_begin_transaction(vol);

    while (written < size)
    {
        uint64_t file_pos = offset + written;
        uint64_t block_index = file_pos / AXFS_BLOCK_SIZE;
        uint64_t block_offset = file_pos % AXFS_BLOCK_SIZE;

        if (inode.direct_blocks[block_index] == 0)
        {
            uint64_t new_block = alloc_data_block(vol, txn);
            if (new_block == 0)
            {
                // Out of space partway through a multi-block write.
                // alloc_data_block() calls made so far in this loop
                // already updated inode.direct_blocks[] in memory and
                // journaled their bitmap bit under `txn` -- but until
                // now, this function just returned false here without
                // ever calling write_inode(), so the on-disk inode
                // never learned about those newly allocated blocks.
                // The bitmap still marked them used (correctly
                // journaled, so that part survives a crash) while no
                // inode anywhere referenced them -- a permanent leak
                // only an fsck-style scan could ever reclaim. Writing
                // the inode now, for whatever prefix of the write
                // actually completed, keeps every block this loop
                // allocated properly attached to the file instead of
                // orphaned -- the write as a whole still fails (the
                // caller asked for `size` bytes and didn't get them),
                // but nothing is silently lost on disk because of it.
                if (offset + written > inode.size_bytes)
                    inode.size_bytes = offset + written;
                inode.modified_at = pit_get_ticks();
                write_inode(vol, txn, inode_num, &inode);
                axfs_journal_commit_transaction(vol, txn);
                return false;
            }
            inode.direct_blocks[block_index] = new_block;
        }

        uint8_t buf[AXFS_BLOCK_SIZE];
        if (!axfs_disk_read_block(&vol->disk, inode.direct_blocks[block_index], buf))
        {
            // Same reasoning as the alloc_data_block() failure above:
            // commit what's already attached to the inode rather than
            // leaving newly allocated blocks orphaned.
            if (offset + written > inode.size_bytes)
                inode.size_bytes = offset + written;
            inode.modified_at = pit_get_ticks();
            write_inode(vol, txn, inode_num, &inode);
            axfs_journal_commit_transaction(vol, txn);
            return false;
        }

        uint64_t chunk = AXFS_BLOCK_SIZE - block_offset;
        if (chunk > size - written)
            chunk = size - written;

        for (uint64_t i = 0; i < chunk; i++)
            buf[block_offset + i] = src[written + i];

        if (!axfs_disk_write_block_journaled(vol, txn, inode.direct_blocks[block_index], buf))
        {
            // Same reasoning as the two failure paths above.
            if (offset + written > inode.size_bytes)
                inode.size_bytes = offset + written;
            inode.modified_at = pit_get_ticks();
            write_inode(vol, txn, inode_num, &inode);
            axfs_journal_commit_transaction(vol, txn);
            return false;
        }

        written += chunk;
    }

    if (offset + size > inode.size_bytes)
        inode.size_bytes = offset + size;
    inode.modified_at = pit_get_ticks();

    bool ok = write_inode(vol, txn, inode_num, &inode);

    axfs_journal_commit_transaction(vol, txn);

    return ok;
}
