// ApexKernel/Includes/Drivers/Display/Framebuffer/FramebufferInternal.h
// ApexOS -- Shared internal state for the Framebuffer module
//
// g_fb/g_fb_ready/g_backbuffer are defined once in Framebuffer.c and
// shared via extern with FramebufferInit.c. Not part of the public
// Framebuffer.h API.

#ifndef APEX_DRIVERS_DISPLAY_FRAMEBUFFER_INTERNAL_H
#define APEX_DRIVERS_DISPLAY_FRAMEBUFFER_INTERNAL_H

#include "Core/Types.h"

#include "Drivers/Display/Framebuffer/Framebuffer.h"

extern fb_info_t g_fb;
extern bool g_fb_ready;

// Off-screen buffer all drawing targets -- see the double-buffering
// note in Framebuffer.h. Same layout as the real framebuffer (same
// pitch_pixels, so fb_put_pixel's indexing math is identical), just
// backed by heap memory instead of VRAM.
extern uint32_t *g_backbuffer;

#endif // APEX_DRIVERS_DISPLAY_FRAMEBUFFER_INTERNAL_H
