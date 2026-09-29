#!/bin/bash
set -e

# ApexOS -- quick UEFI relaunch, no rebuild
#
# test-uefi.sh does a full rm -rf Build && make + rebuilds
# BOOTX64.EFI + recreates test_uefi.img from scratch every time --
# useful for a clean reproducible test, but slow when the only thing
# that changed between runs is nothing at all (e.g. just wanting to
# read the boot log again, or confirm a QEMU-flag-only fix like
# test-uefi.sh's if=ide -> no if= change). This script just re-runs
# QEMU against whatever /tmp/apexos-uefi-test already has on disk --
# run test-uefi.sh first at least once, or after any real source
# change.

WORKDIR=/tmp/apexos-uefi-test
cd "$WORKDIR"

if [ ! -f test_uefi.img ] || [ ! -f OVMF_CODE_4M.fd ]; then
    echo "No existing build found in $WORKDIR -- run test-uefi.sh first."
    exit 1
fi

echo "== Re-launching QEMU + OVMF (no rebuild, reusing existing test_uefi.img) =="

qemu-system-x86_64 \
  -M q35 \
  -smp 4 \
  -drive if=pflash,format=raw,readonly=on,file=OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=OVMF_VARS_4M.fd \
  -drive file=test_uefi.img,format=raw,index=0,media=disk \
  -m 256M \
  -netdev user,id=net0 \
  -device e1000,netdev=net0 \
  -serial stdio \
  -no-reboot \
  -no-shutdown \
  -display sdl
