// apex-kernel/core/shell_commands.h
// ApexOS -- Shell built-in commands
//
// Each function implements one shell command, dispatched by
// shell_execute() in shell.c. `ring3` stays in shell.c itself, since it
// manipulates the same low-level userspace jump/return mechanism that
// axf_load_and_run() relies on -- kept next to shell_execute() for
// visibility rather than split out here.

#ifndef APEX_SHELL_COMMANDS_H
#define APEX_SHELL_COMMANDS_H

#include "../../include/apex/types.h"

void cmd_help(void);
void cmd_clear(void);
void cmd_about(void);
void cmd_mem(void);
void cmd_uptime(void);
void cmd_echo(const char *args);
void cmd_ls(const char *arg);
void cmd_cd(const char *arg);
void cmd_mkdir(const char *name);
void cmd_touch(const char *name);
void cmd_write(char *args);
void cmd_cat(const char *name);
void cmd_rm(const char *name);
void cmd_run(const char *name);

#endif // APEX_SHELL_COMMANDS_H