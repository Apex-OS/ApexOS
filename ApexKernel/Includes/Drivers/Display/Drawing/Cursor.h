// ApexKernel/Includes/Drivers/Display/Drawing/Cursor.h
// ApexOS -- Framebuffer mouse cursor rendering

#ifndef APEX_DRIVERS_DISPLAY_DRAWING_CURSOR_H
#define APEX_DRIVERS_DISPLAY_DRAWING_CURSOR_H

#include "Core/Types.h"

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

#endif // APEX_DRIVERS_DISPLAY_DRAWING_CURSOR_H
