// ApexUI/Core/Input/Input.c
// ApexOS -- ApexUI v1: input handling (see Input.h)

#include "Core/Input/Input.h"
#include "Core/WindowManager/WindowManager.h"
#include "Taskbar/Taskbar.h"
#include "Drivers/Input/Mouse/Mouse.h"
#include "Drivers/Display/Framebuffer/Framebuffer.h"

// Drag state, persists across ui_input_poll() calls -- a drag spans
// many polls (one per mouse-moved-a-little-bit event while the button
// stays held), so this can't be local to the function. dragging_window
// is WM_INVALID_WINDOW_ID whenever no drag is in progress; every other
// field is only meaningful while it isn't.
static window_id_t dragging_window = WM_INVALID_WINDOW_ID;
static int32_t drag_offset_x = 0; // mouse position relative to the window's top-left, captured at drag start
static int32_t drag_offset_y = 0;

static mouse_state_t last_mouse;
static bool last_mouse_valid = false; // false until the first poll -- see ui_input_poll()

// Set by ui_input_poll() on each call that returns true, read by
// ui_get_dirty_rect()/ui_needs_full_redraw() -- see Input.h.
static ui_rect_t last_dirty_rect;
static bool last_needs_full_redraw = false;

// Cursor size used only to size the dirty rect around a cursor
// move -- must be at least as large as fb_draw_cursor()'s actual
// drawn size (Drawing/Cursor.c) or a fast mouse move would leave
// stale cursor pixels outside the recomposited region. Matches its
// hardcoded 12x19 arrow (see that file) with a couple pixels of
// slack rather than importing a constant from a Drivers/ header
// Input.c has no other reason to depend on.
#define UI_CURSOR_DIRTY_SIZE 20

// Grows `r` to also cover rectangle (x, y, w, h) -- used to build the
// union of an old and new position (window drag, cursor move) into a
// single dirty rect that's guaranteed to cover both.
static void rect_union(ui_rect_t *r, int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    if (r->width == 0 && r->height == 0)
    {
        r->x = x;
        r->y = y;
        r->width = w;
        r->height = h;
        return;
    }

    int32_t x0 = (x < r->x) ? x : r->x;
    int32_t y0 = (y < r->y) ? y : r->y;
    int32_t x1_new = x + (int32_t)w;
    int32_t y1_new = y + (int32_t)h;
    int32_t x1_old = r->x + (int32_t)r->width;
    int32_t y1_old = r->y + (int32_t)r->height;
    int32_t x1 = (x1_new > x1_old) ? x1_new : x1_old;
    int32_t y1 = (y1_new > y1_old) ? y1_new : y1_old;

    r->x = x0;
    r->y = y0;
    r->width = (uint32_t)(x1 - x0);
    r->height = (uint32_t)(y1 - y0);
}

// Finds which open window (if any) contains screen point (x, y),
// topmost first -- returns WM_INVALID_WINDOW_ID if the point isn't
// over any window. Topmost-first matters because windows can overlap;
// a click in the overlapping region belongs to whichever window is
// actually visible there (the one on top), not whichever happens to
// be earlier in z-order.
static window_id_t window_at_point(int32_t x, int32_t y)
{
    window_id_t ids[WM_MAX_WINDOWS];
    uint32_t count = wm_get_window_list(ids); // back-to-front

    for (int32_t i = (int32_t)count - 1; i >= 0; i--)
    {
        window_t *win = wm_get_window(ids[i]);
        if (win == NULL || !win->visible)
            continue;

        if (x >= win->x && x < win->x + (int32_t)win->width &&
            y >= win->y && y < win->y + (int32_t)win->height)
            return ids[i];
    }

    return WM_INVALID_WINDOW_ID;
}

