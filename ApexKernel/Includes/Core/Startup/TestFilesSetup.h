// ApexKernel/Includes/Core/Startup/TestFilesSetup.h
// ApexOS -- Debug/test file seeding on @SysDisk

#ifndef APEX_CORE_STARTUP_TEST_FILES_SETUP_H
#define APEX_CORE_STARTUP_TEST_FILES_SETUP_H

#include "Filesystem/AXFS/AXFS.h"

// DEBUG/TEST: writes the embedded test .axp/.axl/.axd files into
// `vol`'s Home/Users/Zeemoud folder, to exercise the AXF/AXD loaders
// (`run test.axp`, etc.) without needing a real way to put files on
// the volume yet. To be removed once there's a real way to put files
// on the volume (ATA driver + real persistence, or upload over the
// serial port).
void core_seed_test_files(axfs_volume_t *vol);

#endif // APEX_CORE_STARTUP_TEST_FILES_SETUP_H
