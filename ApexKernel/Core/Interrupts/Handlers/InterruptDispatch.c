// ApexKernel/Core/Interrupts/Handlers/InterruptDispatch.c
// ApexOS -- Central interrupt entry point, called by every ISR/IRQ stub

#include "Core/Interrupts/Handlers/InterruptDispatch.h"
#include "Core/Interrupts/Exceptions/ExceptionReport.h"
#include "Core/Interrupts/IRQ/IRQDispatch.h"

// Main C handler -- called by every stub in Interrupts.asm.
void interrupt_handler(interrupt_frame_t *frame)
{
    if (frame->int_no < 32)
        exception_report(frame);
    else
        irq_dispatch(frame);
}
