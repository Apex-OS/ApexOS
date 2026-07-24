// apex-kernel/fs/axfs.c
// ApexOS -- AXFS (Apex File System) v1 -- driver
//
// Implements the format described in axfs.h on top of the generic
// disk layer axfs_disk.h (RAM disk for this V1). Low-level helpers
// (bitmap allocation, inode I/O, path resolution, default tree) live
// in their own files -- see axfs_internal.h for the shared contract.
// This file keeps the public API: init/format/mount, lookup/list/
// create/write/read/unlink.

#include "axfs_internal.h"
#include "../drivers/vga.h"
#include "../drivers/pit.h"
#include "../core/klog.h"

// ── Mounted volume state ──────────────────────────────────────────────────
axfs_superblock_t g_sb;
bool g_mounted = false;

#define AXFS_RAMDISK_SIZE (8u * 1024u * 1024u)
#define AXFS_BYTES_PER_INODE 16384u

bool axfs_init(void)
{
    return axfs_disk_init(AXFS_RAMDISK_SIZE);
}

bool axfs_format(const char *volume_label)
{
    uint64_t total_blocks = axfs_disk_total_blocks();
    if (total_blocks < 16)
    {
        klog_error("axfs_format: volume too small");
        return false;
    }

    uint64_t total_bytes = total_blocks * AXFS_BLOCK_SIZE;
    uint64_t total_inodes = total_bytes / AXFS_BYTES_PER_INODE;
    if (total_inodes < 16)
        total_inodes = 16;

    uint64_t inode_table_blocks = (total_inodes + AXFS_INODES_PER_BLOCK - 1) / AXFS_INODES_PER_BLOCK;

    uint64_t inode_bitmap_bytes = (total_inodes + 7) / 8;
    uint64_t inode_bitmap_blocks = (inode_bitmap_bytes + AXFS_BLOCK_SIZE - 1) / AXFS_BLOCK_SIZE;
    if (inode_bitmap_blocks == 0)
        inode_bitmap_blocks = 1;

    uint64_t block_bitmap_bytes = (total_blocks + 7) / 8;
    uint64_t block_bitmap_blocks = (block_bitmap_bytes + AXFS_BLOCK_SIZE - 1) / AXFS_BLOCK_SIZE;
    if (block_bitmap_blocks == 0)
        block_bitmap_blocks = 1;

    uint64_t inode_bitmap_start = 1;
    uint64_t block_bitmap_start = inode_bitmap_start + inode_bitmap_blocks;
    uint64_t inode_table_start = block_bitmap_start + block_bitmap_blocks;
    uint64_t data_start = inode_table_start + inode_table_blocks;

    if (data_start >= total_blocks)
    {
        klog_error("axfs_format: not enough space for metadata");
        return false;
    }

    axfs_superblock_t sb;
    uint8_t *sb_bytes = (uint8_t *)&sb;
    for (uint32_t i = 0; i < sizeof(sb); i++)
        sb_bytes[i] = 0;

    sb.magic = AXFS_MAGIC;
    sb.version = AXFS_VERSION;
    sb.block_size = AXFS_BLOCK_SIZE;
    sb.total_blocks = total_blocks;
    sb.free_blocks = total_blocks - data_start;
    sb.inode_bitmap_block = inode_bitmap_start;
    sb.block_bitmap_block = block_bitmap_start;
    sb.inode_table_block = inode_table_start;
    sb.data_start_block = data_start;
    sb.total_inodes = total_inodes;
    sb.free_inodes = total_inodes - 1;
    sb.root_inode = AXFS_ROOT_INODE;

    uint32_t label_len = str_len(volume_label);
    if (label_len > 31)
        label_len = 31;
    str_copy_n(sb.volume_label, volume_label, label_len);
    sb.volume_label[label_len] = '\0';

    if (!axfs_disk_write_block(0, &sb))
        return false;

    uint8_t zero_buf[AXFS_BLOCK_SIZE];
    for (uint32_t i = 0; i < AXFS_BLOCK_SIZE; i++)
        zero_buf[i] = 0;

    for (uint64_t i = 0; i < inode_bitmap_blocks; i++)
        axfs_disk_write_block(inode_bitmap_start + i, zero_buf);
    for (uint64_t i = 0; i < block_bitmap_blocks; i++)
        axfs_disk_write_block(block_bitmap_start + i, zero_buf);
    for (uint64_t i = 0; i < inode_table_blocks; i++)
        axfs_disk_write_block(inode_table_start + i, zero_buf);

    g_sb = sb;
    g_mounted = true;

    bitmap_set_bit(g_sb.inode_bitmap_block, 0, true);

    uint64_t root_data_block = alloc_data_block();
    if (root_data_block == 0)
    {
        g_mounted = false;
        klog_error("axfs_format: could not allocate root dir block");
        return false;
    }

    axfs_inode_t root;
    uint8_t *root_bytes = (uint8_t *)&root;
    for (uint32_t i = 0; i < sizeof(root); i++)
        root_bytes[i] = 0;

    root.type = AXFS_TYPE_DIR;
    root.permissions = 0777;
    root.owner_id = 0;
    root.size_bytes = 0;
    root.created_at = pit_get_ticks();
    root.modified_at = root.created_at;
    root.links_count = 1;
    root.direct_blocks[0] = root_data_block;

    uint8_t dir_buf[AXFS_BLOCK_SIZE];
    for (uint32_t i = 0; i < AXFS_BLOCK_SIZE; i++)
        dir_buf[i] = 0;

    uint32_t offset = 0;

    axfs_dirent_t *dot = (axfs_dirent_t *)(dir_buf + offset);
    dot->inode_num = AXFS_ROOT_INODE;
    dot->name_len = 1;
    dot->file_type = AXFS_TYPE_DIR;
    dot->name[0] = '.';
    dot->entry_size = AXFS_DIRENT_HEADER_SIZE + dot->name_len;
    offset += dot->entry_size;

    axfs_dirent_t *dotdot = (axfs_dirent_t *)(dir_buf + offset);
    dotdot->inode_num = AXFS_ROOT_INODE;
    dotdot->name_len = 2;
    dotdot->file_type = AXFS_TYPE_DIR;
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';
    dotdot->entry_size = AXFS_DIRENT_HEADER_SIZE + dotdot->name_len;
    offset += dotdot->entry_size;

    axfs_disk_write_block(root_data_block, dir_buf);

    root.size_bytes = offset;
    write_inode(AXFS_ROOT_INODE, &root);

    axfs_disk_write_block(0, &g_sb);

    klog_ok_prefix();
    vga_puts("AXFS formatted: ");
    vga_putdec(total_inodes);
    vga_puts(" inodes, ");
    vga_putdec(sb.free_blocks);
    vga_puts(" free data blocks\n");

    return true;
}

