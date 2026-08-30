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

// Same redraw as fbcon_present(), but always runs even while a batch
// is active. For explicit resets (fbcon_clear(), a command's own
// full-screen redraw on exit) that need to actually show up right
// away rather than wait for (or be skipped by) the batch's own
// end-of-command redraw.
void fbcon_present_force(void);

// Fast path for the single most common redraw: the view scrolled
// down by exactly one line because a new line was just written at
// the bottom (fbcon_new_line()), and no manual scrollback offset is
// active. Shifts the back-buffer up by one glyph row (fb_scroll_up())
// instead of redrawing every on-screen cell from scrollback data --
// only the newly-exposed bottom row needs an actual glyph redraw.
// Callers must only use this when they know the update really is a
// plain one-line scroll (scroll_offset == 0, exactly one new line
// added) -- anything else (manual scrollback, a resize, more than one
// line at once) still needs fbcon_present_force()'s full redraw.
void fbcon_present_scroll_one_line(void);

#endif // APEX_DRIVERS_DISPLAY_CONSOLE_RENDER_H
