<h1 align="center">ApexOS</h1>

<p align="center">
  A 64-bit operating system written from scratch in C and NASM assembly.<br>
  Custom bootloaders, SMP kernel, native filesystem, executable format and network stack.
</p>

<p align="center">
  <img alt="Version" src="https://img.shields.io/badge/version-0.1.5-blue">
  <img alt="Status" src="https://img.shields.io/badge/status-early%20development-orange">
  <img alt="Architecture" src="https://img.shields.io/badge/arch-x86--64-lightgrey">
  <img alt="Boot" src="https://img.shields.io/badge/boot-BIOS%20%2B%20UEFI-informational">
  <img alt="Language" src="https://img.shields.io/badge/lang-C11%20%2B%20NASM-success">
  <img alt="License" src="https://img.shields.io/badge/license-GPLv3-blue">
</p>

<p align="center">
  <img src="Meta/Screenshots/shell.png" width="820" alt="ApexOS shell: directory listing, system information and memory statistics">
</p>

---

## Table of contents

1. [Overview](#overview)
2. [Screenshots](#screenshots)
3. [Project status](#project-status)
4. [Architecture](#architecture)
5. [Security hardening](#security-hardening)
6. [Building and running](#building-and-running)
7. [Debugging](#debugging)
8. [Testing](#testing)
9. [Repository layout](#repository-layout)
10. [Known limitations](#known-limitations)
11. [Roadmap](#roadmap)
12. [Contributing](#contributing)
13. [License](#license)

---

## Overview

ApexOS is an independent, monolithic-kernel operating system for x86-64. It does not use GRUB, Multiboot or any existing kernel: the bootloaders, the kernel, the filesystem, the executable format and the network stack are all implemented in this repository, with no libc and no third-party runtime.

Design goals:

- **Understandable.** Every subsystem is small, self-contained and commented with the reasoning behind non-obvious decisions.
- **Correct before fast.** Cross-CPU behaviour, memory permissions and untrusted input (userspace pointers, executable formats, on-disk structures) are treated as hostile by default.
- **Own formats.** ApexOS defines its own executable format (AXF) and filesystem (AXFS) rather than reimplementing existing ones.

The codebase is roughly 28,000 lines of C and assembly, split into four top-level projects, each with its own README for the detail this file doesn't cover:

- [`ApexKernel/`](ApexKernel/README.md) — boot, memory, interrupts, SMP, scheduling, syscalls, filesystem, executable loading, drivers, networking
- [`ApexLib/`](ApexLib/README.md) — freestanding base library (types, logging; more planned)
- [`ApexUI/`](ApexUI/README.md) — the graphical layer: compositor, window manager, taskbar, widgets
- `ApexPKG/`, `ApexToolchain/` — reserved for a future package manager and AXF toolchain, not started yet

---

## Screenshots

All captures are taken from ApexOS running under QEMU (q35, 4 CPUs, 1600x900 framebuffer).

<details open>
<summary><b>SMP boot on 4 CPUs</b></summary>

ACPI/MADT discovery, then each application processor woken through the trampoline and reporting online, followed by IPI validation, memory statistics and storage, input and network initialisation.

<p align="center">
  <img src="Meta/Screenshots/boot-smp-1.png" width="820" alt="ACPI discovery and application processors coming online">
</p>
<p align="center">
  <img src="Meta/Screenshots/boot-smp-2.png" width="820" alt="IPI validation, memory statistics, storage and network initialisation">
</p>

The two `[WARN]` lines are expected: the boot semaphore test is initialised with two units and requested by four CPUs, so the last two find it empty.

</details>

<details open>
<summary><b>Network stack: ICMP, DNS and TCP</b></summary>

`ping` against the QEMU gateway, then `tcptest`: DNS resolution of `example.com`, TCP three-way handshake, an HTTP request and the `200 OK` response received through the hand-written stack (the body is shown as received, still chunk-encoded).

<p align="center">
  <img src="Meta/Screenshots/network.png" width="820" alt="ping and tcptest output">
</p>

</details>

<details open>
<summary><b>Userspace (Ring 3)</b></summary>

`run calc.axp` loads an AXF program from AXFS, executes it in Ring 3 using system calls for input and output, and returns to the kernel through `SYS_EXIT`.

<p align="center">
  <img src="Meta/Screenshots/userspace.png" width="820" alt="calc.axp running in Ring 3">
</p>

</details>

<details open>
<summary><b>ApexUI</b></summary>

Early graphical layer on the framebuffer: overlapping windows with title bars, a button widget, a taskbar with one entry per window, and a mouse cursor.

<p align="center">
  <img src="Meta/Screenshots/apexui.png" width="820" alt="ApexUI with two windows and a taskbar">
</p>

</details>

---

## Project status

ApexOS is in early development (`v0.1.5`). It boots and runs on QEMU (BIOS and UEFI, 4 CPUs).

| Area                                 | State            | Notes                                                         |
| ------------------------------------ | ---------------- | ------------------------------------------------------------- |
| BIOS bootloader                      | Working          | Two-stage, real mode to long mode                             |
| UEFI bootloader                      | Working          | GOP framebuffer, GPT reader, memory map, `ExitBootServices`   |
| Memory management                    | Working          | Bitmap PMM, 4-level VMM, W^X, kernel heap                     |
| Interrupts and timer                 | Working          | GDT, IDT, TSS, PIC 8259, PIT at 100 Hz                        |
| Syscalls and Ring 3                  | Working          | `syscall`/`sysret`, validated user pointers                   |
| SMP                                  | Working, partial | 4 CPUs online, IPIs, TLB shootdown; round-robin test disabled |
| Scheduler                            | Working          | Preemptive; multi-CPU test pending (see limitations)          |
| Storage                              | Working          | AHCI (SATA) plus RAM backend                                  |
| Filesystem                           | Working          | AXFS with journal, VFS with `@Volume` prefixes                |
| Executable format                    | Working          | AXF: `.axp` programs, `.axl` libraries, `.axd` drivers        |
| Display                              | Working          | VGA text, VBE/GOP framebuffer console, MTRR write-combining   |
| Input                                | Working          | PS/2 keyboard and mouse (wheel)                               |
| Networking                           | Partial          | Intel e1000, Ethernet, ARP, IPv4, ICMP, UDP, TCP (client)     |
| ApexUI                               | Early            | Compositor, window manager, taskbar, widgets                  |
| Package manager, toolchain, userland | Planned          | Directories exist, still empty                                |

---

## Architecture

This section covers the system-wide picture: how the two boot paths converge,
and the project's biggest cross-cutting rule (W^X). Subsystem-level detail —
the exact memory layout, the SMP bring-up sequence, the syscall ABI, the AXF
format, AXFS internals, the network stack's protocol stack — lives in
[`ApexKernel/README.md`](ApexKernel/README.md) instead, next to the code it
describes.

### Boot flow

```
                 BIOS                                   UEFI
                   |                                      |
     stage1 (MBR, 512 B) -> stage2               BOOTX64.EFI (GOP, GPT,
       real mode -> protected -> long              memory map, ExitBootServices)
                   \                                      /
                    \____________ kernel.bin _____________/
                                     |
                              kmain()  (0x100000)
     memory -> interrupts/timer -> ACPI/MADT -> LAPIC -> per-CPU -> APs
        -> IPI and sync validation -> framebuffer -> storage -> AXFS/VFS
        -> drivers (AXD) -> input -> network -> shell
```

Both boot paths load the same `kernel.bin` and pass information (memory map, framebuffer, ACPI RSDP, disk layout) through structures at fixed low-memory addresses that the bootloaders and the kernel agree on.

### W^X

The kernel heap and user stacks are mapped non-executable (`EFER.NXE`,
`VMM_KERNEL_RW_NX`/`VMM_USER_RW_NX`). This is the one memory-safety rule
that reaches across nearly every subsystem — it's why the AP trampoline has
to enable `EFER.NXE` itself (see `ApexKernel/README.md`), why a future JIT
or relocatable loader would need an explicit executable mapping, and why
it's listed first in the hardening table below.

---

## Security hardening

A manual static audit of the kernel produced 16 findings, all fixed. Highlights:

| Area        | Issue                                                                                   | Fix                                                                                          |
| ----------- | --------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------- |
| Memory      | No W^X: heap and user stacks were executable                                            | `EFER.NXE` enabled; heap and user stacks mapped NX                                           |
| Syscalls    | Unvalidated user pointers in `sys_read`/`sys_write`                                     | Range validation against a user-region allowlist                                             |
| AXF loaders | Integer overflows in offset/size arithmetic (7 sites)                                   | `SafeRange` helpers (`axf_range_fits`, `axf_array_fits`)                                     |
| AXD drivers | Export offset never validated (Ring 0 code execution)                                   | Bound and per-entry checks                                                                   |
| Heap        | `kcalloc` overflow, `kfree` double free, no SMP lock                                    | Overflow guard, double-free detection, IRQ-safe spinlock                                     |
| AXFS        | Block leak on failed write, double free, silent path truncation, unvalidated superblock | Cleanup on every error path, bitmap checks, explicit truncation error, mount-time validation |
| VMM         | `pml4` / `active_pml4()` inconsistency in unmap paths                                   | Unified on the active PML4                                                                   |
| Boot        | Unchecked UEFI `AllocatePool`, GPT division by zero, MADT walk without bounds           | Status checks and bounds checks                                                              |

Details are in the commit history.

---

## Building and running

### Requirements

- `nasm`, `gcc`, `ld`, `objcopy`
- `qemu-system-x86_64` (KVM optional)

### Make targets

| Target               | Description                                                       |
| -------------------- | ----------------------------------------------------------------- |
| `make`               | Build `Build/apex.img`                                            |
| `make run`           | Build and boot in QEMU (q35, 4 CPUs, 256 MiB, e1000, SDL display) |
| `make debug`         | Boot paused, GDB server on port 1234                              |
| `make run-debug-int` | Boot with interrupt logging                                       |
| `make run-emergency` | Fallback boot configuration                                       |
| `make clean`         | Remove build output                                               |

After changing the Makefile or an assembly source, do a clean rebuild (`rm -rf Build && make`) so stale objects are not linked.

### UEFI

```bash
./Meta/Scripts/Bash/test-uefi.sh     # full rebuild: builds BOOTX64.EFI and a fresh test image
./Meta/Scripts/Bash/rerun-uefi.sh    # relaunch the last UEFI image without rebuilding
```

Both scripts use paths relative to the current directory, not to the script itself: run them from the project root, as shown above.

### Using the shell

Once booted, press any key to reach the shell. Available commands:

`help` `clear` `about` `mem` `uptime` `echo` `ls` `cd` `mkdir` `touch` `write` `cat` `rm` `run` `test` `ring3` `gfxtest` `apexui` `disktest` `arptest` `ping` `dnstest` `tcptest` `serialmirror` `reboot` `shutdown`

---

## Debugging

```bash
make debug                  # terminal 1: QEMU paused, GDB server on :1234
gdb Build/kernel.elf        # terminal 2
(gdb) target remote :1234
```

For triple faults, log the exception chain instead of inspecting registers after the reset (registers shown after a triple fault are the post-reset state and are misleading):

```bash
qemu-system-x86_64 ... -d int,cpu_reset -D qemu.log -no-reboot
grep -B2 -A25 "check_exception\|Triple fault" qemu.log
```

The log gives the exception vector, error code, `CR2` and `RIP` of the first fault.

Console output can be mirrored to COM1 (`serialmirror` in the shell, or `-serial stdio` under QEMU), which allows headless runs and scripted checks. Early boot lines are buffered until the framebuffer takes over, then imported into its scrollback.

---

## Testing

The kernel embeds a self-test suite, run with `test` from the shell. It covers GDT, IDT, PIT, PMM, VMM, heap, scheduler, AXFS and VFS (sources in [`Tests/`](Tests/)).

Boot itself acts as an integration test: it validates IPI delivery to every AP, atomics, mutex and semaphore behaviour across CPUs, and the execution of a real process on a secondary CPU, and reports each step in the boot log.

---

## Repository layout

```
ApexKernel/
  Arch/x86_64/        GDT, syscall entry, context switch, AP trampoline, MTRR
  Boot/               BIOS (stage1/stage2) and UEFI bootloaders
  Bus/                PCI, serial
  Core/               Init, ACPI, interrupts, memory, SMP, synchronisation,
                      scheduler, syscalls, shell, boot log
  Devices/            Low-level device helpers
  Drivers/            Display, input, AHCI, e1000
  Filesystem/         AXFS and VFS (FAT32 planned)
  Formats/AXF/        AXP, AXL, AXD loaders
  Network/Protocols/  ARP, Ethernet, IPv4, ICMP, UDP, TCP
  Includes/           Headers, mirroring the source tree layout
ApexLib/               Freestanding base library (types, logging)
ApexUI/                Compositor, window manager, taskbar, widgets
Tests/                 In-kernel test suites
Meta/
  Screenshots/          Images used by this README
  Scripts/Bash/         Shell helper scripts (mirror_includes*.sh, test-uefi.sh, rerun-uefi.sh)
  Scripts/Python/       Python helper scripts (make_calc_axp.py)
ApexPKG/  ApexToolchain/  Userland/  Documentation/    Planned, currently empty
Linker.ld  Makefile
```

Build conventions: C11, freestanding, no libc, no stack protector, no red zone, and no SSE/MMX code generation (the kernel does not save that state on interrupts or context switches). `-Wall -Wextra` are enabled. Builds currently use `-O0 -g`.

See [`ApexKernel/README.md`](ApexKernel/README.md), [`ApexLib/README.md`](ApexLib/README.md) and [`ApexUI/README.md`](ApexUI/README.md) for what's actually implemented versus reserved-but-empty within each of these directories.

---

## Known limitations

- **SMP scheduling.** The multi-CPU round-robin scheduling test is disabled because of a known race; scheduling across secondary CPUs is not enabled by default.
- **`SYS_SLEEP`.** Defined in the ABI but not implemented yet.
- **TCP.** Client-only, one connection, stop-and-wait; no congestion control, window scaling or selective ACK.
- **Networking.** Static addressing (no DHCP client); no IPv6.
- **Filesystem.** AXFS only; no FAT32, ext4 or NTFS yet.
- **Hardware.** Developed and tested on QEMU; behaviour on physical machines is untested.

---

## Roadmap

- Fix the SMP scheduler race and re-enable multi-CPU scheduling
- Implement `SYS_SLEEP`
- Extend ApexUI (applications, theming)
- Event-driven scheduler
- Package format and manager (`.axpkg`, `ApexPKG/`)
- FAT32, ext4 and NTFS support, IPv6, DHCP, audio
- Userland and an AXF toolchain (`ApexToolchain/`), documentation (`Documentation/`)

---

## Contributing

The project is currently developed by a single author. Issues and discussions are welcome.

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for setup instructions, design conventions and what to check before opening a pull request. For anything with a security implication, see [`SECURITY.md`](SECURITY.md) instead of opening a public issue.

---

## License

Released under the GNU General Public License v3.0. See [`LICENSE`](LICENSE).