bool axfs_mount(void)
{
    axfs_superblock_t sb;
    if (!axfs_disk_read_block(0, &sb))
    {
        klog_error("axfs_mount: could not read superblock");
        return false;
    }

    if (sb.magic != AXFS_MAGIC)
    {
        klog_error("axfs_mount: bad magic (volume not formatted?)");
        return false;
    }

    if (sb.version != AXFS_VERSION)
    {
        klog_error("axfs_mount: unsupported version");
        return false;
    }

    g_sb = sb;
    g_mounted = true;

    klog_ok_prefix();
    vga_puts("AXFS mounted: \"");
    vga_puts(sb.volume_label);
    vga_puts("\" (");
    vga_putdec(sb.free_inodes);
    vga_puts("/");
    vga_putdec(sb.total_inodes);
    vga_puts(" inodes free, ");
    vga_putdec(sb.free_blocks);
    vga_puts(" data blocks free)\n");

    return true;
}

uint64_t axfs_lookup(uint64_t dir_inode, const char *name)
{
    if (!g_mounted)
        return 0;

    axfs_inode_t dir;
    if (!read_inode(dir_inode, &dir))
        return 0;
    if (dir.type != AXFS_TYPE_DIR)
        return 0;

    return dir_find_entry(&dir, name, NULL, NULL);
}

uint32_t axfs_list_dir(uint64_t dir_inode, axfs_dirent_info_t *out, uint32_t max_entries)
{
    if (!g_mounted || !out || max_entries == 0)
        return 0;

    axfs_inode_t dir;
    if (!read_inode(dir_inode, &dir))
        return 0;
    if (dir.type != AXFS_TYPE_DIR)
        return 0;

    uint32_t count = 0;

    for (int b = 0; b < AXFS_DIRECT_BLOCKS && count < max_entries; b++)
    {
        if (dir.direct_blocks[b] == 0)
            continue;

        uint8_t buf[AXFS_BLOCK_SIZE];
        if (!axfs_disk_read_block(dir.direct_blocks[b], buf))
            continue;

        uint32_t offset = 0;
        while (offset < AXFS_BLOCK_SIZE && count < max_entries)
        {
            axfs_dirent_t *ent = (axfs_dirent_t *)(buf + offset);
            if (ent->entry_size == 0)
                break;

            // Skips "tombstone" entries (deleted, inode_num == 0)
            if (ent->inode_num != 0)
            {
                out[count].inode_num = ent->inode_num;
                str_copy_n(out[count].name, ent->name, ent->name_len);
                out[count].name[ent->name_len] = '\0';
                out[count].type = ent->file_type;
                count++;
            }

            offset += ent->entry_size;
        }
    }

    return count;
}

