// ApexKernel/Includes/Drivers/Network/Vendors/Intel/E1000.h
// ApexOS -- Intel 8254x (e1000) Gigabit Ethernet driver
//
// Targets the 82540EM specifically (QEMU's default `-device e1000`
// model, PCI vendor 0x8086 / device 0x100E) -- the same chip family
// documented at https://wiki.osdev.org/Intel_8254x and used across
// most teaching OS projects (MIT 6.828/JOS, xv6). Register offsets
// and descriptor layouts below are cross-checked against that page,
// the MINIX e1000 driver, and several independent from-scratch driver
// implementations (Go, Rust) that all agree on the same values.
//
// V1 scope: polling only, no interrupts -- e1000_poll_receive() is
// called from the network stack's main loop rather than an IRQ
// handler. Interrupt-driven RX is future work once there's a real
// event loop to hand packets to; polling is simpler to get right
// first and this chip's poll-for-DD-bit pattern is identical either
// way (an interrupt handler would do the exact same descriptor walk,
// just triggered by IRQ instead of being called periodically).

#ifndef APEX_DRIVERS_NETWORK_E1000_H
#define APEX_DRIVERS_NETWORK_E1000_H

#include "Core/Types.h"

#define E1000_VENDOR_ID 0x8086
#define E1000_DEVICE_ID_82540EM 0x100E

// ── Register offsets (byte offsets into the BAR0 MMIO region) ──────
#define E1000_REG_CTRL 0x0000              // Device Control
#define E1000_REG_STATUS 0x0008            // Device Status
#define E1000_REG_EECD 0x0010              // EEPROM/Flash Control
#define E1000_REG_EERD 0x0014              // EEPROM Read
#define E1000_REG_ICR 0x00C0               // Interrupt Cause Read
#define E1000_REG_ITR 0x00C4               // Interrupt Throttling Rate
#define E1000_REG_ICS 0x00C8               // Interrupt Cause Set
#define E1000_REG_IMS 0x00D0               // Interrupt Mask Set
#define E1000_REG_IMC 0x00D8               // Interrupt Mask Clear
#define E1000_REG_RCTL 0x0100              // Receive Control
#define E1000_REG_TCTL 0x0400              // Transmit Control
#define E1000_REG_TIPG 0x0410              // Transmit Inter Packet Gap
#define E1000_REG_RDBAL 0x2800             // RX Descriptor Base Low
#define E1000_REG_RDBAH 0x2804             // RX Descriptor Base High
#define E1000_REG_RDLEN 0x2808             // RX Descriptor Length
#define E1000_REG_RDH 0x2810               // RX Descriptor Head
#define E1000_REG_RDT 0x2818               // RX Descriptor Tail
#define E1000_REG_TDBAL 0x3800             // TX Descriptor Base Low
#define E1000_REG_TDBAH 0x3804             // TX Descriptor Base High
#define E1000_REG_TDLEN 0x3808             // TX Descriptor Length
#define E1000_REG_TDH 0x3810               // TX Descriptor Head
#define E1000_REG_TDT 0x3818               // TX Descriptor Tail
#define E1000_REG_MTA_BASE 0x5200          // Multicast Table Array (128 entries)
#define E1000_REG_RECEIVE_ADDR_LOW 0x5400  // MAC address, low 32 bits
#define E1000_REG_RECEIVE_ADDR_HIGH 0x5404 // MAC address, high 16 bits + Address Valid bit

// ── CTRL bits ────────────────────────────────────────────────────────
#define E1000_CTRL_ASDE (1u << 5) // Auto-Speed Detection Enable
#define E1000_CTRL_SLU (1u << 6)  // Set Link Up
#define E1000_CTRL_RST (1u << 26) // Device Reset

// ── RCTL bits ────────────────────────────────────────────────────────
#define E1000_RCTL_EN (1u << 1)          // Receiver Enable
#define E1000_RCTL_BAM (1u << 15)        // Broadcast Accept Mode
#define E1000_RCTL_BSIZE_2048 (0u << 16) // Receive buffer size 2048 bytes (BSEX=0)
#define E1000_RCTL_SECRC (1u << 26)      // Strip Ethernet CRC from received packets

