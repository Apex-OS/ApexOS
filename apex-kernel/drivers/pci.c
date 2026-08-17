// apex-kernel/drivers/pci.c
// ApexOS -- PCI bus driver (see pci.h)

#include "pci.h"
#include "../include/apex/io.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

// CONFIG_ADDRESS layout (see the PCI Local Bus spec, or any OSDev PCI
// page -- this exact bit layout is universal across all x86 PCI
// hosts):
//   bit 31    : enable (must be 1)
//   bits 30-24: reserved (0)
//   bits 23-16: bus number
//   bits 15-11: device/slot number
//   bits 10-8 : function number
//   bits 7-2  : register offset (dword-aligned -- bits 0-1 dropped)
//   bits 1-0  : 0 (dword access only)
static uint32_t pci_make_address(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset)
{
    return (1u << 31) |
           ((uint32_t)bus << 16) |
           ((uint32_t)(slot & 0x1F) << 11) |
           ((uint32_t)(function & 0x07) << 8) |
           (offset & 0xFC);
}

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset)
{
    outl(PCI_CONFIG_ADDRESS, pci_make_address(bus, slot, function, offset));
    return inl(PCI_CONFIG_DATA);
}

void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint32_t value)
{
    outl(PCI_CONFIG_ADDRESS, pci_make_address(bus, slot, function, offset));
    outl(PCI_CONFIG_DATA, value);
}

// Reads the vendor ID (low 16 bits of register 0x00). 0xFFFF means
// "no device here" -- every real PCI function returns something else,
// so this is the standard way to detect an empty slot/function
// without needing a separate presence bit anywhere.
static uint16_t pci_read_vendor_id(uint8_t bus, uint8_t slot, uint8_t function)
{
    return (uint16_t)(pci_config_read32(bus, slot, function, 0x00) & 0xFFFF);
}

// Fills a pci_device_t from a function already known to be present
// (caller checked vendor_id != 0xFFFF first).
static void pci_read_device(uint8_t bus, uint8_t slot, uint8_t function, pci_device_t *out)
{
    uint32_t reg0 = pci_config_read32(bus, slot, function, 0x00);
    uint32_t reg2 = pci_config_read32(bus, slot, function, 0x08);
    uint32_t reg3 = pci_config_read32(bus, slot, function, 0x0C);

    out->bus = bus;
    out->slot = slot;
    out->function = function;

    out->vendor_id = (uint16_t)(reg0 & 0xFFFF);
    out->device_id = (uint16_t)((reg0 >> 16) & 0xFFFF);

    out->revision_id = (uint8_t)(reg2 & 0xFF);
    out->prog_if = (uint8_t)((reg2 >> 8) & 0xFF);
    out->subclass = (uint8_t)((reg2 >> 16) & 0xFF);
    out->class_code = (uint8_t)((reg2 >> 24) & 0xFF);

    out->header_type = (uint8_t)((reg3 >> 16) & 0xFF);
}

bool pci_find_device(uint8_t class_code, uint8_t subclass, uint8_t prog_if, pci_device_t *out)
{
    // Brute-force scan of every possible bus/slot/function -- 256 * 32
    // * 8 = 65536 config reads in the worst case. Slow-sounding but
    // each read is a couple of port I/O instructions, and this only
    // runs once at boot; not worth the complexity of walking bridges
    // properly (the recursive bus-topology approach) for a V1.
    for (uint16_t bus = 0; bus < 256; bus++)
    {
        for (uint8_t slot = 0; slot < 32; slot++)
        {
            uint16_t vendor = pci_read_vendor_id((uint8_t)bus, slot, 0);
            if (vendor == 0xFFFF)
                continue; // nothing in this slot at all

            // Function 0 always exists if the slot is populated;
            // whether to probe functions 1-7 depends on the
            // multi-function bit in function 0's header type.
            uint32_t reg3 = pci_config_read32((uint8_t)bus, slot, 0, 0x0C);
            bool multi_function = (reg3 & 0x00800000) != 0;
            uint8_t max_function = multi_function ? 8 : 1;

            for (uint8_t function = 0; function < max_function; function++)
            {
                if (pci_read_vendor_id((uint8_t)bus, slot, function) == 0xFFFF)
                    continue;

                pci_device_t dev;
                pci_read_device((uint8_t)bus, slot, function, &dev);

                if (dev.class_code == class_code &&
                    dev.subclass == subclass &&
                    (prog_if == 0xFF || dev.prog_if == prog_if))
                {
                    *out = dev;
                    return true;
                }
            }
        }
    }

    return false;
}