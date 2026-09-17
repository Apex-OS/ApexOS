// ApexKernel/Drivers/Display/VGA.c
// ApexOS -- VGA text mode driver (80x25) with scrollback

#include "Drivers/Display/VGA.h"
#include "Bus/PortIO.h"
#include "Bus/Serial/Serial.h"
#include "Devices/Serial/16550/Serial16550.h"
#include "Drivers/Display/Console/ConsoleCore.h"
#include "Drivers/Display/Console/ConsoleCursor.h"
#include "Drivers/Display/Console/ConsoleScrollback.h"
#include "Drivers/Display/Console/ConsoleRender.h"

// Set once by vga_enable_graphics_mode() (called from Core/Init.c
// after fb_init() succeeds) -- every public function below checks
// this first and forwards to the matching fbcon_* function when true.
// Function NAMES stay the same on purpose: ~200 call sites across the
// kernel already call vga_puts/vga_putchar/etc, and this flag lets
// all of them transparently render into the framebuffer instead of
// real VGA text-mode memory (0xB8000) without touching any of them.
// Stays false (real VGA text mode + serial mirror) if fb_init() never
// succeeds or is never called -- e.g. a machine with no usable VBE
// mode -- so text output always works one way or another.
static bool graphics_mode = false;

// Off by default -- vga_putchar()/fbcon_putchar() only mirror to
// serial when this is explicitly turned on (see
// vga_set_serial_mirror()). Was always-on before, which meant every
// character shown on screen (including the whole shell session) was
// also written out COM1 -- noisy for day-to-day use in a terminal
// attached to that port. Tools that actually need the serial mirror
// (a headless test runner reading serial output) can turn it on for
// the duration of that run.
static bool serial_mirror_enabled = false;

void vga_set_serial_mirror(bool enabled)
{
    serial_mirror_enabled = enabled;
}

bool vga_get_serial_mirror(void)
{
    return serial_mirror_enabled;
}

void vga_enable_graphics_mode(void)
{
    graphics_mode = true;
    fbcon_init();
    // Brings every line printed so far (real VGA text mode, about to
    // stop being read at all now that graphics_mode is true) into
    // this console's own scrollback -- otherwise it's gone the
    // instant fbcon_present() first draws over the physical screen,
    // not just off-screen but nowhere left to scroll back to either.
    // Must run after fbcon_init() above, which calls fbcon_clear() --
    // doing this first would just get wiped by that.
    fbcon_import_scrollback();
    fbcon_present();
}

bool vga_is_graphics_mode(void)
{
    return graphics_mode;
}

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
uint8_t vga_get_row(void) { return graphics_mode ? fbcon_get_row() : vga_row; }
uint8_t vga_get_col(void) { return graphics_mode ? fbcon_get_col() : vga_col; }
uint32_t vga_get_width(void) { return graphics_mode ? fbcon_get_width() : VGA_WIDTH; }
uint32_t vga_get_height(void) { return graphics_mode ? fbcon_get_height() : VGA_HEIGHT; }

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

static void vga_advance_line_internal(void)
{
    write_row++;
    if (total_lines < SCROLLBACK_LINES)
        total_lines++;
    scroll_offset = 0;
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
    if (graphics_mode)
    {
        fbcon_clear();
        return;
    }

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
    if (graphics_mode)
    {
        fbcon_set_color(fg, bg);
        return;
    }
    vga_attr = vga_make_attr(fg, bg);
}

void vga_putchar(char c)
{
    if (graphics_mode)
    {
        fbcon_putchar(c);
        return;
    }

    // Mirrors every character to COM1, only when explicitly turned on
    // (see vga_set_serial_mirror()) -- lets serial output (QEMU
    // `-serial file:...` or `-serial stdio`) capture exactly what
    // appears on screen, with no separate logging path to keep in
    // sync. Used by a headless test runner to check boot/test output
    // without a real display; off by default so a normal interactive
    // session doesn't spam whatever's attached to COM1.
    if (serial_mirror_enabled)
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
    if (graphics_mode)
    {
        fbcon_puts(str);
        return;
    }
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
    if (graphics_mode)
    {
        fbcon_hide_cursor();
        return;
    }
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

void vga_show_cursor(void)
{
    if (graphics_mode)
    {
        fbcon_show_cursor();
        return;
    }

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
    if (graphics_mode)
    {
        fbcon_set_cursor_pos(row, col);
        return;
    }
    vga_row = row;
    vga_col = col;
}

void vga_write_at(uint8_t row, uint8_t col, char c)
{
    if (graphics_mode)
    {
        fbcon_write_at(row, col, c);
        return;
    }

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
    if (graphics_mode)
    {
        fbcon_scroll_up(lines);
        return;
    }
    scroll_offset += lines;
    vga_present();
}

void vga_scroll_down(uint32_t lines)
{
    if (graphics_mode)
    {
        fbcon_scroll_down(lines);
        return;
    }
    if (lines > scroll_offset)
        scroll_offset = 0;
    else
        scroll_offset -= lines;
    vga_present();
}

bool vga_is_scrolled(void)
{
    if (graphics_mode)
        return fbcon_get_scroll_offset() != 0;
    return scroll_offset != 0;
}

void vga_advance_line(void)
{
    if (graphics_mode)
        fbcon_advance_line();
    else
        vga_advance_line_internal();
}

static uint32_t text_batch_start_total_lines = 0;

void vga_begin_batch(void)
{
    if (graphics_mode)
        fbcon_begin_batch();
    text_batch_start_total_lines = total_lines;
}

void vga_end_batch(void)
{
    if (graphics_mode)
        fbcon_end_batch();
}

uint32_t vga_get_scrollback_line_count(void)
{
    return total_lines;
}

bool vga_get_scrollback_cell(uint32_t line, uint32_t col, char *out_c, uint8_t *out_attr)
{
    if (line >= total_lines)
        return false;
    if (col >= VGA_WIDTH)
        return false;

    // Oldest line still physically present in the ring buffer -- see
    // this function's declaration in VGA.h for why lines older than
    // this are simply unavailable, not an error condition to handle
    // specially here.
    uint32_t oldest_available = (total_lines > SCROLLBACK_LINES) ? (total_lines - SCROLLBACK_LINES) : 0;
    if (line < oldest_available)
        return false;

    uint16_t entry = scrollback[line % SCROLLBACK_LINES][col];
    *out_c = (char)(entry & 0xFF);
    *out_attr = (uint8_t)(entry >> 8);
    return true;
}
