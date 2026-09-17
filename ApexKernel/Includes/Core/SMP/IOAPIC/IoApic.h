// ApexKernel/Includes/Core/SMP/IOAPIC/IoApic.h
// ApexOS -- I/O APIC bring-up (final piece of the SMP roadmap: legacy
// PIC 8259 IRQ routing replacement)
//
// Everything before this point in the project's SMP work (Local
// APIC, IPI, per-CPU, the scheduler) left the legacy PIC (Core/
// Interrupts/PIC/) completely untouched -- every hardware IRQ
// (keyboard, mouse, disk, network, the PIT timer) still only ever
// lands on the BSP, because the PIC has no concept of "which CPU"
// at all: it always signals whichever CPU asserts INTR, and on this
// kernel's boot path that has always been the BSP by convention, not
// by any explicit routing decision. The I/O APIC is what actually
// lets an IRQ be steered to a SPECIFIC CPU (or left on the BSP, by
// choice rather than by the PIC's own limitation) -- each of its 24
// redirection table entries independently specifies both a vector and
// a destination APIC ID.
//
// Unlike the Local APIC (Core/SMP/APIC/Lapic.h), whose registers are
// memory-mapped directly (read/write the MMIO address itself), the
// I/O APIC uses INDIRECT register access: two registers at fixed
// offsets within its own small MMIO window -- IOREGSEL (write the
// index of the real register you want) and IOWIN (then read/write
// through this one to actually touch it). See ioapic_read()/
// ioapic_write() in IoApic.c.

#ifndef APEX_CORE_SMP_IOAPIC_IOAPIC_H
#define APEX_CORE_SMP_IOAPIC_IOAPIC_H

#include "Core/Types.h"

// Byte offsets within the I/O APIC's MMIO window (Intel's I/O APIC
// spec) -- both fixed regardless of which specific redirection table
// register is being accessed; IOREGSEL selects the target register
// index, IOWIN reads/writes it.
#define IOAPIC_REG_IOREGSEL 0x00
#define IOAPIC_REG_IOWIN 0x10

// Indirect register indices (written to IOREGSEL, then read/written
// via IOWIN) -- IOAPICID/IOAPICVER are single 32-bit registers;
// IOREDTBL(n) is the FIRST of a PAIR of 32-bit registers for
// redirection table entry n (0-23) -- entry n's low dword is at
// IOAPICREDTBL(n), its high dword at IOAPICREDTBL(n)+1, see
// ioapic_set_redirection() in IoApic.c for why both halves are always
// written together.
#define IOAPIC_REG_ID 0x00
#define IOAPIC_REG_VERSION 0x01
#define IOAPIC_REG_REDTBL(n) (0x10 + (n) * 2)

// One redirection table entry's full 64 bits, split as two 32-bit
// halves purely because that's how the hardware register pair is
// laid out (see IOAPIC_REG_REDTBL above) -- ioapic_set_redirection()
// assembles/disassembles these from the individual fields below
// rather than exposing this struct directly, since getting the bit
// positions right by hand at every call site would be exactly the
// kind of error this header's named constants exist to prevent.
//
// Fields (Intel I/O APIC spec):
//   vector          bits 0-7:   the IDT vector this IRQ delivers to
//   delivery_mode   bits 8-10:  0b000 = Fixed (deliver to `vector` on `dest`) -- the only mode this kernel uses; others (NMI, SMI, ExtINT, INIT) are for chipset-specific/legacy uses this kernel has no need for
//   dest_mode       bit 11:     0 = Physical (dest is a real APIC ID) -- this kernel always uses physical mode, never logical (multicast-style) destinations
//   pin_polarity    bit 13:     0 = active-high, 1 = active-low -- from the ISA bus default (active-high) unless a Type 2 MADT override says otherwise (Core/Acpi/Madt.h's madt_interrupt_source_override_t.flags)
//   trigger_mode    bit 15:     0 = edge-triggered, 1 = level-triggered -- ISA default is edge; again, an override can change this per IRQ
//   mask            bit 16:     1 = this IRQ is disabled (masked) -- the I/O APIC's own per-IRQ equivalent of Core/Interrupts/PIC/PICControl.c's pic_mask_irq(), except per-GSI here rather than per-PIC-cascade-position
//   dest            bits 56-63: target APIC ID (physical mode) -- which CPU this IRQ is actually routed to, the entire point of this kernel doing this work at all
typedef struct
{
    uint8_t vector;
    uint8_t delivery_mode;
    uint8_t dest_mode;
    uint8_t pin_polarity;
    uint8_t trigger_mode;
    bool masked;
    uint8_t dest_apic_id;
} ioapic_redirection_t;

