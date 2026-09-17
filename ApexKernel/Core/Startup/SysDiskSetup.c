// ApexKernel/Core/Startup/SysDiskSetup.c
// ApexOS -- @SysDisk volume bootstrap

#include "Core/Startup/SysDiskSetup.h"
#include "Core/Panic.h"
#include "Core/Boot/DiskLayout.h"
#include "Drivers/Storage/AHCI/AHCI.h"
#include "Filesystem/VFS/VolumeRegistry.h"
#include "Core/Boot/Log/BootLog.h"

axfs_volume_t *core_setup_sysdisk(void)
{
    static axfs_volume_t sysdisk_vol;

    ahci_drive_info_t drive = ahci_drive_info();

    // On a UEFI boot, Boot/UEFI/Main.c has already read the disk's
    // real GPT partition table and written where the ESP (EFI System
    // Partition) ends here -- @SysDisk must start after that, or
    // formatting it would overwrite the very partition UEFI firmware
    // needs to find this OS's bootloader on the next boot. See
    // DiskLayout.h for the full explanation of why this can no longer
    // be a single hardcoded constant now that a real ESP exists on the
    // same disk.
    //
    // On a BIOS boot (or if the UEFI GPT lookup itself failed --
    // see Boot/UEFI/Main.c: find_esp_end_lba()), disk_layout_info_t.valid
    // is 0 and this falls back to the historical fixed 2048-sector
    // offset: BIOS's Boot/BIOS/ path never uses real GPT partitions,
    // it just reserves a fixed header region for stage1/stage2/kernel.bin
    // (currently sectors 0-203, see the comment this replaced) and
    // @SysDisk starts right after with headroom to grow.
    const disk_layout_info_t *disk_layout = (const disk_layout_info_t *)(uint64_t)DISK_LAYOUT_INFO_ADDR;
    uint64_t sysdisk_start_sector;
    if (disk_layout->valid)
    {
        sysdisk_start_sector = disk_layout->sysdisk_start_lba;
        boot_log_ok_n("SysDisk: starting after UEFI ESP at sector", sysdisk_start_sector);
    }
    else
    {
        sysdisk_start_sector = 2048;
    }

    // V1: SysDisk gets a fixed-size region starting after the reserved
    // boot area above. Reserves a modest, fixed size for now (32MB) --
    // large enough for real use once run against a bigger real/virtual
    // disk. Against apex.img's current dev-image size (40MB, 81920
    // sectors -- see the Makefile's `dd count=81920`), this comfortably
    // fits alongside the 1MB boot reservation above.
    uint64_t sysdisk_bytes_ahci = 32ull * 1024 * 1024;

    // The RAM fallback needs its OWN (much smaller) size target -- it's
    // a kmalloc() out of the kernel heap, not disk space, and the heap
    // is nowhere near 32MB (HEAP_MAX_SIZE, see
    // Core/Memory/Heap/HeapSetup.h, is 6MB total for everything the
    // kernel allocates). Confirmed via GDB (breakpoint on kpanic):
    // reusing sysdisk_bytes_ahci here meant the RAM fallback's
    // kmalloc(32MB) failed exactly the same way AHCI's size check did,
    // so "fall back to RAM" still panicked right after -- there was
    // never a size a RAM disk could actually succeed at while sharing
    // this constant. 2MB is comfortably under the heap cap even with
    // everything else (the framebuffer back-buffer, process stacks,
    // etc.) also drawing from it, and plenty for dev/test use on the
    // RAM path -- the real, primary target for real use is the AHCI
    // path above, sized independently.
    uint64_t sysdisk_bytes_ram = 2ull * 1024 * 1024;

    bool disk_ready = drive.ready && axfs_disk_init_ahci(&sysdisk_vol.disk, sysdisk_start_sector, sysdisk_bytes_ahci);
    if (!disk_ready)
    {
        // Falls back to RAM whenever the AHCI attempt didn't pan out --
        // no drive at all (drive.ready == false), or a drive that IS
        // ready but too small for sysdisk_bytes_ahci
        // (axfs_disk_init_ahci's own size check, logged separately --
        // e.g. apex.img's 1.44MB dev image against the 32MB request
        // above). Previously this was a plain ternary (drive.ready ?
        // ahci : ram), which only ever attempted ONE backend -- a
        // ready-but-too-small drive hit axfs_disk_init_ahci's failure
        // and stopped right there, panicking with a message ("RAM and
        // AHCI both failed") that wasn't actually true: RAM was never
        // attempted.
        disk_ready = axfs_disk_init_ram(&sysdisk_vol.disk, sysdisk_bytes_ram);
    }

    if (!disk_ready)
    {
        kpanic("kernel: could not set up SysDisk's disk backend (RAM and AHCI both failed)");
    }

    if (!axfs_mount(&sysdisk_vol))
    {
        // First boot on this disk (or a RAM disk, which is always
        // "first boot") -- format it fresh. axfs_mount() stays silent
        // on this specific failure (see Filesystem/AXFS/Volume/Mount.c)
        // since it's expected here, not a real problem -- this is the
        // message that actually explains what's happening and why, for
        // anyone reading the boot log without already knowing AXFS's
        // internals.
        boot_log_info("SysDisk: no filesystem found, formatting now...");
        if (!axfs_format(&sysdisk_vol, "SysDisk") || !axfs_mount(&sysdisk_vol))
            kpanic("kernel: could not format/mount SysDisk");

        if (!axfs_create_default_tree(&sysdisk_vol))
            kpanic("kernel: could not create SysDisk's default directory tree");
    }

    vfs_register_volume("SysDisk", VFS_FS_AXFS, &sysdisk_vol);

    return &sysdisk_vol;
}
