// ApexKernel/Includes/Core/Interrupts/PIC/PICControl.h
// ApexOS -- PIC 8259 runtime control (EOI, IRQ masking)

#ifndef APEX_CORE_INTERRUPTS_PIC_CONTROL_H
#define APEX_CORE_INTERRUPTS_PIC_CONTROL_H

#include "Core/Types.h"

void pic_send_eoi(uint8_t irq);
void pic_mask_irq(uint8_t irq);
void pic_unmask_irq(uint8_t irq);

// Returns true if `irq` is currently masked at the PIC. Used by
// callers (Core/Syscalls/Handlers/SysRead.c) that need to temporarily
// mask an IRQ and restore its EXACT prior state afterward, rather
// than unconditionally unmasking it back on -- unconditional unmask
// would be wrong for a caller that reached that code with the IRQ
// already masked for its own separate reason.
bool pic_irq_is_masked(uint8_t irq);

#endif // APEX_CORE_INTERRUPTS_PIC_CONTROL_H
