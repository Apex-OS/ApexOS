// ApexKernel/Drivers/Display/Framebuffer/Framebuffer.c
// ApexOS -- Linear framebuffer driver (VESA/VBE graphics mode)

#include "Drivers/Display/Framebuffer/Framebuffer.h"
#include "Drivers/Display/Framebuffer/FramebufferInternal.h"
#include "Drivers/Display/Fonts/Font8x16.h"

fb_info_t g_fb;
bool g_fb_ready = false;
uint32_t *g_backbuffer = NULL;

fb_info_t fb_get_info(void)
{
    return g_fb;
}

const fb_info_t *fb_get_info_ptr(void)
{
    return &g_fb;
}

void fb_clear(uint32_t color)
{
    if (!g_fb_ready)
        return;

    // Writes to the back-buffer only -- nothing visible changes until
    // the next fb_present().
    for (uint32_t y = 0; y < g_fb.height; y++)
    {
        uint32_t *row = g_backbuffer + y * g_fb.pitch_pixels;
        for (uint32_t x = 0; x < g_fb.width; x++)
            row[x] = color;
    }
}

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (!g_fb_ready)
        return;

    g_backbuffer[y * g_fb.pitch_pixels + x] = color;
}

uint32_t fb_get_pixel(uint32_t x, uint32_t y)
{
    if (!g_fb_ready)
        return 0;

    return g_backbuffer[y * g_fb.pitch_pixels + x];
}

void fb_present(void)
{
    if (!g_fb_ready)
        return;

    // Sequential copy, back-buffer -> real VRAM. One pass over
    // contiguous memory (pitch_pixels * height words) rather than a
    // pixel-by-pixel loop -- there's no per-pixel logic needed here
    // (unlike fb_put_pixel, both buffers share the exact same layout)
    // so a flat word copy is both simpler and faster.
    size_t words = (size_t)g_fb.pitch_pixels * g_fb.height;
    for (size_t i = 0; i < words; i++)
        g_fb.addr[i] = g_backbuffer[i];

    // The framebuffer's physical range is marked Write-Combining (see
    // Arch/x86_64/MTRR.c) -- WC writes are weakly ordered and can sit
    // in the CPU's write-combining buffer for a few cycles before
    // actually reaching VRAM. sfence forces everything written above
    // to be ordered ahead of whatever runs after this function
    // returns (an IRQ handler, the next frame's writes, etc.) --
    // without it a caller that assumes "fb_present() returned, so the
    // frame is on screen" isn't actually guaranteed that on WC memory.
    // A no-op in practice on plain WB memory (if MTRR setup failed
    // and this range stayed WB), so unconditionally safe to always
    // issue. NOTE: measured under QEMU/TCG (no KVM) -- WC showed no
    // measurable difference here (~168M vs ~167M cycles, within
    // measurement noise), because a software-emulated memory bus has
    // no real write-combining hardware to accelerate. This should
    // still help on real hardware (or QEMU+KVM), where the combining
    // buffer is physical silicon -- but that's unverified, not
    // measured, unlike the scroll fast-path's confirmed ~33-35% gain.
    __asm__ volatile("sfence" ::: "memory");
}

void fb_present_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    if (!g_fb_ready)
        return;

    if (x >= g_fb.width || y >= g_fb.height)
        return;
    if (x + w > g_fb.width)
        w = g_fb.width - x;
    if (y + h > g_fb.height)
        h = g_fb.height - y;

    // Row-by-row copy, only the changed rectangle -- e.g. a single
    // 16x16 text cell is 1024 pixels instead of the ~1.44M pixels a
    // full fb_present() would copy at 1600x900. This is what makes
    // per-character terminal updates (see Drivers/Display/Console/)
    // fast enough to keep up with typing.
    for (uint32_t row = 0; row < h; row++)
    {
        uint32_t *src = g_backbuffer + (y + row) * g_fb.pitch_pixels + x;
        uint32_t *dst = g_fb.addr + (y + row) * g_fb.pitch_pixels + x;
        for (uint32_t col = 0; col < w; col++)
            dst[col] = src[col];
    }

    // See fb_present()'s matching comment -- same WC ordering
    // guarantee, needed here too since this writes to the same VRAM
    // range.
    __asm__ volatile("sfence" ::: "memory");
}

void fb_scroll_up(uint32_t pixel_rows, uint32_t fill_color)
{
    if (!g_fb_ready)
        return;

    if (pixel_rows == 0)
        return;
    if (pixel_rows >= g_fb.height)
    {
        fb_clear(fill_color);
        return;
    }

    // In-place shift of the whole back-buffer, `pixel_rows` scanlines
    // up. dst is always a strictly lower address than src here (row Y
    // moves to row Y - pixel_rows), so a single forward pass (lowest
    // address written first) is safe -- by the time a source word is
    // overwritten as someone else's destination, it's already been
    // read. This moves entire scanlines (pitch_pixels wide, not just
    // width), so any pitch padding rides along unchanged -- simpler
    // and no less correct than special-casing it out.
    uint32_t rows_remaining = g_fb.height - pixel_rows;
    size_t shift_words = (size_t)pixel_rows * g_fb.pitch_pixels;
    size_t words_to_move = (size_t)rows_remaining * g_fb.pitch_pixels;

    for (size_t i = 0; i < words_to_move; i++)
        g_backbuffer[i] = g_backbuffer[i + shift_words];

    // Fill the newly-exposed rows at the bottom -- the caller (e.g.
    // the console) is expected to draw real content over these right
    // after (the new blank line's cells), but they still need a
    // defined color in the meantime rather than stale duplicated
    // pixels from the row above.
    size_t fill_words = (size_t)pixel_rows * g_fb.pitch_pixels;
    for (size_t i = 0; i < fill_words; i++)
        g_backbuffer[words_to_move + i] = fill_color;
}

void fb_draw_glyph_cell(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg)
{
    if (!g_fb_ready)
        return;

    // Clip once for the whole 8x16 cell -- a text cell is either
    // fully on-screen or (rare: only the last partial row/column at
    // the console's right/bottom edge) needs the per-row/col bounds
    // below anyway. A cell starting fully off-screen is the only case
    // worth bailing out of early.
    if (x >= g_fb.width || y >= g_fb.height)
        return;

    uint32_t max_col = FONT8X16_WIDTH;
    uint32_t max_row = FONT8X16_HEIGHT;
    if (x + max_col > g_fb.width)
        max_col = g_fb.width - x;
    if (y + max_row > g_fb.height)
        max_row = g_fb.height - y;

    const uint8_t *glyph = font8x16_get_glyph(c);

    // Direct pointer indexing instead of fb_put_pixel() per pixel:
    // row_ptr is computed once per scanline (one multiply for the
    // whole row, not one per pixel) and col writes advance it with a
    // plain increment. This is the actual saving over the previous
    // fb_put_pixel()-based version -- both write the same 128 pixels
    // exactly once each (see fb_draw_glyph_cell's header comment),
    // this just removes 128 redundant offset recomputations and
    // function-call overheads per cell (real cost at this codebase's
    // -O0 build, where nothing gets inlined).
    for (uint32_t row = 0; row < max_row; row++)
    {
        uint8_t bits = glyph[row];
        uint32_t *row_ptr = g_backbuffer + (y + row) * g_fb.pitch_pixels + x;
        for (uint32_t col = 0; col < max_col; col++)
            row_ptr[col] = (bits & (0x80 >> col)) ? fg : bg;
    }
}
