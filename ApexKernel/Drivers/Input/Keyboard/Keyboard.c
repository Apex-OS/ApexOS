// ApexKernel/Drivers/Input/Keyboard/Keyboard.c
// ApexOS -- PS/2 keyboard driver

#include "Drivers/Input/Keyboard/Keyboard.h"
#include "Drivers/Input/Keyboard/KeyBuffer.h"
#include "Drivers/Input/Keyboard/KeyboardInternal.h"
#include "Drivers/Input/Keyboard/ScancodeTables.h"
#include "Bus/PortIO.h"

void keyboard_init(void)
{
    // Drains any stale byte left in the PS/2 output buffer.
    int timeout = 1000;
    while (timeout-- && (inb(KEYBOARD_STATUS_PORT) & 0x01))
        inb(KEYBOARD_DATA_PORT);

    kb_buffer_reset();
}

// Whether Shift (left or right) is currently held -- persists across
// interrupts, updated on Shift press (0x2A/0x36) and release
// (0xAA/0xB6).
static bool shift_pressed = false;

// Whether an extended scancode prefix (arrow keys, etc.) was just
// seen: the keyboard sends 0xE0 then a second byte on the next IRQ.
// This just remembers "waiting for the byte after an 0xE0" between
// handler calls.
static bool extended_prefix_pending = false;

void keyboard_handler(void)
{
    if (!(inb(KEYBOARD_STATUS_PORT) & 0x01))
        return;

    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    // Extended scancode prefix (arrow keys, Home/End, dedicated
    // keypad, etc.) -- just remember that the next byte completes it.
    if (scancode == 0xE0)
    {
        extended_prefix_pending = true;
        return;
    }

    if (extended_prefix_pending)
    {
        extended_prefix_pending = false;

        // Ignores key release for extended keys (bit 7 set), same as
        // for normal keys -- only key press matters here.
        if (scancode & 0x80)
            return;

        switch (scancode)
        {
        case 0x48:
            kb_buffer_push(KEY_ARROW_UP);
            return;
        case 0x50:
            kb_buffer_push(KEY_ARROW_DOWN);
            return;
        case 0x4B:
            kb_buffer_push(KEY_ARROW_LEFT);
            return;
        case 0x4D:
            kb_buffer_push(KEY_ARROW_RIGHT);
            return;
        default:
            // Unhandled extended key (Home, End, Delete, numeric
            // keypad...) -- silently ignored for now.
            return;
        }
    }

    // Ignores key release (bit 7 set).
    if (scancode & 0x80)
    {
        // Shift release (left 0xAA = 0x2A | 0x80, right 0xB6 = 0x36 |
        // 0x80) turns off uppercase mode.
        if (scancode == 0xAA || scancode == 0xB6)
            shift_pressed = false;
        return;
    }

    // Shift press (left 0x2A, right 0x36) -- doesn't push anything,
    // just turns on uppercase mode for subsequent keys.
    if (scancode == 0x2A || scancode == 0x36)
    {
        shift_pressed = true;
        return;
    }

    if (scancode == 0x00 || scancode == 0xFF)
        return;

    // Converts scancode -> ASCII, picking the table based on Shift
    // state.
    if (scancode < scancode_map_len)
    {
        char c = shift_pressed ? scancode_map_shift[scancode] : scancode_map[scancode];
        if (c)
            kb_buffer_push((uint8_t)c);
    }
}
