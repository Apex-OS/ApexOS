// ApexKernel/Core/Shell/PathResolver.c
// ApexOS -- Shell path resolution helpers

#include "Core/Shell/PathResolver.h"

char cwd[CWD_MAX_LEN] = "/@SysDisk/Home/Users/Zeemoud";

static void path_normalize(const char *raw, char *out, uint32_t out_size);

void path_make_absolute(const char *arg, char *out, uint32_t out_size)
{
    char raw[CWD_MAX_LEN];

    if (arg[0] == '/')
    {
        uint32_t i = 0;
        while (arg[i] && i < sizeof(raw) - 1)
        {
            raw[i] = arg[i];
            i++;
        }
        raw[i] = '\0';
    }
    else
    {
        uint32_t i = 0;
        while (cwd[i] && i < sizeof(raw) - 1)
        {
            raw[i] = cwd[i];
            i++;
        }
        if (i < sizeof(raw) - 1 && (i == 0 || raw[i - 1] != '/'))
            raw[i++] = '/';

        uint32_t j = 0;
        while (arg[j] && i < sizeof(raw) - 1)
            raw[i++] = arg[j++];
        raw[i] = '\0';
    }

    path_normalize(raw, out, out_size);
}

// Resolves "." and ".." segments in an already-built absolute path
// (with volume prefix "/@Volume/..."). The first segment ("@Volume")
// is always preserved -- ".." at the volume level is a no-op, there is
// no notion of a parent across volumes in v1.
static void path_normalize(const char *raw, char *out, uint32_t out_size)
{
    static char work[CWD_MAX_LEN];
    const char *segments[CWD_MAX_LEN / 2];
    uint32_t segment_count = 0;

    uint32_t wi = 0;
    while (raw[wi] && wi < sizeof(work) - 1)
    {
        work[wi] = raw[wi];
        wi++;
    }
    work[wi] = '\0';

    char *cursor = work;
    if (*cursor == '/')
        cursor++;

    while (*cursor)
    {
        char *seg_start = cursor;
        while (*cursor && *cursor != '/')
            cursor++;
        bool has_more = (*cursor == '/');
        if (has_more)
        {
            *cursor = '\0';
            cursor++;
        }

        if (seg_start[0] == '\0')
        {
            // Empty segment (double slash) -- skip.
        }
        else if (seg_start[0] == '.' && seg_start[1] == '\0')
        {
            // "." -- skip.
        }
        else if (seg_start[0] == '.' && seg_start[1] == '.' && seg_start[2] == '\0')
        {
            if (segment_count > 1)
                segment_count--;
        }
        else if (segment_count < CWD_MAX_LEN / 2)
        {
            segments[segment_count++] = seg_start;
        }

        if (!has_more)
            break;
    }

    uint32_t oi = 0;
    if (oi < out_size - 1)
        out[oi++] = '/';

    for (uint32_t s = 0; s < segment_count; s++)
    {
        if (s > 0 && oi < out_size - 1)
            out[oi++] = '/';
        uint32_t k = 0;
        while (segments[s][k] && oi < out_size - 1)
            out[oi++] = segments[s][k++];
    }
    out[oi] = '\0';
}

bool split_path_parent_name(const char *abs_path, char *parent_out,
                            uint32_t parent_size, char *name_out,
                            uint32_t name_size)
{
    uint32_t len = 0;
    while (abs_path[len])
        len++;

    uint32_t end = len;
    while (end > 1 && abs_path[end - 1] == '/')
        end--;

    int last_slash = -1;
    for (uint32_t i = 0; i < end; i++)
    {
        if (abs_path[i] == '/')
            last_slash = (int)i;
    }
    if (last_slash < 0)
        return false;

    uint32_t name_len = end - (last_slash + 1);
    if (name_len == 0 || name_len >= name_size)
        return false;

    for (uint32_t i = 0; i < name_len; i++)
        name_out[i] = abs_path[last_slash + 1 + i];
    name_out[name_len] = '\0';

    uint32_t parent_len = (uint32_t)last_slash;
    if (parent_len == 0)
        parent_len = 1; // keep the root slash alone if the parent is the absolute root
    if (parent_len >= parent_size)
        return false;

    for (uint32_t i = 0; i < parent_len; i++)
        parent_out[i] = abs_path[i];
    parent_out[parent_len] = '\0';

    return true;
}

char *split_first_space(char *args)
{
    while (*args)
    {
        if (*args == ' ')
        {
            *args = '\0';
            return args + 1;
        }
        args++;
    }
    return NULL;
}

bool str_eq(const char *a, const char *b)
{
    while (*a && *b)
    {
        if (*a != *b)
            return false;
        a++;
        b++;
    }
    return *a == *b;
}

bool str_startswith(const char *str, const char *prefix)
{
    while (*prefix)
    {
        if (*str != *prefix)
            return false;
        str++;
        prefix++;
    }
    return true;
}
