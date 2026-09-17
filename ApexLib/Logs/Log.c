// ApexLib/Logs/Log.c
// ApexOS -- Centralized logging system (see Log.h)
//
// Line format: "[<seconds>.<ms>] <message>" with the status tag
// right-aligned at the edge of the screen, e.g.:
//
//   [  3.140] AHCI: drive ready, port 0                        [ OK ]
//
// Chosen to read like a real kernel boot log (dmesg's leading
// timestamp + detailed message) with a systemd-style status column on
// the right, rather than the old "[OK] message" left prefix. The
// message itself carries the detail (subsystem, what happened) --
// the tag on the right is just pass/fail-at-a-glance, same role
// systemd's "[ OK ]"/"[FAILED]" column plays.

#include "Logs/Log.h"
#include "Drivers/Display/VGA.h"
#include "Core/Time/PITCounter.h"

// Fixed-width status tag, always exactly this many columns including
// the brackets -- e.g. "[ OK ]", "[WARN]", "[FAIL]", "[INFO]". Kept
// identical width across every level so the right edge lines up
// regardless of which one prints.
#define STATUS_TAG_WIDTH 7 // "[ XXXX]" -- 4-char label, e.g. " OK ", "WARN"

// Longest line this module ever builds before handing it to
// vga_puts(): timestamp (~10) + a generously long message (400) +
// padding + status tag (7) + nul. Truncates rather than overflows if
// a caller ever passes something longer -- see append_str().
#define LINE_BUF_SIZE 512

static char line_buf[LINE_BUF_SIZE];
static uint32_t line_len;
static bool shell_log_mode;
static bool line_locked;
static volatile uint8_t log_lock;

static void log_lock_acquire(void)
{
    uint8_t busy = 1;
    do
    {
        __asm__ volatile("xchgb %0, %1" : "+q"(busy), "+m"(log_lock)::"memory");
        if (busy)
            __asm__ volatile("pause");
    } while (busy);
}

static void log_lock_release(void)
{
    __asm__ volatile("" ::: "memory");
    log_lock = 0;
}

// All the small string-building helpers below write into line_buf
// starting at line_len, and stop writing (rather than overflow) once
// LINE_BUF_SIZE-1 is reached, leaving room for the final '\0'. None of
// this needs to be reentrant/thread-safe -- logging happens from
// kernel context, one line fully built and flushed before the next
// starts.

static void line_reset(void)
{
    if (!line_locked && !shell_log_mode)
    {
        log_lock_acquire();
        line_locked = true;
    }
    line_len = 0;
}

static void line_release(void)
{
    line_len = 0;
    if (line_locked)
    {
        line_locked = false;
        log_lock_release();
    }
}

static void append_char(char c)
{
    if (line_len + 1 < LINE_BUF_SIZE)
        line_buf[line_len++] = c;
}

static void append_str(const char *s)
{
    while (*s && line_len + 1 < LINE_BUF_SIZE)
        line_buf[line_len++] = *s++;
}

// Appends a right-aligned, zero-padded unsigned decimal in exactly
// `width` digits (used for the timestamp's seconds/milliseconds
// fields, e.g. "  3" or "042"). If the value doesn't fit in `width`
// digits, prints it in full anyway rather than silently truncating a
// real number.
static void append_udec_padded(uint64_t value, int width)
{
    char digits[20];
    int n = 0;
    if (value == 0)
        digits[n++] = '0';
    else
    {
        while (value && n < (int)sizeof(digits))
        {
            digits[n++] = '0' + (value % 10);
            value /= 10;
        }
    }
    for (int pad = width - n; pad > 0; pad--)
        append_char(' ');
    while (n > 0)
        append_char(digits[--n]);
}

static void append_udec(uint64_t value)
{
    append_udec_padded(value, 0);
}

static void append_hex(uint64_t value)
{
    const char *hex = "0123456789ABCDEF";
    char digits[16];
    int n = 0;
    if (value == 0)
        digits[n++] = '0';
    else
    {
        while (value && n < (int)sizeof(digits))
        {
            digits[n++] = hex[value & 0xF];
            value >>= 4;
        }
    }
    append_str("0x");
    while (n > 0)
        append_char(digits[--n]);
}

