// ApexKernel/Filesystem/VFS/VolumeSyntax.c
// ApexOS -- VFS path syntax parsing ("/@Volume/rest")

#include "Filesystem/VFS/VolumeSyntax.h"

bool vfs_parse_volume(const char *path, char *out, uint32_t out_size)
{
    // Expected format: "/@VolumeName/rest/of/path" or "/@VolumeName".
    if (path[0] != '/' || path[1] != '@')
        return false;

    const char *start = path + 2; // skip "/@"
    uint32_t i = 0;
    while (start[i] && start[i] != '/' && i < out_size - 1)
    {
        out[i] = start[i];
        i++;
    }
    out[i] = '\0';

    return i > 0; // an empty volume name ("/@/...") is invalid
}

const char *vfs_path_after_volume(const char *path)
{
    if (path[0] != '/' || path[1] != '@')
        return NULL;

    const char *p = path + 2;
    while (*p && *p != '/')
        p++;

    if (*p == '/')
        return p + 1; // points right after the '/'
    return p;         // end of string -- path was just "/@Volume", rest = ""
}
