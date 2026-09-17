// ApexKernel/Core/Memory/Heap/HeapStats.c
// ApexOS -- Heap diagnostics

#include "Core/Memory/Heap/HeapStats.h"
#include "Core/Memory/Heap/HeapInternal.h"
#include "Core/Boot/Log/BootLog.h"

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

    boot_log_start();
    boot_log_str("Heap: ");
    boot_log_dec(heap_size / 1024);
    boot_log_str("KB total, ");
    boot_log_dec(blocks);
    boot_log_str(" block(s)");
    boot_log_end_ok();

    boot_log_start();
    boot_log_str("Heap: ");
    boot_log_dec(free_bytes / 1024);
    boot_log_str("KB free, ");
    boot_log_dec(used_bytes / 1024);
    boot_log_str("KB used");
    boot_log_end_ok();
}
