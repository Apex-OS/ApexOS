// apex-kernel/drivers/ahci.c
// ApexOS -- AHCI SATA driver (see ahci.h for the V1 scope)
//
// Polling, not interrupts: V1 issues a command (sets PxCI's bit for
// the slot used), then spins reading PxCI/PxTFD until the bit clears
// or a timeout (via pit_get_ticks()) trips. AHCI fully supports
// interrupt-driven completion (PxIE + the HBA's global interrupt),
// which is the eventual real answer for not blocking the CPU during
// disk I/O the same way ATA PIO would -- but that needs the
// interrupt/IRQ plumbing wired up to this driver specifically, which
// is its own chunk of work. Polling gets a correct, testable driver
// first; switching the wait loop to "sleep on an interrupt" later
// doesn't change anything about the command-building code below.

#include "ahci.h"
#include "pci.h"
#include "pit.h"
#include "../core/klog.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"

// PCI config space offset of BAR5 -- where the AHCI spec requires the
// ABAR (AHCI Base Memory Register) to live for any AHCI controller.
#define PCI_BAR5_OFFSET 0x24

// How many command slots this driver uses per port. The HBA may
// support up to 32 (see hba_cmd_header_t comments) -- 8 is plenty for
// V1's one-command-at-a-time polling design and keeps the command
// list/table memory small.
#define AHCI_CMD_SLOTS 8

static hba_mem_t *g_hba = NULL;
static ahci_drive_info_t g_drive = {0};

// Per-port memory the driver owns: one page for this port's command
// list + received-FIS area (1024 + 256 = 1280 bytes, comfortably
// under 4096), and one page per command slot for that slot's command
// table (a hba_cmd_tbl_t is 128 + AHCI_PRDT_COUNT*16 = 256 bytes,
// comfortably under 4096 too). Using a full page per structure wastes
// space (a few KB total) but keeps every allocation's alignment
// trivially correct -- AHCI requires clb 1K-aligned, fb 256-aligned,
// ctba 128-aligned, and any page (4096-aligned) satisfies all three at
// once, no sub-page allocator needed for a driver this size.
static void *g_cmd_list_fis_page = NULL;
static void *g_cmd_table_pages[AHCI_CMD_SLOTS] = {0};

// Maps one physical page at the same virtual address (identity map,
// same convention as fb_init() for the VBE framebuffer -- see
// drivers/framebuffer.c) and returns it zeroed. Used for every AHCI
// structure below; NULL on allocation failure.
static void *ahci_alloc_zeroed_page(void)
{
    void *phys = pmm_alloc_page();
    if (!phys)
        return NULL;

    vmm_map((uint64_t)phys, (uint64_t)phys, VMM_KERNEL_RW | VMM_NO_CACHE);

    uint8_t *bytes = (uint8_t *)phys;
    for (int i = 0; i < PAGE_SIZE; i++)
        bytes[i] = 0;

    return phys;
}

// Takes the address of a field inside a packed struct (hba_port_t
// below is __attribute__((packed))) without -Waddress-of-packed-member.
// GCC can't prove alignment through `packed` even when every field
// actually IS naturally aligned -- true here: hba_port_t is uint32_t
// fields back to back, hba_mem_t places ports[] at offset 0x100 (a
// multiple of 4), and hba_mem_t itself sits at a page-aligned MMIO
// address (see ahci_init()'s vmm_map() call).
//
// A plain cast on `&s->field` doesn't silence this -- GCC warns on the
// address-of expression itself before any cast gets a chance to apply.
// Computing the field's address as (struct base) + offsetof(field)
// instead never writes `&s->field` at all, so there's nothing for the
// warning to attach to. This trusts the same alignment fact a cast
// would have asserted; it isn't working around a real bug.
#define PACKED_FIELD_ADDR(struct_ptr, field_name) \
    ((volatile uint32_t *)((uint8_t *)(struct_ptr) + __builtin_offsetof(__typeof__(*(struct_ptr)), field_name)))