axfs_inode_type_t axfs_get_type(uint64_t inode_num)
{
    if (!g_mounted)
        return AXFS_TYPE_FREE;

    axfs_inode_t inode;
    if (!read_inode(inode_num, &inode))
        return AXFS_TYPE_FREE;

    return (axfs_inode_type_t)inode.type;
}

uint64_t axfs_create(uint64_t parent_inode, const char *name, axfs_inode_type_t type)
{
    if (!g_mounted)
        return 0;

    axfs_inode_t parent;
    if (!read_inode(parent_inode, &parent))
        return 0;
    if (parent.type != AXFS_TYPE_DIR)
        return 0;

    if (dir_find_entry(&parent, name, NULL, NULL) != 0)
        return 0;

    uint32_t name_len = str_len(name);
    if (name_len == 0 || name_len > AXFS_MAX_NAME_LEN)
        return 0;

    uint64_t new_inode_num = alloc_inode_num();
    if (new_inode_num == 0)
        return 0;

    axfs_inode_t new_inode;
    uint8_t *nb = (uint8_t *)&new_inode;
    for (uint32_t i = 0; i < sizeof(new_inode); i++)
        nb[i] = 0;

    new_inode.type = (uint16_t)type;
    new_inode.permissions = 0777;
    new_inode.owner_id = 0;
    new_inode.size_bytes = 0;
    new_inode.created_at = pit_get_ticks();
    new_inode.modified_at = new_inode.created_at;
    new_inode.links_count = 1;

    if (type == AXFS_TYPE_DIR)
    {
        uint64_t dir_block = alloc_data_block();
        if (dir_block == 0)
        {
            bitmap_set_bit(g_sb.inode_bitmap_block, new_inode_num - 1, false);
            g_sb.free_inodes++;
            return 0;
        }
        new_inode.direct_blocks[0] = dir_block;

        uint8_t dir_buf[AXFS_BLOCK_SIZE];
        for (uint32_t i = 0; i < AXFS_BLOCK_SIZE; i++)
            dir_buf[i] = 0;

        uint32_t off = 0;
        axfs_dirent_t *dot = (axfs_dirent_t *)(dir_buf + off);
        dot->inode_num = new_inode_num;
        dot->name_len = 1;
        dot->file_type = AXFS_TYPE_DIR;
        dot->name[0] = '.';
        dot->entry_size = AXFS_DIRENT_HEADER_SIZE + dot->name_len;
        off += dot->entry_size;

        axfs_dirent_t *dotdot = (axfs_dirent_t *)(dir_buf + off);
        dotdot->inode_num = parent_inode;
        dotdot->name_len = 2;
        dotdot->file_type = AXFS_TYPE_DIR;
        dotdot->name[0] = '.';
        dotdot->name[1] = '.';
        dotdot->entry_size = AXFS_DIRENT_HEADER_SIZE + dotdot->name_len;
        off += dotdot->entry_size;

        axfs_disk_write_block(dir_block, dir_buf);
        new_inode.size_bytes = off;
    }

    write_inode(new_inode_num, &new_inode);

    uint16_t new_entry_size = AXFS_DIRENT_HEADER_SIZE + name_len;
    bool inserted = false;

    for (int b = 0; b < AXFS_DIRECT_BLOCKS && !inserted; b++)
    {
        if (parent.direct_blocks[b] == 0)
        {
            uint64_t new_block = alloc_data_block();
            if (new_block == 0)
                break;
            parent.direct_blocks[b] = new_block;
        }

        uint8_t buf[AXFS_BLOCK_SIZE];
        if (!axfs_disk_read_block(parent.direct_blocks[b], buf))
            continue;

        uint32_t offset = 0;
        while (offset + new_entry_size <= AXFS_BLOCK_SIZE)
        {
            axfs_dirent_t *ent = (axfs_dirent_t *)(buf + offset);

            if (ent->entry_size == 0)
            {
                ent->inode_num = new_inode_num;
                ent->entry_size = new_entry_size;
                ent->name_len = (uint8_t)name_len;
                ent->file_type = (uint8_t)type;
                str_copy_n(ent->name, name, name_len);

                axfs_disk_write_block(parent.direct_blocks[b], buf);
                inserted = true;
                break;
            }

            offset += ent->entry_size;
        }
    }

    if (!inserted)
    {
        bitmap_set_bit(g_sb.inode_bitmap_block, new_inode_num - 1, false);
        g_sb.free_inodes++;
        if (type == AXFS_TYPE_DIR && new_inode.direct_blocks[0] != 0)
            free_data_block(new_inode.direct_blocks[0]);
        return 0;
    }

    parent.modified_at = pit_get_ticks();
    write_inode(parent_inode, &parent);

    axfs_disk_write_block(0, &g_sb);

    return new_inode_num;
}

