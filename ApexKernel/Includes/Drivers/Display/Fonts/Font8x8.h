// ApexKernel/Includes/Drivers/Display/Fonts/Font8x8.h
// ApexOS -- Minimal 8x8 monochrome bitmap font
//
// Hand-authored subset covering the printable ASCII range needed for
// basic kernel UI/debug text (uppercase, lowercase, digits, common
// punctuation) -- not a full ASCII/CP437 table. Unmapped characters
// (see font8x8_get_glyph()) render as a blank glyph rather than
// crashing or showing garbage.
//
// Each glyph is 8 bytes, one per row, MSB = leftmost pixel. E.g. the
// letter 'A':
//   0b00011000   ...##...
//   0b00100100   ..#..#..
//   0b01000010   .#....#.
//   0b01111110   .######.
//   0b01000010   .#....#.
//   0b01000010   .#....#.
//   0b01000010   .#....#.
//   0b00000000   ........

#ifndef APEX_DRIVERS_DISPLAY_FONT8X8_H
#define APEX_DRIVERS_DISPLAY_FONT8X8_H

#include "Core/Types.h"

#define FONT8X8_WIDTH 8
#define FONT8X8_HEIGHT 8

// Returns a pointer to the 8-byte glyph bitmap for `c`, or a blank
// (all-zero) glyph if `c` isn't covered by this font.
const uint8_t *font8x8_get_glyph(char c);

#endif // APEX_DRIVERS_DISPLAY_FONT8X8_H
