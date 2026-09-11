// ApexKernel/Core/Shell/Commands/DiagnosticCommands.c
// ApexOS -- Shell hardware diagnostic/test commands

#include "Core/Shell/Commands/DiagnosticCommands.h"
#include "Drivers/Display/Framebuffer/Framebuffer.h"
#include "Drivers/Display/Console/ConsoleCore.h"
#include "Drivers/Display/Console/ConsoleRender.h"
#include "Drivers/Display/Drawing/Shapes.h"
#include "Drivers/Display/Drawing/Text.h"
#include "Drivers/Display/Drawing/Cursor.h"
#include "Drivers/Display/VGA.h"
#include "Drivers/Input/Mouse/Mouse.h"
#include "Drivers/Input/Keyboard/Keyboard.h"
#include "Drivers/Input/Keyboard/KeyBuffer.h"
#include "Drivers/Storage/AHCI/AHCI.h"
#include "Drivers/Storage/AHCI/Controller.h"
#include "Core/WindowManager/WindowManager.h"
#include "Core/Compositor/Compositor.h"
#include "Core/Input/Input.h"
#include "Widgets/Widgets.h"
#include "Logs/Log.h"
#include "Network/Protocols/Ethernet/Ethernet.h"
#include "Network/Protocols/ARP/ARP.h"
#include "Network/Protocols/IPv4/IPv4.h"
#include "Network/Protocols/ICMP/ICMP.h"
#include "Network/Protocols/UDP/UDP.h"
#include "Network/Protocols/TCP/TCP.h"
#include "Core/Time/PITCounter.h"
#include "Core/Shell/PathResolver.h"

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

    // Redraws the shell's last on-screen state from the untouched
    // scrollback buffer before returning -- gfxtest draws straight to
    // the framebuffer without ever touching the console's scrollback,
    // so this restores exactly what was there before gfxtest ran
    // instead of wiping the whole history (fbcon_clear() would do
    // that). Without this, the shell's next prompt would just get
    // drawn on top of gfxtest's last frame instead of a clean
    // background.
    //
    // fb_clear() first: grid_height*FBCON_GLYPH_H doesn't necessarily
    // cover the full framebuffer height (integer division), so a thin
    // strip at the bottom of gfxtest's scene would otherwise survive
    // fbcon_present() untouched -- fbcon_present() only redraws whole
    // text cells, not stray pixel rows below the last one.
    if (vga_is_graphics_mode())
    {
        fb_clear(FB_RGB(0, 0, 0));
        fbcon_present_force();
    }
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

// Parses "A.B.C.D" into 4 bytes. No error reporting beyond a bool --
// this is a throwaway test command's input parser (see
// DiagnosticCommands.h's header comment on cmd_arptest), not a
// general-purpose one worth a shared StringHelpers.c entry yet.
static bool parse_ipv4(const char *s, uint8_t out_ip[4])
{
    int part = 0;
    int value = 0;
    bool any_digit = false;

    for (;; s++)
    {
        if (*s >= '0' && *s <= '9')
        {
            value = value * 10 + (*s - '0');
            any_digit = true;
            if (value > 255)
                return false;
        }
        else if (*s == '.' || *s == '\0')
        {
            if (!any_digit || part > 3)
                return false;
            out_ip[part++] = (uint8_t)value;
            value = 0;
            any_digit = false;
            if (*s == '\0')
                break;
        }
        else
            return false;
    }

    return part == 4;
}

static void print_mac(const uint8_t mac[ETH_MAC_LEN])
{
    for (int i = 0; i < ETH_MAC_LEN; i++)
    {
        vga_puthex(mac[i]);
        if (i < ETH_MAC_LEN - 1)
            vga_puts(":");
    }
}