// Spins until `cond` becomes false or `timeout_ms` elapses. Returns
// true if `cond` cleared in time. pit_get_ticks() increments every
// 10ms (see drivers/pit.h: PIT_TARGET_FREQ), so this rounds the
// timeout up to the nearest tick -- fine for the multi-hundred-ms
// timeouts AHCI command completion needs.
static bool ahci_wait_clear(volatile uint32_t *reg, uint32_t mask, uint32_t timeout_ms)
{
    uint64_t start = pit_get_ticks();
    uint64_t timeout_ticks = (timeout_ms / 10) + 1;

    while (*reg & mask)
    {
        if (pit_get_ticks() - start > timeout_ticks)
            return false;
    }
    return true;
}

// Stops the port's command engine (clears PxCMD.ST and waits for
// PxCMD.CR to clear) -- required before touching clb/fb, per the AHCI
// spec (section 10.3.1: "software shall not write... while ST is set").
static bool ahci_port_stop_cmd(hba_port_t *port)
{
    port->cmd &= ~HBA_PxCMD_ST;
    port->cmd &= ~HBA_PxCMD_FRE;

    if (!ahci_wait_clear(PACKED_FIELD_ADDR(port, cmd), HBA_PxCMD_FR, 500))
        return false;
    if (!ahci_wait_clear(PACKED_FIELD_ADDR(port, cmd), HBA_PxCMD_CR, 500))
        return false;

    return true;
}

// Starts the port's command engine (FIS receive, then command list
// processing) -- the reverse of ahci_port_stop_cmd(), done once clb/fb
// are set up and it's safe to let the HBA start issuing commands.
static void ahci_port_start_cmd(hba_port_t *port)
{
    while (port->cmd & HBA_PxCMD_CR)
        ; // brief spin -- CR clearing after a stop is near-instant in practice

    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST;
}

// Allocates and wires up the command list, received-FIS area, and per-
// slot command tables for one port, then starts its command engine.
// Returns false if any allocation fails (out of physical memory) --
// callers treat that the same as "no usable drive found".
static bool ahci_port_init_memory(hba_port_t *port)
{
    if (!ahci_port_stop_cmd(port))
        return false;

    g_cmd_list_fis_page = ahci_alloc_zeroed_page();
    if (!g_cmd_list_fis_page)
        return false;

    // Command list: AHCI_CMD_SLOTS headers, 32 bytes each, starting at
    // the page's base -- see hba_cmd_header_t.
    uint64_t cmd_list_phys = (uint64_t)g_cmd_list_fis_page;
    port->clb = (uint32_t)(cmd_list_phys & 0xFFFFFFFF);
    port->clbu = (uint32_t)(cmd_list_phys >> 32);

    // Received FIS area: fixed 256-byte structure, placed right after
    // the command list (32 slots * 32 bytes = 1024 bytes max, so byte
    // offset 1024 is always past the command list regardless of
    // AHCI_CMD_SLOTS) -- both comfortably inside the same page.
    uint64_t fis_phys = cmd_list_phys + 1024;
    port->fb = (uint32_t)(fis_phys & 0xFFFFFFFF);
    port->fbu = (uint32_t)(fis_phys >> 32);

    hba_cmd_header_t *cmd_headers = (hba_cmd_header_t *)cmd_list_phys;
    for (int slot = 0; slot < AHCI_CMD_SLOTS; slot++)
    {
        g_cmd_table_pages[slot] = ahci_alloc_zeroed_page();
        if (!g_cmd_table_pages[slot])
            return false;

        uint64_t ctba_phys = (uint64_t)g_cmd_table_pages[slot];
        cmd_headers[slot].ctba = (uint32_t)(ctba_phys & 0xFFFFFFFF);
        cmd_headers[slot].ctbau = (uint32_t)(ctba_phys >> 32);
        cmd_headers[slot].prdtl = AHCI_PRDT_COUNT;
    }

    // Clears any stale error/interrupt state left over from whatever
    // the BIOS/firmware did with this port before the kernel took
    // over.
    port->serr = 0xFFFFFFFF;
    port->is = 0xFFFFFFFF;

    ahci_port_start_cmd(port);
    return true;
}

