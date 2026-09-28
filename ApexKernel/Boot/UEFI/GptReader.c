// ApexKernel/Boot/UEFI/GptReader.c
// ApexOS -- UEFI bootloader: GPT partition table reading (see
// GptReader.h)

#include "Boot/UEFI/GptReader.h"

// EFI System Partition GUID (PartitionTypeGuid for an ESP, fixed by
// the UEFI spec) -- used to identify which GPT partition entry is the
// ESP when walking the partition array (see find_esp_end_lba() below).
// {C12A7328-F81F-11D2-BA4B-00A0C93EC93B}
#define ESP_PARTITION_TYPE_GUID \
    {0xC12A7328, 0xF81F, 0x11D2, {0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B}}

// Freestanding GUID comparison -- no libc's memcmp available (see
// Efi.h's header comment on why this whole bootloader avoids relying
// on one). Byte-for-byte, since EFI_GUID's layout is fixed wire
// format (see Efi.h) -- comparing field-by-field would be equivalent
// but this is simpler to write correctly.
static BOOLEAN guid_equal(const EFI_GUID *a, const EFI_GUID *b)
{
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    for (size_t i = 0; i < sizeof(EFI_GUID); i++)
    {
        if (pa[i] != pb[i])
            return FALSE;
    }
    return TRUE;
}

BOOLEAN find_esp_end_lba(EFI_SYSTEM_TABLE *st, uint64_t *out_end_lba)
{
    EFI_GUID block_io_guid = EFI_BLOCK_IO_PROTOCOL_GUID;
    size_t handle_count = 0;
    EFI_HANDLE *handles = NULL;

    if (st->boot_services->LocateHandleBuffer(ByProtocol, &block_io_guid, NULL,
                                              &handle_count, &handles) != EFI_SUCCESS)
        return FALSE;

    EFI_BLOCK_IO_PROTOCOL *disk_io = NULL;
    for (size_t i = 0; i < handle_count; i++)
    {
        EFI_BLOCK_IO_PROTOCOL *candidate = NULL;
        if (st->boot_services->HandleProtocol(handles[i], &block_io_guid, (void **)&candidate) != EFI_SUCCESS)
            continue;

        // The raw disk's EFI_BLOCK_IO_PROTOCOL has logical_partition
        // == FALSE; every partition on it (including the ESP) has its
        // own handle with logical_partition == TRUE -- see Efi.h's
        // EFI_BLOCK_IO_MEDIA comment and this file's UEFI spec
        // research notes on why partitions get their own Block IO
        // instance layered over the same underlying disk.
        if (candidate != NULL && candidate->media != NULL && !candidate->media->logical_partition)
        {
            disk_io = candidate;
            break;
        }
    }

    if (disk_io == NULL)
        return FALSE;

    uint32_t block_size = disk_io->media->block_size;
    if (block_size == 0)
        return FALSE; // defensive -- would make every LBA-to-byte-offset math below meaningless

    // GPT_HEADER is smaller than a typical 512-byte block, but
    // ReadBlocks() reads whole blocks -- buffer sized to block_size
    // (not sizeof(GPT_HEADER)) and the header read out of its start,
    // same convention as how the partition entry array is read below.
    uint8_t header_buf[4096]; // generous upper bound on block_size (512 or 4096 in practice)
    if (block_size > sizeof(header_buf))
        return FALSE;

    if (disk_io->ReadBlocks(disk_io, disk_io->media->media_id, 1 /* LBA 1: primary GPT header */,
                            block_size, header_buf) != EFI_SUCCESS)
        return FALSE;

    GPT_HEADER *gpt = (GPT_HEADER *)header_buf;
    if (gpt->signature != GPT_HEADER_SIGNATURE)
        return FALSE;

    EFI_GUID esp_guid = ESP_PARTITION_TYPE_GUID;

    // Partition entries are read one block at a time rather than all
    // at once -- the full array can be up to 16KB+ (see the UEFI spec
    // minimum this file's research notes cite), and this bootloader
    // only needs to find one specific entry, not hold the whole array
    // in memory at once.
    // gpt->size_of_partition_entry comes straight from the on-disk GPT
    // header -- a corrupted disk/header could have it read as 0,
    // which would make the division right below a #DE (divide error)
    // fault instead of a clean failure, crashing the bootloader with
    // no message rather than just reporting "no ESP found". Checked
    // before dividing, not after (the old `entries_per_block == 0`
    // check one line down could never actually run in that case --
    // the fault happens on the division itself, before its result is
    // ever compared to anything).
    if (gpt->size_of_partition_entry == 0)
        return FALSE;
    uint32_t entries_per_block = block_size / gpt->size_of_partition_entry;
    if (entries_per_block == 0)
        return FALSE; // defensive -- a partition entry shouldn't be able to exceed one block in practice

    uint8_t entry_buf[4096];
    if (block_size > sizeof(entry_buf))
        return FALSE;

    for (uint32_t entry_idx = 0; entry_idx < gpt->number_of_partition_entries; entry_idx++)
    {
        uint32_t block_in_array = entry_idx / entries_per_block;
        uint32_t offset_in_block = (entry_idx % entries_per_block) * gpt->size_of_partition_entry;

        if (offset_in_block == 0)
        {
            uint64_t lba = gpt->partition_entry_lba + block_in_array;
            if (disk_io->ReadBlocks(disk_io, disk_io->media->media_id, lba, block_size, entry_buf) != EFI_SUCCESS)
                return FALSE;
        }

        GPT_PARTITION_ENTRY *entry = (GPT_PARTITION_ENTRY *)(entry_buf + offset_in_block);

        // An all-zero partition_type_guid marks an unused entry --
        // the UEFI spec reserves the all-zero GUID for exactly this
        // (never a valid PartitionTypeGuid for a real partition).
        BOOLEAN is_unused = TRUE;
        for (size_t b = 0; b < sizeof(EFI_GUID); b++)
        {
            if (((uint8_t *)&entry->partition_type_guid)[b] != 0)
            {
                is_unused = FALSE;
                break;
            }
        }
        if (is_unused)
            continue;

        // Copied to a local (naturally aligned) variable before
        // guid_equal() takes its address -- entry->partition_type_guid
        // lives inside a packed struct (GPT_PARTITION_ENTRY, wire
        // format from the UEFI spec, see Efi.h) and so isn't
        // guaranteed to start on an 8-byte boundary the way a
        // standalone EFI_GUID normally would be. guid_equal() itself
        // only ever does byte-at-a-time access so the misalignment
        // was never a real correctness issue, just a compiler warning
        // worth silencing properly rather than ignoring.
        EFI_GUID entry_type_guid = entry->partition_type_guid;
        if (guid_equal(&entry_type_guid, &esp_guid))
        {
            *out_end_lba = entry->ending_lba;
            return TRUE;
        }
    }

    return FALSE; // walked every entry, no ESP-type partition found
}
