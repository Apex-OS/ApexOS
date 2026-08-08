// apex-kernel/fs/axfs_inode.c
// ApexOS -- AXFS inode I/O and directory entry lookup

#include "axfs_internal.h"

bool read_inode(uint64_t inode_num, axfs_inode_t *out)
{
    if (inode_num == 0)
        return false;
    uint64_t idx = inode_num - 1;
    uint64_t block = g_sb.inode_table_block + idx / AXFS_INODES_PER_BLOCK;
    uint64_t offset = (idx % AXFS_INODES_PER_BLOCK) * AXFS_INODE_SIZE;

    uint8_t buf[AXFS_BLOCK_SIZE];
    if (!axfs_disk_read_block(block, buf))
        return false;

    uint8_t *src = buf + offset;
    uint8_t *dst = (uint8_t *)out;
    for (uint32_t i = 0; i < AXFS_INODE_SIZE; i++)
        dst[i] = src[i];

    return true;
}

bool write_inode(uint64_t inode_num, const axfs_inode_t *in)
{
    if (inode_num == 0)
        return false;
    uint64_t idx = inode_num - 1;
    uint64_t block = g_sb.inode_table_block + idx / AXFS_INODES_PER_BLOCK;
    uint64_t offset = (idx % AXFS_INODES_PER_BLOCK) * AXFS_INODE_SIZE;

    uint8_t buf[AXFS_BLOCK_SIZE];
    if (!axfs_disk_read_block(block, buf))
        return false;

    uint8_t *dst = buf + offset;
    const uint8_t *src = (const uint8_t *)in;
    for (uint32_t i = 0; i < AXFS_INODE_SIZE; i++)
        dst[i] = src[i];

    return axfs_disk_write_block(block, buf);
}

uint64_t dir_find_entry(const axfs_inode_t *dir, const char *name,
                        uint64_t *out_block, uint32_t *out_offset)
{
    uint32_t name_len = str_len(name);

    for (int b = 0; b < AXFS_DIRECT_BLOCKS; b++)
    {
        if (dir->direct_blocks[b] == 0)
            continue;

        uint8_t buf[AXFS_BLOCK_SIZE];
        if (!axfs_disk_read_block(dir->direct_blocks[b], buf))
            continue;

        uint32_t offset = 0;
        while (offset < AXFS_BLOCK_SIZE)
        {
            axfs_dirent_t *ent = (axfs_dirent_t *)(buf + offset);
            if (ent->entry_size == 0)
                break;

            if (ent->inode_num != 0 && ent->name_len == name_len &&
                str_eq_n(ent->name, name, name_len))
            {
                if (out_block)
                    *out_block = dir->direct_blocks[b];
                if (out_offset)
                    *out_offset = offset;
                return ent->inode_num;
            }

            offset += ent->entry_size;
        }
    }

    return 0;
}

// ── String helpers (no libc in freestanding) ──────────────────────────────

uint32_t str_len(const char *s)
{
    uint32_t n = 0;
    while (s[n])
        n++;
    return n;
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