// ApexKernel/Core/Shell/Shell.c
// ApexOS -- Kernel shell
//
// Owns the command line input state (cmd_buf/cmd_len/cmd_cursor) and
// the main read-eval loop. Command implementations, path resolution,
// display/redraw, and history navigation live in their own files (see
// Commands/, PathResolver.c, Prompt.c, History.c).

#include "Core/Shell/Shell.h"
#include "Core/Shell/ShellInternal.h"
#include "Core/Shell/CommandDispatcher.h"
#include "Core/Shell/Prompt.h"
#include "Core/Shell/History.h"
#include "Core/Version.h"
#include "Drivers/Display/VGA.h"
#include "Drivers/Input/Keyboard/Keyboard.h"
#include "Drivers/Input/Keyboard/KeyBuffer.h"

char cmd_buf[SHELL_BUFFER_SIZE];
uint32_t cmd_len = 0;
uint32_t cmd_cursor = 0;

void shell_init(void)
{
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("\nPress any key to start shell...");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    keyboard_getchar();

    vga_clear();
    cursor_enable();

    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("    ___                    ____  _____\n");
    vga_puts("   /   |  ____  ___  _  __/ __ \\/ ___/\n");
    vga_puts("  / /| | / __ \\/ _ \\| |/_/ / / / /\\__ \n");
    vga_puts(" / ___ |/ /_/ /  __/>  </ /_/ /___/ / \n");
    vga_puts("/_/  |_/ .___/\\___/_/|_/\\____//____/  \n");
    vga_puts("      /_/                             \n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("      ApexOS v" APEXOS_VERSION " - Type 'help'\n\n");

    shell_print_prompt();
}

void shell_run(void)
{
    shell_init();
    cursor_update(vga_get_row(), vga_get_col());

    while (1)
    {
        if (!keyboard_has_key())
            continue;

        // Any keypress jumps back to the live prompt if scrolled up.
        if (vga_is_scrolled())
            vga_scroll_down(0xFFFFFFFF);

        uint8_t c = keyboard_getchar();
        if (c == 0)
            continue;

        if (c == '\n')
        {
            cmd_buf[cmd_len] = '\0';
            uint8_t end_row, end_col;
            shell_line_to_screen_pos(shell_prompt_len + cmd_len, &end_row, &end_col);
            vga_set_cursor_pos(end_row, end_col);
            vga_putchar('\n');
            shell_history_push(cmd_buf);
            vga_begin_batch();
            shell_execute();
            vga_end_batch();
            shell_print_prompt();
        }
        else if (c == '\b')
        {
            // Deletes the character BEFORE the cursor (not necessarily
            // at the end of the line): shifts everything after it one
            // slot left, then redraws.
            if (cmd_cursor > 0)
            {
                for (uint32_t i = cmd_cursor - 1; i < cmd_len - 1; i++)
                    cmd_buf[i] = cmd_buf[i + 1];
                cmd_len--;
                cmd_cursor--;
                shell_redraw_line();
            }
        }
        else if (c == KEY_ARROW_LEFT)
        {
            if (cmd_cursor > 0)
            {
                cmd_cursor--;
                uint8_t row, col;
                shell_line_to_screen_pos(shell_prompt_len + cmd_cursor, &row, &col);
                vga_set_cursor_pos(row, col);
                cursor_update(row, col);
            }
        }
        else if (c == KEY_ARROW_RIGHT)
        {
            if (cmd_cursor < cmd_len)
            {
                cmd_cursor++;
                uint8_t row, col;
                shell_line_to_screen_pos(shell_prompt_len + cmd_cursor, &row, &col);
                vga_set_cursor_pos(row, col);
                cursor_update(row, col);
            }
        }
        else if (c == KEY_ARROW_UP)
        {
            shell_history_navigate_up();
        }
        else if (c == KEY_ARROW_DOWN)
        {
            shell_history_navigate_down();
        }
        else if (c >= 0x80)
        {
            continue; // unhandled special code -- ignore
        }
        else if (cmd_len < SHELL_BUFFER_SIZE - 1)
        {
            // Inserts at the cursor position (not necessarily at the
            // end of the line): shifts everything after it one slot
            // right to make room, then redraws.
            for (uint32_t i = cmd_len; i > cmd_cursor; i--)
                cmd_buf[i] = cmd_buf[i - 1];
            cmd_buf[cmd_cursor] = (char)c;
            cmd_len++;
            cmd_cursor++;
            shell_redraw_line();
        }
        else
        {
            vga_set_color(VGA_RED, VGA_BLACK);
            vga_putchar('\a'); // bell
            vga_set_color(VGA_WHITE, VGA_BLACK);
        }
    }
}
