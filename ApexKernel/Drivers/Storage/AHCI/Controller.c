// ApexKernel/Drivers/Storage/AHCI/Controller.c
// ApexOS -- AHCI controller bootstrap (PCI discovery, MMIO mapping, port init)

#include "Drivers/Storage/AHCI/Controller.h"
#include "Drivers/Storage/AHCI/AHCI.h"
#include "Drivers/Storage/AHCI/AHCIInternal.h"
#include "Drivers/Storage/AHCI/PortMemory.h"
#include "Drivers/Storage/AHCI/CommandExecution.h"
#include "Bus/PCI/PCI.h"
#include "Core/Memory/Virtual/PageTables.h"
#include "Logs/Log.h"

// PCI config space offset of BAR5 -- where the AHCI spec requires the
// ABAR (AHCI Base Memory Register) to live for any AHCI controller.
#define PCI_BAR5_OFFSET 0x24

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
