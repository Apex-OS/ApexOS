// ApexKernel/Includes/Filesystem/AXFS/Journal/Journal.h
// ApexOS -- AXFS write-ahead journal (crash consistency)
//
// Every AXFS operation that changes on-disk state (axfs_create,
// axfs_write, axfs_unlink, bitmap updates, superblock updates, ...)
// touches multiple blocks that must all end up on disk together --
// e.g. axfs_create() on a directory writes the inode bitmap, the data
// block bitmap, the new directory's first data block, the new inode,
// the parent directory's data block, the parent inode, and the
// superblock: 7 separate axfs_disk_write_block() calls (see
// Entry/EntryLifecycle.c and Entry/InodeCreation.c). A power loss or
// crash between any two of those leaves the volume in a state no
// single write ever intended -- an inode marked allocated with no
// directory entry pointing to it, a superblock free-block count that
// doesn't match what's actually on disk, and so on.
//
// This is full journaling (metadata AND file content, not just
// metadata) -- every axfs_disk_write_block() call for a mounted
// volume goes through the journal first, whatever it's writing.
// Simpler to reason about than splitting "which writes matter enough
// to journal" case by case, at the cost of writing each block twice
// (once to the journal, once to its real location) instead of once.
//
// ── On-disk format ──────────────────────────────────────────────────
// A fixed-size ring of journal_blocks slots, reserved on the volume
// right after the superblock (see AXFS.h's disk layout comment and
// Volume/LayoutCalculation.c). Each slot is exactly one
// axfs_journal_slot_t, padded to AXFS_BLOCK_SIZE so slot N always
// starts at journal_start_block + N -- no separate index/table block
// needed, the ring is self-describing from the slots themselves.
//
// Each slot holds one full block's worth of intended write (the
// target block number, the data, and a status). Multi-write
// operations (like axfs_create() above) span multiple consecutive
// slots sharing the same transaction_id; the transaction is only
// "done" once every slot with that ID is marked AXFS_JOURNAL_COMMITTED.
//
// ── Recovery ────────────────────────────────────────────────────────
// On mount (see Volume/Mount.c), axfs_journal_recover() scans every
// slot: any slot still AXFS_JOURNAL_PENDING (its transaction started
// but the volume never came back to mark it committed) gets replayed
// -- its saved data is written to its target block again. Because the
// journal keeps the actual data (not just "block N was about to
// change"), replay is unambiguous: it finishes the write that was
// interrupted, using exactly the bytes that write was going to
// produce. Slots already AXFS_JOURNAL_COMMITTED or AXFS_JOURNAL_FREE
// need no action. After recovery every replayed slot is reset to
// AXFS_JOURNAL_FREE so the same crash-and-replay isn't repeated on
// every subsequent mount.

#ifndef APEX_FILESYSTEM_AXFS_JOURNAL_H
#define APEX_FILESYSTEM_AXFS_JOURNAL_H

#include "Core/Types.h"

// Forward-declared rather than #include "Filesystem/AXFS/AXFS.h" --
// AXFS.h's axfs_volume_t needs journal_start_block/journal_blocks
// (see the superblock addition there), and Journal.h's functions take
// axfs_volume_t* -- an #include cycle either way. AXFS.h includes
// this header (it needs axfs_disk_write_block_journaled's
// declaration for its internal use), so this side only forward-
// declares.
typedef struct axfs_volume_s axfs_volume_t;

typedef enum
{
    AXFS_JOURNAL_FREE = 0,      // Slot unused -- no pending or committed write here.
    AXFS_JOURNAL_PENDING = 1,   // Write recorded, not yet confirmed applied to its real location.
    AXFS_JOURNAL_COMMITTED = 2, // Write successfully applied -- safe to ignore on recovery.
} axfs_journal_status_t;

