// ApexKernel/Core/Memory/Virtual/VirtualMemoryStats.c
// ApexOS -- VMM diagnostics

#include "Core/Memory/Virtual/VirtualMemoryStats.h"
#include "Drivers/Display/VGA.h"
#include "Logs/Log.h"

void vmm_print_stats(void)
{
    klog_ok_prefix();
    vga_puts("VMM initialized - Identity map 0x400000 to end of RAM\n");
}
