// ApexKernel/Core/Shell/Commands/FilesystemCommands.c
// ApexOS -- Shell filesystem commands

#include "Core/Shell/Commands/FilesystemCommands.h"
#include "Core/Shell/PathResolver.h"
#include "Filesystem/VFS/FilesystemRouter.h"
#include "Filesystem/AXFS/Directory/Listing.h"
#include "Filesystem/AXFS/Directory/TypeQuery.h"
#include "Filesystem/AXFS/Content/FileWrite.h"
#include "Filesystem/AXFS/Content/FileRead.h"
#include "Filesystem/AXFS/Entry/Unlink.h"
#include "Formats/AXF/AXP/ProcessLaunch.h"
#include "Drivers/Display/VGA.h"
#include "ApexLib/Logs/Log.h"

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

    axfs_volume_t *vol = vfs_get_volume(abs_path);
    if (!vol)
    {
        klog_error_prefix();
        vga_puts("ls: '");
        vga_puts(abs_path);
        vga_puts("': unknown volume\n");
        return;
    }

    if (axfs_get_type(vol, dir_inode) != AXFS_TYPE_DIR)
    {
        klog_error_prefix();
        vga_puts("ls: '");
        vga_puts(abs_path);
        vga_puts("' is not a directory\n");
        return;
    }

    axfs_dirent_info_t entries[64];
    uint32_t count = axfs_list_dir(vol, dir_inode, entries, 64);

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

    axfs_volume_t *vol = vfs_get_volume(abs_path);
    if (!vol)
    {
        klog_error_prefix();
        vga_puts("cd: '");
        vga_puts(abs_path);
        vga_puts("': unknown volume\n");
        return;
    }

    if (axfs_get_type(vol, inode) != AXFS_TYPE_DIR)
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

    axfs_volume_t *vol = vfs_get_volume(abs_path);
    if (!vol || !axfs_write(vol, inode, content, content_len, 0))
    {
        klog_error("write: failed (file too large, volume full, or unknown volume)");
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

    axfs_volume_t *vol = vfs_get_volume(abs_path);
    if (!vol)
    {
        klog_error_prefix();
        vga_puts("cat: '");
        vga_puts(abs_path);
        vga_puts("': unknown volume\n");
        return;
    }

    char buf[4096];
    uint64_t n = axfs_read(vol, inode, buf, sizeof(buf) - 1, 0);
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
    axfs_volume_t *vol = vfs_get_volume(parent_path);
    if (parent_inode == 0 || !vol || !axfs_unlink(vol, parent_inode, final_name))
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

    axfs_volume_t *vol = vfs_get_volume(abs_path);
    if (!vol)
    {
        klog_error_prefix();
        vga_puts("run: '");
        vga_puts(abs_path);
        vga_puts("': unknown volume\n");
        return;
    }

    if (axfs_get_type(vol, inode) != AXFS_TYPE_FILE)
    {
        klog_error_prefix();
        vga_puts("run: '");
        vga_puts(abs_path);
        vga_puts("' is not a file\n");
        return;
    }

    // The loader needs the parent directory to look up any .axl files
    // imported by this .axp (v1 -- searched in the same directory, see
    // ApexKernel/Formats/AXF/AXL/LibraryLoader.c).
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

    axf_load_and_run(vol, inode, dir_inode);
}
