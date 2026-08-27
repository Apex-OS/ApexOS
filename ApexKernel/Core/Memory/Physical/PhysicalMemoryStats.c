// ApexKernel/Core/Memory/Physical/PhysicalMemoryStats.c
// ApexOS -- PMM diagnostics

#include "Core/Memory/Physical/PhysicalMemoryStats.h"
#include "Core/Memory/Physical/PageBitmap.h"
#include "Drivers/Display/VGA.h"
#include "ApexLib/Logs/Log.h"

void pmm_print_stats(void)
{
    pmm_stats_t s;
    pmm_stats(&s);

    klog_ok_prefix();
    vga_puts("PMM initialized - Total: ");
    vga_putdec(s.total_pages * PAGE_SIZE / (1024 * 1024));
    vga_puts("MB  Free: ");
    vga_putdec(s.free_pages * PAGE_SIZE / (1024 * 1024));
    vga_puts("MB  Used: ");
    vga_putdec(s.used_pages * PAGE_SIZE / (1024 * 1024));
    vga_puts("MB\n");
}
