// ApexKernel/Drivers/Input/Mouse/PS2Port.c
// ApexOS -- Low-level PS/2 controller port I/O (mouse channel)

#include "Drivers/Input/Mouse/PS2Port.h"
#include "Bus/PortIO.h"

void ps2_wait_write(void)
{
    for (int i = 0; i < 100000; i++)
        if (!(inb(PS2_STATUS) & 0x02))
            return;
}

void ps2_wait_read(void)
{
    for (int i = 0; i < 100000; i++)
        if (inb(PS2_STATUS) & 0x01)
            return;
}

void mouse_write(uint8_t val)
{
    ps2_wait_write();
    outb(PS2_CMD, 0xD4);
    ps2_wait_write();
    outb(PS2_DATA, val);
}

uint8_t mouse_read(void)
{
    ps2_wait_read();
    return inb(PS2_DATA);
}
