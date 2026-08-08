// apex-kernel/include/apex/types.h
// ApexOS -- Freestanding base types (no libc available)

#ifndef APEX_TYPES_H
#define APEX_TYPES_H

// Unsigned integers.
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

// Signed integers.
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;

// Memory and addressing.
typedef uint64_t uintptr_t;
typedef int64_t intptr_t;
typedef uint64_t size_t;
typedef int64_t ssize_t;

// Boolean.
typedef uint8_t bool;
#define true 1
#define false 0

#define NULL ((void *)0)

// Utility macros.
#define UNUSED(x) ((void)(x))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))
#define ALIGN_DOWN(x, align) (((x)) & ~((align) - 1))

#endif // APEX_TYPES_H