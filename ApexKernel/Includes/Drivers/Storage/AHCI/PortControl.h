// ApexKernel/Includes/Drivers/Storage/AHCI/PortControl.h
// ApexOS -- AHCI port low-level control (start/stop command engine)

#ifndef APEX_DRIVERS_STORAGE_AHCI_PORT_CONTROL_H
#define APEX_DRIVERS_STORAGE_AHCI_PORT_CONTROL_H

#include "Drivers/Storage/AHCI/AHCI.h"

// Spins until `cond` becomes false or `timeout_ms` elapses. Returns
// true if `cond` cleared in time. pit_get_ticks() increments every
// 10ms (see Core/Time/PITCounter.h: PIT_TARGET_FREQ), so this rounds
// the timeout up to the nearest tick -- fine for the multi-hundred-ms
// timeouts AHCI command completion needs.
bool ahci_wait_clear(volatile uint32_t *reg, uint32_t mask, uint32_t timeout_ms);

// Stops the port's command engine (clears PxCMD.ST and waits for
// PxCMD.CR to clear) -- required before touching clb/fb, per the AHCI
// spec (section 10.3.1: "software shall not write... while ST is set").
bool ahci_port_stop_cmd(hba_port_t *port);

// Starts the port's command engine (FIS receive, then command list
// processing) -- the reverse of ahci_port_stop_cmd(), done once clb/fb
// are set up and it's safe to let the HBA start issuing commands.
void ahci_port_start_cmd(hba_port_t *port);

#endif // APEX_DRIVERS_STORAGE_AHCI_PORT_CONTROL_H
