// apex-kernel/core/axf/axd.c
// ApexOS -- AXD loader (.axd drivers, Ring 0, kernel-only)
//
// Same format parsing as load_axl_and_resolve() in axf_loader.c, but:
//   - mapped with VMM_KERNEL_RW (no VMM_USER -- a Ring 3 process
//     cannot read/execute a loaded driver's memory)
//   - the entire export table is resolved and kept (not just one
//     function like .axl), since a driver exposes several functions
//     called later by func_id, not by name
//   - "driver_init" is looked up and called immediately (direct Ring
//     0 call, no syscall/Ring 3 round trip -- already in Ring 0 here)

#include "axd.h"
#include "../klog.h"
#include "../../fs/axfs.h"
#include "../../mm/pmm.h"
#include "../../mm/vmm.h"
#include "../../drivers/vga.h"
#include "../../../apex-os/formats/axf/axf.h"

axd_driver_t loaded_drivers[AXD_MAX_DRIVERS];
int axd_driver_count = 0;

// Base address where each driver is mapped. V1: a single driver
// "active in memory" at a time, at a fixed address, like
// USER_LIB_ADDR for .axl -- every new axd_load() remaps the same
// physical/virtual page. Enough to validate the mechanism; real
// multi-driver support (distinct addresses per driver) will come if
// the need arises (e.g. real PS/2 + VGA drivers loaded together).
//
// IMPORTANT -- 0x800000 was used initially, but that's EXACTLY
// HEAP_START (see mm/heap.h): the mapping overwrote the first page of
// the kernel heap (including heap_head itself), silently corrupting
// the structure and causing a GPF later on the first allocation/free
// touching that zone. The heap occupies [0x800000, 0xC00000)
// (HEAP_START + HEAP_MAX_SIZE = 4MB) -- 0xD00000 was chosen with
// headroom beyond that to avoid any future overlap if HEAP_MAX_SIZE
// grows.
#define AXD_BASE_ADDR 0xD00000ull

#define AXD_MAX_FILE_SIZE 4096u

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

