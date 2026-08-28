// ApexKernel/Core/Shell/Commands/DiagnosticCommands.c
// ApexOS -- Shell hardware diagnostic/test commands

#include "Core/Shell/Commands/DiagnosticCommands.h"
#include "Drivers/Display/Framebuffer/Framebuffer.h"
#include "Drivers/Display/Console/ConsoleCore.h"
#include "Drivers/Display/Drawing/Shapes.h"
#include "Drivers/Display/Drawing/Text.h"
#include "Drivers/Display/Drawing/Cursor.h"
#include "Drivers/Display/VGA.h"
#include "Drivers/Input/Mouse/Mouse.h"
#include "Drivers/Input/Keyboard/Keyboard.h"
#include "Drivers/Input/Keyboard/KeyBuffer.h"
#include "Drivers/Storage/AHCI/AHCI.h"
#include "Drivers/Storage/AHCI/Controller.h"
#include "Logs/Log.h"

// Smoke-tests the VESA/VBE framebuffer path (see
// ApexKernel/Drivers/Display/Framebuffer/ and
// ApexKernel/Boot/BIOS/Transition/HardwareDetect.asm: detect_vbe_mode).
// One-way in v1 -- there is no switch back to VGA text mode yet, since
// the shell that would print any follow-up message isn't rendered in
// graphics mode either. Reset the machine (or `make run` again) to get
// back to a normal boot.
// Draws the static scene (background + shapes + text) once. The
// cursor is handled separately via fb_save_cursor_bg()/
// fb_restore_cursor_bg() (dirty-rectangle update) in cmd_gfxtest, so
// this never needs to run again after the first frame -- only the
// small block of pixels under the cursor changes per frame.
static void gfxtest_draw_scene(fb_info_t info)
{
    fb_clear(FB_RGB(30, 60, 114)); // solid dark blue background

    fb_fill_rect(50, 50, 200, 120, FB_RGB(220, 80, 60));
    fb_draw_rect(300, 50, 200, 120, 6, FB_RGB(240, 200, 60));
    fb_draw_line(50, 220, info.width - 50, 220, FB_RGB(255, 255, 255));
    fb_draw_line(50, 220, 300, info.height - 50, FB_RGB(120, 220, 140));

    fb_draw_text(50, 250, "ApexOS Graphics Mode", 2, FB_RGB(255, 255, 255));
    fb_draw_text(50, 280, "Hello, framebuffer!", 1, FB_RGB(200, 200, 255));
    fb_draw_text(50, 300, "Move the mouse -- any key to exit", 1, FB_RGB(160, 160, 200));
}

void cmd_gfxtest(void)
{
    // TEST TEMPORAIRE -- fb_init() est maintenant aussi appelée au
    // boot (voir Core/Init.c), donc la rappeler ici double-alloue le
    // back-buffer et fait échouer kmalloc. fb_get_info() retourne des
    // valeurs à zéro si jamais fb_init() n'a jamais réussi, donc ce
    // check est un moyen simple de savoir si on peut sauter le second
    // appel. A restaurer une fois ce test terminé.
    fb_info_t already = fb_get_info();
    if (already.width == 0 && !fb_init())
    {
        klog_error("gfxtest: no usable graphics mode -- staying in text mode");
        return;
    }

    fb_info_t info = fb_get_info();
    klog_ok_n("gfxtest: resolution width", info.width);
    klog_ok_n("gfxtest: resolution height", info.height);

    // Clamps the tracked mouse position to the real screen bounds now
    // that we know them -- before this, Mouse.c tracks position
    // unclamped (harmless, nothing was reading it to draw a cursor).
    mouse_set_bounds((int32_t)info.width - 1, (int32_t)info.height - 1);

    gfxtest_draw_scene(info);

    mouse_state_t last_state = mouse_get_state();
    fb_save_cursor_bg((uint32_t)last_state.x, (uint32_t)last_state.y);
    fb_draw_cursor((uint32_t)last_state.x, (uint32_t)last_state.y, FB_RGB(255, 255, 255));
    fb_present(); // first frame: full copy is fine, happens once

    // Exits on any keypress -- no mode-switch back to VGA text yet
    // (see the file-level comment on cmd_gfxtest's one-way nature),
    // this just stops redrawing so the last frame stays visible.
    while (!keyboard_has_key())
    {
        mouse_state_t state = mouse_get_state();
        if (state.x != last_state.x || state.y != last_state.y)
        {
            // Dirty-rectangle cursor update: erase the old cursor by
            // restoring what was under it, THEN save what's under the
            // new position before drawing there -- the static scene
            // itself is never touched again after the initial draw
            // above, so this is the only work done per frame.
            fb_restore_cursor_bg((uint32_t)last_state.x, (uint32_t)last_state.y);
            fb_save_cursor_bg((uint32_t)state.x, (uint32_t)state.y);
            fb_draw_cursor((uint32_t)state.x, (uint32_t)state.y, FB_RGB(255, 255, 255));

            // Presents only the bounding box covering both the old and
            // new cursor position, instead of the whole screen -- at
            // 1600x900 fb_present() was copying ~5.76MB every single
            // mouse-move frame, capping this well under 60fps. The
            // union of both FB_CURSOR_W x FB_CURSOR_H boxes is at most
            // a few thousand pixels, however far the mouse moved in
            // one frame.
            int32_t min_x = last_state.x < state.x ? last_state.x : state.x;
            int32_t min_y = last_state.y < state.y ? last_state.y : state.y;
            int32_t max_x = last_state.x > state.x ? last_state.x : state.x;
            int32_t max_y = last_state.y > state.y ? last_state.y : state.y;
            fb_present_rect((uint32_t)min_x, (uint32_t)min_y,
                            (uint32_t)(max_x - min_x) + FB_CURSOR_W,
                            (uint32_t)(max_y - min_y) + FB_CURSOR_H);

            last_state = state;
        }
    }
    keyboard_getchar(); // consumes the keypress that ended the loop

    // Clears the framebuffer console back to a blank screen before
    // returning to the shell -- with fbcon_putchar() now only
    // presenting the cell it actually touches (see fb_present_rect()
    // in Framebuffer.c), nothing else repaints the rest of the screen
    // automatically the way the old full-screen fb_present() used to.
    // Without this, the shell's next prompt would just get drawn on
    // top of gfxtest's last frame instead of a clean background.
    if (vga_is_graphics_mode())
        fbcon_clear();
}

