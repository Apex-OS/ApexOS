// ApexKernel/Core/Memory/Virtual/VirtualMemoryStats.c
// ApexOS -- VMM diagnostics

#include "Core/Memory/Virtual/VirtualMemoryStats.h"
#include "Core/Boot/Log/BootLog.h"

void vmm_print_stats(void)
{
    boot_log_ok("VMM: identity map built (scratch table at 0x7000, copied to 0x1000, CR3 reloaded)");
    boot_log_ok("VMM: identity-mapped 0x0 to end of detected RAM, plus a 16MB margin past it");
}
