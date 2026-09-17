// ApexKernel/Includes/Core/Interrupts/IDT/IDT.h
// ApexOS -- Interrupt Descriptor Table
//
// Owns every type shared across Core/Interrupts/: idt_entry_t,
// idt_descriptor_t, interrupt_frame_t, and the IDT/exception
// constants. Handlers/, Exceptions/, and IRQ/ all #include this.

#ifndef APEX_CORE_INTERRUPTS_IDT_H
#define APEX_CORE_INTERRUPTS_IDT_H

#include "Core/Types.h"

// One IDT entry (16 bytes in 64-bit mode).
typedef struct __attribute__((packed))
{
    uint16_t offset_low;  // Handler [0:15]
    uint16_t selector;    // Code segment selector
    uint8_t ist;          // Interrupt Stack Table (0 = disabled)
    uint8_t flags;        // Type + DPL + Present
    uint16_t offset_mid;  // Handler [16:31]
    uint32_t offset_high; // Handler [32:63]
    uint32_t reserved;    // Always 0
} idt_entry_t;

// IDTR register layout.
typedef struct __attribute__((packed))
{
    uint16_t limit;
    uint64_t base;
} idt_descriptor_t;

#define IDT_FLAG_PRESENT (1 << 7)
#define IDT_FLAG_RING0 (0 << 5)
#define IDT_FLAG_RING3 (3 << 5)
#define IDT_FLAG_TRAP 0x0F      // Trap gate
#define IDT_FLAG_INTERRUPT 0x0E // Interrupt gate (clears IF)

// The 32 CPU exception numbers.
#define EXCEPTION_DE 0  // Division by Zero
#define EXCEPTION_DB 1  // Debug
#define EXCEPTION_NMI 2 // Non-Maskable Interrupt
#define EXCEPTION_BP 3  // Breakpoint
#define EXCEPTION_OF 4  // Overflow
#define EXCEPTION_BR 5  // Bound Range Exceeded
#define EXCEPTION_UD 6  // Invalid Opcode
#define EXCEPTION_NM 7  // Device Not Available
#define EXCEPTION_DF 8  // Double Fault
#define EXCEPTION_TS 10 // Invalid TSS
#define EXCEPTION_NP 11 // Segment Not Present
#define EXCEPTION_SS 12 // Stack Segment Fault
#define EXCEPTION_GP 13 // General Protection Fault
#define EXCEPTION_PF 14 // Page Fault
#define EXCEPTION_MF 16 // x87 FPU Error
#define EXCEPTION_AC 17 // Alignment Check
#define EXCEPTION_MC 18 // Machine Check
#define EXCEPTION_XF 19 // SIMD FPU Exception

// Registers saved on the stack during an interrupt.
typedef struct __attribute__((packed))
{
    // General-purpose registers (saved by our stubs).
    uint64_t r15, r14, r13, r12;
    uint64_t r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx;
    uint64_t rcx, rbx, rax;

    // Pushed by our stubs.
    uint64_t int_no;
    uint64_t err_code;

    // Pushed automatically by the CPU.
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} interrupt_frame_t;

void idt_init(void);

// Loads the IDT idt_init() already built (idt_flush() against the
// same idt_desc the BSP itself used) onto WHICHEVER CPU calls this --
// meant for an AP (Core/SMP/Startup/AppProcessorStartup.c's
// ap_c_entry()) to call once, since idt_init() itself only ever runs
// on the BSP (Init.c) and each AP otherwise starts with no IDT loaded
// at all (IDTR left at its post-SIPI default, an invalid/empty
// table). Safe to share one IDT physical table across every CPU the
// same way KERNEL_PML4 already is (Boot/Transition/LongModeEntry.asm,
// Arch/x86_64/ApBootTrampoline.asm) -- an IDT is read-only from every
// CPU's perspective once built, nothing here ever modifies it after
// idt_init()'s one-time setup, so there's no cross-CPU mutation
// hazard the way rebuilding page tables per-CPU would have (see
// ApBootTrampoline.asm's own comment on that exact danger for CR3).
void idt_load_on_this_cpu(void);
void idt_set_entry(uint8_t num, uint64_t handler, uint8_t flags);

#endif // APEX_CORE_INTERRUPTS_IDT_H
