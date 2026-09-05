// ApexKernel/Includes/Filesystem/AXFS/Volume/LayoutCalculation.h
// ApexOS -- AXFS on-disk layout calculation (used only by FormatWrite.c)

#ifndef APEX_FILESYSTEM_AXFS_LAYOUT_CALCULATION_H
#define APEX_FILESYSTEM_AXFS_LAYOUT_CALCULATION_H

#include "Core/Types.h"

#include "Filesystem/AXFS/AXFS.h"

// Computed on-disk zone boundaries for a fresh volume of a given
// size, before anything is written. Pure arithmetic -- no disk I/O.
typedef struct
{
    uint64_t total_blocks;
    uint64_t total_inodes;
    uint64_t journal_start;
    uint64_t inode_bitmap_start;
    uint64_t inode_bitmap_blocks;
    uint64_t block_bitmap_start;
    uint64_t block_bitmap_blocks;
    uint64_t inode_table_start;
    uint64_t inode_table_blocks;
    uint64_t data_start;
} axfs_volume_layout_t;

// Computes every zone boundary for a fresh volume on `disk`. Returns
// false if the volume is too small to hold even the minimum
// metadata (see FormatWrite.c for the exact thresholds).
bool axfs_calculate_layout(const axfs_disk_t *disk, axfs_volume_layout_t *out);

#endif // APEX_FILESYSTEM_AXFS_LAYOUT_CALCULATION_H
