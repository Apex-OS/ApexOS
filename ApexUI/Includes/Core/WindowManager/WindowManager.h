// ApexUI/Includes/Core/WindowManager/WindowManager.h
// ApexOS -- ApexUI v1: window manager core
//
// Owns the set of open windows and their arrangement (position, size,
// z-order, focus) -- not their pixels. Compositor/Compositor.c reads
// this list to know what to draw and where; it never modifies it.
// Input/Input.c is the only thing that changes it, translating raw
// mouse/keyboard events into window operations (move, focus, close).
//
// v1 scope: windows are created in-kernel with hardcoded content (see
// Taskbar/ and the ApexUI demo command) -- there's no syscall yet for
// a separate userspace process to open its own window. The data model
// here is deliberately already shaped for that (each window owns its
// own pixel buffer, addressed by a stable window_id rather than a raw
// pointer external code could hold onto across a resize), so adding
// that syscall later is a matter of exposing this API, not redesigning
// it.

#ifndef APEX_UI_WINDOW_MANAGER_H
#define APEX_UI_WINDOW_MANAGER_H

#include "Core/Types.h"

#define WM_MAX_WINDOWS 16 // Plenty for v1 -- no dynamic app launching yet (see file header)
#define WM_TITLE_MAX_LEN 64
#define WM_TITLEBAR_HEIGHT 24 // Pixels -- also used by Compositor.c to know where a window's content area starts

typedef uint32_t window_id_t;
#define WM_INVALID_WINDOW_ID 0 // 0 is never issued as a real window_id (see wm_create_window)

typedef struct
{
    window_id_t id;
    char title[WM_TITLE_MAX_LEN];

    // Position/size of the whole window (titlebar + content), in
    // screen pixels. Content area is (x, y + WM_TITLEBAR_HEIGHT) to
    // (x + width, y + height).
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;

    // Each window owns its content pixels -- widgets/app content
    // drawn to this buffer, then the compositor blits it to the
    // framebuffer at (x, y + WM_TITLEBAR_HEIGHT) as part of composing
    // the full screen. Owning its own buffer (rather than windows
    // drawing straight into the framebuffer) is what makes
    // occlusion/z-order/dragging work without every window needing to
    // know about every other one -- see Compositor.c.
    uint32_t *content;
    uint32_t content_width;  // == width, cached for convenience once allocated
    uint32_t content_height; // == height - WM_TITLEBAR_HEIGHT

    bool visible;
    bool closed; // true once wm_close_window() runs -- see that function's comment on why this isn't an immediate slot free
} window_t;

// Initializes the window manager -- no windows open, no focus. Call
// once before any wm_create_window() call. Does not touch the
// framebuffer -- Compositor.c's first composite pass is what actually
// draws anything.
void wm_init(void);

// Creates a new window with the given title/position/size and an
// allocated (but not yet drawn to) content buffer, returns its ID.
// The new window becomes focused and is placed at the top of the
// z-order (drawn last, so on top of every existing window) -- this
// matches what a person expects when they open something new. Returns
// WM_INVALID_WINDOW_ID if WM_MAX_WINDOWS is already reached or the
// content buffer's kmalloc fails.
window_id_t wm_create_window(const char *title, int32_t x, int32_t y, uint32_t width, uint32_t height);

// Looks up a window by ID -- returns NULL if it doesn't exist or has
// been closed. Callers (widgets, the taskbar, app code once that
// exists) use this rather than holding a window_t* directly, since a
// pointer into the internal window array isn't guaranteed stable
// across other windows opening/closing (see wm_close_window()).
window_t *wm_get_window(window_id_t id);

// Moves a window to the given top-left position -- does not touch
// z-order or focus. Clipping to keep the window on-screen is the
// caller's job (Input.c does this while dragging); this function
// itself accepts any position, including ones that would put the
// window partly or fully off-screen.
void wm_move_window(window_id_t id, int32_t new_x, int32_t new_y);

// Brings a window to the front of the z-order and gives it input
// focus -- called on click (Input.c) or taskbar selection (Taskbar/).
// A no-op if `id` is already the topmost/focused window, or doesn't
// exist.
void wm_focus_window(window_id_t id);

// Returns the currently focused window's ID, or WM_INVALID_WINDOW_ID
// if no window is open. Keyboard input (once windows consume it, see
// file header on v1 scope) goes to whichever window this returns.
window_id_t wm_get_focused_window(void);

// Marks a window closed and frees its content buffer -- the window_t
// slot itself is not immediately reused (closed stays true, id stays
// assigned) so any window_id_t a caller is still holding resolves to
// "gone" via wm_get_window() returning NULL, rather than silently
// starting to refer to a different, newer window that happened to
// reuse the same slot. Slot reuse only happens on the next
// wm_create_window() once all WM_MAX_WINDOWS slots are exhausted --
// see WindowManager.c.
void wm_close_window(window_id_t id);

// Returns the number of currently open (not closed) windows, and
// fills `out_ids` (must have room for at least WM_MAX_WINDOWS
// entries) with their IDs in z-order (back to front, out_ids[0] is
// the bottommost window). Used by Compositor.c to know what to draw
// and in what order, and by Taskbar/ to list open windows.
uint32_t wm_get_window_list(window_id_t *out_ids);

#endif // APEX_UI_WINDOW_MANAGER_H
