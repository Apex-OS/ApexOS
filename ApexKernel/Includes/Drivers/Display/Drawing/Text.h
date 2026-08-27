// ApexKernel/Includes/Drivers/Display/Drawing/Text.h
// ApexOS -- Framebuffer text rendering

#ifndef APEX_DRIVERS_DISPLAY_DRAWING_TEXT_H
#define APEX_DRIVERS_DISPLAY_DRAWING_TEXT_H

#include "Core/Types.h"

// Draws a null-terminated string starting at (x, y) (top-left corner
// of the first glyph), scaled by `scale` (1 = native 8x16 pixels per
// glyph, 2 = 16x32, etc. -- 0 is treated as 1). Covers the full CP437
// codepage (see Font8x16.h) -- every byte value has a real glyph, no
// blank-cell fallback needed. No wrapping -- text running past the
// right edge is clipped character-by-character via fb_put_pixel's
// normal bounds checking inside the glyph-drawing loop.
void fb_draw_text(uint32_t x, uint32_t y, const char *text, uint32_t scale, uint32_t color);

#endif // APEX_DRIVERS_DISPLAY_DRAWING_TEXT_H
