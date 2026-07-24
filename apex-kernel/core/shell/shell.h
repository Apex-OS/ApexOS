// apex-kernel/core/shell.h
// ApexOS — Kernel Shell

#ifndef APEX_SHELL_H
#define APEX_SHELL_H

#include "../../include/apex/types.h"

#define SHELL_BUFFER_SIZE 4096 // 4KB au lieu de 256
#define SHELL_PROMPT "ApexOS> "

// Command line buffer state -- owned by shell.c (the input loop), read
// and written by shell_ui.c (redraw/prompt) and shell_history.c (line
// loading during history navigation).
extern char cmd_buf[SHELL_BUFFER_SIZE];
extern uint32_t cmd_len;    // total length of the current line
extern uint32_t cmd_cursor; // cursor position within cmd_buf (0..cmd_len)

// Prompt geometry, updated by shell_print_prompt() -- read by shell.c's
// main loop to position the hardware cursor after edits.
extern uint32_t shell_prompt_len;
extern uint8_t prompt_row; // screen row where the input line starts

void shell_init(void);
void shell_run(void); // Tourne comme un processus

#endif // APEX_SHELL_H