// TEST TEMPORAIRE (réseau) -- see DiagnosticCommands.h's header
// comment. Resolves one IPv4 address via ARP and prints the result:
// exercises Network/Protocols/Ethernet/Ethernet.c and
// Network/Protocols/ARP/ARP.c end to end (frame construction, real
// e1000 TX, polling RX, ARP request/reply parsing) against a real
// peer (QEMU's own "-netdev user" gateway, 10.0.2.2, always present
// and always answers ARP -- see the Makefile's `run` target).
void cmd_arptest(const char *args)
{
    uint8_t target_ip[4] = {10, 0, 2, 2}; // QEMU user-mode gateway, default target

    if (args[0] != '\0' && !parse_ipv4(args, target_ip))
    {
        klog_error("arptest: usage: arptest [A.B.C.D] (bad IP format)");
        return;
    }

    // This kernel's own IPv4 address -- hardcoded for this test (no
    // DHCP/static config yet, see the roadmap's IPv4 step). 10.0.2.15
    // is QEMU user-mode networking's default guest address, so ARP
    // replies from the gateway are addressed back to something real.
    uint8_t own_ip[4] = {10, 0, 2, 15};

    eth_init();
    arp_init(own_ip);

    uint8_t own_mac[ETH_MAC_LEN];
    eth_get_mac(own_mac);
    klog_ok_prefix();
    vga_puts("arptest: own MAC ");
    print_mac(own_mac);
    vga_puts("\n");

    klog_ok_prefix();
    vga_puts("arptest: resolving ");
    vga_putdec(target_ip[0]);
    vga_puts(".");
    vga_putdec(target_ip[1]);
    vga_puts(".");
    vga_putdec(target_ip[2]);
    vga_puts(".");
    vga_putdec(target_ip[3]);
    vga_puts(" (timeout 2000ms)...\n");

    uint8_t resolved_mac[ETH_MAC_LEN];
    if (arp_resolve(target_ip, resolved_mac, 2000))
    {
        klog_ok_prefix();
        vga_puts("arptest: resolved to ");
        print_mac(resolved_mac);
        vga_puts("\n");
    }
    else
        klog_error("arptest: timed out -- no ARP reply received");
}

// Parses a single non-negative decimal integer. No error reporting
// beyond a bool -- same throwaway-test-input reasoning as
// parse_ipv4() above.
static bool parse_uint(const char *s, uint32_t *out)
{
    if (*s == '\0')
        return false;

    uint32_t value = 0;
    for (; *s; s++)
    {
        if (*s < '0' || *s > '9')
            return false;
        value = value * 10 + (uint32_t)(*s - '0');
    }
    *out = value;
    return true;
}

// Splits "A.B.C.D [count]" into its IP and optional count part --
// `ip_out`/`ip_out_size` receives the IP substring (for parse_ipv4),
// `count_out` receives the trailing digits if present or NULL-ish
// (empty string) if not. Mirrors CommandDispatcher.c's own
// tokenize_command() approach (skip leading/trailing spaces, split on
// the first run of spaces) rather than pulling in a shared tokenizer
// for what's still just one throwaway test command's argument
// parsing.
static void split_ping_args(const char *args, char *ip_out, uint32_t ip_out_size, char **count_out)
{
    while (*args == ' ')
        args++;

    uint32_t i = 0;
    while (args[i] && args[i] != ' ' && i < ip_out_size - 1)
    {
        ip_out[i] = args[i];
        i++;
    }
    ip_out[i] = '\0';

    args += i;
    while (*args == ' ')
        args++;

    *count_out = (char *)args;
}

