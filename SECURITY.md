# Security Policy

## Reporting a vulnerability

ApexOS is a hobby operating system project under active development, run in QEMU for
development and not (yet) intended for production or exposure to untrusted networks
or media. That said, memory-safety and privilege-boundary bugs are taken seriously,
since a kernel is exactly the kind of codebase where they matter most.

If you find a security issue — a way to corrupt kernel memory, escalate from Ring 3,
crash the kernel with malformed input (an AXF binary, an AXFS image, a network
packet), or bypass one of the checks described below — please report it privately
rather than opening a public issue:

- Open a [GitHub Security Advisory](../../security/advisories/new) on this repository, or
- Contact the maintainer directly (see the GitHub profile linked from this
  repository) if advisories are not available.

Please include:

- A description of the issue and its impact (crash, memory corruption, privilege
  escalation, information leak, ...)
- Steps to reproduce, ideally a minimal input file (AXP/AXL/AXD/AXFS image) or shell
  command sequence
- The commit hash or version you tested against

There is no bug bounty; this is an unfunded personal project. You will get a
response and credit in the changelog once a fix lands.

## Scope

In scope:

- The kernel (`ApexKernel/`), including memory management, the scheduler, syscalls,
  AXFS, the AXF loaders, and the network stack
- The bootloaders (`ApexKernel/Boot/`)
- ApexUI

Out of scope:

- Denial of service that requires physical/local access to the QEMU host itself
  (this is a kernel project, not a hypervisor)
- Issues that only reproduce with `-O0` debug builds and not in principle (e.g. a
  timing artefact of unoptimised code)

## What "secure" means here, for now

ApexOS assumes:

- Userspace (Ring 3 code, AXP/AXL binaries) is untrusted. Syscalls validate user
  pointers against a fixed allowlist of user regions before dereferencing them, and
  the kernel heap and user stacks are mapped non-executable (`EFER.NXE`, W^X).
- On-disk structures (AXFS superblock/inodes, AXF headers) are untrusted. Offsets,
  sizes and counts are validated with overflow-safe range checks before use.
- Every CPU can be running kernel code concurrently. Shared kernel state (the heap,
  page tables, scheduler run queues) is expected to be protected accordingly; a
  missing lock or an unsynchronized shared structure is a valid report.

A summary of the fixes from the most recent internal audit is in the
[README](README.md#security-hardening) and in the git history.

## Known gaps

These are tracked, not secret:

- The SMP round-robin scheduling test is currently disabled (known race) — see the
  [README](README.md#known-limitations).
- Networking has no authentication or encryption anywhere in the stack (no TLS);
  treat all network input as adversarial, which is exactly why it goes through the
  same validation discipline as everything else above.
