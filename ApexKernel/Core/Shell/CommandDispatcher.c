// ApexKernel/Core/Shell/CommandDispatcher.c
// ApexOS -- Shell command tokenizing and dispatch

#include "Core/Shell/CommandDispatcher.h"
#include "Core/Shell/ShellInternal.h"
#include "Core/Shell/PathResolver.h"
#include "Core/Shell/Commands/SystemCommands.h"
#include "Core/Shell/Commands/FilesystemCommands.h"
#include "Core/Shell/Commands/DiagnosticCommands.h"
#include "Core/Shell/Commands/UserspaceTest.h"
#include "Drivers/Display/VGA.h"
#include "Logs/Log.h"

// Tests/ has no Includes/ (test files are self-contained, see project
// notes) -- declared directly here instead.
extern void ktest_run_all(void);

// Splits cmd_buf into (command name, remaining arguments), skipping
// leading/trailing/repeated spaces. `args_out` points directly into
// cmd_buf (no copy), at the first non-space argument character, or at
// an empty string if there are none.
static void tokenize_command(char *name_out, uint32_t name_out_size, char **args_out)
{
    char *p = cmd_buf;
    while (*p == ' ')
        p++;

    uint32_t i = 0;
    while (p[i] && p[i] != ' ' && i < name_out_size - 1)
    {
        name_out[i] = p[i];
        i++;
    }
    name_out[i] = '\0';

    p += i;
    while (*p == ' ')
        p++;

    *args_out = p;
}

void shell_execute(void)
{
    cmd_buf[cmd_len] = '\0';
    if (cmd_len == 0)
        return;

    char cmd_name[32];
    char *args;
    tokenize_command(cmd_name, sizeof(cmd_name), &args);

    if (str_eq(cmd_name, "help"))
    {
        if (args[0] != '\0')
            klog_error("help: too many arguments");
        else
            cmd_help();
    }
    else if (str_eq(cmd_name, "clear"))
    {
        if (args[0] != '\0')
            klog_error("clear: too many arguments");
        else
            cmd_clear();
    }
    else if (str_eq(cmd_name, "about"))
    {
        if (args[0] != '\0')
            klog_error("about: too many arguments");
        else
            cmd_about();
    }
    else if (str_eq(cmd_name, "mem"))
    {
        if (args[0] != '\0')
            klog_error("mem: too many arguments");
        else
            cmd_mem();
    }
    else if (str_eq(cmd_name, "uptime"))
    {
        if (args[0] != '\0')
            klog_error("uptime: too many arguments");
        else
            cmd_uptime();
    }
    else if (str_eq(cmd_name, "echo"))
        cmd_echo(args);
    else if (str_eq(cmd_name, "ls"))
        cmd_ls(args);
    else if (str_eq(cmd_name, "cd"))
    {
        if (args[0] == '\0')
            klog_error("cd: usage: cd <directory>");
        else
            cmd_cd(args);
    }
    else if (str_eq(cmd_name, "mkdir"))
    {
        if (args[0] == '\0')
            klog_error("mkdir: usage: mkdir <n>");
        else
            cmd_mkdir(args);
    }
    else if (str_eq(cmd_name, "touch"))
    {
        if (args[0] == '\0')
            klog_error("touch: usage: touch <n>");
        else
            cmd_touch(args);
    }
    else if (str_eq(cmd_name, "write"))
    {
        if (args[0] == '\0')
            klog_error("write: usage: write <filename> <content>");
        else
            cmd_write(args);
    }
    else if (str_eq(cmd_name, "cat"))
    {
        if (args[0] == '\0')
            klog_error("cat: usage: cat <filename>");
        else
            cmd_cat(args);
    }
    else if (str_eq(cmd_name, "rm"))
    {
        if (args[0] == '\0')
            klog_error("rm: usage: rm <n>");
        else
            cmd_rm(args);
    }
    else if (str_eq(cmd_name, "run"))
    {
        if (args[0] == '\0')
            klog_error("run: usage: run <file.axp>");
        else
            cmd_run(args);
    }
    else if (str_eq(cmd_name, "test"))
    {
        if (args[0] != '\0')
            klog_error("test: too many arguments");
        else
            ktest_run_all();
    }
    else if (str_eq(cmd_name, "ring3"))
    {
        if (args[0] != '\0')
            klog_error("ring3: too many arguments");
        else
            cmd_ring3();
    }
    else if (str_eq(cmd_name, "gfxtest"))
    {
        if (args[0] != '\0')
            klog_error("gfxtest: too many arguments");
        else
            cmd_gfxtest();
    }
    else if (str_eq(cmd_name, "apexui"))
    {
        if (args[0] != '\0')
            klog_error("apexui: too many arguments");
        else
            cmd_apexui();
    }
    else if (str_eq(cmd_name, "disktest"))
    {
        if (args[0] != '\0')
            klog_error("disktest: too many arguments");
        else
            cmd_disktest();
    }
    else if (str_eq(cmd_name, "arptest"))
        cmd_arptest(args);
    else if (str_eq(cmd_name, "ping"))
        cmd_ping(args);
    else if (str_eq(cmd_name, "dnstest"))
        cmd_dnstest(args);
    else if (str_eq(cmd_name, "tcptest"))
        cmd_tcptest(args);
    else if (str_eq(cmd_name, "serialmirror"))
        cmd_serialmirror(args);
    else if (str_eq(cmd_name, "reboot"))
    {
        if (args[0] != '\0')
            klog_error("reboot: too many arguments");
        else
            cmd_reboot();
    }
    else if (str_eq(cmd_name, "shutdown"))
    {
        if (args[0] != '\0')
            klog_error("shutdown: too many arguments");
        else
            cmd_shutdown();
    }
    else
    {
        klog_error_prefix();
        vga_puts("Unknown command: ");
        vga_puts(cmd_name);
        vga_puts("\nType 'help' for available commands.\n");
    }
}
