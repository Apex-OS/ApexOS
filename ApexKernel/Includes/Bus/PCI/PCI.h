// ApexKernel/Includes/Bus/PCI/PCI.h
// ApexOS -- PCI bus driver (configuration space access + device scan)
//
// PCI configuration space is accessed via two 32-bit I/O ports:
// 0xCF8 (CONFIG_ADDRESS) selects which device/function/register to
// read or write, 0xCFC (CONFIG_DATA) is where the actual 32-bit value
// shows up. This is the "legacy" mechanism (as opposed to the newer
// memory-mapped ECAM) -- simpler, universally supported, and all this
// kernel needs for device discovery (AHCI, and later NIC/etc).
//
// This driver's job is narrow on purpose: enumerate devices and let
// callers find the one they want by class/subclass/prog_if (e.g. mass
// storage / SATA / AHCI = 0x01/0x06/0x01, see
// ApexKernel/Drivers/Storage/AHCI/). It does NOT configure BARs,
// enable bus mastering, or anything device-specific -- that's each
// device driver's job once it has found its pci_device_t.

#ifndef APEX_BUS_PCI_H
#define APEX_BUS_PCI_H

#include "Core/Types.h"

// One PCI function, as found during the bus scan. Holds just enough
// to let a device driver take it from here: its bus/slot/function
// address (needed for any further pci_config_read/write32 calls) and
// the identifying fields drivers match against.
typedef struct
{
    uint8_t bus;
    uint8_t slot; // "device" in PCI terminology -- named slot here to
                  // avoid clashing with the generic "device" the
                  // caller is searching for
    uint8_t function;

    uint16_t vendor_id;
    uint16_t device_id;

    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision_id;

    uint8_t header_type;
} pci_device_t;

// Reads a 32-bit register from a device's configuration space.
// `offset` must be 4-byte aligned (bits 0-1 are ignored by the
// hardware) -- pass the full byte offset (e.g. 0x10 for BAR0), not a
// dword index.
uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);

// Writes a 32-bit register to a device's configuration space. Same
// alignment rule as pci_config_read32().
void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint32_t value);

// Scans every bus/slot/function (0-255 / 0-31 / 0-7) for a device
// whose class/subclass/prog_if match. Returns true and fills `out` on
// the first match found, false if nothing matches. prog_if narrows
// the search further (e.g. AHCI vs legacy IDE both live under
// class=0x01 subclass=0x06, prog_if is what tells them apart) -- pass
// 0xFF to match any prog_if within the given class/subclass.
bool pci_find_device(uint8_t class_code, uint8_t subclass, uint8_t prog_if, pci_device_t *out);

// Same brute-force scan as pci_find_device(), but matching on exact
// vendor_id/device_id instead of class/subclass -- what a driver for
// one specific chip (e.g. Drivers/Network/E1000/, matching Intel
// 0x8086 / 82540EM 0x100E) needs rather than "any device in this
// class", since several unrelated chips can share a class/subclass
// (e.g. many different NICs are all class=0x02 subclass=0x00).
bool pci_find_device_by_id(uint16_t vendor_id, uint16_t device_id, pci_device_t *out);

#endif // APEX_BUS_PCI_H
