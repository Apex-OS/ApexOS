// ApexKernel/Filesystem/AXFS/Path/PathCreate.c
// ApexOS -- AXFS nested path creation ("mkdir -p"-style)

#include "Filesystem/AXFS/AXFS.h"
#include "Filesystem/AXFS/Internal.h"
#include "Filesystem/AXFS/Path/PathSegmentation.h"

uint64_t axfs_create_path(axfs_volume_t *vol, uint64_t start_inode, const char *path, axfs_inode_type_t final_type)
{
    if (!vol->mounted)
        return 0;

    if (path[0] == '\0' || (path[0] == '.' && path[1] == '\0'))
        return start_inode;

    if (path[0] == '/')
        path++;

    uint64_t current = start_inode;
    char segment[AXFS_MAX_NAME_LEN + 1];
    const char *rest = path;
    const char *cursor = path;

    while (1)
    {
        bool truncated;
        rest = axfs_path_next_segment(cursor, segment, sizeof(segment), &truncated);
        if (truncated)
            return 0; // a segment longer than AXFS_MAX_NAME_LEN can never be created anyway
        bool is_last_segment = (rest == NULL);

        if (segment[0] == '\0')
        {
            if (is_last_segment)
                break; // path ending in '/' -- nothing more to do
            cursor = rest;
            continue; // empty segment in the middle (e.g. "//") -- skip
        }

        uint64_t existing = axfs_lookup(vol, current, segment);
        if (existing != 0)
        {
            // The segment already exists -- if it's not the last
            // segment, check it's actually a directory (can't
            // continue traversing through a file).
            if (!is_last_segment)
            {
                axfs_inode_t existing_inode;
                if (!read_inode(vol, existing, &existing_inode) ||
                    existing_inode.type != AXFS_TYPE_DIR)
                    return 0;
            }
            current = existing;
        }
        else
        {
            // Creates the missing segment. Intermediate segments are
            // always directories; the last segment takes the type
            // requested by the caller (final_type).
            axfs_inode_type_t type_to_create = is_last_segment ? final_type : AXFS_TYPE_DIR;
            uint64_t created = axfs_create(vol, current, segment, type_to_create);
            if (created == 0)
                return 0;
            current = created;
        }

        if (is_last_segment)
            break;
        cursor = rest;
    }

    return current;
}
