// apex-kernel/drivers/framebuffer.h
// ApexOS -- Linear framebuffer driver (VESA/VBE graphics mode)
//
// The actual mode detection and setup happens in the bootloader (see
// arch/x86_64/boot/stage2.asm: detect_vbe_mode), while still in real
// mode -- BIOS interrupts aren't available once the kernel is
// running in Long Mode. This driver only reads the result stage2
// left behind and maps/uses the framebuffer from there.
//
// VGA text mode (drivers/vga.h) is NOT replaced by this -- the two
// coexist. A machine with no suitable VBE mode simply never gets a
// working framebuffer (fb_init() returns false), and the kernel
// keeps using VGA text mode as it always has.
//
// Double buffering: fb_clear/fb_put_pixel (and everything built on
// top of them in framebuffer_draw.c) write to an off-screen buffer
// in normal RAM, not the real VRAM -- drawing a full frame there
// causes no visible flicker since the monitor never sees a
// partially-drawn frame. fb_present() is the only function that
// touches real VRAM: one sequential copy of the whole back-buffer,
// called once per frame after all drawing for that frame is done.

#ifndef APEX_FRAMEBUFFER_H
#define APEX_FRAMEBUFFER_H

#include "../include/apex/types.h"

// Fixed address where stage2 wrote its mode-detection result (see
// stage2.asm: VBE_RESULT_ADDR). Must match that layout exactly.
#define FB_BOOT_INFO_ADDR 0x9300

typedef struct __attribute__((packed))
{
    uint32_t framebuffer_addr; // physical address (PhysBasePtr)
    uint16_t width;
    uint16_t height;
    uint16_t pitch; // bytes per scanline (can exceed width * 4 -- padding)
    uint8_t bpp;    // always 32 in v1 (see stage2.asm -- only 32bpp modes are considered)
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

// Fills the entire framebuffer with a single color.
void fb_clear(uint32_t color);

// Sets a single pixel. No bounds checking -- callers must stay within
// [0, width) x [0, height); out-of-range writes corrupt adjacent
// framebuffer memory (or, worst case, memory just past it).
void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);

// Reads a single pixel back from the back-buffer. No bounds checking,
// same contract as fb_put_pixel -- callers must stay within
// [0, width) x [0, height). Used by callers that need to save/restore
// a region before drawing over it (e.g. a mouse cursor, see mouse
// refresh code) rather than redrawing everything.
uint32_t fb_get_pixel(uint32_t x, uint32_t y);

// Copies the entire back-buffer to the real framebuffer (VRAM) in one
// sequential pass. Call this once per frame, after all drawing for
// that frame is done -- fb_clear/fb_put_pixel/fb_draw_* never touch
// VRAM directly, so nothing is visible on screen until fb_present()
// runs. No-op if fb_init() hasn't succeeded.
void fb_present(void);

#endif // APEX_FRAMEBUFFER_H