// TEST/COMMANDE RÉSEAU -- ping via ICMP Echo Request/Reply, see
// Network/Protocols/ICMP/ICMP.h. Exercises the full stack end to end:
// Ethernet + ARP (address resolution) + IPv4 (addressing, checksum)
// + ICMP (the actual echo). This is the project roadmap's final
// milestone for the networking step (e1000 -> Ethernet -> ARP -> IPv4
// -> ICMP -> ping).
void cmd_ping(const char *args)
{
    char ip_str[32];
    char *count_str;
    split_ping_args(args, ip_str, sizeof(ip_str), &count_str);

    uint8_t target_ip[4] = {10, 0, 2, 2}; // QEMU user-mode gateway, default target
    if (ip_str[0] != '\0' && !parse_ipv4(ip_str, target_ip))
    {
        klog_error("ping: usage: ping [A.B.C.D] [count] (bad IP format)");
        return;
    }

    uint32_t count = 4;
    if (count_str[0] != '\0' && (!parse_uint(count_str, &count) || count == 0))
    {
        klog_error("ping: usage: ping [A.B.C.D] [count] (bad count)");
        return;
    }

    // Same hardcoded local address as cmd_arptest -- no DHCP/static
    // config yet (see the roadmap's IPv4 step). eth_init()/arp_init()
    // are idempotent enough to call again here (they just re-cache
    // the MAC / reset the ARP cache), so ping works standalone
    // without requiring arptest to have run first.
    uint8_t own_ip[4] = {10, 0, 2, 15};
    eth_init();
    arp_init(own_ip);
    ipv4_init(own_ip);
    uint8_t gateway_ip[4] = {10, 0, 2, 2}; // QEMU user-mode gateway -- see IPv4.h's header comment on routing scope
    ipv4_set_gateway(gateway_ip);

    klog_ok_prefix();
    vga_puts("ping: sending ");
    vga_putdec(count);
    vga_puts(" ICMP echo request(s) to ");
    vga_putdec(target_ip[0]);
    vga_puts(".");
    vga_putdec(target_ip[1]);
    vga_puts(".");
    vga_putdec(target_ip[2]);
    vga_puts(".");
    vga_putdec(target_ip[3]);
    vga_puts("\n");

    // Fixed 16-byte pattern payload -- standard ping tools use
    // something similar (Linux's ping fills incrementing bytes),
    // here just to have a non-empty, easily-recognizable echo body.
    // Not used for timing: round-trip time is measured with
    // pit_get_ticks() below, independent of payload content.
    uint8_t payload[16];
    for (int i = 0; i < 16; i++)
        payload[i] = (uint8_t)i;

    uint8_t rx_buffer[2048];
    uint16_t identifier = 0xBEEF; // fixed per this ping run, distinguishes our own requests from anyone else's
    uint32_t received = 0;

    for (uint16_t seq = 0; seq < (uint16_t)count; seq++)
    {
        uint64_t send_tick = pit_get_ticks();

        if (!icmp_send_echo_request(target_ip, identifier, seq, payload, sizeof(payload)))
        {
            klog_error_n("ping: seq failed to send", seq);
            continue;
        }

        // 1-second reply timeout per request, same
        // pit_get_ticks()-based pattern as arp_resolve() (see
        // ARP.c) -- a fresh timeout window per sequence number
        // rather than one shared budget for the whole run, so one
        // slow reply doesn't eat into the next request's chance to
        // be answered.
        bool got_reply = false;
        uint64_t timeout_ticks = 100; // 100 * 10ms = 1000ms

        while (pit_get_ticks() - send_tick <= timeout_ticks)
        {
            icmp_echo_reply_t reply;
            if (icmp_poll_receive(rx_buffer, &reply) &&
                reply.identifier == identifier && reply.sequence == seq)
            {
                uint64_t rtt_ticks = pit_get_ticks() - send_tick;
                klog_ok_prefix();
                vga_puts("ping: reply from ");
                vga_putdec(reply.src_ip[0]);
                vga_puts(".");
                vga_putdec(reply.src_ip[1]);
                vga_puts(".");
                vga_putdec(reply.src_ip[2]);
                vga_puts(".");
                vga_putdec(reply.src_ip[3]);
                vga_puts(": seq=");
                vga_putdec(seq);
                vga_puts(" time=");
                vga_putdec(rtt_ticks * 10); // ticks -> milliseconds (100Hz PIT, see Core/Time/PITCounter.h)
                vga_puts("ms\n");
                got_reply = true;
                received++;
                break;
            }
        }

        if (!got_reply)
        {
            klog_warn_n("ping: seq timed out", seq);
        }
    }

    klog_ok_prefix();
    vga_puts("ping: ");
    vga_putdec(received);
    vga_puts("/");
    vga_putdec(count);
    vga_puts(" replies received\n");
}