// Finds a free command slot (one whose bit is clear in both PxCI and
// PxSACT -- not currently issued, not marked active) -- returns -1 if
// every slot this driver uses is busy. With V1 issuing one command at
// a time and waiting for it to finish before returning, this should
// always find slot 0 free in practice; the search is here so the
// driver isn't silently wrong if that assumption ever stops holding.
static int ahci_find_free_slot(hba_port_t *port)
{
    uint32_t busy = port->sact | port->ci;
    for (int slot = 0; slot < AHCI_CMD_SLOTS; slot++)
    {
        if (!(busy & (1u << slot)))
            return slot;
    }
    return -1;
}

// Which of the three commands ahci_do_command() builds. Kept as an
// enum rather than stacking more bool parameters (is_identify,
// is_write, ...) -- adding a fourth command kind later (e.g. FLUSH
// CACHE) means one more enum value, not another parameter threaded
// through every call site.
typedef enum
{
    AHCI_CMD_IDENTIFY,
    AHCI_CMD_READ,
    AHCI_CMD_WRITE,
} ahci_cmd_kind_t;

// Builds and issues one command (IDENTIFY, or a READ/WRITE DMA EXT
// covering `count` sectors starting at `lba`), waits for it to
// complete, and transfers data to/from `buf` accordingly. IDENTIFY
// always transfers exactly one 512-byte sector and ignores
// `lba`/`count`; for READ, `buf` receives the drive's data; for
// WRITE, `buf` is the source data sent to the drive.
static bool ahci_do_command(hba_port_t *port, ahci_cmd_kind_t kind, uint64_t lba, uint32_t count, void *buf)
{
    int slot = ahci_find_free_slot(port);
    if (slot < 0)
        return false;

    hba_cmd_header_t *cmd_headers = (hba_cmd_header_t *)(uint64_t)g_cmd_list_fis_page;
    hba_cmd_header_t *hdr = &cmd_headers[slot];
    hba_cmd_tbl_t *tbl = (hba_cmd_tbl_t *)(uint64_t)g_cmd_table_pages[slot];

    // Zeroes the whole command header, not just the fields this
    // function sets below -- confirmed via disktest: a write followed
    // by a read on the same slot failed, traced to stray bits left in
    // hdr (a/p/r/b/c, the ones this function never explicitly
    // assigns) from the previous command. ctba/ctbau are the one
    // exception -- those point at this slot's command table and are
    // set once in ahci_port_init_memory(), never per-command.
    uint32_t ctba_save = hdr->ctba;
    uint32_t ctbau_save = hdr->ctbau;
    for (uint32_t i = 0; i < sizeof(hba_cmd_header_t); i++)
        ((uint8_t *)hdr)[i] = 0;
    hdr->ctba = ctba_save;
    hdr->ctbau = ctbau_save;

    for (uint32_t i = 0; i < sizeof(hba_cmd_tbl_t); i++)
        ((uint8_t *)tbl)[i] = 0;

    uint32_t bytes = (kind == AHCI_CMD_IDENTIFY) ? 512 : count * 512;
    // Upper bound on any single transfer this driver builds, regardless
    // of how many PRDT entries it ends up split across (see the loop
    // below) -- keeps prdt.dbc comfortably away from its 4MB limit and
    // matches what axfs_disk.c's block-sized reads/writes need (4KB
    // blocks, far under this).
    if (bytes > AHCI_PRDT_MAX_BYTES)
        return false;

    // buf is a VIRTUAL address -- the caller's sector[512] (or
    // whatever else it points to) is addressed the same way any other
    // pointer in this kernel is, via the page tables. The HBA has no
    // idea those exist: it DMAs straight to/from physical memory over
    // the PCI bus, so the PRDT needs buf's PHYSICAL address, not its
    // virtual one. Those are only ever the same by coincidence (an
    // identity-mapped page) -- e.g. AHCI's own driver-owned pages
    // (ahci_alloc_zeroed_page() above) are identity-mapped on purpose,
    // but a caller's stack buffer generally is not: heap_init() (see
    // mm/heap.c) maps the heap's virtual range onto whatever physical
    // pages pmm_alloc_page() happened to hand out, and every process's
    // stack -- including the shell's, which is what a plain local
    // buffer in a shell command function like cmd_disktest() ends up
    // on -- is a kmalloc() allocation out of that same heap. Confirmed
    // via disktest: ahci_read_sectors() reported success (no task-file
    // error, PxCI cleared in time) but the buffer stayed all zero,
    // because the DMA was correctly writing to the buffer's physical
    // page while every other read of it went through its virtual
    // address instead, landing on a different physical page entirely.
    //
    // buf also has no guaranteed alignment to a page boundary -- a
    // stack buffer can start anywhere within its page, so a transfer
    // that's well under one page in SIZE can still SPAN two physical
    // pages if it starts close enough to the end of one. Those two
    // pages have no guarantee of being physically contiguous (heap
    // pages come from pmm_alloc_page() in whatever order was free at
    // the time), so this can't be handled as a single contiguous DMA
    // region -- it needs one PRDT entry per physical page touched.
    // Confirmed via GDB (single-stepping this function under
    // disktest's write/read-back test): readback_buf[512], a stack
    // buffer in the shell process, landed at offset 3760 within its
    // page, so its 512-byte transfer ran 176 bytes past the page
    // boundary. An earlier version of this function just refused any
    // transfer that crossed a page boundary rather than handling it --
    // fine for IDENTIFY (a fixed 512-byte, driver-controlled buffer)
    // but not for caller-supplied buffers with arbitrary alignment,
    // which is the normal case for anything calling ahci_read_sectors()
    // /ahci_write_sectors() (e.g. a stack-allocated sector buffer, as
    // above).
    uint64_t buf_virt = (uint64_t)buf;
    uint32_t prdt_count = 0;
    uint32_t bytes_remaining = bytes;
    uint64_t cursor = buf_virt;

    while (bytes_remaining > 0)
    {
        if (prdt_count >= AHCI_PRDT_COUNT)
            return false; // transfer needs more PRDT entries than this driver builds

        uint64_t page_phys = vmm_get_phys(cursor & ~(PAGE_SIZE - 1));
        if (page_phys == 0)
            return false; // this part of buf isn't mapped -- nothing safe to DMA into/from

        uint64_t page_offset = cursor & (PAGE_SIZE - 1);
        uint64_t chunk_phys = page_phys + page_offset;
        uint32_t chunk_bytes = PAGE_SIZE - (uint32_t)page_offset; // bytes left in this page
        if (chunk_bytes > bytes_remaining)
            chunk_bytes = bytes_remaining; // last chunk: don't overshoot the transfer

        tbl->prdt_entry[prdt_count].dba = (uint32_t)(chunk_phys & 0xFFFFFFFF);
        tbl->prdt_entry[prdt_count].dbau = (uint32_t)(chunk_phys >> 32);
        tbl->prdt_entry[prdt_count].dbc = chunk_bytes - 1; // AHCI encodes byte count minus 1
        tbl->prdt_entry[prdt_count].i = 0;

        prdt_count++;
        cursor += chunk_bytes;
        bytes_remaining -= chunk_bytes;
    }

    // Only the last PRDT entry needs its interrupt-on-completion bit
    // set -- that's the one whose completion means the whole transfer
    // is done.
    tbl->prdt_entry[prdt_count - 1].i = 1;

    fis_reg_h2d_t *fis = (fis_reg_h2d_t *)tbl->cfis;
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1; // this FIS carries a command, not a control update
    switch (kind)
    {
    case AHCI_CMD_IDENTIFY:
        fis->command = ATA_CMD_IDENTIFY;
        break;
    case AHCI_CMD_READ:
        fis->command = ATA_CMD_READ_DMA_EX;
        break;
    case AHCI_CMD_WRITE:
        fis->command = ATA_CMD_WRITE_DMA_EX;
        break;
    }
    fis->device = 1 << 6; // LBA mode

    if (kind != AHCI_CMD_IDENTIFY)
    {
        fis->lba0 = (uint8_t)(lba & 0xFF);
        fis->lba1 = (uint8_t)((lba >> 8) & 0xFF);
        fis->lba2 = (uint8_t)((lba >> 16) & 0xFF);
        fis->lba3 = (uint8_t)((lba >> 24) & 0xFF);
        fis->lba4 = (uint8_t)((lba >> 32) & 0xFF);
        fis->lba5 = (uint8_t)((lba >> 40) & 0xFF);
        fis->countl = (uint8_t)(count & 0xFF);
        fis->counth = (uint8_t)((count >> 8) & 0xFF);
    }

    hdr->cfl = sizeof(fis_reg_h2d_t) / sizeof(uint32_t);
    hdr->w = (kind == AHCI_CMD_WRITE) ? 1 : 0; // transfer direction: 1 = host to device
    hdr->prdtl = prdt_count;
    hdr->prdbc = 0;

    // Waits for the drive to stop reporting busy/data-request before
    // issuing -- issuing into an already-busy drive is a spec
    // violation (AHCI 1.3, section 5.3.2.2) some controllers enforce
    // strictly enough to misbehave on.
    if (!ahci_wait_clear(PACKED_FIELD_ADDR(port, tfd), ATA_DEV_BUSY | ATA_DEV_DRQ, 1000))
        return false;

    port->ci = 1u << slot;

    if (!ahci_wait_clear(PACKED_FIELD_ADDR(port, ci), 1u << slot, 5000))
        return false;

    // A set PxIS/PxTFD error bit after completion means the drive
    // rejected the command -- report failure rather than handing back
    // whatever ended up in `buf`.
    uint32_t is_snapshot = port->is;
    bool task_file_error = (is_snapshot & (1u << 30)) != 0; // TFES -- Task File Error Status

    // PxIS bits are write-1-to-clear (AHCI 1.3 spec, section 3.3.6) --
    // reading TFES above does NOT clear it. Left alone, a bit set by
    // one command (even one this function still reports success for,
    // e.g. a transient condition the drive recovered from) stays set
    // forever, making every later command's error check here see a
    // stale error from the past instead of its own actual result.
    // Confirmed via disktest: a write immediately followed by a read
    // failed the read's TFES check even though the write itself
    // reported success. Writing back exactly the bits that were set
    // clears those and leaves everything else untouched.
    port->is = is_snapshot;

    if (task_file_error)
        return false;

    return true;
}

