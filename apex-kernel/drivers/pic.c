// apex-kernel/drivers/pic.c
// ApexOS -- PIC 8259 driver

#include "pic.h"
#include "../include/apex/io.h"

static inline void io_wait(void)
{
    outb(0x80, 0);
}

void pic_init(void)
{
    // Cascade init sequence.
    outb(PIC1_CMD, PIC_INIT);
    io_wait();
    outb(PIC2_CMD, PIC_INIT);
    io_wait();

    // Remaps IRQ 0-15 to INT 32-47 (past the CPU's reserved 0-31).
    outb(PIC1_DATA, PIC1_OFFSET);
    io_wait();
    outb(PIC2_DATA, PIC2_OFFSET);
    io_wait();

    // PIC2 is chained to PIC1's IRQ2 line.
    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    // 8086 mode.
    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    // Masks every IRQ except IRQ0 (timer) and IRQ1 (keyboard).
    outb(PIC1_DATA, 0xFC); // 1111 1100 -> IRQ0 + IRQ1 enabled
    outb(PIC2_DATA, 0xFF); // all masked on PIC2
}

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