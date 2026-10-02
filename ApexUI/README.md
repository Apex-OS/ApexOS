# ApexUI

ApexOS's graphical layer: a compositor, a window manager, input handling,
a taskbar, and two basic widgets, all drawn directly to the VBE/GOP
framebuffer. Early (v1), in-kernel, and single-process for now — see
"Current scope" below for exactly what that means.

See the root README's [screenshots](../README.md#screenshots) for what this
looks like running.

## Layout

```
Core/
  Compositor/       Turns the window list into pixels: background, every
                    window back-to-front, the taskbar, the cursor
  Input/            Bridges raw PS/2 mouse/keyboard state to window
                    manager operations (focus, drag, taskbar clicks)
  WindowManager/     Owns the set of open windows and their position,
                    size, z-order and focus — not their pixels
  Display/          Planned, not started
  Renderer/         Planned, not started
  Surfaces/         Planned, not started
Taskbar/            The bottom strip listing open windows as buttons
Widgets/            Two widgets (button, label), drawn directly into a
                    window's content buffer
API/                Planned: a stable surface for userspace programs to
                    open their own windows (Graphics/Input/Window
                    subdirectories reserved, nothing implemented yet)
Graphics/           Planned: color/geometry/image/pixel helpers, not
                    started — Compositor.c and Widgets.c currently call
                    Drivers/Display/Drawing/ (ApexKernel) directly
Resources/          Planned: cursors/fonts/icons/images/themes as actual
                    asset files, not started — current visuals are plain
                    fills and the kernel's built-in bitmap font
Includes/           Headers, mirroring the structure above (see
                    ApexKernel's README for why headers are split out
                    this way)
```

## Current scope (v1)

Worth reading before touching this code, because it shapes a few
decisions that aren't obvious from the API alone:

- **Windows are created in-kernel, with hardcoded content.** There is no
  syscall yet for a separate Ring 3 process to open its own window. The
  data model (`window_t` in `WindowManager.h`) is already shaped for that
  — each window owns its own pixel buffer and is addressed by a stable
  `window_id_t` rather than a raw pointer — specifically so that adding
  such a syscall later means exposing this API, not redesigning it.
- **No window content interaction yet.** Dragging a window by its titlebar
  and clicking taskbar entries work; clicking inside a window's content
  area, resizing, and closing via a titlebar button don't. `Input.c`'s
  main-loop call site is where that would be extended once widgets need to
  receive their own click events.
- **Widgets are drawing functions, not a retained widget tree.** A widget
  here is a `widget_draw_*()` call plus, for the button, a separate
  `widget_button_hit_test()` — there's no layout engine or event dispatch.
  The caller decides when to check a hit test, typically against a click
  already known to have landed inside a window's content area. This is
  deliberately minimal rather than a real widget toolkit; see
  `Widgets/Widgets.h`'s header comment for when it'd be worth building one.
- **Two redraw paths exist for a reason.** `compositor_draw_frame()`
  redraws the whole screen; `compositor_draw_frame_dirty()` redraws just a
  changed rectangle. A focus/z-order change (which can reveal or hide parts
  of any window, not just the one clicked) needs the full redraw —
  `Input.h`'s `ui_needs_full_redraw()` says which case applies after a given
  `ui_input_poll()`. Skipping this distinction was tried first and made
  window dragging visibly less smooth on a 1600x900 framebuffer; see
  `Compositor.h`'s header comment.
- **The taskbar draws straight to the framebuffer, not through
  `Widgets/`.** Its buttons use `fb_fill_rect()`/`fb_draw_text()` directly
  (from `ApexKernel/Drivers/Display/Drawing/`) because it isn't drawing
  into a window's content buffer the way widgets do — don't take it as a
  model for how a window's own content should be drawn.

## Planned, not started yet

`Core/Display/`, `Core/Renderer/`, `Core/Surfaces/`, all of `API/`, all of
`Graphics/`, and all of `Resources/` are reserved directories with no code
yet. In rough order of what they'd likely unblock: `API/` (a real syscall
surface for userspace windows) → `Resources/Fonts` and `Resources/Themes`
(move off the kernel's built-in bitmap font and hardcoded colors) →
`Graphics/` (stop reaching into `ApexKernel/Drivers/Display/Drawing/`
directly) → `Core/Renderer` and `Core/Surfaces` (whatever abstraction
makes the first three not need to know about raw framebuffer pixels).

Nothing above is a committed design — it's where that code will live once
it exists, not a spec for what it must look like.
