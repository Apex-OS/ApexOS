// apex-kernel/fs/axfs_path.c
// ApexOS -- AXFS nested path resolution and creation

#include "axfs_internal.h"

// Extracts the next segment of a path (up to the next '/' or the end
// of the string) into `out` (max size out_size, truncated if
// necessary -- should never happen with AXFS_MAX_NAME_LEN). Returns a
// pointer to the rest of the path (after the '/'), or NULL if this was
// the last segment.
static const char *path_next_segment(const char *path, char *out, uint32_t out_size)
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

uint64_t axfs_resolve_path(uint64_t start_inode, const char *path)
{
    if (!g_mounted)
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
        rest = path_next_segment(rest ? rest : path, segment, sizeof(segment));

        if (segment[0] == '\0')
            break; // empty segment (e.g. path with "//") -- skip

        uint64_t next = axfs_lookup(current, segment);
        if (next == 0)
            return 0; // segment not found

        current = next;
    } while (rest != NULL);

    return current;
}

uint64_t axfs_create_path(uint64_t start_inode, const char *path, axfs_inode_type_t final_type)
{
    if (!g_mounted)
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
        rest = path_next_segment(cursor, segment, sizeof(segment));
        bool is_last_segment = (rest == NULL);

        if (segment[0] == '\0')
        {
            if (is_last_segment)
                break; // path ending in '/' -- nothing more to do
            cursor = rest;
            continue; // empty segment in the middle (e.g. "//") -- skip
        }

        uint64_t existing = axfs_lookup(current, segment);
        if (existing != 0)
        {
            // The segment already exists -- if it's not the last
            // segment, check it's actually a directory (can't
            // continue traversing through a file).
            if (!is_last_segment)
            {
                axfs_inode_t existing_inode;
                if (!read_inode(existing, &existing_inode) ||
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
            uint64_t created = axfs_create(current, segment, type_to_create);
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