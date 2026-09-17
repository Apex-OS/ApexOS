// ApexKernel/Includes/Core/SMP/IPI/Ipi.h
// ApexOS -- Generic Inter-Processor Interrupts (SMP Phase 5)
//
// Distinct from Core/SMP/Startup/AppProcessorStartup.h's INIT/SIPI:
// those two IPI types exist purely to bring a completely idle AP to
// life (Intel SDM Vol. 3A, 8.4.4) and have no C-level handler at all
// -- an AP receiving SIPI starts executing the AP trampoline in real
// mode from a cold stop, not an interrupt handler on a CPU that was
// already doing something else. A generic IPI is the opposite case:
// interrupting a CPU that's already up and running (any core, BSP or
// AP) to make it do something specific right now -- reschedule,
// flush its TLB, etc. -- via a normal interrupt vector and handler,
// the same mechanism as any hardware IRQ, just delivered by another
// CPU's LAPIC instead of a device.
//
// This phase only establishes the mechanism itself (a reserved
// vector, a handler, and a way to target one specific CPU) proven via
// a simple counter each CPU increments on receipt -- see
// IpiDispatch.c's ipi_dispatch(). Real uses (a reschedule request,
// TLB shootdown) are follow-up work once a per-CPU run queue exists
// to reschedule (Core/SMP/PerCPU/PerCpu.h's own header comment on
// Core/Tasking/Scheduler/Scheduler.c's current_proc still being a
// single global as of this phase) -- there's no reschedule to
// request yet, and no second address space to shoot a TLB entry down
// from, so neither has a real caller to write against today.

#ifndef APEX_CORE_SMP_IPI_IPI_H
#define APEX_CORE_SMP_IPI_IPI_H

#include "Core/Types.h"
#include "Core/Interrupts/IDT/IDT.h"
#include "Core/Tasking/Process/Process.h"

// IDT vector this kernel reserves for generic IPIs. Chosen well clear
// of every vector already in use: 0-31 (CPU exceptions), 32-47 (PIC-
// remapped IRQ 0-15, see Core/Interrupts/PIC/PICSetup.h's
// PIC1_OFFSET/PIC2_OFFSET) -- and clear of 0x80 too, even though nothing
// currently occupies it, since this kernel's syscall path
// (Arch/x86_64/SyscallEntry.asm) uses the dedicated SYSCALL/SYSRET
// instruction pair (STAR/LSTAR MSRs) rather than the classic
// `int 0x80` software-interrupt convention, and reserving that vector
// anyway avoids ever colliding with a future kernel that added
// int 0x80 support for compatibility with something else. 0xF0 is a
// common, unremarkable choice for this exact purpose in hobbyist
// kernels generally -- no specific external convention requires it,
// it just needs to not collide with anything else in THIS kernel's
// own vector space, which it doesn't.
#define IPI_VECTOR 0xF0

// Sends IPI_VECTOR to the CPU at `target_cpu_index` (an index into
// Core/Acpi/Madt.h's g_madt_info.cpu_apic_ids[]/Core/SMP/PerCPU/
// PerCpu.h's percpu array -- NOT a raw APIC ID; this function looks
// that up itself via g_madt_info so every caller works in the same
// index space PerCpu.h's percpu_t.cpu_index already uses). No-op with
// a warning if target_cpu_index is out of range or refers to a CPU
// that was never actually woken (Core/SMP/Startup/
// AppProcessorStartup.c's ap_startup_begin() -- sending an IPI to an
// APIC ID whose core was never sent SIPI has no defined useful
// behavior, there is no handler running there to receive it).
void ipi_send(uint32_t target_cpu_index);

// Phase 6.3: assigns `proc` to `target_cpu_index`'s own percpu_t.current_proc
// (Core/SMP/PerCPU/PerCpu.h), marks it PROCESS_RUNNING, then sends
// IPI_VECTOR to wake that CPU -- see IpiDispatch.c's ipi_dispatch(),
// which checks its own current_proc on receipt and launches it via
// Arch/x86_64/ContextSwitch.asm's proc_iretq_start() if one is
// assigned. Intended for a CPU currently idling in Core/SMP/Startup/
// AppProcessorStartup.c's ap_c_entry() `for (;;) hlt` loop -- sending
// this to a CPU already running a different process would silently
// overwrite that CPU's current_proc without saving its context first,
// abandoning whatever it was doing (no context-save path exists for
// this preemption case, unlike the timer-driven round-robin switch
// Core/Tasking/Scheduler/SchedulerTick.c performs on the BSP, which
// does save the outgoing process's context before switching).
void ipi_send_process(uint32_t target_cpu_index, process_t *proc);

// SMP round-robin: sends IPI_VECTOR to `target_cpu_index` WITHOUT
// assigning it a specific process first, unlike ipi_send_process()
// above -- see IpiDispatch.c's ipi_dispatch(), which (when the target
// CPU's own current_proc is NULL, i.e. nothing was pre-assigned via
// ipi_send_process()) calls Core/Tasking/Scheduler/Scheduler.h's
// scheduler_pick_next() itself on receipt, letting the SAME shared,
// lock-protected round-robin search every CPU's timer tick already
// draws from (Core/Tasking/Scheduler/SchedulerTick.c) decide what
// this CPU runs. This is the difference between "I already decided
// what you're running" (ipi_send_process(), Phase 6.3) and "go pick
// something for yourself" (this function) -- intended for a CPU
// currently idling in Core/SMP/Startup/AppProcessorStartup.c's
// ap_c_entry() `for (;;) hlt` loop with nothing pre-assigned, same
// precondition ipi_send_process() itself documents.
void ipi_send_schedule(uint32_t target_cpu_index);

// Number of IPI_VECTOR interrupts this specific CPU has received
// since boot -- see IpiDispatch.c's ipi_dispatch(), which increments
// this via percpu_get() every time. Reads the CALLING CPU's own
// counter, same "always operates on whichever core executes it"
// convention as Core/SMP/PerCPU/PerCpu.h's percpu_get() itself.
uint64_t ipi_get_received_count(void);

// Called by Core/Interrupts/Handlers/InterruptDispatch.c's
// interrupt_handler() when frame->int_no == IPI_VECTOR -- increments
// the calling (i.e. interrupted) CPU's own percpu_t.ipi_received_count
// and sends the LAPIC EOI (Core/SMP/APIC/Lapic.h's lapic_send_eoi())
// to acknowledge it. Not meant to be called from anywhere except that
// one dispatch site -- declared here (rather than kept file-static in
// IpiDispatch.c) purely because that call site lives in a different
// file.
void ipi_dispatch(interrupt_frame_t *frame);

#endif // APEX_CORE_SMP_IPI_IPI_H
