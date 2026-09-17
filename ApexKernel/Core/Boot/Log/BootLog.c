// ApexKernel/Core/Boot/Log/BootLog.c
// ApexOS -- Boot-time log format (see BootLog.h)

#include "Core/Boot/Log/BootLog.h"
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
#define PENDING_LOG_LINES 128

static char line_buf[LINE_BUF_SIZE];
static uint32_t line_len;
static volatile uint8_t log_lock;
static char pending_lines[PENDING_LOG_LINES][LINE_BUF_SIZE];
static char pending_tags[PENDING_LOG_LINES][8];
static vga_color_t pending_colors[PENDING_LOG_LINES];
static bool pending_is_section[PENDING_LOG_LINES];
static uint32_t pending_count;

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

// None of the string-building helpers below need to be reentrant --
// boot logging happens from kmain()'s single thread of execution, one
// line fully built and flushed before the next starts.

static void line_reset(void)
{
    log_lock_acquire();
    line_len = 0;
}

static void line_release(void)
{
    line_len = 0;
    log_lock_release();
}

static void append_char(char c)
{
    if (line_len + 1 < LINE_BUF_SIZE)
        line_buf[line_len++] = c;
}

static void append_str_impl(const char *s)
{
    while (*s && line_len + 1 < LINE_BUF_SIZE)
        line_buf[line_len++] = *s++;
}

// Appends a zero-padded unsigned decimal in exactly `width` digits
// (used for the timestamp's seconds/milliseconds fields, e.g. "  3"
// or "042"). Prints the value in full rather than truncating if it
// doesn't fit in `width` digits.
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

static void append_udec_impl(uint64_t value)
{
    append_udec_padded(value, 0);
}

static void append_hex_impl(uint64_t value)
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
    append_str_impl("0x");
    while (n > 0)
        append_char(digits[--n]);
}

// Prepends "[<secs>.<ms>] " to the line. Before the PIT is
// initialized (pit_is_ready(), see Core/Time/PITCounter.h), elapsed
// time simply isn't known yet -- prints a fixed "0.000" rather than a
// fake or stale-looking value, the same convention dmesg itself
// follows for timestamps taken before the kernel's own clock exists.
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
    append_str_impl("] ");
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
// for the next call.
static void render_line(char *line, const char *tag, vga_color_t tag_color)
{
    uint32_t length = 0;
    while (line[length] && length < LINE_BUF_SIZE - 1)
        length++;
    line[length] = '\0';

    vga_set_color(VGA_WHITE, VGA_BLACK);

    char *message = line;
    while (message[0] && !(message[0] == ']' && message[1] == ' '))
        message++;
    if (message[0])
    {
        vga_set_color(VGA_WHITE, VGA_BLACK);
        vga_putchar('[');
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        for (char *timestamp = line + 1; timestamp < message; timestamp++)
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
    // tag lands flush against the right edge. If the message already
    // ran past that column, print the tag right after it instead of
    // wrapping -- not worth breaking the line just to force alignment.
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
}

static void flush_line(const char *tag, vga_color_t tag_color)
{
    line_buf[line_len] = '\0';
    if (!vga_is_graphics_mode() && pending_count < PENDING_LOG_LINES)
    {
        uint32_t i = 0;
        while (line_buf[i] && i < LINE_BUF_SIZE - 1)
        {
            pending_lines[pending_count][i] = line_buf[i];
            i++;
        }
        pending_lines[pending_count][i] = '\0';

        i = 0;
        while (tag[i] && i < sizeof(pending_tags[0]) - 1)
        {
            pending_tags[pending_count][i] = tag[i];
            i++;
        }
        pending_tags[pending_count][i] = '\0';
        pending_colors[pending_count] = tag_color;
        pending_is_section[pending_count] = false;
        pending_count++;
        line_release();
        return;
    }

    render_line(line_buf, tag, tag_color);
    line_release();
}

void boot_log_flush_pending(void)
{
    log_lock_acquire();
    for (uint32_t i = 0; i < pending_count; i++)
    {
        if (pending_is_section[i])
        {
            vga_putchar('\n');
            vga_set_color(VGA_CYAN, VGA_BLACK);
            vga_puts(pending_lines[i]);
            vga_putchar('\n');
            vga_set_color(VGA_WHITE, VGA_BLACK);
        }
        else
            render_line(pending_lines[i], pending_tags[i], pending_colors[i]);
    }
    pending_count = 0;
    log_lock_release();
}

void boot_log_start(void)
{
    line_reset();
    append_timestamp();
}

void boot_log_str(const char *s) { append_str_impl(s); }
void boot_log_dec(uint64_t n) { append_udec_impl(n); }
void boot_log_hex(uint64_t n) { append_hex_impl(n); }

void boot_log_end_info(void) { flush_line("[INFO]", VGA_CYAN); }
void boot_log_end_ok(void) { flush_line("[ OK ]", VGA_LIGHT_GREEN); }
void boot_log_end_warn(void) { flush_line("[WARN]", VGA_YELLOW); }
void boot_log_end_error(void) { flush_line("[FAIL]", VGA_RED); }

void boot_log_info(const char *msg)
{
    boot_log_start();
    boot_log_str(msg);
    boot_log_end_info();
}

void boot_log_ok(const char *msg)
{
    boot_log_start();
    boot_log_str(msg);
    boot_log_end_ok();
}

void boot_log_warn(const char *msg)
{
    boot_log_start();
    boot_log_str(msg);
    boot_log_end_warn();
}

void boot_log_error(const char *msg)
{
    boot_log_start();
    boot_log_str(msg);
    boot_log_end_error();
}

void boot_log_ok_n(const char *msg, uint64_t n)
{
    boot_log_start();
    boot_log_str(msg);
    boot_log_str(": ");
    boot_log_dec(n);
    boot_log_end_ok();
}

void boot_log_error_n(const char *msg, uint64_t n)
{
    boot_log_start();
    boot_log_str(msg);
    boot_log_str(": ");
    boot_log_dec(n);
    boot_log_end_error();
}

void boot_log_ok_hex(const char *msg, uint64_t n)
{
    boot_log_start();
    boot_log_str(msg);
    boot_log_str(" ");
    boot_log_hex(n);
    boot_log_end_ok();
}

void boot_log_section(const char *title)
{
    if (!vga_is_graphics_mode() && pending_count < PENDING_LOG_LINES)
    {
        log_lock_acquire();
        uint32_t i = 0;
        while (title[i] && i < LINE_BUF_SIZE - 1)
        {
            pending_lines[pending_count][i] = title[i];
            i++;
        }
        pending_lines[pending_count][i] = '\0';
        pending_tags[pending_count][0] = '\0';
        pending_colors[pending_count] = VGA_CYAN;
        pending_is_section[pending_count] = true;
        pending_count++;
        log_lock_release();
        return;
    }

    vga_putchar('\n');
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts(title);
    vga_putchar('\n');
    vga_set_color(VGA_WHITE, VGA_BLACK);
}