// ── TCTL bits ────────────────────────────────────────────────────────
#define E1000_TCTL_EN (1u << 1)  // Transmitter Enable
#define E1000_TCTL_PSP (1u << 3) // Pad Short Packets

// ── TX descriptor CMD/STATUS bits ───────────────────────────────────
#define E1000_TXD_CMD_EOP (1u << 0) // End Of Packet
#define E1000_TXD_CMD_RS (1u << 3)  // Report Status (sets STAT_DD once transmitted)
#define E1000_TXD_STAT_DD (1u << 0) // Descriptor Done

// ── RX descriptor STATUS bits ───────────────────────────────────────
#define E1000_RXD_STAT_DD (1u << 0)  // Descriptor Done
#define E1000_RXD_STAT_EOP (1u << 1) // End Of Packet

// Ring sizes -- RDLEN/TDLEN must be a multiple of 128 bytes (the
// spec's max cache line size), and each descriptor is 16 bytes, so
// the descriptor count must be a multiple of 8. 32 descriptors (512
// bytes, well under one page) is comfortable headroom for polling-mode
// use without needing more than a single page for each ring.
#define E1000_NUM_RX_DESC 32
#define E1000_NUM_TX_DESC 32

// Fixed per-packet buffer size -- matches E1000_RCTL_BSIZE_2048 above
// (must agree: the hardware writes up to this many bytes into
// whatever buffer a descriptor's addr field points at). Comfortably
// larger than a standard 1500-byte Ethernet MTU + headers.
#define E1000_RX_BUFFER_SIZE 2048
#define E1000_TX_BUFFER_SIZE 2048

// Transmit descriptor -- 16 bytes, matches the hardware layout
// exactly (Intel 8254x manual section 3.3.3 / cross-checked against
// MIT 6.828's JOS driver and several independent implementations,
// see this file's header comment).
typedef struct __attribute__((packed))
{
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} e1000_tx_desc_t;

// Receive descriptor -- 16 bytes, matches the hardware layout exactly
// (Intel 8254x manual section 3.2.3, same cross-check as
// e1000_tx_desc_t above).
typedef struct __attribute__((packed))
{
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} e1000_rx_desc_t;

// Initializes the e1000 NIC: finds it on the PCI bus, maps its MMIO
// BAR0, resets it, reads its permanent MAC address out of the
// RECEIVE_ADDR registers (set by the card from its EEPROM at power-on
// -- see this file's header comment), sets up the RX/TX descriptor
// rings, and enables the receiver/transmitter. Returns false (with a
// klog_warn explaining why) if no e1000 is found on the bus or any
// setup step fails -- the network stack built on top of this should
// treat that the same as "no network hardware available", not a fatal
// boot error.
bool e1000_init(void);

// Returns this NIC's 6-byte MAC address, filled in by e1000_init()
// from hardware. Callers (Ethernet.c) use this to fill in the source
// address of every frame they send. Only valid after e1000_init()
// returns true.
void e1000_get_mac(uint8_t out_mac[6]);

// Queues one Ethernet frame for transmission -- `data`/`length` is the
// complete frame (destination MAC, source MAC, EtherType, payload;
// see Ethernet.c), not including the hardware-computed CRC (RCTL's
// SECRC strips it on receive; the card computes and appends it on
// transmit automatically, this driver never touches it). Returns
// false if length exceeds E1000_TX_BUFFER_SIZE or no free TX
// descriptor is available (the ring is full -- shouldn't happen at
// this project's current traffic volumes, but checked rather than
// silently overwriting an in-flight descriptor).
bool e1000_send_packet(const void *data, uint16_t length);

// Checks the RX ring for a completed (STAT_DD set) packet and, if one
// is present, copies it into `out_buffer` (must be at least
// E1000_RX_BUFFER_SIZE bytes) and returns its length. Returns 0 if no
// packet is currently available -- callers (the network stack's main
// loop) call this repeatedly (polling, see this file's header comment
// on why V1 has no interrupt path yet).
uint16_t e1000_poll_receive(void *out_buffer);

#endif // APEX_DRIVERS_NETWORK_E1000_H
