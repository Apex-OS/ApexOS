// ApexKernel/Core/Tasking/Context/TSS.c
// ApexOS -- Task State Segment

#include "Core/Tasking/Context/TSS.h"
#include "Arch/x86_64/GDTExtension.h"
#include "Core/Boot/Log/BootLog.h"

static tss_t tss;

// Kernel stack used when an interrupt/syscall lifts a Ring 3 process
// back to Ring 0.
#define KERNEL_STACK_SIZE 16384 // 16KB
static uint8_t kernel_stack[KERNEL_STACK_SIZE];

static inline void tss_load(uint16_t selector)
{
    __asm__ volatile("ltr %0" ::"r"(selector));
}

void tss_init(void)
{
    uint8_t *p = (uint8_t *)&tss;
    for (size_t i = 0; i < sizeof(tss_t); i++)
        p[i] = 0;

    tss.rsp0 = (uint64_t)(kernel_stack + KERNEL_STACK_SIZE);
    tss.iomap_base = sizeof(tss_t); // No I/O bitmap

    // Extends the existing GDT with the 2 entries a 64-bit TSS
    // descriptor needs. Selector ends up at 0x28 (GDT entry 5).
    gdt_install_tss((uint64_t)&tss, sizeof(tss_t) - 1);

    tss_load(0x28);

    boot_log_ok_hex("TSS initialized - kernel stack at", tss.rsp0);
}

void tss_set_kernel_stack(uint64_t rsp0)
{
    tss.rsp0 = rsp0;
}
