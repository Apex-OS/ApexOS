// ApexKernel/Includes/Drivers/Display/Console/ConsoleRender.h
// ApexOS -- Framebuffer console cell rendering

#ifndef APEX_DRIVERS_DISPLAY_CONSOLE_RENDER_H
#define APEX_DRIVERS_DISPLAY_CONSOLE_RENDER_H

#include "Core/Types.h"
#include "Drivers/Display/Console/ConsoleCore.h"

// Draws one character cell at grid position (row, col): background
// fill first (so glyphs with descenders/gaps show the right bg, same
// as fb_draw_text's "background left untouched" contract needing an
// explicit clear underneath), then the glyph on top.
void fbcon_draw_cell(uint32_t row, uint32_t col, char c, uint8_t attr);

// Same as fbcon_draw_cell(), but also pushes just this cell's pixels
// to VRAM (fb_present_rect(), not the full-screen fb_present()) --
// the fast path fbcon_putchar() uses for a single changed character,
// so typing doesn't visibly lag waiting for a full-screen copy on
// every keystroke.
void fbcon_draw_cell_present(uint32_t row, uint32_t col, char c, uint8_t attr);

// Redraws the entire visible grid from scrollback, then the cursor,
// then pushes to VRAM. Called after every write, same as VGA.c's
// vga_present() -- simplest correct approach; a dirty-line tracker
// would cut redundant work if this ever becomes a bottleneck.
void fbcon_present(void);

#endif // APEX_DRIVERS_DISPLAY_CONSOLE_RENDER_H
