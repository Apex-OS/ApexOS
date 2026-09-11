// ApexKernel/Drivers/Network/Vendors/Intel/E1000.c
// ApexOS -- Intel 8254x (e1000) Gigabit Ethernet driver (see E1000.h)

#include "Drivers/Network/Vendors/Intel/E1000.h"
#include "Bus/PCI/PCI.h"
#include "Core/Memory/Physical/PageBitmap.h"
#include "Core/Memory/Virtual/PageTables.h"
#include "Logs/Log.h"

// PCI config space offset of the COMMAND register -- bit 2 (Bus
// Master Enable) must be set for this (or any) PCI device to perform
// DMA. Without it, TX/RX descriptor rings are set up correctly in
// software but the card can never actually read a TX descriptor or
// write a received packet into a RX buffer -- it has no permission to
// touch system memory at all. QEMU's "-device e1000" does not enable
// this automatically the way some integrated controllers (e.g. the
// q35 chipset's own AHCI) effectively appear to; a discrete PCI NIC
// needs it turned on explicitly by its driver, same as real hardware.
#define PCI_COMMAND_OFFSET 0x04
#define PCI_COMMAND_BUS_MASTER (1u << 2)

// PCI config space offset of BAR0 -- where the e1000's MMIO register
// window is always exposed (unlike AHCI's ABAR at BAR5, this is just
// this chip's own layout, see the Intel 8254x manual section 5.1).
#define PCI_BAR0_OFFSET 0x10

static volatile uint32_t *g_mmio = NULL;
static uint8_t g_mac[6];

static e1000_rx_desc_t *g_rx_ring = NULL;
static e1000_tx_desc_t *g_tx_ring = NULL;
static void *g_rx_buffers[E1000_NUM_RX_DESC];
static void *g_tx_buffers[E1000_NUM_TX_DESC];
static uint32_t g_rx_tail = 0; // mirrors the hardware RDT -- next descriptor slot to (re)hand to the card
static uint32_t g_tx_tail = 0; // mirrors the hardware TDT -- next descriptor slot to fill on send

static inline uint32_t reg_read(uint32_t offset)
{
    return g_mmio[offset / 4];
}

static inline void reg_write(uint32_t offset, uint32_t value)
{
    g_mmio[offset / 4] = value;
}

// Allocates and zeroes one physical page, identity-mapped uncacheable
// -- same pattern as Drivers/Storage/AHCI/PortMemory.c's
// ahci_alloc_zeroed_page(), duplicated here rather than shared since
// it's a three-line, self-contained helper and pulling in a cross-
// driver dependency for it isn't worth it.
static void *alloc_zeroed_page(void)
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

// Reads the card's MAC address out of RECEIVE_ADDR_LOW/HIGH -- for a
// card that's already been through firmware/BIOS POST (true under
// QEMU and on real hardware), these registers are already populated
// from the EEPROM at power-on, so no manual EEPROM read protocol is
// needed for this V1 (see E1000.h's header comment on scope).
static void read_mac_address(void)
{
    uint32_t low = reg_read(E1000_REG_RECEIVE_ADDR_LOW);
    uint32_t high = reg_read(E1000_REG_RECEIVE_ADDR_HIGH);

    g_mac[0] = (uint8_t)(low & 0xFF);
    g_mac[1] = (uint8_t)((low >> 8) & 0xFF);
    g_mac[2] = (uint8_t)((low >> 16) & 0xFF);
    g_mac[3] = (uint8_t)((low >> 24) & 0xFF);
    g_mac[4] = (uint8_t)(high & 0xFF);
    g_mac[5] = (uint8_t)((high >> 8) & 0xFF);
}

static bool setup_rx_ring(void)
{
    g_rx_ring = (e1000_rx_desc_t *)alloc_zeroed_page();
    if (!g_rx_ring)
        return false;

    for (uint32_t i = 0; i < E1000_NUM_RX_DESC; i++)
    {
        g_rx_buffers[i] = alloc_zeroed_page();
        if (!g_rx_buffers[i])
            return false;

        g_rx_ring[i].addr = (uint64_t)g_rx_buffers[i];
        g_rx_ring[i].status = 0;
    }

    uint64_t ring_phys = (uint64_t)g_rx_ring;
    reg_write(E1000_REG_RDBAL, (uint32_t)(ring_phys & 0xFFFFFFFF));
    reg_write(E1000_REG_RDBAH, (uint32_t)(ring_phys >> 32));
    reg_write(E1000_REG_RDLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t));
    reg_write(E1000_REG_RDH, 0);

    // RDT (tail) is set to the *last* descriptor index, not
    // NUM_RX_DESC (one past the end) -- the card treats RDT as "the
    // last descriptor I'm allowed to write into", and head == tail
    // means "ring empty" to the hardware, so leaving one descriptor
    // (index NUM_RX_DESC - 1) marked as not-yet-given-to-the-card
    // avoids that ambiguous full/empty state. This is the same
    // off-by-one every from-scratch e1000 driver in this file's
    // research handles the same way.
    g_rx_tail = E1000_NUM_RX_DESC - 1;
    reg_write(E1000_REG_RDT, g_rx_tail);

    reg_write(E1000_REG_RCTL,
              E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SECRC | E1000_RCTL_BSIZE_2048);

    return true;
}

