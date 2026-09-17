// ApexKernel/Boot/UEFI/Main.c
// ApexOS -- UEFI bootloader entry point
//
// Orchestrates the boot sequence, in order (see this file's git
// history / project notes for the earlier Hello-World-only milestone
// that validated the PE32+/GPT+FAT32/OVMF chain before this real
// logic was written). Each step's actual logic lives in its own
// module -- this file only calls into them in the right order and
// handles the couple of decisions that only make sense at the
// top level (what's fatal vs. non-fatal, when to give up and hang):
//   1. GraphicsSetup.c: locate GOP, pick a usable mode, write
//      fb_boot_info_t to FB_BOOT_INFO_ADDR (0x9300) -- the exact same
//      address/layout the BIOS path's HardwareDetect.asm already
//      writes, so Drivers/Display/Framebuffer/FramebufferInit.c needs
//      zero changes to work from either boot path.
//   2. KernelLoad.c: open kernel.bin from the ESP and load it to
//      physical 0x100000 -- the same fixed load address
//      Boot/BIOS/Transition/LongModeEntry.asm's `call 0x100000` jumps
//      to, so the kernel itself needs no changes to be entered from
//      either path.
//   3. GptReader.c: read the disk's real GPT partition table (raw
//      block IO, not through the file system) to find where the ESP
//      ends, written to DISK_LAYOUT_INFO_ADDR (0x9400) -- see
//      Core/Boot/DiskLayout.h. Needed because @SysDisk/AXFS's start
//      position (Core/Startup/SysDiskSetup.c) can no longer be a
//      fixed hardcoded sector: it now shares a real disk with a real
//      ESP partition whose size isn't known until boot time.
//   4. AcpiFind.c: locate the ACPI RSDP via the firmware's
//      configuration table, written to ACPI_INFO_ADDR (0x9500) --
//      see Core/Acpi/Madt.c, which parses it (RSDT/XSDT -> MADT) for
//      SMP CPU discovery. On a BIOS boot the same acpi_info_t is
//      instead filled by the kernel itself scanning low memory
//      directly (see Core/Acpi/RsdpFind.c) -- UEFI firmware doesn't
//      leave the RSDP at a fixed scannable address the way BIOS does,
//      it must be asked for via this configuration table.
//   5. MemoryMapConvert.c: get the memory map and convert it to this
//      kernel's existing E820 entry format at MMAP_ADDR/
//      MMAP_COUNT_ADDR (0x9600/0x95FC) -- Core/Memory/MemoryMap/
//      MemoryMap.c already reads from there regardless of who wrote
//      it.
//   6. ExitBootServices() using the map key from step 5's call --
//      must be the *same* GetMemoryMap() call's key, so nothing
//      between step 5 and this call may allocate/free (which would
//      change the map and invalidate the key -- see the retry loop
//      below for how this is enforced).
//   7. Jump to the kernel at 0x100000. No page table setup needed
//      here (unlike the BIOS path's setup_paging) -- UEFI leaves CR3
//      pointing at an identity-mapped-for-low-memory page table on
//      x86_64, which survives ExitBootServices() and already covers
//      0x100000 (see this file's design notes / project research: the
//      UEFI spec guarantees boot-service-era allocations, which is
//      what AllocatePages(0x100000) in step 2 is, stay identity
//      mapped). The kernel builds its own full mapping later (see
//      Core/Memory/Virtual/IdentityMap.c) regardless of which boot
//      path got it there.

#include "Boot/UEFI/Efi.h"
#include "Boot/UEFI/GraphicsSetup.h"
#include "Boot/UEFI/KernelLoad.h"
#include "Boot/UEFI/GptReader.h"
#include "Boot/UEFI/MemoryMapConvert.h"
#include "Boot/UEFI/AcpiFind.h"

static CHAR16 msg_gop_fail[] = {'[', 'E', 'R', 'R', ']', ' ', 'G', 'O', 'P', ' ',
                                'n', 'o', 't', ' ', 'f', 'o', 'u', 'n', 'd', '\r', '\n', 0};

static void print(EFI_SYSTEM_TABLE *st, const CHAR16 *s)
{
    st->con_out->OutputString(st->con_out, (CHAR16 *)s);
}