// One journal slot's metadata -- occupies its own AXFS_BLOCK_SIZE
// disk block (the slot's *data* -- the full block being written --
// occupies the very next disk block; see this file's header comment
// on why a slot spans two disk blocks instead of one). Padded to
// AXFS_BLOCK_SIZE (4096, duplicated here as a literal since AXFS.h
// isn't included -- see the forward-declaration comment above) so
// slot N's metadata always starts at journal_start_block +
// N*AXFS_JOURNAL_SLOT_BLOCKS, trivially addressable with no separate
// index table.
typedef struct __attribute__((packed))
{
    uint32_t status;         // axfs_journal_status_t
    uint32_t transaction_id; // groups slots written as one logical operation (see file header)
    uint64_t target_block;   // which volume block this write is ultimately destined for
    uint8_t _pad[4096 - 4 - 4 - 8];
} axfs_journal_slot_meta_t;

// Disk blocks per journal slot: one for axfs_journal_slot_meta_t, one
// for the full AXFS_BLOCK_SIZE payload being written. Used by
// Journal.c to convert a slot index into the pair of disk blocks it
// occupies (journal_start_block + slot_index * this, and + 1 for the
// data block), and by Volume/LayoutCalculation.c to size the
// journal's reserved region.
#define AXFS_JOURNAL_SLOT_BLOCKS 2

// Fixed slot count -- 32 slots covers axfs_create()'s worst case (7
// writes for a new directory, see this file's header comment) more
// than 4 times over, with plenty of headroom for axfs_write()'s
// larger multi-block file writes. Generous rather than tight: at
// AXFS_JOURNAL_SLOT_BLOCKS blocks/slot and 4KB/block, 32 slots is
// 256KB -- negligible next to a real volume's size (e.g. @SysDisk's
// ~32MB), so there's no real cost to erring high. There is currently
// no SMP (see project roadmap), so only one transaction is ever
// actually in flight at a time regardless of this ceiling.
#define AXFS_JOURNAL_SLOT_COUNT 32

// Initializes the in-memory journal cursor for `vol` (next slot to
// write, next transaction_id) -- does NOT touch the disk. Call once
// right after a fresh axfs_format() (Volume/FormatWrite.c already
// zeroes the journal region as part of formatting the volume, so
// there's nothing to write here) or right after axfs_journal_recover()
// on an existing volume (recovery needs to know which slots it
// cleared before the cursor can safely resume from slot 0).
void axfs_journal_init(axfs_volume_t *vol);

// Scans every journal slot on `vol` and replays any still
// AXFS_JOURNAL_PENDING -- see this file's header comment for why that
// means "a write started but the volume never confirmed it finished".
// Safe to call on a volume with no pending writes (does nothing).
// Must run after the volume's superblock is loaded (axfs_mount()) but
// before any other AXFS operation touches the volume -- replay writes
// directly via the raw disk backend, bypassing the journal itself
// (see Journal.c), so nothing should be relying on the volume's state
// being final until this has run.
void axfs_journal_recover(axfs_volume_t *vol);

// Begins a new transaction -- returns its ID, used in the following
// axfs_disk_write_block_journaled() calls to group them as one atomic
// operation. Callers doing a multi-write operation (axfs_create(),
// axfs_write(), axfs_unlink(), ...) call this once at the start and
// pass the returned ID to every write in that operation, then call
// axfs_journal_commit_transaction() once all of them succeed.
uint32_t axfs_journal_begin_transaction(axfs_volume_t *vol);

// Journaled block write: records `data` for `target_block` in the
// journal (status PENDING) under `transaction_id`, then writes it to
// its real location, matching the file-level meaning callers actually
// want ("this block is now durably `data`") rather than exposing the
// two-step journal-then-apply mechanics. Returns false if either the
// journal write or the real write fails -- callers should treat that
// the same as axfs_disk_write_block() failing (abort the operation;
// see e.g. Entry/EntryLifecycle.c's existing rollback-on-failure
// paths).
bool axfs_disk_write_block_journaled(axfs_volume_t *vol, uint32_t transaction_id, uint64_t target_block, const void *data);

// Marks every slot under `transaction_id` as AXFS_JOURNAL_COMMITTED --
// call once every axfs_disk_write_block_journaled() in the operation
// has returned true. A transaction whose slots are all COMMITTED (or
// were never PENDING because the whole operation never even started)
// is never replayed on recovery, since every one of its writes is
// already confirmed to have reached its real location.
void axfs_journal_commit_transaction(axfs_volume_t *vol, uint32_t transaction_id);

#endif // APEX_FILESYSTEM_AXFS_JOURNAL_H
