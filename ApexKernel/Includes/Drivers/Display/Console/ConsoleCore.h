// ApexKernel/Includes/Drivers/Display/Console/ConsoleCore.h
// ApexOS -- Framebuffer console: renders a text-mode-like terminal
// (character grid, scrollback, colors, cursor) on top of the VESA/VBE
// framebuffer (see Drivers/Display/Framebuffer/), instead of the real
// VGA text-mode hardware (0xB8000).
//
// This exists so the kernel's existing text output (klog, shell,
// AXFS/AXF logging -- ~200 call sites across the codebase) shows up
// in the QEMU window when booted in graphics mode, without changing
// any of those call sites. Drivers/Display/VGA.c is the integration
// point: its public functions (vga_puts, vga_putchar, etc.) check a
// graphics-mode flag and forward to this file's fbcon_* functions
// when set, otherwise they behave exactly as before (real VGA text
// mode) -- see VGA.c for that wiring. This file has no callers
// outside VGA.c; nothing else should call fbcon_* directly.
//
// Deliberately mirrors VGA.c's internal design (scrollback buffer,
// write_row/vga_row split, \n/\r/\t/\b handling) so the two behave
// identically from the caller's point of view -- the only real
// difference is what happens at "present" time: VGA.c copies
// characters into 0xB8000, this copies rendered glyphs into the
// framebuffer back-buffer via fb_draw_text/fb_fill_rect.

#ifndef APEX_DRIVERS_DISPLAY_CONSOLE_CORE_H
#define APEX_DRIVERS_DISPLAY_CONSOLE_CORE_H

#include "Core/Types.h"
#include "Drivers/Display/VGA.h" // vga_color_t -- reused so callers' color arguments need no translation

// One console cell -- character plus color attribute.
typedef struct
{
    char c;
    uint8_t attr; // low nibble = fg, high nibble = bg -- same packing as VGA.c's vga_make_attr
} fbcon_cell_t;

// Glyph cell size on screen -- FONT8X16_WIDTH/HEIGHT scaled up (see
// fb_draw_text's `scale` parameter). 1 (native size) chosen now that
// the font itself is the real VGA BIOS CP437 8x16 font (see
// Fonts/Font8x16.h) -- it's designed to be crisp at its native size,
// the way real VGA text mode always rendered it, so no scaling is
// needed the way Font8x8.h's smaller glyphs needed 2x to stay
// readable at 1600x900. Gives a ~200x56 character grid at 1600x900
// (see Boot/BIOS/Stage2.asm: MAX_FB_WIDTH/HEIGHT).
#define FBCON_SCALE 1
#define FBCON_GLYPH_W (FONT8X16_WIDTH * FBCON_SCALE)
#define FBCON_GLYPH_H (FONT8X16_HEIGHT * FBCON_SCALE)

// Scrollback depth, in lines -- same value as VGA.c's
// SCROLLBACK_LINES for consistent history length regardless of which
// console is active. Grid width is capped at this file's
// FBCON_MAX_WIDTH rather than sized dynamically, so the scrollback
// array can be a flat static allocation (heap usage is already tight
// -- see Core/Memory/Heap/ -- and this avoids one more kmalloc'd
// structure to track).
#define FBCON_SCROLLBACK_LINES 500
#define FBCON_MAX_WIDTH 256 // headroom above the 200 cols 1600px/8px gives (Font8x16.h, scale=1)

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

// Grid dimensions in characters, computed once in fbcon_init() from
// the framebuffer's pixel resolution and FBCON_GLYPH_W/H above.
// Exposed so VGA.c can size its VGA_WIDTH/VGA_HEIGHT-shaped logic
// against the real grid when in graphics mode, if ever needed.
uint32_t fbcon_get_width(void);
uint32_t fbcon_get_height(void);

static inline uint8_t fbcon_make_attr(vga_color_t fg, vga_color_t bg)
{
    return (uint8_t)(fg | (bg << 4));
}

#endif // APEX_DRIVERS_DISPLAY_CONSOLE_CORE_H
