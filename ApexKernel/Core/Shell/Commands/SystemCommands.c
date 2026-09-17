// ApexKernel/Core/Shell/Commands/SystemCommands.c
// ApexOS -- Shell system/info commands

#include "Core/Shell/Commands/SystemCommands.h"
#include "Core/Shell/Prompt.h"
#include "Core/Time/Uptime.h"
#include "Core/Memory/Physical/PageBitmap.h"
#include "Core/Shell/PathResolver.h"
#include "Core/Version.h"
#include "Drivers/Display/VGA.h"
#include "Bus/PortIO.h"
#include "Logs/Log.h"

static void help_entry(const char *name, const char *description)
{
    uint32_t name_length = 0;

    while (name[name_length])
        name_length++;

    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("  ");
    vga_puts(name);
    while (name_length++ < 13)
        vga_putchar(' ');
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts(description);
    vga_putchar('\n');
}

static void help_section(const char *title)
{
    vga_putchar('\n');
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts(title);
    vga_putchar('\n');
}

static void about_row(const char *logo, const char *label, const char *value)
{
    uint32_t logo_length = 0;
    while (logo[logo_length])
        logo_length++;

    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts(logo);
    while (logo_length++ < 27)
        vga_putchar(' ');
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts(label);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts(value);
    vga_putchar('\n');
}

static void about_row_number(const char *logo, const char *label, uint64_t value, const char *unit)
{
    uint32_t logo_length = 0;
    while (logo[logo_length])
        logo_length++;

    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts(logo);
    while (logo_length++ < 27)
        vga_putchar(' ');
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts(label);
    vga_putdec(value);
    vga_set_color(VGA_WHITE, VGA_BLACK);
    if (unit[0] == 'M' && unit[1] == 'B')
    {
        vga_puts("MB");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_puts(unit + 2);
    }
    else
    {
        vga_puts(unit);
    }
    vga_putchar('\n');
}

static void about_row_uptime(const char *logo, uint64_t minutes, uint64_t seconds)
{
    uint32_t logo_length = 0;
    while (logo[logo_length])
        logo_length++;

    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts(logo);
    while (logo_length++ < 27)
        vga_putchar(' ');
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("Uptime: ");
    vga_putdec(minutes);
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("m ");
    vga_putdec(seconds);
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("s\n");
}