// Draws two demo windows' worth of content once, right after
// wm_create_window() -- content buffers start zeroed (see
// WindowManager.c: kcalloc) and are only ever redrawn by whatever
// owns them (there's no per-frame "redraw every window's content"
// step in Compositor.c -- see that file's header comment: it composes
// whatever is already in each window's buffer, it doesn't regenerate
// it). A real app would redraw its own content in response to its own
// events; this demo has nothing that changes after the initial draw,
// so once is enough.
static void apexui_draw_demo_content(window_id_t win1_id, window_id_t win2_id)
{
    window_t *win1 = wm_get_window(win1_id);
    if (win1 != NULL)
    {
        widget_draw_label(win1->content, win1->content_width, win1->content_height,
                          10, 10, "Hello from ApexUI v1!", WIDGET_LABEL_TEXT_COLOR);
        widget_draw_button(win1->content, win1->content_width, win1->content_height,
                           10, 40, 100, 28, "Click me", false);
    }

    window_t *win2 = wm_get_window(win2_id);
    if (win2 != NULL)
    {
        widget_draw_label(win2->content, win2->content_width, win2->content_height,
                          10, 10, "Second window", WIDGET_LABEL_TEXT_COLOR);
        widget_draw_label(win2->content, win2->content_width, win2->content_height,
                          10, 30, "Drag titlebars, click taskbar", WIDGET_LABEL_TEXT_COLOR);
    }
}

// ApexUI v1 demo: window manager + compositor + input + widgets +
// taskbar, all wired together -- see this file's cmd_gfxtest() for
// the lower-level graphics primitives this builds on, and
// ApexUI/Includes/Core/WindowManager/WindowManager.h for the overall
// v1 scope (in-kernel, hardcoded demo windows, no app-facing syscall
// yet).
void cmd_apexui(void)
{
    fb_info_t already = fb_get_info();
    if (already.width == 0 && !fb_init())
    {
        klog_error("apexui: no usable graphics mode -- staying in text mode");
        return;
    }

    fb_info_t info = fb_get_info();
    mouse_set_bounds((int32_t)info.width - 1, (int32_t)info.height - 1);

    wm_init();
    window_id_t win1 = wm_create_window("Welcome", 100, 80, 320, 200);
    window_id_t win2 = wm_create_window("Info", 250, 180, 300, 160);

    if (win1 == WM_INVALID_WINDOW_ID || win2 == WM_INVALID_WINDOW_ID)
    {
        klog_error("apexui: window creation failed (out of memory?) -- aborting");
        if (win1 != WM_INVALID_WINDOW_ID)
            wm_close_window(win1);
        if (win2 != WM_INVALID_WINDOW_ID)
            wm_close_window(win2);
        return;
    }

    apexui_draw_demo_content(win1, win2);

    compositor_draw_frame(); // first frame: full composite, ui_input_poll() hasn't run yet to report a change

    // Exits on any keypress, same one-way-out convention as
    // cmd_gfxtest() above -- see that function's header comment.
    while (!keyboard_has_key())
    {
        if (ui_input_poll())
        {
            if (ui_needs_full_redraw())
                compositor_draw_frame();
            else
                compositor_draw_frame_dirty(ui_get_dirty_rect());
        }
    }
    keyboard_getchar(); // consumes the keypress that ended the loop

    wm_close_window(win1);
    wm_close_window(win2);

    // Same shell-restoration steps as cmd_gfxtest() -- see that
    // function's closing comment for why both fb_clear() and
    // fbcon_present_force() are needed together.
    if (vga_is_graphics_mode())
    {
        fb_clear(FB_RGB(0, 0, 0));
        fbcon_present_force();
    }
}

static uint32_t str_len_local(const char *s)
{
    uint32_t n = 0;
    while (s[n])
        n++;
    return n;
}

// Encodes a hostname into DNS's length-prefixed label format (RFC
// 1035 section 3.1) -- "example.com" becomes
// {7}example{3}com{0}. Returns the number of bytes written, or 0 on
// overflow/malformed input (empty label, e.g. "a..b" or a leading/
// trailing dot -- rejected rather than silently producing a
// malformed query the resolver would reject anyway).
static uint32_t encode_dns_name(const char *hostname, uint8_t *out, uint32_t out_size)
{
    uint32_t out_pos = 0;
    uint32_t label_start = 0;
    uint32_t i = 0;
    uint32_t len = str_len_local(hostname);

    while (i <= len)
    {
        if (hostname[i] == '.' || hostname[i] == '\0')
        {
            uint32_t label_len = i - label_start;
            if (label_len == 0 || label_len > 63 || out_pos + 1 + label_len >= out_size)
                return 0;

            out[out_pos++] = (uint8_t)label_len;
            for (uint32_t j = 0; j < label_len; j++)
                out[out_pos++] = (uint8_t)hostname[label_start + j];

            label_start = i + 1;
        }
        i++;
    }

    if (out_pos >= out_size)
        return 0;
    out[out_pos++] = 0; // root label terminator

    return out_pos;
}

