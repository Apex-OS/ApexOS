// ApexKernel/Includes/Filesystem/AXFS/Path/PathSegmentation.h
// ApexOS -- AXFS path segment parsing (internal helper for PathResolve.c/PathCreate.c)

#ifndef APEX_FILESYSTEM_AXFS_PATH_SEGMENTATION_H
#define APEX_FILESYSTEM_AXFS_PATH_SEGMENTATION_H

#include "Core/Types.h"

// Extracts the next segment of a path (up to the next '/' or the end
// of the string) into `out` (max size out_size). Returns a pointer to
// the rest of the path (after the '/'), or NULL if this was the last
// segment.
//
// `*out_truncated` is set to true if the segment itself was longer
// than out_size-1 and had to be cut short -- callers should treat
// this as an error (reject the path) rather than silently continuing
// on a truncated name, which could otherwise resolve to the wrong
// inode (if a same-prefix name of the right truncated length happens
// to exist) or misreport "last segment" (NULL) when the path in fact
// had more after the part that didn't fit -- see this file's .c for
// why the old behavior conflated the two. Set to false on every other
// return, including when path itself is empty. Callers that don't
// pass AXFS_MAX_NAME_LEN+1 for out_size (every current caller does)
// are the only ones actually at risk of ever seeing this set to true.
const char *axfs_path_next_segment(const char *path, char *out, uint32_t out_size, bool *out_truncated);

#endif // APEX_FILESYSTEM_AXFS_PATH_SEGMENTATION_H
