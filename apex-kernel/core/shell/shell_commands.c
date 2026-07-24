// apex-kernel/core/shell_commands.c
// ApexOS -- Shell built-in commands

#include "shell_commands.h"
#include "shell_path.h"
#include "shell_ui.h"
#include "../klog.h"
#include "../axf/axf_loader.h"
#include "../../drivers/vga.h"
#include "../../drivers/pit.h"
#include "../../mm/pmm.h"
#include "../../fs/axfs.h"
#include "../../fs/vfs.h"

void cmd_help(void)
{
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("Available commands:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("  help    - Show this help\n");
    vga_puts("  clear   - Clear screen\n");
    vga_puts("  about   - About ApexOS\n");
    vga_puts("  mem     - Memory stats\n");
    vga_puts("  uptime  - System uptime\n");
    vga_puts("  echo    - Echo text\n");
    vga_puts("  ls      - List files (current or given path)\n");
    vga_puts("  cd      - Change current directory\n");
    vga_puts("  mkdir   - Create a directory\n");
    vga_puts("  touch   - Create an empty file\n");
    vga_puts("  write   - Write text to a file (write <name> <text>)\n");
    vga_puts("  cat     - Show file contents\n");
    vga_puts("  rm      - Remove a file or empty directory\n");
    vga_puts("  run     - Run an .axp executable\n");
    vga_puts("  test    - Run kernel self-tests\n");
    vga_puts("  ring3   - Test Ring 3 userspace\n");
}

void cmd_clear(void)
{
    vga_clear();
    // Resyncs the hardware cursor immediately (not just at the end via
    // shell_print_prompt()) for instant visual feedback.
    cursor_update(vga_get_row(), vga_get_col());
}

void cmd_about(void)
{
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("ApexOS v0.0.1\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("  Architecture : x86-64\n");
    vga_puts("  Kernel       : Monolithic\n");
    vga_puts("  Built        : From scratch, no GRUB\n");
    vga_puts("  Features     : Preemptive multitasking, VMM, Heap\n");
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("  \"Built from scratch. No limits.\"\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

void cmd_mem(void)
{
    pmm_stats_t s;
    pmm_stats(&s);
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("Memory stats:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("  Total : ");
    vga_putdec(s.total_pages * 4096 / (1024 * 1024));
    vga_puts(" MB\n  Used  : ");
    vga_putdec(s.used_pages * 4096 / (1024 * 1024));
    vga_puts(" MB\n  Free  : ");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_putdec(s.free_pages * 4096 / (1024 * 1024));
    vga_puts(" MB\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

void cmd_uptime(void)
{
    uint64_t ticks = pit_get_ticks();
    uint64_t secs = ticks / 100;
    uint64_t mins = secs / 60;
    secs %= 60;
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("Uptime: ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_putdec(mins);
    vga_puts("m ");
    vga_putdec(secs);
    vga_puts("s\n");
}

void cmd_echo(const char *args)
{
    vga_puts(args);
    vga_puts("\n");
}

void cmd_ls(const char *arg)
{
    char abs_path[CWD_MAX_LEN];
    path_make_absolute(arg[0] ? arg : ".", abs_path, sizeof(abs_path));

    uint64_t dir_inode = vfs_resolve(abs_path);
    if (dir_inode == 0)
    {
        klog_error_prefix();
        vga_puts("ls: '");
        vga_puts(abs_path);
        vga_puts("' not found\n");
        return;
    }

    if (axfs_get_type(dir_inode) != AXFS_TYPE_DIR)
    {
        klog_error_prefix();
        vga_puts("ls: '");
        vga_puts(abs_path);
        vga_puts("' is not a directory\n");
        return;
    }

    axfs_dirent_info_t entries[64];
    uint32_t count = axfs_list_dir(dir_inode, entries, 64);

    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts(abs_path);
    vga_puts("/\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    uint32_t shown = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        if (str_eq(entries[i].name, ".") || str_eq(entries[i].name, ".."))
            continue;

        if (entries[i].type == AXFS_TYPE_DIR)
        {
            vga_set_color(VGA_LIGHT_BLUE, VGA_BLACK);
            vga_puts("  [DIR]  ");
        }
        else
        {
            vga_set_color(VGA_WHITE, VGA_BLACK);
            vga_puts("  [FILE] ");
        }
        vga_puts(entries[i].name);
        vga_puts("\n");
        shown++;
    }
    vga_set_color(VGA_WHITE, VGA_BLACK);

    if (shown == 0)
        vga_puts("  (empty)\n");
}

void cmd_cd(const char *arg)
{
    if (arg[0] == '\0')
    {
        klog_error("Usage: cd <path>");
        return;
    }

    char abs_path[CWD_MAX_LEN];
    path_make_absolute(arg, abs_path, sizeof(abs_path));

    uint64_t inode = vfs_resolve(abs_path);
    if (inode == 0)
    {
        klog_error_prefix();
        vga_puts("cd: '");
        vga_puts(abs_path);
        vga_puts("' not found\n");
        return;
    }

    if (axfs_get_type(inode) != AXFS_TYPE_DIR)
    {
        klog_error_prefix();
        vga_puts("cd: '");
        vga_puts(abs_path);
        vga_puts("' is not a directory\n");
        return;
    }

    uint32_t i = 0;
    while (abs_path[i] && i < CWD_MAX_LEN - 1)
    {
        cwd[i] = abs_path[i];
        i++;
    }
    cwd[i] = '\0';
}

void cmd_mkdir(const char *name)
{
    if (name[0] == '\0')
    {
        klog_error("Usage: mkdir <name>");
        return;
    }

    char abs_path[CWD_MAX_LEN];
    path_make_absolute(name, abs_path, sizeof(abs_path));

    uint64_t inode = vfs_create(abs_path, AXFS_TYPE_DIR);
    if (inode == 0)
    {
        klog_error_prefix();
        vga_puts("mkdir: could not create '");
        vga_puts(abs_path);
        vga_puts("' (already exists?)\n");
    }
}

void cmd_touch(const char *name)
{
    if (name[0] == '\0')
    {
        klog_error("Usage: touch <name>");
        return;
    }

    char abs_path[CWD_MAX_LEN];
    path_make_absolute(name, abs_path, sizeof(abs_path));

    uint64_t inode = vfs_create(abs_path, AXFS_TYPE_FILE);
    if (inode == 0)
    {
        klog_error_prefix();
        vga_puts("touch: could not create '");
        vga_puts(abs_path);
        vga_puts("' (already exists?)\n");
    }
}

void cmd_write(char *args)
{
    char *content = split_first_space(args);
    if (!content)
    {
        klog_error("Usage: write <filename> <content>");
        return;
    }

    char abs_path[CWD_MAX_LEN];
    path_make_absolute(args, abs_path, sizeof(abs_path));

    uint64_t inode = vfs_resolve(abs_path);
    if (inode == 0)
    {
        // Creates the file if it doesn't exist yet -- convenient for
        // quick testing without a separate `touch` first.
        inode = vfs_create(abs_path, AXFS_TYPE_FILE);
        if (inode == 0)
        {
            klog_error_prefix();
            vga_puts("write: could not create '");
            vga_puts(abs_path);
            vga_puts("'\n");
            return;
        }
    }

    uint32_t content_len = 0;
    while (content[content_len])
        content_len++;

    if (!axfs_write(inode, content, content_len, 0))
    {
        klog_error("write: failed (file too large or volume full)");
        return;
    }

    klog_ok_prefix();
    vga_putdec(content_len);
    vga_puts(" bytes written to '");
    vga_puts(abs_path);
    vga_puts("'\n");
}

void cmd_cat(const char *name)
{
    if (name[0] == '\0')
    {
        klog_error("Usage: cat <filename>");
        return;
    }

    char abs_path[CWD_MAX_LEN];
    path_make_absolute(name, abs_path, sizeof(abs_path));

    uint64_t inode = vfs_resolve(abs_path);
    if (inode == 0)
    {
        klog_error_prefix();
        vga_puts("cat: '");
        vga_puts(abs_path);
        vga_puts("' not found\n");
        return;
    }

    char buf[4096];
    uint64_t n = axfs_read(inode, buf, sizeof(buf) - 1, 0);
    buf[n] = '\0';

    vga_puts(buf);
    vga_puts("\n");
}

void cmd_rm(const char *name)
{
    if (name[0] == '\0')
    {
        klog_error("Usage: rm <name>");
        return;
    }

    char abs_path[CWD_MAX_LEN];
    path_make_absolute(name, abs_path, sizeof(abs_path));

    char parent_path[CWD_MAX_LEN];
    char final_name[AXFS_MAX_NAME_LEN + 1];
    if (!split_path_parent_name(abs_path, parent_path, sizeof(parent_path),
                                final_name, sizeof(final_name)))
    {
        klog_error_prefix();
        vga_puts("rm: invalid path '");
        vga_puts(abs_path);
        vga_puts("'\n");
        return;
    }

    uint64_t parent_inode = vfs_resolve(parent_path);
    if (parent_inode == 0 || !axfs_unlink(parent_inode, final_name))
    {
        klog_error_prefix();
        vga_puts("rm: could not remove '");
        vga_puts(abs_path);
        vga_puts("' (not found, or directory not empty)\n");
    }
}

void cmd_run(const char *name)
{
    char abs_path[CWD_MAX_LEN];
    path_make_absolute(name, abs_path, sizeof(abs_path));

    uint64_t inode = vfs_resolve(abs_path);
    if (inode == 0)
    {
        klog_error_prefix();
        vga_puts("run: '");
        vga_puts(abs_path);
        vga_puts("' not found\n");
        return;
    }

    if (axfs_get_type(inode) != AXFS_TYPE_FILE)
    {
        klog_error_prefix();
        vga_puts("run: '");
        vga_puts(abs_path);
        vga_puts("' is not a file\n");
        return;
    }

    // The loader needs the parent directory to look up any .axl files
    // imported by this .axp (v1 -- searched in the same directory, see
    // axf_loader.c).
    char parent_path[CWD_MAX_LEN];
    char final_name[AXFS_MAX_NAME_LEN + 1];
    if (!split_path_parent_name(abs_path, parent_path, sizeof(parent_path),
                                final_name, sizeof(final_name)))
    {
        klog_error_prefix();
        vga_puts("run: invalid path '");
        vga_puts(abs_path);
        vga_puts("'\n");
        return;
    }

    uint64_t dir_inode = vfs_resolve(parent_path);
    if (dir_inode == 0)
    {
        klog_error("run: could not resolve parent directory");
        return;
    }

    axf_load_and_run(inode, dir_inode);
}