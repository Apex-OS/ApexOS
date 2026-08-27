// ApexKernel/Includes/Core/Interrupts/PIC/PICSetup.h
// ApexOS -- PIC 8259 bootstrap (cascade remap)

#ifndef APEX_CORE_INTERRUPTS_PIC_SETUP_H
#define APEX_CORE_INTERRUPTS_PIC_SETUP_H

#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI 0x20  // End of Interrupt
#define PIC_INIT 0x11 // Init cascade mode

// IRQ offsets after remap.
#define PIC1_OFFSET 0x20 // IRQ 0-7  -> INT 32-39
#define PIC2_OFFSET 0x28 // IRQ 8-15 -> INT 40-47

void pic_init(void);

#endif // APEX_CORE_INTERRUPTS_PIC_SETUP_H