// Prepends "[<secs>.<ms>] " to the line. Before the PIT is
// initialized (pit_is_ready(), see PITCounter.h), elapsed time simply
// isn't known yet -- prints a fixed "0.000" rather than a fake or
// stale-looking value, the same convention dmesg itself follows for
// timestamps taken before the kernel's own clock exists.
static void append_timestamp(void)
{
    uint64_t secs = 0, ms = 0;
    if (pit_is_ready())
    {
        uint64_t ticks = pit_get_ticks(); // 100 ticks/sec (PITCounter.h)
        secs = ticks / 100;
        ms = (ticks % 100) * 10;
    }
    append_char('[');
    append_udec_padded(secs, 3);
    append_char('.');
    append_udec_padded(ms, 3);
    append_str("] ");
}

static bool is_log_word_char(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '_';
}

static uint32_t important_keyword_length(const char *text, const char *start)
{
    static const char *keywords[] = {
        "AHCI", "AXFS", "UEFI", "SMP", "ACPI", "PIT", "VMM", "GDT",
        "IDT", "PIC", "NIC", "IRQ", "MAC", "CR3", "x86-64", "Kernel",
        "Long Mode", "Long mode", "Default filesystem tree", "AXFS AHCI disk",
        "PS/2 mouse", "PS/2", "disk"};

    if (text != start && is_log_word_char(text[-1]))
        return 0;

    for (uint32_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++)
    {
        uint32_t length = 0;
        while (keywords[i][length] && text[length] == keywords[i][length])
            length++;
        if (keywords[i][length] == '\0' && !is_log_word_char(text[length]))
            return length;
    }

    return 0;
}

static uint32_t log_unit_length(const char *text)
{
    static const char *units[] = {"MHz", "KB", "MB", "GB", "TB", "ms", "Hz", "B", "%"};
    for (uint32_t i = 0; i < sizeof(units) / sizeof(units[0]); i++)
    {
        uint32_t length = 0;
        while (units[i][length] && text[length] == units[i][length])
            length++;
        if (units[i][length] == '\0' &&
            !((text[length] >= 'A' && text[length] <= 'Z') ||
              (text[length] >= 'a' && text[length] <= 'z')))
            return length;
    }
    return 0;
}

static void render_message_details(const char *message)
{
    const char *cursor = message;
    const char *start = message;
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    while (*cursor)
    {
        if (*cursor >= '0' && *cursor <= '9')
        {
            while (start < cursor)
                vga_putchar(*start++);

            if (cursor[0] == '0' && (cursor[1] == 'x' || cursor[1] == 'X'))
            {
                vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
                vga_putchar(*cursor++);
                vga_putchar(*cursor++);
                while ((*cursor >= '0' && *cursor <= '9') ||
                       (*cursor >= 'A' && *cursor <= 'F') ||
                       (*cursor >= 'a' && *cursor <= 'f'))
                    vga_putchar(*cursor++);
            }
            else
            {
                vga_set_color(VGA_WHITE, VGA_BLACK);
                while (*cursor >= '0' && *cursor <= '9')
                    vga_putchar(*cursor++);

                uint32_t unit_length = log_unit_length(cursor);
                while (unit_length-- > 0)
                    vga_putchar(*cursor++);
            }
            vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
            start = cursor;
        }
        else
        {
            uint32_t keyword_length = important_keyword_length(cursor, message);
            if (keyword_length != 0)
            {
                while (start < cursor)
                    vga_putchar(*start++);
                vga_set_color(VGA_WHITE, VGA_BLACK);
                while (keyword_length-- > 0)
                    vga_putchar(*cursor++);
                vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
                start = cursor;
            }
            else
                cursor++;
        }
    }

    while (*start)
        vga_putchar(*start++);
}

