// ApexKernel/Includes/Core/Interrupts/PIC/PICControl.h
// ApexOS -- PIC 8259 runtime control (EOI, IRQ masking)

#ifndef APEX_CORE_INTERRUPTS_PIC_CONTROL_H
#define APEX_CORE_INTERRUPTS_PIC_CONTROL_H

#include "Core/Types.h"

void pic_send_eoi(uint8_t irq);
void pic_mask_irq(uint8_t irq);
void pic_unmask_irq(uint8_t irq);

#endif // APEX_CORE_INTERRUPTS_PIC_CONTROL_H
