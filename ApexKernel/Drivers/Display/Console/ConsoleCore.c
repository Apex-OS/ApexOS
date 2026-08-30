// ApexKernel/Drivers/Display/Console/ConsoleCore.c
// ApexOS -- Framebuffer console (see ConsoleCore.h for the full picture)

#include "Drivers/Display/Console/ConsoleCore.h"
#include "Drivers/Display/Console/Internal.h"
#include "Drivers/Display/Console/ConsoleRender.h"
#include "Drivers/Display/Console/ConsoleScrollback.h"
#include "Drivers/Display/Console/ConsoleCursor.h"
#include "Drivers/Display/Framebuffer/Framebuffer.h"
#include "Bus/Serial/Serial.h"
#include "Devices/Serial/16550/Serial16550.h"

fbcon_cell_t scrollback[FBCON_SCROLLBACK_LINES][FBCON_MAX_WIDTH];
uint32_t total_lines = 1;
uint32_t write_row = 0;
uint32_t scroll_offset = 0;

uint32_t grid_width = 0;
uint32_t grid_height = 0;
uint8_t cur_col = 0;
uint32_t cur_row = 0; // on-screen write row (0..grid_height-1)
uint8_t cur_attr = 0;
bool cursor_hidden = false;
bool present_suppressed = false;

const uint32_t vga_palette[16] = {
    FB_RGB(0, 0, 0),       // VGA_BLACK
    FB_RGB(0, 0, 170),     // VGA_BLUE
    FB_RGB(0, 170, 0),     // VGA_GREEN
    FB_RGB(0, 170, 170),   // VGA_CYAN
    FB_RGB(170, 0, 0),     // VGA_RED
    FB_RGB(170, 0, 170),   // VGA_MAGENTA
    FB_RGB(170, 85, 0),    // VGA_BROWN
    FB_RGB(170, 170, 170), // VGA_LIGHT_GREY
    FB_RGB(85, 85, 85),    // VGA_DARK_GREY
    FB_RGB(85, 85, 255),   // VGA_LIGHT_BLUE
    FB_RGB(85, 255, 85),   // VGA_LIGHT_GREEN
    FB_RGB(85, 255, 255),  // VGA_LIGHT_CYAN
    FB_RGB(255, 85, 85),   // VGA_LIGHT_RED
    FB_RGB(255, 85, 255),  // VGA_LIGHT_MAGENTA
    FB_RGB(255, 255, 85),  // VGA_YELLOW
    FB_RGB(255, 255, 255), // VGA_WHITE
};

uint32_t fbcon_get_width(void) { return grid_width; }
uint32_t fbcon_get_height(void) { return grid_height; }
uint8_t fbcon_get_row(void) { return (uint8_t)cur_row; }
uint8_t fbcon_get_col(void) { return cur_col; }
uint32_t fbcon_get_scroll_offset(void) { return scroll_offset; }
// See fbcon_begin_batch()/fbcon_end_batch() below.
static uint32_t batch_start_total_lines = 0;

void fbcon_begin_batch(void)
{
    present_suppressed = true;
    batch_start_total_lines = total_lines;
}

void fbcon_end_batch(void)
{
    present_suppressed = false;

    // Only forces the full-grid redraw if the batch actually scrolled
    // (total_lines grew past what it was, and enough to have crossed
    // grid_height at some point) -- individual characters/lines were
    // already drawn incrementally via fbcon_draw_cell_present() while
    // suppressed (that path isn't gated by present_suppressed), so a
    // short command that never scrolled needs no extra redraw here.
    if (total_lines != batch_start_total_lines && total_lines >= grid_height)
        fbcon_present();
}

void fbcon_init(void)
{
    fb_info_t info = fb_get_info();
    grid_width = info.width / FBCON_GLYPH_W;
    grid_height = info.height / FBCON_GLYPH_H;

    if (grid_width > FBCON_MAX_WIDTH)
        grid_width = FBCON_MAX_WIDTH; // clamp -- see FBCON_MAX_WIDTH's comment

    cur_attr = fbcon_make_attr(VGA_WHITE, VGA_BLACK);
    cur_col = 0;
    cur_row = 0;
    cursor_hidden = false;
    fbcon_clear();
}

void fbcon_clear(void)
{
    uint8_t blank_attr = fbcon_make_attr(VGA_WHITE, VGA_BLACK);
    for (uint32_t i = 0; i < FBCON_SCROLLBACK_LINES; i++)
        for (uint32_t c = 0; c < grid_width; c++)
        {
            scrollback[i][c].c = ' ';
            scrollback[i][c].attr = blank_attr;
        }

    total_lines = 1;
    write_row = 0;
    scroll_offset = 0;
    cur_col = 0;
    cur_row = 0;
    fb_clear(vga_palette[VGA_BLACK]);
    fbcon_present_force();
}

