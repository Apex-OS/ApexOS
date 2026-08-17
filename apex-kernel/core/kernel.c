// apex-kernel/core/kernel.c
// ApexOS -- Kernel entry point

#include "gdt.h"
#include "idt.h"
#include "shell/shell.h"
#include "scheduler.h"
#include "tss.h"
#include "syscall.h"
#include "userspace.h"
#include "../drivers/vga.h"
#include "../drivers/framebuffer.h"
#include "../drivers/pic.h"
#include "../drivers/pit.h"
#include "../drivers/serial.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../drivers/pci.h"
#include "../drivers/ahci.h"
#include "../include/apex/mmap.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"
#include "../mm/heap.h"
#include "../fs/axfs.h"
#include "../fs/vfs.h"
#include "klog.h"
#include "axf/test_axp_embed.h"
#include "axf/libmath_axl_embed.h"
#include "axf/test_import_axp_embed.h"
#include "axf/stub_axd_embed.h"
#include "axf/test_driver_axp_embed.h"
#include "axf/axd.h"

// ── System info ────────────────────────────────────────────────────────────
static void print_sysinfo(void)
{
    klog_ok_hex("Kernel loaded at", 0x100000);
    if (vga_is_graphics_mode())
        klog_ok("Console: framebuffer text mode (see drivers/fbcon.c)");
    else
        klog_ok("VGA driver initialized (80x25)");
    klog_ok("Long mode active - x86-64");
    klog_ok("GDT initialized (5 descriptors)");
    klog_ok("IDT initialized (256 entries)");
    klog_ok("PIC 8259 remapped (IRQ 0x20-0x2F)");
}

// ── Kernel panic ───────────────────────────────────────────────────────────
void kpanic(const char *msg)
{
    vga_set_color(VGA_WHITE, VGA_RED);
    vga_puts("\n!! KERNEL PANIC !!\n  ");
    vga_puts(msg);
    vga_puts("\n  System halted.\n");
    __asm__ volatile("cli");
    for (;;)
        __asm__ volatile("hlt");
}

