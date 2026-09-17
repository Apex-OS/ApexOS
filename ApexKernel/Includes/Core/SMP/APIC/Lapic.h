// ApexKernel/Includes/Core/SMP/APIC/Lapic.h
// ApexOS -- Local APIC (per-CPU interrupt controller) bring-up
//
// Every CPU core has its own Local APIC, but they all sit at the same
// physical MMIO address (g_madt_info.local_apic_addr, see
// Core/Acpi/Madt.h -- discovered from the MADT, typically 0xFEE00000
// on real hardware and QEMU alike) -- reading/writing that address
// always talks to *this* core's own LAPIC, never another core's, by
// virtue of how the CPU routes it in hardware. That's what makes
// lapic_init() below safe to eventually call identically from every
// AP during SMP bring-up (Core/SMP/Startup, not yet implemented) as
// well as from the boot processor here in Init.c: same code, same
// address, each core still only ever touches its own LAPIC.
//
// This step (Phase 2 of the project's SMP roadmap) only brings the
// boot processor's own LAPIC up and leaves the legacy PIC
// (Core/Interrupts/PIC/) exactly as-is, still routing every IRQ
// exactly like before -- switching IRQ routing away from the PIC to
// the I/O APIC (g_madt_info.io_apic_addrs[0]) is separate work for
// later, only possible once this succeeds. The two controllers don't
// conflict merely by both being enabled: nothing routes an IRQ
// through the LAPIC's own I/O redirection yet, so PIC-delivered
// interrupts keep arriving exactly as they always have.

#ifndef APEX_CORE_SMP_APIC_LAPIC_H
#define APEX_CORE_SMP_APIC_LAPIC_H

#include "Core/Types.h"

// Register byte offsets within the LAPIC's 4KB MMIO window (Intel SDM
// Vol. 3A, Table 10-1) -- every register here is 32 bits wide but
// individually 128-byte aligned (the SDM reserves the rest of each
// 16-byte-aligned slot), which is why these are spaced out rather
// than packed -- see lapic_read()/lapic_write() in Lapic.c, which
// divide by 4 like E1000.c's reg_read()/reg_write() to get a
// uint32_t-array index from one of these byte offsets.
#define LAPIC_REG_ID 0x020       // This core's own APIC ID (read-only, top 8 bits of the 32-bit value)
#define LAPIC_REG_SPURIOUS 0x0F0 // Spurious Interrupt Vector Register -- see LAPIC_SPURIOUS_ENABLE below
#define LAPIC_REG_EOI 0x0B0      // End Of Interrupt -- any write (value ignored) acknowledges the current interrupt
#define LAPIC_REG_ICR_LOW 0x300  // Interrupt Command Register, bits 0-31 -- see lapic_send_ipi()
#define LAPIC_REG_ICR_HIGH 0x310 // Interrupt Command Register, bits 32-63 -- destination APIC ID (bits 24-31 of this register)

// Bit 8 of the Spurious Interrupt Vector Register -- the LAPIC's own
// master enable. Unset (the power-on default), the LAPIC does not
// deliver ANY interrupt to this core regardless of anything else
// being configured, which is why this is the first bit lapic_init()
// touches. The low 8 bits of the same register additionally set the
// vector number spurious interrupts (a hardware corner case, not a
// real interrupt source) are delivered on -- set to 0xFF here
// (LAPIC_SPURIOUS_VECTOR) since Intel's own recommendation is to keep
// the low 4 bits set to avoid colliding with a real, lower-numbered
// vector some older APIC implementations reserve.
#define LAPIC_SPURIOUS_ENABLE (1u << 8)
#define LAPIC_SPURIOUS_VECTOR 0xFF

// Enables this core's Local APIC (MSR-level enable, then the
// spurious-vector register's own software enable bit) and identity-
// maps its MMIO window uncacheable, the same way E1000.c/AHCI's
// PortMemory.c map their own device registers. No-op (returns
// immediately) if g_madt_info.valid is false -- see Madt.h's own
// comment on that fallback: no MADT means this machine's LAPIC
// address is unknown, so there's nothing safe to map or write to.
void lapic_init(void);

