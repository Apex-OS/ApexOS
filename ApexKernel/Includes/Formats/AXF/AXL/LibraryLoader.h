// ApexKernel/Includes/Formats/AXF/AXL/LibraryLoader.h
// ApexOS -- AXL (.axl library) loading and function resolution

#ifndef APEX_FORMATS_AXF_AXL_LIBRARY_LOADER_H
#define APEX_FORMATS_AXF_AXL_LIBRARY_LOADER_H

#include "Core/Types.h"

#include "Filesystem/AXFS/AXFS.h"

// Loads a .axl (looked up by name in `dir_inode`), maps it at
// USER_LIB_ADDR, and returns the ABSOLUTE address of the function
// `func_name` found in its export table. Returns 0 on failure
// (library not found, wrong type, function missing...).
//
// V1 known limitation: if several imports come from the SAME .axl, it
// gets reloaded/remapped each time (no cache). If two imports come
// from TWO different .axl files, the second one overwrites the
// first's mapping at USER_LIB_ADDR (a single fixed address) -- so in
// V1 a .axp can really only rely on ONE .axl at a time (but can
// import several FUNCTIONS from that same .axl). Lifting this
// limitation would need dynamic management of multiple memory regions
// per process, planned for later.
uint64_t load_axl_and_resolve(axfs_volume_t *vol, uint64_t dir_inode, const char *lib_name,
                              const char *func_name);

#endif // APEX_FORMATS_AXF_AXL_LIBRARY_LOADER_H
