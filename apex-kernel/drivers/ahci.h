// apex-kernel/drivers/ahci.h
// ApexOS -- AHCI (Advanced Host Controller Interface) SATA driver
//
// V1 scope: finds the AHCI controller via PCI (see drivers/pci.h),
// maps its MMIO registers (ABAR, PCI BAR5), initializes exactly ONE
// port (the first with a SATA drive attached), and supports reading
// physical sectors via DMA. No writing yet, no multi-port, no
// interrupts (polls the command-issue register instead) -- enough to
// validate the whole AHCI plumbing works before axfs_disk.c grows a
// real-disk backend on top of it.
//
// Structures below follow the AHCI 1.3 spec exactly (same field names
// as the OSDev AHCI wiki page and the Intel spec itself) -- anyone
// cross-referencing either will recognize every field here.

#ifndef APEX_AHCI_H
#define APEX_AHCI_H

#include "../include/apex/types.h"

// ── FIS types (Serial ATA Revision 3.0) ─────────────────────────────────
typedef enum
{
    FIS_TYPE_REG_H2D = 0x27,   // Register FIS - host to device
    FIS_TYPE_REG_D2H = 0x34,   // Register FIS - device to host
    FIS_TYPE_DMA_ACT = 0x39,   // DMA activate FIS - device to host
    FIS_TYPE_DMA_SETUP = 0x41, // DMA setup FIS - bidirectional
    FIS_TYPE_DATA = 0x46,      // Data FIS - bidirectional
    FIS_TYPE_BIST = 0x58,      // BIST activate FIS - bidirectional
    FIS_TYPE_PIO_SETUP = 0x5F, // PIO setup FIS - device to host
    FIS_TYPE_DEV_BITS = 0xA1,  // Set device bits FIS - device to host
} fis_type_t;

// Host-to-device register FIS -- how the host issues an ATA command
// (read/write/identify/etc) to the drive. Built fresh for every
// command, written into the command table's cfis[] field (see
// hba_cmd_tbl_t below).
typedef struct __attribute__((packed))
{
    uint8_t fis_type; // FIS_TYPE_REG_H2D

    uint8_t pmport : 4; // Port multiplier
    uint8_t rsv0 : 3;
    uint8_t c : 1; // 1: Command, 0: Control

    uint8_t command;  // Command register (see AHCI_CMD_* below)
    uint8_t featurel; // Feature register, 7:0

    uint8_t lba0; // LBA 7:0
    uint8_t lba1; // LBA 15:8
    uint8_t lba2; // LBA 23:16
    uint8_t device;

    uint8_t lba3; // LBA 31:24
    uint8_t lba4; // LBA 39:32
    uint8_t lba5; // LBA 47:40
    uint8_t featureh;

    uint8_t countl; // Sector count, 7:0
    uint8_t counth; // Sector count, 15:8
    uint8_t icc;
    uint8_t control;

    uint8_t rsv1[4];
} fis_reg_h2d_t;

// ── HBA memory registers (see ahci_hba_mem_t) ───────────────────────────
// Port control registers -- one of these per implemented port, at
// ABAR + 0x100 + port_index * 0x80.
typedef volatile struct __attribute__((packed))
{
    uint32_t clb;  // 0x00, command list base address (1K-aligned)
    uint32_t clbu; // 0x04, command list base address, upper 32 bits
    uint32_t fb;   // 0x08, FIS base address (256-byte aligned)
    uint32_t fbu;  // 0x0C, FIS base address, upper 32 bits
    uint32_t is;   // 0x10, interrupt status
    uint32_t ie;   // 0x14, interrupt enable
    uint32_t cmd;  // 0x18, command and status
    uint32_t rsv0;
    uint32_t tfd;  // 0x20, task file data
    uint32_t sig;  // 0x24, device signature
    uint32_t ssts; // 0x28, SATA status (SStatus)
    uint32_t sctl; // 0x2C, SATA control (SControl)
    uint32_t serr; // 0x30, SATA error (SError)
    uint32_t sact; // 0x34, SATA active (SActive)
    uint32_t ci;   // 0x38, command issue
    uint32_t sntf; // 0x3C, SATA notification
    uint32_t fbs;  // 0x40, FIS-based switching control
    uint32_t rsv1[11];
    uint32_t vendor[4];
} hba_port_t;

// Full HBA register block, starting at ABAR (PCI BAR5). Generic Host
// Control (0x00-0x2B) affects the whole controller; ports[] (from
// 0x100 onward) is per-port -- see hba_port_t.
typedef volatile struct __attribute__((packed))
{
    // 0x00 - 0x2B, Generic Host Control
    uint32_t cap;     // 0x00, host capabilities
    uint32_t ghc;     // 0x04, global host control
    uint32_t is;      // 0x08, interrupt status
    uint32_t pi;      // 0x0C, ports implemented (one bit per port)
    uint32_t vs;      // 0x10, version
    uint32_t ccc_ctl; // 0x14
    uint32_t ccc_pts; // 0x18
    uint32_t em_loc;  // 0x1C
    uint32_t em_ctl;  // 0x20
    uint32_t cap2;    // 0x24
    uint32_t bohc;    // 0x28, BIOS/OS handoff control and status

    uint8_t rsv[0xA0 - 0x2C];
    uint8_t vendor[0x100 - 0xA0];

    hba_port_t ports[32]; // 0x100+, only ports with a set bit in `pi` are real
} hba_mem_t;

