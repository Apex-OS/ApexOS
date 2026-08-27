// ApexKernel/Core/Interrupts/IRQ/IRQDispatch.c
// ApexOS -- Hardware IRQ routing to driver handlers

#include "Core/Interrupts/IRQ/IRQDispatch.h"
#include "Core/Interrupts/PIC/PICControl.h"
#include "Drivers/Input/Keyboard/Keyboard.h"
#include "Drivers/Input/Mouse/Mouse.h"

void irq_dispatch(interrupt_frame_t *frame)
{
    uint8_t irq = frame->int_no - 32;
    if (irq == 0)
    {
        // IRQ0 (PIT tick / scheduler) is handled entirely in
        // TimerInterrupt.asm, not here.
    }
    else if (irq == 1)
    {
        keyboard_handler();
    }
    else if (irq == 12)
    {
        mouse_handler();
    }
    pic_send_eoi(irq);
}
