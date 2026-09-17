// ApexKernel/Drivers/Input/Mouse/Mouse.c
// ApexOS -- PS/2 mouse driver: wheel (IntelliMouse extension) and
// absolute position/button tracking for graphics mode.

#include "Drivers/Input/Mouse/Mouse.h"
#include "Drivers/Input/Mouse/PS2Port.h"
#include "Drivers/Display/VGA.h"
#include "Bus/PortIO.h"
#include "Core/Boot/Log/BootLog.h"

static uint8_t packet[4];
static uint8_t packet_index = 0;
static bool has_wheel = false;

// Absolute position, accumulated from the relative deltas each packet
// reports (see mouse_handler() -- PS/2 mice only ever report movement
// since the last packet, never a real position). Starts at (0,0) --
// callers wanting a centered/specific starting point should draw
// their own initial cursor rather than relying on this default.
static mouse_state_t state = {0, 0, false, false, false};

// Bounds the position is clamped to, set via mouse_set_bounds().
// Defaults to a huge range so tracking is harmless (just unclamped)
// before graphics mode is initialized.
static int32_t bound_max_x = 0x7FFFFFFF;
static int32_t bound_max_y = 0x7FFFFFFF;

void mouse_set_bounds(int32_t max_x, int32_t max_y)
{
    bound_max_x = max_x;
    bound_max_y = max_y;

    if (state.x > bound_max_x)
        state.x = bound_max_x;
    if (state.y > bound_max_y)
        state.y = bound_max_y;
}

mouse_state_t mouse_get_state(void)
{
    return state;
}

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
        boot_log_ok("PS/2 mouse initialized (wheel supported)");
    else
        boot_log_warn("PS/2 mouse: no wheel detected");

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

    // Byte 0 (packet[0]) layout: bit0=left button, bit1=right button,
    // bit2=middle button, bit3=always 1 (sync, already checked
    // above), bit4=X sign, bit5=Y sign, bit6/7=X/Y overflow.
    state.left_button = (packet[0] & 0x01) != 0;
    state.right_button = (packet[0] & 0x02) != 0;
    state.middle_button = (packet[0] & 0x04) != 0;

    // Ignores this packet's movement entirely on overflow -- the OSDev
    // wiki notes the sign/magnitude become unreliable once the mouse
    // has moved further than the 9-bit counter can represent between
    // samples (extremely fast movement); silently skipping avoids the
    // cursor jumping to a bogus position rather than showing
    // reasonable (if slightly stale) movement.
    if (!(packet[0] & 0xC0))
    {
        // Bytes 1/2 are 8-bit magnitudes; the matching sign bit from
        // byte 0 sign-extends them into a real 9-bit two's complement
        // delta -- see the file header for the PS/2 packet layout.
        int32_t delta_x = packet[1];
        if (packet[0] & 0x10)
            delta_x -= 256;

        int32_t delta_y = packet[2];
        if (packet[0] & 0x20)
            delta_y -= 256;

        // PS/2 Y is positive going UP the screen; framebuffer Y is
        // positive going DOWN (row 0 = top) -- flipped here so
        // "moving the mouse up" moves the cursor up on screen, not
        // down.
        state.x -= delta_x;
        state.y += delta_y;

        if (state.x < 0)
            state.x = 0;
        if (state.y < 0)
            state.y = 0;
        if (state.x > bound_max_x)
            state.x = bound_max_x;
        if (state.y > bound_max_y)
            state.y = bound_max_y;
    }

    if (has_wheel)
    {
        // Byte 4: signed 8-bit wheel delta (two's complement).
        // Typical values: 1 = one notch up, -1 (0xFF) = one notch
        // down. X/Y movement and buttons are ignored for now -- only
        // the wheel matters here.
        int8_t wheel_delta = (int8_t)packet[3];
        if (wheel_delta > 0)
            vga_scroll_down((uint32_t)wheel_delta * 2);
        else if (wheel_delta < 0)
            vga_scroll_up((uint32_t)(-wheel_delta) * 2);
    }
}
