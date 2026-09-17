// ApexKernel/Drivers/Display/Console/ConsoleScrollback.c
// ApexOS -- Framebuffer console scrollback/scrolling

#include "Drivers/Display/Console/ConsoleScrollback.h"
#include "Drivers/Display/Console/Internal.h"
#include "Drivers/Display/Console/ConsoleRender.h"
#include "Drivers/Display/VGA.h"

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

void fbcon_import_scrollback(void)
{
    uint32_t vga_line_count = vga_get_scrollback_line_count();
    if (vga_line_count == 0)
        return;

    // Import at most FBCON_SCROLLBACK_LINES worth -- if VGA.c somehow
    // held more than this console's own ring buffer can hold (it
    // can't today, both are sized independently but happen to match;
    // this is just a safety bound against that changing later), keep
    // only the most recent lines rather than overflowing here.
    uint32_t import_count = vga_line_count;
    if (import_count > FBCON_SCROLLBACK_LINES)
        import_count = FBCON_SCROLLBACK_LINES;
    uint32_t first_line = vga_line_count - import_count;

    uint8_t blank_attr = fbcon_make_attr(VGA_WHITE, VGA_BLACK);
    for (uint32_t i = 0; i < import_count; i++)
    {
        uint32_t src_line = first_line + i;
        uint32_t dest_line = i % FBCON_SCROLLBACK_LINES;

        for (uint32_t c = 0; c < grid_width; c++)
        {
            char ch = ' ';
            uint8_t attr = blank_attr;
            // Out-of-range columns (grid_width can exceed VGA_WIDTH's
            // 80 once in framebuffer mode, see FBCON_MAX_WIDTH) simply
            // stay blank -- there's no real VGA text-mode content to
            // show past column 80 anyway, since that's every column
            // that ever existed on the text-mode screen this is
            // importing from.
            vga_get_scrollback_cell(src_line, c, &ch, &attr);
            scrollback[dest_line][c].c = ch;
            scrollback[dest_line][c].attr = attr;
        }
        // Blank out any remaining columns beyond VGA_WIDTH on this
        // imported line (already the case if the loop above never
        // wrote them -- vga_get_scrollback_cell() returns false past
        // VGA_WIDTH, leaving ch/attr at their blank defaults -- this
        // is just making that explicit rather than relying on it).
    }

    write_row = import_count - 1;
    total_lines = import_count;
    scroll_offset = 0;

    // Positions the write cursor to continue right after the last
    // imported line, NOT at the bottom of the screen -- with fewer
    // imported lines than grid_height (the common case: a handful of
    // early boot lines on a screen with room for dozens), row
    // grid_height-1 would leave a visible gap of blank rows between
    // the imported history and the next line written, which is
    // exactly the "big empty gap, fixed by scrolling" symptom this
    // was chased down for: fbcon_putchar()'s incremental fast paths
    // (ConsoleCore.c) decide whether the screen has actually scrolled
    // yet purely from `total_lines >= grid_height` -- while that's
    // false (fewer lines imported than the screen can hold), writing
    // at the bottom draws one isolated cell there without ever
    // repainting the rows in between, and only a manual scroll's
    // fbcon_present_force() (which redraws every row honestly from
    // total_lines) revealed the lines that were sitting in
    // scrollback[][] the whole time. Clamped to grid_height-1 for the
    // opposite case (more imported lines than the screen can show at
    // once): row import_count would be off-screen entirely then, and
    // the already-scrolled state fbcon_putchar() reads from
    // total_lines is exactly correct in that case, which the
    // untouched write_row/total_lines above already reflects.
    cur_row = (import_count < grid_height) ? import_count : grid_height - 1;
    cur_col = 0;
}