void fbcon_set_color(vga_color_t fg, vga_color_t bg)
{
    cur_attr = fbcon_make_attr(fg, bg);
}

void fbcon_putchar(char c)
{
    // Mirrors to serial too, only when explicitly turned on -- see
    // vga_set_serial_mirror() in VGA.c, which owns this flag (shared
    // with vga_putchar() so both text-output paths behave the same
    // way regardless of which one is active).
    if (vga_get_serial_mirror())
        serial_putchar(c);

    if (c == '\n')
    {
        uint32_t old_cursor_row = cur_row;
        uint32_t old_cursor_col = cur_col;

        cur_col = 0;
        bool was_scrolled = (total_lines >= grid_height);
        fbcon_new_line();
        if (cur_row < grid_height - 1)
            cur_row++;

        // Only a real scroll (the visible window's content actually
        // shifted) needs a scroll redraw -- otherwise every line
        // below the new one is unchanged, so just draw the newly
        // exposed blank line + move the cursor there. This is the
        // difference between ~5600 cells/newline (unusable under
        // QEMU at -O0 -- confirmed via GDB, boot stalling for tens of
        // seconds per log line) and 1 cell.
        //
        // When it did scroll: if no manual scrollback offset is
        // active, this is the single most common case (typing/output
        // at the bottom of a full screen) and gets the fast path --
        // fb_scroll_up()'s memmove instead of redrawing all
        // grid_width * grid_height cells from scrollback. A nonzero
        // scroll_offset means the user has scrolled back through
        // history, so the visible window isn't a simple "shift up by
        // one" relative to what's already on screen; fall back to the
        // full redraw in that case.
        if (was_scrolled)
        {
            if (scroll_offset == 0)
            {
                if (!present_suppressed)
                {
                    // Erase the cursor at its old position (the
                    // bottom row as it was just before this newline)
                    // in real, non-inverted colors before the fast
                    // scroll path's memmove runs -- otherwise the
                    // inverted cursor pixels get dragged up along
                    // with everything else and land on a row this
                    // fast path never repaints, leaving a permanent
                    // trail. write_row - 1 (not write_row, which
                    // fbcon_new_line() already blanked above) is the
                    // line that was actually on screen with the
                    // cursor on it -- same lookup the non-scroll
                    // branch below uses for the same reason.
                    fbcon_cell_t *old_cell = &scrollback[(write_row - 1) % FBCON_SCROLLBACK_LINES][old_cursor_col];
                    fbcon_draw_cell(old_cursor_row, old_cursor_col,
                                    old_cell->c ? old_cell->c : ' ', old_cell->attr);
                    fbcon_present_scroll_one_line();
                }
            }
            else
            {
                fbcon_present();
            }
        }
        else
        {
            // Erases the cursor's old cell (end of the previous line)
            // first -- otherwise it's left showing as a solid inverted
            // block there permanently, since nothing else repaints it
            // once fb_present_rect() stops copying the whole screen.
            fbcon_cell_t *old_cell = &scrollback[(write_row - 1) % FBCON_SCROLLBACK_LINES][old_cursor_col];
            fbcon_draw_cell_present(old_cursor_row, old_cursor_col,
                                    old_cell->c ? old_cell->c : ' ', old_cell->attr);

            fbcon_draw_cell(cur_row, cur_col, ' ', fbcon_make_attr(VGA_WHITE, VGA_BLACK));
            fb_present_rect(cur_col * FBCON_GLYPH_W, cur_row * FBCON_GLYPH_H, FBCON_GLYPH_W, FBCON_GLYPH_H);
            fbcon_draw_cursor_present();
        }
        return;
    }

    if (c == '\r')
    {
        cur_col = 0;
        return;
    }

    if (c == '\t')
    {
        uint8_t spaces = 4 - (cur_col % 4);
        for (uint8_t i = 0; i < spaces; i++)
            fbcon_putchar(' ');
        return;
    }

    if (c == '\b')
    {
        // Erases the cursor at its current cell before moving it back
        // -- fbcon_draw_cursor_present() below will redraw it
        // (inverted) at the new position; without this the old cell
        // is left in its inverted state permanently, since nothing
        // else repaints it.
        fbcon_draw_cell(cur_row, cur_col,
                        scrollback[write_row % FBCON_SCROLLBACK_LINES][cur_col].c
                            ? scrollback[write_row % FBCON_SCROLLBACK_LINES][cur_col].c
                            : ' ',
                        scrollback[write_row % FBCON_SCROLLBACK_LINES][cur_col].attr);
        fb_present_rect(cur_col * FBCON_GLYPH_W, cur_row * FBCON_GLYPH_H, FBCON_GLYPH_W, FBCON_GLYPH_H);

        if (cur_col > 0)
            cur_col--;
        else if (cur_row > 0)
        {
            cur_row--;
            cur_col = (uint8_t)(grid_width - 1);
        }
        fbcon_draw_cursor_present();
        return;
    }

    uint32_t line = write_row % FBCON_SCROLLBACK_LINES;
    scrollback[line][cur_col].c = c;
    scrollback[line][cur_col].attr = cur_attr;

    // Draws just this one cell and pushes just that rect to VRAM --
    // see the '\n' case above for why: redrawing the whole grid (and
    // copying the whole framebuffer) per character was the actual
    // bottleneck, not anything about the rendering itself. This also
    // erases the cursor if it was sitting on this exact cell (typing
    // over it draws the real character in normal colors, replacing
    // whatever inverted cursor block was there).
    fbcon_draw_cell_present(cur_row, cur_col, c, cur_attr);
    uint32_t old_cursor_row = cur_row;
    uint32_t old_cursor_col = cur_col;
    cur_col++;

    if (cur_col >= grid_width)
    {
        cur_col = 0;
        bool was_scrolled = (total_lines >= grid_height);
        fbcon_new_line();
        if (cur_row < grid_height - 1)
            cur_row++;
        if (was_scrolled)
        {
            // Same fast-path condition as the '\n' handler above: a
            // plain scroll (no manual scrollback offset active) uses
            // fb_scroll_up()'s memmove instead of a full grid redraw.
            // Also gated on present_suppressed, same as fbcon_present()
            // itself -- during a batch (fbcon_begin_batch()), nothing
            // should touch VRAM until fbcon_end_batch()'s single
            // full-screen redraw, or a multi-line command would
            // flash each intermediate scroll to the screen instead of
            // updating atomically once at the end.
            if (scroll_offset == 0)
            {
                if (!present_suppressed)
                    fbcon_present_scroll_one_line();
            }
            else
            {
                fbcon_present();
            }
            return;
        }
    }

    // Cursor moved to a new cell -- repaint the old one in its real
    // (non-inverted) colors first, or it's left permanently showing
    // as a solid block. The character just written there (above)
    // already handles the col==old_cursor_col case; this covers
    // moving to a new row (cur_col wrapped to 0) or the initial
    // whitespace cells the cursor passes over without ever being
    // "typed on".
    if (old_cursor_row != cur_row || old_cursor_col != cur_col)
    {
        fbcon_cell_t *old_cell = &scrollback[write_row % FBCON_SCROLLBACK_LINES][old_cursor_col];
        fbcon_draw_cell_present(old_cursor_row, old_cursor_col,
                                old_cell->c ? old_cell->c : ' ', old_cell->attr);
    }

    fbcon_draw_cursor_present();
}

