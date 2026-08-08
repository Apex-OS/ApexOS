// apex-kernel/drivers/framebuffer.c
// ApexOS -- Linear framebuffer driver (VESA/VBE graphics mode)

#include "framebuffer.h"
#include "../mm/vmm.h"
#include "../core/klog.h"

static fb_info_t g_fb;
static bool g_fb_ready = false;

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
    g_fb.pitch_pixels = boot_info->pitch / 4; // bpp is always 32 in v1 -- see stage2.asm

    g_fb_ready = true;

    klog_ok_n("fb_init: framebuffer ready, width", g_fb.width);
    klog_ok_n("fb_init: framebuffer ready, height", g_fb.height);

    return true;
}

fb_info_t fb_get_info(void)
{
    return g_fb;
}

void fb_clear(uint32_t color)
{
    if (!g_fb_ready)
        return;

    for (uint32_t y = 0; y < g_fb.height; y++)
    {
        uint32_t *row = g_fb.addr + y * g_fb.pitch_pixels;
        for (uint32_t x = 0; x < g_fb.width; x++)
            row[x] = color;
    }
}

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (!g_fb_ready)
        return;

    g_fb.addr[y * g_fb.pitch_pixels + x] = color;
}