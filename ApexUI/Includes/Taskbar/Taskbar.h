// ApexUI/Includes/Taskbar/Taskbar.h
// ApexOS -- ApexUI v1: taskbar
//
// A strip along the bottom of the screen listing every open window's
// title as a clickable button; the focused window's button is
// highlighted. v1: clicking a taskbar entry focuses that window (see
// Core/Input/Input.c) -- no minimize/restore state yet (that needs
// windows to support being hidden without closing, which
// WindowManager.h's `visible` field is already there for, just not
// wired up to anything yet).

#ifndef APEX_UI_TASKBAR_H
#define APEX_UI_TASKBAR_H

#include "Core/Types.h"
#include "Core/WindowManager/WindowManager.h"

#define TASKBAR_HEIGHT 32
#define TASKBAR_BG_COLOR 0x202020
#define TASKBAR_BUTTON_COLOR 0x3A3A3A
#define TASKBAR_BUTTON_FOCUSED_COLOR 0x505090
#define TASKBAR_BUTTON_WIDTH 140
#define TASKBAR_BUTTON_GAP 4

// Draws the taskbar at the bottom of a `screen_width` x `screen_height`
// screen -- one button per open window (WindowManager's z-order, left
// to right in that order), the focused window's button highlighted.
// Called by Compositor.c as the second-to-last step of every frame
// (after windows, before the cursor).
void taskbar_draw(uint32_t screen_width, uint32_t screen_height);

// Returns the window_id_t whose taskbar button contains screen point
// (x, y), or WM_INVALID_WINDOW_ID if the point isn't over any button
// (including if it's outside the taskbar strip entirely, or over the
// taskbar background between/after buttons). Used by Input.c to
// handle taskbar clicks -- kept as a separate hit-test function
// rather than folding click handling into taskbar_draw() itself, so
// Input.c can hit-test against the same layout taskbar_draw() just
// drew without the two ever disagreeing about where a button actually
// is.
window_id_t taskbar_hit_test(int32_t x, int32_t y, uint32_t screen_height);

#endif // APEX_UI_TASKBAR_H
