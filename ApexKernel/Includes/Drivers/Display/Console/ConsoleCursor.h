// ApexKernel/Includes/Drivers/Display/Console/ConsoleCursor.h
// ApexOS -- Framebuffer console cursor rendering

#ifndef APEX_DRIVERS_DISPLAY_CONSOLE_CURSOR_H
#define APEX_DRIVERS_DISPLAY_CONSOLE_CURSOR_H

// Draws a solid block cursor at the given grid position -- no
// hardware cursor exists in graphics mode (unlike VGA.c's port
// 0x3D4/0x3D5 blinking block), so this just inverts the cell's
// colors, redrawn each fbcon_present().
void fbcon_draw_cursor(void);

// Same as fbcon_draw_cursor(), but also pushes just the cursor's cell
// to VRAM -- the fast path fbcon_putchar() uses, symmetric with
// fbcon_draw_cell_present().
void fbcon_draw_cursor_present(void);

void fbcon_hide_cursor(void);
void fbcon_show_cursor(void);

#endif // APEX_DRIVERS_DISPLAY_CONSOLE_CURSOR_H