#define IOAPIC_DELIVERY_FIXED 0
#define IOAPIC_DEST_PHYSICAL 0
#define IOAPIC_POLARITY_ACTIVE_HIGH 0
#define IOAPIC_POLARITY_ACTIVE_LOW 1
#define IOAPIC_TRIGGER_EDGE 0
#define IOAPIC_TRIGGER_LEVEL 1

// Maps I/O APIC vectors 0x30-0x3F (16 GSIs -- more than enough for
// every legacy ISA IRQ 0-15 this kernel's drivers use, see
// Core/Interrupts/IRQ/IRQDispatch.c), deliberately placed just above
// IPI_VECTOR's neighborhood is NOT required -- chosen here simply to
// keep every non-exception, non-legacy-PIC-remap vector this kernel
// uses in one contiguous, easy-to-audit block, clear of both the
// original PIC1_OFFSET/PIC2_OFFSET range (0x20-0x2F, Core/Interrupts/
// PIC/PICSetup.h -- kept defined even after ioapic_init() below masks
// the PIC entirely, see that function's own comment on why) and
// IPI_VECTOR (0xF0, Core/SMP/IPI/Ipi.h).
#define IOAPIC_VECTOR_BASE 0x30

// Identity-maps the first I/O APIC's MMIO window (g_madt_info's own
// io_apic_addrs[0], see Core/Acpi/Madt.h) uncacheable, same pattern as
// Core/SMP/APIC/Lapic.h's lapic_init(). No-op with a warning if
// g_madt_info.valid is false or io_apic_count == 0 -- same single-
// I/O-APIC-required fallback every other MADT-dependent piece of this
// kernel's SMP work already follows. Does NOT program any redirection
// entries or touch the legacy PIC itself -- see ioapic_route_irq()
// below for that, and Core/Interrupts/PIC/PICSetup.h's own comment
// (added alongside this file) on why the PIC is disabled separately,
// not by this function.
void ioapic_init(void);

// Routes legacy ISA IRQ `irq` (0-15) to `dest_apic_id`, vectored at
// IOAPIC_VECTOR_BASE + irq, unmasked, Fixed delivery. Automatically
// resolves `irq` through g_madt_info.irq_to_gsi[] first (Core/Acpi/
// Madt.h) -- callers always think in terms of the IRQ NUMBER a driver
// already knows (e.g. IRQ1 for keyboard, unchanged from the PIC-based
// convention every existing driver call site already uses), never in
// terms of the underlying GSI/redirection-table-index directly, so a
// machine that overrides IRQ0 to GSI 2 (confirmed on QEMU via -d int
// logging during this feature's own development) is handled
// transparently -- ioapic_route_irq(0, ...) still means "the PIT
// timer", regardless of which redirection table entry that actually
// programs underneath.
//
// Polarity/trigger mode are also resolved from any matching Type 2
// override (defaulting to ISA's own active-high/edge-triggered when
// none exists) -- getting these wrong for an overridden IRQ (level-
// triggered PCI IRQs being the most common real-world case,
// unrelated to but structurally similar to the IRQ0 case this kernel
// has actually observed) can cause an interrupt storm or a missed
// interrupt depending on which way the mismatch goes, so this is not
// optional/cosmetic even though defaulting to ISA behavior would
// often happen to still work.
void ioapic_route_irq(uint8_t irq, uint8_t dest_apic_id);

// Masks (disables) legacy IRQ `irq` at the I/O APIC level -- the
// per-GSI equivalent of Core/Interrupts/PIC/PICControl.c's
// pic_mask_irq(), used by the same call sites once the PIC itself is
// no longer what's routing this IRQ (see Formats/AXF/AXP/
// ProcessLaunch.c's IRQ0 mask/unmask around Ring 3 execution, which
// this function's own introduction is what lets that code keep
// working once the PIC is disabled).
void ioapic_mask_irq(uint8_t irq);
void ioapic_unmask_irq(uint8_t irq);

#endif // APEX_CORE_SMP_IOAPIC_IOAPIC_H
