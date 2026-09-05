// ApexKernel/Includes/Boot/UEFI/GptReader.h
// ApexOS -- UEFI bootloader: GPT partition table reading
//
// Named GptReader rather than DiskLayout to avoid confusion with
// Core/Boot/DiskLayout.h -- that header defines the shared
// disk_layout_info_t format both this bootloader and the kernel agree
// on; this one is the UEFI-side logic that produces the value written
// into it (see Main.c: efi_main).

#ifndef APEX_BOOT_UEFI_GPT_READER_H
#define APEX_BOOT_UEFI_GPT_READER_H

#include "Boot/UEFI/Efi.h"

// Mirrors Core/Boot/DiskLayout.h's disk_layout_info_t exactly -- same
// duplication reasoning as GraphicsSetup.h's fb_boot_info_t.
typedef struct __attribute__((packed))
{
    uint64_t sysdisk_start_lba;
    uint8_t valid;
} disk_layout_info_t;

#define DISK_LAYOUT_INFO_ADDR 0x9400

// Finds the ESP's ending LBA by reading the real GPT off the physical
// disk this bootloader was itself loaded from -- see
// Core/Startup/SysDiskSetup.c for why @SysDisk needs this. Returns
// FALSE (out left unwritten) if no raw (non-partition)
// EFI_BLOCK_IO_PROTOCOL handle is found, the GPT signature doesn't
// check out, or no ESP-type partition entry exists -- any of which
// the caller treats as "couldn't determine disk layout", not a fatal
// boot error (see Main.c: efi_main, disk_layout_info_t.valid stays 0,
// and SysDiskSetup.c already has a fallback for that).
BOOLEAN find_esp_end_lba(EFI_SYSTEM_TABLE *st, uint64_t *out_end_lba);

#endif // APEX_BOOT_UEFI_GPT_READER_H
