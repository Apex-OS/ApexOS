# ApexProject/Makefile
# ApexOS -- Build system

# ── Toolchain ──────────────────────────────────────────────────────────────
ASM       := nasm
CC        := gcc
LD        := ld

# ── Flags ──────────────────────────────────────────────────────────────────
ASMFLAGS  := -f bin
ASMFLAGS_ELF := -f elf64

# -mno-sse -mno-sse2 -mno-mmx are mandatory: the kernel never saves/
# restores SSE/MMX state on a context switch or interrupt entry, so
# any GCC-generated SSE/MMX instruction (which GCC will emit by
# default at -O0 for things like struct copies) causes a silent crash
# the first time it runs, before any log output -- confirmed the hard
# way in an earlier session (see project notes).
CFLAGS    := -m64                   \
             -ffreestanding         \
             -fno-stack-protector   \
             -fno-builtin           \
             -fno-pic               \
             -mno-red-zone          \
             -mno-sse               \
             -mno-sse2              \
             -mno-mmx               \
             -nostdlib              \
             -nostdinc              \
             -Wall                  \
             -Wextra                \
             -std=c11               \
             -O0                    \
             -g                     \
             -IApexKernel/Includes  \
             -IApexLib/Includes

LDFLAGS   := -T Linker.ld           \
             -nostdlib              \
             -z noexecstack

# ── Directories ────────────────────────────────────────────────────────────
BUILD     := build
BOOT_DIR  := ApexKernel/Boot/BIOS

