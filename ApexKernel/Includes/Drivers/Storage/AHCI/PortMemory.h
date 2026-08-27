// ApexKernel/Includes/Drivers/Storage/AHCI/PortMemory.h
// ApexOS -- AHCI per-port DMA memory setup (command list, FIS, PRDT)

#ifndef APEX_DRIVERS_STORAGE_AHCI_PORT_MEMORY_H
#define APEX_DRIVERS_STORAGE_AHCI_PORT_MEMORY_H

#include "Drivers/Storage/AHCI/AHCI.h"

// Allocates and wires up the command list, received-FIS area, and per-
// slot command tables for one port, then starts its command engine.
// Returns false if any allocation fails (out of physical memory) --
// callers treat that the same as "no usable drive found".
bool ahci_port_init_memory(hba_port_t *port);

#endif // APEX_DRIVERS_STORAGE_AHCI_PORT_MEMORY_H
