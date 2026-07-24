// apex-kernel/core/axf/axf_loader.c
// ApexOS -- AXF loader
//
// Loads a .axp file from AXFS, resolves any imports from .axl
// libraries, maps its sections into memory, then jumps to Ring 3 --
// same return mechanism as the shell's `ring3` command (see
// syscall.c: syscall_set_shell_return_point / shell_return_bypass),
// reused here as-is.

#include "axf_loader.h"
#include "../syscall.h"
#include "../userspace.h"
#include "../klog.h"
#include "../../fs/axfs.h"
#include "../../mm/pmm.h"
#include "../../mm/vmm.h"
#include "../../drivers/vga.h"
#include "../../../apex-os/formats/axf/axf.h"

// Return point shared with `ring3` (see syscall.c). 0 = first pass
// (haven't jumped to Ring 3 yet), 1 = returning from a SYS_EXIT.
extern volatile uint64_t shell_return_bypass;

// Max accepted size for a whole AXF file in V1 -- only a single code
// page is allocated for now (same limit as `ring3`/user_main);
// multi-page/multi-section support will come later if needed.
#define AXF_MAX_FILE_SIZE 4096u

static bool str_eq(const char *a, const char *b)
{
    while (*a && *b)
    {
        if (*a != *b)
            return false;
        a++;
        b++;
    }
    return *a == *b;
}

// Loads a .axl (looked up by name in `dir_inode`), maps it at
// USER_LIB_ADDR, and returns the ABSOLUTE address of the function
// `func_name` found in its export table. Returns 0 on failure
// (library not found, wrong type, function missing...).
//
// V1 known limitation: if several imports come from the SAME .axl, it
// gets reloaded/remapped each time (no cache). If two imports come
// from TWO different .axl files, the second one overwrites the
// first's mapping at USER_LIB_ADDR (a single fixed address) -- so in
// V1 a .axp can really only rely on ONE .axl at a time (but can
// import several FUNCTIONS from that same .axl). Lifting this
// limitation would need dynamic management of multiple memory regions
// per process, planned for later.
static uint64_t load_axl_and_resolve(uint64_t dir_inode, const char *lib_name,
                                     const char *func_name)
{
    uint64_t lib_inode = axfs_lookup(dir_inode, lib_name);
    if (lib_inode == 0)
    {
        klog_error_prefix();
        vga_puts("axf_load: import: library '");
        vga_puts(lib_name);
        vga_puts("' not found\n");
        return 0;
    }

    static uint8_t filebuf[AXF_MAX_FILE_SIZE];
    uint64_t read_bytes = axfs_read(lib_inode, filebuf, sizeof(filebuf), 0);
    if (read_bytes < sizeof(axf_header_t))
    {
        klog_error("axf_load: import: library file too small");
        return 0;
    }

    axf_header_t header;
    uint8_t *hb = (uint8_t *)&header;
    for (uint32_t i = 0; i < sizeof(header); i++)
        hb[i] = filebuf[i];

    if (header.magic != AXF_MAGIC || header.version != AXF_VERSION)
    {
        klog_error("axf_load: import: invalid library file");
        return 0;
    }

    if (header.type != AXF_TYPE_AXL)
    {
        klog_error_prefix();
        vga_puts("axf_load: import: '");
        vga_puts(lib_name);
        vga_puts("' is not a .axl file\n");
        return 0;
    }

    if (header.section_count == 0 || header.section_count > 8)
    {
        klog_error("axf_load: import: invalid section_count in library");
        return 0;
    }

    // Maps the .axl at USER_LIB_ADDR (same logic as a .axp, see
    // below -- a single 4KB page in V1).
    void *lib_phys = pmm_alloc_page();
    if (!lib_phys)
    {
        klog_error("axf_load: import: out of physical memory");
        return 0;
    }
    vmm_map(USER_LIB_ADDR, (uint64_t)lib_phys, VMM_USER_RW);

    uint8_t *dst_page = (uint8_t *)USER_LIB_ADDR;
    for (uint32_t i = 0; i < 4096; i++)
        dst_page[i] = 0;

    for (uint32_t s = 0; s < header.section_count; s++)
    {
        axf_section_t sec;
        uint64_t sec_offset = header.section_table_offset + s * sizeof(axf_section_t);
        if (sec_offset + sizeof(sec) > read_bytes)
        {
            klog_error("axf_load: import: section table extends beyond file");
            return 0;
        }
        uint8_t *sb = (uint8_t *)&sec;
        for (uint32_t i = 0; i < sizeof(sec); i++)
            sb[i] = filebuf[sec_offset + i];

        // .axl files are compiled to be loaded at USER_CODE_ADDR (same
        // convention as .axp -- a single "standard" base address in
        // V1) but physically MAPPED at USER_LIB_ADDR. Each section's
        // offset is therefore computed relative to the file's own
        // base address (header.base_addr, or AXF_DEFAULT_BASE_ADDR),
        // not relative to USER_LIB_ADDR directly -- this lets the same
        // generator tool produce a .axp or a .axl without an address
        // distinction.
        uint64_t lib_base = header.base_addr ? header.base_addr : AXF_DEFAULT_BASE_ADDR;

        if (sec.vaddr < lib_base || sec.vaddr + sec.mem_size > lib_base + 4096)
        {
            klog_error("axf_load: import: library section out of supported range");
            return 0;
        }

        uint64_t page_offset = sec.vaddr - lib_base;

        if (sec.file_size > 0)
        {
            if (sec.file_offset + sec.file_size > read_bytes)
            {
                klog_error("axf_load: import: section data extends beyond file");
                return 0;
            }
            for (uint64_t i = 0; i < sec.file_size; i++)
                dst_page[page_offset + i] = filebuf[sec.file_offset + i];
        }
    }

    // Looks up the requested function in the export table.
    if (header.export_count == 0 || header.export_table_offset + header.export_count * sizeof(axf_export_t) > read_bytes)
    {
        klog_error("axf_load: import: library has no valid export table");
        return 0;
    }

    for (uint32_t e = 0; e < header.export_count; e++)
    {
        axf_export_t exp;
        uint64_t exp_offset = header.export_table_offset + e * sizeof(axf_export_t);
        uint8_t *eb = (uint8_t *)&exp;
        for (uint32_t i = 0; i < sizeof(exp); i++)
            eb[i] = filebuf[exp_offset + i];

        if (str_eq(exp.name, func_name))
        {
            // Absolute address = where the function actually lands in
            // memory (USER_LIB_ADDR), not the file's "logical" base
            // address (lib_base) -- exp.offset is relative to the
            // start of the file/section, so it's directly usable here.
            return USER_LIB_ADDR + exp.offset;
        }
    }

    klog_error_prefix();
    vga_puts("axf_load: import: function '");
    vga_puts(func_name);
    vga_puts("' not found in '");
    vga_puts(lib_name);
    vga_puts("'\n");
    return 0;
}