// TEST TEMPORAIRE (réseau) -- see DiagnosticCommands.h's header
// comment. Builds and sends a minimal RFC 1035 DNS query (header +
// one question, type A / class IN) over Network/Protocols/UDP/UDP.c
// to QEMU's user-mode built-in resolver, then parses just enough of
// the reply to print the first A record found. Not a real DNS
// client (no retries, no compression-pointer following in the
// answer section beyond what's needed to skip over the echoed
// question, no TCP fallback for truncated replies) -- exists purely
// to exercise UDP send/receive end to end against a real peer, the
// same role cmd_ping() played for ICMP.
// klog_error() takes a single fixed string -- this wraps it so
// resolve_hostname() can prefix its error messages with which
// caller (cmd_dnstest vs cmd_tcptest) is doing the resolving,
// without needing a real string-formatting facility in this
// freestanding environment.
static void log_error_labeled(const char *label, const char *message)
{
    klog_error_prefix();
    vga_puts(label);
    vga_puts(": ");
    vga_puts(message);
    vga_puts("\n");
}

// Resolves `hostname` to an IPv4 address via QEMU's user-mode
// built-in DNS resolver (10.0.2.3) -- shared by cmd_dnstest() and
// cmd_tcptest() so the query-building/reply-parsing logic (see this
// function's body, extracted unchanged from what was originally
// cmd_dnstest()'s own inline logic) isn't duplicated between the two
// test commands. `label` is a short prefix for this call's log lines
// (e.g. "dnstest" or "tcptest") so output makes sense regardless of
// which command is doing the resolving.
static bool resolve_hostname(const char *hostname, uint8_t out_ip[4], const char *label)
{
    uint8_t dns_server[4] = {10, 0, 2, 3}; // QEMU user-mode built-in DNS resolver

    // --- Build the query ---
    uint8_t query[512];
    // Header (RFC 1035 section 4.1.1): ID, flags, QDCOUNT=1, rest 0.
    uint16_t id = 0x1234;
    query[0] = (uint8_t)(id >> 8);
    query[1] = (uint8_t)id;
    query[2] = 0x01; // flags high byte: RD (recursion desired) = 1, standard query
    query[3] = 0x00; // flags low byte
    query[4] = 0x00;
    query[5] = 0x01; // QDCOUNT = 1
    query[6] = 0x00;
    query[7] = 0x00; // ANCOUNT = 0
    query[8] = 0x00;
    query[9] = 0x00; // NSCOUNT = 0
    query[10] = 0x00;
    query[11] = 0x00; // ARCOUNT = 0

    uint32_t name_len = encode_dns_name(hostname, query + 12, sizeof(query) - 12 - 4);
    if (name_len == 0)
    {
        log_error_labeled(label, "hostname too long or malformed");
        return false;
    }

    uint32_t qtype_offset = 12 + name_len;
    query[qtype_offset] = 0x00;
    query[qtype_offset + 1] = 0x01; // QTYPE = A
    query[qtype_offset + 2] = 0x00;
    query[qtype_offset + 3] = 0x01; // QCLASS = IN

    uint16_t query_len = (uint16_t)(qtype_offset + 4);

    uint16_t src_port = 33333;
    if (!udp_send(dns_server, 53, src_port, query, query_len))
    {
        log_error_labeled(label, "DNS udp_send failed");
        return false;
    }

    // --- Wait for the reply ---
    uint8_t rx_buffer[2048];
    uint64_t start = pit_get_ticks();
    uint64_t timeout_ticks = 200; // 2000ms, same budget as cmd_arptest's arp_resolve() call

    while (pit_get_ticks() - start <= timeout_ticks)
    {
        udp_datagram_t datagram;
        if (udp_poll_receive(rx_buffer, &datagram) && datagram.dest_port == src_port)
        {
            // --- Parse just enough of the reply ---
            if (datagram.data_len < 12)
            {
                log_error_labeled(label, "DNS reply too short");
                return false;
            }

            uint16_t ancount = ((uint16_t)datagram.data[6] << 8) | datagram.data[7];
            if (ancount == 0)
            {
                log_error_labeled(label, "DNS: no answers (NXDOMAIN or empty response)");
                return false;
            }

            // Skip the echoed question section: the name (each label
            // length-prefixed, terminated by a 0 byte -- assumed
            // uncompressed here since it's the exact name we just
            // sent, never a pointer) + QTYPE + QCLASS (4 bytes).
            uint32_t pos = 12;
            while (pos < datagram.data_len && datagram.data[pos] != 0)
                pos += datagram.data[pos] + 1;
            pos += 1 + 4; // root label byte + QTYPE + QCLASS

            // Each answer record: NAME (a compression pointer, 2
            // bytes, in every real-world reply) + TYPE(2) + CLASS(2)
            // + TTL(4) + RDLENGTH(2) + RDATA. Only the first answer
            // is examined, and only if it's an A record (RDLENGTH 4,
            // TYPE 1) -- good enough for this test's purpose (see
            // this function's header comment on scope); a CNAME
            // chain before the A record would need more parsing than
            // this test bothers with.
            if (pos + 12 > datagram.data_len)
            {
                log_error_labeled(label, "DNS reply truncated before first answer");
                return false;
            }

            uint16_t rtype = ((uint16_t)datagram.data[pos + 2] << 8) | datagram.data[pos + 3];
            uint16_t rdlength = ((uint16_t)datagram.data[pos + 10] << 8) | datagram.data[pos + 11];

            if (rtype != 1 || rdlength != 4)
            {
                log_error_labeled(label, "DNS: first answer isn't a plain A record (got a CNAME first?)");
                return false;
            }

            const uint8_t *addr = datagram.data + pos + 12;
            for (int i = 0; i < 4; i++)
                out_ip[i] = addr[i];
            return true;
        }
    }

    log_error_labeled(label, "DNS timed out -- no reply received");
    return false;
}

