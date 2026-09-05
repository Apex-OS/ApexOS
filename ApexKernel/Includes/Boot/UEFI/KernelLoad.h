// ApexKernel/Includes/Boot/UEFI/KernelLoad.h
// ApexOS -- UEFI bootloader: kernel.bin loading from the ESP

#ifndef APEX_BOOT_UEFI_KERNEL_LOAD_H
#define APEX_BOOT_UEFI_KERNEL_LOAD_H

#include "Boot/UEFI/Efi.h"

#define KERNEL_LOAD_ADDR 0x100000

// Walks image_handle -> EFI_LOADED_IMAGE_PROTOCOL -> DeviceHandle ->
// EFI_SIMPLE_FILE_SYSTEM_PROTOCOL -> OpenVolume() to get the root
// directory of whatever partition this bootloader itself was loaded
// from (the ESP). Returns NULL on any failure along the chain.
EFI_FILE_PROTOCOL *open_esp_root(EFI_SYSTEM_TABLE *st, EFI_HANDLE image_handle);

// Loads \kernel.bin from `root` directly to KERNEL_LOAD_ADDR, matching
// the fixed physical load address the BIOS path also uses (see
// Boot/BIOS/Transition/LongModeEntry.asm's `call 0x100000`). Returns
// FALSE on any failure (file missing, read error) -- caller treats
// this as fatal, there's nothing useful this bootloader can do
// without a kernel to jump to.
BOOLEAN load_kernel(EFI_FILE_PROTOCOL *root);

#endif // APEX_BOOT_UEFI_KERNEL_LOAD_H