// One command header -- a "slot" in the command list. Points at the
// command table (ctba) that holds the actual FIS and PRDT entries for
// this command.
typedef struct __attribute__((packed))
{
    uint8_t cfl : 5; // Command FIS length, in DWORDs (2-16)
    uint8_t a : 1;   // ATAPI
    uint8_t w : 1;   // 1 = write (H2D), 0 = read (D2H)
    uint8_t p : 1;   // Prefetchable

    uint8_t r : 1; // Reset
    uint8_t b : 1; // BIST
    uint8_t c : 1; // Clear busy upon R_OK
    uint8_t rsv0 : 1;
    uint8_t pmp : 4;

    uint16_t prdtl; // PRDT entry count

    volatile uint32_t prdbc; // Bytes transferred so far (updated by the HBA)

    uint32_t ctba;  // Command table base address (128-byte aligned)
    uint32_t ctbau; // Upper 32 bits

    uint32_t rsv1[4];
} hba_cmd_header_t;

// One Physical Region Descriptor -- a (address, length) pair
// describing part of the data buffer for a command. AHCI_PRDT_COUNT
// entries live at the end of each command table.
typedef struct __attribute__((packed))
{
    uint32_t dba;  // Data base address
    uint32_t dbau; // Upper 32 bits
    uint32_t rsv0;

    uint32_t dbc : 22; // Byte count minus 1 (max 4MB per entry)
    uint32_t rsv1 : 9;
    uint32_t i : 1; // Interrupt on completion
} hba_prdt_entry_t;

// How many PRDT entries each command table gets -- see ahci.c for the
// memory layout this determines. 8 entries * 8K max each (see
// AHCI_PRDT_MAX_BYTES) covers a 64KB transfer in one command, plenty
// for reading a handful of AXFS blocks (4KB each) at a time.
#define AHCI_PRDT_COUNT 8
#define AHCI_PRDT_MAX_BYTES (8 * 1024) // keeps prdt.dbc comfortably away from its 4MB limit

// Command table: the actual command FIS, an ATAPI command buffer (not
// used in V1 -- no optical drive support), then the PRDT entries.
typedef struct __attribute__((packed))
{
    uint8_t cfis[64]; // Command FIS (fis_reg_h2d_t lives here)
    uint8_t acmd[16]; // ATAPI command, unused in V1
    uint8_t rsv[48];
    hba_prdt_entry_t prdt_entry[AHCI_PRDT_COUNT];
} hba_cmd_tbl_t;

// ── Port PxCMD bits ──────────────────────────────────────────────────────
#define HBA_PxCMD_ST 0x0001  // Start
#define HBA_PxCMD_FRE 0x0010 // FIS Receive Enable
#define HBA_PxCMD_FR 0x4000  // FIS Receive Running
#define HBA_PxCMD_CR 0x8000  // Command list Running

// ── Port PxSSTS.det / PxTFD bits ────────────────────────────────────────
#define HBA_PORT_DET_PRESENT 0x3
#define HBA_PORT_IPM_ACTIVE 0x1
#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ 0x08

// ── SATA device signatures (HBA_PORT.sig) ───────────────────────────────
#define SATA_SIG_ATA 0x00000101u   // Regular SATA drive -- what V1 supports
#define SATA_SIG_ATAPI 0xEB140101u // SATAPI (optical) -- detected, not supported yet
#define SATA_SIG_SEMB 0xC33C0101u
#define SATA_SIG_PM 0x96690101u

// ── ATA commands used by this driver ────────────────────────────────────
#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_READ_DMA_EX 0x25  // READ DMA EXT (48-bit LBA)
#define ATA_CMD_WRITE_DMA_EX 0x35 // WRITE DMA EXT (48-bit LBA)

// Result of probing the AHCI controller and its ports at boot.
// ahci_init() fills this in; callers (axfs_disk.c's future real-disk
// backend) check `ready` before calling ahci_read_sectors().
typedef struct
{
    bool ready;            // true if a usable SATA drive was found and initialized
    uint32_t port_index;   // which HBA port the drive is on
    uint64_t sector_count; // total addressable 512-byte sectors (from IDENTIFY)
} ahci_drive_info_t;

// Finds the AHCI controller via PCI, maps its MMIO registers, and
// initializes the first port with a real SATA drive attached (signature
// SATA_SIG_ATA, link active). Returns false (ahci_drive_info() will
// report ready=false) if no AHCI controller or no usable drive is found
// -- callers should keep working off the RAM disk in that case, not
// treat this as fatal.
bool ahci_init(void);

// Returns the result of the most recent ahci_init() call.
ahci_drive_info_t ahci_drive_info(void);

// Reads `count` sectors (512 bytes each) starting at `lba` into `buf`.
// `buf` must be at least count*512 bytes. Returns false on any error
// (drive not ready, HBA reported a task file error, command timeout).
// Blocking -- polls the command-issue register rather than waiting for
// an interrupt (see the file-level comment in ahci.c for why V1 does
// it this way).
bool ahci_read_sectors(uint64_t lba, uint32_t count, void *buf);

// Writes `count` sectors (512 bytes each) from `buf` starting at
// `lba`. `buf` must be at least count*512 bytes. Same error/blocking
// behavior as ahci_read_sectors(). Callers are responsible for not
// writing past the drive's actual size (see ahci_drive_info()'s
// sector_count) -- this doesn't check.
bool ahci_write_sectors(uint64_t lba, uint32_t count, const void *buf);

// Raw port register snapshot -- for diagnosing a command failure from
// the shell without needing GDB. Not meant for normal driver logic.
typedef struct
{
    uint32_t tfd;
    uint32_t is;
    uint32_t serr;
    uint32_t ci;
    uint32_t sact;
    uint32_t cmd;
} ahci_port_debug_t;

// Snapshots the current active drive's port registers. All fields are
// zero if no drive is ready.
ahci_port_debug_t ahci_port_debug_info(void);

#endif // APEX_AHCI_H