void cmd_disktest(void)
{
    ahci_drive_info_t info = ahci_drive_info();
    if (!info.ready)
    {
        klog_error("disktest: no AHCI drive ready -- run this after a successful boot with a real disk");
        return;
    }

    uint8_t sector[512];
    if (!ahci_read_sectors(0, 1, sector))
    {
        klog_error("disktest: ahci_read_sectors(0, 1) failed");
        return;
    }

    // Every valid boot sector (sector 0 on a bootable disk) ends with
    // this exact two-byte signature -- it's what the BIOS itself
    // checks before handing control to a disk's boot code. Finding it
    // at the right offset here confirms the AHCI DMA read actually
    // pulled real disk data, not just that the command completed
    // without error.
    bool signature_ok = (sector[510] == 0x55 && sector[511] == 0xAA);

    klog_ok_prefix();
    vga_puts("disktest: sector 0 boot signature ");
    vga_puts(signature_ok ? "OK (0x55 0xAA found)" : "MISSING -- read may be wrong");
    vga_puts("\n");

    // Diagnostic: show what was actually read, to tell "wrong sector"
    // apart from "corrupted data" apart from "read nothing at all".
    klog_ok_prefix();
    vga_puts("disktest: first 8 bytes: ");
    for (int i = 0; i < 8; i++)
    {
        vga_puthex(sector[i]);
        vga_puts(" ");
    }
    vga_puts("\n");
    klog_ok_prefix();
    vga_puts("disktest: bytes 508-511: ");
    for (int i = 508; i < 512; i++)
    {
        vga_puthex(sector[i]);
        vga_puts(" ");
    }
    vga_puts("\n");

    // Write/read-back test on a sector far from anything that
    // matters -- Stage1 (sector 0), Stage2 (sectors 1-32), and the
    // kernel (sectors 34 through roughly 234, see
    // ApexKernel/Boot/BIOS/Stage2.asm: KERNEL_SECTORS) all live well
    // before sector 1000, so writing here can't corrupt anything the
    // bootloader or this running kernel depend on.
    const uint64_t test_lba = 1000;
    uint8_t write_buf[512];
    for (int i = 0; i < 512; i++)
        write_buf[i] = (uint8_t)(i & 0xFF); // simple repeating 0x00-0xFF pattern

    if (!ahci_write_sectors(test_lba, 1, write_buf))
    {
        klog_error("disktest: ahci_write_sectors failed");
        return;
    }

    uint8_t readback_buf[512];
    if (!ahci_read_sectors(test_lba, 1, readback_buf))
    {
        klog_error("disktest: read-back after write failed");
        ahci_port_debug_t dbg = ahci_port_debug_info();
        klog_ok_prefix();
        vga_puts("disktest: port tfd=");
        vga_puthex(dbg.tfd);
        vga_puts(" is=");
        vga_puthex(dbg.is);
        vga_puts(" serr=");
        vga_puthex(dbg.serr);
        vga_puts(" ci=");
        vga_puthex(dbg.ci);
        vga_puts(" sact=");
        vga_puthex(dbg.sact);
        vga_puts(" cmd=");
        vga_puthex(dbg.cmd);
        vga_puts("\n");
        return;
    }

    bool write_ok = true;
    for (int i = 0; i < 512; i++)
    {
        if (readback_buf[i] != write_buf[i])
        {
            write_ok = false;
            break;
        }
    }

    klog_ok_prefix();
    vga_puts("disktest: write+read-back at sector 1000 ");
    vga_puts(write_ok ? "OK (data matches)" : "MISMATCH");
    vga_puts("\n");
}
