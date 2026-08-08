// apex-kernel/drivers/framebuffer_draw.h
// ApexOS -- Framebuffer drawing primitives (rectangles, lines)
//
// Built on top of framebuffer.h's fb_put_pixel()/fb_get_info() -- this
// file has no knowledge of VESA/VBE or memory mapping, only pixel
// coordinates. Split out of framebuffer.c to keep the low-level driver
// (init, mode detection result, memory mapping) separate from drawing
// logic, which is expected to grow (text rendering, more shapes) as
// the project's graphics needs grow.

#ifndef APEX_FRAMEBUFFER_DRAW_H
#define APEX_FRAMEBUFFER_DRAW_H

#include "../include/apex/types.h"

// Fills a rectangle [x, x+w) x [y, y+h) with a solid color. Clipped to
// the framebuffer's actual bounds -- a rectangle partially or fully
// outside the screen is silently cropped rather than corrupting
// adjacent memory.
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

// Draws a rectangle's outline only (border thickness in pixels),
// clipped the same way as fb_fill_rect. A thickness that would make
// the border overlap itself (e.g. thickness > w/2 or h/2) still
// produces a sensible filled-looking result rather than corruption.
void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t thickness, uint32_t color);

// Draws a straight line from (x0, y0) to (x1, y1), any direction
// (horizontal, vertical, diagonal). Uses Bresenham's algorithm --
// exact integer arithmetic, no floating point (unavailable/unwise in
// freestanding kernel code without saving/restoring FPU state).
void fb_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);

// Draws a null-terminated string starting at (x, y) (top-left corner
// of the first glyph), scaled by `scale` (1 = native 8x8 pixels per
// glyph, 2 = 16x16, etc. -- 0 is treated as 1). Characters not covered
// by the built-in font (see font8x8.h) render as a blank cell rather
// than garbage. No wrapping -- text running past the right edge is
// clipped character-by-character via fb_put_pixel's normal bounds
// checking inside the glyph-drawing loop.
void fb_draw_text(uint32_t x, uint32_t y, const char *text, uint32_t scale, uint32_t color);

#endif // APEX_FRAMEBUFFER_DRAW_H