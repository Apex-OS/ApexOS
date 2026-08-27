// ApexKernel/Includes/Drivers/Input/Mouse/PS2Port.h
// ApexOS -- Low-level PS/2 controller port I/O (mouse channel)

#ifndef APEX_DRIVERS_INPUT_PS2_PORT_H
#define APEX_DRIVERS_INPUT_PS2_PORT_H

#include "Core/Types.h"

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_CMD 0x64

void ps2_wait_write(void);
void ps2_wait_read(void);

// Sends a byte to the mouse (second PS/2 port) via command 0xD4.
void mouse_write(uint8_t val);
uint8_t mouse_read(void);

#endif // APEX_DRIVERS_INPUT_PS2_PORT_H
