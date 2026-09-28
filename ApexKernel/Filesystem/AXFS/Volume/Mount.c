// ApexKernel/Filesystem/AXFS/Volume/Mount.c
// ApexOS -- AXFS volume mounting

#include "Filesystem/AXFS/AXFS.h"
#include "Drivers/Display/VGA.h"
#include "Core/Boot/Log/BootLog.h"

bool axfs_mount(axfs_volume_t *vol)
{
    axfs_superblock_t sb;
    if (!axfs_disk_read_block(&vol->disk, 0, &sb))
    {
        boot_log_error("axfs_mount: could not read superblock");
        return false;
    }

    if (sb.magic != AXFS_MAGIC)
    {
        // No log here on purpose -- this is the expected first read on
        // a fresh disk/RAM disk that's never been formatted, not a
        // real problem axfs_mount() should be announcing. The caller
        // (see Core/Startup/SysDiskSetup.c) already knows what a
        // failed axfs_mount() means in this context (format, then
        // mount again) and prints its own message explaining that,
        // with real context ("no filesystem found, formatting now")
        // instead of leaving something like "bad magic (volume not
        // formatted?)" for whoever's reading the boot log to puzzle
        // out on their own.
        return false;
    }

    if (sb.version != AXFS_VERSION)
    {
        boot_log_error("axfs_mount: unsupported version");
        return false;
    }

    // Sanity-checks the superblock's own internal layout before
    // trusting any of it -- a corrupted superblock (disk corruption, a
    // bug elsewhere that wrote garbage over it, a buggy external
    // formatting tool) that still happens to pass the magic/version
    // checks above could otherwise propagate straight into
    // alloc_data_block() (Allocation/DataBlockAllocation.c):
    // `total_blocks - data_start_block` is an unsigned subtraction, so
    // a data_start_block greater than total_blocks wraps to a huge
    // value and the block-search loop scans far past the real end of
    // the disk. Checked here, once, at mount time, rather than
    // trusting every later reader of these fields to defend against a
    // superblock that was never internally consistent to begin with.
    // Doesn't re-derive the exact block counts LayoutCalculation.c
    // would compute for this total_inodes/total_blocks (that formula
    // is FormatWrite.c-only today, and re-deriving it here would tie
    // this check to that formula never changing) -- just confirms the
    // five region start offsets this superblock claims are in the
    // expected order and inside the volume, which is enough to catch
    // gross corruption without assuming anything about exact region
    // sizes.
    if (!(sb.journal_start_block == 0 || sb.journal_start_block == 1) ||
        sb.inode_bitmap_block <= (sb.journal_start_block == 0 ? 0u : sb.journal_start_block) ||
        sb.block_bitmap_block <= sb.inode_bitmap_block ||
        sb.inode_table_block <= sb.block_bitmap_block ||
        sb.data_start_block <= sb.inode_table_block ||
        sb.data_start_block >= sb.total_blocks)
    {
        boot_log_error("axfs_mount: superblock layout is inconsistent -- refusing to mount");
        return false;
    }

    // sb.volume_label is read straight off disk and passed to
    // boot_log_str() (a plain C string function) further down --
    // must be null-terminated somewhere inside its 32 bytes, or that
    // call reads past the end of the struct looking for one that was
    // never there. Same hazard AXF's lib_name/func_name fields have
    // (see Formats/AXF's own validation) applied here to this
    // filesystem's own on-disk string field.
    bool label_terminated = false;
    for (size_t i = 0; i < sizeof(sb.volume_label); i++)
    {
        if (sb.volume_label[i] == '\0')
        {
            label_terminated = true;
            break;
        }
    }
    if (!label_terminated)
    {
        boot_log_error("axfs_mount: superblock volume_label is not null-terminated -- refusing to mount");
        return false;
    }

    vol->sb = sb;
    vol->mounted = true;

    // journal_start_block was added to the superblock alongside
    // full journaling -- a volume formatted before that has zeros
    // there (it came from what used to be padding in `reserved[]`),
    // which would collide with the superblock's own block (0) if
    // treated as a real journal location. Rather than risk writing
    // the journal over the superblock on an old volume, journaling is
    // simply left off for it: axfs_disk_write_block_journaled()'s
    // callers all check vol->journal_start_block != 0 first (0 is a
    // real value elsewhere, e.g. an unset field, but never a valid
    // journal_start_block: block 0 is always the superblock -- see
    // this module's disk layout comment) and fall back to a plain
    // axfs_disk_write_block() when it's unset -- same behavior this
    // volume had before journaling existed, not a regression, just
    // not yet upgraded. A real migration (recomputing the layout and
    // shifting the bitmaps/inode table/data that are already written)
    // is future work, not attempted here.
    if (sb.journal_start_block == 0)
    {
        boot_log_warn("axfs_mount: volume has no journal region (formatted before journaling existed) -- writes to this volume are not crash-protected until it's reformatted");
    }
    else
    {
        axfs_journal_recover(vol);
    }

    boot_log_start();
    boot_log_str("AXFS mounted: \"");
    boot_log_str(sb.volume_label);
    boot_log_str("\" (");
    boot_log_dec(sb.free_inodes);
    boot_log_str("/");
    boot_log_dec(sb.total_inodes);
    boot_log_str(" inodes free, ");
    boot_log_dec(sb.free_blocks);
    boot_log_str(" data blocks free)");
    boot_log_end_ok();

    return true;
}
