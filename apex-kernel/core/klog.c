// apex-kernel/core/klog.c
// ApexOS -- Centralized logging system (see klog.h)

#include "klog.h"
#include "../drivers/vga.h"

// Prints the colored "[LABEL] " prefix, then resets to the default
// color (white) so the message that follows is always normally
// readable, even if the caller left another color active before.
static void klog_prefix(const char *label, vga_color_t color)
{
    vga_set_color(color, VGA_BLACK);
    vga_puts("[");
    vga_puts(label);
    vga_puts("] ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

void klog_info(const char *msg)
{
    klog_prefix("INFO", VGA_CYAN);
    vga_puts(msg);
    vga_puts("\n");
}

void klog_ok(const char *msg)
{
    klog_prefix("OK", VGA_LIGHT_GREEN);
    vga_puts(msg);
    vga_puts("\n");
}

void klog_warn(const char *msg)
{
    klog_prefix("WARN", VGA_YELLOW);
    vga_puts(msg);
    vga_puts("\n");
}

void klog_error(const char *msg)
{
    klog_prefix("ERROR", VGA_RED);
    vga_puts(msg);
    vga_puts("\n");
}

void klog_info_n(const char *msg, uint64_t n)
{
    klog_prefix("INFO", VGA_CYAN);
    vga_puts(msg);
    vga_puts(": ");
    vga_putdec(n);
    vga_puts("\n");
}

void klog_ok_n(const char *msg, uint64_t n)
{
    klog_prefix("OK", VGA_LIGHT_GREEN);
    vga_puts(msg);
    vga_puts(": ");
    vga_putdec(n);
    vga_puts("\n");
}

void klog_warn_n(const char *msg, uint64_t n)
{
    klog_prefix("WARN", VGA_YELLOW);
    vga_puts(msg);
    vga_puts(": ");
    vga_putdec(n);
    vga_puts("\n");
}

void klog_error_n(const char *msg, uint64_t n)
{
    klog_prefix("ERROR", VGA_RED);
    vga_puts(msg);
    vga_puts(": ");
    vga_putdec(n);
    vga_puts("\n");
}

void klog_info_hex(const char *msg, uint64_t n)
{
    klog_prefix("INFO", VGA_CYAN);
    vga_puts(msg);
    vga_puts(" ");
    vga_puthex(n);
    vga_puts("\n");
}

void klog_ok_hex(const char *msg, uint64_t n)
{
    klog_prefix("OK", VGA_LIGHT_GREEN);
    vga_puts(msg);
    vga_puts(" ");
    vga_puthex(n);
    vga_puts("\n");
}

void klog_warn_hex(const char *msg, uint64_t n)
{
    klog_prefix("WARN", VGA_YELLOW);
    vga_puts(msg);
    vga_puts(" ");
    vga_puthex(n);
    vga_puts("\n");
}

void klog_error_hex(const char *msg, uint64_t n)
{
    klog_prefix("ERROR", VGA_RED);
    vga_puts(msg);
    vga_puts(" ");
    vga_puthex(n);
    vga_puts("\n");
}

void klog_ok_prefix(void)
{
    klog_prefix("OK", VGA_LIGHT_GREEN);
}

void klog_error_prefix(void)
{
    klog_prefix("ERROR", VGA_RED);
}

void klog_test_pass(const char *test_name)
{
    klog_prefix("PASS", VGA_LIGHT_GREEN);
    vga_puts(test_name);
    vga_puts("\n");
}

void klog_test_fail(const char *test_name, const char *reason)
{
    klog_prefix("FAIL", VGA_RED);
    vga_puts(test_name);
    if (reason && reason[0] != '\0')
    {
        vga_puts(" - ");
        vga_puts(reason);
    }
    vga_puts("\n");
}