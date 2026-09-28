// ApexKernel/Filesystem/AXFS/Path/PathResolve.c
// ApexOS -- AXFS nested path resolution

#include "Filesystem/AXFS/AXFS.h"
#include "Filesystem/AXFS/Path/PathSegmentation.h"

uint64_t axfs_resolve_path(axfs_volume_t *vol, uint64_t start_inode, const char *path)
{
    if (!vol->mounted)
        return 0;

    // Empty path or "." -- stay in place.
    if (path[0] == '\0' || (path[0] == '.' && path[1] == '\0'))
        return start_inode;

    // Skips a leading '/', if any (an "absolute" path here is relative
    // to the caller-supplied start_inode -- the volume's actual root
    // is handled by the VFS layer above, not here).
    if (path[0] == '/')
        path++;

    uint64_t current = start_inode;
    char segment[AXFS_MAX_NAME_LEN + 1];
    const char *rest = path;

    do
    {
        bool truncated;
        rest = axfs_path_next_segment(rest ? rest : path, segment, sizeof(segment), &truncated);
        if (truncated)
            return 0; // a segment longer than AXFS_MAX_NAME_LEN can never match a real name anyway

        if (segment[0] == '\0')
            break; // empty segment (e.g. path with "//") -- skip

        uint64_t next = axfs_lookup(vol, current, segment);
        if (next == 0)
            return 0; // segment not found

        current = next;
    } while (rest != NULL);

    return current;
}
