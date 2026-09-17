// ApexKernel/Includes/Core/SMP/Startup/AppProcessorStartup.h
// ApexOS -- SMP bring-up: waking Application Processors (APs) via
// INIT-SIPI-SIPI
//
// The boot processor (BSP) is already running the moment kmain()
// starts -- every other CPU core (an AP) sits completely idle in a
// hardware-defined wait state until explicitly told to start, via a
// specific LAPIC IPI sequence (Intel SDM Vol. 3A, 8.4.4): first an
// INIT IPI (resets the AP to a known state), then one or two SIPI
// (Startup IPI, tells it where to begin executing).
//
// The address a SIPI tells an AP to jump to isn't arbitrary: an AP
// receiving SIPI always starts execution in 16-bit real mode, at
// physical address (vector << 12) -- meaning the trampoline code
// below has to physically live at a 4KB-page-aligned address below
// 1MB, real-mode-addressable, chosen and reserved specifically for
// this (see AP_TRAMPOLINE_ADDR below), and be written in 16-bit
// assembly for at least its very first instructions (see
// ApBootTrampoline.asm) -- no way around this, it's how the x86
// startup IPI has worked since the original MP spec.

#ifndef APEX_CORE_SMP_STARTUP_AP_PROCESSOR_STARTUP_H
#define APEX_CORE_SMP_STARTUP_AP_PROCESSOR_STARTUP_H

#include "Core/Types.h"
#include "Core/Synchronization/Mutex/Mutex.h"
#include "Core/Synchronization/Semaphore/Semaphore.h"

// Physical address the AP trampoline (ApBootTrampoline.asm) is copied
// to before sending any SIPI, and the address every SIPI targets
// (vector 0x08, since 0x08 << 12 == 0x8000). Chosen from the gap
// between Boot/BIOS/Transition/LongModeEntry.asm's PT_ADDR (0x4000,
// two page tables ending at 0x6000) and Stage2.asm's VBE_INFO_ADDR
// (0x9000) -- specifically NOT SCRATCH_PML4 (0x7000, see
// Core/Memory/Virtual/PageTables.h), which sits right in the middle
// of that same gap and looks free by the time this code ever runs
// (vmm_init() has long finished with it) but naming this trampoline
// after an address already claimed for something else, even
// something whose use has ended, invites exactly the kind of "wait,
// which one owns this byte" confusion this project's low-memory
// address map has already caused real bugs over (see
// Core/Boot/AcpiInfo.h's own history). 0x8000 has no such conflict:
// nothing else in either boot path ever claims it.
//
// Safe from the PMM's perspective too -- Core/Memory/Physical/
// PhysicalMemoryDetection.c's pmm_init() unconditionally reserves the
// entire low 1MB before handing out a single page, so
// pmm_alloc_page() can never be handed this address out from under
// this code.
#define AP_TRAMPOLINE_ADDR 0x8000
#define AP_TRAMPOLINE_VECTOR 0x08 // AP_TRAMPOLINE_ADDR >> 12 -- what actually gets sent in the SIPI

// Data the trampoline (running in real/protected mode, no C runtime,
// no stack of its own yet at the very start) needs from the BSP,
// placed at a fixed offset within the same trampoline page so both
// the assembly and this C code agree on the layout without needing a
// linker to reconcile separately-compiled symbols -- the trampoline
// is hand-assembled machine code copied byte-for-byte into place, not
// linked in the normal sense (see ap_startup_begin()'s comment on
// this).
//
// Deliberately placed at the very end of the page (0x8000 + 0xF00 =
// 0x8F00, 256 bytes before the next page starts) rather than
// immediately after the trampoline code -- keeps this struct's offset
// fixed regardless of how much the trampoline code itself grows
// during development, so ApBootTrampoline.asm's hardcoded references
// to these offsets don't need updating every time a line is added or
// removed earlier in the code.
#define AP_TRAMPOLINE_DATA_OFFSET 0xF00

typedef struct __attribute__((packed))
{
    // Physical address of the top of this AP's own stack -- written
    // by the BSP (ap_startup_wake_one() in the .c) before sending
    // this AP's SIPI, read by the trampoline once it reaches 64-bit
    // code. Every AP MUST get a different value here: two APs
    // sharing a stack corrupts both the instant either one pushes
    // anything, and there is no hardware mechanism that would even
    // detect this, only silent, unpredictable stack corruption a few
    // instructions after both are running.
    uint64_t stack_top;

    // Physical (== virtual, everything low is identity-mapped, see
    // Core/Memory/Virtual/IdentityMap.c) address of the C function the
    // trampoline jumps to once it's fully in 64-bit mode with its own
    // stack set up -- see ap_c_entry() below. A fixed function
    // pointer here rather than the trampoline calling ap_c_entry()
    // directly by name: the trampoline is raw assembly with no
    // knowledge of this kernel's symbol table (see above, it isn't
    // linked), so this is the only way it can find its way into C
    // code at all.
    uint64_t c_entry_addr;

    // Which logical CPU this is, 0-indexed in the same order as
    // Core/Acpi/Madt.h's g_madt_info.cpu_apic_ids[] -- written by the
    // BSP right before this AP's SIPI, read back by ap_c_entry() once
    // running, so each AP's own boot log line (see AppProcessorStartup.c)
    // can identify itself without needing to re-derive its own index
    // from its APIC ID.
    uint32_t cpu_index;

    // Set to 1 by ap_c_entry() as the very last thing it does before
    // idling -- see ap_startup_wake_one()'s spin-wait loop, which
    // polls this to know when it's safe to move on to waking the next
    // AP (or return, once every AP has been woken) rather than moving
    // on while this one might still be mid-boot and about to touch
    // shared state (the trampoline page itself, about to be reused
    // for the next AP -- see that function's own comment on why this
    // handoff has to be synchronous, one AP at a time, rather than
    // firing every SIPI up front).
    volatile uint32_t ap_ready;
} ap_trampoline_data_t;

// Wakes every enabled AP found by Core/Acpi/Madt.c's MADT parse
// (g_madt_info.cpu_apic_ids[1..cpu_count-1] -- index 0 is the BSP
// itself, already running, never sent a SIPI), one at a time, each
// through the full INIT-SIPI-SIPI sequence and a spin-wait for that
// AP to confirm it's alive before moving on to the next. No-op if
// g_madt_info.valid is false or cpu_count <= 1 -- same single-CPU
// fallback philosophy as every other MADT-dependent piece of this
// SMP work (see Madt.h).
void ap_startup_begin(void);

// Phase 6a validation globals -- see AppProcessorStartup.c's own
// comment on g_smp_test_counter/g_smp_test_mutex for what these
// prove. Exposed here so Init.c (the BSP's own side of the same test)
// can participate using the identical shared counter/mutex every AP
// uses, rather than each having its own separate instance that would
// prove nothing about cross-CPU synchronization.
extern uint32_t g_smp_test_counter;
extern mutex_t g_smp_test_mutex;
extern semaphore_t g_smp_test_semaphore;

#endif // APEX_CORE_SMP_STARTUP_AP_PROCESSOR_STARTUP_H
