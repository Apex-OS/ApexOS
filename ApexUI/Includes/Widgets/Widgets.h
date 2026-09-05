// ApexUI/Includes/Widgets/Widgets.h
// ApexOS -- ApexUI v1: basic widgets
//
// v1 scope: two widgets, drawn directly into a window's content
// buffer (window_t.content, see WindowManager.h) by whatever built
// that window's content -- there's no retained widget tree, layout
// engine, or event dispatch yet. A widget here is a drawing function
// plus (for the button) a hit-test helper; the caller is responsible
// for deciding when to call the hit-test (see the ApexUI demo command
// for the pattern: check widget_button_hit_test() against a click
// already known to have landed inside this window's content area).
//
// This is deliberately minimal rather than a "real" widget toolkit --
// enough for ApexUI v1's own demo windows and the taskbar (see
// Taskbar/Taskbar.c, which draws its buttons directly with
// fb_fill_rect()/fb_draw_text() rather than through this API, since it
// draws straight to the framebuffer, not into a window's content
// buffer). A retained-mode widget tree with real event dispatch is
// future work once there's more than one or two demo windows'
// worth of UI to justify it.

#ifndef APEX_UI_WIDGETS_H
#define APEX_UI_WIDGETS_H

#include "Core/Types.h"

#define WIDGET_BUTTON_COLOR 0x4A4A4A
#define WIDGET_BUTTON_HOVER_COLOR 0x5A5A5A
#define WIDGET_BUTTON_TEXT_COLOR 0xFFFFFF
#define WIDGET_LABEL_TEXT_COLOR 0xE0E0E0

// Draws a button at (x, y) in `content` (a window_t.content buffer,
// content_width x content_height pixels -- see WindowManager.h) with
// the given size and label text, centered. `hovered` selects between
// WIDGET_BUTTON_COLOR and WIDGET_BUTTON_HOVER_COLOR -- the caller
// decides hover state (typically via widget_button_hit_test() against
// the current mouse position translated into this window's content-
// relative coordinates), this function only draws.
void widget_draw_button(uint32_t *content, uint32_t content_width, uint32_t content_height,
                        int32_t x, int32_t y, uint32_t w, uint32_t h,
                        const char *label, bool hovered);

// Returns true if content-relative point (px, py) falls inside the
// button rectangle (x, y, w, h) -- same rectangle passed to
// widget_draw_button() for this button. Kept separate from drawing
// (same reasoning as Taskbar.h's taskbar_hit_test()) so hit-testing
// can never disagree with what was actually drawn.
bool widget_button_hit_test(int32_t px, int32_t py, int32_t x, int32_t y, uint32_t w, uint32_t h);

// Draws plain text at (x, y) in `content` -- a thin wrapper that
// exists so window content code has one consistent widget-style API
// (widget_draw_label/widget_draw_button) instead of mixing direct
// fb_draw_text() calls (which write to the framebuffer, not a content
// buffer -- see this file's header on why widgets draw into
// window_t.content instead) with widget calls.
void widget_draw_label(uint32_t *content, uint32_t content_width, uint32_t content_height,
                       int32_t x, int32_t y, const char *text, uint32_t color);

#endif // APEX_UI_WIDGETS_H
