// ApexKernel/Includes/Core/Boot/DiskLayout.h
// ApexOS -- Boot-time disk layout info (UEFI path only)
//
// The UEFI bootloader (Boot/UEFI/Main.c) reads the disk's real GPT
// partition table to find where the ESP (EFI System Partition) ends,
// and writes that here -- so Core/Startup/SysDiskSetup.c knows where
// @SysDisk/AXFS can safely start without colliding with the ESP.
//
// This didn't exist for the BIOS path: that bootloader (Boot/BIOS/)
// doesn't use real GPT partitions at all -- it just reserves a fixed
// 2048-sector header region for itself (stage1/stage2/kernel.bin) and
// @SysDisk starts right after (see SysDiskSetup.c's own comment on
// that hardcoded value). UEFI needs a real ESP (a spec requirement,
// not a design choice -- the firmware itself only knows how to read
// FAT32), so @SysDisk's start position now depends on how large that
// partition actually turned out to be, which isn't known until boot
// time -- hence a runtime value passed here, rather than a second
// hardcoded constant that would drift out of sync with whatever size
// the ESP is actually created at (see the project's UEFI GPT+FAT32
// image-building scripts).
//
// `valid == 0` (the default if this address is never written, e.g. on
// a BIOS boot) means SysDiskSetup.c falls back to the historical fixed
// 2048-sector offset -- see that file for the actual fallback logic.
// This file only defines the address/format both sides agree on.

#ifndef APEX_CORE_BOOT_DISK_LAYOUT_H
#define APEX_CORE_BOOT_DISK_LAYOUT_H

#include "Core/Types.h"

// Placed in the gap between VBE_RESULT_ADDR's end (0x930C) and
// MMAP_ADDR (0x9600) -- both already proven safe low-memory scratch
// space for this bootloader (see MMAP_ADDR's own bug-fix comment in
// Boot/BIOS/Stage2.asm on why *not* every low-memory address is
// actually safe: 0xA000+ overlaps the EBDA, which real BIOS/firmware
// can be actively using -- an earlier version of this address at
// 0xA300 corrupted the BIOS boot path this way, confirmed via a
// visibly corrupted framebuffer after adding it). Same fixed-address
// convention as FB_BOOT_INFO_ADDR/MMAP_ADDR (see Framebuffer.h /
// MemoryMap.h) -- picked to not overlap either.
#define DISK_LAYOUT_INFO_ADDR 0x9400

typedef struct __attribute__((packed))
{
    uint64_t sysdisk_start_lba; // First LBA @SysDisk/AXFS may use -- see SysDiskSetup.c
    uint8_t valid;              // 0 = not written (BIOS boot, or GPT parsing failed) -- see file header
} disk_layout_info_t;

#endif // APEX_CORE_BOOT_DISK_LAYOUT_H