static void help_detail(const char *command, const char *usage, const char *description)
{
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts(command);
    vga_putchar('\n');
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("  Usage: ");
    vga_puts(usage);
    vga_putchar('\n');
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("  ");
    vga_puts(description);
    vga_putchar('\n');
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

static bool help_for_command(const char *command)
{
    if (str_eq(command, "help"))
        help_detail("help", "help [command]", "Show all commands or details for one command.");
    else if (str_eq(command, "clear"))
        help_detail("clear", "clear", "Clear the console screen.");
    else if (str_eq(command, "about"))
        help_detail("about", "about", "Show ApexOS version and architecture information.");
    else if (str_eq(command, "mem"))
        help_detail("mem", "mem", "Show physical memory usage.");
    else if (str_eq(command, "uptime"))
        help_detail("uptime", "uptime", "Show elapsed system uptime.");
    else if (str_eq(command, "echo"))
        help_detail("echo", "echo <text>", "Print text to the console.");
    else if (str_eq(command, "ls"))
        help_detail("ls", "ls [path]", "List files and directories.");
    else if (str_eq(command, "cd"))
        help_detail("cd", "cd <directory>", "Change the current directory.");
    else if (str_eq(command, "mkdir"))
        help_detail("mkdir", "mkdir <name>", "Create a directory.");
    else if (str_eq(command, "touch"))
        help_detail("touch", "touch <name>", "Create an empty file.");
    else if (str_eq(command, "write"))
        help_detail("write", "write <filename> <content>", "Write text to a file.");
    else if (str_eq(command, "cat"))
        help_detail("cat", "cat <filename>", "Print a file's contents.");
    else if (str_eq(command, "rm"))
        help_detail("rm", "rm <path>", "Remove a file or empty directory.");
    else if (str_eq(command, "run"))
        help_detail("run", "run <file.axp>", "Run an AXF executable.");
    else if (str_eq(command, "test"))
        help_detail("test", "test", "Run the kernel self-tests.");
    else if (str_eq(command, "ring3"))
        help_detail("ring3", "ring3", "Test Ring 3 userspace execution.");
    else if (str_eq(command, "gfxtest"))
        help_detail("gfxtest", "gfxtest", "Test VESA/VBE graphics mode.");
    else if (str_eq(command, "apexui"))
        help_detail("apexui", "apexui", "Launch the ApexUI smoke test.");
    else if (str_eq(command, "disktest"))
        help_detail("disktest", "disktest", "Test AHCI disk read and write access.");
    else if (str_eq(command, "arptest"))
        help_detail("arptest", "arptest [A.B.C.D]", "Resolve an IPv4 address through ARP.");
    else if (str_eq(command, "ping"))
        help_detail("ping", "ping [A.B.C.D] [count]", "Send ICMP echo requests.");
    else if (str_eq(command, "dnstest"))
        help_detail("dnstest", "dnstest [hostname]", "Resolve a hostname through DNS.");
    else if (str_eq(command, "tcptest"))
        help_detail("tcptest", "tcptest [hostname]", "Test TCP and HTTP connectivity.");
    else if (str_eq(command, "serialmirror"))
        help_detail("serialmirror", "serialmirror on|off", "Mirror console output to COM1.");
    else if (str_eq(command, "reboot"))
        help_detail("reboot", "reboot", "Restart the machine.");
    else if (str_eq(command, "shutdown"))
        help_detail("shutdown", "shutdown", "Power off the machine or halt.");
    else
        return false;

    return true;
}

void cmd_help(const char *args)
{
    if (args[0] != '\0')
    {
        if (!help_for_command(args))
            klog_error("help: unknown command");
        return;
    }

    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("Available commands\n");

    help_section("System");
    help_entry("help", "Show this help");
    help_entry("clear", "Clear the screen");
    help_entry("about", "Show ApexOS information");
    help_entry("mem", "Show memory statistics");
    help_entry("uptime", "Show system uptime");
    help_entry("echo", "Print text");

    help_section("Filesystem");
    help_entry("ls", "List a directory");
    help_entry("cd", "Change directory");
    help_entry("mkdir", "Create a directory");
    help_entry("touch", "Create an empty file");
    help_entry("write", "Write text to a file");
    help_entry("cat", "Show file contents");
    help_entry("rm", "Remove a file or empty directory");
    help_entry("run", "Run an .axp executable");

    help_section("Diagnostics and tests");
    help_entry("test", "Run kernel self-tests");
    help_entry("ring3", "Test Ring 3 userspace");
    help_entry("gfxtest", "Test VESA/VBE graphics mode");
    help_entry("disktest", "Test the AHCI disk");

    help_section("Network");
    help_entry("arptest", "Resolve an IPv4 address via ARP");
    help_entry("ping", "Send ICMP echo requests");
    help_entry("dnstest", "Resolve a hostname via DNS");
    help_entry("tcptest", "Test TCP and HTTP connectivity");

    help_section("Power and I/O");
    help_entry("serialmirror", "Mirror output to COM1 (on|off)");
    help_entry("reboot", "Reboot the machine");
    help_entry("shutdown", "Power off or halt the machine");

    vga_set_color(VGA_WHITE, VGA_BLACK);
}

void cmd_clear(void)
{
    vga_clear();
    // Resyncs the hardware cursor immediately (not just at the end via
    // shell_print_prompt()) for instant visual feedback.
    cursor_update(vga_get_row(), vga_get_col());
}

void cmd_about(void)
{
    pmm_stats_t stats;
    uint64_t minutes;
    uint64_t seconds;
    pmm_stats(&stats);
    time_get_uptime(&minutes, &seconds);

    vga_putchar('\n');
    about_row("              +-              ", "ApexOS ", "v" APEXOS_VERSION);
    about_row("             ++--             ", "Kernel: ", "Monolithic");
    about_row("            +++---            ", "Arch: ", "x86-64");
    about_row_number("           ++++ ---           ", "Memory: ",
                     stats.total_pages * 4096 / (1024 * 1024), "MB total");
    about_row_uptime("          +++    ----         ", minutes, seconds);
    about_row("        *+++  ++  ----        ", "Display: ", vga_is_graphics_mode() ? "Framebuffer" : "VGA text");
    about_row("       ++++ ++++++ ----       ", "Filesystem: ", "AXFS / @SysDisk");
    about_row("      +++            ---      ", "Features: ", "VMM, Heap, SMP, Network");
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("\n  Built from scratch. No limits.\n\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

void cmd_mem(void)
{
    pmm_stats_t s;
    pmm_stats(&s);
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("Memory stats:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("  Total : ");
    vga_putdec(s.total_pages * 4096 / (1024 * 1024));
    vga_puts(" MB\n  Used  : ");
    vga_putdec(s.used_pages * 4096 / (1024 * 1024));
    vga_puts(" MB\n  Free  : ");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_putdec(s.free_pages * 4096 / (1024 * 1024));
    vga_puts(" MB\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
}

void cmd_uptime(void)
{
    uint64_t mins, secs;
    time_get_uptime(&mins, &secs);
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("Uptime: ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_putdec(mins);
    vga_puts("m ");
    vga_putdec(secs);
    vga_puts("s\n");
}

void cmd_echo(const char *args)
{
    vga_puts(args);
    vga_puts("\n");
}

void cmd_reboot(void)
{
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("Rebooting...\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    __asm__ volatile("cli");

    uint8_t status;
    do
    {
        status = inb(0x64);
    } while (status & 0x02);

    outb(0x64, 0xFE); // pulse reset line

    struct __attribute__((packed))
    {
        uint16_t limit;
        uint64_t base;
    } zero_idt = {0, 0};
    __asm__ volatile("lidt %0" ::"m"(zero_idt));
    __asm__ volatile("int3");

    __asm__ volatile("hlt");
}

void cmd_shutdown(void)
{
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("Shutting down...\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    __asm__ volatile("cli");

    outb(0x604, 0x00);
    outb(0x605, 0x20);

    vga_set_color(VGA_RED, VGA_BLACK);
    vga_puts("Shutdown not supported on this platform -- halting instead.\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    __asm__ volatile("hlt");
}