// Writes line_buf to the screen with the given status tag/color
// right-aligned at the current display width, then resets line_buf
// for the next call. Splitting the line in two vga_puts() calls (body
// then padding+tag) rather than one keeps this simple: the tag's
// color needs to differ from the body's, and vga_puts() has no color
// parameter of its own (see vga_set_color()).
static void flush_line(const char *tag, vga_color_t tag_color)
{
    if (shell_log_mode)
    {
        const char *label = "INFO";
        if (tag[1] == ' ')
            label = "OK";
        else if (tag[1] == 'F')
            label = "ERROR";
        else if (tag[1] == 'W')
            label = "WARN";
        else if (tag[1] == 'P')
            label = "PASS";

        vga_set_color(tag_color, VGA_BLACK);
        vga_putchar('[');
        vga_puts(label);
        vga_puts("] ");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        line_buf[line_len] = '\0';

        char *message = line_buf;
        while (message[0] && !(message[0] == ']' && message[1] == ' '))
            message++;
        if (message[0])
            message += 2;
        vga_puts(message);
        vga_putchar('\n');
        line_release();
        return;
    }

    vga_set_color(VGA_WHITE, VGA_BLACK);
    line_buf[line_len] = '\0';

    char *message = line_buf;
    while (message[0] && !(message[0] == ']' && message[1] == ' '))
        message++;
    if (message[0])
    {
        vga_set_color(VGA_WHITE, VGA_BLACK);
        vga_putchar('[');
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        for (char *timestamp = line_buf + 1; timestamp < message; timestamp++)
            vga_putchar(*timestamp);
        vga_set_color(VGA_WHITE, VGA_BLACK);
        vga_putchar(']');
        vga_putchar(' ');
        message += 2;
    }

    char *prefix_end = message;
    while (*prefix_end && *prefix_end != ':')
        prefix_end++;

    if (*prefix_end == ':')
    {
        char saved = *prefix_end;
        *prefix_end = '\0';
        vga_set_color(VGA_WHITE, VGA_BLACK);
        vga_puts(message);
        vga_putchar(':');
        vga_set_color(VGA_WHITE, VGA_BLACK);
        *prefix_end = saved;
        render_message_details(prefix_end + 1);
    }
    else
    {
        vga_set_color(VGA_WHITE, VGA_BLACK);
        render_message_details(message);
    }

    // Pad with spaces up to (screen width - STATUS_TAG_WIDTH), so the
    // tag itself lands flush against the right edge. If the message
    // was already that long or longer, print the tag right after it
    // instead of wrapping -- a line that's already run past the
    // status column isn't worth breaking to force alignment.
    uint32_t width = vga_get_width();
    uint32_t col = vga_get_col();
    while (col + STATUS_TAG_WIDTH < width)
    {
        vga_putchar(' ');
        col++;
    }

    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_putchar('[');
    vga_set_color(tag_color, VGA_BLACK);
    for (const char *status = tag + 1; status[1] != '\0'; status++)
        vga_putchar(*status);
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_putchar(']');
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_putchar('\n');

    line_release();
}

void klog_shell_begin(void)
{
    shell_log_mode = true;
}

void klog_shell_end(void)
{
    shell_log_mode = false;
}

void klog_info(const char *msg)
{
    line_reset();
    append_timestamp();
    append_str(msg);
    flush_line("[INFO]", VGA_CYAN);
}

void klog_ok(const char *msg)
{
    line_reset();
    append_timestamp();
    append_str(msg);
    flush_line("[ OK ]", VGA_LIGHT_GREEN);
}

void klog_warn(const char *msg)
{
    line_reset();
    append_timestamp();
    append_str(msg);
    flush_line("[WARN]", VGA_YELLOW);
}

void klog_error(const char *msg)
{
    line_reset();
    append_timestamp();
    append_str(msg);
    flush_line("[FAIL]", VGA_RED);
}

void klog_info_n(const char *msg, uint64_t n)
{
    line_reset();
    append_timestamp();
    append_str(msg);
    append_str(": ");
    append_udec(n);
    flush_line("[INFO]", VGA_CYAN);
}

void klog_ok_n(const char *msg, uint64_t n)
{
    line_reset();
    append_timestamp();
    append_str(msg);
    append_str(": ");
    append_udec(n);
    flush_line("[ OK ]", VGA_LIGHT_GREEN);
}

