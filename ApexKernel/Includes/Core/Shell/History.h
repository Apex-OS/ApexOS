// ApexKernel/Includes/Core/Shell/History.h
// ApexOS -- Shell command history (circular buffer + navigation)

#ifndef APEX_CORE_SHELL_HISTORY_H
#define APEX_CORE_SHELL_HISTORY_H

// Archives `cmd` as the most recent history entry. No-op for empty
// lines.
void shell_history_push(const char *cmd);

// Resets history navigation to "not currently browsing" -- called by
// shell_print_prompt() after each command is executed.
void shell_history_reset_navigation(void);

// Moves navigation one step towards older commands and loads that
// entry into the command line via shell_load_line(). No-op if there is
// no history or already at the oldest entry. On the first call, saves
// the line currently being typed so ARROW_DOWN can return to it.
void shell_history_navigate_up(void);

// Moves navigation one step towards newer commands (or back to the
// in-progress draft line) and loads it via shell_load_line(). No-op if
// not currently navigating.
void shell_history_navigate_down(void);

#endif // APEX_CORE_SHELL_HISTORY_H
