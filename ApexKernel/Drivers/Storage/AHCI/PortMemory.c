// ApexKernel/Drivers/Storage/AHCI/PortMemory.c
// ApexOS -- AHCI per-port DMA memory setup (command list, FIS, PRDT)

#include "Drivers/Storage/AHCI/PortMemory.h"
#include "Drivers/Storage/AHCI/AHCIInternal.h"
#include "Drivers/Storage/AHCI/PortControl.h"
#include "Core/Memory/Physical/PageBitmap.h"
#include "Core/Memory/Virtual/PageTables.h"

void *ahci_alloc_zeroed_page(void)
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

bool ahci_port_init_memory(hba_port_t *port)
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
