// apex-kernel/drivers/serial.h
// ApexOS -- Serial port COM1 driver (debug output)

#ifndef APEX_SERIAL_H
#define APEX_SERIAL_H

#include "../include/apex/types.h"

#define COM1_PORT 0x3F8

void serial_init(void);
void serial_putchar(char c);
void serial_puts(const char *str);
void serial_puthex(uint64_t val);
void serial_putdec(uint64_t val);

#endif // APEX_SERIAL_H