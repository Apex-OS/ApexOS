// apex-kernel/drivers/framebuffer.c
// ApexOS -- Linear framebuffer driver (VESA/VBE graphics mode)

#include "framebuffer.h"
#include "../mm/vmm.h"
#include "../mm/heap.h"
#include "../core/klog.h"

static fb_info_t g_fb;
static bool g_fb_ready = false;

// Off-screen buffer all drawing targets -- see the double-buffering
// note in framebuffer.h. Same layout as the real framebuffer (same
// pitch_pixels, so fb_put_pixel's indexing math is identical), just
// backed by heap memory instead of VRAM.
static uint32_t *g_backbuffer = NULL;

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

    // Back-buffer sized off pitch_pixels (not width) so its layout
    // matches the real framebuffer exactly, including any scanline
    // padding -- fb_put_pixel/fb_get_pixel then use the same
    // `y * pitch_pixels + x` indexing for both without special-casing.
    // stage2.asm caps the picked mode to MAX_FB_WIDTH x MAX_FB_HEIGHT
    // (1600x900) specifically so this allocation (up to ~5.76MB)
    // always fits the 8MB kernel heap (mm/heap.h) alongside other users.
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

fb_info_t fb_get_info(void)
{
    return g_fb;
}

void fb_clear(uint32_t color)
{
    if (!g_fb_ready)
        return;

    // Writes to the back-buffer only -- nothing visible changes until
    // the next fb_present().
    for (uint32_t y = 0; y < g_fb.height; y++)
    {
        uint32_t *row = g_backbuffer + y * g_fb.pitch_pixels;
        for (uint32_t x = 0; x < g_fb.width; x++)
            row[x] = color;
    }
}

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (!g_fb_ready)
        return;

    g_backbuffer[y * g_fb.pitch_pixels + x] = color;
}

uint32_t fb_get_pixel(uint32_t x, uint32_t y)
{
    if (!g_fb_ready)
        return 0;

    return g_backbuffer[y * g_fb.pitch_pixels + x];
}

void fb_present(void)
{
    if (!g_fb_ready)
        return;

    // Sequential copy, back-buffer -> real VRAM. One pass over
    // contiguous memory (pitch_pixels * height words) rather than a
    // pixel-by-pixel loop -- there's no per-pixel logic needed here
    // (unlike fb_put_pixel, both buffers share the exact same layout)
    // so a flat word copy is both simpler and faster.
    size_t words = (size_t)g_fb.pitch_pixels * g_fb.height;
    for (size_t i = 0; i < words; i++)
        g_fb.addr[i] = g_backbuffer[i];
}