// ── Entry point ────────────────────────────────────────────────────────────
void kmain(void)
{
    // Init hardware.
    vga_init();
    vga_hide_cursor();
    // serial_init() must run right after vga_init() and before any
    // klog_ok/vga_puts call -- vga_putchar() mirrors every character
    // to COM1 (see vga.c), and serial_putchar() spins in
    // serial_wait() until the UART's transmit-empty status bit is
    // set. On an unconfigured port that bit's default state isn't
    // guaranteed, so any output before serial_init() risked hanging
    // the boot right here.
    serial_init();

    gdt_init();
    tss_init();
    pic_init();
    idt_init();
    syscall_init();

    // Init memory.
    pmm_init();
    pmm_print_stats();
    vmm_init();
    vmm_print_stats();
    heap_init();
    heap_print_stats();

    // Switches text output to render inside the VESA/VBE framebuffer
    // (see drivers/fbcon.c) instead of real VGA text-mode memory --
    // stage2.asm already put the machine in graphics mode at boot
    // (see arch/x86_64/boot/stage2.asm: detect_vbe_mode), so without
    // this every vga_puts/klog call from here on would write to
    // 0xB8000 memory nothing is scanning out to the screen, and the
    // QEMU window would stay black while output only ever reached the
    // serial mirror.
    //
    // MUST run after heap_init() above, not before -- fb_init()
    // allocates the graphics back-buffer with kmalloc() (see
    // drivers/framebuffer.c), and calling it before the heap exists
    // makes that kmalloc() fail every time, silently falling back to
    // VGA text mode. (First version of this code called fb_init()
    // right after serial_init(), before any memory init -- caught via
    // the "[ERROR] fb_init: kmalloc failed for back-buffer" log line,
    // followed immediately by "[OK] Heap initialized" further down:
    // proof the heap genuinely didn't exist yet at the point fb_init()
    // ran.)
    //
    // Consequence: GDT/IDT/PIC/PMM/VMM/heap init messages above this
    // point are only ever printed via real VGA text mode (and its
    // serial mirror) -- never in the framebuffer console, even on a
    // successful graphics boot. Only output from here on (AXFS init,
    // the shell, etc.) can appear in the QEMU window. Falls back to
    // real VGA text mode automatically if fb_init() fails for any
    // other reason (no suitable VBE mode found) -- vga_enable_graphics_mode()
    // is simply never called in that case, and every vga_* function
    // keeps working exactly as it always has.
    if (fb_init())
        vga_enable_graphics_mode();

    // Init filesystem (AXFS -- RAM disk for this V1, see fs/axfs.c).
    axfs_init();
    axfs_format("SysDisk"); // V1: reformatted on every boot (RAM disk, no persistence across boots)
    axfs_mount();
    axfs_create_default_tree();
    vfs_register_volume("SysDisk", VFS_FS_AXFS);

    // DEBUG/TEST: writes a test .axp into the user folder, to exercise
    // the AXF loader (`run test.axp`) without needing a real disk
    // driver -- see tools/make_test_axp.py for this file's source,
    // embedded via test_axp_embed.h. To be removed once there's a
    // real way to put files on the volume (ATA driver + real
    // persistence, or upload over the serial port).
    {
        uint64_t home_inode = axfs_resolve_path(AXFS_ROOT_INODE, "Home/Users/Zeemoud");
        if (home_inode != 0)
        {
            uint64_t axp_inode = axfs_create(home_inode, "test.axp", AXFS_TYPE_FILE);
            if (axp_inode != 0)
                axfs_write(axp_inode, test_axp_data, sizeof(test_axp_data), 0);

            // Tests the .axp -> .axl import/export mechanism (see
            // tools/make_test_axl.py and
            // tools/make_test_axp_with_import.py): libmath.axl exports
            // add(a,b), test_import.axp imports it and calls it with
            // (3,4) -- should print "add(3,4) = 7".
            uint64_t axl_inode = axfs_create(home_inode, "libmath.axl", AXFS_TYPE_FILE);
            if (axl_inode != 0)
                axfs_write(axl_inode, libmath_axl_data, sizeof(libmath_axl_data), 0);

            uint64_t import_axp_inode = axfs_create(home_inode, "test_import.axp", AXFS_TYPE_FILE);
            if (import_axp_inode != 0)
                axfs_write(import_axp_inode, test_import_axp_data, sizeof(test_import_axp_data), 0);

            // Tests the .axd mechanism (see tools/make_test_axd.py):
            // stub.axd exports driver_init() (returns immediately,
            // just validates that loading + the Ring 0 call work) and
            // driver_ping(arg) -> 42 (validates axd_call() directly,
            // without going through SYS_DRIVER_CALL -- the full
            // syscall test from a .axp comes once this first level is
            // validated).
            uint64_t axd_inode = axfs_create(home_inode, "stub.axd", AXFS_TYPE_FILE);
            if (axd_inode != 0)
            {
                axfs_write(axd_inode, stub_axd_data, sizeof(stub_axd_data), 0);

                int driver_id = axd_load(axd_inode);
                if (driver_id >= 0)
                {
                    uint64_t ping_result = axd_call(driver_id, 1, 0); // func_id 1 = driver_ping
                    klog_ok_n("kernel: stub.axd driver_ping() returned", ping_result);

                    // Tests the FULL Ring3 -> SYS_DRIVER_CALL -> Ring0
                    // -> axd_call() path (see
                    // tools/make_test_driver_axp.py). Only written if
                    // the driver actually loaded (driver_id=0
                    // expected, hardcoded in the generated .axp) --
                    // running `run test_driver.axp` in the shell
                    // should print "driver_ping() = 42".
                    uint64_t drv_axp_inode = axfs_create(home_inode, "test_driver.axp", AXFS_TYPE_FILE);
                    if (drv_axp_inode != 0)
                        axfs_write(drv_axp_inode, test_driver_axp_data, sizeof(test_driver_axp_data), 0);
                }
                else
                {
                    klog_error("kernel: stub.axd failed to load");
                }
            }
        }
    }

    // Init drivers.
    keyboard_init();
    pic_unmask_irq(1);

    mouse_init();
    pic_unmask_irq(2);  // slave PIC cascade -- needed for IRQ8-15
    pic_unmask_irq(12); // mouse

    // Clamps mouse tracking to the real screen bounds now that
    // graphics mode is up (see fb_init()/vga_enable_graphics_mode()
    // above) -- same call gfxtest makes on its own (see
    // shell_commands.c: cmd_gfxtest), done here too so the bound is
    // already correct for any future code that draws a cursor outside
    // that one test command. No-op (harmless) if fb_init() failed and
    // the machine is on VGA text mode -- mouse.c just keeps tracking
    // unclamped, as documented in mouse.h.
    {
        fb_info_t info = fb_get_info();
        if (info.width > 0 && info.height > 0)
            mouse_set_bounds((int32_t)info.width - 1, (int32_t)info.height - 1);
    }

    // Init scheduler + PIT.
    scheduler_init();
    pit_init();

    // First step toward real disk persistence (see project notes).
    // Detects the AHCI controller, initializes its first port with a
    // real SATA drive, and issues an IDENTIFY command -- validates
    // the whole AHCI plumbing (MMIO mapping, command list, FIS,
    // PRDT) works end to end. axfs_disk.c's future real-disk backend
    // builds on ahci_read_sectors()/ahci_drive_info() from here.
    // Non-fatal if it fails: the RAM disk backend keeps working
    // exactly as it always has either way.
    //
    // MUST run after pit_init() above, not before -- ahci_init()'s
    // command timeouts (see ahci.c: ahci_wait_clear()) are measured
    // via pit_get_ticks(), which only advances once the PIT is
    // actually initialized and ticking.
    ahci_init();

    // Creates the shell process.
    process_t *shell = process_create("shell", shell_run);
    scheduler_add(shell);

    print_sysinfo();

    // Starts the shell.
    current_proc_start(shell);

    for (;;)
        __asm__ volatile("hlt");
}