bool ahci_init(void)
{
    pci_device_t dev;
    if (!pci_find_device(0x01, 0x06, 0x01, &dev))
    {
        klog_warn("ahci_init: no AHCI controller found");
        return false;
    }

    uint32_t bar5 = pci_config_read32(dev.bus, dev.slot, dev.function, PCI_BAR5_OFFSET);
    // BAR5 is always a 32-bit memory BAR for AHCI (bit 0 = 0 for
    // memory space, bits 2:1 = address type -- the spec requires this
    // exact layout), so no 64-bit BAR pair-reading logic is needed
    // here the way a general-purpose PCI BAR reader would need.
    uint64_t abar_phys = bar5 & 0xFFFFFFF0u;

    // The ABAR region is small (up to 0x1100 bytes for 32 ports, see
    // hba_mem_t) but PCI BARs are page-aligned by construction, so one
    // page covers it -- maps it uncacheable (VMM_NO_CACHE), same
    // reasoning as any other MMIO region (see the framebuffer's
    // fb_init(), which -- confirmed the hard way in an earlier
    // session -- didn't actually need this flag to function, but it's
    // still the textbook-correct thing to do for device registers).
    vmm_map(abar_phys, abar_phys, VMM_KERNEL_RW | VMM_NO_CACHE);
    g_hba = (hba_mem_t *)abar_phys;

    // AHCI Base bit (GHC bit 31) must be set before the rest of the
    // registers behave per-spec -- some firmware leaves the controller
    // in legacy/compatibility mode otherwise.
    g_hba->ghc |= (1u << 31);

    for (uint32_t i = 0; i < 32; i++)
    {
        if (!(g_hba->pi & (1u << i)))
            continue; // port not implemented on this controller

        hba_port_t *port = &g_hba->ports[i];

        uint8_t ipm = (uint8_t)((port->ssts >> 8) & 0x0F);
        uint8_t det = (uint8_t)(port->ssts & 0x0F);
        if (det != HBA_PORT_DET_PRESENT || ipm != HBA_PORT_IPM_ACTIVE)
            continue; // no drive physically present/linked on this port

        if (port->sig != SATA_SIG_ATA)
            continue; // ATAPI/port multiplier/etc -- not supported in V1

        if (!ahci_port_init_memory(port))
        {
            klog_error("ahci_init: failed to set up port memory");
            continue;
        }

        uint8_t identify_buf[512];
        if (!ahci_do_command(port, AHCI_CMD_IDENTIFY, 0, 0, identify_buf))
        {
            klog_error("ahci_init: IDENTIFY command failed");
            continue;
        }

        // IDENTIFY DEVICE data, ATA-8/ACS spec: words 100-103 hold the
        // 48-bit total LBA count for drives supporting LBA48 (word 83
        // bit 10 signals that support -- V1 assumes it's set, true for
        // essentially every drive QEMU or real hardware presents
        // today).
        uint16_t *identify_words = (uint16_t *)identify_buf;
        uint64_t sectors = (uint64_t)identify_words[100] |
                           ((uint64_t)identify_words[101] << 16) |
                           ((uint64_t)identify_words[102] << 32) |
                           ((uint64_t)identify_words[103] << 48);

        g_drive.ready = true;
        g_drive.port_index = i;
        g_drive.sector_count = sectors;

        klog_ok_n("ahci_init: drive ready, port", i);
        klog_ok_n("ahci_init: drive ready, sectors", sectors);
        return true;
    }

    klog_warn("ahci_init: AHCI controller found, but no usable SATA drive on any port");
    return false;
}

