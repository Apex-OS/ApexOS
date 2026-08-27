// ApexKernel/Includes/Core/Tasking/Process/Process.h
// ApexOS -- Process Control Block

#ifndef APEX_CORE_TASKING_PROCESS_H
#define APEX_CORE_TASKING_PROCESS_H

#include "Core/Types.h"

// Was 16KB -- confirmed via GDB (hardware watchpoint on a heap
// block's magic field, backtrace, $rsp inspection) that this was too
// small: the shell process's call chain into AXFS
// (Shell.c:shell_run -> ... -> EntryLifecycle.c:axfs_create ->
// alloc_data_block -> bitmap_find_free -> bitmap_test_bit ->
// axfs_disk_read_block) stacks several 4KB local buffers
// (uint8_t buf[AXFS_BLOCK_SIZE] in more than one of those frames) on
// top of each other, plus every intermediate frame's own locals.
// $rsp ended up only ~1.2KB from the heap block allocated right after
// this process's stack (proc->stack_base is a kmalloc() allocation,
// not a fixed low-memory region -- see process_create() in
// Process.c), so a deeper call or a slightly larger local overran the
// 16KB allocation and silently corrupted that block's magic, which is
// what made every kmalloc() afterward fail (kmalloc() aborts the
// whole free-list walk the moment it hits a block with a bad magic --
// see ApexKernel/Core/Memory/Heap/Heap.c). 64KB gives that same call
// chain comfortable headroom.
#define PROCESS_STACK_SIZE 65536 // 64KB per process
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

#endif // APEX_CORE_TASKING_PROCESS_H