bool ui_input_poll(void)
{
    mouse_state_t mouse = mouse_get_state();

    if (!last_mouse_valid)
    {
        // First call: nothing to compare against yet -- record the
        // baseline and report "no change" rather than treating the
        // very first poll as a move from (0,0), which would be a
        // false movement of whatever distance the mouse happens to
        // already be from the origin at ApexUI startup.
        last_mouse = mouse;
        last_mouse_valid = true;
        return false;
    }

    bool changed = false;
    ui_rect_t dirty = {0, 0, 0, 0};
    bool needs_full = false;

    bool button_just_pressed = mouse.left_button && !last_mouse.left_button;
    bool button_just_released = !mouse.left_button && last_mouse.left_button;
    bool moved = (mouse.x != last_mouse.x) || (mouse.y != last_mouse.y);

    if (button_just_pressed)
    {
        fb_info_t info = fb_get_info();

        window_id_t taskbar_hit = taskbar_hit_test(mouse.x, mouse.y, info.height);
        if (taskbar_hit != WM_INVALID_WINDOW_ID)
        {
            wm_focus_window(taskbar_hit);
            changed = true;
            // A focus/z-order change can reveal or hide parts of any
            // open window, not just the one clicked -- not a single
            // bounded region, so no dirty rect is computed here.
            needs_full = true;
        }
        else
        {
            window_id_t hit = window_at_point(mouse.x, mouse.y);
            if (hit != WM_INVALID_WINDOW_ID)
            {
                wm_focus_window(hit);
                changed = true;
                needs_full = true; // same reasoning as the taskbar case above

                window_t *win = wm_get_window(hit);
                // Only the titlebar starts a drag -- a click on the
                // content area still focuses the window (above) but
                // shouldn't move it; that's what a person expects
                // from clicking inside a window versus grabbing its
                // title bar.
                if (win != NULL && mouse.y >= win->y && mouse.y < win->y + (int32_t)WM_TITLEBAR_HEIGHT)
                {
                    dragging_window = hit;
                    drag_offset_x = mouse.x - win->x;
                    drag_offset_y = mouse.y - win->y;
                }
            }
        }
    }
    else if (button_just_released)
    {
        dragging_window = WM_INVALID_WINDOW_ID;
    }
    else if (moved && dragging_window != WM_INVALID_WINDOW_ID)
    {
        window_t *win = wm_get_window(dragging_window);
        if (win != NULL)
        {
            // Old position first (before the move actually happens),
            // then the new one -- rect_union() below covers both, so
            // whatever the window used to overlap (now exposed) and
            // whatever it newly overlaps both get redrawn.
            rect_union(&dirty, win->x, win->y, win->width, win->height);
            wm_move_window(dragging_window, mouse.x - drag_offset_x, mouse.y - drag_offset_y);
            rect_union(&dirty, win->x, win->y, win->width, win->height);
        }
        else
        {
            wm_move_window(dragging_window, mouse.x - drag_offset_x, mouse.y - drag_offset_y);
        }
        changed = true;
    }

    // A plain mouse move (dragging or not) still needs the cursor's
    // old and new position redrawn -- the cursor itself moved even
    // when no window did, and even during a drag the cursor's own
    // rect can extend past the dragged window's (e.g. right at a
    // window edge), so this is unioned in on top of any window rect
    // set above rather than replacing it.
    if (moved)
    {
        changed = true;
        rect_union(&dirty, last_mouse.x, last_mouse.y, UI_CURSOR_DIRTY_SIZE, UI_CURSOR_DIRTY_SIZE);
        rect_union(&dirty, mouse.x, mouse.y, UI_CURSOR_DIRTY_SIZE, UI_CURSOR_DIRTY_SIZE);
    }

    last_dirty_rect = dirty;
    last_needs_full_redraw = needs_full;
    last_mouse = mouse;
    return changed;
}

ui_rect_t ui_get_dirty_rect(void)
{
    return last_dirty_rect;
}

bool ui_needs_full_redraw(void)
{
    return last_needs_full_redraw;
}
