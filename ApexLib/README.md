# ApexLib

ApexOS's freestanding base library — the small set of things every other
part of the codebase (kernel, ApexUI, eventually userland) needs and
shouldn't have to redefine: basic types, and logging.

No libc is available anywhere in this project (see the root README's build
conventions), so even things as basic as `uint8_t` or `NULL` have to be
defined somewhere — that somewhere is here, not scattered across whichever
file happens to need them first.

## What's here today

- **`Includes/Core/Types.h`** — fixed-width integer types (`uint8_t` …
  `int64_t`), `uintptr_t`/`size_t`/`ssize_t`, `bool`/`true`/`false`, `NULL`,
  and a handful of utility macros (`ARRAY_SIZE`, `ALIGN_UP`, `ALIGN_DOWN`,
  `UNUSED`). Every other file in the project that needs a basic type
  includes this rather than assuming a compiler-provided one.
- **`Logs/Log.c`** (`Includes/Logs/Log.h`) — the `klog_*` logging functions
  (`klog_info`, `klog_ok`, `klog_warn`, `klog_error`, and `_n`/`_hex`/
  `_prefix` variants). Writes a line immediately in the format
  `[<secs>.<ms>] message                    [ OK ]`, with the status tag
  right-aligned and colored. Used by the shell and anywhere output is
  needed outside of the boot sequence proper.

    This is deliberately separate from `ApexKernel/Core/Boot/Log/` (the
    `boot_log_*` functions), which exists specifically for the boot sequence
    and defers output until the framebuffer console is ready (see that
    module's own comments, and the root README's Debugging section). The two
    aren't interchangeable — check which one a given call site actually needs
    before assuming they can be merged.

## Planned, not started yet

These directories exist to reserve the shape of the library as it grows,
but contain no code yet:

- `Collections/` — generic data structures (lists, maps, ...)
- `Crypto/` — hashing/checksums, if ApexOS ever needs them outside AXFS's
  own journal
- `Debug/` — assertion helpers, debug-build-only instrumentation
- `Encoding/` — base64, UTF-8 handling, etc.
- `JSON/` — a parser/serializer, likely for `ApexPKG`'s package metadata
  once that exists
- `Math/` — anything beyond what `libmath.axl` (an AXL library, Ring 3,
  not part of ApexLib) already covers for userspace
- `Memory/` — generic memory helpers not specific to the kernel heap
  (`memcpy`/`memset`-style primitives currently live wherever each
  subsystem defines its own)
- `Network/` — protocol-independent helpers, if any emerge as
  `ApexKernel/Network/Protocols/` grows
- `Numbers/` — parsing/formatting beyond what `Core/Boot/Log/` already
  has inline (`append_udec_padded` and friends)
- `String/` — `strlen`/`strcmp`-style primitives, currently duplicated
  per subsystem rather than centralized here

Don't assume any of the above has a particular design yet — these are
placeholders for where that code will go, not a spec for what it must
look like.
