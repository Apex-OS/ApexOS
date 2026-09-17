// ApexKernel/Includes/Drivers/Display/VGA.h
// ApexOS -- VGA text mode driver (80x25)

#ifndef APEX_DRIVERS_DISPLAY_VGA_H
#define APEX_DRIVERS_DISPLAY_VGA_H

#include "Core/Types.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((uint16_t *)0xB8000)

typedef enum
{
    VGA_BLACK = 0,
    VGA_BLUE = 1,
    VGA_GREEN = 2,
    VGA_CYAN = 3,
    VGA_RED = 4,
    VGA_MAGENTA = 5,
    VGA_BROWN = 6,
    VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8,
    VGA_LIGHT_BLUE = 9,
    VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11,
    VGA_LIGHT_RED = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW = 14,
    VGA_WHITE = 15,
} vga_color_t;

void vga_init(void);

// Switches every vga_* function below to render through
// Drivers/Display/Console/ instead of real VGA text-mode memory
// (0xB8000). Call once, from Core/Init.c, after fb_init() has
// succeeded -- calling this before fb_init() succeeded, or on a
// machine where it never does, would forward into an
// uninitialized/nonexistent framebuffer. One-way in v1: no matching
// vga_disable_graphics_mode(), since nothing currently needs to
// switch back once graphics mode is up.
void vga_enable_graphics_mode(void);

// Returns true if vga_enable_graphics_mode() has been called and is
// currently active -- lets callers that care (e.g. SystemInfo.c)
// report which console is actually in use.
bool vga_is_graphics_mode(void);

// Controls whether every character shown via vga_putchar()/
// fbcon_putchar() is also mirrored to COM1 (see Bus/Serial/Serial.h).
// Off by default -- a normal interactive session no longer spams
// whatever's attached to the serial port. Turn this on for tooling
// that specifically needs it (e.g. a headless test runner reading
// output from the serial line).
void vga_set_serial_mirror(bool enabled);
bool vga_get_serial_mirror(void);

void vga_clear(void);
void vga_set_color(vga_color_t fg, vga_color_t bg);
void vga_putchar(char c);
void vga_puts(const char *str);
void vga_puthex(uint64_t value);
void vga_putdec(uint64_t value);
void vga_hide_cursor(void);

// Re-enables the hardware cursor and syncs its position to
// vga_row/vga_col. Called internally by vga_present() whenever
// scroll_offset returns to 0 (live) -- exposed here for completeness,
// since vga_hide_cursor() (its counterpart) is also public.
void vga_show_cursor(void);

uint8_t vga_get_row(void);
uint8_t vga_get_col(void);
uint32_t vga_get_width(void);
uint32_t vga_get_height(void);

// Moves the logical cursor (vga_row/vga_col) without writing
// anything. Used by the shell to reposition the cursor during command
// line navigation (left/right arrows).
void vga_set_cursor_pos(uint8_t row, uint8_t col);

// Writes a character at a specific position WITHOUT advancing the
// cursor or handling scroll/newline/tab/backspace (unlike
// vga_putchar). Used by the shell to redraw a command line without
// disturbing the logical cursor position.
void vga_write_at(uint8_t row, uint8_t col, char c);

// Scrollback: scrolls back/forward `lines` lines through history.
void vga_scroll_up(uint32_t lines);
void vga_scroll_down(uint32_t lines);
// True if the view is currently scrolled up into history (not showing
// the live/bottom of the buffer).
bool vga_is_scrolled(void);
void vga_advance_line(void);
void vga_begin_batch(void);
void vga_end_batch(void);

// ── Text-mode scrollback export (for the framebuffer console switch) ──────
// Every boot log line printed before vga_enable_graphics_mode() is
// called (Core/Init.c: GDT/IDT/PIC/PMM/VMM/heap/ACPI, all still real
// VGA text mode at that point) lives only in VGA.c's own internal
// scrollback[][] buffer -- vga_enable_graphics_mode()'s mode switch
// wipes the physical VGA text-mode screen the instant it succeeds
// (see that function's own comment), so without exporting this
// history first, all of it is genuinely gone: not just off-screen,
// but nowhere to scroll back to either, in either the old text-mode
// buffer (about to stop being read at all) or the new framebuffer
// console's own scrollback (which starts out empty).
//
// vga_get_scrollback_line_count() returns how many logical lines have
// ever been written (Core/Init.c calls this once, right after
// switching, to know how far back to read). vga_get_scrollback_cell()
// reads a single cell from logical line `line` (0 = oldest line still
// held -- see this function's own bounds comment -- up to
// vga_get_scrollback_line_count()-1 = most recent), column `col`
// (0..vga_get_width()-1). Returns false (leaving *out_c/*out_attr
// untouched) if `line` is out of range -- either >= the line count
// above, or older than what's still retained (VGA.c's scrollback[][]
// is a fixed-size ring buffer -- see that file's SCROLLBACK_LINES --
// so very early boot output is only available at all if total output
// before the switch stayed under that many lines, otherwise oldest
// lines are already gone by the time this could ever be called).
uint32_t vga_get_scrollback_line_count(void);
bool vga_get_scrollback_cell(uint32_t line, uint32_t col, char *out_c, uint8_t *out_attr);

#endif // APEX_DRIVERS_DISPLAY_VGA_H
