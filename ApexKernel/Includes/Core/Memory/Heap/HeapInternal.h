// ApexKernel/Includes/Core/Memory/Heap/HeapInternal.h
// ApexOS -- Shared internal state for the Heap module
//
// heap_head and heap_size are defined once in Heap.c and shared via
// extern with HeapSetup.c and HeapStats.c. Not part of the public
// Heap.h API: nothing outside Core/Memory/Heap/ should touch these
// directly.

#ifndef APEX_CORE_MEMORY_HEAP_INTERNAL_H
#define APEX_CORE_MEMORY_HEAP_INTERNAL_H

#include "Core/Memory/Heap/Heap.h"

extern heap_block_t *heap_head;
extern size_t heap_size;

#endif // APEX_CORE_MEMORY_HEAP_INTERNAL_H
