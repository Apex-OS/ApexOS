// apex-kernel/mm/pmm.c
// ApexOS -- Physical Memory Manager (bitmap allocator)

#include "pmm.h"
#include "../drivers/vga.h"
#include "../core/klog.h"

static uint8_t *pmm_bitmap = NULL;
static uint64_t pmm_total = 0;       // Total page count
static uint64_t pmm_used = 0;        // Pages currently in use
static uint64_t pmm_bitmap_size = 0; // Bitmap size in bytes

static inline void bitmap_set(uint64_t page)
{
    pmm_bitmap[page / 8] |= (1 << (page % 8));
}

static inline void bitmap_clear(uint64_t page)
{
    pmm_bitmap[page / 8] &= ~(1 << (page % 8));
}

static inline bool bitmap_test(uint64_t page)
{
    return (pmm_bitmap[page / 8] >> (page % 8)) & 1;
}

void pmm_init(void)
{
    mmap_entry_t *entries = mmap_get_entries();
    uint32_t count = mmap_get_count();

    // Finds the highest USABLE address only (type == Available).
    uint64_t max_addr = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        if (entries[i].type != MMAP_AVAILABLE)
            continue;
        uint64_t end = entries[i].base + entries[i].length;
        if (end > max_addr)
            max_addr = end;
    }

    pmm_total = ADDR_TO_PAGE(max_addr);
    pmm_bitmap_size = (pmm_total + 7) / 8;

    // Places the bitmap right after the kernel image.
    pmm_bitmap = (uint8_t *)((uint64_t)&__kernel_end);

    // Marks everything used by default; available E820 ranges are
    // freed below.
    for (uint64_t i = 0; i < pmm_bitmap_size; i++)
        pmm_bitmap[i] = 0xFF;

    pmm_used = pmm_total;

    for (uint32_t i = 0; i < count; i++)
    {
        if (entries[i].type != MMAP_AVAILABLE)
            continue;

        uint64_t base = ADDR_TO_PAGE(entries[i].base);
        uint64_t pages = entries[i].length / PAGE_SIZE;

        for (uint64_t p = base; p < base + pages; p++)
        {
            if (p < pmm_total && bitmap_test(p))
            {
                bitmap_clear(p);
                pmm_used--;
            }
        }
    }

    // Re-reserves the low 1MB (BIOS/boot data, real-mode IVT, etc.).
    uint64_t low_pages = ADDR_TO_PAGE(0x100000);
    for (uint64_t p = 0; p < low_pages && p < pmm_total; p++)
    {
        if (!bitmap_test(p))
        {
            bitmap_set(p);
            pmm_used++;
        }
    }

    // Re-reserves the kernel image plus the bitmap that follows it.
    uint64_t kernel_start_page = ADDR_TO_PAGE(0x100000);
    uint64_t bitmap_end = (uint64_t)&__kernel_end + pmm_bitmap_size;
    uint64_t kernel_end_page = ADDR_TO_PAGE(
        (bitmap_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));

    for (uint64_t p = kernel_start_page; p < kernel_end_page && p < pmm_total; p++)
    {
        if (!bitmap_test(p))
        {
            bitmap_set(p);
            pmm_used++;
        }
    }
}

void *pmm_alloc_page(void)
{
    for (uint64_t p = 0; p < pmm_total; p++)
    {
        if (!bitmap_test(p))
        {
            bitmap_set(p);
            pmm_used++;
            return (void *)PAGE_TO_ADDR(p);
        }
    }
    return NULL; // Out of physical memory.
}

void pmm_free_page(void *addr)
{
    uint64_t page = ADDR_TO_PAGE((uint64_t)addr);
    if (bitmap_test(page))
    {
        bitmap_clear(page);
        pmm_used--;
    }
}

void pmm_stats(pmm_stats_t *stats)
{
    stats->total_pages = pmm_total;
    stats->used_pages = pmm_used;
    stats->free_pages = pmm_total - pmm_used;
}

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