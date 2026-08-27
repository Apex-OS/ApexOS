// ApexKernel/Drivers/Display/Drawing/Cursor.c
// ApexOS -- Framebuffer mouse cursor rendering

#include "Drivers/Display/Drawing/Cursor.h"
#include "Drivers/Display/Drawing/Shapes.h"
#include "Drivers/Display/Framebuffer/Framebuffer.h"

void fb_draw_cursor(uint32_t x, uint32_t y, uint32_t color)
{
    // Classic arrow shape: a tall diagonal spine from the hotspot
    // (top-left tip) down-right, with a short tail cutting back up to
    // the left partway down -- gives the familiar pointer silhouette
    // without needing a bitmap asset. Coordinates are relative offsets
    // from (x, y); fb_draw_line handles clipping if the cursor is
    // near a screen edge.
    fb_draw_line(x, y, x, y + 16, color);
    fb_draw_line(x, y, x + 11, y + 11, color);
    fb_draw_line(x, y + 16, x + 4, y + 12, color);
    fb_draw_line(x + 4, y + 12, x + 7, y + 19, color);
    fb_draw_line(x + 7, y + 19, x + 9, y + 18, color);
    fb_draw_line(x + 9, y + 18, x + 6, y + 11, color);
    fb_draw_line(x + 6, y + 11, x + 11, y + 11, color);
}

// Backing store for fb_save_cursor_bg()/fb_restore_cursor_bg() --
// FB_CURSOR_W x FB_CURSOR_H pixels, matching fb_draw_cursor's
// bounding box (offsets 0..11 in x, 0..19 in y -- see the coordinates
// above). Static, single-slot: only one cursor is ever on screen at a
// time, so there's no need for more than one saved block.
static uint32_t cursor_bg[FB_CURSOR_W * FB_CURSOR_H];

void fb_save_cursor_bg(uint32_t x, uint32_t y)
{
    fb_info_t info = fb_get_info();

    for (uint32_t row = 0; row < FB_CURSOR_H; row++)
    {
        uint32_t py = y + row;
        if (py >= info.height)
            break; // rows past this are all off-screen too (y only grows)

        for (uint32_t col = 0; col < FB_CURSOR_W; col++)
        {
            uint32_t px = x + col;
            // Off-screen columns are left as whatever they were
            // (uninitialized on the very first save, stale on later
            // ones) -- harmless, since fb_restore_cursor_bg() applies
            // the same clipping and will never write those slots back
            // to an off-screen pixel either.
            if (px >= info.width)
                continue;
            cursor_bg[row * FB_CURSOR_W + col] = fb_get_pixel(px, py);
        }
    }
}

void fb_restore_cursor_bg(uint32_t x, uint32_t y)
{
    fb_info_t info = fb_get_info();

    for (uint32_t row = 0; row < FB_CURSOR_H; row++)
    {
        uint32_t py = y + row;
        if (py >= info.height)
            break;

        for (uint32_t col = 0; col < FB_CURSOR_W; col++)
        {
            uint32_t px = x + col;
            if (px >= info.width)
                continue;
            fb_put_pixel(px, py, cursor_bg[row * FB_CURSOR_W + col]);
        }
    }
}