EFI_STATUS efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table)
{
    fb_boot_info_t *fb_info = (fb_boot_info_t *)(uint64_t)FB_BOOT_INFO_ADDR;
    if (!setup_graphics(system_table, fb_info))
        print(system_table, msg_gop_fail); // non-fatal -- see GraphicsSetup.h's comment

    EFI_FILE_PROTOCOL *root = open_esp_root(system_table, image_handle);
    if (root == NULL || !load_kernel(root))
    {
        // Fatal -- nothing to jump to. Hangs rather than returning to
        // the firmware boot menu, same reasoning as this file's
        // earlier Hello-World milestone had for its own infinite loop.
        for (;;)
        {
        }
    }

    // ── Disk layout (ESP end LBA, for @SysDisk) ─────────────────────
    // Non-fatal if this fails -- see GptReader.h's comment and
    // Core/Startup/SysDiskSetup.c's fallback for a BIOS boot / failed
    // lookup (disk_layout_info_t.valid == 0 either way).
    disk_layout_info_t *disk_layout = (disk_layout_info_t *)(uint64_t)DISK_LAYOUT_INFO_ADDR;
    disk_layout->sysdisk_start_lba = 0;
    disk_layout->valid = 0;

    uint64_t esp_end_lba = 0;
    if (find_esp_end_lba(system_table, &esp_end_lba))
    {
        disk_layout->sysdisk_start_lba = esp_end_lba + 1;
        disk_layout->valid = 1;
    }

    // ── ACPI RSDP (for SMP CPU discovery via MADT) ──────────────────
    // Non-fatal if this fails, same reasoning as disk_layout above --
    // see Core/Acpi/Madt.c: no RSDP means the kernel just proceeds
    // with a single detected CPU (this one, already running) instead
    // of enumerating APs to wake.
    acpi_info_t *acpi_info = (acpi_info_t *)(uint64_t)ACPI_INFO_ADDR;
    acpi_info->rsdp_addr = 0;
    acpi_info->valid = 0;

    uint64_t rsdp_addr = 0;
    if (find_acpi_rsdp(system_table, &rsdp_addr))
    {
        acpi_info->rsdp_addr = rsdp_addr;
        acpi_info->valid = 1;
    }

    // ── Memory map + ExitBootServices ───────────────────────────────
    // GetMemoryMap()'s first call (with a too-small buffer) exists
    // purely to learn how big a buffer is actually needed
    // (EFI_BUFFER_TOO_SMALL is expected and not an error here) -- see
    // the UEFI spec's documented two-call pattern for this function.
    // A small margin is added to the reported size (descriptor_size *
    // 2 extra) since allocating the real buffer below is itself a
    // memory operation that can grow the map by a descriptor or two
    // between the sizing call and the real one.
    size_t map_size = 0;
    size_t map_key = 0;
    size_t descriptor_size = 0;
    uint32_t descriptor_version = 0;

    system_table->boot_services->GetMemoryMap(&map_size, NULL, &map_key, &descriptor_size, &descriptor_version);
    map_size += descriptor_size * 2;

    EFI_MEMORY_DESCRIPTOR *map_buffer = NULL;
    system_table->boot_services->AllocatePool(2 /* EfiLoaderData */, map_size, (void **)&map_buffer);

    // This GetMemoryMap() call's map_key is what gets passed to
    // ExitBootServices() below -- nothing between here and that call
    // may allocate or free (that would change the map and invalidate
    // the key, see this file's header comment step 4), which is why
    // convert_memory_map() only ever writes to the fixed MMAP_ADDR
    // destination rather than allocating anything of its own.
    system_table->boot_services->GetMemoryMap(&map_size, map_buffer, &map_key, &descriptor_size, &descriptor_version);

    convert_memory_map(map_buffer, map_size, descriptor_size);

    // ExitBootServices() can fail with EFI_INVALID_PARAMETER if the
    // map changed between the GetMemoryMap() call above and this call
    // (stale map_key) -- looping to retry with a fresh map handles the
    // rare case where firmware-internal activity (a timer callback,
    // etc.) changed the map in that narrow window, rather than this
    // bootloader just failing outright. Bounded to a handful of
    // attempts rather than looping forever on a firmware that's
    // persistently failing this for some other reason.
    EFI_STATUS exit_status = EFI_LOAD_ERROR;
    for (int attempt = 0; attempt < 3; attempt++)
    {
        exit_status = system_table->boot_services->ExitBootServices(image_handle, map_key);
        if (exit_status == EFI_SUCCESS)
            break;

        map_size = 0;
        system_table->boot_services->GetMemoryMap(&map_size, NULL, &map_key, &descriptor_size, &descriptor_version);
        map_size += descriptor_size * 2;
        system_table->boot_services->AllocatePool(2, map_size, (void **)&map_buffer);
        system_table->boot_services->GetMemoryMap(&map_size, map_buffer, &map_key, &descriptor_size, &descriptor_version);
        convert_memory_map(map_buffer, map_size, descriptor_size);
    }

    if (exit_status != EFI_SUCCESS)
    {
        // No ConOut available anymore to report this (ExitBootServices()
        // may have already torn down console services even on this
        // failure path in some firmware) -- hang is the only safe
        // option left.
        for (;;)
        {
        }
    }

    // ── Jump to the kernel ───────────────────────────────────────────
    // No boot services, no firmware console, no going back from here.
    // See this file's header comment, step 6, for why no page table
    // setup is needed first on x86_64.
    void (*kernel_entry)(void) = (void (*)(void))(uint64_t)KERNEL_LOAD_ADDR;
    kernel_entry();

    for (;;)
    {
    }

    return EFI_SUCCESS; // unreachable
}
