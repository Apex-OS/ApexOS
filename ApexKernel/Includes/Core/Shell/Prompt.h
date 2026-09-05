// ApexKernel/Includes/Core/Shell/Prompt.h
// ApexOS -- Shell display helpers (hardware cursor, prompt, line redraw)

#ifndef APEX_CORE_SHELL_PROMPT_H
#define APEX_CORE_SHELL_PROMPT_H

#include "Core/Types.h"

// Enables the VGA hardware cursor (blinking block) and sets its shape.
void cursor_enable(void);

// Moves the hardware cursor to (row, col).
void cursor_update(uint8_t row, uint8_t col);

// Prints the shell prompt ("ApexOS <cwd>> "), resets the command
// buffer state (length/cursor/history navigation), and records the
// prompt row for later redraws.
void shell_print_prompt(void);

// Redraws the command line from cmd_buf onto the screen at
// prompt_row/shell_prompt_len, and repositions the cursor at
// cmd_cursor. Called after every edit (insert, backspace, history
// navigation).
void shell_redraw_line(void);

// Replaces cmd_buf's content with `src` (used when loading a history
// entry) and redraws the line.
void shell_load_line(const char *src);

void shell_line_to_screen_pos(uint32_t linear_pos, uint8_t *out_row, uint8_t *out_col);

#endif // APEX_CORE_SHELL_PROMPT_H
