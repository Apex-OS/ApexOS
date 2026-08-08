// apex-kernel/core/shell_history.c
// ApexOS -- Shell command history (circular buffer + navigation)

#include "shell_history.h"
#include "shell.h"
#include "shell_ui.h"

#define SHELL_HISTORY_SIZE 32

static char history[SHELL_HISTORY_SIZE][SHELL_BUFFER_SIZE];
static uint32_t history_count = 0;                // number of valid entries
static uint32_t history_next = 0;                 // next write slot (circular)
static uint32_t history_pos = SHELL_HISTORY_SIZE; // current navigation position

// Saves the line being typed when navigation starts, so ARROW_DOWN can
// return to it after browsing past the newest entry.
static char history_draft[SHELL_BUFFER_SIZE];

void shell_history_push(const char *cmd)
{
    if (cmd[0] == '\0')
        return; // don't archive empty lines

    uint32_t idx = history_next % SHELL_HISTORY_SIZE;
    uint32_t i = 0;
    while (cmd[i] && i < SHELL_BUFFER_SIZE - 1)
    {
        history[idx][i] = cmd[i];
        i++;
    }
    history[idx][i] = '\0';

    history_next++;
    if (history_count < SHELL_HISTORY_SIZE)
        history_count++;
}

void shell_history_reset_navigation(void)
{
    history_pos = SHELL_HISTORY_SIZE;
}

void shell_history_navigate_up(void)
{
    if (history_count == 0)
        return;

    if (history_pos == SHELL_HISTORY_SIZE)
    {
        // First navigation step: save the in-progress draft.
        uint32_t i = 0;
        while (i < cmd_len)
        {
            history_draft[i] = cmd_buf[i];
            i++;
        }
        history_draft[i] = '\0';
        history_pos = 0;
    }
    else if (history_pos + 1 < history_count)
    {
        history_pos++;
    }
    else
    {
        return; // already at the oldest entry
    }

    // history_pos = 0 -> most recently archived command, etc.
    uint32_t idx = (history_next - 1 - history_pos) % SHELL_HISTORY_SIZE;
    shell_load_line(history[idx]);
}

void shell_history_navigate_down(void)
{
    if (history_pos == SHELL_HISTORY_SIZE)
        return; // not currently navigating

    if (history_pos == 0)
    {
        history_pos = SHELL_HISTORY_SIZE;
        shell_load_line(history_draft);
        return;
    }

    history_pos--;
    uint32_t idx = (history_next - 1 - history_pos) % SHELL_HISTORY_SIZE;
    shell_load_line(history[idx]);
}