int axd_load(uint64_t inode_num)
{
    if (axd_driver_count >= AXD_MAX_DRIVERS)
    {
        klog_error("axd_load: no more driver slots available");
        return -1;
    }

    static uint8_t filebuf[AXD_MAX_FILE_SIZE];
    uint64_t read_bytes = axfs_read(inode_num, filebuf, sizeof(filebuf), 0);

    if (read_bytes < sizeof(axf_header_t))
    {
        klog_error("axd_load: file too small to contain a valid header");
        return -1;
    }

    axf_header_t header;
    uint8_t *hb = (uint8_t *)&header;
    for (uint32_t i = 0; i < sizeof(header); i++)
        hb[i] = filebuf[i];

    if (header.magic != AXF_MAGIC || header.version != AXF_VERSION)
    {
        klog_error("axd_load: invalid AXF header");
        return -1;
    }

    if (header.type != AXF_TYPE_AXD)
    {
        klog_error("axd_load: file is not a .axd (wrong type field)");
        return -1;
    }

    if (header.section_count == 0 || header.section_count > 8)
    {
        klog_error_n("axd_load: invalid section_count", header.section_count);
        return -1;
    }

    if (header.export_count == 0 || header.export_count > 16)
    {
        klog_error("axd_load: .axd must export at least driver_init (max 16 exports in v1)");
        return -1;
    }

    // Maps the driver in KERNEL-ONLY pages (no VMM_USER -- this is the
    // whole difference from load_axl_and_resolve).
    void *drv_phys = pmm_alloc_page();
    if (!drv_phys)
    {
        klog_error("axd_load: out of physical memory");
        return -1;
    }
    vmm_map(AXD_BASE_ADDR, (uint64_t)drv_phys, VMM_KERNEL_RW);

    uint8_t *dst_page = (uint8_t *)AXD_BASE_ADDR;
    for (uint32_t i = 0; i < 4096; i++)
        dst_page[i] = 0;

    if (header.section_table_offset + header.section_count * sizeof(axf_section_t) > read_bytes)
    {
        klog_error("axd_load: section table extends beyond file");
        return -1;
    }

    // Same convention as .axl: compiled for AXF_DEFAULT_BASE_ADDR (or
    // header.base_addr), but physically mapped at AXD_BASE_ADDR --
    // each section's offset is computed relative to the file's
    // "logical" base address, not AXD_BASE_ADDR directly.
    uint64_t file_base = header.base_addr ? header.base_addr : AXF_DEFAULT_BASE_ADDR;

    for (uint32_t s = 0; s < header.section_count; s++)
    {
        axf_section_t sec;
        uint64_t sec_offset = header.section_table_offset + s * sizeof(axf_section_t);
        uint8_t *sb = (uint8_t *)&sec;
        for (uint32_t i = 0; i < sizeof(sec); i++)
            sb[i] = filebuf[sec_offset + i];

        if (sec.vaddr < file_base || sec.vaddr + sec.mem_size > file_base + 4096)
        {
            klog_error("axd_load: section out of supported range (v1: single 4KB page)");
            return -1;
        }

        uint64_t page_offset = sec.vaddr - file_base;

        if (sec.file_size > 0)
        {
            if (sec.file_offset + sec.file_size > read_bytes)
            {
                klog_error("axd_load: section data extends beyond file");
                return -1;
            }
            for (uint64_t i = 0; i < sec.file_size; i++)
                dst_page[page_offset + i] = filebuf[sec.file_offset + i];
        }
    }

    // Resolves the ENTIRE export table (not just one function -- a
    // driver exposes several functions called later by index).
    if (header.export_table_offset + header.export_count * sizeof(axf_export_t) > read_bytes)
    {
        klog_error("axd_load: export table extends beyond file");
        return -1;
    }

    axd_driver_t *drv = &loaded_drivers[axd_driver_count];
    drv->base_addr = AXD_BASE_ADDR;
    drv->export_count = header.export_count;
    drv->active = false;

    uint64_t init_addr = 0;

    for (uint32_t e = 0; e < header.export_count; e++)
    {
        axf_export_t exp;
        uint64_t exp_offset = header.export_table_offset + e * sizeof(axf_export_t);
        uint8_t *eb = (uint8_t *)&exp;
        for (uint32_t i = 0; i < sizeof(exp); i++)
            eb[i] = filebuf[exp_offset + i];

        uint64_t abs_addr = AXD_BASE_ADDR + exp.offset;
        drv->export_offsets[e] = abs_addr;

        if (str_eq(exp.name, AXD_INIT_EXPORT_NAME))
            init_addr = abs_addr;
    }

    if (init_addr == 0)
    {
        klog_error("axd_load: .axd has no 'driver_init' export");
        return -1;
    }

    // Generic name in V1 -- no name field in the AXF header yet (see
    // axf.h: reserved for future extension). "unnamed driver" if
    // nothing better is available.
    for (int i = 0; i < AXD_MAX_NAME; i++)
        drv->name[i] = 0;
    const char *generic_name = "axd_driver";
    for (int i = 0; generic_name[i] && i < AXD_MAX_NAME - 1; i++)
        drv->name[i] = generic_name[i];

    // Direct call to driver_init(), Ring 0, already on the current
    // kernel stack -- no syscall, no Ring 3 jump (unlike .axp/.axl
    // which run in Ring 3).
    void (*driver_init)(void) = (void (*)(void))init_addr;
    driver_init();

    drv->active = true;
    int driver_id = axd_driver_count;
    axd_driver_count++;

    klog_ok_n("axd_load: driver loaded (id)", (uint64_t)driver_id);

    return driver_id;
}

uint64_t axd_call(int driver_id, int func_id, uint64_t arg)
{
    if (driver_id < 0 || driver_id >= axd_driver_count)
        return (uint64_t)-1;

    axd_driver_t *drv = &loaded_drivers[driver_id];
    if (!drv->active)
        return (uint64_t)-1;

    if (func_id < 0 || (uint32_t)func_id >= drv->export_count)
        return (uint64_t)-1;

    uint64_t (*fn)(uint64_t) = (uint64_t (*)(uint64_t))drv->export_offsets[func_id];
    return fn(arg);
}