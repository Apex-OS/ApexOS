// ApexKernel/Devices/Serial/16550/Serial16550.c
// ApexOS -- 16550 UART implementation (COM1)

#include "Devices/Serial/16550/Serial16550.h"
#include "Bus/PortIO.h"

void serial_init(void)
{
    outb(COM1_PORT + 1, 0x00); // Disable interrupts
    outb(COM1_PORT + 3, 0x80); // Enable DLAB
    outb(COM1_PORT + 0, 0x03); // Baud rate 38400 (lo)
    outb(COM1_PORT + 1, 0x00); // Baud rate 38400 (hi)
    outb(COM1_PORT + 3, 0x03); // 8 bits, no parity, 1 stop
    outb(COM1_PORT + 2, 0xC7); // Enable FIFO
    outb(COM1_PORT + 4, 0x0B); // IRQs enabled, RTS/DSR set
}

static void serial_wait(void)
{
    while (!(inb(COM1_PORT + 5) & 0x20))
        ;
}

void serial_putchar(char c)
{
    serial_wait();
    outb(COM1_PORT, c);
}