static bool setup_tx_ring(void)
{
    g_tx_ring = (e1000_tx_desc_t *)alloc_zeroed_page();
    if (!g_tx_ring)
        return false;

    for (uint32_t i = 0; i < E1000_NUM_TX_DESC; i++)
    {
        g_tx_buffers[i] = alloc_zeroed_page();
        if (!g_tx_buffers[i])
            return false;

        g_tx_ring[i].addr = (uint64_t)g_tx_buffers[i];
        // STAT_DD set on every descriptor up front -- e1000_send_packet()
        // checks this bit to know a slot is free to reuse (see that
        // function). All slots start "free" since nothing's been sent
        // yet.
        g_tx_ring[i].status = E1000_TXD_STAT_DD;
    }

    uint64_t ring_phys = (uint64_t)g_tx_ring;
    reg_write(E1000_REG_TDBAL, (uint32_t)(ring_phys & 0xFFFFFFFF));
    reg_write(E1000_REG_TDBAH, (uint32_t)(ring_phys >> 32));
    reg_write(E1000_REG_TDLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));
    reg_write(E1000_REG_TDH, 0);
    reg_write(E1000_REG_TDT, 0);
    g_tx_tail = 0;

    // TIPG values from the Intel 8254x manual section 13.4.34 (also
    // this file's cross-checked research) -- IPGT=10, IPGR1=8,
    // IPGR2=6, packed into the register's three sub-fields.
    reg_write(E1000_REG_TIPG, 10 | (8 << 10) | (6 << 20));

    reg_write(E1000_REG_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP);

    return true;
}

bool e1000_init(void)
{
    pci_device_t dev;
    if (!pci_find_device_by_id(E1000_VENDOR_ID, E1000_DEVICE_ID_82540EM, &dev))
    {
        klog_warn("e1000_init: no e1000 NIC found on the PCI bus");
        return false;
    }

    uint32_t bar0 = pci_config_read32(dev.bus, dev.slot, dev.function, PCI_BAR0_OFFSET);
    // Bit 0 = 0 for a memory BAR (vs I/O), bits 2:1 = address type (0 =
    // 32-bit, 2 = 64-bit) -- this driver only handles the 32-bit case,
    // which is what QEMU's e1000 model presents (see E1000.h's header
    // comment on scope).
    uint64_t mmio_phys = bar0 & 0xFFFFFFF0u;

    // Enables Bus Mastering -- see PCI_COMMAND_BUS_MASTER's
    // definition above for why this is mandatory and easy to miss
    // (everything else about setup can look correct while this alone
    // silently makes every DMA the card would do a no-op). Preserves
    // whatever else is already set in COMMAND (e.g. Memory Space
    // Enable, generally already on for a BAR to be usable at all)
    // rather than overwriting the whole register.
    uint32_t command = pci_config_read32(dev.bus, dev.slot, dev.function, PCI_COMMAND_OFFSET);
    pci_config_write32(dev.bus, dev.slot, dev.function, PCI_COMMAND_OFFSET,
                       command | PCI_COMMAND_BUS_MASTER);

    // The e1000's MMIO register window is 128KB (see the Intel 8254x
    // manual's register map) -- several pages, all mapped uncacheable
    // for the same reason as any other MMIO region (see AHCI's
    // Controller.c: ahci_init() for the equivalent comment on that
    // driver).
    for (uint64_t offset = 0; offset < 0x20000; offset += PAGE_SIZE)
        vmm_map(mmio_phys + offset, mmio_phys + offset, VMM_KERNEL_RW | VMM_NO_CACHE);

    g_mmio = (volatile uint32_t *)mmio_phys;

    // Full device reset -- CTRL.RST is self-clearing, but the card
    // needs a moment to actually complete the reset before further
    // register writes are meaningful. Busy-waits on STATUS being
    // readable as a crude but effective "the card is back" check
    // (reading all-1s, 0xFFFFFFFF, generally means the device isn't
    // responding yet/at all).
    reg_write(E1000_REG_CTRL, reg_read(E1000_REG_CTRL) | E1000_CTRL_RST);
    for (volatile int i = 0; i < 1000000; i++)
    {
        if (reg_read(E1000_REG_STATUS) != 0xFFFFFFFF)
            break;
    }

    // Auto-speed detection + set-link-up -- without SLU, the card
    // stays administratively down regardless of physical link state,
    // and nothing will ever transmit or receive (this is a documented
    // gotcha every from-scratch e1000 driver in this file's research
    // hits and fixes the same way).
    reg_write(E1000_REG_CTRL, reg_read(E1000_REG_CTRL) | E1000_CTRL_ASDE | E1000_CTRL_SLU);

    // Clears the Multicast Table Array -- 128 32-bit entries, all
    // zeroed so no multicast address is accepted until something
    // explicitly asks for one (not needed for this V1's unicast/
    // broadcast-only use, see E1000.h's header comment on scope, but
    // left in a known-clean state rather than whatever the firmware
    // happened to leave it in).
    for (uint32_t i = 0; i < 128; i++)
        reg_write(E1000_REG_MTA_BASE + i * 4, 0);

    // Masks all interrupts -- this V1 is polling-only (see E1000.h's
    // header comment), so nothing should ever actually fire one, but
    // masking them explicitly avoids depending on whatever state the
    // firmware/QEMU defaults left the IMS register in.
    reg_write(E1000_REG_IMC, 0xFFFFFFFF);

    read_mac_address();

    if (!setup_rx_ring() || !setup_tx_ring())
    {
        klog_error("e1000_init: RX/TX ring setup failed (out of memory?)");
        return false;
    }

    klog_ok_n("e1000_init: NIC ready, MAC address ends in", g_mac[5]);
    return true;
}

