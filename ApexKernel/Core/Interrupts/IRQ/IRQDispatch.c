// ApexKernel/Core/Interrupts/IRQ/IRQDispatch.c
// ApexOS -- Hardware IRQ routing to driver handlers

#include "Core/Interrupts/IRQ/IRQDispatch.h"
#include "Core/Interrupts/PIC/PICControl.h"
#include "Core/SMP/APIC/Lapic.h"
#include "Core/SMP/IOAPIC/IoApic.h"
#include "Drivers/Input/Keyboard/Keyboard.h"
#include "Drivers/Input/Mouse/Mouse.h"

void irq_dispatch(interrupt_frame_t *frame)
{
    // Determines both the actual IRQ number and which acknowledgment
    // path to use from int_no's range -- see IRQDispatch.h's own
    // comment on why this is enough to tell the two paths apart
    // without any other signal (an IRQ is either PIC-routed or I/O
    // APIC-routed, never delivered via both at once for the same
    // IRQ number).
    bool via_ioapic = frame->int_no >= IOAPIC_VECTOR_BASE && frame->int_no < (IOAPIC_VECTOR_BASE + 16);
    uint8_t irq = via_ioapic ? (uint8_t)(frame->int_no - IOAPIC_VECTOR_BASE) : (uint8_t)(frame->int_no - 32);

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

    if (via_ioapic)
        lapic_send_eoi();
    else
        pic_send_eoi(irq);
}
