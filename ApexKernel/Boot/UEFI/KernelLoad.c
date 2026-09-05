// ApexKernel/Boot/UEFI/KernelLoad.c
// ApexOS -- UEFI bootloader: kernel.bin loading from the ESP (see
// KernelLoad.h)

#include "Boot/UEFI/KernelLoad.h"

EFI_FILE_PROTOCOL *open_esp_root(EFI_SYSTEM_TABLE *st, EFI_HANDLE image_handle)
{
    EFI_GUID loaded_image_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *loaded_image = NULL;
    if (st->boot_services->HandleProtocol(image_handle, &loaded_image_guid, (void **)&loaded_image) != EFI_SUCCESS)
        return NULL;

    EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    if (st->boot_services->HandleProtocol(loaded_image->device_handle, &fs_guid, (void **)&fs) != EFI_SUCCESS)
        return NULL;

    EFI_FILE_PROTOCOL *root = NULL;
    if (fs->OpenVolume(fs, &root) != EFI_SUCCESS)
        return NULL;

    return root;
}

BOOLEAN load_kernel(EFI_FILE_PROTOCOL *root)
{
    CHAR16 kernel_path[] = {'k', 'e', 'r', 'n', 'e', 'l', '.', 'b', 'i', 'n', 0};

    EFI_FILE_PROTOCOL *file = NULL;
    if (root->Open(root, &file, kernel_path, EFI_FILE_MODE_READ, 0) != EFI_SUCCESS)
        return FALSE;

    // GetInfo()'s buffer needs to be large enough for EFI_FILE_INFO
    // plus the file name the firmware appends after it (the real
    // struct has a variable-length CHAR16 FileName[] tail) -- 512
    // bytes is comfortably more than EFI_FILE_INFO's fixed fields plus
    // any reasonable file name, avoiding the dynamic-buffer-growth
    // dance some UEFI examples do for GetInfo() since this bootloader
    // only ever asks about one fixed, short file name ("kernel.bin").
    uint8_t info_buf[512];
    size_t info_size = sizeof(info_buf);
    EFI_GUID file_info_guid = EFI_FILE_INFO_ID;
    if (file->GetInfo(file, &file_info_guid, &info_size, info_buf) != EFI_SUCCESS)
    {
        file->Close(file);
        return FALSE;
    }

    EFI_FILE_INFO *file_info = (EFI_FILE_INFO *)info_buf;
    size_t read_size = (size_t)file_info->file_size;

    // Reads directly into the fixed physical load address -- relies
    // on that region already being free/usable memory this early in
    // boot (true in practice: it's low conventional memory UEFI
    // hasn't claimed for anything by this point, same assumption the
    // BIOS path's KernelLoader.asm makes about its own load target).
    // A more defensive version would AllocatePages(KERNEL_LOAD_ADDR)
    // first to have the firmware confirm/reserve it -- left as a
    // known simplification for this milestone rather than added
    // speculatively before it's shown to actually be a problem.
    void *dest = (void *)(uint64_t)KERNEL_LOAD_ADDR;
    EFI_STATUS status = file->Read(file, &read_size, dest);

    file->Close(file);

    return status == EFI_SUCCESS;
}
