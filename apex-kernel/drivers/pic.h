// apex-kernel/drivers/pic.h
// ApexOS -- PIC 8259 driver

#ifndef APEX_PIC_H
#define APEX_PIC_H

#include "../include/apex/types.h"

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
void pic_send_eoi(uint8_t irq);
void pic_mask_irq(uint8_t irq);
void pic_unmask_irq(uint8_t irq);

#endif // APEX_PIC_H