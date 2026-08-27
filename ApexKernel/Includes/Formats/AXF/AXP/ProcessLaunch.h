// ApexKernel/Includes/Formats/AXF/AXP/ProcessLaunch.h
// ApexOS -- AXP process launch (stack setup + Ring 3 jump)

#ifndef APEX_FORMATS_AXF_AXP_PROCESS_LAUNCH_H
#define APEX_FORMATS_AXF_AXP_PROCESS_LAUNCH_H

#include "Filesystem/AXFS/AXFS.h"

// Loads a .axp from `vol` (inode `inode_num`), resolves any imports
// from .axl libraries in `dir_inode`, maps its sections into memory,
// then jumps to Ring 3 -- same return mechanism as the shell's
// `ring3` command (see Core/Syscalls/Handlers/SysExit.c:
// syscall_set_shell_return_point / shell_return_bypass), reused here
// as-is. Returns true once the program has run and exited (SYS_EXIT
// jumped back here), false if loading failed before ever reaching
// Ring 3.
bool axf_load_and_run(axfs_volume_t *vol, uint64_t inode_num, uint64_t dir_inode);

#endif // APEX_FORMATS_AXF_AXP_PROCESS_LAUNCH_H
