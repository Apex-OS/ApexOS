// apex-kernel/drivers/mouse.c
// ApexOS -- PS/2 mouse driver, wheel only (IntelliMouse extension)
//
// Standard PS/2 mouse = 3-byte packets (buttons, delta X, delta Y).
// The IntelliMouse extension adds a 4th byte (wheel delta) after an
// activation sequence (sample rate 200,100,80 sent via the "Set
// Sample Rate" command 0xF3). Without that sequence, the 4th byte
// doesn't exist and the wheel is silently ignored by the hardware.

#include "mouse.h"
#include "../include/apex/io.h"
#include "vga.h"
#include "../core/klog.h"

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_CMD 0x64

static void ps2_wait_write(void)
{
    for (int i = 0; i < 100000; i++)
        if (!(inb(PS2_STATUS) & 0x02))
            return;
}

static void ps2_wait_read(void)
{
    for (int i = 0; i < 100000; i++)
        if (inb(PS2_STATUS) & 0x01)
            return;
}

// Sends a byte to the mouse (second PS/2 port) via command 0xD4.
static void mouse_write(uint8_t val)
{
    ps2_wait_write();
    outb(PS2_CMD, 0xD4);
    ps2_wait_write();
    outb(PS2_DATA, val);
}

static uint8_t mouse_read(void)
{
    ps2_wait_read();
    return inb(PS2_DATA);
}

static uint8_t packet[4];
static uint8_t packet_index = 0;
static bool has_wheel = false;

void mouse_init(void)
{
    // Enables the second PS/2 port (mouse).
    ps2_wait_write();
    outb(PS2_CMD, 0xA8);

    // Enables IRQ12 in the PS/2 controller.
    ps2_wait_write();
    outb(PS2_CMD, 0x20); // "Read Config Byte"
    uint8_t config = mouse_read();
    config |= 0x02;  // bit 1 = enable IRQ12
    config &= ~0x20; // bit 5 = un-pause mouse clock

    ps2_wait_write();
    outb(PS2_CMD, 0x60); // "Write Config Byte"
    ps2_wait_write();
    outb(PS2_DATA, config);

    // IntelliMouse activation sequence: sample rate 200, 100, 80 with
    // nothing read in between -- hardware that supports the wheel
    // extension recognizes this magic sequence and switches to 4-byte
    // packets. No adverse effect on hardware without a wheel.
    mouse_write(0xF3);
    mouse_read(); // ACK
    mouse_write(200);
    mouse_read(); // ACK

    mouse_write(0xF3);
    mouse_read();
    mouse_write(100);
    mouse_read();

    mouse_write(0xF3);
    mouse_read();
    mouse_write(80);
    mouse_read();

    // Requests the device ID -- 0x03 = mouse with wheel (IntelliMouse),
    // 0x00 = standard 3-button mouse without a wheel.
    mouse_write(0xF2);
    mouse_read(); // ACK
    uint8_t device_id = mouse_read();
    has_wheel = (device_id == 0x03);

    // Re-enables data reporting.
    mouse_write(0xF4);
    mouse_read(); // ACK

    if (has_wheel)
        klog_ok("PS/2 mouse initialized (wheel supported)");
    else
        klog_warn("PS/2 mouse: no wheel detected");

    packet_index = 0;
}

void mouse_handler(void)
{
    uint8_t data = inb(PS2_DATA);

    // The first byte of a packet always has bit 3 set (sync marker)
    // -- if we get a byte without it while expecting a first byte,
    // the stream is desynced (can happen right after boot): drop it
    // and retry on the next byte.
    if (packet_index == 0 && !(data & 0x08))
        return;

    packet[packet_index++] = data;

    uint8_t packet_size = has_wheel ? 4 : 3;
    if (packet_index < packet_size)
        return;

    packet_index = 0;

    if (has_wheel)
    {
        // Byte 4: signed 8-bit wheel delta (two's complement).
        // Typical values: 1 = one notch up, -1 (0xFF) = one notch
        // down. X/Y movement and buttons are ignored for now -- only
        // the wheel matters here.
        int8_t wheel_delta = (int8_t)packet[3];
        if (wheel_delta > 0)
            vga_scroll_down((uint32_t)wheel_delta);
        else if (wheel_delta < 0)
            vga_scroll_up((uint32_t)(-wheel_delta));
    }
}