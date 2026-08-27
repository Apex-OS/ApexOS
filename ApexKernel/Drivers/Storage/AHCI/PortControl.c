// ApexKernel/Drivers/Storage/AHCI/PortControl.c
// ApexOS -- AHCI port low-level control (start/stop command engine)

#include "Drivers/Storage/AHCI/PortControl.h"
#include "Drivers/Storage/AHCI/AHCIInternal.h"
#include "Core/Time/PITCounter.h"

bool ahci_wait_clear(volatile uint32_t *reg, uint32_t mask, uint32_t timeout_ms)
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

bool ahci_port_stop_cmd(hba_port_t *port)
{
    port->cmd &= ~HBA_PxCMD_ST;
    port->cmd &= ~HBA_PxCMD_FRE;

    if (!ahci_wait_clear(PACKED_FIELD_ADDR(port, cmd), HBA_PxCMD_FR, 500))
        return false;
    if (!ahci_wait_clear(PACKED_FIELD_ADDR(port, cmd), HBA_PxCMD_CR, 500))
        return false;

    return true;
}

void ahci_port_start_cmd(hba_port_t *port)
{
    while (port->cmd & HBA_PxCMD_CR)
        ; // brief spin -- CR clearing after a stop is near-instant in practice

    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST;
}
