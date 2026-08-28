// ApexKernel/Core/SystemInfo.c
// ApexOS -- Boot summary diagnostics

#include "Core/SystemInfo.h"
#include "Drivers/Display/VGA.h"
#include "Logs/Log.h"

void print_sysinfo(void)
{
    klog_ok_hex("Kernel loaded at", 0x100000);
    if (vga_is_graphics_mode())
        klog_ok("Console: framebuffer text mode (see Drivers/Display/Console/)");
    else
        klog_ok("VGA driver initialized (80x25)");
    klog_ok("Long mode active - x86-64");
    klog_ok("GDT initialized (5 descriptors)");
    klog_ok("IDT initialized (256 entries)");
    klog_ok("PIC 8259 remapped (IRQ 0x20-0x2F)");
}
