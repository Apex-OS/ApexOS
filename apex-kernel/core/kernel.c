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
#include "../drivers/pic.h"
#include "../drivers/pit.h"
#include "../drivers/serial.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
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

    // Init scheduler + PIT.
    scheduler_init();
    pit_init();

    // Creates the shell process.
    process_t *shell = process_create("shell", shell_run);
    scheduler_add(shell);

    print_sysinfo();

    // Starts the shell.
    current_proc_start(shell);

    for (;;)
        __asm__ volatile("hlt");
}