bool axf_load_and_run(uint64_t inode_num, uint64_t dir_inode)
{
    static uint8_t filebuf[AXF_MAX_FILE_SIZE];
    uint64_t read_bytes = axfs_read(inode_num, filebuf, sizeof(filebuf), 0);

    if (read_bytes < sizeof(axf_header_t))
    {
        klog_error("axf_load: file too small to contain a valid header");
        return false;
    }

    axf_header_t header;
    uint8_t *hb = (uint8_t *)&header;
    for (uint32_t i = 0; i < sizeof(header); i++)
        hb[i] = filebuf[i];

    if (header.magic != AXF_MAGIC)
    {
        klog_error("axf_load: bad magic (not a valid AXF file)");
        return false;
    }

    if (header.version != AXF_VERSION)
    {
        klog_error_n("axf_load: unsupported AXF version", header.version);
        return false;
    }

    if (header.type != AXF_TYPE_AXP)
    {
        // V1: only .axp files (standalone programs) are directly
        // executable. .axl/.axd need different handling (loaded as a
        // library / driver instead), see axd.c for .axd.
        klog_error("axf_load: only .axp files can be run directly (v1)");
        return false;
    }

    if (header.section_count == 0 || header.section_count > 8)
    {
        // Guard: no sections, or an unreasonable count (the file is
        // likely corrupted or malicious).
        klog_error_n("axf_load: invalid section_count", header.section_count);
        return false;
    }

    uint64_t base_addr = header.base_addr ? header.base_addr : AXF_DEFAULT_BASE_ADDR;

    // V1: only accepts the default base (USER_CODE_ADDR) -- an
    // arbitrary base address would require dynamically choosing where
    // to map pages, which the current ring3 return mechanism doesn't
    // support yet (USER_CODE_ADDR/USER_STACK_ADDR are fixed constants
    // shared with `ring3`).
    if (base_addr != USER_CODE_ADDR)
    {
        klog_error("axf_load: only the default base address is supported (v1)");
        return false;
    }

    // Resolves imports (needed .axl files) BEFORE loading/mapping the
    // .axp itself: if an import fails, this bails out cleanly without
    // having touched the .axp's memory yet (nothing to clean up).
    if (header.import_count > 0)
    {
        if (header.import_table_offset + header.import_count * sizeof(axf_import_t) > read_bytes)
        {
            klog_error("axf_load: import table extends beyond file");
            return false;
        }

        uint64_t resolved[64]; // matches AXF_IMPORT_TABLE_ADDR -- 64 imports max in V1 (plenty for now)
        if (header.import_count > 64)
        {
            klog_error("axf_load: too many imports (max 64 in v1)");
            return false;
        }

        for (uint32_t i = 0; i < header.import_count; i++)
        {
            axf_import_t imp;
            uint64_t imp_offset = header.import_table_offset + i * sizeof(axf_import_t);
            uint8_t *ib = (uint8_t *)&imp;
            for (uint32_t k = 0; k < sizeof(imp); k++)
                ib[k] = filebuf[imp_offset + k];

            uint64_t addr = load_axl_and_resolve(dir_inode, imp.lib_name, imp.func_name);
            if (addr == 0)
            {
                // load_axl_and_resolve() already printed a precise
                // error (library not found / function missing / etc.)
                // -- no need to add another one here.
                return false;
            }
            resolved[i] = addr;
        }

        // Writes the resolved import table into memory, at the fixed
        // address the .axp will read from at startup (see axf.h:
        // AXF_IMPORT_TABLE_ADDR). Needs its own page -- separate from
        // the .axp's code page, even though the chosen address
        // (USER_CODE_ADDR + 4096) makes it logically "adjacent".
        void *import_table_phys = pmm_alloc_page();
        if (!import_table_phys)
        {
            klog_error("axf_load: out of physical memory (import table)");
            return false;
        }
        vmm_map(AXF_IMPORT_TABLE_ADDR, (uint64_t)import_table_phys, VMM_USER_RW);

        uint64_t *import_table = (uint64_t *)AXF_IMPORT_TABLE_ADDR;
        for (uint32_t i = 0; i < header.import_count; i++)
            import_table[i] = resolved[i];
    }

    if (header.section_table_offset + header.section_count * sizeof(axf_section_t) > read_bytes)
    {
        klog_error("axf_load: section table extends beyond file");
        return false;
    }

    // Prepares the code page (same approach as `ring3`: a single
    // page, zero-filled, sections copied into it).
    void *code_phys = pmm_alloc_page();
    if (!code_phys)
    {
        klog_error("axf_load: out of physical memory");
        return false;
    }
    vmm_map(USER_CODE_ADDR, (uint64_t)code_phys, VMM_USER_RW);

    uint8_t *dst_page = (uint8_t *)USER_CODE_ADDR;
    for (uint32_t i = 0; i < 4096; i++)
        dst_page[i] = 0;

    uint64_t entry_point = header.entry_point;
    bool entry_in_range = false;

    for (uint32_t s = 0; s < header.section_count; s++)
    {
        axf_section_t sec;
        uint64_t sec_offset = header.section_table_offset + s * sizeof(axf_section_t);
        uint8_t *sb = (uint8_t *)&sec;
        for (uint32_t i = 0; i < sizeof(sec); i++)
            sb[i] = filebuf[sec_offset + i];

        // V1: every section must fit inside the single page allocated
        // above, within [USER_CODE_ADDR, USER_CODE_ADDR+4096). No
        // multi-page support yet.
        if (sec.vaddr < USER_CODE_ADDR || sec.vaddr + sec.mem_size > USER_CODE_ADDR + 4096)
        {
            klog_error("axf_load: section out of supported range (v1: single 4KB page)");
            return false;
        }

        uint64_t page_offset = sec.vaddr - USER_CODE_ADDR;

        if (sec.file_size > 0)
        {
            if (sec.file_offset + sec.file_size > read_bytes)
            {
                klog_error("axf_load: section data extends beyond file");
                return false;
            }
            for (uint64_t i = 0; i < sec.file_size; i++)
                dst_page[page_offset + i] = filebuf[sec.file_offset + i];
        }
        // The rest of mem_size beyond file_size (typically .bss, or
        // padding) stays zero -- already handled by zero-filling the
        // whole page above.

        if (entry_point >= sec.vaddr && entry_point < sec.vaddr + sec.mem_size)
            entry_in_range = true;
    }

    if (!entry_in_range)
    {
        klog_error("axf_load: entry_point is not inside any loaded section");
        return false;
    }

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

    uint64_t stack_top = USER_STACK_ADDR + USER_STACK_SIZE; // already 16-byte aligned (see shell.c/ring3)

    // Jumps to Ring 3, same return mechanism as `ring3`.
    syscall_set_shell_return_point();
    syscall_mark_shell_return_point_ready();

    if (shell_return_bypass == 0)
    {
        // First pass: actually jump.
        userspace_jump(entry_point, stack_top);
    }

    // Reached only after the loaded program's SYS_EXIT
    // (shell_return_bypass == 1 at this point).
    klog_ok("axf_load: program exited, back in kernel");

    return true;
}