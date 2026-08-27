// ApexKernel/Includes/Core/Interrupts/IRQ/IRQDispatch.h
// ApexOS -- Hardware IRQ routing to driver handlers

#ifndef APEX_CORE_INTERRUPTS_IRQ_DISPATCH_H
#define APEX_CORE_INTERRUPTS_IRQ_DISPATCH_H

#include "Core/Interrupts/IDT/IDT.h"

// Routes a hardware IRQ (int_no >= 32) to the matching driver handler
// and sends EOI to the PIC. IRQ0 is handled entirely in
// TimerInterrupt.asm, not here.
void irq_dispatch(interrupt_frame_t *frame);

#endif // APEX_CORE_INTERRUPTS_IRQ_DISPATCH_H
