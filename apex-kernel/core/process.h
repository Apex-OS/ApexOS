// apex-kernel/core/process.h
// ApexOS -- Process Control Block

#ifndef APEX_PROCESS_H
#define APEX_PROCESS_H

#include "../include/apex/types.h"

#define PROCESS_STACK_SIZE 16384 // 16KB per process
#define PROCESS_MAX 64           // Max 64 concurrent processes

typedef enum
{
    PROCESS_DEAD = 0,
    PROCESS_READY = 1,
    PROCESS_RUNNING = 2,
    PROCESS_BLOCKED = 3,
} process_state_t;

// CPU context saved on a context switch.
typedef struct __attribute__((packed))
{
    uint64_t r15, r14, r13, r12;
    uint64_t r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx;
    uint64_t rcx, rbx, rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} cpu_context_t;

typedef struct process
{
    uint32_t pid;          // Process ID
    char name[32];         // Process name
    process_state_t state; // State
    cpu_context_t context; // CPU context
    uint64_t stack_base;   // Stack base
    uint64_t stack_top;    // Stack top
    uint64_t ticks;        // CPU ticks consumed
    struct process *next;  // Linked list
} process_t;

process_t *process_create(const char *name, void (*entry)(void));
void process_destroy(process_t *proc);

#endif // APEX_PROCESS_H