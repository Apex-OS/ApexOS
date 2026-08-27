// ApexKernel/Includes/Filesystem/VFS/VolumeSyntax.h
// ApexOS -- VFS path syntax parsing ("/@Volume/rest")

#ifndef APEX_FILESYSTEM_VFS_VOLUME_SYNTAX_H
#define APEX_FILESYSTEM_VFS_VOLUME_SYNTAX_H

#include "Core/Types.h"

// Extracts the volume name from an absolute path ("/@SysDisk/Home" ->
// "SysDisk"). Written into `out` (max size out_size). Returns false
// if the path doesn't match the expected format (must start with
// "/@").
bool vfs_parse_volume(const char *path, char *out, uint32_t out_size);

// Returns a pointer to the rest of the path AFTER the volume name
// (i.e. what should be passed to axfs_resolve_path/axfs_create_path).
// E.g. "/@SysDisk/Home/Users" -> pointer to "Home/Users". Returns NULL
// if the path doesn't match the expected format.
const char *vfs_path_after_volume(const char *path);

#endif // APEX_FILESYSTEM_VFS_VOLUME_SYNTAX_H
