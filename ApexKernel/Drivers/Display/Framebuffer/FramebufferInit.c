// ApexKernel/Drivers/Display/Framebuffer/FramebufferInit.c
// ApexOS -- Framebuffer bootstrap (VBE mode mapping)

#include "Drivers/Display/Framebuffer/FramebufferInit.h"
#include "Drivers/Display/Framebuffer/FramebufferInternal.h"
#include "Core/Memory/Virtual/PageTables.h"
#include "Core/Memory/Physical/PageBitmap.h"
#include "Core/Memory/Heap/Heap.h"
#include "Logs/Log.h"

bool fb_init(void)
{
    const fb_boot_info_t *boot_info = (const fb_boot_info_t *)FB_BOOT_INFO_ADDR;

    if (!boot_info->valid)
    {
        klog_warn("fb_init: no suitable VBE mode found at boot -- staying in VGA text mode");
        return false;
    }

    // Identity-maps the framebuffer: virtual address == physical
    // address, same approach as the rest of the kernel's low-memory
    // mappings (heap, AXD drivers). The framebuffer's physical base is
    // typically well above the 4MB-64MB range already identity-mapped
    // by vmm_init() (real hardware/QEMU usually place it around
    // 0xE0000000+), so it needs its own explicit mapping here.
    uint64_t phys_base = boot_info->framebuffer_addr;
    uint64_t total_bytes = (uint64_t)boot_info->pitch * boot_info->height;
    uint64_t pages = (total_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

    // Framebuffer memory is physically contiguous (VESA guarantees
    // this for the linear framebuffer) -- each page just needs
    // mapping to the matching physical offset, no allocation (the
    // memory already exists, owned by the graphics card).
    for (uint64_t i = 0; i < pages; i++)
    {
        uint64_t addr = phys_base + i * PAGE_SIZE;
        vmm_map(addr, addr, VMM_KERNEL_RW);
    }

    g_fb.addr = (uint32_t *)phys_base;
    g_fb.width = boot_info->width;
    g_fb.height = boot_info->height;
    g_fb.pitch_pixels = boot_info->pitch / 4; // bpp is always 32 in v1 -- see HardwareDetect.asm

    // Back-buffer sized off pitch_pixels (not width) so its layout
    // matches the real framebuffer exactly, including any scanline
    // padding -- fb_put_pixel/fb_get_pixel then use the same
    // `y * pitch_pixels + x` indexing for both without special-casing.
    // stage2 caps the picked mode to MAX_FB_WIDTH x MAX_FB_HEIGHT
    // (1600x900) specifically so this allocation (up to ~5.76MB)
    // always fits the kernel heap (Core/Memory/Heap/) alongside other
    // users.
    size_t backbuffer_bytes = (size_t)g_fb.pitch_pixels * g_fb.height * sizeof(uint32_t);
    g_backbuffer = (uint32_t *)kmalloc(backbuffer_bytes);
    if (!g_backbuffer)
    {
        klog_error("fb_init: kmalloc failed for back-buffer -- staying in VGA text mode");
        return false;
    }

    g_fb_ready = true;

    klog_ok_n("fb_init: framebuffer ready, width", g_fb.width);
    klog_ok_n("fb_init: framebuffer ready, height", g_fb.height);

    return true;
}
