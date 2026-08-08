// apex-kernel/core/idt.h
// ApexOS -- Interrupt Descriptor Table

#ifndef APEX_IDT_H
#define APEX_IDT_H

#include "../include/apex/types.h"

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
void idt_set_entry(uint8_t num, uint64_t handler, uint8_t flags);

void interrupt_handler(interrupt_frame_t *frame);

#endif // APEX_IDT_H