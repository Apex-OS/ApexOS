// ApexKernel/Core/Shell/Commands/SystemCommands.c
// ApexOS -- Shell system/info commands

#include "Core/Shell/Commands/SystemCommands.h"
#include "Core/Shell/Prompt.h"
#include "Core/Time/Uptime.h"
#include "Core/Memory/Physical/PageBitmap.h"
#include "Drivers/Display/VGA.h"
#include "Bus/PortIO.h"

void cmd_help(void)
{
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("Available commands:\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("  help    - Show this help\n");
    vga_puts("  clear   - Clear screen\n");
    vga_puts("  about   - About ApexOS\n");
    vga_puts("  mem     - Memory stats\n");
    vga_puts("  uptime  - System uptime\n");
    vga_puts("  echo    - Echo text\n");
    vga_puts("  ls      - List files (current or given path)\n");
    vga_puts("  cd      - Change current directory\n");
    vga_puts("  mkdir   - Create a directory\n");
    vga_puts("  touch   - Create an empty file\n");
    vga_puts("  write   - Write text to a file (write <name> <text>)\n");
    vga_puts("  cat     - Show file contents\n");
    vga_puts("  rm      - Remove a file or empty directory\n");
    vga_puts("  run     - Run an .axp executable\n");
    vga_puts("  test    - Run kernel self-tests\n");
    vga_puts("  ring3   - Test Ring 3 userspace\n");
    vga_puts("  gfxtest - Test VESA/VBE graphics mode (one-way, see README)\n");
    vga_puts("  disktest - Read sector 0 via AHCI and verify its boot signature\n");
    vga_puts("  arptest - Resolve an IPv4 address to a MAC via ARP (arptest [A.B.C.D])\n");
    vga_puts("  ping    - ICMP echo request/reply (ping [A.B.C.D] [count])\n");
    vga_puts("  dnstest - Resolve a hostname via DNS (dnstest [hostname])\n");
    vga_puts("  tcptest - TCP handshake + HTTP GET over a real connection (tcptest [hostname])\n");
    vga_puts("  serialmirror - Mirror screen output to COM1 (serialmirror on|off)\n");
    vga_puts("  reboot  - Reboot the machine\n");
    vga_puts("  shutdown - Power off (QEMU only) / halt\n");
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
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("ApexOS v0.0.1\n");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts("  Architecture : x86-64\n");
    vga_puts("  Kernel       : Monolithic\n");
    vga_puts("  Built        : From scratch, no GRUB\n");
    vga_puts("  Features     : Preemptive multitasking, VMM, Heap\n");
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("  \"Built from scratch. No limits.\"\n");
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
