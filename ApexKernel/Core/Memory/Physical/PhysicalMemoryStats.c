// ApexKernel/Core/Memory/Physical/PhysicalMemoryStats.c
// ApexOS -- PMM diagnostics

#include "Core/Memory/Physical/PhysicalMemoryStats.h"
#include "Core/Memory/Physical/PageBitmap.h"
#include "Core/Boot/Log/BootLog.h"

void pmm_print_stats(void)
{
    pmm_stats_t s;
    pmm_stats(&s);

    boot_log_start();
    boot_log_str("PMM: total ");
    boot_log_dec(s.total_pages * PAGE_SIZE / (1024 * 1024));
    boot_log_str("MB (");
    boot_log_dec(s.total_pages);
    boot_log_str(" pages)");
    boot_log_end_ok();

    boot_log_start();
    boot_log_str("PMM: ");
    boot_log_dec(s.free_pages);
    boot_log_str(" pages free (");
    boot_log_dec(s.free_pages * PAGE_SIZE / (1024 * 1024));
    boot_log_str("MB), ");
    boot_log_dec(s.used_pages);
    boot_log_str(" pages used (");
    boot_log_dec(s.used_pages * PAGE_SIZE / (1024 * 1024));
    boot_log_str("MB)");
    boot_log_end_ok();
}
