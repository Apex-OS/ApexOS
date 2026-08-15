// apex-kernel/drivers/fbcon.c
// ApexOS -- Framebuffer console (see fbcon.h for the full picture)

#include "fbcon.h"
#include "framebuffer.h"
#include "framebuffer_draw.h"
#include "font8x16.h"
#include "serial.h"

// Glyph cell size on screen -- FONT8X16_WIDTH/HEIGHT scaled up (see
// fb_draw_text's `scale` parameter). 1 (native size) chosen now that
// the font itself is the real VGA BIOS CP437 8x16 font (see
// font8x16.h) -- it's designed to be crisp at its native size, the
// way real VGA text mode always rendered it, so no scaling is needed
// the way font8x8.c's smaller glyphs needed 2x to stay readable at
// 1600x900. Gives a ~200x56 character grid at 1600x900 (see
// stage2.asm: MAX_FB_WIDTH/HEIGHT).
#define FBCON_SCALE 1
#define FBCON_GLYPH_W (FONT8X16_WIDTH * FBCON_SCALE)
#define FBCON_GLYPH_H (FONT8X16_HEIGHT * FBCON_SCALE)

// Scrollback depth, in lines -- same value as vga.c's SCROLLBACK_LINES
// for consistent history length regardless of which console is
// active. Grid width is capped at this file's FBCON_MAX_WIDTH rather
// than sized dynamically, so the scrollback array can be a flat
// static allocation (heap usage is already tight -- see mm/heap.h --
// and this avoids one more kmalloc'd structure to track).
#define FBCON_SCROLLBACK_LINES 500
#define FBCON_MAX_WIDTH 256 // headroom above the 200 cols 1600px/8px gives (font8x16.h, scale=1)

typedef struct
{
    char c;
    uint8_t attr; // low nibble = fg, high nibble = bg -- same packing as vga.c's vga_make_attr
} fbcon_cell_t;

static fbcon_cell_t scrollback[FBCON_SCROLLBACK_LINES][FBCON_MAX_WIDTH];
static uint32_t total_lines = 1;
static uint32_t write_row = 0;
static uint32_t scroll_offset = 0;

static uint32_t grid_width = 0;
static uint32_t grid_height = 0;
static uint8_t cur_col = 0;
static uint32_t cur_row = 0; // on-screen write row (0..grid_height-1)
static uint8_t cur_attr = 0;
static bool cursor_hidden = false;

// 16-color VGA palette, standard RGB values (same as most VGA text
// mode implementations/terminal emulators default to) -- used to
// translate vga_color_t into an FB_RGB() pixel value for fb_draw_text.
static const uint32_t vga_palette[16] = {
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

static inline uint8_t fbcon_make_attr(vga_color_t fg, vga_color_t bg)
{
    return (uint8_t)(fg | (bg << 4));
}

uint32_t fbcon_get_width(void) { return grid_width; }
uint32_t fbcon_get_height(void) { return grid_height; }
uint8_t fbcon_get_row(void) { return (uint8_t)cur_row; }
uint8_t fbcon_get_col(void) { return cur_col; }

// Draws one character cell at grid position (row, col): background
// fill first (so glyphs with descenders/gaps show the right bg, same
// as fb_draw_text's "background left untouched" contract needing an
// explicit clear underneath), then the glyph on top.
static void fbcon_draw_cell(uint32_t row, uint32_t col, char c, uint8_t attr)
{
    uint32_t px = col * FBCON_GLYPH_W;
    uint32_t py = row * FBCON_GLYPH_H;
    uint32_t fg = vga_palette[attr & 0x0F];
    uint32_t bg = vga_palette[(attr >> 4) & 0x0F];

    fb_fill_rect(px, py, FBCON_GLYPH_W, FBCON_GLYPH_H, bg);

    char text[2] = {c, '\0'};
    fb_draw_text(px, py, text, FBCON_SCALE, fg);
}

// Same as fbcon_draw_cell(), but also pushes just this cell's pixels
// to VRAM (fb_present_rect(), not the full-screen fb_present()) --
// the fast path fbcon_putchar() uses for a single changed character,
// so typing doesn't visibly lag waiting for a full-screen copy on
// every keystroke.
static void fbcon_draw_cell_present(uint32_t row, uint32_t col, char c, uint8_t attr)
{
    fbcon_draw_cell(row, col, c, attr);
    fb_present_rect(col * FBCON_GLYPH_W, row * FBCON_GLYPH_H, FBCON_GLYPH_W, FBCON_GLYPH_H);
}

// Draws a solid block cursor at the given grid position -- no
// hardware cursor exists in graphics mode (unlike vga.c's port
// 0x3D4/0x3D5 blinking block), so this just inverts the cell's
// colors, redrawn each fbcon_present().
static void fbcon_draw_cursor(void)
{
    if (cursor_hidden)
        return;
    if (scroll_offset > 0)
        return; // matches vga.c: cursor hidden while scrolled into history

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

// Same as fbcon_draw_cursor(), but also pushes just the cursor's cell
// to VRAM -- the fast path fbcon_putchar() uses, symmetric with
// fbcon_draw_cell_present() above.
static void fbcon_draw_cursor_present(void)
{
    fbcon_draw_cursor();
    if (cursor_hidden || scroll_offset > 0)
        return; // fbcon_draw_cursor() didn't draw anything -- nothing to present
    fb_present_rect(cur_col * FBCON_GLYPH_W, cur_row * FBCON_GLYPH_H, FBCON_GLYPH_W, FBCON_GLYPH_H);
}

// Redraws the entire visible grid from scrollback, then the cursor,
// then pushes to VRAM. Called after every write, same as vga.c's
// vga_present() -- simplest correct approach; a dirty-line tracker
// would cut redundant work if this ever becomes a bottleneck (each
// frame here is at most grid_width*grid_height glyphs, not the whole
// pixel framebuffer, so it's far cheaper than gfxtest's old
// whole-scene redraw was).
static void fbcon_present(void)
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

static void fbcon_new_line(void)
{
    write_row++;
    if (total_lines < FBCON_SCROLLBACK_LINES)
        total_lines++;

    uint32_t line = write_row % FBCON_SCROLLBACK_LINES;
    uint8_t default_blank_attr = fbcon_make_attr(VGA_WHITE, VGA_BLACK);
    for (uint32_t c = 0; c < grid_width; c++)
    {
        scrollback[line][c].c = ' ';
        scrollback[line][c].attr = default_blank_attr;
    }

    scroll_offset = 0;
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
    fbcon_present();
}

void fbcon_set_color(vga_color_t fg, vga_color_t bg)
{
    cur_attr = fbcon_make_attr(fg, bg);
}

void fbcon_putchar(char c)
{
    // Mirrors to serial too, only when explicitly turned on -- see
    // vga_set_serial_mirror() in vga.c, which owns this flag (shared
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
        // shifted) needs the full grid redraw -- otherwise every line
        // below the new one is unchanged, so just draw the newly
        // exposed blank line + move the cursor there. This is the
        // difference between ~5600 cells/newline (unusable under
        // QEMU at -O0 -- confirmed via GDB, boot stalling for tens of
        // seconds per log line) and 1 cell.
        if (was_scrolled)
        {
            fbcon_present();
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
            fbcon_present();
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
    // fb_present_rect() in framebuffer.c).
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

void fbcon_scroll_up(uint32_t lines)
{
    scroll_offset += lines;
    fbcon_present();
}

void fbcon_scroll_down(uint32_t lines)
{
    if (lines > scroll_offset)
        scroll_offset = 0;
    else
        scroll_offset -= lines;
    fbcon_present();
}