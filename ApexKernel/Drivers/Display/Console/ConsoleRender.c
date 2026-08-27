// ApexKernel/Drivers/Display/Console/ConsoleRender.c
// ApexOS -- Framebuffer console cell rendering

#include "Drivers/Display/Console/ConsoleRender.h"
#include "Drivers/Display/Console/Internal.h"
#include "Drivers/Display/Console/ConsoleCursor.h"
#include "Drivers/Display/Drawing/Shapes.h"
#include "Drivers/Display/Drawing/Text.h"
#include "Drivers/Display/Framebuffer/Framebuffer.h"

void fbcon_draw_cell(uint32_t row, uint32_t col, char c, uint8_t attr)
{
    uint32_t px = col * FBCON_GLYPH_W;
    uint32_t py = row * FBCON_GLYPH_H;
    uint32_t fg = vga_palette[attr & 0x0F];
    uint32_t bg = vga_palette[(attr >> 4) & 0x0F];

    fb_fill_rect(px, py, FBCON_GLYPH_W, FBCON_GLYPH_H, bg);

    char text[2] = {c, '\0'};
    fb_draw_text(px, py, text, FBCON_SCALE, fg);
}

void fbcon_draw_cell_present(uint32_t row, uint32_t col, char c, uint8_t attr)
{
    fbcon_draw_cell(row, col, c, attr);
    fb_present_rect(col * FBCON_GLYPH_W, row * FBCON_GLYPH_H, FBCON_GLYPH_W, FBCON_GLYPH_H);
}

void fbcon_present(void)
{
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
}
