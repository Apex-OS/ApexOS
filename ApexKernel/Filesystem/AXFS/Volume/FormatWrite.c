// ApexKernel/Filesystem/AXFS/Volume/FormatWrite.c
// ApexOS -- AXFS volume formatting (superblock + bitmap + root inode write)

#include "Filesystem/AXFS/AXFS.h"
#include "Filesystem/AXFS/Internal.h"
#include "Filesystem/AXFS/Volume/LayoutCalculation.h"
#include "Core/Time/PITCounter.h"
#include "Drivers/Display/VGA.h"
#include "Logs/Log.h"

bool axfs_format(axfs_volume_t *vol, const char *volume_label)
{
    axfs_volume_layout_t layout;
    if (!axfs_calculate_layout(&vol->disk, &layout))
    {
        klog_error("axfs_format: volume too small or not enough space for metadata");
        return false;
    }

    axfs_superblock_t sb;
    uint8_t *sb_bytes = (uint8_t *)&sb;
    for (uint32_t i = 0; i < sizeof(sb); i++)
        sb_bytes[i] = 0;

    sb.magic = AXFS_MAGIC;
    sb.version = AXFS_VERSION;
    sb.block_size = AXFS_BLOCK_SIZE;
    sb.total_blocks = layout.total_blocks;
    sb.free_blocks = layout.total_blocks - layout.data_start;
    sb.inode_bitmap_block = layout.inode_bitmap_start;
    sb.block_bitmap_block = layout.block_bitmap_start;
    sb.inode_table_block = layout.inode_table_start;
    sb.data_start_block = layout.data_start;
    sb.total_inodes = layout.total_inodes;
    sb.free_inodes = layout.total_inodes - 1;
    sb.root_inode = AXFS_ROOT_INODE;

    uint32_t label_len = str_len(volume_label);
    if (label_len > 31)
        label_len = 31;
    str_copy_n(sb.volume_label, volume_label, label_len);
    sb.volume_label[label_len] = '\0';

    if (!axfs_disk_write_block(&vol->disk, 0, &sb))
        return false;

    uint8_t zero_buf[AXFS_BLOCK_SIZE];
    for (uint32_t i = 0; i < AXFS_BLOCK_SIZE; i++)
        zero_buf[i] = 0;

    for (uint64_t i = 0; i < layout.inode_bitmap_blocks; i++)
        axfs_disk_write_block(&vol->disk, layout.inode_bitmap_start + i, zero_buf);
    for (uint64_t i = 0; i < layout.block_bitmap_blocks; i++)
        axfs_disk_write_block(&vol->disk, layout.block_bitmap_start + i, zero_buf);
    for (uint64_t i = 0; i < layout.inode_table_blocks; i++)
        axfs_disk_write_block(&vol->disk, layout.inode_table_start + i, zero_buf);

    vol->sb = sb;
    vol->mounted = true;

    bitmap_set_bit(vol, vol->sb.inode_bitmap_block, 0, true);

    uint64_t root_data_block = alloc_data_block(vol);
    if (root_data_block == 0)
    {
        vol->mounted = false;
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

    axfs_disk_write_block(&vol->disk, root_data_block, dir_buf);

    root.size_bytes = offset;
    write_inode(vol, AXFS_ROOT_INODE, &root);

    axfs_disk_write_block(&vol->disk, 0, &vol->sb);

    klog_ok_prefix();
    vga_puts("AXFS formatted: ");
    vga_putdec(layout.total_inodes);
    vga_puts(" inodes, ");
    vga_putdec(sb.free_blocks);
    vga_puts(" free data blocks\n");

    return true;
}
