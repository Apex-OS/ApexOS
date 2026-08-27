// ApexKernel/Includes/Drivers/Display/Drawing/Shapes.h
// ApexOS -- Framebuffer geometric primitives (rectangles, lines)

#ifndef APEX_DRIVERS_DISPLAY_DRAWING_SHAPES_H
#define APEX_DRIVERS_DISPLAY_DRAWING_SHAPES_H

#include "Core/Types.h"

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

#endif // APEX_DRIVERS_DISPLAY_DRAWING_SHAPES_H
