# Contributing to ApexOS

ApexOS is currently developed by a single author as a from-scratch operating
system project. Issues, questions and discussions are welcome; this document
covers how to contribute code.

## Before you start

Open an issue first for anything beyond a small, obvious fix (typos, comment
corrections, a one-line bug fix with a clear cause). For anything larger —
a new driver, a filesystem feature, a change to the memory layout, a new
syscall — discuss the approach in an issue before writing code. This project
makes deliberate, sometimes non-obvious tradeoffs (see "Design conventions"
below), and it's easier to align on those before a PR than after.

## Development setup

Requirements: `nasm`, `gcc`, `ld`, `objcopy`, `qemu-system-x86_64` (KVM
optional). See the [README](README.md#building-and-running) for build and
run instructions, and [SECURITY.md](SECURITY.md) if what you're working on
touches a trust boundary (userspace input, on-disk structures, network
input).

Before opening a PR:

- `rm -rf Build && make` builds cleanly with no new warnings
  (`-Wall -Wextra` are enabled)
- `make run` boots to the shell with `-smp 4` (the default in this repo's
  QEMU targets)
- If you touched the self-test suite or a subsystem it covers, `test` in the
  shell passes

## Design conventions

- **C11, freestanding.** No libc, no stack protector, no red zone, no
  SSE/MMX code generation in kernel code (the kernel does not currently
  save that state on interrupts or context switches — using it would
  corrupt other code's state).
- **Untrusted by default.** Userspace pointers, on-disk structures (AXFS,
  AXF headers) and network input are treated as adversarial. Validate
  offsets, sizes and counts with overflow-safe checks before use — see
  `ApexKernel/Includes/Formats/AXF/SafeRange/SafeRange.h` for the pattern
  used elsewhere in the codebase.
- **SMP-safe.** The kernel boots with multiple CPUs online by default.
  Any shared kernel state you add (a new global, a new data structure)
  needs to state its locking strategy, even if that strategy is "only
  ever touched by the BSP before APs start."
- **Comment the _why_, not the _what_.** Existing files explain the
  reasoning behind non-obvious decisions, including dead ends that were
  tried and abandoned, right in the source. Match that style rather than
  restating what a line of code already says.
- **Own formats over reimplementing existing ones.** AXF and AXFS exist
  because the project is explicitly not aiming to be ELF- or ext4-
  compatible. Keep that spirit when adding new formats or protocols.

## Commit messages

One logical change per commit. The message should explain the reason for
the change, not just describe it — "what broke and why" for a fix, "what
this enables" for a feature. Look at the existing history for the style
used in this repository.

## Reporting bugs vs. reporting vulnerabilities

Regular bugs (crashes with no security implication, incorrect output,
build failures): open a GitHub issue.

Anything with a security implication (memory corruption, privilege
escalation, a way to crash the kernel from unprivileged input): see
[SECURITY.md](SECURITY.md) instead of opening a public issue.