# ── Sources ────────────────────────────────────────────────────────────────
# Generated from the full ApexKernel/ApexLib/Tests tree -- see
# Meta/Scripts/ for the script that regenerates this list after adding
# or removing files (mirror_includes.sh's C_SRCS sibling).
C_SRCS    :=              ApexKernel/Arch/x86_64/GDTExtension.c \
             ApexKernel/Arch/x86_64/GDTSetup.c \
             ApexKernel/Bus/PCI/PCI.c \
             ApexKernel/Bus/Serial/Serial.c \
             ApexKernel/Core/Init.c \
             ApexKernel/Core/Interrupts/Exceptions/ExceptionNames.c \
             ApexKernel/Core/Interrupts/Exceptions/ExceptionReport.c \
             ApexKernel/Core/Interrupts/Handlers/InterruptDispatch.c \
             ApexKernel/Core/Interrupts/IDT/IDT.c \
             ApexKernel/Core/Interrupts/IRQ/IRQDispatch.c \
             ApexKernel/Core/Interrupts/PIC/PICControl.c \
             ApexKernel/Core/Interrupts/PIC/PICSetup.c \
             ApexKernel/Core/Memory/Heap/Heap.c \
             ApexKernel/Core/Memory/Heap/HeapSetup.c \
             ApexKernel/Core/Memory/Heap/HeapStats.c \
             ApexKernel/Core/Memory/MemoryMap/MemoryMap.c \
             ApexKernel/Core/Memory/MemoryMap/MemoryMapPrinter.c \
             ApexKernel/Core/Memory/Physical/PageBitmap.c \
             ApexKernel/Core/Memory/Physical/PhysicalMemoryDetection.c \
             ApexKernel/Core/Memory/Physical/PhysicalMemoryStats.c \
             ApexKernel/Core/Memory/Virtual/IdentityMap.c \
             ApexKernel/Core/Memory/Virtual/PageTables.c \
             ApexKernel/Core/Memory/Virtual/VirtualMemoryStats.c \
             ApexKernel/Core/Panic.c \
             ApexKernel/Core/Shell/CommandDispatcher.c \
             ApexKernel/Core/Shell/Commands/DiagnosticCommands.c \
             ApexKernel/Core/Shell/Commands/FilesystemCommands.c \
             ApexKernel/Core/Shell/Commands/SystemCommands.c \
             ApexKernel/Core/Shell/Commands/UserspaceTest.c \
             ApexKernel/Core/Shell/History.c \
             ApexKernel/Core/Shell/PathResolver.c \
             ApexKernel/Core/Shell/Prompt.c \
             ApexKernel/Core/Shell/Shell.c \
             ApexKernel/Core/Startup/SysDiskSetup.c \
             ApexKernel/Core/Startup/TestFilesSetup.c \
             ApexKernel/Core/Syscalls/ABI/SyscallSetup.c \
             ApexKernel/Core/Syscalls/Dispatcher/SyscallDispatcher.c \
             ApexKernel/Core/Syscalls/Handlers/SysDriverCall.c \
             ApexKernel/Core/Syscalls/Handlers/SysExit.c \
             ApexKernel/Core/Syscalls/Handlers/SysGetPID.c \
             ApexKernel/Core/Syscalls/Handlers/SysSleep.c \
             ApexKernel/Core/Syscalls/Handlers/SysWrite.c \
             ApexKernel/Core/SystemInfo.c \
             ApexKernel/Core/Tasking/Context/TSS.c \
             ApexKernel/Core/Tasking/Process/Process.c \
             ApexKernel/Core/Tasking/Scheduler/ProcessLauncher.c \
             ApexKernel/Core/Tasking/Scheduler/Scheduler.c \
             ApexKernel/Core/Tasking/Scheduler/SchedulerStats.c \
             ApexKernel/Core/Tasking/Scheduler/SchedulerTick.c \
             ApexKernel/Core/Time/PITCounter.c \
             ApexKernel/Core/Time/PITSetup.c \
             ApexKernel/Core/Time/Uptime.c \
             ApexKernel/Devices/Serial/16550/Serial16550.c \
             ApexKernel/Drivers/Display/Console/ConsoleCore.c \
             ApexKernel/Drivers/Display/Console/ConsoleCursor.c \
             ApexKernel/Drivers/Display/Console/ConsoleRender.c \
             ApexKernel/Drivers/Display/Console/ConsoleScrollback.c \
             ApexKernel/Drivers/Display/Drawing/Cursor.c \
             ApexKernel/Drivers/Display/Drawing/Shapes.c \
             ApexKernel/Drivers/Display/Drawing/Text.c \
             ApexKernel/Drivers/Display/Fonts/Font8x16.c \
             ApexKernel/Drivers/Display/Fonts/Font8x8.c \
             ApexKernel/Drivers/Display/Framebuffer/Framebuffer.c \
             ApexKernel/Drivers/Display/Framebuffer/FramebufferInit.c \
             ApexKernel/Drivers/Display/VGA.c \
             ApexKernel/Drivers/Input/Keyboard/KeyBuffer.c \
             ApexKernel/Drivers/Input/Keyboard/Keyboard.c \
             ApexKernel/Drivers/Input/Keyboard/ScancodeTables.c \
             ApexKernel/Drivers/Input/Mouse/Mouse.c \
             ApexKernel/Drivers/Input/Mouse/PS2Port.c \
             ApexKernel/Drivers/Storage/AHCI/AHCI.c \
             ApexKernel/Drivers/Storage/AHCI/CommandExecution.c \
             ApexKernel/Drivers/Storage/AHCI/Controller.c \
             ApexKernel/Drivers/Storage/AHCI/PortControl.c \
             ApexKernel/Drivers/Storage/AHCI/PortMemory.c \
             ApexKernel/Filesystem/AXFS/Allocation/BitOperations.c \
             ApexKernel/Filesystem/AXFS/Allocation/DataBlockAllocation.c \
             ApexKernel/Filesystem/AXFS/Allocation/InodeAllocation.c \
             ApexKernel/Filesystem/AXFS/Allocation/SuperblockSync.c \
             ApexKernel/Filesystem/AXFS/Content/FileRead.c \
             ApexKernel/Filesystem/AXFS/Content/FileWrite.c \
             ApexKernel/Filesystem/AXFS/Directory/DirectoryEntrySearch.c \
             ApexKernel/Filesystem/AXFS/Directory/Listing.c \
             ApexKernel/Filesystem/AXFS/Directory/Lookup.c \
             ApexKernel/Filesystem/AXFS/Directory/TypeQuery.c \
             ApexKernel/Filesystem/AXFS/Disk/AHCIBackend.c \
             ApexKernel/Filesystem/AXFS/Disk/DiskRouter.c \
             ApexKernel/Filesystem/AXFS/Disk/RAMBackend.c \
             ApexKernel/Filesystem/AXFS/Entry/DirectoryInsertion.c \
             ApexKernel/Filesystem/AXFS/Entry/EntryLifecycle.c \
             ApexKernel/Filesystem/AXFS/Entry/InodeCreation.c \
             ApexKernel/Filesystem/AXFS/Entry/Unlink.c \
             ApexKernel/Filesystem/AXFS/Inode/InodeRead.c \
             ApexKernel/Filesystem/AXFS/Inode/InodeWrite.c \
             ApexKernel/Filesystem/AXFS/Path/PathCreate.c \
             ApexKernel/Filesystem/AXFS/Path/PathResolve.c \
             ApexKernel/Filesystem/AXFS/Path/PathSegmentation.c \
             ApexKernel/Filesystem/AXFS/StringHelpers.c \
             ApexKernel/Filesystem/AXFS/Tree/DefaultTreeBuilder.c \
             ApexKernel/Filesystem/AXFS/Tree/DefaultTreeData.c \
             ApexKernel/Filesystem/AXFS/Volume/FormatWrite.c \
             ApexKernel/Filesystem/AXFS/Volume/LayoutCalculation.c \
             ApexKernel/Filesystem/AXFS/Volume/Mount.c \
             ApexKernel/Filesystem/VFS/FilesystemRouter.c \
             ApexKernel/Filesystem/VFS/VolumeRegistry.c \
             ApexKernel/Filesystem/VFS/VolumeSyntax.c \
             ApexKernel/Formats/AXF/AXD/DriverInvocation.c \
             ApexKernel/Formats/AXF/AXD/ExportResolution.c \
             ApexKernel/Formats/AXF/AXD/HeaderValidation.c \
             ApexKernel/Formats/AXF/AXL/LibraryLoader.c \
             ApexKernel/Formats/AXF/AXP/HeaderValidation.c \
             ApexKernel/Formats/AXF/AXP/ImportResolution.c \
             ApexKernel/Formats/AXF/AXP/ProcessLaunch.c \
             ApexKernel/Formats/AXF/AXP/SectionLoader.c \
             ApexLib/Logs/Log.c \
             Tests/Filesystem/AXFSTest.c \
             Tests/Filesystem/VFSTest.c \
             Tests/Kernel/GDTTest.c \
             Tests/Kernel/HeapTest.c \
             Tests/Kernel/IDTTest.c \
             Tests/Kernel/PITTest.c \
             Tests/Kernel/PMMTest.c \
             Tests/Kernel/SchedulerTest.c \
             Tests/Kernel/TestRunner.c \
             Tests/Kernel/VMMTest.c

