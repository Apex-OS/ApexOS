// ApexKernel/Core/Memory/MemoryMap/MemoryMapPrinter.c
// ApexOS -- E820 memory map diagnostics

#include "Core/Memory/MemoryMap/MemoryMapPrinter.h"
#include "Core/Memory/MemoryMap/MemoryMap.h"
#include "Drivers/Display/VGA.h"

void mmap_print(void)
{
    mmap_entry_t *entries = mmap_get_entries();
    uint32_t count = mmap_get_count();

    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("Memory Map (E820):\n");

    for (uint32_t i = 0; i < count; i++)
    {
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_puts("  [");
        vga_putdec(i);
        vga_puts("] Base: ");
        vga_puthex(entries[i].base);
        vga_puts("  Len: ");
        vga_puthex(entries[i].length);
        vga_puts("  Type: ");

        switch (entries[i].type)
        {
        case MMAP_AVAILABLE:
            vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
            vga_puts("Available");
            break;
        case MMAP_RESERVED:
            vga_set_color(VGA_RED, VGA_BLACK);
            vga_puts("Reserved");
            break;
        case MMAP_ACPI_RECLAIM:
            vga_set_color(VGA_YELLOW, VGA_BLACK);
            vga_puts("ACPI Reclaimable");
            break;
        case MMAP_ACPI_NVS:
            vga_set_color(VGA_YELLOW, VGA_BLACK);
            vga_puts("ACPI NVS");
            break;
        case MMAP_BAD:
            vga_set_color(VGA_RED, VGA_BLACK);
            vga_puts("Bad Memory");
            break;
        default:
            vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
            vga_puts("Unknown");
            break;
        }
        vga_puts("\n");
    }

    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("Total RAM: ");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);

    uint64_t total_mb = mmap_get_total_ram() / (1024 * 1024);
    vga_putdec(total_mb);
    vga_puts(" MB\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
}
