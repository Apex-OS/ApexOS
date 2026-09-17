// ApexKernel/Includes/Core/Interrupts/IRQ/IRQDispatch.h
// ApexOS -- Hardware IRQ routing to driver handlers

#ifndef APEX_CORE_INTERRUPTS_IRQ_DISPATCH_H
#define APEX_CORE_INTERRUPTS_IRQ_DISPATCH_H

#include "Core/Interrupts/IDT/IDT.h"

// Routes a hardware IRQ to the matching driver handler and sends EOI
// through whichever mechanism actually delivered it: the legacy PIC
// (Core/Interrupts/PIC/PICControl.c's pic_send_eoi()) for int_no in
// 32-47, or the LAPIC (Core/SMP/APIC/Lapic.h's lapic_send_eoi()) for
// int_no in IOAPIC_VECTOR_BASE..IOAPIC_VECTOR_BASE+15 (Core/SMP/
// IOAPIC/IoApic.h) -- an IRQ migrated to I/O APIC routing arrives on a
// DIFFERENT vector than its PIC-remapped one specifically so this
// function (and Core/Interrupts/IDT/IDT.c's own two separate stub
// registrations) can tell which path delivered it without needing any
// other signal. IRQ0 is handled entirely in TimerInterrupt.asm, not
// here.
void irq_dispatch(interrupt_frame_t *frame);

#endif // APEX_CORE_INTERRUPTS_IRQ_DISPATCH_H
