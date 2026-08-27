// ApexKernel/Includes/Drivers/Input/Keyboard/KeyBuffer.h
// ApexOS -- Keyboard ring buffer

#ifndef APEX_DRIVERS_INPUT_KEY_BUFFER_H
#define APEX_DRIVERS_INPUT_KEY_BUFFER_H

#include "Core/Types.h"

#define KEYBOARD_BUFFER_SIZE 256

bool keyboard_has_key(void);

// uint8_t (not char): arrow codes (KEY_ARROW_*) are >= 0x80, which
// would be ambiguous with a signed char (0x80 becomes -128). uint8_t
// removes the ambiguity -- normal ASCII (0-127) is unaffected.
uint8_t keyboard_getchar(void);

#endif // APEX_DRIVERS_INPUT_KEY_BUFFER_H
