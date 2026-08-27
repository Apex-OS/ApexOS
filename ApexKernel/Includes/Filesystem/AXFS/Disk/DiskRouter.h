// ApexKernel/Includes/Filesystem/AXFS/Disk/DiskRouter.h
// ApexOS -- AXFS disk abstraction layer
//
// AXFS never talks to a physical disk directly -- it goes through
// this small interface (read_block/write_block), bound to a specific
// volume's backend rather than a single global disk. This is what
// makes multiple AXFS volumes possible at once (see axfs_volume_t in
// AXFS.h): each volume owns its own axfs_disk_t, pointing at either
// the RAM disk backend or the real AHCI disk backend, and every
// axfs_disk_* call below takes that instance explicitly instead of
// reaching for global state.
//
// Two backends exist:
//   - RAM disk (axfs_disk_init_ram, see RAMBackend.c): a kmalloc'd
//     buffer standing in for a disk. Fast, no hardware dependency, but
//     volatile -- wiped every boot. Useful for testing/scratch
//     volumes.
//   - AHCI disk (axfs_disk_init_ahci, see AHCIBackend.c): the real
//     SATA drive, via Drivers/Storage/AHCI/. Persistent across boots
//     -- this is what @SysDisk uses now that AHCI is validated.

#ifndef APEX_FILESYSTEM_AXFS_DISK_ROUTER_H
#define APEX_FILESYSTEM_AXFS_DISK_ROUTER_H

#include "Core/Types.h"

typedef enum
{
    AXFS_DISK_BACKEND_NONE = 0,
    AXFS_DISK_BACKEND_RAM = 1,
    AXFS_DISK_BACKEND_AHCI = 2,
} axfs_disk_backend_kind_t;

// One volume's disk backend state. Opaque to the rest of AXFS -- they
// only ever call the axfs_disk_* functions below, passing this struct
// through. Callers outside Disk/ should treat the fields as private;
// they're declared here (rather than hidden behind a forward-declared
// pointer) only so axfs_volume_t can embed one directly, no extra
// allocation per volume.
typedef struct
{
    axfs_disk_backend_kind_t kind;

    // RAM backend: buffer + size. Unused (zero/NULL) for AHCI.
    uint8_t *ram_buffer;
    uint64_t ram_size_bytes;

    // AHCI backend: which sector the volume starts at, and how many
    // AXFS blocks (AXFS_BLOCK_SIZE each) it spans -- lets a volume
    // live in a sub-region of the disk rather than assuming it owns
    // every sector, though V1 always starts one at sector 0. Unused
    // (zero) for RAM.
    uint64_t ahci_start_lba;
    uint64_t ahci_total_blocks;
} axfs_disk_t;

// Initializes a RAM disk backend: allocates `size_bytes` from the
// kernel heap (kmalloc) and zeroes it. `size_bytes` should be a
// multiple of AXFS_BLOCK_SIZE (see AXFS.h) -- any remainder is
// wasted, not an error.
bool axfs_disk_init_ram(axfs_disk_t *disk, uint64_t size_bytes);

// Initializes an AHCI disk backend: the volume starts at
// `start_sector` (a 512-byte LBA, not an AXFS block number) and spans
// `size_bytes` worth of AXFS blocks. Requires ahci_init() to have
// already found a ready drive (see Drivers/Storage/AHCI/) -- fails
// otherwise, or if the requested range doesn't fit on the drive
// (ahci_drive_info().sector_count).
bool axfs_disk_init_ahci(axfs_disk_t *disk, uint64_t start_sector, uint64_t size_bytes);

// Reads one full block (AXFS_BLOCK_SIZE bytes) from `disk` into buf.
// `block_num` is relative to the volume (0 = the volume's own first
// block, its superblock's) -- axfs_disk_t's ahci_start_lba/
// ram_buffer offset translates that into the underlying disk/buffer
// position, callers never see the difference between backends.
bool axfs_disk_read_block(axfs_disk_t *disk, uint64_t block_num, void *buf);

// Writes one full block from buf to `disk`.
bool axfs_disk_write_block(axfs_disk_t *disk, uint64_t block_num, const void *buf);

// Total number of AXFS blocks available on `disk`.
uint64_t axfs_disk_total_blocks(const axfs_disk_t *disk);

#endif // APEX_FILESYSTEM_AXFS_DISK_ROUTER_H
