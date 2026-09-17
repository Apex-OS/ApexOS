// ApexKernel/Core/SMP/APIC/Lapic.c
// ApexOS -- Local APIC bring-up (see Lapic.h)

#include "Core/SMP/APIC/Lapic.h"
#include "Core/Acpi/Madt.h"
#include "Core/Memory/Physical/PageBitmap.h"
#include "Core/Memory/Virtual/PageTables.h"
#include "Core/Boot/Log/BootLog.h"

// IA32_APIC_BASE MSR (Intel SDM Vol. 3A, 10.4.4) -- the CPU-level
// switch for the entire APIC, one level above the spurious-vector
// register's own software enable bit. Bit 11 here must be set for the
// LAPIC to respond to its MMIO window at all; bits 12-35 hold the
// same physical base address the MADT itself reports (they're
// supposed to always agree -- this kernel trusts the MADT's copy
// rather than reading this MSR back, since it's already committed to
// MADT as the source of truth for every other SMP fact, see Madt.h).
#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_ENABLE (1ull << 11)

static volatile uint32_t *g_lapic_mmio = NULL;

static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t value)
{
    uint32_t lo = (uint32_t)value;
    uint32_t hi = (uint32_t)(value >> 32);
    __asm__ volatile("wrmsr" ::"c"(msr), "a"(lo), "d"(hi));
}

// Same offset-divided-by-4 pattern as E1000.c's reg_read()/
// reg_write() -- g_lapic_mmio is a uint32_t*, but every LAPIC_REG_*
// constant in Lapic.h is a byte offset per the SDM's own table.
static inline uint32_t lapic_read(uint32_t reg_offset)
{
    return g_lapic_mmio[reg_offset / 4];
}

static inline void lapic_write(uint32_t reg_offset, uint32_t value)
{
    g_lapic_mmio[reg_offset / 4] = value;
}

void lapic_init(void)
{
    if (!g_madt_info.valid)
    {
        boot_log_warn("lapic_init: no MADT info available, skipping LAPIC bring-up");
        return;
    }

    uint64_t lapic_phys = g_madt_info.local_apic_addr;

    // Identity-map the LAPIC's 4KB MMIO window uncacheable -- same
    // reasoning as any other MMIO device this kernel talks to
    // (E1000.c, AHCI's PortMemory.c): a cached read/write here would
    // let the CPU serve it from cache instead of actually reaching
    // the hardware register, silently breaking everything from here
    // on (stale LAPIC_REG_ID reads, an EOI that never reaches the
    // hardware, etc.).
    vmm_map(lapic_phys, lapic_phys, VMM_KERNEL_RW | VMM_NO_CACHE);
    g_lapic_mmio = (volatile uint32_t *)lapic_phys;

    // CPU-level enable first (the MSR) -- the spurious-vector
    // register's own software enable bit below has no effect while
    // this one is clear, so order matters here even though both are
    // logically "enable the LAPIC".
    uint64_t apic_base = rdmsr(IA32_APIC_BASE_MSR);
    wrmsr(IA32_APIC_BASE_MSR, apic_base | IA32_APIC_BASE_ENABLE);

    // Software enable + spurious vector, see Lapic.h's comment on
    // LAPIC_SPURIOUS_ENABLE/LAPIC_SPURIOUS_VECTOR for why both halves
    // of this one write matter.
    lapic_write(LAPIC_REG_SPURIOUS, LAPIC_SPURIOUS_ENABLE | LAPIC_SPURIOUS_VECTOR);

    boot_log_ok_n("lapic_init: Local APIC enabled, ID", lapic_get_id());
}

uint8_t lapic_get_id(void)
{
    // Bits 31:24 of LAPIC_REG_ID hold the ID -- see Intel SDM Vol. 3A,
    // Table 10-1 ("bits 24 through 31").
    return (uint8_t)(lapic_read(LAPIC_REG_ID) >> 24);
}

void lapic_send_eoi(void)
{
    lapic_write(LAPIC_REG_EOI, 0);
}

void lapic_send_ipi(uint8_t dest_apic_id, uint32_t command_bits)
{
    // Destination APIC ID lives in bits 24-31 of ICR_HIGH -- written
    // first, see Lapic.h's comment on why order matters here (writing
    // ICR_LOW is what actually triggers delivery).
    lapic_write(LAPIC_REG_ICR_HIGH, (uint32_t)dest_apic_id << 24);
    lapic_write(LAPIC_REG_ICR_LOW, command_bits);
}

void lapic_wait_ipi_sent(void)
{
    // Bit 12 of ICR_LOW -- Delivery Status, 1 while the IPI is still
    // pending send, cleared by hardware once it's actually gone out.
    // See Lapic.h's comment on lapic_wait_ipi_sent() for why every
    // caller spins here before sending anything else.
    while (lapic_read(LAPIC_REG_ICR_LOW) & (1u << 12))
        __asm__ volatile("pause");
}
