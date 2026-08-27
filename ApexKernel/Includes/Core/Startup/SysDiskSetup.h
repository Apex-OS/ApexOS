// ApexKernel/Includes/Core/Startup/SysDiskSetup.h
// ApexOS -- @SysDisk volume bootstrap

#ifndef APEX_CORE_STARTUP_SYS_DISK_SETUP_H
#define APEX_CORE_STARTUP_SYS_DISK_SETUP_H

#include "Filesystem/AXFS/AXFS.h"

// Sets up @SysDisk: tries the real AHCI disk first (persistent across
// boots), falls back to a RAM disk if no usable drive is found, then
// mounts (or formats + mounts, on first boot) and registers it with
// the VFS as "SysDisk". Panics if neither backend can be set up, or if
// format/mount fails. Returns a pointer to the static volume instance
// this function owns -- callers (e.g. Startup/TestFilesSetup.c) that
// need the volume for further setup use this rather than declaring
// their own.
axfs_volume_t *core_setup_sysdisk(void);

#endif // APEX_CORE_STARTUP_SYS_DISK_SETUP_H
