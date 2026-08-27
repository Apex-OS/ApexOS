// ApexKernel/Includes/Drivers/Storage/AHCI/AHCIInternal.h
// ApexOS -- Shared internal state for the AHCI module
//
// g_hba/g_drive/g_cmd_list_fis_page/g_cmd_table_pages are defined
// once in AHCI.c and shared via extern with Controller.c,
// PortMemory.c, and CommandExecution.c. Not part of the public AHCI.h
// API: nothing outside Drivers/Storage/AHCI/ should touch these
// directly.

#ifndef APEX_DRIVERS_STORAGE_AHCI_INTERNAL_H
#define APEX_DRIVERS_STORAGE_AHCI_INTERNAL_H

#include "Drivers/Storage/AHCI/AHCI.h"

extern hba_mem_t *g_hba;
extern ahci_drive_info_t g_drive;

// Per-port memory the driver owns: one page for this port's command
// list + received-FIS area (1024 + 256 = 1280 bytes, comfortably
// under 4096), and one page per command slot for that slot's command
// table (a hba_cmd_tbl_t is 128 + AHCI_PRDT_COUNT*16 = 256 bytes,
// comfortably under 4096 too). Using a full page per structure wastes
// space (a few KB total) but keeps every allocation's alignment
// trivially correct -- AHCI requires clb 1K-aligned, fb 256-aligned,
// ctba 128-aligned, and any page (4096-aligned) satisfies all three at
// once, no sub-page allocator needed for a driver this size.
extern void *g_cmd_list_fis_page;
extern void *g_cmd_table_pages[AHCI_CMD_SLOTS];

// Maps one physical page at the same virtual address (identity map,
// same convention as fb_init() for the VBE framebuffer) and returns
// it zeroed. Used for every AHCI structure; NULL on allocation
// failure.
void *ahci_alloc_zeroed_page(void);

// Takes the address of a field inside a packed struct (hba_port_t is
// __attribute__((packed))) without -Waddress-of-packed-member. GCC
// can't prove alignment through `packed` even when every field
// actually IS naturally aligned -- true here: hba_port_t is uint32_t
// fields back to back, hba_mem_t places ports[] at offset 0x100 (a
// multiple of 4), and hba_mem_t itself sits at a page-aligned MMIO
// address (see Controller.c's vmm_map() call).
//
// A plain cast on `&s->field` doesn't silence this -- GCC warns on the
// address-of expression itself before any cast gets a chance to apply.
// Computing the field's address as (struct base) + offsetof(field)
// instead never writes `&s->field` at all, so there's nothing for the
// warning to attach to. This trusts the same alignment fact a cast
// would have asserted; it isn't working around a real bug.
#define PACKED_FIELD_ADDR(struct_ptr, field_name) \
    ((volatile uint32_t *)((uint8_t *)(struct_ptr) + __builtin_offsetof(__typeof__(*(struct_ptr)), field_name)))

#endif // APEX_DRIVERS_STORAGE_AHCI_INTERNAL_H
