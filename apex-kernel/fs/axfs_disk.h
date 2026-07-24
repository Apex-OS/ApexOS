// apex-kernel/fs/axfs_disk.h
// ApexOS -- AXFS disk abstraction layer
//
// AXFS never talks to a physical disk directly -- it goes through
// this small interface (read_block/write_block). This lets the whole
// filesystem logic be validated on a RAM disk (fast, easy to debug,
// zero driver dependency), then a real ATA/AHCI disk can be wired up
// later by changing ONLY this file, without touching axfs.c.

#ifndef APEX_AXFS_DISK_H
#define APEX_AXFS_DISK_H

#include "../include/apex/types.h"
#include "axfs.h"

// Initializes the disk (RAM disk: allocates/zeroes the buffer; future
// ATA: detects the disk, reads its parameters). Must be called before
// any axfs_format()/axfs_mount().
//
// `size_bytes`: total size of the volume to create (RAM disk:
// allocates exactly that much from the kernel heap).
bool axfs_disk_init(uint64_t size_bytes);

// Reads one full block (AXFS_BLOCK_SIZE bytes) from disk into buf.
// `block_num` is an absolute block number (0 = the very first block
// of the volume, the superblock's).
bool axfs_disk_read_block(uint64_t block_num, void *buf);

// Writes one full block from buf to disk.
bool axfs_disk_write_block(uint64_t block_num, const void *buf);

// Total number of blocks available on the currently initialized disk.
uint64_t axfs_disk_total_blocks(void);

#endif // APEX_AXFS_DISK_H