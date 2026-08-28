// ApexKernel Tests/Filesystem/AXFSTest.c
// ApexOS -- AXFS self-test

#include "AXFSTest.h"
#include "../Kernel/TestRunner.h"
#include "Drivers/Display/VGA.h"
#include "Filesystem/VFS/FilesystemRouter.h"
#include "Filesystem/AXFS/AXFS.h"

// Runs entirely under a dedicated "KTest" directory (created fresh
// under root, removed at the end) rather than under Home/Users/... --
// keeps this test run from ever touching real user files, and safe to
// run more than once per boot (each run starts from a clean slate).
void test_axfs(void)
{
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("AXFS:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    axfs_volume_t *vol = vfs_get_volume("/@SysDisk");
    TEST("SysDisk volume available", vol != NULL, "vfs_get_volume(\"/@SysDisk\") returned NULL");
    if (vol == NULL)
        return; // nothing else can run without it

    uint64_t test_dir = axfs_create(vol, AXFS_ROOT_INODE, "KTest", AXFS_TYPE_DIR);
    TEST("axfs_create(dir)", test_dir != 0, "could not create test dir");
    if (test_dir == 0)
        return; // nothing else can run without it

    TEST("axfs_get_type(dir) == DIR", axfs_get_type(vol, test_dir) == AXFS_TYPE_DIR, "wrong type");

    uint64_t file = axfs_create(vol, test_dir, "file.txt", AXFS_TYPE_FILE);
    TEST("axfs_create(file)", file != 0, "could not create file");

    uint64_t dup = axfs_create(vol, test_dir, "file.txt", AXFS_TYPE_FILE);
    TEST("axfs_create rejects duplicate name", dup == 0, "duplicate was allowed");

    const char *msg = "Hello, AXFS!";
    uint32_t msg_len = 0;
    while (msg[msg_len])
        msg_len++;

    bool wrote = axfs_write(vol, file, msg, msg_len, 0);
    TEST("axfs_write", wrote, "write failed");

    char buf[64];
    uint64_t read_bytes = axfs_read(vol, file, buf, sizeof(buf), 0);
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

    uint64_t looked_up = axfs_lookup(vol, test_dir, "file.txt");
    TEST("axfs_lookup finds file", looked_up == file, "lookup mismatch");

    uint64_t missing = axfs_lookup(vol, test_dir, "does_not_exist.txt");
    TEST("axfs_lookup returns 0 for missing entry", missing == 0, "should not have found anything");

    uint64_t nested = axfs_create_path(vol, test_dir, "a/b/c", AXFS_TYPE_DIR);
    TEST("axfs_create_path creates nested dirs", nested != 0, "nested creation failed");

    uint64_t resolved = axfs_resolve_path(vol, test_dir, "a/b/c");
    TEST("axfs_resolve_path finds nested dir", resolved == nested, "resolve mismatch");

    uint64_t resolved_missing = axfs_resolve_path(vol, test_dir, "a/b/does_not_exist");
    TEST("axfs_resolve_path returns 0 for missing segment", resolved_missing == 0, "should not resolve");

    axfs_dirent_info_t entries[16];
    uint32_t count = axfs_list_dir(vol, test_dir, entries, 16);
    // ".", "..", "file.txt", "a" -- at least 4 entries expected.
    TEST("axfs_list_dir sees created entries", count >= 4, "missing entries");

    bool unlinked = axfs_unlink(vol, test_dir, "file.txt");
    TEST("axfs_unlink removes file", unlinked, "unlink failed");

    uint64_t after_unlink = axfs_lookup(vol, test_dir, "file.txt");
    TEST("file gone after unlink", after_unlink == 0, "file still resolvable");

    bool unlink_missing = axfs_unlink(vol, test_dir, "does_not_exist.txt");
    TEST("axfs_unlink fails on missing entry", !unlink_missing, "should have failed");

    // Cleanup (removing test_dir and everything under it) happens in
    // test_axfs_cleanup(), after test_vfs() -- test_vfs() still needs
    // this tree (it creates a file under test_dir), so it can't be
    // torn down here yet.
}

// Tears down the "KTest" tree that test_axfs() and test_vfs() build
// (see the "no cleanup" comment at the end of test_axfs() -- true for
// a single run, but ktest_run_all() can be invoked more than once per
// boot via the shell's "test" command, and axfs_create() refuses a
// duplicate name, see Filesystem/AXFS/Entry/EntryLifecycle.c:
// dir_find_entry() check). Removes children before parents since
// axfs_unlink() refuses non-empty directories. Not wrapped in TEST()
// -- this is teardown, not an assertion, so it doesn't add to
// tests_passed/tests_failed; a failure here just means next run's
// axfs_create(dir) will report the real problem instead of silently
// leaving stale state behind.
void test_axfs_cleanup(void)
{
    axfs_volume_t *vol = vfs_get_volume("/@SysDisk");
    if (vol == NULL)
        return; // nothing to clean up (test_axfs never got the volume either)

    uint64_t test_dir = axfs_lookup(vol, AXFS_ROOT_INODE, "KTest");
    if (test_dir == 0)
        return; // nothing to clean up (test_axfs bailed out before creating it)

    axfs_unlink(vol, test_dir, "vfs_test.txt"); // created by test_vfs()

    uint64_t a = axfs_resolve_path(vol, test_dir, "a");
    uint64_t b = axfs_resolve_path(vol, test_dir, "a/b");
    uint64_t c = axfs_resolve_path(vol, test_dir, "a/b/c");
    if (c != 0 && b != 0)
        axfs_unlink(vol, b, "c");
    if (b != 0 && a != 0)
        axfs_unlink(vol, a, "b");
    if (a != 0)
        axfs_unlink(vol, test_dir, "a");

    axfs_unlink(vol, AXFS_ROOT_INODE, "KTest");
}
