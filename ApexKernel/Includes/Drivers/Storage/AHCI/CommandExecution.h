// ApexKernel/Includes/Drivers/Storage/AHCI/CommandExecution.h
// ApexOS -- AHCI command building and issuing (IDENTIFY/READ/WRITE DMA EXT)

#ifndef APEX_DRIVERS_STORAGE_AHCI_COMMAND_EXECUTION_H
#define APEX_DRIVERS_STORAGE_AHCI_COMMAND_EXECUTION_H

#include "Drivers/Storage/AHCI/AHCI.h"

// Finds a free command slot (one whose bit is clear in both PxCI and
// PxSACT -- not currently issued, not marked active) -- returns -1 if
// every slot this driver uses is busy. With V1 issuing one command at
// a time and waiting for it to finish before returning, this should
// always find slot 0 free in practice; the search is here so the
// driver isn't silently wrong if that assumption ever stops holding.
int ahci_find_free_slot(hba_port_t *port);

// Builds and issues one command (IDENTIFY, or a READ/WRITE DMA EXT
// covering `count` sectors starting at `lba`), waits for it to
// complete, and transfers data to/from `buf` accordingly. IDENTIFY
// always transfers exactly one 512-byte sector and ignores
// `lba`/`count`; for READ, `buf` receives the drive's data; for
// WRITE, `buf` is the source data sent to the drive.
bool ahci_do_command(hba_port_t *port, ahci_cmd_kind_t kind, uint64_t lba, uint32_t count, void *buf);

#endif // APEX_DRIVERS_STORAGE_AHCI_COMMAND_EXECUTION_H
