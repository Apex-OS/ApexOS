// apex-kernel/core/userspace.h
// ApexOS -- Userspace jump utilities

#ifndef APEX_USERSPACE_H
#define APEX_USERSPACE_H

#include "../include/apex/types.h"

#define USER_STACK_ADDR 0x700000 // Userspace stack
#define USER_STACK_SIZE 16384    // 16KB
#define USER_CODE_ADDR 0x400000  // Userspace code

// Address where a .axl imported by a .axp is loaded (V1 -- one .axl
// at a time, fixed address; no support yet for several libraries
// loaded simultaneously at different addresses). Far enough from
// USER_CODE_ADDR (0x400000, 1 page = 4KB) and AXF_IMPORT_TABLE_ADDR
// (0x401000) to avoid any overlap, and far from USER_STACK_ADDR
// (0x700000) too.
#define USER_LIB_ADDR 0x600000

void userspace_jump(uint64_t entry, uint64_t stack);

// Symbols marking the bounds of user_main in the kernel binary (see
// usertest.c). Give its EXACT size instead of copying a fixed,
// arbitrary size that risked spilling into neighboring kernel code.
extern uint8_t user_main_start[];
extern uint8_t user_main_end[];

#endif // APEX_USERSPACE_H