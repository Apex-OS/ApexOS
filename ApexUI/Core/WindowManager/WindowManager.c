// ApexUI/Core/WindowManager/WindowManager.c
// ApexOS -- ApexUI v1: window manager core (see WindowManager.h)

#include "Core/WindowManager/WindowManager.h"
#include "Core/Memory/Heap/Heap.h"

static window_t g_windows[WM_MAX_WINDOWS];
static uint32_t g_window_count = 0; // number of slots ever used, not number currently open -- see wm_create_window()
static window_id_t g_next_id = 1;   // 0 is WM_INVALID_WINDOW_ID, never issued

// Z-order, back to front -- g_z_order[0] is drawn first (bottommost),
// g_z_order[g_z_count - 1] is drawn last (topmost, and the focused
// window -- see wm_focus_window()). Stored as a separate array of
// window_id_t rather than sorting g_windows[] itself, so a window_t's
// index in g_windows[] stays stable for its whole lifetime (wm_get_window()
// can find it directly by scanning for a matching id -- see below --
// without anything needing to track index changes from reordering).
static window_id_t g_z_order[WM_MAX_WINDOWS];
static uint32_t g_z_count = 0;

void wm_init(void)
{
    for (uint32_t i = 0; i < WM_MAX_WINDOWS; i++)
    {
        g_windows[i].id = WM_INVALID_WINDOW_ID;
        g_windows[i].closed = true;
    }
    g_window_count = 0;
    g_next_id = 1;
    g_z_count = 0;
}

// Finds a window_t slot by ID -- linear scan is fine at WM_MAX_WINDOWS
// (16) scale, called once or twice per input event at most (see
// Input.c), nowhere near a hot path like the per-pixel framebuffer
// code.
static window_t *find_slot(window_id_t id)
{
    if (id == WM_INVALID_WINDOW_ID)
        return NULL;

    for (uint32_t i = 0; i < g_window_count; i++)
    {
        if (g_windows[i].id == id && !g_windows[i].closed)
            return &g_windows[i];
    }
    return NULL;
}

window_id_t wm_create_window(const char *title, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    // Reuse the first closed slot if one exists, rather than always
    // growing g_window_count -- otherwise WM_MAX_WINDOWS creations
    // total (even with closes in between) would permanently exhaust
    // the array. Falls through to appending a fresh slot if every
    // existing slot (up to g_window_count) is still open.
    uint32_t slot_index = g_window_count;
    for (uint32_t i = 0; i < g_window_count; i++)
    {
        if (g_windows[i].closed)
        {
            slot_index = i;
            break;
        }
    }

    if (slot_index >= WM_MAX_WINDOWS)
        return WM_INVALID_WINDOW_ID;

    uint32_t content_height = (height > WM_TITLEBAR_HEIGHT) ? (height - WM_TITLEBAR_HEIGHT) : 0;
    uint32_t *content = (uint32_t *)kcalloc((size_t)width * content_height, sizeof(uint32_t));
    if (content == NULL)
        return WM_INVALID_WINDOW_ID;

    window_t *win = &g_windows[slot_index];
    win->id = g_next_id++;

    uint32_t i;
    for (i = 0; i < WM_TITLE_MAX_LEN - 1 && title[i]; i++)
        win->title[i] = title[i];
    win->title[i] = '\0';

    win->x = x;
    win->y = y;
    win->width = width;
    win->height = height;
    win->content = content;
    win->content_width = width;
    win->content_height = content_height;
    win->visible = true;
    win->closed = false;

    if (slot_index == g_window_count)
        g_window_count++;

    g_z_order[g_z_count++] = win->id;

    return win->id;
}

window_t *wm_get_window(window_id_t id)
{
    return find_slot(id);
}

void wm_move_window(window_id_t id, int32_t new_x, int32_t new_y)
{
    window_t *win = find_slot(id);
    if (win == NULL)
        return;

    win->x = new_x;
    win->y = new_y;
}

void wm_focus_window(window_id_t id)
{
    if (find_slot(id) == NULL)
        return;

    // Find id's current position in the z-order and shift everything
    // above it down by one slot, then place id at the top -- this is
    // just an array remove+append, done in place. g_z_count stays the
    // same either way (id was already somewhere in the array, this
    // only reorders it), so this never needs to touch g_z_count.
    uint32_t found_at = g_z_count; // sentinel: "not found" (shouldn't happen if find_slot() above succeeded)
    for (uint32_t i = 0; i < g_z_count; i++)
    {
        if (g_z_order[i] == id)
        {
            found_at = i;
            break;
        }
    }
    if (found_at == g_z_count)
        return; // defensive -- id passed find_slot() but isn't in z-order, shouldn't happen

    for (uint32_t i = found_at; i + 1 < g_z_count; i++)
        g_z_order[i] = g_z_order[i + 1];
    g_z_order[g_z_count - 1] = id;
}

window_id_t wm_get_focused_window(void)
{
    if (g_z_count == 0)
        return WM_INVALID_WINDOW_ID;
    return g_z_order[g_z_count - 1]; // topmost == focused, see wm_focus_window()
}

void wm_close_window(window_id_t id)
{
    window_t *win = find_slot(id);
    if (win == NULL)
        return;

    if (win->content != NULL)
    {
        kfree(win->content);
        win->content = NULL;
    }
    win->closed = true;
    win->visible = false;

    // Remove from z-order -- shift everything above it down by one,
    // same technique as wm_focus_window(). Without this, a closed
    // window's ID would linger in g_z_order and Compositor.c would
    // either have to special-case skipping closed windows on every
    // single composite pass (checked far more often than a close
    // happens) or -- worse -- draw a window whose content buffer was
    // just freed.
    for (uint32_t i = 0; i < g_z_count; i++)
    {
        if (g_z_order[i] == id)
        {
            for (uint32_t j = i; j + 1 < g_z_count; j++)
                g_z_order[j] = g_z_order[j + 1];
            g_z_count--;
            break;
        }
    }
}

uint32_t wm_get_window_list(window_id_t *out_ids)
{
    for (uint32_t i = 0; i < g_z_count; i++)
        out_ids[i] = g_z_order[i];
    return g_z_count;
}
