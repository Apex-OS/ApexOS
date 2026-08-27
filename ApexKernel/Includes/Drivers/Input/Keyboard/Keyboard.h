// ApexKernel/Includes/Drivers/Input/Keyboard/Keyboard.h
// ApexOS -- PS/2 keyboard driver

#ifndef APEX_DRIVERS_INPUT_KEYBOARD_H
#define APEX_DRIVERS_INPUT_KEYBOARD_H

#include "Core/Types.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

// Codes for non-ASCII keys (arrows). Placed in the unused high range
// (128-255) to stay compatible with the existing char-based buffer
// without colliding with normal ASCII.
#define KEY_ARROW_UP 0x80
#define KEY_ARROW_DOWN 0x81
#define KEY_ARROW_LEFT 0x82
#define KEY_ARROW_RIGHT 0x83

void keyboard_init(void);
void keyboard_handler(void); // Called by IRQ1

#endif // APEX_DRIVERS_INPUT_KEYBOARD_H
