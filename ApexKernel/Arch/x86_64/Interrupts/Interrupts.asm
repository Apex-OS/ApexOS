; ApexKernel/Arch/x86_64/Interrupts/Interrupts.asm
; ApexOS -- Generic ISR/IRQ stub dispatch (IDT flush, exception stubs,
; IRQ1-15 stubs, common handlers). IRQ0 (timer + context switch) is
; handled separately -- see TimerInterrupt.asm.

[BITS 64]

global idt_flush
global isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7
global isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15
global isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
global isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
global irq1,  irq2,  irq3,  irq4,  irq5,  irq6,  irq7
global irq8,  irq9,  irq10, irq11, irq12, irq13, irq14, irq15
global isr240 ; IPI_VECTOR (0xF0), see Core/SMP/IPI/Ipi.h
global isr48, isr49, isr50, isr51, isr52, isr53, isr54, isr55
global isr56, isr57, isr58, isr59, isr60, isr61, isr62, isr63 ; IOAPIC_VECTOR_BASE (0x30) + IRQ 0-15, see Core/SMP/IOAPIC/IoApic.h
global isr241 ; TLB_SHOOTDOWN_VECTOR (0xF1), see Core/SMP/TLB/TlbShootdown.h

extern interrupt_handler

section .text

idt_flush:
    lidt [rdi]
    ret

; ── Macros ───────────────────────────────────────────────────────────────
%macro ISR_NOERR 1
isr%1:
    push qword 0
    push qword %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
isr%1:
    push qword %1
    jmp isr_common
%endmacro

%macro IRQ 2
irq%1:
    push qword 0
    push qword %2
    jmp irq_common
%endmacro

; ── ISR definitions ───────────────────────────────────────────────────────
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

; ── IRQ 1-15 ─────────────────────────────────────────────────────────────
IRQ 1,  33
IRQ 2,  34
IRQ 3,  35
IRQ 4,  36
IRQ 5,  37
IRQ 6,  38
IRQ 7,  39
IRQ 8,  40
IRQ 9,  41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

; ── Generic IPI (Core/SMP/IPI/Ipi.h) ────────────────────────────────────
; Reuses isr_common (below), NOT irq_common -- irq_common's own tail
; calls pic_send_eoi() (Core/Interrupts/PIC/PICControl.c), the wrong
; acknowledgment mechanism entirely for an interrupt that arrived via
; a LAPIC IPI rather than the legacy PIC. isr_common has no EOI logic
; of its own either way (every CPU exception it also handles is a
; fault/trap, never something an EOI applies to) -- Core/SMP/IPI/
; IpiDispatch.c's ipi_dispatch() sends the LAPIC EOI itself
; (Core/SMP/APIC/Lapic.h's lapic_send_eoi()) from C once it's done,
; the correct acknowledgment for this specific vector.
ISR_NOERR 240

; ── I/O APIC IRQ vectors (IOAPIC_VECTOR_BASE=0x30 through 0x3F) ──────────
; Reuses isr_common (below), NOT irq_common -- same reasoning as
; ISR_NOERR 240's own comment just above: irq_common's tail calls
; pic_send_eoi() (Core/Interrupts/PIC/PICControl.c), the wrong
; acknowledgment mechanism for an interrupt that arrived via the I/O
; APIC/LAPIC path rather than the legacy PIC. Core/Interrupts/IRQ/
; IRQDispatch.c's irq_dispatch() sends the LAPIC EOI itself
; (Core/SMP/APIC/Lapic.h's lapic_send_eoi()) once it detects the
; vector is in this range -- see that file's own comment.
ISR_NOERR 48
ISR_NOERR 49
ISR_NOERR 50
ISR_NOERR 51
ISR_NOERR 52
ISR_NOERR 53
ISR_NOERR 54
ISR_NOERR 55
ISR_NOERR 56
ISR_NOERR 57
ISR_NOERR 58
ISR_NOERR 59
ISR_NOERR 60
ISR_NOERR 61
ISR_NOERR 62
ISR_NOERR 63

; ── TLB shootdown (Core/SMP/TLB/TlbShootdown.h) ──────────────────────────
; Same isr_common reuse, same EOI-via-LAPIC reasoning as ISR_NOERR 240
; just above -- this is another CPU's own coordination IPI, never
; delivered via the PIC.
ISR_NOERR 241

; ── Handler commun ISR ────────────────────────────────────────────────────
isr_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    call interrupt_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16
    iretq

; ── Handler commun IRQ 1-15 ──────────────────────────────────────────────
irq_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    call interrupt_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16
    iretq
