// ApexKernel/Includes/Filesystem/VFS/VFSInternal.h
// ApexOS -- Shared internal state for the VFS module
//
// g_volumes[] is defined once in VolumeRegistry.c and shared via
// extern with FilesystemRouter.c (which needs find_volume()). Not
// part of the public VolumeRegistry.h/FilesystemRouter.h API.

#ifndef APEX_FILESYSTEM_VFS_INTERNAL_H
#define APEX_FILESYSTEM_VFS_INTERNAL_H

#include "Filesystem/VFS/VolumeRegistry.h"

typedef struct
{
    char name[VFS_MAX_VOLUME_NAME];
    vfs_fs_type_t type;
    bool in_use;
    axfs_volume_t *axfs_vol; // NULL unless type == VFS_FS_AXFS
} vfs_volume_t;

extern vfs_volume_t g_volumes[VFS_MAX_VOLUMES];

// Finds a volume by name. Returns its index in g_volumes, or -1 if
// not found.
int vfs_find_volume(const char *name);

#endif // APEX_FILESYSTEM_VFS_INTERNAL_H
