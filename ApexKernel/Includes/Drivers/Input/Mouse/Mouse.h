// ApexKernel/Includes/Drivers/Input/Mouse/Mouse.h
// ApexOS -- PS/2 mouse driver (wheel scroll + absolute position tracking)
//
// Standard PS/2 mouse = 3-byte packets (buttons, delta X, delta Y).
// The IntelliMouse extension adds a 4th byte (wheel delta) after an
// activation sequence (sample rate 200,100,80 sent via the "Set
// Sample Rate" command 0xF3). Without that sequence, the 4th byte
// doesn't exist and the wheel is silently ignored by the hardware.

#ifndef APEX_DRIVERS_INPUT_MOUSE_H
#define APEX_DRIVERS_INPUT_MOUSE_H

#include "Core/Types.h"

typedef struct
{
    int32_t x;
    int32_t y;
    bool left_button;
    bool right_button;
    bool middle_button;
} mouse_state_t;

void mouse_init(void);
void mouse_handler(void); // Called by IRQ12

// Sets the bounds the tracked position is clamped to -- call once
// graphics mode is active (see fb_get_info()) so the cursor can't be
// moved off-screen. Before this is called (or on a machine with no
// framebuffer), position tracking still works but is never clamped,
// which is harmless as long as nothing reads mouse_get_state() to draw
// a cursor.
void mouse_set_bounds(int32_t max_x, int32_t max_y);

// Returns the current tracked position and button state. Safe to call
// from anywhere -- mouse_handler() (IRQ context) is the only writer,
// this is a plain read of the same static state.
mouse_state_t mouse_get_state(void);

#endif // APEX_DRIVERS_INPUT_MOUSE_H
