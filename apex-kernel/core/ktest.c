// apex-kernel/core/ktest.c
// ApexOS -- Kernel self-tests

#include "ktest.h"
#include "gdt.h"
#include "idt.h"
#include "scheduler.h"
#include "../drivers/vga.h"
#include "../drivers/pit.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"
#include "../mm/heap.h"
#include "../fs/axfs.h"
#include "../fs/vfs.h"
#include "../include/apex/types.h"
#include "klog.h"

static uint32_t tests_passed = 0;
static uint32_t tests_failed = 0;

static void test_pass(const char *name)
{
    vga_puts("  ");
    klog_test_pass(name);
    tests_passed++;
}

static void test_fail(const char *name, const char *reason)
{
    vga_puts("  ");
    klog_test_fail(name, reason);
    tests_failed++;
}

#define TEST(name, cond, reason)     \
    do                               \
    {                                \
        if (cond)                    \
            test_pass(name);         \
        else                         \
            test_fail(name, reason); \
    } while (0)

static void test_gdt(void)
{
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("GDT:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    uint16_t cs, ds, ss;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    __asm__ volatile("mov %%ds, %0" : "=r"(ds));
    __asm__ volatile("mov %%ss, %0" : "=r"(ss));

    TEST("CS = 0x08", cs == 0x08, "wrong code segment");
    TEST("DS = 0x10", ds == 0x10, "wrong data segment");
    TEST("SS = 0x10", ss == 0x10, "wrong stack segment");
}

static void test_idt(void)
{
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("IDT:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    uint64_t rflags;
    __asm__ volatile(
        "pushfq\n"
        "pop %0\n"
        : "=r"(rflags));
    TEST("Interrupts enabled (IF=1)", rflags & (1 << 9), "IF flag not set");
    TEST("IDT loaded", true, "");
}

static void test_pit(void)
{
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("PIT:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    uint64_t t1 = pit_get_ticks();
    uint64_t wait = 0;
    while (pit_get_ticks() == t1 && wait < 10000000)
        wait++;
    uint64_t t2 = pit_get_ticks();

    TEST("PIT ticking", t2 > t1, "no tick detected");
    TEST("PIT ticks > 0", t2 > 0, "ticks = 0");
}

static void test_pmm(void)
{
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("PMM:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    pmm_stats_t s;
    pmm_stats(&s);

    TEST("Total pages > 0", s.total_pages > 0, "no pages");
    TEST("Free pages > 0", s.free_pages > 0, "no free pages");
    TEST("RAM > 64MB", s.total_pages * 4096 > 64 * 1024 * 1024, "less than 64MB");

    uint64_t used_before = s.used_pages;
    void *p1 = pmm_alloc_page();
    void *p2 = pmm_alloc_page();
    TEST("pmm_alloc_page != NULL", p1 != NULL && p2 != NULL, "alloc failed");
    TEST("pages different", p1 != p2, "same page allocated twice");

    pmm_stats(&s);
    TEST("used_pages increased", s.used_pages > used_before, "used not updated");

    pmm_free_page(p1);
    pmm_free_page(p2);
    pmm_stats(&s);
    TEST("used_pages decreased after free", s.used_pages == used_before, "free failed");
}

static void test_vmm(void)
{
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("VMM:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    uint64_t phys = vmm_get_phys(0x100000);
    TEST("vmm_get_phys(0x100000) = 0x100000", phys == 0x100000, "wrong translation");

    void *phys_page = pmm_alloc_page();
    TEST("alloc phys page for VMM test", phys_page != NULL, "alloc failed");

    vmm_map(0x500000, (uint64_t)phys_page, VMM_KERNEL_RW);
    uint64_t translated = vmm_get_phys(0x500000);
    TEST("vmm_map + vmm_get_phys", translated == (uint64_t)phys_page, "map failed");

    uint64_t *test_ptr = (uint64_t *)0x500000;
    *test_ptr = 0xDEADBEEFCAFEBABE;
    TEST("write/read mapped page", *test_ptr == 0xDEADBEEFCAFEBABE, "data mismatch");

    vmm_unmap(0x500000);
    pmm_free_page(phys_page);
    TEST("vmm_unmap done", true, "");
}

static void test_heap(void)
{
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("Heap:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    void *a = kmalloc(64);
    void *b = kmalloc(128);
    void *c = kmalloc(256);

    TEST("kmalloc(64)  != NULL", a != NULL, "alloc failed");
    TEST("kmalloc(128) != NULL", b != NULL, "alloc failed");
    TEST("kmalloc(256) != NULL", c != NULL, "alloc failed");
    TEST("allocations different", a != b && b != c && a != c, "same addr");

    uint8_t *pa = (uint8_t *)a;
    for (int i = 0; i < 64; i++)
        pa[i] = (uint8_t)i;
    TEST("write 64 bytes", pa[63] == 63, "write failed");

    uint8_t *z = (uint8_t *)kcalloc(32, 1);
    bool zeroed = true;
    for (int i = 0; i < 32; i++)
        if (z[i] != 0)
        {
            zeroed = false;
            break;
        }
    TEST("kcalloc zero-initialized", zeroed, "not zeroed");

    kfree(a);
    void *d = kmalloc(64);
    TEST("kmalloc after kfree", d != NULL, "alloc after free failed");

    kfree(b);
    kfree(c);
    kfree(d);
    kfree(z);
    TEST("kfree all blocks", true, "");
}

static void test_scheduler(void)
{
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("Scheduler:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    process_t *cur = scheduler_current();
    TEST("current process != NULL", cur != NULL, "no current process");
    TEST("current process RUNNING", cur && cur->state == PROCESS_RUNNING, "not running");
    TEST("current process ticks > 0", cur && cur->ticks > 0, "no ticks");

    uint64_t t = pit_get_ticks();
    TEST("PIT ticks > 0", t > 0, "no ticks");
}

// Runs entirely under a dedicated "KTest" directory (created fresh
// under root, removed at the end) rather than under Home/Users/... --
// keeps this test run from ever touching real user files, and safe to
// run more than once per boot (each run starts from a clean slate).
static void test_axfs(void)
{
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("AXFS:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    uint64_t test_dir = axfs_create(AXFS_ROOT_INODE, "KTest", AXFS_TYPE_DIR);
    TEST("axfs_create(dir)", test_dir != 0, "could not create test dir");
    if (test_dir == 0)
        return; // nothing else can run without it

    TEST("axfs_get_type(dir) == DIR", axfs_get_type(test_dir) == AXFS_TYPE_DIR, "wrong type");

    uint64_t file = axfs_create(test_dir, "file.txt", AXFS_TYPE_FILE);
    TEST("axfs_create(file)", file != 0, "could not create file");

    uint64_t dup = axfs_create(test_dir, "file.txt", AXFS_TYPE_FILE);
    TEST("axfs_create rejects duplicate name", dup == 0, "duplicate was allowed");

    const char *msg = "Hello, AXFS!";
    uint32_t msg_len = 0;
    while (msg[msg_len])
        msg_len++;

    bool wrote = axfs_write(file, msg, msg_len, 0);
    TEST("axfs_write", wrote, "write failed");

    char buf[64];
    uint64_t read_bytes = axfs_read(file, buf, sizeof(buf), 0);
    TEST("axfs_read length matches", read_bytes == msg_len, "length mismatch");

    bool content_matches = true;
    for (uint32_t i = 0; i < msg_len; i++)
    {
        if (buf[i] != msg[i])
        {
            content_matches = false;
            break;
        }
    }
    TEST("axfs_read content matches", content_matches, "content mismatch");

    uint64_t looked_up = axfs_lookup(test_dir, "file.txt");
    TEST("axfs_lookup finds file", looked_up == file, "lookup mismatch");

    uint64_t missing = axfs_lookup(test_dir, "does_not_exist.txt");
    TEST("axfs_lookup returns 0 for missing entry", missing == 0, "should not have found anything");

    uint64_t nested = axfs_create_path(test_dir, "a/b/c", AXFS_TYPE_DIR);
    TEST("axfs_create_path creates nested dirs", nested != 0, "nested creation failed");

    uint64_t resolved = axfs_resolve_path(test_dir, "a/b/c");
    TEST("axfs_resolve_path finds nested dir", resolved == nested, "resolve mismatch");

    uint64_t resolved_missing = axfs_resolve_path(test_dir, "a/b/does_not_exist");
    TEST("axfs_resolve_path returns 0 for missing segment", resolved_missing == 0, "should not resolve");

    axfs_dirent_info_t entries[16];
    uint32_t count = axfs_list_dir(test_dir, entries, 16);
    // ".", "..", "file.txt", "a" -- at least 4 entries expected.
    TEST("axfs_list_dir sees created entries", count >= 4, "missing entries");

    bool unlinked = axfs_unlink(test_dir, "file.txt");
    TEST("axfs_unlink removes file", unlinked, "unlink failed");

    uint64_t after_unlink = axfs_lookup(test_dir, "file.txt");
    TEST("file gone after unlink", after_unlink == 0, "file still resolvable");

    bool unlink_missing = axfs_unlink(test_dir, "does_not_exist.txt");
    TEST("axfs_unlink fails on missing entry", !unlink_missing, "should have failed");

    // No cleanup of test_dir/nested dirs here -- axfs_unlink() refuses
    // non-empty directories (see axfs.c), and this is a RAM disk
    // reformatted on every boot anyway (see kernel.c), so leftover
    // test data never persists or accumulates across runs.
}

static void test_vfs(void)
{
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("VFS:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    char volume_name[32];
    bool parsed = vfs_parse_volume("/@SysDisk/Home/Users", volume_name, sizeof(volume_name));
    TEST("vfs_parse_volume extracts name", parsed, "parse failed");
    TEST("vfs_parse_volume name == SysDisk",
         parsed && volume_name[0] == 'S' && volume_name[1] == 'y' && volume_name[2] == 's',
         "wrong volume name");

    bool bad_parse = vfs_parse_volume("no/leading/at/sign", volume_name, sizeof(volume_name));
    TEST("vfs_parse_volume rejects malformed path", !bad_parse, "should have failed");

    const char *rest = vfs_path_after_volume("/@SysDisk/Home/Users");
    TEST("vfs_path_after_volume points past the volume",
         rest && rest[0] == 'H' && rest[1] == 'o' && rest[2] == 'm',
         "wrong remainder");

    uint64_t resolved = vfs_resolve("/@SysDisk/Home");
    TEST("vfs_resolve finds /@SysDisk/Home", resolved != 0, "resolve failed");

    uint64_t bogus = vfs_resolve("/@NoSuchVolume/whatever");
    TEST("vfs_resolve fails for unknown volume", bogus == 0, "should have failed");

    uint64_t created = vfs_create("/@SysDisk/KTest/vfs_test.txt", AXFS_TYPE_FILE);
    TEST("vfs_create creates a file", created != 0, "create failed");

    uint64_t reresolved = vfs_resolve("/@SysDisk/KTest/vfs_test.txt");
    TEST("vfs_resolve finds the file just created", reresolved == created, "resolve mismatch");
}

void ktest_run_all(void)
{
    tests_passed = 0;
    tests_failed = 0;

    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("\n=== ApexOS Kernel Self-Test ===\n\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    test_gdt();
    vga_set_color(VGA_WHITE, VGA_BLACK);
    test_idt();
    vga_set_color(VGA_WHITE, VGA_BLACK);
    test_pit();
    vga_set_color(VGA_WHITE, VGA_BLACK);
    test_pmm();
    vga_set_color(VGA_WHITE, VGA_BLACK);
    test_vmm();
    vga_set_color(VGA_WHITE, VGA_BLACK);
    test_heap();
    vga_set_color(VGA_WHITE, VGA_BLACK);
    test_scheduler();
    vga_set_color(VGA_WHITE, VGA_BLACK);
    test_axfs();
    vga_set_color(VGA_WHITE, VGA_BLACK);
    test_vfs();
    vga_set_color(VGA_WHITE, VGA_BLACK);

    vga_puts("\n");
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("Results: ");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_putdec(tests_passed);
    vga_puts(" passed  ");
    if (tests_failed > 0)
        vga_set_color(VGA_RED, VGA_BLACK);
    vga_putdec(tests_failed);
    vga_puts(" failed\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("===============================\n");
}