// apex-kernel/drivers/fbcon.h
// ApexOS -- Framebuffer console: renders a text-mode-like terminal
// (character grid, scrollback, colors, cursor) on top of the VESA/VBE
// framebuffer (see drivers/framebuffer.h), instead of the real VGA
// text-mode hardware (0xB8000).
//
// This exists so the kernel's existing text output (klog, shell,
// AXFS/AXF logging -- ~200 call sites across the codebase) shows up
// in the QEMU window when booted in graphics mode, without changing
// any of those call sites. drivers/vga.c is the integration point:
// its public functions (vga_puts, vga_putchar, etc.) check a
// graphics-mode flag and forward to this file's fbcon_* functions
// when set, otherwise they behave exactly as before (real VGA text
// mode) -- see vga.c for that wiring. This file has no callers
// outside vga.c; nothing else should call fbcon_* directly.
//
// Deliberately mirrors vga.c's internal design (scrollback buffer,
// write_row/vga_row split, \n/\r/\t/\b handling) so the two behave
// identically from the caller's point of view -- the only real
// difference is what happens at "present" time: vga.c copies
// characters into 0xB8000, this copies rendered glyphs into the
// framebuffer back-buffer via fb_draw_text/fb_fill_rect.

#ifndef APEX_FBCON_H
#define APEX_FBCON_H

#include "../include/apex/types.h"
#include "vga.h" // vga_color_t -- reused so callers' color arguments need no translation

// Must be called once, after fb_init() has succeeded, before any
// other fbcon_* function. Computes the character grid size from the
// current fb_get_info() and clears the screen.
void fbcon_init(void);

void fbcon_clear(void);
void fbcon_set_color(vga_color_t fg, vga_color_t bg);
void fbcon_putchar(char c);
void fbcon_puts(const char *str);

uint8_t fbcon_get_row(void);
uint8_t fbcon_get_col(void);
void fbcon_set_cursor_pos(uint8_t row, uint8_t col);
void fbcon_write_at(uint8_t row, uint8_t col, char c);

void fbcon_hide_cursor(void);
void fbcon_show_cursor(void);

void fbcon_scroll_up(uint32_t lines);
void fbcon_scroll_down(uint32_t lines);

// Grid dimensions in characters, computed once in fbcon_init() from
// the framebuffer's pixel resolution and FBCON_GLYPH_W/H below.
// Exposed so vga.c can size its VGA_WIDTH/VGA_HEIGHT-shaped logic
// (e.g. shell_ui.c's SHELL_PROMPT_MAX_PATH_LEN-style wrap checks)
// against the real grid when in graphics mode, if ever needed.
uint32_t fbcon_get_width(void);
uint32_t fbcon_get_height(void);

#endif // APEX_FBCON_H