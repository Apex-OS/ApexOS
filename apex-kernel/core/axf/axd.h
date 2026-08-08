// apex-kernel/core/axf/axd.h
// ApexOS -- AXD loader (loads a .axd from AXFS in Ring 0, kernel-only)
//
// Reuses the AXF format (same header/sections/exports as .axl) but:
//   - maps pages WITHOUT VMM_USER (kernel-only, not accessible from
//     Ring 3)
//   - calls the "driver_init" export directly in Ring 0 right after
//     loading (no Ring 3 jump, no syscall for init)
//   - keeps the loaded driver in a global table (loaded_drivers[]),
//     later reachable via SYS_DRIVER_CALL (see syscall.c)
//
// V1 convention: a .axd MUST export "driver_init" (entry point,
// called once at load time). Other exports are called later by their
// index in the export table (func_id), not by name -- see axd_call().

#ifndef APEX_AXD_H
#define APEX_AXD_H

#include "../../include/apex/types.h"

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

// Loads the .axd identified by `inode_num`, maps it into kernel-only
// pages, copies its sections, resolves its export table, then calls
// "driver_init" in Ring 0. Returns the driver_id (>= 0, index into
// loaded_drivers[]) on success, -1 on failure (invalid header, no
// driver_init export, no slot available...).
int axd_load(uint64_t inode_num);

// Calls exported function number `func_id` (0-based index into the
// export table, NOT "driver_init" which is already called at load
// time) of driver `driver_id`, with a single generic argument.
// Returns the function's return value, or (uint64_t)-1 if
// driver_id/func_id is invalid or the driver is inactive.
uint64_t axd_call(int driver_id, int func_id, uint64_t arg);

#endif // APEX_AXD_H