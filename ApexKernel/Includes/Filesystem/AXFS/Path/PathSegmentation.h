// ApexKernel/Includes/Filesystem/AXFS/Path/PathSegmentation.h
// ApexOS -- AXFS path segment parsing (internal helper for PathResolve.c/PathCreate.c)

#ifndef APEX_FILESYSTEM_AXFS_PATH_SEGMENTATION_H
#define APEX_FILESYSTEM_AXFS_PATH_SEGMENTATION_H

#include "Core/Types.h"

// Extracts the next segment of a path (up to the next '/' or the end
// of the string) into `out` (max size out_size, truncated if
// necessary -- should never happen with AXFS_MAX_NAME_LEN). Returns a
// pointer to the rest of the path (after the '/'), or NULL if this
// was the last segment.
const char *axfs_path_next_segment(const char *path, char *out, uint32_t out_size);

#endif // APEX_FILESYSTEM_AXFS_PATH_SEGMENTATION_H
