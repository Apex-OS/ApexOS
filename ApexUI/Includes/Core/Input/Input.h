// ApexUI/Includes/Core/Input/Input.h
// ApexOS -- ApexUI v1: input handling
//
// Bridges the raw PS/2 mouse/keyboard state (Drivers/Input/) to
// WindowManager operations: clicking a window's titlebar focuses it
// and starts a drag; clicking a taskbar button focuses that window;
// releasing the mouse ends a drag. This is the only code that calls
// wm_move_window()/wm_focus_window() -- WindowManager.h's own API
// doesn't know about the mouse at all, it just exposes what can be
// done to a window (see that file's header comment).
//
// v1 scope: no window content interaction yet (clicking inside a
// window's content area, resizing, closing via a titlebar button) --
// just enough to open/arrange/switch between windows. See this
// module's ui_input_poll() call site (the ApexUI main loop) for where
// that would be extended once widgets need to receive their own
// click events (see Widgets/Widgets.h).

#ifndef APEX_UI_INPUT_H
#define APEX_UI_INPUT_H

#include "Core/Types.h"

// Screen-space rectangle, used to report the region that actually
// changed since the last poll -- see ui_get_dirty_rect() below.
typedef struct
{
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
} ui_rect_t;

// Call once per ApexUI main-loop iteration (see the ApexUI demo
// command). Reads the current mouse/keyboard state, updates window
// manager state accordingly (focus, drag position), and returns true
// if anything changed that requires a recomposite (compositor_draw_frame())
// -- false means the screen is still accurate as last drawn, so the
// caller can skip an unnecessary full recomposite. Mirrors this
// codebase's fbcon convention of only redrawing on an actual change
// (see Console/ConsoleCore.c's fbcon_putchar -- present is skipped
// when nothing moved).
bool ui_input_poll(void);

// Returns the screen-space rectangle that changed as a result of the
// most recent ui_input_poll() call that returned true -- valid only
// right after such a call, and only meaningful when
// ui_needs_full_redraw() is false (see that function). A plain cursor
// move or window drag only affects a bounded region (the union of the
// affected window/cursor's old and new position); recompositing just
// that region instead of the whole screen is what makes dragging
// smooth on a large VBE framebuffer (see Compositor.c's
// compositor_draw_frame() and the fbcon_present_force() history this
// mirrors -- full-screen redraws on a 1600x900 framebuffer are the
// expensive case there too).
ui_rect_t ui_get_dirty_rect(void);

// True if the most recent change (focus/z-order change from a click,
// or a window opening/closing) means more than one bounded region
// changed -- e.g. focusing a window changes its draw order relative
// to every other window, which can reveal/hide parts of any of them,
// not just the clicked one. In that case ui_get_dirty_rect() isn't
// meaningful and the caller should fall back to a full recomposite.
bool ui_needs_full_redraw(void);

#endif // APEX_UI_INPUT_H
