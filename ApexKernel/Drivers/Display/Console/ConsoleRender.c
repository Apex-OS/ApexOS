// ApexKernel/Drivers/Display/Console/ConsoleRender.c
// ApexOS -- Framebuffer console cell rendering

#include "Drivers/Display/Console/ConsoleRender.h"
#include "Drivers/Display/Console/Internal.h"
#include "Drivers/Display/Console/ConsoleCursor.h"
#include "Drivers/Display/Drawing/Text.h"
#include "Drivers/Display/Framebuffer/Framebuffer.h"

void fbcon_draw_cell(uint32_t row, uint32_t col, char c, uint8_t attr)
{
    uint32_t px = col * FBCON_GLYPH_W;
    uint32_t py = row * FBCON_GLYPH_H;
    uint32_t fg = vga_palette[attr & 0x0F];
    uint32_t bg = vga_palette[(attr >> 4) & 0x0F];

    // FBCON_SCALE is always 1 (see ConsoleCore.h) -- native 8x16
    // glyphs, so fb_draw_glyph_cell()'s single-pass fg/bg-per-pixel
    // rendering applies directly with no scale handling needed here.
    fb_draw_glyph_cell(px, py, c, fg, bg);
}

void fbcon_draw_cell_present(uint32_t row, uint32_t col, char c, uint8_t attr)
{
    fbcon_draw_cell(row, col, c, attr);
    fb_present_rect(col * FBCON_GLYPH_W, row * FBCON_GLYPH_H, FBCON_GLYPH_W, FBCON_GLYPH_H);
}

void fbcon_present(void)
{
    if (present_suppressed)
        return;
    fbcon_present_force();
}

void fbcon_present_force(void)
{
    __asm__ volatile("cli");

    uint32_t first = 0;
    if (total_lines > grid_height)
        first = total_lines - grid_height;
    if (scroll_offset > first)
        scroll_offset = first;
    first -= scroll_offset;

    for (uint32_t r = 0; r < grid_height; r++)
    {
        uint32_t src_line = first + r;
        for (uint32_t c = 0; c < grid_width; c++)
        {
            if (src_line < total_lines)
            {
                fbcon_cell_t *cell = &scrollback[src_line % FBCON_SCROLLBACK_LINES][c];
                fbcon_draw_cell(r, c, cell->c ? cell->c : ' ', cell->attr);
            }
            else
            {
                fbcon_draw_cell(r, c, ' ', fbcon_make_attr(VGA_WHITE, VGA_BLACK));
            }
        }
    }

    fbcon_draw_cursor();
    fb_present();

    __asm__ volatile("sti");
}

void fbcon_present_scroll_one_line(void)
{
    __asm__ volatile("cli");

    // Cursor erase happens in the caller (fbcon_putchar, ConsoleCore.c)
    // *before* fbcon_new_line() runs -- see the comment there for why:
    // this function is called after fbcon_new_line() has already
    // blanked the new scrollback line, so by this point there's no
    // way to recover what the old bottom row's cursor cell actually
    // held.

    // Shift every on-screen row up by one glyph row -- this is the
    // memmove that replaces redrawing all grid_width * grid_height
    // cells from scrollback data (see Framebuffer.c: fb_scroll_up()).
    // The exposed bottom row is filled with black here as a
    // placeholder; the loop right below immediately overwrites it
    // with the real new line's glyphs.
    fb_scroll_up(FBCON_GLYPH_H, vga_palette[VGA_BLACK]);

    // Only the bottom row is new content -- everything above it was
    // already correct on screen and just got shifted up by the
    // memmove, no re-fetch from scrollback or re-render needed.
    uint32_t bottom_row = grid_height - 1;
    uint32_t src_line = (total_lines > 0) ? total_lines - 1 : 0;
    for (uint32_t c = 0; c < grid_width; c++)
    {
        if (src_line < total_lines)
        {
            fbcon_cell_t *cell = &scrollback[src_line % FBCON_SCROLLBACK_LINES][c];
            fbcon_draw_cell(bottom_row, c, cell->c ? cell->c : ' ', cell->attr);
        }
        else
        {
            fbcon_draw_cell(bottom_row, c, ' ', fbcon_make_attr(VGA_WHITE, VGA_BLACK));
        }
    }

    // cur_row was already updated by the caller (fbcon_putchar) to
    // grid_height - 1 before calling here, so this draws the cursor
    // at the new bottom row -- the only row where it can legally be
    // after a scroll.
    fbcon_draw_cursor();

    // Whole-screen VRAM push, same cost as fbcon_present_force()'s --
    // fb_present_rect() can't help here because the *source* pixels
    // for rows 0..grid_height-2 moved (the memmove already put the
    // right data at the right place in the back-buffer, but VRAM
    // still holds the old, unshifted frame everywhere until this
    // copy runs). The win is entirely on the CPU/back-buffer side:
    // one sequential memmove instead of grid_width * grid_height
    // font lookups + fills + glyph draws.
    fb_present();

    __asm__ volatile("sti");
}
