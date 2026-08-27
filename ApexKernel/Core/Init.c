// ApexKernel/Core/Init.c
// ApexOS -- Kernel entry point

#include "Core/Panic.h"
#include "Core/SystemInfo.h"
#include "Core/Startup/SysDiskSetup.h"
#include "Core/Startup/TestFilesSetup.h"
#include "Arch/x86_64/GDTSetup.h"
#include "Core/Tasking/Context/TSS.h"
#include "Core/Interrupts/PIC/PICSetup.h"
#include "Core/Interrupts/PIC/PICControl.h"
#include "Core/Interrupts/IDT/IDT.h"
#include "Core/Syscalls/ABI/SyscallSetup.h"
#include "Core/Memory/Physical/PageBitmap.h"
#include "Core/Memory/Physical/PhysicalMemoryDetection.h"
#include "Core/Memory/Physical/PhysicalMemoryStats.h"
#include "Core/Memory/Virtual/IdentityMap.h"
#include "Core/Memory/Virtual/VirtualMemoryStats.h"
#include "Core/Memory/Heap/HeapSetup.h"
#include "Core/Memory/Heap/HeapStats.h"
#include "Core/Time/PITSetup.h"
#include "Core/Tasking/Scheduler/Scheduler.h"
#include "Core/Tasking/Scheduler/ProcessLauncher.h"
#include "Core/Tasking/Process/Process.h"
#include "Core/Shell/Shell.h"
#include "Drivers/Display/VGA.h"
#include "Drivers/Display/Framebuffer/Framebuffer.h"
#include "Devices/Serial/16550/Serial16550.h"
#include "Drivers/Input/Keyboard/Keyboard.h"
#include "Drivers/Input/Mouse/Mouse.h"
#include "Drivers/Storage/AHCI/AHCI.h"

void kmain(void)
{
    // Init hardware.
    vga_init();
    vga_hide_cursor();
    // serial_init() must run right after vga_init() and before any
    // klog_ok/vga_puts call -- vga_putchar() mirrors every character
    // to COM1 (see Drivers/Display/VGA.c), and serial_putchar() spins
    // in serial_wait() until the UART's transmit-empty status bit is
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
    // (see Drivers/Display/Console/) instead of real VGA text-mode
    // memory -- Stage2.asm already put the machine in graphics mode
    // at boot (see Boot/BIOS/Stage2.asm: detect_vbe_mode), so without
    // this every vga_puts/klog call from here on would write to
    // 0xB8000 memory nothing is scanning out to the screen, and the
    // QEMU window would stay black while output only ever reached the
    // serial mirror.
    //
    // MUST run after heap_init() above, not before -- fb_init()
    // allocates the graphics back-buffer with kmalloc() (see
    // Drivers/Display/Framebuffer/FramebufferInit.c), and calling it
    // before the heap exists makes that kmalloc() fail every time,
    // silently falling back to VGA text mode. (First version of this
    // code called fb_init() right after serial_init(), before any
    // memory init -- caught via the "[ERROR] fb_init: kmalloc failed
    // for back-buffer" log line, followed immediately by "[OK] Heap
    // initialized" further down: proof the heap genuinely didn't exist
    // yet at the point fb_init() ran.)
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

    // Init scheduler + PIT -- moved up from its previous spot further
    // down, because ahci_init() (right below) needs pit_get_ticks()
    // for its command timeouts, and AXFS's real volume now needs AHCI
    // ready before it can mount (see the SysDisk setup right after).
    scheduler_init();
    pit_init();

    // Detects the AHCI controller, initializes its first port with a
    // real SATA drive, and issues an IDENTIFY command -- validates the
    // whole AHCI plumbing (MMIO mapping, command list, FIS, PRDT)
    // works end to end. SysDisk (right below) needs this to have
    // already run to use the AHCI backend; falls back to a volatile
    // RAM disk if it fails (no AHCI controller, or no drive attached)
    // so the kernel still boots to a usable shell either way.
    ahci_init();

    // Sets up @SysDisk -- the AXFS volume the shell/filesystem
    // commands resolve "@SysDisk/..." paths against (see
    // Filesystem/VFS/). Prefers the real AHCI disk (persistent across
    // boots) over the RAM disk: only falls back to RAM if
    // ahci_drive_info().ready is false, e.g. no AHCI controller/drive
    // found above.
    axfs_volume_t *sysdisk_vol = core_setup_sysdisk();

    // DEBUG/TEST: seeds the volume with the embedded test .axp/.axl/
    // .axd files, to exercise the AXF loader without needing a real
    // way to put files on the volume yet.
    core_seed_test_files(sysdisk_vol);

    // Init drivers.
    keyboard_init();
    pic_unmask_irq(1);

    mouse_init();
    pic_unmask_irq(2);  // slave PIC cascade -- needed for IRQ8-15
    pic_unmask_irq(12); // mouse

    // Clamps mouse tracking to the real screen bounds now that
    // graphics mode is up (see fb_init()/vga_enable_graphics_mode()
    // above) -- same call gfxtest makes on its own (see
    // Core/Shell/Commands/DiagnosticCommands.c: cmd_gfxtest), done
    // here too so the bound is already correct for any future code
    // that draws a cursor outside that one test command. No-op
    // (harmless) if fb_init() failed and the machine is on VGA text
    // mode -- Mouse.c just keeps tracking unclamped, as documented in
    // Mouse.h.
    {
        fb_info_t info = fb_get_info();
        if (info.width > 0 && info.height > 0)
            mouse_set_bounds((int32_t)info.width - 1, (int32_t)info.height - 1);
    }

    // Creates the shell process.
    process_t *shell = process_create("shell", shell_run);
    scheduler_add(shell);

    print_sysinfo();

    // Starts the shell.
    current_proc_start(shell);

    for (;;)
        __asm__ volatile("hlt");
}
