// ApexLib/Includes/Logs/Log.h
// ApexOS -- Centralized logging system
//
// Replaces the pattern repeated all over the codebase:
//
//   vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
//   vga_puts("[OK] ");
//   vga_set_color(VGA_WHITE, VGA_BLACK);
//   vga_puts("Something happened\n");
//
// with a single call:
//
//   klog_ok("Something happened");
//
// Each call prints a full line in the format:
//
//   [<secs>.<ms>] Something happened                          [ OK ]
//
// -- a leading elapsed-time timestamp (0.000 until the PIT is
// initialized -- see Core/Time/PITCounter.h's pit_is_ready(), same
// convention dmesg itself follows), the message, and the status tag
// ([ OK ]/[WARN]/[FAIL]/[INFO]/[PASS]) right-aligned at the edge of
// the screen -- dmesg's timestamp-plus-detail on the left, a
// systemd-style status column on the right. The message must NOT
// include a trailing '\n' -- it is added automatically.

#ifndef APEX_LIB_LOGS_LOG_H
#define APEX_LIB_LOGS_LOG_H

#include "Core/Types.h"

// Switches the interactive shell to the historical compact log format.
// Boot logging remains on the timestamped/status-column format.
void klog_shell_begin(void);
void klog_shell_end(void);

// ── General log levels ────────────────────────────────────────────────────
void klog_info(const char *msg);  // [INFO] cyan
void klog_ok(const char *msg);    // [ OK ] light green
void klog_warn(const char *msg);  // [WARN] yellow
void klog_error(const char *msg); // [FAIL] red

// ── Variants with a trailing number ───────────────────────────────────────
// Avoids the "vga_puts(\"text: \"); vga_putdec(n); vga_puts(\"\\n\");"
// pattern, very common across the codebase (memory stats, counters,
// etc.). Prints: "[<ts>] msg: N ... [LEVEL]"
void klog_info_n(const char *msg, uint64_t n);
void klog_ok_n(const char *msg, uint64_t n);
void klog_warn_n(const char *msg, uint64_t n);
void klog_error_n(const char *msg, uint64_t n);

// ── Variants with a trailing hex value (addresses, etc.) ─────────────────
void klog_info_hex(const char *msg, uint64_t n);
void klog_ok_hex(const char *msg, uint64_t n);
void klog_warn_hex(const char *msg, uint64_t n);
void klog_error_hex(const char *msg, uint64_t n);

// ── "Prefix" variants -- multi-value messages ─────────────────────────────
// For cases where the message needs several values (e.g. "X inodes, Y
// blocks") and none of the simple helpers above (klog_*_n / klog_*_hex)
// cover it. Usage:
//
//   klog_ok_prefix();
//   klog_append_str("X inodes, ");
//   klog_append_dec(count);
//   klog_append_str(" blocks");
//   klog_ok_prefix_end();
//
// klog_*_prefix() starts a new buffered line (with its timestamp
// already in place); klog_append_*() appends to it (no output yet --
// the status tag has to be placed after the full message is known, to
// right-align it); klog_*_prefix_end() writes the finished line with
// its status tag. Unlike the old version of this API, nothing is
// written to the screen until the matching *_prefix_end() call --
// every klog_*_prefix() must be paired with one.
void klog_ok_prefix(void);
void klog_error_prefix(void);
void klog_ok_prefix_end(void);
void klog_error_prefix_end(void);
void klog_append_str(const char *s);
void klog_append_dec(uint64_t n);
void klog_append_hex(uint64_t n);

// ── Tests ────────────────────────────────────────────────────────────────
void klog_test_pass(const char *test_name);                     // [PASS] light green
void klog_test_fail(const char *test_name, const char *reason); // [FAIL] red

#endif // APEX_LIB_LOGS_LOG_H
