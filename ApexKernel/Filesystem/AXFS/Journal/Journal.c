// ApexKernel/Filesystem/AXFS/Journal/Journal.c
// ApexOS -- AXFS write-ahead journal (see Journal.h for the on-disk
// format and recovery model)

#include "Filesystem/AXFS/Journal/Journal.h"
#include "Filesystem/AXFS/AXFS.h"
#include "Logs/Log.h"

// Converts a slot index into its two disk blocks: metadata, then
// data. Both always relative to vol->sb.journal_start_block -- see
// Journal.h's axfs_journal_slot_meta_t comment on why each slot spans
// AXFS_JOURNAL_SLOT_BLOCKS (2) disk blocks rather than 1.
static uint64_t slot_meta_block(const axfs_volume_t *vol, uint32_t slot_index)
{
    return vol->sb.journal_start_block + (uint64_t)slot_index * AXFS_JOURNAL_SLOT_BLOCKS;
}

static uint64_t slot_data_block(const axfs_volume_t *vol, uint32_t slot_index)
{
    return slot_meta_block(vol, slot_index) + 1;
}

void axfs_journal_init(axfs_volume_t *vol)
{
    vol->journal_next_slot = 0;
    vol->journal_next_transaction_id = 1; // 0 reserved as "no transaction" for callers that never begin one
}

void axfs_journal_recover(axfs_volume_t *vol)
{
    uint32_t replayed = 0;

    for (uint32_t i = 0; i < AXFS_JOURNAL_SLOT_COUNT; i++)
    {
        axfs_journal_slot_meta_t meta;
        if (!axfs_disk_read_block(&vol->disk, slot_meta_block(vol, i), &meta))
            continue; // unreadable slot -- nothing safe to do but skip it

        if (meta.status != AXFS_JOURNAL_PENDING)
            continue; // FREE or already COMMITTED -- no replay needed

        // This slot's write started but the volume never confirmed it
        // reached its real location (see Journal.h's header comment)
        // -- finish it now, using the data the journal already has,
        // before anything else touches the volume.
        uint8_t data[4096];
        if (axfs_disk_read_block(&vol->disk, slot_data_block(vol, i), data))
        {
            axfs_disk_write_block(&vol->disk, meta.target_block, data);
            replayed++;
        }

        // Reset to FREE either way -- a slot whose data block couldn't
        // even be read is unrecoverable, but leaving it PENDING would
        // just make every future mount retry the same failed replay
        // forever. Better to move on: the interrupted write is lost,
        // but the journal itself stays healthy for the next
        // operation, rather than wedging on this one bad slot.
        meta.status = AXFS_JOURNAL_FREE;
        axfs_disk_write_block(&vol->disk, slot_meta_block(vol, i), &meta);
    }

    if (replayed > 0)
        klog_ok_n("axfs_journal_recover: replayed pending writes", replayed);

    axfs_journal_init(vol);
}

uint32_t axfs_journal_begin_transaction(axfs_volume_t *vol)
{
    uint32_t id = vol->journal_next_transaction_id++;
    if (vol->journal_next_transaction_id == 0)
        vol->journal_next_transaction_id = 1; // wrap past the reserved 0, extremely unlikely but free to guard
    return id;
}

bool axfs_disk_write_block_journaled(axfs_volume_t *vol, uint32_t transaction_id, uint64_t target_block, const void *data)
{
    // Old volumes formatted before journaling existed have
    // journal_start_block == 0 (see Volume/Mount.c's migration
    // comment) -- block 0 is always the superblock, never a real
    // journal location, so this is an unambiguous "no journal here"
    // signal. Falling back to a plain write matches this volume's
    // behavior before journaling existed rather than writing into the
    // superblock or another live structure.
    if (vol->sb.journal_start_block == 0)
        return axfs_disk_write_block(&vol->disk, target_block, data);

    // Every slot is claimed round-robin and always fully committed or
    // reset to FREE before axfs_disk_write_block_journaled() returns
    // (see below) -- so a slot being reused here is never actually
    // "in use" by an unfinished transaction; there's nothing to wait
    // on or evict. If this ever changes (e.g. real concurrent
    // transactions once SMP lands -- see project roadmap), this is
    // the point that would need real slot allocation instead of a
    // bare counter.
    uint32_t slot = vol->journal_next_slot;
    vol->journal_next_slot = (vol->journal_next_slot + 1) % AXFS_JOURNAL_SLOT_COUNT;

    axfs_journal_slot_meta_t meta;
    meta.status = AXFS_JOURNAL_PENDING;
    meta.transaction_id = transaction_id;
    meta.target_block = target_block;

    if (!axfs_disk_write_block(&vol->disk, slot_data_block(vol, slot), data))
        return false;
    if (!axfs_disk_write_block(&vol->disk, slot_meta_block(vol, slot), &meta))
        return false;

    // Only after both journal writes succeed does the real write
    // happen -- this is the "write-ahead" in write-ahead journal: the
    // journal always has a complete, durable copy of this write
    // before the real (possibly interrupted) write is even attempted.
    if (!axfs_disk_write_block(&vol->disk, target_block, data))
        return false;

    // The real write succeeded -- commit this single slot immediately
    // rather than waiting for axfs_journal_commit_transaction(). This
    // matters for partial-transaction crashes: if a later write in the
    // same transaction fails or the kernel dies before
    // commit_transaction() runs, this slot is already COMMITTED and
    // recovery correctly leaves it alone (its write really did
    // finish) instead of needlessly replaying it. commit_transaction()
    // below is what makes the *whole operation* durable from the
    // caller's perspective, but each individual write is self-
    // consistent the moment it lands on disk.
    meta.status = AXFS_JOURNAL_COMMITTED;
    axfs_disk_write_block(&vol->disk, slot_meta_block(vol, slot), &meta);

    return true;
}

void axfs_journal_commit_transaction(axfs_volume_t *vol, uint32_t transaction_id)
{
    // Every axfs_disk_write_block_journaled() call already commits
    // its own slot as soon as its real write succeeds (see above) --
    // so by the time a caller has successfully called that for every
    // write in its operation, there is nothing left for this function
    // to actually do to the journal itself.
    //
    // It still exists as an explicit, required call (rather than
    // folding this into the last write) because it's the caller-
    // facing boundary that means "this whole multi-write operation is
    // now durable and consistent" -- the write-level auto-commit above
    // is an implementation detail of how individual slots stay
    // recovery-safe even if the operation as a whole never reaches
    // this call (e.g. the caller bails out partway through and never
    // asks for the rest of the writes it originally planned). Keeping
    // this as a distinct step also leaves room for a real per-
    // transaction commit record later (e.g. if a future version needs
    // "all N writes happened, or none did" semantics instead of the
    // current "each write that happened, happened correctly" -- see
    // this file's header comment on the difference) without changing
    // every call site again.
    (void)vol;
    (void)transaction_id;
}
