// ApexKernel/Core/Acpi/Madt.c
// ApexOS -- ACPI MADT parsing / SMP CPU discovery (see Madt.h)

#include "Core/Acpi/Madt.h"
#include "Core/Acpi/Rsdp.h"
#include "Core/Boot/AcpiInfo.h"
#include "Core/Boot/Log/BootLog.h"

madt_info_t g_madt_info;

static void zero_info(madt_info_t *out)
{
    out->valid = false;
    out->local_apic_addr = 0;
    out->cpu_count = 0;
    out->io_apic_count = 0;
    for (int i = 0; i < MADT_MAX_CPUS; i++)
        out->cpu_apic_ids[i] = 0;
    for (int i = 0; i < MADT_MAX_IO_APICS; i++)
        out->io_apic_addrs[i] = 0;
    for (int i = 0; i < 16; i++)
        out->irq_to_gsi[i] = MADT_NO_OVERRIDE;
}

// Resolves the RSDP's physical address, trying the kernel-known
// value first. See Madt.h's madt_discover_cpus() comment for why this
// order: AcpiInfo.h's acpi_info_t is authoritative when present (the
// UEFI bootloader already did the one correct lookup method for that
// boot path), and Rsdp.c's scan is only reached at all on a BIOS
// boot, where nothing else could have filled acpi_info_t in.
static void *resolve_rsdp(void)
{
    acpi_info_t *boot_info = (acpi_info_t *)(uint64_t)ACPI_INFO_ADDR;
    if (boot_info->valid && boot_info->rsdp_addr != 0)
        return (void *)boot_info->rsdp_addr;

    return rsdp_find();
}

// Walks every MADT entry from just after madt_t's fixed header to
// madt_t.header.length bytes from the table's start, dispatching on
// each entry's type. Unknown entry types (Local APIC NMI and several
// others this kernel doesn't need) are silently skipped via
// madt_entry_header_t.length alone -- this loop never needs to know
// their internal layout to step past them correctly. Types 0, 1, and
// 2 (Local APIC, I/O APIC, Interrupt Source Override) are the only
// ones this kernel's SMP/IOAPIC work actually needs.
static void walk_entries(madt_t *madt, madt_info_t *out)
{
    uint8_t *pos = (uint8_t *)madt + sizeof(madt_t);
    uint8_t *end = (uint8_t *)madt + madt->header.length;

    while (pos < end)
    {
        madt_entry_header_t *entry_hdr = (madt_entry_header_t *)pos;

        // A zero-length entry would spin this loop forever -- not
        // expected from a spec-conformant table, but a corrupt/
        // malicious MADT shouldn't be able to hang boot over it.
        if (entry_hdr->length == 0)
            break;

        if (entry_hdr->type == MADT_ENTRY_LOCAL_APIC && out->cpu_count < MADT_MAX_CPUS)
        {
            madt_local_apic_t *lapic = (madt_local_apic_t *)pos;
            if (lapic->flags & MADT_LOCAL_APIC_ENABLED)
            {
                out->cpu_apic_ids[out->cpu_count] = lapic->apic_id;
                out->cpu_count++;
            }
        }
        else if (entry_hdr->type == MADT_ENTRY_IO_APIC && out->io_apic_count < MADT_MAX_IO_APICS)
        {
            madt_io_apic_t *ioapic = (madt_io_apic_t *)pos;
            out->io_apic_addrs[out->io_apic_count] = ioapic->io_apic_addr;
            out->io_apic_count++;
        }
        else if (entry_hdr->type == MADT_ENTRY_INTERRUPT_SOURCE_OVERRIDE)
        {
            madt_interrupt_source_override_t *iso = (madt_interrupt_source_override_t *)pos;
            // irq_source is only ever 0-15 (a legacy ISA IRQ number,
            // see the struct's own comment) -- this bounds check is
            // defensive against a malformed table rather than
            // something expected to ever actually trigger.
            if (iso->irq_source < 16)
                out->irq_to_gsi[iso->irq_source] = iso->global_system_interrupt;
        }

        pos += entry_hdr->length;
    }
}