// Returns this core's own APIC ID by reading LAPIC_REG_ID directly
// from hardware -- distinct from g_madt_info.cpu_apic_ids[0], which
// is only ACPI's static *inventory* of every CPU the firmware knows
// about, not "which one is currently running this code". They happen
// to agree for the boot processor (by convention, though not a
// guarantee ACPI itself makes -- see Madt.h), but once APs exist
// (Core/SMP/Startup, not yet implemented) each core's own call to
// this function is the only way to know which one it actually is.
uint8_t lapic_get_id(void);

// Sends End-Of-Interrupt to this core's own LAPIC -- required at the
// end of any interrupt handler that was delivered via the LAPIC/I-O
// APIC path rather than the legacy PIC (which uses its own separate
// PIC_EOI, see Core/Interrupts/PIC/PICControl.c -- unrelated
// mechanism, unrelated register, do not confuse the two once I/O APIC
// routing exists). Unused by anything yet in this phase -- present
// now since Phase 5 (IPI) needs it and it's a two-line function that
// belongs next to the rest of this file's register access.
void lapic_send_eoi(void);

// ── Inter-Processor Interrupts (INIT/SIPI) ────────────────────────────────
// Delivery mode field (ICR bits 8-10) -- Intel SDM Vol. 3A, Table
// 10-6. Only these two are ever used by this kernel: the INIT-SIPI-
// SIPI sequence Core/SMP/Startup/AppProcessorStartup.c sends to wake
// each AP (see that file). Every other delivery mode (NMI, SMI,
// ExtINT, ...) targets interrupt-controller use cases this kernel
// never generates on its own.
#define LAPIC_ICR_DELIVERY_INIT (0b101u << 8)
#define LAPIC_ICR_DELIVERY_STARTUP (0b110u << 8)

// Level/trigger bits (ICR bits 14-15) -- an INIT IPI is conventionally
// sent as level-triggered assert-then-deassert (two separate writes,
// see AppProcessorStartup.c's ap_startup_wake_one()) per the original
// MP Specification; a SIPI is always edge-triggered (this bit simply
// doesn't apply to it, but the field's default state is the correct
// one either way so no separate SIPI-specific constant is needed
// here).
#define LAPIC_ICR_LEVEL_ASSERT (1u << 14)
#define LAPIC_ICR_TRIGGER_LEVEL (1u << 15)

// Destination Shorthand field (ICR bits 18-19) -- 0b00 (the default,
// no bits set here) means "use whatever APIC ID is written into
// LAPIC_REG_ICR_HIGH", which is what every call in this kernel wants:
// each AP is always targeted individually by its own specific APIC
// ID (from g_madt_info.cpu_apic_ids[], see Madt.h), never as a
// broadcast to "all but self" or "all including self" (the other two
// meaningful shorthand values) -- no code path here ever needs to
// interrupt every CPU at once.

// Writes one IPI to the ICR and sends it to `dest_apic_id`.
// `command_bits` is delivery mode + level/trigger bits (the
// LAPIC_ICR_* constants above, OR'd together as needed) plus, for a
// SIPI specifically, the target vector in its low 8 bits (Intel SDM
// Vol. 3A, 8.4.4 -- see AppProcessorStartup.h's own comment on why
// that vector directly encodes the trampoline's physical address).
// Writing LAPIC_REG_ICR_LOW is literally what sends the IPI (Intel
// SDM: writing the low dword is defined to trigger delivery), so
// LAPIC_REG_ICR_HIGH -- the destination -- must always be written
// first, or the IPI goes out before its destination is set.
void lapic_send_ipi(uint8_t dest_apic_id, uint32_t command_bits);

// Spins reading LAPIC_REG_ICR_LOW's Delivery Status bit (bit 12,
// Intel SDM Vol. 3A Table 10-6) until it clears, meaning the LAPIC
// hardware has finished sending the most recent IPI written via
// lapic_send_ipi() above. AppProcessorStartup.c's ap_startup_wake_one()
// calls this after every single IPI it sends (INIT-assert, INIT-
// deassert, each SIPI) before sending the next one or proceeding --
// sending a second IPI while the first is still pending is undefined
// per the SDM, not merely redundant.
void lapic_wait_ipi_sent(void);

#endif // APEX_CORE_SMP_APIC_LAPIC_H
