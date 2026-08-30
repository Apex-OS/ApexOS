// ApexKernel/Drivers/Display/Drawing/Text.c
// ApexOS -- Framebuffer text rendering

#include "Drivers/Display/Drawing/Text.h"
#include "Drivers/Display/Drawing/Shapes.h"
#include "Drivers/Display/Framebuffer/Framebuffer.h"
#include "Drivers/Display/Fonts/Font8x16.h"

void fb_draw_text(uint32_t x, uint32_t y, const char *text, uint32_t scale, uint32_t color)
{
    if (scale == 0)
        scale = 1;

    const fb_info_t *info = fb_get_info_ptr();
    uint32_t cursor_x = x;

    for (const char *p = text; *p; p++)
    {
        const uint8_t *glyph = font8x16_get_glyph(*p);

        for (uint32_t row = 0; row < FONT8X16_HEIGHT; row++)
        {
            uint8_t bits = glyph[row];
            for (uint32_t col = 0; col < FONT8X16_WIDTH; col++)
            {
                if (!(bits & (0x80 >> col)))
                    continue; // pixel off -- background left untouched (no per-glyph clear)

                // Each source pixel becomes a `scale`x`scale` block --
                // draws it as a small filled rect instead of one
                // fb_put_pixel call per output pixel, cheaper for
                // scale > 1.
                uint32_t px = cursor_x + col * scale;
                uint32_t py = y + row * scale;
                if (px >= info->width || py >= info->height)
                    continue;
                fb_fill_rect(px, py, scale, scale, color);
            }
        }

        cursor_x += FONT8X16_WIDTH * scale;
    }
}

// fb_draw_glyph_cell() moved to Framebuffer.c -- see Framebuffer.h's
// comment on the declaration for why.
