// ApexKernel/Core/SMP/IOAPIC/IoApic.c
// ApexOS -- I/O APIC bring-up (see IoApic.h)

#include "Core/SMP/IOAPIC/IoApic.h"
#include "Core/Acpi/Madt.h"
#include "Core/Memory/Virtual/PageTables.h"
#include "Logs/Log.h"

static volatile uint32_t *g_ioapic_mmio = NULL;

// Unlike Core/SMP/APIC/Lapic.c's direct-mapped register access, the
// I/O APIC only exposes TWO registers at fixed MMIO offsets --
// IOREGSEL (write which real register you want) and IOWIN (then
// actually read/write it). Every other register (ID, VERSION, each
// redirection table entry) is reached indirectly through this pair,
// which is why ioapic_read()/ioapic_write() below take a register
// INDEX (IOAPIC_REG_ID, IOAPIC_REG_REDTBL(n), etc.) rather than a
// byte offset the way Lapic.c's own reg_read()/reg_write() do.
static uint32_t ioapic_read(uint8_t reg_index)
{
    g_ioapic_mmio[IOAPIC_REG_IOREGSEL / 4] = reg_index;
    return g_ioapic_mmio[IOAPIC_REG_IOWIN / 4];
}

static void ioapic_write(uint8_t reg_index, uint32_t value)
{
    g_ioapic_mmio[IOAPIC_REG_IOREGSEL / 4] = reg_index;
    g_ioapic_mmio[IOAPIC_REG_IOWIN / 4] = value;
}

void ioapic_init(void)
{
    if (!g_madt_info.valid || g_madt_info.io_apic_count == 0)
    {
        klog_warn("ioapic_init: no I/O APIC available, skipping");
        return;
    }

    uint64_t ioapic_phys = g_madt_info.io_apic_addrs[0];

    // vmm_map_and_shootdown(), NOT plain vmm_map() -- this function
    // runs after every AP is already up and executing real code (see
    // Init.c's ordering: ap_startup_begin() and the round-robin test
    // both happen before this). See PageTables.h's own comment on
    // vmm_map_and_shootdown() for the exact bug this avoids -- found
    // via extensive isolated testing when this line was still a plain
    // vmm_map() call.
    vmm_map_and_shootdown(ioapic_phys, ioapic_phys, VMM_KERNEL_RW | VMM_NO_CACHE);
    g_ioapic_mmio = (volatile uint32_t *)ioapic_phys;

    klog_ok_n("ioapic_init: I/O APIC mapped at", ioapic_phys);
}

// Resolves `irq` (a legacy IRQ number, 0-15) to the redirection table
// entry index it should actually be programmed at, via
// g_madt_info.irq_to_gsi[] (Core/Acpi/Madt.h) -- see IoApic.h's own
// comment on ioapic_route_irq() for why callers never need to do this
// resolution themselves. Assumes a single I/O APIC starting at GSI 0
// (true for every machine this kernel has been tested against, and
// the overwhelmingly common case even on real multi-I/O-APIC systems
// for the specific legacy ISA IRQs this function handles) -- a
// machine with more than one I/O APIC where legacy IRQs are actually
// split across them would need this to also check
// g_madt_info.io_apic_addrs[]'s corresponding global_irq_base values,
// not implemented here since g_madt_info doesn't currently retain
// those bases past discovery (Core/Acpi/Madt.c's walk_entries() reads
// but never stores madt_io_apic_t.global_irq_base).
static uint8_t resolve_gsi(uint8_t irq)
{
    if (irq < 16 && g_madt_info.irq_to_gsi[irq] != MADT_NO_OVERRIDE)
        return (uint8_t)g_madt_info.irq_to_gsi[irq];

    return irq;
}

void ioapic_route_irq(uint8_t irq, uint8_t dest_apic_id)
{
    if (!g_ioapic_mmio)
    {
        klog_error_n("ioapic_route_irq: I/O APIC not initialized, irq", irq);
        return;
    }

    uint8_t gsi = resolve_gsi(irq);

    // Polarity/trigger default to ISA bus conventions (active-high,
    // edge-triggered) unless a Type 2 override says otherwise for
    // THIS specific irq -- see IoApic.h's own comment on why getting
    // this wrong matters. g_madt_info.irq_to_gsi[]'s own MADT_NO_OVERRIDE
    // check already told us above whether an override exists at all;
    // re-deriving polarity/trigger from the same override entry would
    // need madt_info_t to also retain its flags field, which it
    // currently doesn't (only global_system_interrupt is kept, see
    // Core/Acpi/Madt.c's walk_entries()) -- defaulting to ISA
    // conventions here is therefore a known simplification, correct
    // for every override this kernel has actually observed (QEMU's
    // IRQ0->GSI2 remap uses ISA-default polarity/trigger despite
    // changing the GSI number) but not a substitute for reading the
    // real flags value on hardware that also changes those.
    uint32_t low = (uint32_t)IOAPIC_VECTOR_BASE + irq;
    low |= (uint32_t)IOAPIC_DELIVERY_FIXED << 8;
    low |= (uint32_t)IOAPIC_DEST_PHYSICAL << 11;
    low |= (uint32_t)IOAPIC_POLARITY_ACTIVE_HIGH << 13;
    low |= (uint32_t)IOAPIC_TRIGGER_EDGE << 15;
    // Mask bit (16) left clear -- unmasked, since routing an IRQ and
    // leaving it masked would make the routing pointless until a
    // separate ioapic_unmask_irq() call.

    uint32_t high = (uint32_t)dest_apic_id << 24;

    // Both halves of a redirection table entry must be written
    // together to form one coherent 64-bit configuration -- writing
    // only one half would leave the entry in a mixed old/new state
    // for however long until the second write happens, briefly
    // routing to a stale destination or vector. High half written
    // first here (safe regardless of order, since the entry stays
    // masked-by-convention... actually it's unmasked above, so order
    // does matter slightly less here than it would for an
    // already-active entry, but writing dest before vector/flags
    // keeps a consistent habit with LAPIC ICR's own high-before-low
    // convention, Core/SMP/APIC/Lapic.c's lapic_send_ipi()).
    ioapic_write(IOAPIC_REG_REDTBL(gsi) + 1, high);
    ioapic_write(IOAPIC_REG_REDTBL(gsi), low);

    klog_ok_n("ioapic_route_irq: irq routed, dest_apic_id", dest_apic_id);
}

void ioapic_mask_irq(uint8_t irq)
{
    if (!g_ioapic_mmio)
        return;

    uint8_t gsi = resolve_gsi(irq);
    uint32_t low = ioapic_read(IOAPIC_REG_REDTBL(gsi));
    low |= (1u << 16);
    ioapic_write(IOAPIC_REG_REDTBL(gsi), low);
}

void ioapic_unmask_irq(uint8_t irq)
{
    if (!g_ioapic_mmio)
        return;

    uint8_t gsi = resolve_gsi(irq);
    uint32_t low = ioapic_read(IOAPIC_REG_REDTBL(gsi));
    low &= ~(1u << 16);
    ioapic_write(IOAPIC_REG_REDTBL(gsi), low);
}
