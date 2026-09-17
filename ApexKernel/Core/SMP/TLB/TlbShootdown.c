// ApexKernel/Core/SMP/TLB/TlbShootdown.c
// ApexOS -- Cross-CPU TLB invalidation (see TlbShootdown.h)

#include "Core/SMP/TLB/TlbShootdown.h"
#include "Core/SMP/APIC/Lapic.h"
#include "Core/SMP/PerCPU/PerCpu.h"
#include "Core/Acpi/Madt.h"

static tlb_shootdown_request_t g_shootdown_request;

void tlb_shootdown(uint64_t virt_addr)
{
    if (!g_madt_info.valid || g_madt_info.cpu_count <= 1)
        return; // No other CPU exists that could hold a stale TLB entry -- see this function's own header comment.

    uint32_t this_cpu = percpu_get()->cpu_index;

    // Counts how many OTHER CPUs are actually online (percpu_t.online,
    // Core/SMP/PerCPU/PerCpu.h) to know how many acks to wait for --
    // NOT simply g_madt_info.cpu_count - 1, since an AP that was never
    // woken (or hasn't finished percpu_init_this_cpu() yet, extremely
    // early in boot) has no handler running to ever acknowledge this
    // request, and waiting for an ack that will never come would hang
    // forever.
    uint32_t target_count = 0;
    for (uint32_t i = 0; i < g_madt_info.cpu_count; i++)
    {
        if (i == this_cpu)
            continue;
        percpu_t *target = percpu_get_by_index(i);
        if (target && target->online)
            target_count++;
    }

    if (target_count == 0)
        return;

    g_shootdown_request.virt_addr = virt_addr;
    g_shootdown_request.acks_received = 0;

    for (uint32_t i = 0; i < g_madt_info.cpu_count; i++)
    {
        if (i == this_cpu)
            continue;
        percpu_t *target = percpu_get_by_index(i);
        if (!target || !target->online)
            continue;

        uint8_t dest_apic_id = g_madt_info.cpu_apic_ids[i];
        lapic_send_ipi(dest_apic_id, TLB_SHOOTDOWN_VECTOR);
        lapic_wait_ipi_sent();
    }

    // Spins until every targeted CPU has confirmed its own invlpg
    // completed -- see TlbShootdown.h's own comment on why this
    // synchronous wait matters: a caller proceeding to, say, free the
    // physical page that used to back virt_addr while another CPU
    // might still access it through a stale TLB entry would be a
    // genuine use-after-free race, not just a performance concern.
    // No formal timeout, same reasoning as every other cross-CPU
    // confirmation spin in this kernel (Core/SMP/Startup/
    // AppProcessorStartup.c's ap_ready, Init.c's various Phase
    // confirmations) -- a stuck spin here is a visible, debuggable
    // failure.
    while (g_shootdown_request.acks_received < target_count)
        __asm__ volatile("pause");
}

void tlb_shootdown_dispatch(void)
{
    uint64_t virt_addr = g_shootdown_request.virt_addr;
    __asm__ volatile("invlpg (%0)" ::"r"(virt_addr) : "memory");

    // Acknowledges AFTER the invlpg has actually executed, never
    // before -- tlb_shootdown()'s spin (above) treats this counter
    // reaching target_count as proof every CPU's TLB is already
    // clean, so acknowledging early would defeat the entire point of
    // waiting synchronously.
    __asm__ volatile("lock incl %0" : "+m"(g_shootdown_request.acks_received));

    lapic_send_eoi();
}
