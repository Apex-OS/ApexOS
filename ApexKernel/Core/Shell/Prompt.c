// ApexKernel/Core/Shell/Prompt.c
// ApexOS -- Shell display helpers (hardware cursor, prompt, line redraw)

#include "Core/Shell/Prompt.h"
#include "Core/Shell/ShellInternal.h"
#include "Core/Shell/PathResolver.h"
#include "Core/Shell/History.h"
#include "Drivers/Display/VGA.h"
#include "Bus/PortIO.h"

void cursor_enable(void)
{
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 13);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 15);
}

void cursor_update(uint8_t row, uint8_t col)
{
    uint16_t pos = row * VGA_WIDTH + col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

// Prompt geometry is not a fixed constant since the displayed path
// varies with cwd -- updated on every shell_print_prompt() call, read
// wherever the caller needs to position the cursor relative to the
// start of the input line.
uint32_t shell_prompt_len = 8; // initial value = former fixed "ApexOS> "
uint8_t prompt_row = 0;
static uint32_t prompt_wrapped_lines = 0;

#define SHELL_PROMPT_MAX_PATH_LEN 40

void shell_print_prompt(void)
{
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("ApexOS ");
    vga_set_color(VGA_CYAN, VGA_BLACK);

    uint32_t cwd_len = 0;
    while (cwd[cwd_len])
        cwd_len++;

    uint32_t printed_len;
    if (cwd_len > SHELL_PROMPT_MAX_PATH_LEN)
    {
        vga_puts("...");
        vga_puts(cwd + (cwd_len - SHELL_PROMPT_MAX_PATH_LEN + 3));
        printed_len = SHELL_PROMPT_MAX_PATH_LEN;
    }
    else
    {
        vga_puts(cwd);
        printed_len = cwd_len;
    }

    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("> ");

    // "ApexOS " (7) + displayed path + "> " (2)
    shell_prompt_len = 7 + printed_len + 2;

    cmd_len = 0;
    cmd_cursor = 0;
    shell_history_reset_navigation();
    prompt_row = vga_get_row();

    // Keeps the hardware cursor (the blinking block driven via ports
    // 0x3D4/0x3D5) in sync with the logical cursor (vga_row/vga_col,
    // updated by vga_puts() above) -- otherwise the blinking cursor
    // stays stuck at its previous position after Enter or `clear`.
    cursor_update(vga_get_row(), vga_get_col());
}

// Converts a linear position within the input line (0 = the first
// character right after the prompt) into an actual on-screen (row,
// col), wrapping at the screen width -- shell_prompt_len + i can run
// well past a single row once the typed command is longer than what
// fits after the prompt. If that pushes past the bottom of the
// screen, this scrolls the view up by the needed number of rows
// first (adjusting prompt_row to match) so every position always
// lands on a valid on-screen row.
void shell_line_to_screen_pos(uint32_t linear_pos, uint8_t *out_row, uint8_t *out_col)
{
    uint32_t width = vga_get_width();
    uint32_t height = vga_get_height();

    uint32_t wrapped_line = linear_pos / width;
    uint32_t col = linear_pos % width;

    while (prompt_wrapped_lines < wrapped_line)
    {
        vga_advance_line();
        prompt_wrapped_lines++;
        if (prompt_row + prompt_wrapped_lines >= height)
            prompt_row--;
    }

    *out_row = (uint8_t)(prompt_row + wrapped_line);
    *out_col = (uint8_t)col;
}

void shell_redraw_line(void)
{
    for (uint32_t i = 0; i < cmd_len; i++)
    {
        uint8_t row, col;
        shell_line_to_screen_pos(shell_prompt_len + i, &row, &col);
        vga_write_at(row, col, cmd_buf[i]);
    }

    static uint32_t last_drawn_len = 0;
    for (uint32_t i = cmd_len; i < last_drawn_len; i++)
    {
        uint8_t row, col;
        shell_line_to_screen_pos(shell_prompt_len + i, &row, &col);
        vga_write_at(row, col, ' ');
    }
    last_drawn_len = cmd_len;

    uint8_t cursor_row, cursor_col;
    shell_line_to_screen_pos(shell_prompt_len + cmd_cursor, &cursor_row, &cursor_col);
    vga_set_cursor_pos(cursor_row, cursor_col);
    cursor_update(cursor_row, cursor_col);
}

void shell_load_line(const char *src)
{
    cmd_len = 0;
    while (src[cmd_len] && cmd_len < SHELL_BUFFER_SIZE - 1)
    {
        cmd_buf[cmd_len] = src[cmd_len];
        cmd_len++;
    }
    cmd_cursor = cmd_len;
    shell_redraw_line();
}