void cmd_dnstest(const char *args)
{
    const char *hostname = (args[0] != '\0') ? args : "example.com";

    uint8_t own_ip[4] = {10, 0, 2, 15};
    eth_init();
    arp_init(own_ip);
    ipv4_init(own_ip);
    uint8_t gateway_ip[4] = {10, 0, 2, 2}; // QEMU user-mode gateway -- see IPv4.h's header comment on routing scope
    ipv4_set_gateway(gateway_ip);

    klog_ok_prefix();
    vga_puts("dnstest: querying ");
    vga_puts(hostname);
    vga_puts(" via 10.0.2.3...\n");

    uint8_t resolved_ip[4];
    if (!resolve_hostname(hostname, resolved_ip, "dnstest"))
        return;

    klog_ok_prefix();
    vga_puts("dnstest: ");
    vga_puts(hostname);
    vga_puts(" = ");
    vga_putdec(resolved_ip[0]);
    vga_puts(".");
    vga_putdec(resolved_ip[1]);
    vga_puts(".");
    vga_putdec(resolved_ip[2]);
    vga_puts(".");
    vga_putdec(resolved_ip[3]);
    vga_puts("\n");
}

// Prints up to `len` bytes as raw text -- used to dump whatever an
// HTTP server sends back in cmd_tcptest(). vga_puts() expects a
// null-terminated C string, but HTTP response bodies/headers aren't
// guaranteed to be null-terminated (and may not even be safely
// printable, e.g. embedded binary in a non-text response) -- this
// copies through a small fixed chunk buffer, always null-terminating
// each chunk, rather than assuming the whole buffer is one safe
// string.
static void print_raw_bytes(const uint8_t *data, uint16_t len)
{
    char chunk[128];
    uint16_t pos = 0;
    while (pos < len)
    {
        uint16_t chunk_len = (uint16_t)(len - pos);
        if (chunk_len > sizeof(chunk) - 1)
            chunk_len = sizeof(chunk) - 1;
        for (uint16_t i = 0; i < chunk_len; i++)
            chunk[i] = (char)data[pos + i];
        chunk[chunk_len] = '\0';
        vga_puts(chunk);
        pos += chunk_len;
    }
}

