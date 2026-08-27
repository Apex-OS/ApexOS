// ApexKernel/Core/Memory/Heap/HeapStats.c
// ApexOS -- Heap diagnostics

#include "Core/Memory/Heap/HeapStats.h"
#include "Core/Memory/Heap/HeapInternal.h"
#include "Drivers/Display/VGA.h"
#include "ApexLib/Logs/Log.h"

void heap_print_stats(void)
{
    size_t free_bytes = 0;
    size_t used_bytes = 0;
    size_t blocks = 0;

    heap_block_t *block = heap_head;
    while (block)
    {
        if (block->free)
            free_bytes += block->size;
        else
            used_bytes += block->size;
        blocks++;
        block = block->next;
    }
    klog_ok_prefix();
    vga_puts("Heap initialized - Size: ");
    vga_putdec(heap_size / 1024);
    vga_puts("KB  Free: ");
    vga_putdec(free_bytes / 1024);
    vga_puts("KB  Used: ");
    vga_putdec(used_bytes / 1024);
    vga_puts("KB  Blocks: ");
    vga_putdec(blocks);
    vga_puts("\n");
}
