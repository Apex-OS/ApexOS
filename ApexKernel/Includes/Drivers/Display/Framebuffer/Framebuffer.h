// ApexKernel/Includes/Drivers/Display/Framebuffer/Framebuffer.h
// ApexOS -- Linear framebuffer driver (VESA/VBE graphics mode)
//
// The actual mode detection and setup happens in the bootloader (see
// ApexKernel/Boot/BIOS/Transition/HardwareDetect.asm: detect_vbe_mode),
// while still in real mode -- BIOS interrupts aren't available once
// the kernel is running in Long Mode. This driver only reads the
// result stage2 left behind and maps/uses the framebuffer from there.
//
// VGA text mode (Drivers/Display/VGA.h) is NOT replaced by this --
// the two coexist. A machine with no suitable VBE mode simply never
// gets a working framebuffer (fb_init() returns false), and the
// kernel keeps using VGA text mode as it always has.
//
// Double buffering: fb_clear/fb_put_pixel (and everything built on
// top of them in Drivers/Display/Drawing/) write to an off-screen
// buffer in normal RAM, not the real VRAM -- drawing a full frame
// there causes no visible flicker since the monitor never sees a
// partially-drawn frame. fb_present() is the only function that
// touches real VRAM: one sequential copy of the whole back-buffer,
// called once per frame after all drawing for that frame is done.

#ifndef APEX_DRIVERS_DISPLAY_FRAMEBUFFER_H
#define APEX_DRIVERS_DISPLAY_FRAMEBUFFER_H

#include "Core/Types.h"

// Fixed address where stage2 wrote its mode-detection result (see
// HardwareDetect.asm: VBE_RESULT_ADDR). Must match that layout exactly.
#define FB_BOOT_INFO_ADDR 0x9300

typedef struct __attribute__((packed))
{
    uint32_t framebuffer_addr; // physical address (PhysBasePtr)
    uint16_t width;
    uint16_t height;
    uint16_t pitch; // bytes per scanline (can exceed width * 4 -- padding)
    uint8_t bpp;    // always 32 in v1 (see HardwareDetect.asm -- only 32bpp modes are considered)
    uint8_t valid;  // 0 = no suitable VBE mode was found at boot
} fb_boot_info_t;

typedef struct
{
    uint32_t *addr; // virtual address the framebuffer is mapped at (== physical in v1, identity-mapped)
    uint32_t width;
    uint32_t height;
    uint32_t pitch_pixels; // pitch converted from bytes to pixels (pitch / 4), for indexing addr[]
} fb_info_t;

// 0xRRGGBB -- alpha ignored (always opaque in v1).
#define FB_RGB(r, g, b) ((uint32_t)(((r) << 16) | ((g) << 8) | (b)))

// Reads the boot-time VBE result and maps the framebuffer into the
// kernel's address space (identity-mapped, VMM_KERNEL_RW -- the
// physical address VESA handed back becomes the virtual address used
// to write pixels). Returns false if stage2 didn't find a suitable
// mode (fb_boot_info_t.valid == 0) -- callers must check the return
// value before touching fb_get_info() or drawing anything; VGA text
// mode is unaffected either way.
bool fb_init(void);

// Returns the current framebuffer geometry. Only valid after a
// successful fb_init().
fb_info_t fb_get_info(void);

// Same data as fb_get_info(), but returns a pointer to the live
// fb_info_t instead of a copy -- fb_info_t is 20 bytes (over the
// 16-byte limit for returning a struct in registers on x86-64 System
// V), so fb_get_info() returns via a hidden out-parameter the
// compiler manages; that's fine for occasional calls, but callers
// that invoke it many times per frame (e.g. clip_rect() in
// Drivers/Display/Drawing/Shapes.c, called for every glyph fill in
// Console text rendering -- thousands of times per frame) should use
// this instead to skip the copy. Never NULL after a successful
// fb_init(); result is read-only, don't modify through it.
const fb_info_t *fb_get_info_ptr(void);

// Fills the entire framebuffer with a single color.
void fb_clear(uint32_t color);

// Sets a single pixel. No bounds checking -- callers must stay within
// [0, width) x [0, height); out-of-range writes corrupt adjacent
// framebuffer memory (or, worst case, memory just past it).
void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);

// Reads a single pixel back from the back-buffer. No bounds checking,
// same contract as fb_put_pixel -- callers must stay within
// [0, width) x [0, height). Used by callers that need to save/restore
// a region before drawing over it (e.g. a mouse cursor) rather than
// redrawing everything.
uint32_t fb_get_pixel(uint32_t x, uint32_t y);

// Copies the entire back-buffer to the real framebuffer (VRAM) in one
// sequential pass. Call this once per frame, after all drawing for
// that frame is done -- fb_clear/fb_put_pixel/fb_draw_* never touch
// VRAM directly, so nothing is visible on screen until fb_present()
// runs. No-op if fb_init() hasn't succeeded.
void fb_present(void);

// Same as fb_present(), but only copies the given rectangle instead
// of the whole screen. Use this whenever only a small region actually
// changed (a single character cell, a cursor) -- fb_present() copying
// the full back-buffer (up to ~5.76MB at 1600x900) on every keystroke
// is the difference between a terminal that keeps up with typing and
// one that visibly lags behind it. Clipped to the framebuffer bounds,
// same as fb_fill_rect -- safe to call with a rect that runs off an
// edge.
void fb_present_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h);

// Scrolls the back-buffer's contents up by `pixel_rows` scanlines:
// shifts every row still on-screen up by that amount (row Y becomes
// row Y - pixel_rows), and clears the `pixel_rows` newly-exposed rows
// at the bottom to `fill_color`. Implemented as a single memmove over
// the whole back-buffer (safe even with pitch padding, since it moves
// entire scanlines including any padding) rather than redrawing every
// on-screen cell from source data -- turns an O(width * height)
// redraw into an O(width * height) *memmove* (touches the same amount
// of memory once, sequentially, instead of recomputing every pixel
// from font data) plus a small fill for the exposed rows. Does not
// call fb_present()/fb_present_rect() itself -- callers still need to
// push the result to VRAM. No-op if pixel_rows >= height (nothing
// would remain on screen; caller should fb_clear() instead).
void fb_scroll_up(uint32_t pixel_rows, uint32_t fill_color);

// Draws one native-size (8x16, unscaled) glyph cell directly into the
// back-buffer -- background fill and foreground glyph pixels in a
// single pass, writing through an incrementing pointer instead of 128
// separate fb_put_pixel() calls (each of which re-derives its offset
// via a multiply and pays a full call/ret at this codebase's -O0 build
// -- see Console/ConsoleRender.c: fbcon_draw_cell(), the only caller,
// called for every character the console ever prints). x/y are the
// cell's top-left corner. Clipped to the framebuffer bounds (checked
// once per cell, not per pixel) -- safe to call with a cell that runs
// off an edge, same as fb_fill_rect. Lives here rather than in
// Drawing/Text.c because it needs direct back-buffer indexing
// (g_backbuffer/pitch_pixels), which is Framebuffer-internal state --
// same reasoning as fb_scroll_up() above.
void fb_draw_glyph_cell(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);

#endif // APEX_DRIVERS_DISPLAY_FRAMEBUFFER_H
