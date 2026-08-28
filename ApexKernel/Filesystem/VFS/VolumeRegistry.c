// ApexKernel/Filesystem/VFS/VolumeRegistry.c
// ApexOS -- VFS volume table management

#include "Filesystem/VFS/VolumeRegistry.h"
#include "Filesystem/VFS/VFSInternal.h"
#include "Drivers/Display/VGA.h"
#include "Logs/Log.h"

vfs_volume_t g_volumes[VFS_MAX_VOLUMES];

// String helpers (no libc in freestanding).
// TODO: duplicated across several modules (Filesystem/AXFS/,
// Formats/AXF/) -- candidate for centralizing in ApexLib/String/ once
// that's populated.
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

bool vfs_register_volume(const char *name, vfs_fs_type_t type, axfs_volume_t *axfs_vol)
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
            g_volumes[i].axfs_vol = axfs_vol;
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

int vfs_find_volume(const char *name)
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
