// ApexKernel/Filesystem/AXFS/StringHelpers.c
// ApexOS -- Freestanding string helpers used across AXFS's own files

#include "Filesystem/AXFS/Internal.h"

uint32_t str_len(const char *s)
{
    uint32_t len = 0;
    while (s[len])
        len++;
    return len;
}

bool str_eq_n(const char *a, const char *b, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

void str_copy_n(char *dst, const char *src, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
        dst[i] = src[i];
}
