// apex-kernel/core/idt.c
// ApexOS -- Interrupt Descriptor Table

#include "idt.h"
#include "gdt.h"
#include "../drivers/vga.h"
#include "../drivers/pic.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../drivers/serial.h"

extern void idt_flush(uint64_t idt_descriptor);

// Emergency-only raw VGA text-mode writer, bypassing vga_puts/fbcon.c
// entirely (see already_reporting_exception below for why this
// exists). Writes straight to 0xB8000 -- ignores graphics_mode, the
// scrollback buffer, cursor position tracking, everything. Only
// capable of a single fixed-position line; that's all this needs.
static void emergency_vga_line(const char *msg)
{
    volatile uint16_t *vga = (volatile uint16_t *)0xB8000;
    uint8_t attr = 0x4F; // white on red, matches the normal exception report's colors
    int i = 0;
    for (; msg[i] && i < 80; i++)
        vga[i] = (uint16_t)(uint8_t)msg[i] | ((uint16_t)attr << 8);
    for (; i < 80; i++)
        vga[i] = (uint16_t)' ' | ((uint16_t)attr << 8);
}

// Writes a fixed-width hex number directly to a VGA text-mode row
// (row 0-24), starting at the given column. Self-contained (no
// vga_puthex dependency, which itself could be the thing crashing --
// see already_reporting_exception) -- this is the last line of
// defense for seeing WHAT crashed when the normal reporting path
// (vga_puts/fbcon.c) is itself broken.
static void emergency_vga_hex(uint8_t row, uint8_t col, uint64_t value)
{
    volatile uint16_t *vga = (volatile uint16_t *)0xB8000;
    const char *hex = "0123456789ABCDEF";
    uint8_t attr = 0x4F;
    for (int i = 15; i >= 0; i--)
    {
        char digit = hex[value & 0xF];
        vga[row * 80 + col + i] = (uint16_t)(uint8_t)digit | ((uint16_t)attr << 8);
        value >>= 4;
    }
}

// ISR stubs (idt.asm).
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

// IRQ stubs (idt.asm).
extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

static const char *exception_names[] = {
    "Division by Zero",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 FPU Error",
    "Alignment Check",
    "Machine Check",
    "SIMD FPU Exception",
    "Virtualization Exception",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved",
    "Security Exception",
    "Reserved"};

static idt_entry_t idt[256];
static idt_descriptor_t idt_desc;

void idt_set_entry(uint8_t num, uint64_t handler, uint8_t flags)
{
    idt[num].offset_low = handler & 0xFFFF;
    idt[num].offset_mid = (handler >> 16) & 0xFFFF;
    idt[num].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[num].selector = GDT_KERNEL_CODE;
    idt[num].ist = 0;
    idt[num].flags = flags;
    idt[num].reserved = 0;
}