void e1000_get_mac(uint8_t out_mac[6])
{
    for (int i = 0; i < 6; i++)
        out_mac[i] = g_mac[i];
}

bool e1000_send_packet(const void *data, uint16_t length)
{
    if (length > E1000_TX_BUFFER_SIZE)
        return false;

    e1000_tx_desc_t *desc = &g_tx_ring[g_tx_tail];

    // STAT_DD means the hardware finished with this descriptor's
    // previous packet (or it's a slot that's never been used, see
    // setup_tx_ring()'s initialization) -- if it's not set, the card
    // is still transmitting whatever was queued here last, and this
    // slot isn't safe to reuse yet. At this project's current traffic
    // volumes (ICMP echo, ARP -- see the network stack built on top of
    // this) with a 32-descriptor ring, this should never actually be
    // hit, but it's checked rather than assumed.
    if (!(desc->status & E1000_TXD_STAT_DD))
        return false;

    uint8_t *buf = (uint8_t *)g_tx_buffers[g_tx_tail];
    const uint8_t *src = (const uint8_t *)data;
    for (uint16_t i = 0; i < length; i++)
        buf[i] = src[i];

    desc->length = length;
    desc->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
    desc->status = 0; // cleared here, set back to STAT_DD by the card once it's actually sent

    g_tx_tail = (g_tx_tail + 1) % E1000_NUM_TX_DESC;
    reg_write(E1000_REG_TDT, g_tx_tail);

    return true;
}

uint16_t e1000_poll_receive(void *out_buffer)
{
    // The next descriptor to check is one past the last one already
    // handed back to the card (g_rx_tail) -- see setup_rx_ring()'s
    // comment on the head/tail-off-by-one convention this follows.
    uint32_t next = (g_rx_tail + 1) % E1000_NUM_RX_DESC;
    e1000_rx_desc_t *desc = &g_rx_ring[next];

    if (!(desc->status & E1000_RXD_STAT_DD))
        return 0; // nothing new since the last poll

    uint16_t length = desc->length;
    uint8_t *buf = (uint8_t *)g_rx_buffers[next];
    uint8_t *dst = (uint8_t *)out_buffer;
    for (uint16_t i = 0; i < length; i++)
        dst[i] = buf[i];

    // Hands the descriptor back to the card by clearing its status
    // and advancing RDT to include it -- this is what tells the
    // hardware "you can write a new packet into this buffer again".
    desc->status = 0;
    g_rx_tail = next;
    reg_write(E1000_REG_RDT, g_rx_tail);

    return length;
}