ahci_drive_info_t ahci_drive_info(void)
{
    return g_drive;
}

bool ahci_read_sectors(uint64_t lba, uint32_t count, void *buf)
{
    if (!g_drive.ready)
        return false;
    if (count == 0)
        return true;

    hba_port_t *port = &g_hba->ports[g_drive.port_index];
    return ahci_do_command(port, AHCI_CMD_READ, lba, count, buf);
}

bool ahci_write_sectors(uint64_t lba, uint32_t count, const void *buf)
{
    if (!g_drive.ready)
        return false;
    if (count == 0)
        return true;

    hba_port_t *port = &g_hba->ports[g_drive.port_index];
    // ahci_do_command()'s buf parameter is non-const (it's shared with
    // the read path, which writes into it) -- safe here since WRITE
    // never modifies buf, only reads from it to fill the PRDT-described
    // memory the HBA then DMAs out of.
    return ahci_do_command(port, AHCI_CMD_WRITE, lba, count, (void *)buf);
}

ahci_port_debug_t ahci_port_debug_info(void)
{
    ahci_port_debug_t out = {0};
    if (!g_drive.ready)
        return out;

    hba_port_t *port = &g_hba->ports[g_drive.port_index];
    out.tfd = port->tfd;
    out.is = port->is;
    out.serr = port->serr;
    out.ci = port->ci;
    out.sact = port->sact;
    out.cmd = port->cmd;
    return out;
}