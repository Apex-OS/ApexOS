// ApexKernel/Core/Interrupts/PIC/PICControl.c
// ApexOS -- PIC 8259 runtime control (EOI, IRQ masking)

#include "Core/Interrupts/PIC/PICControl.h"
#include "Core/Interrupts/PIC/PICSetup.h"
#include "Bus/PortIO.h"

void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

void pic_mask_irq(uint8_t irq)
{
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8)
        irq -= 8;
    outb(port, inb(port) | (1 << irq));
}

void pic_unmask_irq(uint8_t irq)
{
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8)
        irq -= 8;
    outb(port, inb(port) & ~(1 << irq));
}
