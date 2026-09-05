// ApexKernel/Boot/UEFI/GraphicsSetup.c
// ApexOS -- UEFI bootloader: GOP graphics mode selection (see
// GraphicsSetup.h)

#include "Boot/UEFI/GraphicsSetup.h"

BOOLEAN setup_graphics(EFI_SYSTEM_TABLE *st, fb_boot_info_t *out)
{
    out->framebuffer_addr = 0;
    out->width = 0;
    out->height = 0;
    out->pitch = 0;
    out->bpp = 0;
    out->valid = 0;

    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    EFI_STATUS status = st->boot_services->LocateProtocol(&gop_guid, NULL, (void **)&gop);
    if (status != EFI_SUCCESS || gop == NULL)
        return FALSE;

    // Scans every mode this GOP instance offers, keeping the largest
    // (by width, then height as a tie-break -- same simple comparison
    // the BIOS path uses, see HardwareDetect.asm's .better label) that
    // still fits within the target and uses a plain linear RGB/BGR
    // framebuffer.
    uint32_t best_mode = gop->Mode->mode; // fallback: whatever's already active, if nothing better qualifies
    uint32_t best_width = 0;
    uint32_t best_height = 0;
    BOOLEAN found_any = FALSE;

    for (uint32_t mode_num = 0; mode_num < gop->Mode->max_mode; mode_num++)
    {
        size_t mode_info_size = 0;
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mode_info = NULL;
        if (gop->QueryMode(gop, mode_num, &mode_info_size, &mode_info) != EFI_SUCCESS)
            continue; // some mode numbers can be invalid/unsupported -- skip, same as the BIOS scan's .next_mode

        if (mode_info->pixel_format != PixelRedGreenBlueReserved8BitPerColor &&
            mode_info->pixel_format != PixelBlueGreenRedReserved8BitPerColor)
            continue; // PixelBitMask/PixelBltOnly -- no plain linear framebuffer, see comment above

        if (mode_info->horizontal_resolution > TARGET_FB_WIDTH ||
            mode_info->vertical_resolution > TARGET_FB_HEIGHT)
            continue; // over the target -- see GraphicsSetup.h's header comment on why bigger isn't better here

        BOOLEAN better = (mode_info->horizontal_resolution > best_width) ||
                         (mode_info->horizontal_resolution == best_width &&
                          mode_info->vertical_resolution > best_height);
        if (better)
        {
            best_mode = mode_num;
            best_width = mode_info->horizontal_resolution;
            best_height = mode_info->vertical_resolution;
            found_any = TRUE;
        }
    }

    if (!found_any)
        return FALSE; // no mode at all had an acceptable pixel format within the target size

    if (gop->SetMode(gop, best_mode) != EFI_SUCCESS)
        return FALSE;

    // Re-read after SetMode() -- gop->Mode->info/frame_buffer_base now
    // reflect the mode that was actually just switched to, which is
    // what needs to end up in fb_boot_info_t for the kernel.
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = gop->Mode->info;

    out->framebuffer_addr = (uint32_t)gop->Mode->frame_buffer_base;
    out->width = (uint16_t)info->horizontal_resolution;
    out->height = (uint16_t)info->vertical_resolution;
    out->pitch = (uint16_t)(info->pixels_per_scan_line * 4); // GOP gives pitch in pixels, fb_boot_info_t wants bytes
    out->bpp = 32;                                           // both accepted pixel formats above are 32bpp -- see Efi.h's PixelFormat comment
    out->valid = 1;

    return TRUE;
}
