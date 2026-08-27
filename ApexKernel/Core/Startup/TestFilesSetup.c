// ApexKernel/Core/Startup/TestFilesSetup.c
// ApexOS -- Debug/test file seeding on @SysDisk

#include "Core/Startup/TestFilesSetup.h"
#include "Formats/AXF/AXD/HeaderValidation.h"
#include "Formats/AXF/AXD/DriverInvocation.h"
#include "Formats/AXF/Embed/TestAXP.h"
#include "Formats/AXF/Embed/LibmathAXL.h"
#include "Formats/AXF/Embed/TestImportAXP.h"
#include "Formats/AXF/Embed/StubAXD.h"
#include "Formats/AXF/Embed/TestDriverAXP.h"
#include "ApexLib/Logs/Log.h"

void core_seed_test_files(axfs_volume_t *vol)
{
    uint64_t home_inode = axfs_resolve_path(vol, AXFS_ROOT_INODE, "Home/Users/Zeemoud");
    if (home_inode == 0)
        return;

    uint64_t axp_inode = axfs_create(vol, home_inode, "test.axp", AXFS_TYPE_FILE);
    if (axp_inode != 0)
        axfs_write(vol, axp_inode, test_axp_data, sizeof(test_axp_data), 0);

    // Tests the .axp -> .axl import/export mechanism (see
    // ApexToolchain/'s make_test_axl.py and
    // make_test_axp_with_import.py): libmath.axl exports add(a,b),
    // test_import.axp imports it and calls it with (3,4) -- should
    // print "add(3,4) = 7".
    uint64_t axl_inode = axfs_create(vol, home_inode, "libmath.axl", AXFS_TYPE_FILE);
    if (axl_inode != 0)
        axfs_write(vol, axl_inode, libmath_axl_data, sizeof(libmath_axl_data), 0);

    uint64_t import_axp_inode = axfs_create(vol, home_inode, "test_import.axp", AXFS_TYPE_FILE);
    if (import_axp_inode != 0)
        axfs_write(vol, import_axp_inode, test_import_axp_data, sizeof(test_import_axp_data), 0);

    // Tests the .axd mechanism (see ApexToolchain/'s make_test_axd.py):
    // stub.axd exports driver_init() (returns immediately, just
    // validates that loading + the Ring 0 call work) and
    // driver_ping(arg) -> 42 (validates axd_call() directly, without
    // going through SYS_DRIVER_CALL -- the full syscall test from a
    // .axp comes once this first level is validated).
    uint64_t axd_inode = axfs_create(vol, home_inode, "stub.axd", AXFS_TYPE_FILE);
    if (axd_inode == 0)
        return;

    axfs_write(vol, axd_inode, stub_axd_data, sizeof(stub_axd_data), 0);

    int driver_id = axd_load(vol, axd_inode);
    if (driver_id < 0)
    {
        klog_error("kernel: stub.axd failed to load");
        return;
    }

    uint64_t ping_result = axd_call(driver_id, 1, 0); // func_id 1 = driver_ping
    klog_ok_n("kernel: stub.axd driver_ping() returned", ping_result);

    // Tests the FULL Ring3 -> SYS_DRIVER_CALL -> Ring0 -> axd_call()
    // path (see ApexToolchain/'s make_test_driver_axp.py). Only
    // written if the driver actually loaded (driver_id=0 expected,
    // hardcoded in the generated .axp) -- running `run
    // test_driver.axp` in the shell should print "driver_ping() = 42".
    uint64_t drv_axp_inode = axfs_create(vol, home_inode, "test_driver.axp", AXFS_TYPE_FILE);
    if (drv_axp_inode != 0)
        axfs_write(vol, drv_axp_inode, test_driver_axp_data, sizeof(test_driver_axp_data), 0);
}
