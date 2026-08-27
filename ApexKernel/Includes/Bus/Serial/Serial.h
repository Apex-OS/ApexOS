// ApexKernel/Includes/Bus/Serial/Serial.h
// ApexOS -- Generic serial output (chip-agnostic)

#ifndef APEX_BUS_SERIAL_H
#define APEX_BUS_SERIAL_H

#include "Core/Types.h"

void serial_puts(const char *str);
void serial_puthex(uint64_t val);
void serial_putdec(uint64_t val);

#endif // APEX_BUS_SERIAL_H