void idt_init(void)
{
    // CPU exceptions (ISR 0-31).
    idt_set_entry(0, (uint64_t)isr0, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(1, (uint64_t)isr1, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(2, (uint64_t)isr2, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(3, (uint64_t)isr3, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(4, (uint64_t)isr4, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(5, (uint64_t)isr5, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(6, (uint64_t)isr6, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(7, (uint64_t)isr7, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(8, (uint64_t)isr8, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(9, (uint64_t)isr9, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(10, (uint64_t)isr10, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(11, (uint64_t)isr11, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(12, (uint64_t)isr12, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(13, (uint64_t)isr13, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(14, (uint64_t)isr14, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(15, (uint64_t)isr15, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(16, (uint64_t)isr16, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(17, (uint64_t)isr17, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(18, (uint64_t)isr18, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(19, (uint64_t)isr19, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(20, (uint64_t)isr20, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(21, (uint64_t)isr21, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(22, (uint64_t)isr22, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(23, (uint64_t)isr23, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(24, (uint64_t)isr24, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(25, (uint64_t)isr25, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(26, (uint64_t)isr26, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(27, (uint64_t)isr27, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(28, (uint64_t)isr28, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(29, (uint64_t)isr29, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(30, (uint64_t)isr30, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(31, (uint64_t)isr31, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);

    // IRQ 0-15, remapped by the PIC to 0x20-0x2F.
    idt_set_entry(32, (uint64_t)irq0, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(33, (uint64_t)irq1, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(34, (uint64_t)irq2, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(35, (uint64_t)irq3, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(36, (uint64_t)irq4, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(37, (uint64_t)irq5, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(38, (uint64_t)irq6, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(39, (uint64_t)irq7, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(40, (uint64_t)irq8, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(41, (uint64_t)irq9, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(42, (uint64_t)irq10, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(43, (uint64_t)irq11, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(44, (uint64_t)irq12, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(45, (uint64_t)irq13, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(46, (uint64_t)irq14, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);
    idt_set_entry(47, (uint64_t)irq15, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);

    idt_desc.limit = sizeof(idt) - 1;
    idt_desc.base = (uint64_t)&idt;
    idt_flush((uint64_t)&idt_desc);
}

// Set the moment an exception (int_no < 32) starts being reported
// below, cleared never -- this handler halts the CPU permanently once
// it finishes reporting one, so there's no "done reporting, safe to
// report again" state to return to.
//
// Guards against a specific failure mode: if the exception-reporting
// code itself (vga_puts -> ... -> fb_fill_rect/clip_rect, see
// drivers/fbcon.c) touches invalid memory, that triggers a SECOND
// exception, which re-enters this handler and tries to report AGAIN,
// which can fault again, forever -- each pass pushing a fresh set of
// stack frames (ISR entry + the reporting call chain) without ever
// unwinding the previous ones. Confirmed via GDB: caught mid-loop
// with $rsp low enough that a struct return (fb_get_info(), in
// clip_rect) was landing inside .bss -- specifically observed
// overwriting grid_width/grid_height (drivers/fbcon.c), corrupting
// state that fbcon_present() reads on every call, worsening the
// spiral further. This flag turns "silent infinite reporting loop,
// indistinguishable from a hang" into "reports the first exception,
// then halts cleanly" -- if a second exception fires while this is
// set, that means the report path itself is broken, so this skips
// straight to halting instead of retrying it.
static volatile bool already_reporting_exception = false;

// Main C handler -- called by every stub above.
void interrupt_handler(interrupt_frame_t *frame)
{
    if (frame->int_no < 32)
    {
        // DIAGNOSTIC TEMPORAIRE -- halt immédiat, sans aucun logging,
        // pour isoler si le triple fault vient du code de logging
        // (emergency_vga_*/vga_puts/fbcon) ou d'ailleurs. A restaurer
        // une fois confirmé/infirmé.
        __asm__ volatile("cli");
        for (;;)
            __asm__ volatile("hlt");

        if (already_reporting_exception)
        {
            // Second exception while still reporting the first one --
            // the reporting path itself is what's faulting. Don't
            // retry it (see comment above); write a minimal message
            // directly to VGA memory (bypassing the broken path
            // entirely) so there's still SOMETHING visible, then stop.
            emergency_vga_line("!! DOUBLE FAULT WHILE REPORTING EXCEPTION -- SEE SERIAL LOG !!");
            __asm__ volatile("cli");
            for (;;)
                __asm__ volatile("hlt");
        }
        already_reporting_exception = true;

        // Written FIRST, via the raw/always-safe path -- guarantees
        // int_no/rip/cr2 are visible even if the fancier report just
        // below (vga_puts -> ... -> fbcon.c) is itself what ends up
        // faulting. Row 0 = summary, row 1 = rip, row 2 = cr2.
        emergency_vga_line("!! EXCEPTION -- see hex below (row1=RIP row2=CR2) !!");
        emergency_vga_hex(1, 0, frame->int_no);
        emergency_vga_hex(1, 20, frame->rip);
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        emergency_vga_hex(2, 0, cr2);

        vga_set_color(VGA_WHITE, VGA_RED);

        vga_puts("\n!! EXCEPTION: ");
        vga_puts(exception_names[frame->int_no]);
        vga_puts(" (");
        vga_putdec(frame->int_no);
        vga_puts(") !!");

        // Remembers where the first line ended, so the second line can
        // be padded to the same length (keeps the red background block
        // a clean rectangle instead of a ragged edge).
        uint8_t target_column = vga_get_col();

        vga_puts("\n");

        vga_puts("RIP: ");
        vga_puthex(frame->rip);
        vga_puts("  ERR: ");
        vga_puthex(frame->err_code);

        while (vga_get_col() < target_column)
        {
            vga_putchar(' ');
        }

        vga_set_color(VGA_WHITE, VGA_BLACK);
        vga_puts("\n");

        __asm__ volatile("cli");
        for (;;)
            __asm__ volatile("hlt");
    }
    else
    {
        uint8_t irq = frame->int_no - 32;
        if (irq == 0)
        {
            // IRQ0 (PIT tick / scheduler) is handled entirely in
            // idt.asm, not here.
        }
        else if (irq == 1)
        {
            keyboard_handler();
        }
        else if (irq == 12)
        {
            mouse_handler();
        }
        pic_send_eoi(irq);
    }
}