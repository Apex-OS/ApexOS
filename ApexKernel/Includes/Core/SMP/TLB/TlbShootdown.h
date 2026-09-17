// ApexKernel/Includes/Core/SMP/TLB/TlbShootdown.h
// ApexOS -- Cross-CPU TLB invalidation
//
// The bug this file fixes: Core/Memory/Virtual/PageTables.c's
// vmm_map()/vmm_unmap() only ever execute `invlpg` on the CPU that
// calls them -- every OTHER CPU's own TLB can keep a stale cached
// translation for that same virtual address indefinitely, since
// nothing tells it to drop it. This was completely invisible through
// every earlier phase of this project's SMP work, because nothing
// ever called vmm_map()/vmm_unmap() again once APs were already up
// and running real code -- Core/SMP/APIC/Lapic.h's lapic_init() and
// every other MMIO mapping so far happened once, early, from the BSP,
// before any AP existed to hold a stale TLB entry for anything an
// unrelated later mapping might disturb.
//
// This surfaced for the first time with Core/SMP/IOAPIC/IoApic.h's
// ioapic_init(), which runs AFTER APs are already executing real
// process code (the SMP round-robin scheduler test, Init.c) --
// confirmed via extensive isolated testing (a byte-identical
// hardcoded address mapped fine; the exact same value read from
// g_madt_info intermittently hung the round-robin test in a way that
// changed simply by adding a debug log line, the classic signature of
// a timing-dependent race rather than a deterministic logic bug) that
// mapping a NEW page table entry on the BSP, alongside vmm_get_or_create()
// allocating a fresh PD/PT page via pmm_alloc_page() for it, was
// leaving some AP's TLB holding a stale translation for a page table
// slot that had just changed shape underneath it.
//
// The fix: any vmm_map()/vmm_unmap() call made after APs are already
// running must be followed by telling every OTHER active CPU to flush
// its own TLB for that address -- this file's tlb_shootdown() does
// exactly that, via a dedicated IPI vector (deliberately NOT reusing
// Core/SMP/IPI/Ipi.h's IPI_VECTOR -- see TLB_SHOOTDOWN_VECTOR below
// for why a generic IPI's existing dispatch logic, built around
// "does this CPU have a process to run", is the wrong shape entirely
// for "invalidate this specific address").

#ifndef APEX_CORE_SMP_TLB_TLB_SHOOTDOWN_H
#define APEX_CORE_SMP_TLB_TLB_SHOOTDOWN_H

#include "Core/Types.h"

// A dedicated vector rather than overloading IPI_VECTOR (Core/SMP/
// IPI/Ipi.h) -- a TLB shootdown carries a specific piece of data (the
// virtual address to invalidate) that a generic IPI's dispatch
// (IpiDispatch.c's ipi_dispatch(), built around "check current_proc,
// maybe launch a process") has no way to carry or would need
// invasive changes to support safely. Chosen adjacent to IPI_VECTOR
// (0xF0) in the same otherwise-unused high vector range this kernel
// reserves for its own SMP coordination, clear of every exception
// (0-31), PIC-remapped IRQ (32-47), and I/O APIC-remapped IRQ
// (Core/SMP/IOAPIC/IoApic.h's IOAPIC_VECTOR_BASE=0x30 through 0x3F)
// vector.
#define TLB_SHOOTDOWN_VECTOR 0xF1

// Shared data the requesting CPU writes before sending the shootdown
// IPI, read by every receiving CPU's own handler
// (TlbShootdown.c's tlb_shootdown_dispatch()) -- a single global
// rather than per-target data because a shootdown is always broadcast
// to every OTHER active CPU at once (see tlb_shootdown() below), all
// reading the exact same address to invalidate.
typedef struct
{
    volatile uint64_t virt_addr;

    // Incremented by EACH receiving CPU's own handler once its own
    // invlpg has completed -- tlb_shootdown() spins on this reaching
    // the expected count before returning, the same "don't proceed
    // until every target actually did the work" philosophy every
    // other synchronous SMP operation in this kernel already follows
    // (Core/SMP/Startup/AppProcessorStartup.c's own ap_ready spin is
    // the original example this mirrors).
    volatile uint32_t acks_received;
} tlb_shootdown_request_t;

// Sends a TLB invalidation request for `virt_addr` to every OTHER
// currently-online CPU (Core/SMP/PerCPU/PerCpu.h's percpu_t.online),
// and spins until all of them confirm completion. Does NOT invalidate
// the CALLING CPU's own TLB -- callers that modified their own page
// tables are expected to have already done their own invlpg (exactly
// as Core/Memory/Virtual/PageTables.c's vmm_map()/vmm_unmap() already
// do unconditionally) before calling this to handle every OTHER CPU.
// Safe to call even when no AP has been woken yet, or from the
// single-CPU fallback path -- becomes a no-op (nothing to broadcast
// to, returns immediately) rather than an error in that case, since a
// BSP-only system has no other TLB that could ever go stale.
void tlb_shootdown(uint64_t virt_addr);

// Called by Core/Interrupts/Handlers/InterruptDispatch.c's
// interrupt_handler() when frame->int_no == TLB_SHOOTDOWN_VECTOR --
// invalidates this CPU's own TLB entry for the address the requesting
// CPU wrote, acknowledges via acks_received, and sends the LAPIC EOI.
void tlb_shootdown_dispatch(void);

#endif // APEX_CORE_SMP_TLB_TLB_SHOOTDOWN_H
