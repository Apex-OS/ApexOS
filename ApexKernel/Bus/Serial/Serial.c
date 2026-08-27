// ApexKernel/Bus/Serial/Serial.c
// ApexOS -- Generic serial output (chip-agnostic)

#include "Bus/Serial/Serial.h"
#include "Devices/Serial/16550/Serial16550.h"

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
