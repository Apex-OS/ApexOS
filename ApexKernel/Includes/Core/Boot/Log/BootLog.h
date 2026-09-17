// ApexKernel/Includes/Core/Boot/Log/BootLog.h
// ApexOS -- Boot-time log format: timestamp + detailed message, with
// a systemd-style status tag right-aligned at the edge of the screen.
//
//   [  3.140] AHCI: drive ready, port 0                        [ OK ]
//
// Separate from ApexLib/Includes/Logs/Log.h's klog_* functions on
// purpose: this format is for the boot sequence only (Core/Init.c,
// and the *_print_stats()/_init() functions it calls) -- the shell's
// own commands (ls, ping, cd, disktest, ...) keep using klog_* and
// its plain "[OK] message" left-prefix format unchanged. Boot
// messages benefit from a timestamp and a lot of low-level detail the
// way a real kernel's dmesg does; a shell command's output is read
// interactively right after typing it, where a timestamp adds nothing
// and detail just gets in the way of the answer the person asked for.
//
// Usage is the same shape as klog_* for the simple cases:
//
//   boot_log_ok("AHCI: drive ready, port 0");
//
// For messages built from several values, buffer with boot_log_str()/
// boot_log_dec()/boot_log_hex() between a start/end pair:
//
//   boot_log_start();
//   boot_log_str("AHCI: drive ready, port ");
//   boot_log_dec(port);
//   boot_log_end_ok();
//
// Nothing is written to the screen until boot_log_end_ok()/
// boot_log_end_warn()/boot_log_end_error() -- the status tag has to
// be placed after the full message is known, to right-align it.
#ifndef APEX_CORE_BOOT_LOG_BOOT_LOG_H
#define APEX_CORE_BOOT_LOG_BOOT_LOG_H

#include "Core/Types.h"

// ── Simple one-line messages ──────────────────────────────────────────────
void boot_log_info(const char *msg);  // [INFO] cyan
void boot_log_ok(const char *msg);    // [ OK ] light green
void boot_log_warn(const char *msg);  // [WARN] yellow
void boot_log_error(const char *msg); // [FAIL] red

// ── Variants with a trailing number/hex value ─────────────────────────────
void boot_log_ok_n(const char *msg, uint64_t n);
void boot_log_error_n(const char *msg, uint64_t n);
void boot_log_ok_hex(const char *msg, uint64_t n);

// ── Multi-value messages ──────────────────────────────────────────────────
// See this file's header comment for the start/append/end pattern.
void boot_log_start(void);
void boot_log_str(const char *s);
void boot_log_dec(uint64_t n);
void boot_log_hex(uint64_t n);
void boot_log_end_info(void);
void boot_log_end_ok(void);
void boot_log_end_warn(void);
void boot_log_end_error(void);
void boot_log_flush_pending(void);

// ── Section headers ───────────────────────────────────────────────────────
// A standalone line, no timestamp/status tag -- printed in cyan to
// visually break the boot log into groups (memory, ACPI, storage,
// ...), the way SysV init's rc.M prints a bare "Initialising advanced
// hardware ..." line ahead of the [ OK ]-tagged lines that follow it.
void boot_log_section(const char *title);

#endif // APEX_CORE_BOOT_LOG_BOOT_LOG_H
