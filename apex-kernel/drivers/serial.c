// apex-kernel/drivers/serial.c
// ApexOS -- Serial port COM1 driver (debug output)

#include "serial.h"
#include "../include/apex/io.h"

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

void serial_puts(const char *str)
{
    while (*str)
        serial_putchar(*str++);
}

void serial_puthex(uint64_t val)
{
    const char *hex = "0123456789ABCDEF";
    char buf[19];
    int i = 18;
    buf[i--] = '\0';
    if (val == 0)
    {
        buf[i] = '0';
        serial_puts("0x");
        serial_puts(&buf[i]);
        return;
    }
    while (val && i > 1)
    {
        buf[i--] = hex[val & 0xF];
        val >>= 4;
    }
    buf[i--] = 'x';
    buf[i] = '0';
    serial_puts(&buf[i]);
}

void serial_putdec(uint64_t val)
{
    char buf[21];
    int i = 20;
    buf[i--] = '\0';
    if (val == 0)
    {
        buf[i] = '0';
        serial_puts(&buf[i]);
        return;
    }
    while (val && i >= 0)
    {
        buf[i--] = '0' + (val % 10);
        val /= 10;
    }
    serial_puts(&buf[i + 1]);
}