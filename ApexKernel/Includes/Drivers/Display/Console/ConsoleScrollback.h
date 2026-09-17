// ApexKernel/Includes/Drivers/Display/Console/ConsoleScrollback.h
// ApexOS -- Framebuffer console scrollback/scrolling

#ifndef APEX_DRIVERS_DISPLAY_CONSOLE_SCROLLBACK_H
#define APEX_DRIVERS_DISPLAY_CONSOLE_SCROLLBACK_H

#include "Core/Types.h"

// Advances write_row, allocates the new blank line in the scrollback
// ring, and resets scroll_offset (a fresh newline always brings the
// view back to the bottom).
void fbcon_new_line(void);
void fbcon_advance_line(void);

void fbcon_scroll_up(uint32_t lines);
void fbcon_scroll_down(uint32_t lines);

// Copies every line VGA.c's real-text-mode scrollback still holds
// (vga_get_scrollback_line_count()/vga_get_scrollback_cell(), see
// VGA.h's own comment on why this exists) into this console's own
// scrollback, so boot output from before the switch to graphics mode
// (Core/Init.c's vga_enable_graphics_mode() call) stays visible/
// scrollable-to afterwards instead of just vanishing. Must be called
// AFTER fbcon_init() (which calls fbcon_clear(), wiping this
// console's scrollback to blank -- calling this first would just get
// overwritten) and is meant to run exactly once, right after that
// switch -- see vga_enable_graphics_mode() itself.
void fbcon_import_scrollback(void);

#endif // APEX_DRIVERS_DISPLAY_CONSOLE_SCROLLBACK_H