bool axfs_write(uint64_t inode_num, const void *data, uint64_t size, uint64_t offset)
{
    if (!g_mounted)
        return false;

    axfs_inode_t inode;
    if (!read_inode(inode_num, &inode))
        return false;
    if (inode.type != AXFS_TYPE_FILE)
        return false;

    uint64_t max_size = AXFS_DIRECT_BLOCKS * AXFS_BLOCK_SIZE;
    if (offset + size > max_size)
        return false;

    const uint8_t *src = (const uint8_t *)data;
    uint64_t written = 0;

    while (written < size)
    {
        uint64_t abs_offset = offset + written;
        uint64_t block_idx = abs_offset / AXFS_BLOCK_SIZE;
        uint64_t block_off = abs_offset % AXFS_BLOCK_SIZE;

        if (inode.direct_blocks[block_idx] == 0)
        {
            uint64_t new_block = alloc_data_block();
            if (new_block == 0)
                return false;
            inode.direct_blocks[block_idx] = new_block;
        }

        uint8_t buf[AXFS_BLOCK_SIZE];
        if (!axfs_disk_read_block(inode.direct_blocks[block_idx], buf))
            return false;

        uint64_t chunk = AXFS_BLOCK_SIZE - block_off;
        if (chunk > size - written)
            chunk = size - written;

        for (uint64_t i = 0; i < chunk; i++)
            buf[block_off + i] = src[written + i];

        axfs_disk_write_block(inode.direct_blocks[block_idx], buf);
        written += chunk;
    }

    if (offset + size > inode.size_bytes)
        inode.size_bytes = offset + size;
    inode.modified_at = pit_get_ticks();
    write_inode(inode_num, &inode);

    axfs_disk_write_block(0, &g_sb);

    return true;
}

uint64_t axfs_read(uint64_t inode_num, void *buf, uint64_t size, uint64_t offset)
{
    if (!g_mounted)
        return 0;

    axfs_inode_t inode;
    if (!read_inode(inode_num, &inode))
        return 0;
    if (inode.type != AXFS_TYPE_FILE)
        return 0;

    if (offset >= inode.size_bytes)
        return 0;

    uint64_t available = inode.size_bytes - offset;
    uint64_t to_read = (size < available) ? size : available;

    uint8_t *dst = (uint8_t *)buf;
    uint64_t read_total = 0;

    while (read_total < to_read)
    {
        uint64_t abs_offset = offset + read_total;
        uint64_t block_idx = abs_offset / AXFS_BLOCK_SIZE;
        uint64_t block_off = abs_offset % AXFS_BLOCK_SIZE;

        if (block_idx >= AXFS_DIRECT_BLOCKS || inode.direct_blocks[block_idx] == 0)
            break;

        uint8_t blk[AXFS_BLOCK_SIZE];
        if (!axfs_disk_read_block(inode.direct_blocks[block_idx], blk))
            break;

        uint64_t chunk = AXFS_BLOCK_SIZE - block_off;
        if (chunk > to_read - read_total)
            chunk = to_read - read_total;

        for (uint64_t i = 0; i < chunk; i++)
            dst[read_total + i] = blk[block_off + i];

        read_total += chunk;
    }

    return read_total;
}

bool axfs_unlink(uint64_t dir_inode, const char *name)
{
    if (!g_mounted)
        return false;

    axfs_inode_t dir;
    if (!read_inode(dir_inode, &dir))
        return false;
    if (dir.type != AXFS_TYPE_DIR)
        return false;

    uint64_t target_block = 0;
    uint32_t target_offset = 0;
    uint64_t target_inode_num = dir_find_entry(&dir, name, &target_block, &target_offset);
    if (target_inode_num == 0)
        return false;

    uint32_t name_len = str_len(name);
    if ((name_len == 1 && name[0] == '.') ||
        (name_len == 2 && name[0] == '.' && name[1] == '.'))
        return false;

    axfs_inode_t target;
    if (!read_inode(target_inode_num, &target))
        return false;

    if (target.type == AXFS_TYPE_DIR && target.size_bytes > AXFS_DIRENT_HEADER_SIZE * 2 + 3)
        return false;

    uint8_t buf[AXFS_BLOCK_SIZE];
    if (!axfs_disk_read_block(target_block, buf))
        return false;
    axfs_dirent_t *ent = (axfs_dirent_t *)(buf + target_offset);
    ent->inode_num = 0;
    axfs_disk_write_block(target_block, buf);

    for (int b = 0; b < AXFS_DIRECT_BLOCKS; b++)
    {
        if (target.direct_blocks[b] != 0)
            free_data_block(target.direct_blocks[b]);
    }

    bitmap_set_bit(g_sb.inode_bitmap_block, target_inode_num - 1, false);
    g_sb.free_inodes++;

    axfs_disk_write_block(0, &g_sb);

    return true;
}