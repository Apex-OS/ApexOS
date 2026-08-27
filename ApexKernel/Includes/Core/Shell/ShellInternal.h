// ApexKernel/Includes/Core/Shell/ShellInternal.h
// ApexOS -- Shared internal state for the Shell module
//
// cmd_buf/cmd_len/cmd_cursor are owned by Shell.c (the input loop),
// read and written by Prompt.c (redraw/prompt) and History.c (line
// loading during history navigation). shell_prompt_len/prompt_row are
// updated by Prompt.c and read by Shell.c's main loop to position the
// hardware cursor after edits. cwd is owned by PathResolver.c and
// shared with Prompt.c (prompt display) and the Commands/.

#ifndef APEX_CORE_SHELL_INTERNAL_H
#define APEX_CORE_SHELL_INTERNAL_H

#include "Core/Shell/Shell.h"

extern char cmd_buf[SHELL_BUFFER_SIZE];
extern uint32_t cmd_len;    // total length of the current line
extern uint32_t cmd_cursor; // cursor position within cmd_buf (0..cmd_len)

extern uint32_t shell_prompt_len;
extern uint8_t prompt_row; // screen row where the input line starts

#endif // APEX_CORE_SHELL_INTERNAL_H
