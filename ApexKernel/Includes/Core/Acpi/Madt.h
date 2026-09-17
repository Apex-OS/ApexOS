// ApexKernel/Includes/Core/Acpi/Madt.h
// ApexOS -- ACPI MADT (Multiple APIC Description Table) parsing and
// SMP CPU discovery
//
// MADT is ACPI's inventory of interrupt-related hardware: every
// Local APIC (one per logical CPU -- what Core/SMP/Startup needs to
// know how many APs to wake and what to send their INIT-SIPI-SIPI
// to) and every I/O APIC (not used by this kernel yet -- still on
// PIC-only interrupt routing, see Core/Interrupts/PIC/ -- but
// recorded now since discovering it later would mean re-parsing MADT
// a second time for no reason). This file is the top-level entry
// point for all of Core/Acpi/: madt_discover_cpus() below is the only
// function anything outside this directory needs to call.

#ifndef APEX_CORE_ACPI_MADT_H
#define APEX_CORE_ACPI_MADT_H

#include "Core/Acpi/Rsdt.h"

#define MADT_SIGNATURE "APIC"

// MADT's own fixed header, immediately after the common
// acpi_sdt_header_t (see Rsdt.h) -- the variable-length entry list
// (madt_entry_header_t, one after another) starts right after this.
typedef struct __attribute__((packed))
{
    acpi_sdt_header_t header;
    uint32_t local_apic_addr; // Physical address of the Local APIC MMIO region -- same for every CPU, see Core/SMP/APIC
    uint32_t flags;
} madt_t;

// Every MADT entry (whatever its type) starts with this 2-byte header
// -- type + this entry's own length, which is what lets
// madt_discover_cpus() walk the list without needing to understand
// every entry type, only the ones it actually cares about (0 and 1
// below; anything else is skipped using this length field alone).
typedef struct __attribute__((packed))
{
    uint8_t type;
    uint8_t length;
} madt_entry_header_t;

#define MADT_ENTRY_LOCAL_APIC 0
#define MADT_ENTRY_IO_APIC 1
#define MADT_ENTRY_INTERRUPT_SOURCE_OVERRIDE 2

// Type 0 -- one per logical CPU. processor_id is ACPI's own numbering
// (not necessarily 0-based or contiguous); apic_id is what actually
// matters for SMP -- it's the value Core/SMP/Startup's INIT-SIPI-SIPI
// sequence targets to wake that specific core, and what
// Core/SMP/PerCPU indexes per-CPU structures by.
typedef struct __attribute__((packed))
{
    madt_entry_header_t header;
    uint8_t processor_id;
    uint8_t apic_id;
    uint32_t flags; // bit 0: enabled -- see MADT_LOCAL_APIC_ENABLED below
} madt_local_apic_t;

// If this bit isn't set, the entry describes a CPU socket the
// firmware knows about but that isn't actually usable (an empty
// socket on a multi-socket board, or a core the firmware/BIOS has
// deliberately disabled) -- madt_discover_cpus() skips any entry
// without it, since attempting to wake a disabled/absent CPU with
// INIT-SIPI-SIPI has no defined behavior.
#define MADT_LOCAL_APIC_ENABLED 0x1

// Type 1 -- one per I/O APIC chip. Not used by this kernel's
// interrupt routing yet (still PIC-only, see Core/Interrupts/PIC/),
// recorded here purely so a future switch to I/O APIC routing doesn't
// need to re-parse MADT from scratch.
typedef struct __attribute__((packed))
{
    madt_entry_header_t header;
    uint8_t io_apic_id;
    uint8_t reserved;
    uint32_t io_apic_addr;    // Physical address of this I/O APIC's MMIO region
    uint32_t global_irq_base; // First global system interrupt this I/O APIC handles
} madt_io_apic_t;