ASM_SRCS  :=              ApexKernel/Arch/x86_64/ContextSwitch.asm \
             ApexKernel/Arch/x86_64/Entry.asm \
             ApexKernel/Arch/x86_64/GDT.asm \
             ApexKernel/Arch/x86_64/Interrupts/Interrupts.asm \
             ApexKernel/Arch/x86_64/Interrupts/TimerInterrupt.asm \
             ApexKernel/Arch/x86_64/SyscallEntry.asm \
             ApexKernel/Arch/x86_64/Userspace.asm

# ── Object files ───────────────────────────────────────────────────────────
C_OBJS    := $(patsubst %.c, $(BUILD)/%.o, $(C_SRCS))
ASM_OBJS  := $(patsubst %.asm, $(BUILD)/%.o, $(ASM_SRCS))

# ── Main targets ───────────────────────────────────────────────────────────
.PHONY: all clean run debug run-emergency run-debug-int

all: $(BUILD)/apex.img

# ── C compilation ──────────────────────────────────────────────────────────
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# ── ELF assembly (for .asm files linked with the kernel) ──────────────────
$(BUILD)/%.o: %.asm
	@mkdir -p $(dir $@)
	@echo "  ASM     $<"
	@$(ASM) $(ASMFLAGS_ELF) $< -o $@

# ── Kernel ELF link ────────────────────────────────────────────────────────
$(BUILD)/kernel.elf: $(C_OBJS) $(ASM_OBJS)
	@mkdir -p $(BUILD)
	@echo "  LD      kernel.elf"
	@$(LD) $(LDFLAGS) $^ -o $@

# ── Kernel binary extraction ───────────────────────────────────────────────
$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	@echo "  OBJCOPY kernel.bin"
	@objcopy -O binary $< $@

