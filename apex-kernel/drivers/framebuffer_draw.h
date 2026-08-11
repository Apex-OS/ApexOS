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

// Draws a simple arrow-shaped mouse cursor with its hotspot (the tip)
// at (x, y) -- built from fb_draw_line calls, not a bitmap, so it
// scales cleanly at any position without needing a sprite asset.
void fb_draw_cursor(uint32_t x, uint32_t y, uint32_t color);

// Bounding box of fb_draw_cursor's arrow shape -- the save/restore
// pair below copies exactly this many pixels, so it must stay in
// sync with the line coordinates inside fb_draw_cursor.
#define FB_CURSOR_W 12
#define FB_CURSOR_H 20

// Saves the FB_CURSOR_W x FB_CURSOR_H block of pixels at (x, y) into
// an internal buffer, so the cursor can be drawn there and later
// erased with fb_restore_cursor_bg() without redrawing the whole
// scene. Call this immediately before fb_draw_cursor() at the same
// (x, y). Clipped the same way as fb_fill_rect -- safe to call near a
// screen edge.
void fb_save_cursor_bg(uint32_t x, uint32_t y);

// Restores the block saved by the most recent fb_save_cursor_bg()
// call, at the SAME (x, y) that was passed to it -- this erases the
// cursor drawn there, exposing whatever was underneath again. Calling
// this without a prior fb_save_cursor_bg() at the same position (or
// after the screen contents changed some other way) restores stale
// data -- callers are responsible for keeping the save/draw/restore
// sequence in order.
void fb_restore_cursor_bg(uint32_t x, uint32_t y);

#endif // APEX_FRAMEBUFFER_DRAW_H