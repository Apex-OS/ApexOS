// ApexUI/Includes/Core/Compositor/Compositor.h
// ApexOS -- ApexUI v1: compositor
//
// Turns the window manager's window list (WindowManager.h) into
// actual pixels: background, every visible window's titlebar +
// content (back to front, so later windows correctly draw over
// earlier ones), the taskbar, and the cursor on top of everything.
//
// Two entry points: compositor_draw_frame() (full screen -- used for
// the first frame, and whenever Input.c reports a focus/z-order
// change via ui_needs_full_redraw(), since that can reveal/hide parts
// of any window, not just one bounded region) and
// compositor_draw_frame_dirty() (just a given rect -- used for a
// plain cursor move or window drag, see Input.h's ui_get_dirty_rect().
// Was full-screen-only before; a full fb_clear()+fb_present() every
// poll made dragging visibly less smooth than gfxtest's one-shot draw
// on a large VBE framebuffer, which is what motivated adding the
// dirty-rect path.

#ifndef APEX_UI_COMPOSITOR_H
#define APEX_UI_COMPOSITOR_H

#include "Core/Types.h"
#include "Core/Input/Input.h"

// Background fill color -- a plain desktop color for v1 (no wallpaper
// image support yet, see ApexUI/Resources/Images/ in the project's
// planned layout for where that would eventually come from).
#define COMPOSITOR_BG_COLOR 0x1E3C72 // matches gfxtest's old demo blue, kept for visual continuity

// Composites the full screen: background, every window in
// WindowManager's z-order, the taskbar (see Taskbar/Taskbar.h), then
// the cursor on top. Pushes the result to VRAM itself (calls
// fb_present() internally) -- callers don't need to.
void compositor_draw_frame(void);

// Same composite, but only the given screen rect is cleared, redrawn,
// and pushed to VRAM (fb_present_rect() internally) -- for a change
// bounded to one region (see Input.h's ui_get_dirty_rect() and
// ui_needs_full_redraw() for when this is and isn't valid to use
// instead of compositor_draw_frame()).
void compositor_draw_frame_dirty(ui_rect_t rect);

#endif // APEX_UI_COMPOSITOR_H
