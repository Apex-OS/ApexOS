// ApexUI/Widgets/Widgets.c
// ApexOS -- ApexUI v1: basic widgets (see Widgets.h)

#include "Widgets/Widgets.h"
#include "Drivers/Display/Fonts/Font8x16.h"

// Writes a single pixel into an arbitrary content buffer (not the
// framebuffer/back-buffer -- see Widgets.h's header comment on why
// widgets can't reuse fb_put_pixel()/fb_fill_rect() directly, those
// are hardcoded to the global back-buffer). Clipped to the buffer's
// own bounds -- a widget drawn partially outside its window's content
// area (e.g. a button placed too close to the edge) is silently
// cropped rather than corrupting adjacent heap memory.
static void content_put_pixel(uint32_t *content, uint32_t content_width, uint32_t content_height,
                              int32_t x, int32_t y, uint32_t color)
{
    if (x < 0 || y < 0 || (uint32_t)x >= content_width || (uint32_t)y >= content_height)
        return;
    content[(uint32_t)y * content_width + (uint32_t)x] = color;
}

static void content_fill_rect(uint32_t *content, uint32_t content_width, uint32_t content_height,
                              int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    for (uint32_t row = 0; row < h; row++)
        for (uint32_t col = 0; col < w; col++)
            content_put_pixel(content, content_width, content_height, x + (int32_t)col, y + (int32_t)row, color);
}

// Same glyph-bit-per-pixel approach as fb_draw_text() (see
// Drivers/Display/Drawing/Text.c), but targeting an arbitrary content
// buffer instead of the framebuffer -- see this file's header comment
// on content_put_pixel() for why the two can't share code directly.
static void content_draw_text(uint32_t *content, uint32_t content_width, uint32_t content_height,
                              int32_t x, int32_t y, const char *text, uint32_t color)
{
    int32_t cursor_x = x;
    for (const char *p = text; *p; p++)
    {
        const uint8_t *glyph = font8x16_get_glyph(*p);
        for (uint32_t row = 0; row < 16; row++)
        {
            uint8_t bits = glyph[row];
            for (uint32_t col = 0; col < 8; col++)
            {
                if (bits & (0x80 >> col))
                    content_put_pixel(content, content_width, content_height,
                                      cursor_x + (int32_t)col, y + (int32_t)row, color);
            }
        }
        cursor_x += 8;
    }
}

// Rough width estimate for centering -- every glyph in this codebase's
// font is a fixed 8px wide (see Font8x16.h: FONT8X16_WIDTH), so this
// is exact, not approximate, but kept as a named helper rather than
// inlining "8 * len" at each call site for readability.
static uint32_t text_width(const char *text)
{
    uint32_t len = 0;
    while (text[len])
        len++;
    return len * 8;
}

void widget_draw_button(uint32_t *content, uint32_t content_width, uint32_t content_height,
                        int32_t x, int32_t y, uint32_t w, uint32_t h,
                        const char *label, bool hovered)
{
    uint32_t color = hovered ? WIDGET_BUTTON_HOVER_COLOR : WIDGET_BUTTON_COLOR;
    content_fill_rect(content, content_width, content_height, x, y, w, h, color);

    uint32_t label_w = text_width(label);
    int32_t text_x = x + (int32_t)((w > label_w) ? (w - label_w) / 2 : 0);
    int32_t text_y = y + (int32_t)((h > 16) ? (h - 16) / 2 : 0);
    content_draw_text(content, content_width, content_height, text_x, text_y, label, WIDGET_BUTTON_TEXT_COLOR);
}

bool widget_button_hit_test(int32_t px, int32_t py, int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    return px >= x && px < x + (int32_t)w && py >= y && py < y + (int32_t)h;
}

void widget_draw_label(uint32_t *content, uint32_t content_width, uint32_t content_height,
                       int32_t x, int32_t y, const char *text, uint32_t color)
{
    content_draw_text(content, content_width, content_height, x, y, text, color);
}
