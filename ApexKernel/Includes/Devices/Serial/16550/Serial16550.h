// ApexKernel/Includes/Devices/Serial/16550/Serial16550.h
// ApexOS -- 16550 UART implementation (COM1)

#ifndef APEX_DEVICES_SERIAL_16550_H
#define APEX_DEVICES_SERIAL_16550_H

#define COM1_PORT 0x3F8

void serial_init(void);
void serial_putchar(char c);

#endif // APEX_DEVICES_SERIAL_16550_H
