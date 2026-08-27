// ApexKernel/Includes/Drivers/Display/Fonts/Font8x16.h
// ApexOS -- VGA BIOS-compatible 8x16 CP437 bitmap font
//
// This is the standard IBM PC/VGA CP437 font -- same shapes as the
// font the real VGA BIOS loads into text mode by default (int 10h
// AH=11h), re-extracted from the Linux kernel's own copy
// (lib/fonts/font_8x16.c, SPDX-License-Identifier: GPL-2.0) rather
// than read out of BIOS memory at boot, since that memory isn't
// accessible anymore once the kernel is running in Long Mode (see
// ApexKernel/Boot/BIOS/Stage2.asm -- BIOS interrupts only work in
// real mode, well before this driver runs).
//
// Covers the full CP437 codepage (256 glyphs, indexed directly by
// byte value 0-255) -- unlike Font8x8.h's hand-authored subset, every
// value of `char` maps to a real glyph here, no fallback/blank case
// needed.
//
// Each glyph is 16 bytes, one per row, MSB = leftmost pixel -- same
// bit-per-pixel convention as Font8x8.h, just twice the rows.

#ifndef APEX_DRIVERS_DISPLAY_FONT8X16_H
#define APEX_DRIVERS_DISPLAY_FONT8X16_H

#include "Core/Types.h"

#define FONT8X16_WIDTH 8
#define FONT8X16_HEIGHT 16

// Returns a pointer to the 16-byte glyph bitmap for `c`. Every byte
// value (0-255) has a real glyph in this font -- there's no
// "unmapped character" case the way font8x8_get_glyph() has.
const uint8_t *font8x16_get_glyph(char c);

#endif // APEX_DRIVERS_DISPLAY_FONT8X16_H
