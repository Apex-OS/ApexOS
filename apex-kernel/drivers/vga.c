// apex-kernel/drivers/vga.c
// ApexOS -- VGA text mode driver (80x25) with scrollback

#include "vga.h"
#include "../include/apex/io.h"
#include "serial.h"

// All writes go into an in-RAM history buffer (SCROLLBACK_LINES
// lines) instead of writing directly to VGA memory. VGA memory only
// displays a VGA_HEIGHT-line window, copied from the buffer at the
// current scroll offset. scroll_offset=0 means bottom (live), >0
// means scrolled back into history.
#define SCROLLBACK_LINES 500
static uint16_t scrollback[SCROLLBACK_LINES][VGA_WIDTH];
static uint32_t total_lines = 1;   // number of lines used so far (>= 1)
static uint32_t write_row = 0;     // current write line in scrollback
static uint32_t scroll_offset = 0; // 0 = live, N = scrolled back N lines

static uint16_t *vga_hw = VGA_MEMORY; // real VGA memory (display only)
static uint8_t vga_col = 0;
static uint8_t vga_row = 0; // on-screen write row (0..VGA_HEIGHT-1); matches write_row when scroll_offset=0
static uint8_t vga_attr = 0;
uint8_t vga_get_row(void) { return vga_row; }
uint8_t vga_get_col(void) { return vga_col; }

static inline uint8_t vga_make_attr(vga_color_t fg, vga_color_t bg)
{
    return (uint8_t)(fg | (bg << 4));
}

static inline uint16_t vga_make_entry(char c, uint8_t attr)
{
    return (uint16_t)(uint8_t)c | ((uint16_t)attr << 8);
}

// Copies the visible window (determined by scroll_offset) from
// scrollback into real VGA memory. Called after every write or every
// change to scroll_offset.
static void vga_present(void)
{
    uint32_t first = 0;
    if (total_lines > VGA_HEIGHT)
        first = total_lines - VGA_HEIGHT;
    if (scroll_offset > first)
        scroll_offset = first; // don't scroll back past the start
    first -= scroll_offset;

    for (uint8_t r = 0; r < VGA_HEIGHT; r++)
    {
        uint32_t src_line = first + r;
        for (uint8_t c = 0; c < VGA_WIDTH; c++)
        {
            vga_hw[r * VGA_WIDTH + c] =
                (src_line < total_lines) ? scrollback[src_line % SCROLLBACK_LINES][c]
                                         : vga_make_entry(' ', (VGA_WHITE | (VGA_BLACK << 4)));
        }
    }

    if (scroll_offset > 0)
        vga_hide_cursor();
    else
        vga_show_cursor();
}

// Advances one line in the scrollback buffer -- called on every new
// line (each '\n' or automatic wrap at end of column), whether the
// visible screen is already full or not.
//
// write_row must advance here unconditionally: an earlier version
// only advanced it from within the old scroll-triggered path, which
// only ran once vga_row exceeded VGA_HEIGHT. Every line typed within
// the first screenful wrote to scrollback[0], overwriting the
// previous one instead of appending.
static void vga_new_line(void)
{
    write_row++;
    if (total_lines < SCROLLBACK_LINES)
        total_lines++;

    uint32_t line = write_row % SCROLLBACK_LINES;

    // Blanks the new line with a fixed default attribute (white on
    // black) rather than the current vga_attr -- otherwise a line
    // cleared while an error message is mid-print (red text) would
    // blank using red, discoloring unrelated blank space below it.
    uint8_t default_blank_attr = (VGA_WHITE | (VGA_BLACK << 4));

    for (uint8_t c = 0; c < VGA_WIDTH; c++)
        scrollback[line][c] = vga_make_entry(' ', default_blank_attr);

    scroll_offset = 0; // any new write jumps back to live
}

void vga_init(void)
{
    vga_attr = vga_make_attr(VGA_WHITE, VGA_BLACK);
    vga_col = 0;
    vga_row = 0;
    vga_clear();
}

void vga_clear(void)
{
    for (uint32_t i = 0; i < SCROLLBACK_LINES; i++)
        for (uint8_t c = 0; c < VGA_WIDTH; c++)
            scrollback[i][c] = vga_make_entry(' ', vga_attr);

    total_lines = 1;
    write_row = 0;
    scroll_offset = 0;
    vga_col = 0;
    vga_row = 0;
    vga_present();
}

