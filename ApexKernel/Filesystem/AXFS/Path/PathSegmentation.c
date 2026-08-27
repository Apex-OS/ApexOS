// ApexKernel/Filesystem/AXFS/Path/PathSegmentation.c
// ApexOS -- AXFS path segment parsing

#include "Filesystem/AXFS/Path/PathSegmentation.h"

const char *axfs_path_next_segment(const char *path, char *out, uint32_t out_size)
{
    uint32_t i = 0;
    while (path[i] && path[i] != '/' && i < out_size - 1)
    {
        out[i] = path[i];
        i++;
    }
    out[i] = '\0';

    if (path[i] == '/')
        return path + i + 1;
    return NULL; // last segment (end of string)
}
