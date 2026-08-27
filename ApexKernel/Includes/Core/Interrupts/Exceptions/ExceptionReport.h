// ApexKernel/Includes/Core/Interrupts/Exceptions/ExceptionReport.h
// ApexOS -- CPU exception reporting (crash screen)

#ifndef APEX_CORE_INTERRUPTS_EXCEPTION_REPORT_H
#define APEX_CORE_INTERRUPTS_EXCEPTION_REPORT_H

#include "Core/Interrupts/IDT/IDT.h"

// Reports a CPU exception (int_no < 32) and halts. Never returns.
void exception_report(interrupt_frame_t *frame);

#endif // APEX_CORE_INTERRUPTS_EXCEPTION_REPORT_H