// Type 2 -- one per legacy ISA IRQ that isn't identity-mapped to the
// same-numbered Global System Interrupt on THIS machine's I/O APIC.
// Firmware publishes one of these specifically to tell an OS "IRQ N
// isn't wired to I/O APIC input N the way you might assume -- route
// it to global_system_interrupt instead". The most common real-world
// case (and the reason this entry type exists in this kernel at all,
// rather than just assuming IRQ == GSI everywhere): IRQ0 (the PIT
// timer) is very frequently overridden to GSI 2 on real chipsets and
// on QEMU alike, because I/O APIC input 0 is conventionally reserved
// for a different purpose on many boards. Core/SMP/IOAPIC/ MUST check
// this table before programming any legacy IRQ's redirection entry --
// assuming identity mapping would silently route to the wrong I/O
// APIC input on any machine that publishes an override for that IRQ.
typedef struct __attribute__((packed))
{
    madt_entry_header_t header;
    uint8_t bus_source;               // Always 0 (ISA) in practice -- no other bus type is defined by the spec for this entry
    uint8_t irq_source;               // The legacy IRQ number (0-15) being overridden
    uint32_t global_system_interrupt; // The actual I/O APIC input this IRQ should be routed to instead
    uint16_t flags;                   // Polarity/trigger mode -- bits 0-1 polarity, bits 2-3 trigger mode (0 = bus default in both fields)
} madt_interrupt_source_override_t;

// Result of a full RSDP -> RSDT/XSDT -> MADT walk. Fixed-size arrays
// rather than a dynamically allocated list -- this runs before
// heap_init() in kmain() (see Init.c's ordering: ACPI discovery
// belongs right after vmm_init(), same reasoning DiskLayout/AcpiInfo
// give for why their own kernel-side consumers run early), so
// anything requiring kmalloc() isn't available yet. 32 CPUs is a
// generous ceiling for what this kernel/QEMU testing setup will
// realistically ever see; MADT_MAX_IO_APICS at 8 similarly generous
// for I/O APIC count (real systems typically have 1-2).
#define MADT_MAX_CPUS 32
#define MADT_MAX_IO_APICS 8

// Sentinel meaning "no override published for this legacy IRQ" in
// irq_to_gsi[] below -- madt_info_t is zero-initialized (zero_info(),
// Madt.c) before any parsing happens, so 0 itself can't be used as
// the sentinel (a genuine override to GSI 0 is legal, however
// unusual). 0xFFFFFFFF can never collide with a real GSI: the ACPI
// spec caps meaningful GSI values well below the range a uint32_t can
// represent, and no real firmware publishes anything close to this
// value.
#define MADT_NO_OVERRIDE 0xFFFFFFFFu

typedef struct
{
    bool valid; // false if RSDP/RSDT/MADT couldn't be found or failed validation anywhere along the chain

    uint32_t local_apic_addr; // From madt_t.local_apic_addr -- see Core/SMP/APIC

    uint8_t cpu_count;
    uint8_t cpu_apic_ids[MADT_MAX_CPUS]; // Only enabled CPUs (see MADT_LOCAL_APIC_ENABLED) -- index 0 is the boot processor by convention, not guaranteed by ACPI itself (see Madt.c)

    uint8_t io_apic_count;
    uint32_t io_apic_addrs[MADT_MAX_IO_APICS];

    // Indexed by legacy IRQ number (0-15) -- irq_to_gsi[N] is the
    // Global System Interrupt (I/O APIC redirection table index) IRQ N
    // should actually be routed to, from a Type 2 Interrupt Source
    // Override entry (see madt_interrupt_source_override_t above).
    // MADT_NO_OVERRIDE means no override was published for that IRQ --
    // callers (Core/SMP/IOAPIC, not yet implemented) treat that as
    // "GSI == IRQ number", the identity mapping the vast majority of
    // IRQs use in practice (overrides exist specifically as
    // exceptions to that default, not as the general case).
    uint32_t irq_to_gsi[16];
} madt_info_t;

// Runs the full ACPI discovery chain and fills `out`. Tries the
// kernel-known RSDP first (Core/Boot/AcpiInfo.h -- either what the
// UEFI bootloader found via the firmware configuration table, or
// left unset on a BIOS boot), falling back to Rsdp.h's BIOS memory
// scan if that's unset. On any failure along the chain (no RSDP
// found by either method, RSDT/XSDT checksum invalid, no MADT table
// published), `out->valid` is left false and every other field is
// zeroed -- callers (Core/SMP/Startup) treat that as "single-CPU
// system, don't attempt SMP bring-up", the same safe degraded
// fallback AcpiInfo.h's header comment describes.
void madt_discover_cpus(madt_info_t *out);

// Global result of Init.c's madt_discover_cpus() call, made available
// here rather than only as a local in kmain() -- Core/SMP/Startup
// (AP wakeup, not yet implemented) needs the same cpu_apic_ids list
// discovered once at boot, and re-running the whole RSDP/RSDT/MADT
// walk a second time just to get it again would be redundant work
// for an answer that can't change at runtime.
extern madt_info_t g_madt_info;

#endif // APEX_CORE_ACPI_MADT_H
