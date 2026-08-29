// ApexKernel/Includes/Drivers/Display/Console/Internal.h
// ApexOS -- Shared internal state for the Console module
//
// The scrollback buffer and cursor/grid tracking variables are
// defined once in ConsoleCore.c and shared via extern with
// ConsoleRender.c, ConsoleScrollback.c, and ConsoleCursor.c. Not part
// of the public ConsoleCore.h API: nothing outside
// Drivers/Display/Console/ should touch these directly.

#ifndef APEX_DRIVERS_DISPLAY_CONSOLE_INTERNAL_H
#define APEX_DRIVERS_DISPLAY_CONSOLE_INTERNAL_H

#include "Core/Types.h"

#include "Drivers/Display/Console/ConsoleCore.h"

extern fbcon_cell_t scrollback[FBCON_SCROLLBACK_LINES][FBCON_MAX_WIDTH];
extern uint32_t total_lines;
extern uint32_t write_row;
extern uint32_t scroll_offset;

extern uint32_t grid_width;
extern uint32_t grid_height;
extern uint8_t cur_col;
extern uint32_t cur_row; // on-screen write row (0..grid_height-1)
extern uint8_t cur_attr;
extern bool cursor_hidden;
extern bool present_suppressed;

// 16-color VGA palette, standard RGB values (same as most VGA text
// mode implementations/terminal emulators default to) -- used to
// translate vga_color_t into an FB_RGB() pixel value for fb_draw_text.
extern const uint32_t vga_palette[16];

#endif // APEX_DRIVERS_DISPLAY_CONSOLE_INTERNAL_H
