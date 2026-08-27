// ApexKernel/Drivers/Display/Drawing/Shapes.c
// ApexOS -- Framebuffer geometric primitives (rectangles, lines)

#include "Drivers/Display/Drawing/Shapes.h"
#include "Drivers/Display/Framebuffer/Framebuffer.h"

// Clamps a rectangle to the framebuffer's actual bounds. Returns false
// if the rectangle is entirely off-screen (nothing to draw) -- callers
// should bail out early in that case rather than looping zero times,
// since x+w or y+h can wrap/overflow for pathological inputs.
static bool clip_rect(uint32_t *x, uint32_t *y, uint32_t *w, uint32_t *h)
{
    const fb_info_t *info = fb_get_info_ptr();

    if (*x >= info->width || *y >= info->height)
        return false;

    if (*x + *w > info->width)
        *w = info->width - *x;
    if (*y + *h > info->height)
        *h = info->height - *y;

    return *w > 0 && *h > 0;
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    if (!clip_rect(&x, &y, &w, &h))
        return;

    for (uint32_t row = 0; row < h; row++)
        for (uint32_t col = 0; col < w; col++)
            fb_put_pixel(x + col, y + row, color);
}

void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t thickness, uint32_t color)
{
    if (thickness == 0)
        return;

    // A border thicker than half the rectangle's width/height would
    // leave the top/bottom (or left/right) strips overlapping in the
    // middle -- rather than draw them twice (harmless, just wasted
    // work) or under-draw, clamp thickness so the four strips below
    // exactly tile the rectangle with no gap and no overlap beyond
    // what fb_fill_rect's own clipping already handles.
    uint32_t max_thickness_w = (w + 1) / 2;
    uint32_t max_thickness_h = (h + 1) / 2;
    if (thickness > max_thickness_w)
        thickness = max_thickness_w;
    if (thickness > max_thickness_h)
        thickness = max_thickness_h;

    fb_fill_rect(x, y, w, thickness, color);                 // top
    fb_fill_rect(x, y + h - thickness, w, thickness, color); // bottom
    fb_fill_rect(x, y, thickness, h, color);                 // left
    fb_fill_rect(x + w - thickness, y, thickness, h, color); // right
}

void fb_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color)
{
    fb_info_t info = fb_get_info();

    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int32_t dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int32_t sx = x0 < x1 ? 1 : -1;
    int32_t sy = y0 < y1 ? 1 : -1;
    int32_t err = (dx > dy ? dx : -dy) / 2;

    while (1)
    {
        // Per-pixel bounds check -- a line can legitimately start or
        // end off-screen (or cross the edge), unlike fb_fill_rect
        // where the whole shape is clipped once up front.
        if (x0 >= 0 && x0 < (int32_t)info.width && y0 >= 0 && y0 < (int32_t)info.height)
            fb_put_pixel((uint32_t)x0, (uint32_t)y0, color);

        if (x0 == x1 && y0 == y1)
            break;

        int32_t e2 = err;
        if (e2 > -dx)
        {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dy)
        {
            err += dx;
            y0 += sy;
        }
    }
}
