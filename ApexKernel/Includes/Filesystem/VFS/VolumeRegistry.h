// ApexKernel/Includes/Filesystem/VFS/VolumeRegistry.h
// ApexOS -- VFS (Virtual File System)
//
// Abstraction layer above concrete filesystems (AXFS for now,
// potentially FAT32/ISO9660 later for reading non-ApexOS USB/CD/DVD
// disks). Deals in full ABSOLUTE paths, prefixed with a volume name:
//
//   /@SysDisk/Home/Users/Zeemoud/Documents/photo.png
//   ^          ^
//   |          `-- rest of the path, delegated to the volume's driver
//   `-- volume prefix ("@VolumeName"), always first
//
// The leading '/' marks an absolute path (the only form supported for
// now -- no path relative to a "current directory" at the VFS level
// yet; Core/Shell/PathResolver.c handles its own navigation, tracking
// the current directory and building absolute paths before calling
// into the VFS).
//
// V1: a single real volume (SysDisk, backed by AXFS). The volume
// table is designed to support several from the start (USB, DataDisk,
// etc. later) without needing a redesign.

#ifndef APEX_FILESYSTEM_VFS_VOLUME_REGISTRY_H
#define APEX_FILESYSTEM_VFS_VOLUME_REGISTRY_H

#include "Core/Types.h"
#include "Filesystem/AXFS/AXFS.h"

#define VFS_MAX_VOLUMES 8
#define VFS_MAX_VOLUME_NAME 32

typedef enum
{
    VFS_FS_NONE = 0,
    VFS_FS_AXFS = 1,
    // VFS_FS_FAT32, VFS_FS_ISO9660... will come later for non-ApexOS
    // removable media.
} vfs_fs_type_t;

// Registers a volume in the VFS table. `name` is what appears after
// the '@' in paths (e.g. "SysDisk"). Only VFS_FS_AXFS is actually
// supported for now. `axfs_vol` is the already-mounted AXFS volume
// this name should resolve to (see AXFS.h's axfs_volume_t) --
// ownership/storage stays with the caller (typically a static
// instance in Core/Startup/SysDiskSetup.c), the VFS only keeps a
// pointer to it.
bool vfs_register_volume(const char *name, vfs_fs_type_t type, axfs_volume_t *axfs_vol);

#endif // APEX_FILESYSTEM_VFS_VOLUME_REGISTRY_H
