// ApexKernel/Filesystem/AXFS/Path/PathSegmentation.c
// ApexOS -- AXFS path segment parsing

#include "Filesystem/AXFS/Path/PathSegmentation.h"

const char *axfs_path_next_segment(const char *path, char *out, uint32_t out_size, bool *out_truncated)
{
    *out_truncated = false;

    uint32_t i = 0;
    while (path[i] && path[i] != '/' && i < out_size - 1)
    {
        out[i] = path[i];
        i++;
    }
    out[i] = '\0';

    // The loop above can stop for two different reasons that look
    // identical from here on: a real end of segment (path[i] is '/'
    // or '\0'), or simply running out of room in `out` while path[i]
    // is still some ordinary character in the middle of a name that's
    // too long. The old version of this function didn't distinguish
    // them: it fell straight to the `path[i] == '/'` check below,
    // which is false in the truncation case (path[i] is a name
    // character, not '/'), so it returned NULL -- "last segment" --
    // even when the untruncated remainder of `path` might have had
    // several more real segments after it, silently discarding them.
    if (path[i] != '\0' && path[i] != '/')
    {
        *out_truncated = true;
        return NULL;
    }

    if (path[i] == '/')
        return path + i + 1;
    return NULL; // last segment (end of string)
}
