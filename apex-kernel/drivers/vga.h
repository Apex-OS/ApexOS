// apex-kernel/drivers/vga.h
// ApexOS -- VGA text mode driver (80x25)

#ifndef APEX_VGA_H
#define APEX_VGA_H

#include "../include/apex/types.h"

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

// Switches every vga_* function below to render through fbcon.c (see
// drivers/fbcon.h) instead of real VGA text-mode memory (0xB8000).
// Call once, from kernel.c, after fb_init() has succeeded -- calling
// this before fb_init() succeeded, or on a machine where it never
// does, would forward into an uninitialized/nonexistent framebuffer.
// One-way in v1: no matching vga_disable_graphics_mode(), since
// nothing currently needs to switch back once graphics mode is up.
void vga_enable_graphics_mode(void);

// Returns true if vga_enable_graphics_mode() has been called and is
// currently active -- lets callers that care (e.g. print_sysinfo() in
// kernel.c) report which console is actually in use.
bool vga_is_graphics_mode(void);

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

// Moves the logical cursor (vga_row/vga_col) without writing
// anything. Used by the shell to reposition the cursor during command
// line navigation (left/right arrows).
void vga_set_cursor_pos(uint8_t row, uint8_t col);

// Writes a character at a specific position WITHOUT advancing the
// cursor or handling scroll/newline/tab/backspace (unlike
// vga_putchar). Used by the shell to redraw a command line without
// disturbing the logical cursor position.
void vga_write_at(uint8_t row, uint8_t col, char c);

// Scrollback (see vga.c): scrolls back/forward `lines` lines through
// history.
void vga_scroll_up(uint32_t lines);
void vga_scroll_down(uint32_t lines);

#endif // APEX_VGA_H