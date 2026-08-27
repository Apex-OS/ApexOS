// ApexKernel/Drivers/Storage/AHCI/CommandExecution.c
// ApexOS -- AHCI command building and issuing (IDENTIFY/READ/WRITE DMA EXT)
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

#include "Drivers/Storage/AHCI/CommandExecution.h"
#include "Drivers/Storage/AHCI/AHCIInternal.h"
#include "Drivers/Storage/AHCI/PortControl.h"
#include "Core/Memory/Physical/PageBitmap.h"
#include "Core/Memory/Virtual/PageTables.h"

int ahci_find_free_slot(hba_port_t *port)
{
    uint32_t busy = port->sact | port->ci;
    for (int slot = 0; slot < AHCI_CMD_SLOTS; slot++)
    {
        if (!(busy & (1u << slot)))
            return slot;
    }
    return -1;
}

bool ahci_do_command(hba_port_t *port, ahci_cmd_kind_t kind, uint64_t lba, uint32_t count, void *buf)
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
    // set once in PortMemory.c's ahci_port_init_memory(), never
    // per-command.
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
    // matches what AHCIBackend.c's block-sized reads/writes need (4KB
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
    // (ahci_alloc_zeroed_page() in PortMemory.c) are identity-mapped
    // on purpose, but a caller's stack buffer generally is not:
    // HeapSetup.c's heap_init() maps the heap's virtual range onto
    // whatever physical pages pmm_alloc_page() happened to hand out,
    // and every process's stack -- including the shell's, which is
    // what a plain local buffer in a shell command function like
    // cmd_disktest() ends up on -- is a kmalloc() allocation out of
    // that same heap. Confirmed via disktest: ahci_read_sectors()
    // reported success (no task-file error, PxCI cleared in time) but
    // the buffer stayed all zero, because the DMA was correctly
    // writing to the buffer's physical page while every other read of
    // it went through its virtual address instead, landing on a
    // different physical page entirely.
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
