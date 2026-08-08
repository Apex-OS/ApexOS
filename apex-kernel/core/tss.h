// apex-kernel/core/tss.h
// ApexOS -- Task State Segment

#ifndef APEX_TSS_H
#define APEX_TSS_H

#include "../include/apex/types.h"

// 64-bit TSS layout.
typedef struct __attribute__((packed))
{
    uint32_t reserved0;
    uint64_t rsp0; // Ring 0 kernel stack
    uint64_t rsp1; // Ring 1 stack (unused)
    uint64_t rsp2; // Ring 2 stack (unused)
    uint64_t reserved1;
    uint64_t ist[7]; // Interrupt Stack Table
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base; // I/O permission bitmap
} tss_t;

void tss_init(void);
void tss_set_kernel_stack(uint64_t rsp0);

#endif // APEX_TSS_H