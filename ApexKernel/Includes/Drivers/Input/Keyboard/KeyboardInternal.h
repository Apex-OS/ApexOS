// ApexKernel/Includes/Drivers/Input/Keyboard/KeyboardInternal.h
// ApexOS -- Shared internal state for the Keyboard module
//
// kb_buffer_push/kb_buffer_pop and their backing ring buffer are
// defined once in KeyBuffer.c and shared via extern with Keyboard.c
// (the IRQ1 handler pushes into the buffer; keyboard_getchar/
// keyboard_has_key, in KeyBuffer.c itself, read from it). Not part of
// the public KeyBuffer.h API.

#ifndef APEX_DRIVERS_INPUT_KEYBOARD_INTERNAL_H
#define APEX_DRIVERS_INPUT_KEYBOARD_INTERNAL_H

#include "Core/Types.h"

void kb_buffer_push(uint8_t c);
uint8_t kb_buffer_pop(void);

// Resets the ring buffer to empty. Called by keyboard_init().
void kb_buffer_reset(void);

#endif // APEX_DRIVERS_INPUT_KEYBOARD_INTERNAL_H
