// apex-kernel/core/shell_ui.c
// ApexOS -- Shell display helpers (hardware cursor, prompt, line redraw)

#include "shell_ui.h"
#include "shell.h"
#include "shell_path.h"
#include "shell_history.h"
#include "../../drivers/vga.h"
#include "../../include/apex/io.h"

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

// KNOWN LIMITATION: does not handle a line wrapping past the screen
// width (80 - 8 = 72 visible columns) -- past that, the display will
// be incorrect. SHELL_BUFFER_SIZE (4096) allows much longer commands
// internally, but in-place editing assumes the line fits on a single
// screen row. A wrap-aware redraw would be the next improvement if
// needed.
void shell_redraw_line(void)
{
    for (uint32_t i = 0; i < cmd_len; i++)
        vga_write_at(prompt_row, shell_prompt_len + i, cmd_buf[i]);

    // Clears any leftover characters past the new end (e.g. after a
    // Backspace, or when loading a shorter command from history).
    static uint32_t last_drawn_len = 0;
    for (uint32_t i = cmd_len; i < last_drawn_len; i++)
        vga_write_at(prompt_row, shell_prompt_len + i, ' ');
    last_drawn_len = cmd_len;

    vga_set_cursor_pos(prompt_row, shell_prompt_len + cmd_cursor);
    cursor_update(prompt_row, shell_prompt_len + cmd_cursor);
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