// ApexKernel/Drivers/Display/Console/ConsoleScrollback.c
// ApexOS -- Framebuffer console scrollback/scrolling

#include "Drivers/Display/Console/ConsoleScrollback.h"
#include "Drivers/Display/Console/Internal.h"
#include "Drivers/Display/Console/ConsoleRender.h"

void fbcon_new_line(void)
{
    write_row++;
    if (total_lines < FBCON_SCROLLBACK_LINES)
        total_lines++;

    uint32_t line = write_row % FBCON_SCROLLBACK_LINES;
    uint8_t default_blank_attr = fbcon_make_attr(VGA_WHITE, VGA_BLACK);
    for (uint32_t c = 0; c < grid_width; c++)
    {
        scrollback[line][c].c = ' ';
        scrollback[line][c].attr = default_blank_attr;
    }

    scroll_offset = 0;
}

void fbcon_advance_line(void)
{
    write_row++;
    if (total_lines < FBCON_SCROLLBACK_LINES)
        total_lines++;
    scroll_offset = 0;
}

void fbcon_scroll_up(uint32_t lines)
{
    scroll_offset += lines;
    fbcon_present();
}

void fbcon_scroll_down(uint32_t lines)
{
    if (lines > scroll_offset)
        scroll_offset = 0;
    else
        scroll_offset -= lines;
    fbcon_present();
}
