// ApexKernel/Formats/AXF/AXP/ProcessLaunch.c
// ApexOS -- AXP process launch (stack setup + Ring 3 jump)

#include "Formats/AXF/AXP/ProcessLaunch.h"
#include "Formats/AXF/AXP/HeaderValidation.h"
#include "Formats/AXF/AXP/ImportResolution.h"
#include "Formats/AXF/AXP/SectionLoader.h"
#include "Core/Userspace.h"
#include "Core/Syscalls/Handlers/SysExit.h"
#include "Core/Memory/Physical/PageBitmap.h"
#include "Core/Memory/Virtual/PageTables.h"
#include "Logs/Log.h"

// Return point shared with `ring3` (see
// Core/Syscalls/Handlers/SysExit.c). 0 = first pass (haven't jumped
// to Ring 3 yet), 1 = returning from a SYS_EXIT.
extern volatile uint64_t shell_return_bypass;

// Max accepted size for a whole AXF file in V1 -- only a single code
// page is allocated for now (same limit as `ring3`/user_main);
// multi-page/multi-section support will come later if needed.
#define AXF_MAX_FILE_SIZE 4096u

bool axf_load_and_run(axfs_volume_t *vol, uint64_t inode_num, uint64_t dir_inode)
{
    static uint8_t filebuf[AXF_MAX_FILE_SIZE];
    uint64_t read_bytes = axfs_read(vol, inode_num, filebuf, sizeof(filebuf), 0);

    if (read_bytes < sizeof(axf_header_t))
    {
        klog_error("axf_load: file too small to contain a valid header");
        return false;
    }

    axf_header_t header;
    uint8_t *hb = (uint8_t *)&header;
    for (uint32_t i = 0; i < sizeof(header); i++)
        hb[i] = filebuf[i];

    if (!axp_validate_header(&header))
        return false;

    // Resolves imports (needed .axl files) BEFORE loading/mapping the
    // .axp itself: if an import fails, this bails out cleanly without
    // having touched the .axp's memory yet (nothing to clean up).
    if (!axp_resolve_imports(vol, dir_inode, &header, filebuf, read_bytes))
        return false;

    bool entry_point_valid = false;
    if (!axp_load_sections(&header, filebuf, read_bytes, &entry_point_valid))
        return false;

    // Allocates the user stack (identical to `ring3`).
    for (uint64_t addr = USER_STACK_ADDR;
         addr < USER_STACK_ADDR + USER_STACK_SIZE;
         addr += 4096)
    {
        void *phys = pmm_alloc_page();
        if (!phys)
        {
            klog_error("axf_load: out of physical memory (stack)");
            return false;
        }
        vmm_map(addr, (uint64_t)phys, VMM_USER_RW);
    }

    uint64_t stack_top = USER_STACK_ADDR + USER_STACK_SIZE; // already 16-byte aligned (see Shell.c/ring3)

    // Jumps to Ring 3, same return mechanism as `ring3`.
    syscall_set_shell_return_point();
    syscall_mark_shell_return_point_ready();

    if (shell_return_bypass == 0)
    {
        // First pass: actually jump.
        userspace_jump(header.entry_point, stack_top);
    }

    // Reached only after the loaded program's SYS_EXIT
    // (shell_return_bypass == 1 at this point).
    klog_ok("axf_load: program exited, back in kernel");

    return true;
}
