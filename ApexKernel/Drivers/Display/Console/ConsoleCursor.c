// ApexKernel/Drivers/Display/Console/ConsoleCursor.c
// ApexOS -- Framebuffer console cursor rendering

#include "Drivers/Display/Console/ConsoleCursor.h"
#include "Drivers/Display/Console/Internal.h"
#include "Drivers/Display/Console/ConsoleRender.h"
#include "Drivers/Display/Framebuffer/Framebuffer.h"

void fbcon_draw_cursor(void)
{
    if (cursor_hidden)
        return;
    if (scroll_offset > 0)
        return; // matches VGA.c: cursor hidden while scrolled into history

    uint32_t first = 0;
    if (total_lines > grid_height)
        first = total_lines - grid_height;
    uint32_t visible_row = cur_row; // cur_row already tracks the visible window position, see fbcon_new_line

    fbcon_cell_t *cell = &scrollback[(first + visible_row) % FBCON_SCROLLBACK_LINES][cur_col];
    // Inverted: bg becomes fg and vice versa -- a simple, cheap way to
    // show the cursor without a separate overlay/save-restore pair
    // (the whole visible grid is already redrawn every fbcon_present()
    // call, so there's no flicker/tearing concern here the way there
    // was for the mouse cursor over a static scene).
    fbcon_draw_cell(cur_row, cur_col, cell->c ? cell->c : ' ',
                    fbcon_make_attr((vga_color_t)((cell->attr >> 4) & 0x0F),
                                    (vga_color_t)(cell->attr & 0x0F)));
}

void fbcon_draw_cursor_present(void)
{
    fbcon_draw_cursor();
    if (cursor_hidden || scroll_offset > 0)
        return; // fbcon_draw_cursor() didn't draw anything -- nothing to present
    fb_present_rect(cur_col * FBCON_GLYPH_W, cur_row * FBCON_GLYPH_H, FBCON_GLYPH_W, FBCON_GLYPH_H);
}

void fbcon_hide_cursor(void)
{
    cursor_hidden = true;
    fbcon_present();
}

void fbcon_show_cursor(void)
{
    cursor_hidden = false;
    fbcon_present();
}
