// ApexKernel Tests/Filesystem/VFSTest.c
// ApexOS -- VFS self-test

#include "VFSTest.h"
#include "Tests/Kernel/TestRunner.h"
#include "Drivers/Display/VGA.h"
#include "Filesystem/VFS/VolumeSyntax.h"
#include "Filesystem/VFS/FilesystemRouter.h"
#include "Filesystem/AXFS/AXFS.h"

void test_vfs(void)
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
