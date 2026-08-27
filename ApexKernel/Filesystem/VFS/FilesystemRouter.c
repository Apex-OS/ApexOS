// ApexKernel/Filesystem/VFS/FilesystemRouter.c
// ApexOS -- VFS path resolution routing (dispatches to the concrete filesystem)

#include "Filesystem/VFS/FilesystemRouter.h"
#include "Filesystem/VFS/VFSInternal.h"
#include "Filesystem/VFS/VolumeSyntax.h"

uint64_t vfs_resolve(const char *path)
{
    char volume_name[VFS_MAX_VOLUME_NAME];
    if (!vfs_parse_volume(path, volume_name, sizeof(volume_name)))
        return 0;

    int idx = vfs_find_volume(volume_name);
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
        return axfs_resolve_path(g_volumes[idx].axfs_vol, AXFS_ROOT_INODE, rest);
    default:
        return 0;
    }
}

uint64_t vfs_create(const char *path, axfs_inode_type_t type)
{
    char volume_name[VFS_MAX_VOLUME_NAME];
    if (!vfs_parse_volume(path, volume_name, sizeof(volume_name)))
        return 0;

    int idx = vfs_find_volume(volume_name);
    if (idx < 0)
        return 0;

    const char *rest = vfs_path_after_volume(path);
    if (!rest)
        return 0;

    switch (g_volumes[idx].type)
    {
    case VFS_FS_AXFS:
        return axfs_create_path(g_volumes[idx].axfs_vol, AXFS_ROOT_INODE, rest, type);
    default:
        return 0;
    }
}

axfs_volume_t *vfs_get_volume(const char *path)
{
    char volume_name[VFS_MAX_VOLUME_NAME];
    if (!vfs_parse_volume(path, volume_name, sizeof(volume_name)))
        return NULL;

    int idx = vfs_find_volume(volume_name);
    if (idx < 0 || g_volumes[idx].type != VFS_FS_AXFS)
        return NULL;

    return g_volumes[idx].axfs_vol;
}
