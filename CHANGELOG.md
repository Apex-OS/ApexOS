# Changelog

All notable changes to ApexOS are documented here. Dates are UTC, taken from
the commit history.

The format loosely follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versioning is informal (`vMAJOR.MINOR.PATCH`) until the project reaches a
first stable release; most of this history predates any version tagging,
so entries are grouped by development phase instead of by tag.

## [Unreleased]

### Added

- `CHANGELOG.md`, `SECURITY.md`, `CONTRIBUTING.md`, `.editorconfig`

### Changed

- Helper scripts (`mirror_includes*.sh`, `test-uefi.sh`, `rerun-uefi.sh`) moved into `Meta/Scripts/`
- README rewritten: architecture diagrams, memory layout, security hardening table, project status table, screenshots

## 2026-09-28 — Security audit and SMP fix

A manual static audit of the kernel produced 16 findings; all fixed in this
batch, immediately followed by a regression it introduced.

### Fixed

- **AP boot triple fault** under `-smp 2`/`-smp 4`, introduced by the W^X change below: the AP trampoline enabled `EFER.LME` but not `EFER.NXE`, so the NX bit now set on the kernel heap (and therefore each AP's stack) was a reserved bit from the AP's point of view — the first `push` after entering `ap_c_entry` raised a page fault with no IDT loaded, cascading into a triple fault. Fixed by enabling `LME | NXE` together in `ApBootTrampoline.asm`.
- AXFS: block leak on a failed write path, missing double-free protection on data block release, silent path-segment truncation, unvalidated superblock region order on mount.
- `axfs_path_next_segment` truncation reporting (missing header declaration for `out_truncated`, follow-up to the AXFS fix above).
- Ring 3: `userspace_jump()` was reloading `%gs`, overwriting the per-CPU `GS_BASE` MSR with the flat GDT base and breaking `percpu_get()` after the first jump to Ring 3.
- Boot: unchecked UEFI `AllocatePool` return value, GPT division by zero on `size_of_partition_entry = 0`, MADT entry walk without per-entry bounds checks.

### Added

- W^X: `EFER.NXE`, `VMM_KERNEL_RW_NX` / `VMM_USER_RW_NX` page flags; kernel heap and user stacks mapped non-executable.
- Userspace pointer validation for syscalls (`Core/Syscalls/Validation`), used by `sys_read`/`sys_write`.
- `SafeRange` overflow-safe bounds checking for AXF header/offset/size parsing (7 sites) and AXD export offset validation.
- IRQ-safe spinlock around the kernel heap; overflow guard in `kcalloc`, double-free detection in `kfree`.
- `calc.axp` — a minimal Ring 3 test program exercising syscalls end to end (also served to catch the `userspace_jump()` GS_BASE bug above).
- QEMU targets now run with `-smp 4` by default.

## 2026-09-17 — SMP

### Added

- ACPI discovery: RSDP, RSDT, MADT parsing
- Local APIC and I/O APIC bring-up
- Application processor wakeup via a real mode -> protected mode -> long mode trampoline (INIT-SIPI-SIPI), reusing the BSP's page tables
- Per-CPU data via `GS_BASE`, inter-processor interrupts, TLB shootdown
- Atomics, spinlocks, mutexes, semaphores
- Preemptive scheduler

### Known issues

- SMP round-robin scheduling test disabled (known race), left for a later fix

## 2026-09-11 — Networking and BIOS/UEFI parity

### Added

- Full network stack: Intel e1000 driver -> Ethernet -> ARP -> IPv4 -> ICMP -> UDP -> TCP (client)

### Fixed

- VMM: CR3/PML4 handling corrected so BIOS and UEFI boot paths build identical, safe page tables (scratch table built at a fixed address, then copied into place)

## 2026-09-05 — ApexUI and UEFI bootloader

### Added

- ApexUI v1: window manager, input handling, rendering, widgets, taskbar
- UEFI bootloader (GOP framebuffer, GPT reader, memory map, `ExitBootServices`), alongside the existing BIOS path

## 2026-08-27 to 2026-08-30 — Architecture rewrite

### Changed

- Major reorganization of the entire codebase into the current PascalCase directory structure, with headers mirrored under `Includes/` and one file per responsibility

### Fixed

- Post-migration build errors (missing includes, `Userspace.asm` stub, header paths)
- Intermittent #GP on return from Ring 3, caused by `fbcon_present` being interrupted mid-redraw by the timer IRQ
- Framebuffer console performance: dirty-rectangle scrolling, write-combining framebuffer mapping, fused glyph rendering

## 2026-08-08 to 2026-08-17 — Graphics and storage foundations

### Added

- VESA/VBE graphics mode
- Framebuffer drawing primitives (rectangles, lines, text) and an 8x8 bitmap font, later replaced by a VGA BIOS CP437 font
- Double buffering and a dirty-rectangle cursor
- Framebuffer console (graphical shell)
- AHCI SATA driver: PCI enumeration, DMA read/write — the foundation for real-disk storage

### Fixed

- VESA/VBE: disk sector overflow bugs, E820 memory map corruption, real-mode disk access DAP 64 KB limit
- PMM/VMM bugs surfaced by the framebuffer console work
- Serial mirror now quiet by default; cursor visibility fixed

## 2026-07-22 to 2026-07-24 — v0.1.0

Initial public history: the first BIOS bootloader and kernel core.

[Unreleased]: https://github.com/Apex-OS/ApexOS/compare/d89e8c4...HEAD
