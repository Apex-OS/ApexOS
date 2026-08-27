// ApexKernel/Includes/Core/Interrupts/Handlers/InterruptDispatch.h
// ApexOS -- Central interrupt entry point, called by every ISR/IRQ stub

#ifndef APEX_CORE_INTERRUPTS_HANDLERS_DISPATCH_H
#define APEX_CORE_INTERRUPTS_HANDLERS_DISPATCH_H

#include "Core/Interrupts/IDT/IDT.h"

void interrupt_handler(interrupt_frame_t *frame);

#endif // APEX_CORE_INTERRUPTS_HANDLERS_DISPATCH_H
