// apex-kernel/drivers/keyboard.h
// ApexOS -- PS/2 keyboard driver

#ifndef APEX_KEYBOARD_H
#define APEX_KEYBOARD_H

#include "../include/apex/types.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

#define KEYBOARD_BUFFER_SIZE 256

// Codes for non-ASCII keys (arrows). Placed in the unused high range
// (128-255) to stay compatible with the existing char-based buffer
// without colliding with normal ASCII.
#define KEY_ARROW_UP 0x80
#define KEY_ARROW_DOWN 0x81
#define KEY_ARROW_LEFT 0x82
#define KEY_ARROW_RIGHT 0x83

void keyboard_init(void);
void keyboard_handler(void); // Called by IRQ1
bool keyboard_has_key(void);

// uint8_t (not char): arrow codes (KEY_ARROW_*) are >= 0x80, which
// would be ambiguous with a signed char (0x80 becomes -128). uint8_t
// removes the ambiguity -- normal ASCII (0-127) is unaffected.
uint8_t keyboard_getchar(void);

#endif // APEX_KEYBOARD_H