void cmd_tcptest(const char *args)
{
    const char *hostname = (args[0] != '\0') ? args : "example.com";

    uint8_t own_ip[4] = {10, 0, 2, 15};
    eth_init();
    arp_init(own_ip);
    ipv4_init(own_ip);
    uint8_t gateway_ip[4] = {10, 0, 2, 2}; // QEMU user-mode gateway -- see IPv4.h's header comment on routing scope
    ipv4_set_gateway(gateway_ip);
    tcp_init();

    klog_ok_prefix();
    vga_puts("tcptest: resolving ");
    vga_puts(hostname);
    vga_puts("...\n");

    uint8_t server_ip[4];
    if (!resolve_hostname(hostname, server_ip, "tcptest"))
        return;

    klog_ok_prefix();
    vga_puts("tcptest: connecting to ");
    vga_putdec(server_ip[0]);
    vga_puts(".");
    vga_putdec(server_ip[1]);
    vga_puts(".");
    vga_putdec(server_ip[2]);
    vga_puts(".");
    vga_putdec(server_ip[3]);
    vga_puts(":80...\n");

    uint16_t src_port = 44444;
    if (!tcp_connect(server_ip, 80, src_port, 3000))
    {
        klog_error("tcptest: connection failed (timeout or refused)");
        return;
    }

    klog_ok_prefix();
    vga_puts("tcptest: connected (handshake complete)\n");

    // Minimal HTTP/1.1 GET -- "Connection: close" so the server ends
    // the TCP connection itself once the response is fully sent
    // (rather than this test needing to know the response's
    // Content-Length or handle chunked transfer-encoding to know
    // when to stop reading -- simplest correct way to know "the
    // response is over" for a throwaway test, see this function's
    // header comment on scope). "Host" is required for HTTP/1.1
    // (RFC 7230) -- most real-world servers reject a request without
    // it, including virtual-hosted ones.
    char request[256];
    uint32_t req_len = 0;
    const char *parts[] = {"GET / HTTP/1.1\r\nHost: ", hostname,
                           "\r\nConnection: close\r\n\r\n"};
    for (int p = 0; p < 3; p++)
    {
        uint32_t part_len = str_len_local(parts[p]);
        for (uint32_t i = 0; i < part_len && req_len < sizeof(request) - 1; i++)
            request[req_len++] = parts[p][i];
    }

    if (!tcp_send(request, (uint16_t)req_len))
    {
        klog_error("tcptest: failed to send HTTP request");
        tcp_close(1000);
        return;
    }

    klog_ok_prefix();
    vga_puts("tcptest: request sent, reading response...\n\n");

    uint8_t scratch_buffer[2048];
    uint8_t data_buffer[1024];
    uint64_t start = pit_get_ticks();
    uint64_t timeout_ticks = 300; // 3000ms overall budget for the whole response
    uint32_t total_received = 0;

    while (pit_get_ticks() - start <= timeout_ticks)
    {
        if (tcp_get_state() == TCP_STATE_CLOSED_BY_PEER)
            break; // server sent FIN -- "Connection: close" means this is expected, not an error

        uint16_t received = tcp_poll_receive(scratch_buffer, data_buffer, sizeof(data_buffer));
        if (received > 0)
        {
            print_raw_bytes(data_buffer, received);
            total_received += received;
            start = pit_get_ticks(); // reset the idle timeout each time data actually arrives
        }
    }

    vga_puts("\n");
    klog_ok_prefix();
    vga_puts("tcptest: ");
    vga_putdec(total_received);
    vga_puts(" bytes received, closing connection\n");

    tcp_close(1000);
}

void cmd_serialmirror(const char *args)
{
    if (str_eq(args, "on"))
    {
        vga_set_serial_mirror(true);
        klog_ok_prefix();
        vga_puts("serialmirror: enabled -- screen output now mirrored to COM1\n");
    }
    else if (str_eq(args, "off"))
    {
        vga_set_serial_mirror(false);
        klog_ok_prefix();
        vga_puts("serialmirror: disabled\n");
    }
    else
        klog_error("serialmirror: usage: serialmirror on|off");
}