void klog_warn_n(const char *msg, uint64_t n)
{
    line_reset();
    append_timestamp();
    append_str(msg);
    append_str(": ");
    append_udec(n);
    flush_line("[WARN]", VGA_YELLOW);
}

void klog_error_n(const char *msg, uint64_t n)
{
    line_reset();
    append_timestamp();
    append_str(msg);
    append_str(": ");
    append_udec(n);
    flush_line("[FAIL]", VGA_RED);
}

void klog_info_hex(const char *msg, uint64_t n)
{
    line_reset();
    append_timestamp();
    append_str(msg);
    append_str(" ");
    append_hex(n);
    flush_line("[INFO]", VGA_CYAN);
}

void klog_ok_hex(const char *msg, uint64_t n)
{
    line_reset();
    append_timestamp();
    append_str(msg);
    append_str(" ");
    append_hex(n);
    flush_line("[ OK ]", VGA_LIGHT_GREEN);
}

void klog_warn_hex(const char *msg, uint64_t n)
{
    line_reset();
    append_timestamp();
    append_str(msg);
    append_str(" ");
    append_hex(n);
    flush_line("[WARN]", VGA_YELLOW);
}

void klog_error_hex(const char *msg, uint64_t n)
{
    line_reset();
    append_timestamp();
    append_str(msg);
    append_str(" ");
    append_hex(n);
    flush_line("[FAIL]", VGA_RED);
}

// klog_ok_prefix()/klog_error_prefix() used to print just the
// "[LEVEL] " prefix, letting the caller compose the rest of the
// message with raw vga_puts()/vga_putdec()/vga_puthex() calls and a
// final '\n'. That pattern can't produce this module's new format
// (the status tag has to be placed AFTER the full message is known,
// to right-align it) -- so these now buffer into line_buf like every
// other function here, and the matching klog_*_prefix_end() below
// flushes the line. Every existing call site composing a message this
// way needs one extra call added (klog_ok_prefix_end() /
// klog_error_prefix_end()) after its last vga_puts()/vga_putdec()/
// vga_puthex() and before/instead of its own trailing "\n" -- see
// Log.h's comment on these two functions.
void klog_ok_prefix(void)
{
    line_reset();
    if (shell_log_mode)
    {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_puts("[OK] ");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }
    append_timestamp();
}

void klog_error_prefix(void)
{
    line_reset();
    if (shell_log_mode)
    {
        vga_set_color(VGA_RED, VGA_BLACK);
        vga_puts("[ERROR] ");
        vga_set_color(VGA_WHITE, VGA_BLACK);
        return;
    }
    append_timestamp();
}

void klog_ok_prefix_end(void)
{
    flush_line("[ OK ]", VGA_LIGHT_GREEN);
}

void klog_error_prefix_end(void)
{
    flush_line("[FAIL]", VGA_RED);
}

// Appends to the message being built between klog_ok_prefix()/
// klog_error_prefix() and klog_ok_prefix_end()/klog_error_prefix_end()
// -- callers that used to reach for raw vga_puts()/vga_putdec()/
// vga_puthex() between the old prefix-only functions and their own
// trailing '\n' use these instead now, so the text lands in line_buf
// (and gets right-aligned against the status tag) instead of being
// written straight to the screen.
void klog_append_str(const char *s)
{
    append_str(s);
}

void klog_append_dec(uint64_t n)
{
    append_udec(n);
}

void klog_append_hex(uint64_t n)
{
    append_hex(n);
}

void klog_test_pass(const char *test_name)
{
    line_reset();
    append_timestamp();
    append_str(test_name);
    flush_line("[PASS]", VGA_LIGHT_GREEN);
}

void klog_test_fail(const char *test_name, const char *reason)
{
    line_reset();
    append_timestamp();
    append_str(test_name);
    if (reason && reason[0] != '\0')
    {
        append_str(" - ");
        append_str(reason);
    }
    flush_line("[FAIL]", VGA_RED);
}
