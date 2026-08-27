// ApexKernel/Drivers/Storage/AHCI/AHCI.c
// ApexOS -- AHCI SATA driver public API (drive info, read/write sectors, debug)

#include "Drivers/Storage/AHCI/AHCI.h"
#include "Drivers/Storage/AHCI/AHCIInternal.h"
#include "Drivers/Storage/AHCI/CommandExecution.h"

hba_mem_t *g_hba = NULL;
ahci_drive_info_t g_drive = {0};
void *g_cmd_list_fis_page = NULL;
void *g_cmd_table_pages[AHCI_CMD_SLOTS] = {0};

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
