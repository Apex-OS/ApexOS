// ApexKernel/Drivers/Display/Framebuffer/Framebuffer.c
// ApexOS -- Linear framebuffer driver (VESA/VBE graphics mode)

#include "Drivers/Display/Framebuffer/Framebuffer.h"
#include "Drivers/Display/Framebuffer/FramebufferInternal.h"

fb_info_t g_fb;
bool g_fb_ready = false;
uint32_t *g_backbuffer = NULL;

fb_info_t fb_get_info(void)
{
    return g_fb;
}

const fb_info_t *fb_get_info_ptr(void)
{
    return &g_fb;
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

void fb_present_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    if (!g_fb_ready)
        return;

    if (x >= g_fb.width || y >= g_fb.height)
        return;
    if (x + w > g_fb.width)
        w = g_fb.width - x;
    if (y + h > g_fb.height)
        h = g_fb.height - y;

    // Row-by-row copy, only the changed rectangle -- e.g. a single
    // 16x16 text cell is 1024 pixels instead of the ~1.44M pixels a
    // full fb_present() would copy at 1600x900. This is what makes
    // per-character terminal updates (see Drivers/Display/Console/)
    // fast enough to keep up with typing.
    for (uint32_t row = 0; row < h; row++)
    {
        uint32_t *src = g_backbuffer + (y + row) * g_fb.pitch_pixels + x;
        uint32_t *dst = g_fb.addr + (y + row) * g_fb.pitch_pixels + x;
        for (uint32_t col = 0; col < w; col++)
            dst[col] = src[col];
    }
}