void vga_set_color(vga_color_t fg, vga_color_t bg)
{
    vga_attr = vga_make_attr(fg, bg);
}

void vga_putchar(char c)
{
    // Mirrors every character to COM1 -- lets serial output (QEMU
    // `-serial file:...` or `-serial stdio`) capture exactly what
    // appears on screen, with no separate logging path to keep in
    // sync. Used by tools/tests/run_tests.sh to check boot/test output
    // without a real display. serial_init() must have run first (see
    // kernel.c) -- called well before the earliest vga_putchar() in
    // practice, but writing to an uninitialized COM1 before that is
    // harmless (the UART ignores writes if not configured) so no
    // ordering guard is needed here.
    serial_putchar(c);

    if (c == '\n')
    {
        vga_col = 0;
        vga_new_line();
        if (vga_row < VGA_HEIGHT - 1)
            vga_row++;
        // Otherwise: already at the bottom of the visible screen, stay
        // there (scrollback still advances via vga_new_line() above --
        // only the visible window stops moving, matching how a
        // terminal scrolls).
        vga_present();
        return;
    }

    if (c == '\r')
    {
        vga_col = 0;
        return;
    }

    if (c == '\t')
    {
        uint8_t spaces = 4 - (vga_col % 4);
        for (uint8_t i = 0; i < spaces; i++)
            vga_putchar(' ');
        return;
    }

    if (c == '\b')
    {
        if (vga_col > 0)
            vga_col--;
        else if (vga_row > 0)
        {
            vga_row--;
            vga_col = VGA_WIDTH - 1;
        }
        vga_present();
        return;
    }

    uint32_t line = write_row % SCROLLBACK_LINES;
    scrollback[line][vga_col] = vga_make_entry(c, vga_attr);
    vga_col++;

    if (vga_col >= VGA_WIDTH)
    {
        vga_col = 0;
        vga_new_line();
        if (vga_row < VGA_HEIGHT - 1)
            vga_row++;
    }
}

void vga_puts(const char *str)
{
    while (*str)
        vga_putchar(*str++);
    vga_present();
}

void vga_puthex(uint64_t value)
{
    const char *hex = "0123456789ABCDEF";
    char buf[19];
    int i = 18;
    buf[i--] = '\0';
    if (value == 0)
        buf[i--] = '0';
    else
    {
        while (value && i > 1)
        {
            buf[i--] = hex[value & 0xF];
            value >>= 4;
        }
    }
    buf[i--] = 'x';
    buf[i] = '0';
    vga_puts(&buf[i]);
    vga_present();
}

void vga_putdec(uint64_t value)
{
    char buf[21];
    int i = 20;
    buf[i--] = '\0';
    if (value == 0)
    {
        buf[i] = '0';
        vga_puts(&buf[i]);
        return;
    }
    while (value && i >= 0)
    {
        buf[i--] = '0' + (value % 10);
        value /= 10;
    }
    vga_puts(&buf[i + 1]);
    vga_present();
}

void vga_hide_cursor(void)
{
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

void vga_show_cursor(void)
{
    uint16_t pos = vga_row * VGA_WIDTH + vga_col;

    // Re-enables the hardware cursor (restores the standard 13-15
    // scanline range).
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 0x0D); // start scanline
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 0x0E); // end scanline

    // Updates its position.
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_set_cursor_pos(uint8_t row, uint8_t col)
{
    vga_row = row;
    vga_col = col;
}

void vga_write_at(uint8_t row, uint8_t col, char c)
{
    if (row >= VGA_HEIGHT || col >= VGA_WIDTH)
        return;

    uint32_t first = 0;
    if (total_lines > VGA_HEIGHT)
        first = total_lines - VGA_HEIGHT;
    first -= scroll_offset;
    uint32_t line = (first + row) % SCROLLBACK_LINES;
    scrollback[line][col] = vga_make_entry(c, vga_attr);
    vga_present();
}

// ── Scrollback: mouse wheel API ─────────────────────────────────────────
void vga_scroll_up(uint32_t lines)
{
    scroll_offset += lines;
    vga_present();
}

void vga_scroll_down(uint32_t lines)
{
    if (lines > scroll_offset)
        scroll_offset = 0;
    else
        scroll_offset -= lines;
    vga_present();
}