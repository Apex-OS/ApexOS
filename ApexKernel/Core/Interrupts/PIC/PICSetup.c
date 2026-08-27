// ApexKernel/Core/Interrupts/PIC/PICSetup.c
// ApexOS -- PIC 8259 bootstrap (cascade remap)

#include "Core/Interrupts/PIC/PICSetup.h"
#include "Bus/PortIO.h"

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
