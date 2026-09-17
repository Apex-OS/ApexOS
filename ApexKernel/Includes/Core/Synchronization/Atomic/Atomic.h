// ApexKernel/Includes/Core/Synchronization/Atomic/Atomic.h
// ApexOS -- Generic atomic operations (SMP fundamentals)
//
// Every atomic operation this kernel has needed so far was written
// inline, once, exactly where it was used -- Spinlock.c's own xchg,
// Core/SMP/APIC/Lapic.c's rdmsr/wrmsr, PerCpu.c's GS_BASE round-trip.
// That was fine while each of those was genuinely a one-off talking
// to a specific piece of hardware. It stops being fine the moment
// ordinary in-memory state needs to be shared and mutated by more
// than one CPU at once WITHOUT the cost of a full spinlock_acquire()/
// spinlock_release() pair around it -- a plain shared counter, for
// instance, where the only thing that needs to be atomic is the
// increment itself, not some larger multi-step operation the counter
// happens to be part of. Core/Tasking/Scheduler/Scheduler.c's
// proc_count is exactly this kind of case, and is the reason this
// file exists now rather than being written inline again for a fifth
// time.
//
// Every function here compiles to a single locked instruction (the
// x86 LOCK prefix) -- these are NOT a substitute for
// Core/Synchronization/Spinlock/Spinlock.h when protecting multi-step
// sequences (e.g. Scheduler.c's linked-list insert/remove, several
// separate reads and writes that all need to happen as one unit) --
// use a spinlock for that, exactly as before. These are for the
// narrower case of a single read-modify-write on a single word.

#ifndef APEX_CORE_SYNCHRONIZATION_ATOMIC_ATOMIC_H
#define APEX_CORE_SYNCHRONIZATION_ATOMIC_ATOMIC_H

#include "Core/Types.h"

// Atomically adds `value` to *ptr and returns the value *ptr held
// BEFORE the add (the x86 XADD instruction's own semantics) -- e.g.
// atomic_add32(&proc_count, 1) both increments proc_count and hands
// back what it was a moment ago, useful when a caller needs to know
// which specific former value it claimed (Scheduler.c doesn't need
// this yet, but a future per-CPU work-stealing scheme assigning
// process slots by claiming successive counter values would).
uint32_t atomic_add32(volatile uint32_t *ptr, uint32_t value);
uint64_t atomic_add64(volatile uint64_t *ptr, uint64_t value);

// Convenience wrappers over atomic_add32/64 with value = 1 or -1 --
// covers the overwhelmingly common case (a plain shared counter, see
// this file's own header comment) without every call site spelling
// out atomic_add32(ptr, (uint32_t)-1) for a decrement.
uint32_t atomic_inc32(volatile uint32_t *ptr);
uint32_t atomic_dec32(volatile uint32_t *ptr);
uint64_t atomic_inc64(volatile uint64_t *ptr);
uint64_t atomic_dec64(volatile uint64_t *ptr);

// Compare-and-swap: if *ptr == expected, atomically sets *ptr = desired
// and returns true. If *ptr != expected, *ptr is left completely
// unchanged and this returns false -- the caller decides what to do
// next (commonly: re-read *ptr and retry with a new `expected`, the
// standard lock-free retry-loop pattern). This is the operation
// Core/Synchronization/Mutex/Mutex.c's mutex_try_lock() is built on --
// a mutex's locked/unlocked flag is exactly a single word suitable for
// CAS, its acquire is nothing more than "CAS 0 -> 1, retry if that
// fails".
bool atomic_cas32(volatile uint32_t *ptr, uint32_t expected, uint32_t desired);
bool atomic_cas64(volatile uint64_t *ptr, uint64_t expected, uint64_t desired);

// Atomically stores `value` into *ptr and returns whatever *ptr held
// immediately before -- the exact primitive Spinlock.c's own
// spinlock_acquire() already hand-rolled (see that file's
// atomic_xchg()) before this header existed. Not migrating
// Spinlock.c to call this instead of its own copy for now -- that
// file is foundational enough (every other piece of SMP work in this
// kernel depends on it working exactly as already tested) that a
// purely cosmetic refactor of working, heavily-relied-upon code isn't
// worth the retest it would demand for zero behavioral change.
uint32_t atomic_exchange32(volatile uint32_t *ptr, uint32_t value);
uint64_t atomic_exchange64(volatile uint64_t *ptr, uint64_t value);

#endif // APEX_CORE_SYNCHRONIZATION_ATOMIC_ATOMIC_H
