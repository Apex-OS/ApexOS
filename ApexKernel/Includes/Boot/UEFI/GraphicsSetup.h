// ApexKernel/Includes/Boot/UEFI/GraphicsSetup.h
// ApexOS -- UEFI bootloader: GOP graphics mode selection

#ifndef APEX_BOOT_UEFI_GRAPHICS_SETUP_H
#define APEX_BOOT_UEFI_GRAPHICS_SETUP_H

#include "Boot/UEFI/Efi.h"

#define FB_BOOT_INFO_ADDR 0x9300

// Mirrors Framebuffer.h's fb_boot_info_t exactly (packed, same field
// order/sizes) -- duplicated here rather than #include'd because this
// is a separate PE32+ build with its own toolchain (x86_64-w64-mingw32-gcc,
// not the kernel's plain gcc -- see Efi.h's header comment) and doesn't
// link against the rest of the kernel's headers.
typedef struct __attribute__((packed))
{
    uint32_t framebuffer_addr;
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    uint8_t bpp;
    uint8_t valid;
} fb_boot_info_t;

// Same target resolution as the BIOS path's VBE mode scan (see
// Boot/BIOS/Stage2.asm: MAX_FB_WIDTH/MAX_FB_HEIGHT) -- ApexOS's
// framebuffer console, back-buffer sizing, and UI layout (see
// ApexUI/) are tuned around 1600x900, not "whatever the biggest mode
// happens to be" -- a higher resolution than this isn't actually
// better for this kernel, just more back-buffer memory and more
// pixels to composite for no real benefit yet.
#define TARGET_FB_WIDTH 1600
#define TARGET_FB_HEIGHT 900

// Locates GOP, enumerates every mode it offers via QueryMode(), and
// picks the largest one at or under TARGET_FB_WIDTH x TARGET_FB_HEIGHT
// -- same target and same "biggest that still fits" selection logic
// as the BIOS path's VBE mode scan (see Boot/BIOS/Transition/
// HardwareDetect.asm), just via GOP's mode list instead of int 0x10
// calls. Calls SetMode() on the winning mode before reading its final
// framebuffer info, since GOP starts on whatever mode the firmware
// happened to already be in (often lower than this kernel wants), not
// necessarily the best one available.
//
// Returns FALSE (leaving `out` zeroed, `valid` = 0) if GOP isn't
// available, no mode has a pixel format this bootloader/kernel
// understands (PixelBitMask and PixelBltOnly don't give a plain
// linear RGB/BGR framebuffer -- see Efi.h's PixelFormat comment), or
// SetMode() itself fails -- the kernel already handles `valid == 0`
// gracefully (see HardwareDetect.asm's equivalent BIOS-side comment:
// falls back to VGA text mode) so this isn't a fatal error for the
// bootloader itself, just means no graphics.
BOOLEAN setup_graphics(EFI_SYSTEM_TABLE *st, fb_boot_info_t *out);

#endif // APEX_BOOT_UEFI_GRAPHICS_SETUP_H
