// apex-kernel/core/axf/axf_loader.h
// ApexOS -- AXF loader (loads a .axp from AXFS and runs it)

#ifndef APEX_AXF_LOADER_H
#define APEX_AXF_LOADER_H

#include "../../include/apex/types.h"

// Loads the AXF file identified by `inode_num` (already resolved by
// the caller via vfs_resolve/axfs_lookup), validates its header,
// resolves any imports (.axl files it needs, looked up in `dir_inode`
// -- V1: same directory as the .axp itself), maps all its sections
// into memory, then jumps to its entry point in Ring 3.
//
// Does NOT return until the loaded program calls exit() (same return
// mechanism as `ring3` -- see syscall.c).
//
// Returns false if loading fails BEFORE the jump (invalid header,
// section too large, import not found, volume full, etc.) -- in that
// case the program never started running. Returns true after a
// normal exit().
bool axf_load_and_run(uint64_t inode_num, uint64_t dir_inode);

#endif // APEX_AXF_LOADER_H