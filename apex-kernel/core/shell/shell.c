// apex-kernel/core/shell.c
// ApexOS -- Kernel shell
//
// Owns the command line input state (cmd_buf/cmd_len/cmd_cursor) and
// the main read-eval loop. Command implementations, path resolution,
// display/redraw, and history navigation live in their own files (see
// shell_commands.c, shell_path.c, shell_ui.c, shell_history.c) --
// split out of this file during the 0.1.0 refactor.

#include "shell.h"
#include "shell_path.h"
#include "shell_ui.h"
#include "shell_history.h"
#include "shell_commands.h"
#include "../ktest.h"
#include "../syscall.h"
#include "../userspace.h"
#include "../../drivers/vga.h"
#include "../../drivers/keyboard.h"
#include "../../mm/vmm.h"
#include "../../mm/pmm.h"
#include "../klog.h"

char cmd_buf[SHELL_BUFFER_SIZE];
uint32_t cmd_len = 0;
uint32_t cmd_cursor = 0;

// Defined in syscall.c, set by the naked syscall_set_shell_return_point()
// / shell_return_jump() pair (see cmd_ring3() below and axf_loader.c,
// which relies on the exact same mechanism).
extern volatile uint64_t shell_return_bypass;

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

// Jumps user_main (see usertest.c) into Ring 3 as a quick smoke test
// of the userspace/syscall path, independent of the AXF loader. Kept
// inline in shell_execute() rather than split into shell_commands.c:
// it manipulates the shell_return_bypass jump mechanism directly (the
// same one axf_load_and_run() relies on), so it stays next to the
// dispatcher for visibility on that fragile code path.
static void cmd_ring3(void)
{
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("Jumping to Ring 3...\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    uint64_t user_main_size = (uint64_t)user_main_end - (uint64_t)user_main_start;
    if (user_main_size == 0 || user_main_size > 4096)
    {
        vga_puts("ring3: user_main too large or size=0 -- aborting\n");
        return;
    }

    void *code_phys = pmm_alloc_page();
    vmm_map(USER_CODE_ADDR, (uint64_t)code_phys, VMM_USER_RW);

    uint8_t *src = (uint8_t *)user_main_start;
    uint8_t *dst = (uint8_t *)USER_CODE_ADDR;
    for (uint64_t i = 0; i < 4096; i++)
        dst[i] = (i < user_main_size) ? src[i] : 0;

    for (uint64_t addr = USER_STACK_ADDR;
         addr < USER_STACK_ADDR + USER_STACK_SIZE;
         addr += 4096)
    {
        void *phys = pmm_alloc_page();
        vmm_map(addr, (uint64_t)phys, VMM_USER_RW);
    }

    uint64_t stack_top = USER_STACK_ADDR + USER_STACK_SIZE;

    syscall_set_shell_return_point();
    syscall_mark_shell_return_point_ready();

    if (shell_return_bypass == 0)
    {
        // First pass: actually jump.
        userspace_jump(USER_CODE_ADDR, stack_top);
    }

    // Reached only after SYS_EXIT's return jump (shell_return_bypass
    // == 1 at this point, see syscall.c). The syscall handler leaves
    // interrupts disabled (cli) right before the jump back here --
    // `sti` must stay at the very end of shell_return_jump(), never
    // before the RSP restore (a race condition here was fixed once
    // already: an interrupt firing between `sti` and the RSP restore
    // corrupts the stack).
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("Back in kernel shell.\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

static void shell_execute(void)
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
            klog_error("mkdir: usage: mkdir <name>");
        else
            cmd_mkdir(args);
    }
    else if (str_eq(cmd_name, "touch"))
    {
        if (args[0] == '\0')
            klog_error("touch: usage: touch <name>");
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
            klog_error("rm: usage: rm <name>");
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
    else
    {
        klog_error_prefix();
        vga_puts("Unknown command: ");
        vga_puts(cmd_name);
        vga_puts("\nType 'help' for available commands.\n");
    }
}

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
    vga_puts("  ApexOS v0.0.1 - Type 'help'\n\n");

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

        uint8_t c = keyboard_getchar();
        if (c == 0)
            continue;

        if (c == '\n')
        {
            cmd_buf[cmd_len] = '\0';
            vga_set_cursor_pos(prompt_row, shell_prompt_len + cmd_len);
            vga_putchar('\n');
            shell_history_push(cmd_buf);
            shell_execute();
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
                vga_set_cursor_pos(prompt_row, shell_prompt_len + cmd_cursor);
                cursor_update(prompt_row, shell_prompt_len + cmd_cursor);
            }
        }
        else if (c == KEY_ARROW_RIGHT)
        {
            if (cmd_cursor < cmd_len)
            {
                cmd_cursor++;
                vga_set_cursor_pos(prompt_row, shell_prompt_len + cmd_cursor);
                cursor_update(prompt_row, shell_prompt_len + cmd_cursor);
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