void fbcon_puts(const char *str)
{
    while (*str)
        fbcon_putchar(*str++);
}

void fbcon_set_cursor_pos(uint8_t row, uint8_t col)
{
    if (row == cur_row && col == cur_col)
        return; // nothing moved, nothing to redraw

    // Erases the cursor at its old position first (redraws that
    // cell's real, non-inverted content) -- otherwise it's left
    // showing as a solid inverted block there permanently once
    // nothing else repaints the whole screen per keystroke (see
    // fb_present_rect() in Framebuffer.c).
    if (cur_row < grid_height && cur_col < grid_width)
    {
        uint32_t first = 0;
        if (total_lines > grid_height)
            first = total_lines - grid_height;
        first -= scroll_offset;
        fbcon_cell_t *old_cell = &scrollback[(first + cur_row) % FBCON_SCROLLBACK_LINES][cur_col];
        fbcon_draw_cell_present(cur_row, cur_col, old_cell->c ? old_cell->c : ' ', old_cell->attr);
    }

    cur_row = row;
    cur_col = col;
    fbcon_draw_cursor_present();
}

void fbcon_write_at(uint8_t row, uint8_t col, char c)
{
    if (row >= grid_height || col >= grid_width)
        return;

    uint32_t first = 0;
    if (total_lines > grid_height)
        first = total_lines - grid_height;
    first -= scroll_offset;
    uint32_t line = (first + row) % FBCON_SCROLLBACK_LINES;
    scrollback[line][col].c = c;
    scrollback[line][col].attr = cur_attr;
    fbcon_draw_cell(row, col, c, cur_attr);
    fb_present_rect(col * FBCON_GLYPH_W, row * FBCON_GLYPH_H, FBCON_GLYPH_W, FBCON_GLYPH_H);
}
