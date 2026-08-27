// ApexKernel/Core/Interrupts/Exceptions/ExceptionReport.c
// ApexOS -- CPU exception reporting (crash screen)

#include "Core/Interrupts/Exceptions/ExceptionReport.h"
#include "Core/Interrupts/Exceptions/ExceptionNames.h"
#include "Drivers/Display/VGA.h"

// Emergency-only raw VGA text-mode writer, bypassing vga_puts/
// FrameBufferConsole.c entirely (see already_reporting_exception
// below for why this exists). Writes straight to 0xB8000 -- ignores
// graphics_mode, the scrollback buffer, cursor position tracking,
// everything. Only capable of a single fixed-position line; that's
// all this needs.
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
// (vga_puts/FrameBufferConsole.c) is itself broken.
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

// Set the moment an exception (int_no < 32) starts being reported
// below, cleared never -- this handler halts the CPU permanently once
// it finishes reporting one, so there's no "done reporting, safe to
// report again" state to return to.
//
// Guards against a specific failure mode: if the exception-reporting
// code itself (vga_puts -> ... -> fb_fill_rect/clip_rect, see
// Drivers/Display/Console/FrameBufferConsole.c) touches invalid
// memory, that triggers a SECOND exception, which re-enters this
// handler and tries to report AGAIN, which can fault again, forever
// -- each pass pushing a fresh set of stack frames (ISR entry + the
// reporting call chain) without ever unwinding the previous ones.
// Confirmed via GDB: caught mid-loop with $rsp low enough that a
// struct return (fb_get_info(), in clip_rect) was landing inside
// .bss -- specifically observed overwriting grid_width/grid_height
// (Drivers/Display/Console/), corrupting state that fbcon_present()
// reads on every call, worsening the spiral further. This flag turns
// "silent infinite reporting loop, indistinguishable from a hang"
// into "reports the first exception, then halts cleanly" -- if a
// second exception fires while this is set, that means the report
// path itself is broken, so this skips straight to halting instead of
// retrying it.
static volatile bool already_reporting_exception = false;

void exception_report(interrupt_frame_t *frame)
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
