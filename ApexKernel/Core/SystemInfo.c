// ApexKernel/Core/SystemInfo.c
// ApexOS -- Boot summary diagnostics

#include "Core/SystemInfo.h"
#include "Drivers/Display/VGA.h"
#include "Core/Boot/Log/BootLog.h"

void print_sysinfo(void)
{
    boot_log_ok_hex("Kernel loaded at", 0x100000);
    if (vga_is_graphics_mode())
        boot_log_ok("Console: framebuffer text mode (see Drivers/Display/Console/)");
    else
        boot_log_ok("VGA driver initialized (80x25)");
    boot_log_ok("Long mode active - x86-64");
    boot_log_ok("GDT initialized (5 descriptors)");
    boot_log_ok("IDT initialized (256 entries)");
    boot_log_ok("PIC 8259 remapped (IRQ 0x20-0x2F)");
}
