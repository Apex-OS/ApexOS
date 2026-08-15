# apex-kernel/Makefile
# ApexOS -- Build system

# ── Toolchain ──────────────────────────────────────────────────────────────
ASM       := nasm
CC        := gcc
LD        := ld

# ── Flags ──────────────────────────────────────────────────────────────────
ASMFLAGS  := -f bin
ASMFLAGS_ELF := -f elf64

CFLAGS    := -m64                   \
             -ffreestanding         \
             -fno-stack-protector   \
             -fno-builtin           \
             -fno-pic               \
             -mno-red-zone          \
             -nostdlib              \
             -nostdinc              \
             -Wall                  \
             -Wextra                \
             -std=c11               \
             -O0                    \
             -g                     \
             -Iinclude

LDFLAGS   := -T linker.ld           \
             -nostdlib              \
             -z noexecstack

# ── Directories ────────────────────────────────────────────────────────────
BUILD     := build
BOOT_DIR  := arch/x86_64/boot

# ── Sources ────────────────────────────────────────────────────────────────
C_SRCS    := core/kernel.c            \
             core/gdt.c               \
             core/idt.c               \
             core/process.c           \
             core/scheduler.c         \
             core/shell/shell.c          \
             core/shell/shell_path.c     \
             core/shell/shell_ui.c       \
             core/shell/shell_history.c  \
             core/shell/shell_commands.c \
             core/ktest.c             \
             core/tss.c               \
             core/syscall.c           \
             core/usertest.c          \
             core/klog.c              \
             core/axf/axf_loader.c    \
             core/axf/axd.c           \
             drivers/vga.c            \
             drivers/fbcon.c          \
             drivers/framebuffer.c    \
             drivers/framebuffer_draw.c \
             drivers/font8x16.c       \
             drivers/pic.c            \
             drivers/pit.c            \
             drivers/serial.c         \
             drivers/keyboard.c       \
             drivers/mouse.c          \
             mm/mmap.c                \
             mm/pmm.c                 \
             mm/vmm.c                 \
             mm/heap.c                \
             fs/axfs_disk.c           \
             fs/axfs.c                \
             fs/axfs_bitmap.c         \
             fs/axfs_inode.c          \
             fs/axfs_path.c           \
             fs/axfs_tree.c           \
             fs/vfs.c

ASM_SRCS  := arch/x86_64/entry.asm         \
             arch/x86_64/gdt.asm           \
             arch/x86_64/idt.asm           \
             arch/x86_64/userspace.asm     \
             arch/x86_64/syscall_entry.asm \
             arch/x86_64/context_switch.asm

# ── Object files ───────────────────────────────────────────────────────────
C_OBJS    := $(patsubst %.c, $(BUILD)/%.o, $(C_SRCS))
ASM_OBJS  := $(patsubst %.asm, $(BUILD)/%.o, $(ASM_SRCS))

# ── Main targets ───────────────────────────────────────────────────────────
.PHONY: all clean run debug

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
$(BUILD)/stage1.bin: $(BOOT_DIR)/stage1.asm
	@mkdir -p $(BUILD)
	@echo "  ASM     stage1.bin"
	@$(ASM) $(ASMFLAGS) $< -o $@

# ── Stage2 assembly ────────────────────────────────────────────────────────
$(BUILD)/stage2.bin: $(BOOT_DIR)/stage2.asm
	@mkdir -p $(BUILD)
	@echo "  ASM     stage2.bin"
	@$(ASM) $(ASMFLAGS) $< -o $@

# ── Disk image creation ─────────────────────────────────────────────────────
$(BUILD)/apex.img: $(BUILD)/stage1.bin $(BUILD)/stage2.bin $(BUILD)/kernel.bin
	@echo "  IMG     apex.img"
	@dd if=/dev/zero       of=$@              bs=512 count=2880 status=none
	@dd if=$(BUILD)/stage1.bin  of=$@ bs=512 count=1  conv=notrunc status=none
	@dd if=$(BUILD)/stage2.bin  of=$@ bs=512 seek=1  count=32 conv=notrunc status=none
	@dd if=$(BUILD)/kernel.bin  of=$@ bs=512 seek=34 conv=notrunc status=none
	@echo "  OK      apex.img ready ($(shell du -sh $@ | cut -f1))"

# ── QEMU ───────────────────────────────────────────────────────────────────
run: all
	@echo "  QEMU    Launching ApexOS..."
	@qemu-system-x86_64              \
		-drive file=$(BUILD)/apex.img,format=raw,index=0,media=disk \
		-m 256M                      \
		-serial stdio                \
		-no-reboot                   \
		-no-shutdown                 \
		-display sdl

run-emergency: all
	@echo "  QEMU    Launching ApexOS (Fallback Test)..."
	@qemu-system-x86_64              \
		-drive file=$(BUILD)/apex.img,format=raw,index=0,media=disk \
		-m 256M                      \
		-serial stdio                \
		-vga cirrus                  \
		-display sdl

run-debug-int: all
	@echo "  QEMU    Launching with interrupt logging..."
	@qemu-system-x86_64              \
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
		-drive file=$(BUILD)/apex.img,format=raw,index=0,media=disk \
		-m 256M                  \
		-serial stdio            \
		-no-reboot               \
		-no-shutdown             \
		-s -S

# ── Clean ──────────────────────────────────────────────────────────────────
clean:
	@echo "  CLEAN"
	@rm -rf $(BUILD)
	@echo "  OK"