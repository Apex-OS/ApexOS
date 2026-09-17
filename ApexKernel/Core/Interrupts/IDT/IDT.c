// ApexKernel/Core/Interrupts/IDT/IDT.c
// ApexOS -- Interrupt Descriptor Table

#include "Core/Interrupts/IDT/IDT.h"
#include "Arch/x86_64/GDTSetup.h"

extern void idt_flush(uint64_t idt_descriptor);

// ISR stubs (Interrupts.asm).
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

// IRQ stubs (Interrupts.asm / TimerInterrupt.asm).
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
extern void isr240(void); // IPI_VECTOR (0xF0), see Core/SMP/IPI/Ipi.h
extern void isr49(void);  // IOAPIC_VECTOR_BASE (0x30) + IRQ1 (keyboard), see Core/SMP/IOAPIC/IoApic.h -- first IRQ migrated from PIC to I/O APIC routing, others follow once this is validated
extern void isr241(void); // TLB_SHOOTDOWN_VECTOR (0xF1), see Core/SMP/TLB/TlbShootdown.h

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

    // Generic IPI, see Core/SMP/IPI/Ipi.h.
    idt_set_entry(240, (uint64_t)isr240, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);

    // First IRQ migrated to I/O APIC routing -- see IoApic.h's own
    // header comment on why this starts with just IRQ1 (keyboard)
    // rather than converting every IRQ at once.
    idt_set_entry(49, (uint64_t)isr49, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);

    // Cross-CPU TLB invalidation -- see Core/SMP/TLB/TlbShootdown.h.
    idt_set_entry(241, (uint64_t)isr241, IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_INTERRUPT);

    idt_desc.limit = sizeof(idt) - 1;
    idt_desc.base = (uint64_t)&idt;
    idt_flush((uint64_t)&idt_desc);
}

void idt_load_on_this_cpu(void)
{
    // idt_desc was already filled in by idt_init() (the BSP's own
    // call, Init.c) -- this just re-issues the lidt against that same
    // descriptor/table from whichever CPU calls it. See IDT.h's own
    // comment on why sharing one physical table this way is safe.
    idt_flush((uint64_t)&idt_desc);
}
