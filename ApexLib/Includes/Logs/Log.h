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
// Each function handles the prefix color itself AND resets to the
// default color (white) after the message, exactly like the scattered
// code did before. The message must NOT include a trailing '\n' -- it
// is added automatically (avoids the missing/duplicate newlines seen
// in the existing code).

#ifndef APEX_LIB_LOGS_LOG_H
#define APEX_LIB_LOGS_LOG_H

#include "Core/Types.h"

// ── General log levels ────────────────────────────────────────────────────
void klog_info(const char *msg);  // [INFO]  cyan
void klog_ok(const char *msg);    // [OK]    light green
void klog_warn(const char *msg);  // [WARN]  yellow
void klog_error(const char *msg); // [ERROR] red

// ── Variants with a trailing number ───────────────────────────────────────
// Avoids the "vga_puts(\"text: \"); vga_putdec(n); vga_puts(\"\\n\");"
// pattern, very common across the codebase (memory stats, counters,
// etc.). Prints: "[LEVEL] msg: N\n"
void klog_info_n(const char *msg, uint64_t n);
void klog_ok_n(const char *msg, uint64_t n);
void klog_warn_n(const char *msg, uint64_t n);
void klog_error_n(const char *msg, uint64_t n);

// ── Variants with a trailing hex value (addresses, etc.) ─────────────────
void klog_info_hex(const char *msg, uint64_t n);
void klog_ok_hex(const char *msg, uint64_t n);
void klog_warn_hex(const char *msg, uint64_t n);
void klog_error_hex(const char *msg, uint64_t n);

// ── "Prefix only" variants (no message, no '\n') ──────────────────────────
// For cases where the message needs several values (e.g. "X inodes, Y
// blocks") -- no simple helper (klog_*_n / klog_*_hex) covers that.
// Prints just "[LEVEL] " with no trailing '\n' -- the caller composes
// and terminates the message with vga_puts()/vga_putdec()/vga_puthex(),
// followed by a final '\n'.
void klog_ok_prefix(void);
void klog_error_prefix(void);

// ── Tests ────────────────────────────────────────────────────────────────
void klog_test_pass(const char *test_name);
void klog_test_fail(const char *test_name, const char *reason);

#endif // APEX_LIB_LOGS_LOG_H
