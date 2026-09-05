#!/bin/bash
set -e

WORKDIR=/tmp/apexos-uefi-test
mkdir -p "$WORKDIR"
cd "$WORKDIR"

echo "== Building the real kernel (BIOS + UEFI share the same kernel.bin) =="
cd "$OLDPWD"
rm -rf Build
make
cp Build/kernel.bin "$WORKDIR/kernel.bin"
cd "$WORKDIR"

echo "== Compiling UEFI bootloader modules =="
UEFI_CFLAGS="-Wall -Wextra -ffreestanding -fshort-wchar -mno-red-zone -fno-stack-check -fno-stack-protector -mno-stack-arg-probe -I$OLDPWD/ApexKernel/Includes"

for f in Main GraphicsSetup KernelLoad GptReader MemoryMapConvert; do
  echo "  CC  $f.c"
  x86_64-w64-mingw32-gcc $UEFI_CFLAGS \
    -c "$OLDPWD/ApexKernel/Boot/UEFI/$f.c" \
    -o "$f.o"
done

echo "== Linking BOOTX64.EFI =="
x86_64-w64-mingw32-gcc \
  -nostdlib \
  -Wl,-entry,efi_main \
  -Wl,-subsystem,10 \
  -Wl,--dll \
  Main.o GraphicsSetup.o KernelLoad.o GptReader.o MemoryMapConvert.o \
  -o BOOTX64.EFI

file BOOTX64.EFI

echo "== Building GPT disk image (100MB ESP + rest for @SysDisk) =="
# ESP must be >= ~33MB for a real FAT32 filesystem (mkfs.vfat warns/
# misbehaves below that) -- 100MB here leaves plenty of margin.
# Total image: 200MB -> ESP 100MB + ~100MB for @SysDisk (kernel wants
# 32MB minimum for @SysDisk, see SysDiskSetup.c).
rm -f test_uefi.img
dd if=/dev/zero of=test_uefi.img bs=1M count=200
parted -s test_uefi.img mklabel gpt
parted -s test_uefi.img mkpart ESP fat32 1MiB 100MiB
parted -s test_uefi.img set 1 esp on
parted -s test_uefi.img mkpart SysDisk 100MiB 100%
parted -s test_uefi.img print

echo "== Formatting ESP and copying boot files =="
LOOPDEV=$(sudo losetup --show -f -o 1048576 --sizelimit $((104857600-1048576)) test_uefi.img)
sudo mkfs.vfat -F 32 -n ESP "$LOOPDEV"
sudo mmd -i "$LOOPDEV" ::/EFI
sudo mmd -i "$LOOPDEV" ::/EFI/BOOT
sudo mcopy -i "$LOOPDEV" BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI
sudo mcopy -i "$LOOPDEV" kernel.bin ::/kernel.bin
sudo mdir -i "$LOOPDEV" ::/
sudo losetup -d "$LOOPDEV"

echo "== Booting with QEMU + OVMF =="
cp /usr/share/OVMF/OVMF_CODE_4M.fd .
cp /usr/share/OVMF/OVMF_VARS_4M.fd .

qemu-system-x86_64 \
  -M q35 \
  -drive if=pflash,format=raw,readonly=on,file=OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=OVMF_VARS_4M.fd \
  -drive file=test_uefi.img,format=raw,if=ide \
  -m 256M
