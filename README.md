# ApexOS 💪

> A modern x86-64 operating system, built from scratch in C and NASM assembly — no GRUB, no existing kernel, no shortcuts.

ApexOS aims to be lightweight like Linux, powerful like Windows, with a modern, customizable look and a kernel built around a few genuinely new ideas rather than just re-implementing what already exists.

**Current version: `v0.1.0`**

## What's working right now

**Boot & core**

- Custom multi-stage bootloader (stage1/stage2, no GRUB) → Long Mode → kernel
- GDT, IDT, TSS, PIC 8259, PIT timer
- Physical/virtual memory managers (bitmap PMM, 4-level paging VMM) and a kernel heap (`kmalloc`/`kfree`)
- Preemptive round-robin scheduler
- Full syscall interface (`syscall`/`sysret`) with real Ring 3 userspace execution

**Drivers**

- VGA text mode (80×25) with 500-line scrollback and mouse wheel support
- PS/2 keyboard (Shift, arrow keys, extended scancodes) and mouse
- Serial port (COM1), mirrored from VGA output — useful for headless debugging and automated tests

**Filesystem**

- **AXFS** — a custom filesystem (superblock, bitmap allocation, inodes, direct blocks) running on a RAM disk for now, with a real ApexOS-style default directory tree auto-created on format
- **VFS** — a thin layer above AXFS with `@Volume` path prefixes (`/@SysDisk/Home/...`), ready to support other filesystem types later (FAT32, ext4, NTFS)

**Kernel shell**

- Command history, cursor navigation, dynamic prompt, all the usual file commands (`ls`, `cd`, `mkdir`, `cat`, `rm`, `write`...)

**AXF — ApexOS's own executable format**

- `.axp` — standalone Ring 3 programs
- `.axl` — shared libraries, with real inter-binary function calls (import/export tables)
- `.axd` — kernel drivers, loaded Ring 0-only, callable from userspace via a dedicated syscall

**Testing**

- An internal self-test suite (`ktest.c`, 55+ assertions covering memory, filesystem, and VFS)
- An automated end-to-end test script that boots ApexOS headless in QEMU, drives the shell, and checks for crashes/regressions — see [`tools/tests/run_tests.sh`](tools/tests/run_tests.sh)

## What's next

Ghost Mode, an event-driven scheduler, a package format (`.axpkg`), and broader filesystem compatibility (NTFS/ext4/FAT32) are on the roadmap. See [`other/APEXOS.md`](other/APEXOS.md) for the original design notes and long-term vision.

## Building & running

**Requirements:** `nasm`, `gcc`, `ld`, `objcopy`, `qemu-system-x86_64`

```bash
cd apex-kernel
make run      # build + boot in QEMU
make debug    # same, with GDB listening on :1234
```

Once booted, press any key to reach the shell. Type `help` for available commands, or `test` to run the internal self-test suite.

## Automated tests

```bash
./tools/tests/run_tests.sh
```

Boots ApexOS in headless QEMU, runs the self-test suite plus a handful of real `.axp`/`.axl`/`.axd` programs, and checks the output for the expected results — and for the absence of any CPU exception (GPF, page fault, double fault). Requires `socat` or `nc` in addition to the build tools above.

## Project structure

```
apex-kernel/           Kernel source
├── arch/x86_64/        Boot code and low-level assembly (GDT, IDT, syscall entry, context switch)
├── core/                Kernel core: entry point, scheduler, syscalls, shell, AXF/AXD loaders
├── drivers/             VGA, keyboard, mouse, PIC, PIT, serial
├── fs/                  AXFS and VFS
├── mm/                  Physical/virtual memory managers, heap
└── include/apex/        Freestanding base types and low-level primitives

apex-os/                Userspace-side pieces
└── formats/axf/          The AXF executable format definition, shared with the kernel loader

tools/                  Test binary generators and the automated test suite
other/                  Design notes, logo, and other project assets
```

## Stack

- C (freestanding, no libc) + x86-64 assembly (Intel syntax) — NASM + GCC
- QEMU for development and testing
- GDB for debugging

## License

GPLv3 — see [`LICENSE`](LICENSE).
