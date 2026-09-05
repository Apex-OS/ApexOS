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

#endif // APEX_DRIVERS_DISPLAY_CONSOLE_SCROLLBACK_H
