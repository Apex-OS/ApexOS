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
#
# -MMD -MP: emit a .d dependency file alongside each .o, listing every
# header it #includes. Without this, editing a .h doesn't trigger a
# rebuild of the .c files that include it -- make only sees the .c
# file's own timestamp, so a stale .o can silently linger after a
# header-only change. -MP adds phony targets for each header so a
# deleted/renamed header doesn't break the build with a "no rule to
# make target" error.
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
             -MMD -MP               \
             -IApexKernel/Includes  \
             -IApexLib/Includes    \
             -IApexUI/Includes

LDFLAGS   := -T Linker.ld           \
             -nostdlib              \
             -z noexecstack

# ── Directories ────────────────────────────────────────────────────────────
BUILD     := Build
BOOT_DIR  := ApexKernel/Boot/BIOS

# ── Sources ────────────────────────────────────────────────────────────────
# Auto-discovered from disk via find -- adding or removing a .c/.asm file
# under these trees is picked up automatically on the next `make`, no
# manual list to maintain. Excludes Includes/ (headers only, no .c there
# anyway) implicitly since find only matches *.c/*.asm.
#
# ApexKernel/Boot/UEFI is excluded here even though it lives under
# ApexKernel/ -- those files (Main.c/efi_main and friends) are PE32+
# code built separately by test-uefi.sh with x86_64-w64-mingw32-gcc,
# not this freestanding-ELF kernel build. Without this exclusion they
# were being silently swept into kernel.elf by this same `find` (bug
# found while adding AcpiFind.c: it showed up in this Makefile's own
# `make all` output, which is what caught that Main.c/GraphicsSetup.c/
# KernelLoad.c/GptReader.c/MemoryMapConvert.c had already been doing
# the same thing unnoticed -- harmless so far only because none of
# them happened to collide with a kernel symbol, not because it was
# ever correct).
SRC_DIRS  := ApexKernel ApexLib ApexUI Tests

C_SRCS    := $(shell find $(SRC_DIRS) -name '*.c' -not -path 'ApexKernel/Boot/UEFI/*' | sort)
ASM_SRCS  := $(shell find ApexKernel/Arch -name '*.asm' | sort)

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
	@echo "  OK      apex.img ready ($$(du -sh $@ | cut -f1))"

# ── QEMU ───────────────────────────────────────────────────────────────────
run: all
	@echo "  QEMU    Launching ApexOS..."
	@qemu-system-x86_64              \
		-M q35                       \
		-smp 4                       \
		-drive file=$(BUILD)/apex.img,format=raw,index=0,media=disk \
		-m 256M                      \
		-netdev user,id=net0         \
  		-device e1000,netdev=net0    \
		-serial stdio                \
		-no-reboot                   \
		-no-shutdown                 \
		-display sdl

run-emergency: all
	@echo "  QEMU    Launching ApexOS (Fallback Test)..."
	@qemu-system-x86_64              \
        -M q35                       \
		-smp 4                       \
		-drive file=$(BUILD)/apex.img,format=raw,index=0,media=disk \
		-m 256M                      \
		-netdev user,id=net0         \
  		-device e1000,netdev=net0    \
		-serial stdio                \
		-no-reboot                   \
		-no-shutdown                 \
		-display sdl                 \
		-vga cirrus

run-debug-int: all
	@echo "  QEMU    Launching with interrupt logging..."
	@qemu-system-x86_64              \
		-M q35                       \
		-smp 4                       \
		-drive file=$(BUILD)/apex.img,format=raw,index=0,media=disk \
		-m 256M                      \
		-netdev user,id=net0         \
  		-device e1000,netdev=net0    \
		-serial stdio                \
		-no-reboot                   \
		-no-shutdown                 \
		-display sdl                 \
		-d int,cpu_reset             \
		-D qemu-debug.log

# ── QEMU + GDB debug ───────────────────────────────────────────────────────
debug: all
	@echo "  QEMU    Debug mode (GDB on port 1234)..."
	@qemu-system-x86_64          \
		-M q35                   \
		-smp 4                       \
		-drive file=$(BUILD)/apex.img,format=raw,index=0,media=disk \
		-m 256M                  \
		-netdev user,id=net0         \
  		-device e1000,netdev=net0    \
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

# ── Header dependency tracking ──────────────────────────────────────────────
# Pulls in the .d files gcc generated via -MMD -MP (see CFLAGS above), so
# editing a .h correctly triggers a rebuild of every .c that includes it.
# The leading '-' silences make's complaint on first build, when no .d
# files exist yet.
DEPS := $(C_OBJS:.o=.d)
-include $(DEPS)
