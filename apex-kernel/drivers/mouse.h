// apex-kernel/drivers/mouse.h
// ApexOS -- PS/2 mouse driver (wheel scroll only for now)

#ifndef APEX_MOUSE_H
#define APEX_MOUSE_H

#include "../include/apex/types.h"

void mouse_init(void);
void mouse_handler(void); // Called by IRQ12

#endif // APEX_MOUSE_H