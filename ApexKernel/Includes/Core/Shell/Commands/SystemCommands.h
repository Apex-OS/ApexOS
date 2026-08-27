// ApexKernel/Includes/Core/Shell/Commands/SystemCommands.h
// ApexOS -- Shell system/info commands

#ifndef APEX_CORE_SHELL_COMMANDS_SYSTEM_H
#define APEX_CORE_SHELL_COMMANDS_SYSTEM_H

void cmd_help(void);
void cmd_clear(void);
void cmd_about(void);
void cmd_mem(void);
void cmd_uptime(void);
void cmd_echo(const char *args);

#endif // APEX_CORE_SHELL_COMMANDS_SYSTEM_H
