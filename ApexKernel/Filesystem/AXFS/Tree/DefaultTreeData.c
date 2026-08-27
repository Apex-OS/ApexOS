// ApexKernel/Filesystem/AXFS/Tree/DefaultTreeData.c
// ApexOS -- Default directory tree data (path list)
//
// Creates the base folder structure expected by ApexOS (Home/Admin,
// Home/Users/Zeemoud, System/SysCore/... etc.) -- like an installed OS
// (Windows, etc.) that already has its system folder structure on
// first boot, without the user having to build it by hand with mkdir.

#include "Filesystem/AXFS/Tree/DefaultTreeData.h"

const char *const AXFS_DEFAULT_TREE[] = {
    "Home/Admin/AppsData",
    "Home/Admin/Desktop",
    "Home/Admin/Documents",
    "Home/Admin/Downloads/Archives",
    "Home/Admin/Downloads/Documents",
    "Home/Admin/Downloads/Executable",
    "Home/Admin/Downloads/Media",
    "Home/Admin/Downloads/Others",
    "Home/Admin/Media/Images/Photos",
    "Home/Admin/Media/Images/Screenshots",
    "Home/Admin/Media/Music",
    "Home/Admin/Media/Videos/Movies",
    "Home/Admin/Media/Videos/ScreenRecordings",
    "Home/Users/Zeemoud/AppsData",
    "Home/Users/Zeemoud/Desktop",
    "Home/Users/Zeemoud/Documents",
    "Home/Users/Zeemoud/Downloads/Archives",
    "Home/Users/Zeemoud/Downloads/Documents",
    "Home/Users/Zeemoud/Downloads/Executable",
    "Home/Users/Zeemoud/Downloads/Media",
    "Home/Users/Zeemoud/Downloads/Others",
    "Home/Users/Zeemoud/Media/Images/Photos",
    "Home/Users/Zeemoud/Media/Images/Screenshots",
    "Home/Users/Zeemoud/Media/Music",
    "Home/Users/Zeemoud/Media/Videos/Movies",
    "Home/Users/Zeemoud/Media/Videos/ScreenRecordings",
    "System/Apps",
    "System/Boot",
    "System/Cache",
    "System/Drivers",
    "System/Help",
    "System/Languages/en-US",
    "System/Languages/fr-FR",
    "System/Libraries/AXL",
    "System/Libraries/DLL",
    "System/Logs",
    "System/Media",
    "System/Programs/AXP",
    "System/Programs/EXE",
    "System/ProgramsData",
    "System/Resources",
    "System/SysCore/ApexUI",
    "System/SysCore/SysApps",
    "System/SysCore/SysCache",
    "System/SysCore/SysLibraries",
    "System/SysCore/SysLogs",
    "System/SysCore/SysMedia",
    "System/SysCore/SysPrograms",
    "System/SysCore/SysResources/Cursors",
    "System/SysCore/SysResources/Fonts",
    "System/SysCore/SysResources/Themes",
    "System/SysCore/SysTemp",
    "System/Temp",
};

const unsigned long AXFS_DEFAULT_TREE_COUNT = sizeof(AXFS_DEFAULT_TREE) / sizeof(AXFS_DEFAULT_TREE[0]);
