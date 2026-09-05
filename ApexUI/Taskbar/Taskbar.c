// ApexUI/Taskbar/Taskbar.c
// ApexOS -- ApexUI v1: taskbar (see Taskbar.h)

#include "Taskbar/Taskbar.h"
#include "Drivers/Display/Drawing/Shapes.h"
#include "Drivers/Display/Drawing/Text.h"

// Shared layout logic between taskbar_draw() and taskbar_hit_test() --
// given a button's index in the open-window list, returns its
// on-screen rectangle. Keeping this in one place is what guarantees
// hit-testing always agrees with what was actually drawn (see
// Taskbar.h's comment on why they're separate functions in the first
// place).
static void button_rect(uint32_t index, uint32_t screen_height, int32_t *out_x, int32_t *out_y)
{
    *out_x = TASKBAR_BUTTON_GAP + (int32_t)index * (TASKBAR_BUTTON_WIDTH + TASKBAR_BUTTON_GAP);
    *out_y = (int32_t)(screen_height - TASKBAR_HEIGHT) + (TASKBAR_HEIGHT - 20) / 2;
}

void taskbar_draw(uint32_t screen_width, uint32_t screen_height)
{
    uint32_t bar_y = screen_height - TASKBAR_HEIGHT;
    fb_fill_rect(0, bar_y, screen_width, TASKBAR_HEIGHT, TASKBAR_BG_COLOR);

    window_id_t ids[WM_MAX_WINDOWS];
    uint32_t count = wm_get_window_list(ids);
    window_id_t focused = wm_get_focused_window();

    for (uint32_t i = 0; i < count; i++)
    {
        window_t *win = wm_get_window(ids[i]);
        if (win == NULL)
            continue; // shouldn't happen -- wm_get_window_list() only returns open windows

        int32_t bx, by;
        button_rect(i, screen_height, &bx, &by);

        uint32_t color = (ids[i] == focused) ? TASKBAR_BUTTON_FOCUSED_COLOR : TASKBAR_BUTTON_COLOR;
        fb_fill_rect((uint32_t)bx, (uint32_t)by, TASKBAR_BUTTON_WIDTH, 20, color);
        fb_draw_text((uint32_t)bx + 6, (uint32_t)by + 2, win->title, 1, 0xFFFFFF);
    }
}

window_id_t taskbar_hit_test(int32_t x, int32_t y, uint32_t screen_height)
{
    int32_t bar_y = (int32_t)(screen_height - TASKBAR_HEIGHT);
    if (y < bar_y || y >= (int32_t)screen_height)
        return WM_INVALID_WINDOW_ID;

    window_id_t ids[WM_MAX_WINDOWS];
    uint32_t count = wm_get_window_list(ids);

    for (uint32_t i = 0; i < count; i++)
    {
        int32_t bx, by;
        button_rect(i, screen_height, &bx, &by);

        if (x >= bx && x < bx + TASKBAR_BUTTON_WIDTH && y >= by && y < by + 20)
            return ids[i];
    }

    return WM_INVALID_WINDOW_ID;
}
