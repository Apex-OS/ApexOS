// apex-kernel/fs/vfs.c
// ApexOS -- VFS (Virtual File System) implementation

#include "vfs.h"
#include "../core/klog.h"
#include "../drivers/vga.h"

typedef struct
{
    char name[VFS_MAX_VOLUME_NAME];
    vfs_fs_type_t type;
    bool in_use;
} vfs_volume_t;

static vfs_volume_t g_volumes[VFS_MAX_VOLUMES];

// String helpers (no libc in freestanding).
static uint32_t str_len(const char *s)
{
    uint32_t n = 0;
    while (s[n])
        n++;
    return n;
}

static bool str_eq_n(const char *a, const char *b, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

static void str_copy_n(char *dst, const char *src, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
        dst[i] = src[i];
}

bool vfs_register_volume(const char *name, vfs_fs_type_t type)
{
    uint32_t name_len = str_len(name);
    if (name_len == 0 || name_len >= VFS_MAX_VOLUME_NAME)
        return false;

    // Rejects a name already in use -- two volumes can't share a name
    // (would make "@name" paths ambiguous).
    for (uint32_t i = 0; i < VFS_MAX_VOLUMES; i++)
    {
        if (g_volumes[i].in_use && str_eq_n(g_volumes[i].name, name, name_len) &&
            g_volumes[i].name[name_len] == '\0')
            return false;
    }

    for (uint32_t i = 0; i < VFS_MAX_VOLUMES; i++)
    {
        if (!g_volumes[i].in_use)
        {
            str_copy_n(g_volumes[i].name, name, name_len);
            g_volumes[i].name[name_len] = '\0';
            g_volumes[i].type = type;
            g_volumes[i].in_use = true;

            klog_ok_prefix();
            vga_puts("Volume registered: @");
            vga_puts(g_volumes[i].name);
            vga_puts("\n");
            return true;
        }
    }

    klog_error("vfs_register_volume: volume table full");
    return false;
}

bool vfs_parse_volume(const char *path, char *out, uint32_t out_size)
{
    // Expected format: "/@VolumeName/rest/of/path" or "/@VolumeName".
    if (path[0] != '/' || path[1] != '@')
        return false;

    const char *start = path + 2; // skip "/@"
    uint32_t i = 0;
    while (start[i] && start[i] != '/' && i < out_size - 1)
    {
        out[i] = start[i];
        i++;
    }
    out[i] = '\0';

    return i > 0; // an empty volume name ("/@/...") is invalid
}

const char *vfs_path_after_volume(const char *path)
{
    if (path[0] != '/' || path[1] != '@')
        return NULL;

    const char *p = path + 2;
    while (*p && *p != '/')
        p++;

    if (*p == '/')
        return p + 1; // points right after the '/'
    return p;         // end of string -- path was just "/@Volume", rest = ""
}

// Finds a volume by name. Returns its index in g_volumes, or -1 if
// not found.
static int find_volume(const char *name)
{
    uint32_t name_len = str_len(name);
    for (int i = 0; i < VFS_MAX_VOLUMES; i++)
    {
        if (g_volumes[i].in_use && str_eq_n(g_volumes[i].name, name, name_len) &&
            g_volumes[i].name[name_len] == '\0')
            return i;
    }
    return -1;
}

uint64_t vfs_resolve(const char *path)
{
    char volume_name[VFS_MAX_VOLUME_NAME];
    if (!vfs_parse_volume(path, volume_name, sizeof(volume_name)))
        return 0;

    int idx = find_volume(volume_name);
    if (idx < 0)
        return 0;

    const char *rest = vfs_path_after_volume(path);
    if (!rest)
        return 0;

    // V1: only one filesystem type is actually supported (AXFS). The
    // switch is already set up to accept more later (FAT32 for a USB
    // drive, etc.) without touching the public API.
    switch (g_volumes[idx].type)
    {
    case VFS_FS_AXFS:
        return axfs_resolve_path(AXFS_ROOT_INODE, rest);
    default:
        return 0;
    }
}

uint64_t vfs_create(const char *path, axfs_inode_type_t type)
{
    char volume_name[VFS_MAX_VOLUME_NAME];
    if (!vfs_parse_volume(path, volume_name, sizeof(volume_name)))
        return 0;

    int idx = find_volume(volume_name);
    if (idx < 0)
        return 0;

    const char *rest = vfs_path_after_volume(path);
    if (!rest)
        return 0;

    switch (g_volumes[idx].type)
    {
    case VFS_FS_AXFS:
        return axfs_create_path(AXFS_ROOT_INODE, rest, type);
    default:
        return 0;
    }
}