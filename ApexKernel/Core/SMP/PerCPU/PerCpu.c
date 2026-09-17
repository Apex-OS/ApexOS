// ApexKernel/Core/SMP/PerCPU/PerCpu.c
// ApexOS -- Per-CPU data (see PerCpu.h)

#include "Core/SMP/PerCPU/PerCpu.h"
#include "Core/Acpi/Madt.h"
#include "Core/SMP/APIC/Lapic.h"
#include "Core/Memory/Heap/Heap.h"
#include "Logs/Log.h"

#define IA32_GS_BASE_MSR 0xC0000101

static percpu_t *g_percpu_array = NULL;
static uint32_t g_percpu_count = 0;
volatile bool g_percpu_ready = false;

static inline void wrmsr(uint32_t msr, uint64_t value)
{
    uint32_t lo = (uint32_t)value;
    uint32_t hi = (uint32_t)(value >> 32);
    __asm__ volatile("wrmsr" ::"c"(msr), "a"(lo), "d"(hi));
}

void percpu_init_array(void)
{
    // Falls back to a single slot (index 0, the BSP) if MADT
    // discovery failed or reported an implausible count -- same
    // single-CPU degraded philosophy as every other MADT-dependent
    // piece of this SMP work (see Madt.h). Init.c never calls
    // ap_startup_begin() in that same situation either, so no AP
    // would ever need a slot beyond this one regardless.
    g_percpu_count = (g_madt_info.valid && g_madt_info.cpu_count > 0) ? g_madt_info.cpu_count : 1;

    g_percpu_array = (percpu_t *)kcalloc(g_percpu_count, sizeof(percpu_t));
    if (!g_percpu_array)
    {
        klog_error("percpu_init_array: allocation failed");
        g_percpu_count = 0;
        return;
    }

    klog_ok_n("percpu_init_array: slots allocated", g_percpu_count);
}

void percpu_init_this_cpu(uint32_t cpu_index)
{
    if (!g_percpu_array || cpu_index >= g_percpu_count)
    {
        // No array (percpu_init_array() itself failed, logged above)
        // or an out-of-range index (would only happen from a caller
        // bug -- every real call site passes an index already bounds-
        // checked against g_madt_info.cpu_count, the same count this
        // array was sized from). Either way, GS_BASE is deliberately
        // left untouched rather than pointed at garbage: code that
        // later calls percpu_get() and dereferences the result would
        // rather crash immediately on a null/stale GS_BASE than
        // silently read/write memory that belongs to something else
        // entirely.
        klog_error_n("percpu_init_this_cpu: invalid array or cpu_index", cpu_index);
        return;
    }

    percpu_t *self = &g_percpu_array[cpu_index];
    wrmsr(IA32_GS_BASE_MSR, (uint64_t)self);

    self->cpu_index = cpu_index;
    self->apic_id = lapic_get_id();
    self->online = true;

    // Only the BSP's own call (cpu_index == 0) sets this -- see
    // PerCpu.h's own comment on g_percpu_ready: it exists specifically
    // to guard scheduler_tick_asm(), which only ever runs on the BSP
    // (IRQ0 is still exclusively PIC-routed there, see
    // Core/Interrupts/PIC/PICSetup.h). An AP's own call here has no
    // reason to touch this flag -- it's meaningless from an AP's
    // perspective either way, since scheduler_tick_asm() will never
    // execute on one regardless of this flag's state.
    if (cpu_index == 0)
        g_percpu_ready = true;
}

percpu_t *percpu_get(void)
{
    // Reads GS_BASE back out via rdmsr rather than `mov rax, gs:[0]`
    // -- this returns the base address itself (a pointer to the
    // percpu_t), whereas gs:[offset] addressing reads *through* that
    // base to get a specific field. Both are valid ways to use
    // GS_BASE; this one is simpler here since the whole point of this
    // function is handing back the pointer itself for the caller to
    // dereference however it needs.
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(IA32_GS_BASE_MSR));
    return (percpu_t *)(((uint64_t)hi << 32) | lo);
}

percpu_t *percpu_get_by_index(uint32_t cpu_index)
{
    if (!g_percpu_array || cpu_index >= g_percpu_count)
        return NULL;

    return &g_percpu_array[cpu_index];
}
