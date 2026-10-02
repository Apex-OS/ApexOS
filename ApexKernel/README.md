# ApexKernel

The kernel: boot, memory, interrupts, SMP, scheduling, syscalls, filesystem,
executable loading, drivers, and networking. Everything here runs in Ring 0,
except where noted.

For the system-wide picture (boot flow, memory layout, SMP bring-up sequence,
the syscall ABI, security hardening) see the [root README](../README.md#architecture) —
this file stays at the directory level: what's where, and why it's split
the way it is.

## Layout

```
Arch/x86_64/        CPU-specific assembly: GDT, syscall entry, context switch,
                     AP trampoline, MTRR — anything that can't be written in C
Boot/
  BIOS/              Two-stage real-mode bootloader (stage1 fits in the
                      512-byte MBR, stage2 does the real->long mode transition)
  Common/            Code shared by both boot paths
  UEFI/               GOP framebuffer, GPT reader, memory map conversion,
                      ExitBootServices — built separately (see test-uefi.sh),
                      not part of the normal `make` build (see Makefile)
Bus/                 PCI enumeration, serial (16550), USB (not started yet)
Core/
  Acpi/                RSDP/RSDT/MADT parsing
  Boot/Log/            The boot log itself (timestamped, dmesg-style; not to
                        be confused with ApexLib/Logs, the shell's logger)
  Interrupts/          GDT/IDT/TSS, PIC, exception and IRQ dispatch
  IPC/                 Inter-process communication — in progress
  Memory/              Physical (bitmap PMM) and virtual (4-level paging, W^X)
                        memory management, kernel heap
  SMP/                 ACPI-driven CPU/IOAPIC discovery, LAPIC, AP wakeup
                        trampoline, per-CPU data, IPIs, TLB shootdown
  Shell/               The kernel's interactive command-line shell
  Startup/             Early init: system disk setup, default test files
  Synchronization/     Atomics, spinlocks, mutexes, semaphores
  Syscalls/            syscall/sysret ABI, dispatcher, handlers, user-pointer
                        validation
  Tasking/             Process control blocks, context switching, scheduler
  Time/                PIT timer, uptime tracking
Devices/              Low-level device register helpers, grouped by kind
                      (Audio/GPU/Input/Storage/Time not populated yet, serial
                      16550 is)
Drivers/              Actual hardware drivers: display (VGA/framebuffer/
                      console), PS/2 input, AHCI storage, Intel e1000 network
Filesystem/
  AXFS/                 ApexOS's native filesystem (superblock, bitmap
                        allocation, inodes, journal)
  FAT32/                Planned, not started
  VFS/                  Thin routing layer over AXFS, `@Volume` path prefixes
Formats/
  AXF/                  ApexOS's own executable format: AXP (programs), AXL
                        (libraries), AXD (Ring-0 drivers)
  PE/                   Planned (for running PE binaries?), not started
Network/Protocols/    ARP, Ethernet, IPv4, ICMP, UDP, TCP — hand-written,
                      built directly on the e1000 driver
Includes/             Public headers, one-to-one mirroring every directory
                      above (see "Headers" below)
```

## Headers

Every `.c` file's public interface lives under `Includes/`, at the same
relative path with the directory renamed — `Core/Memory/Heap/Heap.c`'s
header is `Includes/Core/Memory/Heap/Heap.h`, not `Core/Memory/Heap/Heap.h`.
This keeps declarations separate from implementation without scattering
headers next to their `.c` files, and lets other code `#include
"Core/Memory/Heap/Heap.h"` without caring whether the included file is
kernel-internal or something ApexLib/ApexUI also needs.

`Meta/Scripts/Bash/mirror_includes.sh` exists to catch drift between a `.c`
file's structure and its header's location — run it after adding a new
directory under a kernel subsystem.

## Where things are decided, not just where they live

A few things worth knowing before reading code here, because they're not
obvious from file names alone:

- **The AP trampoline (`Arch/x86_64/ApBootTrampoline.asm`) runs at a fixed
  physical address (`0x8000`), not wherever the linker placed it in
  `kernel.elf`.** Every address inside that file is `0x8000 + <offset>`,
  computed at assembly time — see the file's own header comment.
- **Two independent logging systems exist on purpose.** `Core/Boot/Log/`
  (`boot_log_*`) is the timestamped boot log with right-aligned status tags,
  buffered until the framebuffer takes over. `ApexLib/Logs/` (`klog_*`) is
  older and writes immediately — used by the shell and anything that needs
  output before or independent of the boot sequence. Don't assume one can
  replace the other without checking `Core/Boot/Log/BootLog.c`'s pending-line
  logic first.
- **The kernel heap and user stacks are mapped non-executable.** Any new
  code path that writes machine code somewhere and expects to jump into it
  (a JIT, a relocatable loader) needs an explicit executable mapping — see
  `Core/Memory/Heap/HeapSetup.c`'s header comment and the W^X section of
  the root README.
