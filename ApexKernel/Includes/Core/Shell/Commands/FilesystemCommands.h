// ApexKernel/Includes/Core/Shell/Commands/FilesystemCommands.h
// ApexOS -- Shell filesystem commands

#ifndef APEX_CORE_SHELL_COMMANDS_FILESYSTEM_H
#define APEX_CORE_SHELL_COMMANDS_FILESYSTEM_H

void cmd_ls(const char *arg);
void cmd_cd(const char *arg);
void cmd_mkdir(const char *name);
void cmd_touch(const char *name);
void cmd_write(char *args);
void cmd_cat(const char *name);
void cmd_rm(const char *name);
void cmd_run(const char *name);

#endif // APEX_CORE_SHELL_COMMANDS_FILESYSTEM_H
