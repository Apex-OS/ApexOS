// ApexUI/Core/Compositor/Compositor.c
// ApexOS -- ApexUI v1: compositor (see Compositor.h)

#include "Core/Compositor/Compositor.h"
#include "Core/WindowManager/WindowManager.h"
#include "Core/Input/Input.h"
#include "Taskbar/Taskbar.h"
#include "Drivers/Display/Framebuffer/Framebuffer.h"
#include "Drivers/Display/Drawing/Shapes.h"
#include "Drivers/Display/Drawing/Text.h"
#include "Drivers/Display/Drawing/Cursor.h"
#include "Drivers/Input/Mouse/Mouse.h"

#define WINDOW_TITLEBAR_COLOR_FOCUSED 0x4060C0
#define WINDOW_TITLEBAR_COLOR_UNFOCUSED 0x505050
#define WINDOW_BORDER_COLOR 0x000000

// Blits a window's content buffer to the framebuffer at its current
// screen position -- direct pointer writes into the back-buffer via
// fb_put_pixel(), clipped to the screen (a window can be dragged
// partly off-screen; wm_move_window() doesn't clamp, see
// WindowManager.h). Not using fb_present_rect() here -- that pushes
// back-buffer to VRAM, a separate step this function doesn't do itself
// (compositor_draw_frame() calls fb_present() once at the very end for
// the whole composited frame, not per-window).
static void blit_window_content(const window_t *win)
{
    fb_info_t info = fb_get_info();
    int32_t content_x = win->x;
    int32_t content_y = win->y + WM_TITLEBAR_HEIGHT;

    for (uint32_t row = 0; row < win->content_height; row++)
    {
        int32_t py = content_y + (int32_t)row;
        if (py < 0 || py >= (int32_t)info.height)
            continue;

        for (uint32_t col = 0; col < win->content_width; col++)
        {
            int32_t px = content_x + (int32_t)col;
            if (px < 0 || px >= (int32_t)info.width)
                continue;

            fb_put_pixel((uint32_t)px, (uint32_t)py, win->content[row * win->content_width + col]);
        }
    }
}

static void draw_window(const window_t *win, bool focused)
{
    if (!win->visible)
        return;

    uint32_t titlebar_color = focused ? WINDOW_TITLEBAR_COLOR_FOCUSED : WINDOW_TITLEBAR_COLOR_UNFOCUSED;

    // Titlebar first, then content, then a 1px border around the
    // whole window -- border drawn last so it's never partially
    // overwritten by the content blit landing exactly on the edge.
    fb_fill_rect((uint32_t)win->x, (uint32_t)win->y, win->width, WM_TITLEBAR_HEIGHT, titlebar_color);
    fb_draw_text((uint32_t)win->x + 6, (uint32_t)win->y + 4, win->title, 1, 0xFFFFFF);

    blit_window_content(win);

    fb_draw_rect((uint32_t)win->x, (uint32_t)win->y, win->width, win->height, 1, WINDOW_BORDER_COLOR);
}

void compositor_draw_frame(void)
{
    fb_info_t info = fb_get_info();

    fb_clear(COMPOSITOR_BG_COLOR);

    window_id_t ids[WM_MAX_WINDOWS];
    uint32_t count = wm_get_window_list(ids); // back-to-front order -- see WindowManager.h
    window_id_t focused = wm_get_focused_window();

    for (uint32_t i = 0; i < count; i++)
    {
        window_t *win = wm_get_window(ids[i]);
        if (win != NULL)
            draw_window(win, ids[i] == focused);
    }

    taskbar_draw(info.width, info.height);

    mouse_state_t mouse = mouse_get_state();
    fb_draw_cursor((uint32_t)mouse.x, (uint32_t)mouse.y, 0xFFFFFF);

    fb_present();
}

// Clips `r` to the screen, in place -- a dragged window/cursor rect
// can extend off-screen (WindowManager.h doesn't clamp positions, see
// wm_move_window()), and fb_fill_rect()/fb_present_rect() below don't
// clip their width/height themselves the way fb_put_pixel() does per
// pixel.
static void clip_rect_to_screen(ui_rect_t *r, uint32_t screen_w, uint32_t screen_h)
{
    r->x -= 4;
    r->y -= 4;
    r->width += 8;
    r->height += 8;

    if (r->x < 0)
    {
        r->width = (r->width > (uint32_t)(-r->x)) ? r->width - (uint32_t)(-r->x) : 0;
        r->x = 0;
    }
    if (r->y < 0)
    {
        r->height = (r->height > (uint32_t)(-r->y)) ? r->height - (uint32_t)(-r->y) : 0;
        r->y = 0;
    }
    if ((uint32_t)r->x + r->width > screen_w)
        r->width = (uint32_t)r->x < screen_w ? screen_w - (uint32_t)r->x : 0;
    if ((uint32_t)r->y + r->height > screen_h)
        r->height = (uint32_t)r->y < screen_h ? screen_h - (uint32_t)r->y : 0;
}

// Same composite as compositor_draw_frame(), but only clears, redraws
// into, and pushes to VRAM the given screen rect instead of the whole
// framebuffer -- for a plain cursor move or window drag, where only a
// bounded region actually changed (see Input.h's ui_get_dirty_rect()
// and the history comment there on why this exists: a full
// fb_clear()+fb_present() every poll made dragging visibly less
// smooth than gfxtest's one-shot draw, on a large VBE framebuffer).
//
// Every window still gets its draw_window() call attempted (not just
// the one that moved) -- correctness over a finer-grained per-window
// dirty test: a window's own rect union with the dragged window's old
// position can reveal what's underneath, and getting that occlusion
// right in general (partial overlaps, z-order) is real work
// draw_window() and fb_put_pixel()'s per-pixel screen clip already
// handle for free by just being asked to draw the whole scene again
// -- clipping what reaches VRAM (this rect) is where the actual
// framebuffer-sized cost was.
void compositor_draw_frame_dirty(ui_rect_t rect)
{
    fb_info_t info = fb_get_info();
    clip_rect_to_screen(&rect, info.width, info.height);
    if (rect.width == 0 || rect.height == 0)
        return;

    fb_fill_rect((uint32_t)rect.x, (uint32_t)rect.y, rect.width, rect.height, COMPOSITOR_BG_COLOR);

    window_id_t ids[WM_MAX_WINDOWS];
    uint32_t count = wm_get_window_list(ids);
    window_id_t focused = wm_get_focused_window();

    for (uint32_t i = 0; i < count; i++)
    {
        window_t *win = wm_get_window(ids[i]);
        if (win != NULL)
            draw_window(win, ids[i] == focused);
    }

    taskbar_draw(info.width, info.height);

    mouse_state_t mouse = mouse_get_state();
    fb_draw_cursor((uint32_t)mouse.x, (uint32_t)mouse.y, 0xFFFFFF);

    fb_present_rect((uint32_t)rect.x, (uint32_t)rect.y, rect.width, rect.height);
}