void madt_discover_cpus(madt_info_t *out)
{
    zero_info(out);

    void *rsdp_ptr = resolve_rsdp();
    if (!rsdp_ptr)
    {
        boot_log_warn("ACPI: no RSDP found, assuming single-CPU system");
        return;
    }
    boot_log_start();
    boot_log_str("ACPI: RSDP found at ");
    boot_log_hex((uint64_t)rsdp_ptr);
    boot_log_end_ok();

    rsdp_v1_t *rsdp = (rsdp_v1_t *)rsdp_ptr;

    // Prefer the XSDT (64-bit table pointers, no practical size
    // limitation) whenever the RSDP is new enough to carry one --
    // see Rsdt.h's acpi_find_table() comment on why this preference
    // exists. A v2 RSDP with xsdt_addr == 0 does happen on some real
    // firmware despite claiming revision 2 (spec-compliant XSDT
    // support isn't universal even among firmware that bumped the
    // revision field for other reasons) -- falls back to the v1
    // rsdt_addr in that case rather than trusting revision alone.
    uint64_t table_addr;
    bool is_xsdt;
    if (rsdp->revision >= 2 && ((rsdp_v2_t *)rsdp)->xsdt_addr != 0)
    {
        table_addr = ((rsdp_v2_t *)rsdp)->xsdt_addr;
        is_xsdt = true;
    }
    else
    {
        table_addr = rsdp->rsdt_addr;
        is_xsdt = false;
    }
    boot_log_start();
    boot_log_str("ACPI: using ");
    boot_log_str(is_xsdt ? "XSDT" : "RSDT");
    boot_log_str(" at ");
    boot_log_hex(table_addr);
    boot_log_end_ok();

    madt_t *madt = (madt_t *)acpi_find_table(table_addr, is_xsdt, MADT_SIGNATURE);
    if (!madt)
    {
        boot_log_warn("ACPI: no MADT table found, assuming single-CPU system");
        return;
    }
    boot_log_start();
    boot_log_str("ACPI: MADT found at ");
    boot_log_hex((uint64_t)madt);
    boot_log_str(", local APIC at ");
    boot_log_hex(madt->local_apic_addr);
    boot_log_end_ok();

    out->local_apic_addr = madt->local_apic_addr;
    walk_entries(madt, out);
    out->valid = true;

    for (uint32_t i = 0; i < out->cpu_count; i++)
    {
        boot_log_start();
        boot_log_str("ACPI: CPU ");
        boot_log_dec(i);
        boot_log_str(", APIC ID ");
        boot_log_dec(out->cpu_apic_ids[i]);
        boot_log_end_ok();
    }
    for (uint32_t i = 0; i < out->io_apic_count; i++)
    {
        boot_log_start();
        boot_log_str("ACPI: I/O APIC ");
        boot_log_dec(i);
        boot_log_str(" at ");
        boot_log_hex(out->io_apic_addrs[i]);
        boot_log_end_ok();
    }
    for (uint32_t irq = 0; irq < 16; irq++)
    {
        if (out->irq_to_gsi[irq] == MADT_NO_OVERRIDE)
            continue;
        boot_log_start();
        boot_log_str("ACPI: IRQ ");
        boot_log_dec(irq);
        boot_log_str(" overridden to GSI ");
        boot_log_dec(out->irq_to_gsi[irq]);
        boot_log_end_ok();
    }
    boot_log_start();
    boot_log_str("ACPI: ");
    boot_log_dec(out->cpu_count);
    boot_log_str(" CPU(s), ");
    boot_log_dec(out->io_apic_count);
    boot_log_str(" I/O APIC(s) detected");
    boot_log_end_ok();
}
