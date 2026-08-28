// ApexKernel/Includes/Formats/AXF/AXD/HeaderValidation.h
// ApexOS -- AXD loader (loads a .axd from AXFS in Ring 0, kernel-only)
//
// Reuses the AXF format (same header/sections/exports as .axl) but:
//   - maps pages WITHOUT VMM_USER (kernel-only, not accessible from
//     Ring 3)
//   - calls the "driver_init" export directly in Ring 0 right after
//     loading (no Ring 3 jump, no syscall for init)
//   - keeps the loaded driver in a global table (loaded_drivers[]),
//     later reachable via SYS_DRIVER_CALL (see
//     Core/Syscalls/Handlers/SysDriverCall.c)
//
// V1 convention: a .axd MUST export "driver_init" (entry point,
// called once at load time). Other exports are called later by their
// index in the export table (func_id), not by name -- see
// DriverInvocation.c's axd_call().

#ifndef APEX_FORMATS_AXF_AXD_HEADER_VALIDATION_H
#define APEX_FORMATS_AXF_AXD_HEADER_VALIDATION_H

#include "Core/Types.h"
#include "Filesystem/AXFS/AXFS.h"
#include "Formats/AXF/AXFFormat.h"

#define AXD_MAX_DRIVERS 32
#define AXD_MAX_NAME 32
#define AXD_INIT_EXPORT_NAME "driver_init"

typedef struct
{
    char name[AXD_MAX_NAME];     // .axd file name (e.g. "stub.axd")
    uint64_t base_addr;          // virtual address the driver is mapped at (kernel-only)
    uint64_t export_offsets[16]; // absolute addresses of exported functions, in file order
    uint32_t export_count;
    bool active;
} axd_driver_t;

extern axd_driver_t loaded_drivers[AXD_MAX_DRIVERS];
extern int axd_driver_count;

// Validates the AXF header read into `header` (magic, version, type
// == AXF_TYPE_AXD, section/export counts within v1's supported
// range). Returns false (and logs why) if invalid.
bool axd_validate_header(const axf_header_t *header);

// Loads the .axd identified by `inode_num` on volume `vol`, maps it
// into kernel-only pages, copies its sections, resolves its export
// table, then calls "driver_init" in Ring 0. Returns the driver_id
// (>= 0, index into loaded_drivers[]) on success, -1 on failure
// (invalid header, no driver_init export, no slot available...).
int axd_load(axfs_volume_t *vol, uint64_t inode_num);

#endif // APEX_FORMATS_AXF_AXD_HEADER_VALIDATION_H