# ── Stage1 assembly (MBR) ──────────────────────────────────────────────────
$(BUILD)/stage1.bin: $(BOOT_DIR)/Stage1.asm
	@mkdir -p $(BUILD)
	@echo "  ASM     stage1.bin"
	@$(ASM) $(ASMFLAGS) $< -o $@

# ── Stage2 assembly ────────────────────────────────────────────────────────
# Stage2.asm %includes its Transition/ files at assemble time -- nasm
# resolves those relative to Stage2.asm's own directory, so no
# separate object/link step is needed for KernelLoader.asm/
# HardwareDetect.asm/LongModeEntry.asm the way the kernel's own .asm
# files (ASM_SRCS above) need.
$(BUILD)/stage2.bin: $(BOOT_DIR)/Stage2.asm $(BOOT_DIR)/Transition/KernelLoader.asm $(BOOT_DIR)/Transition/HardwareDetect.asm $(BOOT_DIR)/Transition/LongModeEntry.asm
	@mkdir -p $(BUILD)
	@echo "  ASM     stage2.bin"
	@$(ASM) $(ASMFLAGS) -I$(BOOT_DIR)/ $< -o $@

# ── Disk image creation ─────────────────────────────────────────────────────
$(BUILD)/apex.img: $(BUILD)/stage1.bin $(BUILD)/stage2.bin $(BUILD)/kernel.bin
	@echo "  IMG     apex.img"
	@# 81920 sectors = 40MB -- covers the 32MB Core/Startup/SysDiskSetup.c's
	@# SysDisk setup requests over AHCI (sysdisk_bytes_ahci), plus headroom.
	@# Used to be 2880 sectors (1.44MB, floppy-disk sized), which was
	@# smaller than that 32MB request -- axfs_disk_init_ahci's own size
	@# check (Filesystem/AXFS/Disk/AHCIBackend.c) correctly refused it
	@# ("requested range exceeds drive size"), but that's the dev image
	@# being too small for what the kernel asks for, not a real bug in the
	@# AHCI/AXFS code.
	@dd if=/dev/zero       of=$@              bs=512 count=81920 status=none
	@dd if=$(BUILD)/stage1.bin  of=$@ bs=512 count=1  conv=notrunc status=none
	@dd if=$(BUILD)/stage2.bin  of=$@ bs=512 seek=1  count=32 conv=notrunc status=none
	@dd if=$(BUILD)/kernel.bin  of=$@ bs=512 seek=34 conv=notrunc status=none
	@echo "  OK      apex.img ready ($(shell du -sh $@ | cut -f1))"

# ── QEMU ───────────────────────────────────────────────────────────────────
run: all
	@echo "  QEMU    Launching ApexOS..."
	@qemu-system-x86_64              \
		-M q35                       \
		-drive file=$(BUILD)/apex.img,format=raw,index=0,media=disk \
		-m 256M                      \
		-serial stdio                \
		-no-reboot                   \
		-no-shutdown                 \
		-display sdl

run-emergency: all
	@echo "  QEMU    Launching ApexOS (Fallback Test)..."
	@qemu-system-x86_64              \
        -M q35                       \
		-drive file=$(BUILD)/apex.img,format=raw,index=0,media=disk \
		-m 256M                      \
		-serial stdio                \
		-vga cirrus                  \
		-display sdl

run-debug-int: all
	@echo "  QEMU    Launching with interrupt logging..."
	@qemu-system-x86_64              \
		-M q35                       \
		-drive file=$(BUILD)/apex.img,format=raw,index=0,media=disk \
		-m 256M                      \
		-serial stdio                \
		-no-reboot                   \
		-no-shutdown                 \
		-accel tcg                   \
		-d int,cpu_reset              \
		-D qemu-debug.log             \
		-display sdl

# ── QEMU + GDB debug ───────────────────────────────────────────────────────
debug: all
	@echo "  QEMU    Debug mode (GDB on port 1234)..."
	@qemu-system-x86_64          \
		-M q35                   \
		-drive file=$(BUILD)/apex.img,format=raw,index=0,media=disk \
		-m 256M                  \
		-serial stdio            \
		-no-reboot               \
		-no-shutdown             \
		-display sdl             \
		-s -S

# ── Clean ──────────────────────────────────────────────────────────────────
clean:
	@echo "  CLEAN"
	@rm -rf $(BUILD)
	@echo "  OK"
