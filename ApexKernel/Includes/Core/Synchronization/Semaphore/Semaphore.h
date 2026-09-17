// ApexKernel/Includes/Core/Synchronization/Semaphore/Semaphore.h
// ApexOS -- Counting semaphore
//
// Distinct from Core/Synchronization/Mutex/Mutex.h in two ways: a
// semaphore's count can be greater than 1 (limiting concurrent access
// to N identical resources -- a mutex only ever models exactly one),
// and it has no ownership concept at all -- ANY CPU can sem_signal()
// a semaphore, regardless of which CPU (if any) is currently
// sem_wait()-ing on it, unlike Mutex.h's own owner_cpu_index tracking
// which specifically requires the SAME CPU that locked a mutex to be
// the one that unlocks it. This makes a semaphore the right tool for
// producer/consumer signaling (one CPU produces something and
// signals, a completely different CPU is the one waiting to consume
// it) where a mutex's ownership rule would actively get in the way.
//
// Same honest limitation as Mutex.h: sem_wait() busy-waits (spins)
// rather than actually blocking/descheduling the calling process --
// this kernel has no scheduler-level primitive yet to mark a process
// NOT_RUNNABLE and switch away while it waits (see Mutex.h's own
// header comment on Core/Tasking/Scheduler/Scheduler.c's current_proc
// still being unmigrated to per-CPU). A sem_wait() that actually
// sleeps is follow-up work for once that scheduler migration exists.

#ifndef APEX_CORE_SYNCHRONIZATION_SEMAPHORE_SEMAPHORE_H
#define APEX_CORE_SYNCHRONIZATION_SEMAPHORE_SEMAPHORE_H

#include "Core/Types.h"

typedef struct
{
    // Never manipulated directly -- always through atomic_dec32()/
    // atomic_inc32()/atomic_cas32() (Core/Synchronization/Atomic/
    // Atomic.h) in Semaphore.c, so every change is a single locked
    // instruction rather than a read-modify-write spinlock_acquire()/
    // release() pair would need to protect. A semaphore's entire
    // state IS this one word -- unlike Mutex.h's mutex_t, which pairs
    // a spinlock with a separate owner_cpu_index that must be updated
    // together as one unit, there's nothing else here that could ever
    // need to change atomically alongside `count`.
    volatile int32_t count;
} semaphore_t;

#define SEMAPHORE_INIT(initial_count) {(initial_count)}

void sem_init(semaphore_t *s, int32_t initial_count);

// Blocks (busy-waits, see this file's own header comment) until count
// is greater than 0, then atomically decrements it and returns. Two
// CPUs both waiting when count == 1 -- only one of them ever
// successfully claims that decrement (via atomic_cas32() retry loop
// in Semaphore.c); the other keeps spinning exactly as intended, the
// same guarantee atomic_inc32() gave Core/SMP/Startup/
// AppProcessorStartup.c's shared test counter against losing an
// increment, applied here to not letting two waiters both believe
// they claimed the same single available unit.
void sem_wait(semaphore_t *s);

// Non-blocking version of sem_wait() -- attempts to claim one unit
// once, returns immediately either way. Returns true (count
// decremented) if a unit was available; false (count untouched) if it
// was already 0.
bool sem_try_wait(semaphore_t *s);

// Atomically increments count -- releases one unit back, or (for the
// producer/consumer case this file's own header comment describes)
// signals that one unit of work is now available for some other CPU's
// sem_wait() to claim. No upper bound enforced: a semaphore
// initialized with sem_init(s, 1) and signaled twice in a row without
// an intervening wait reaches count == 2, silently allowing two
// concurrent claims where only one resource may have actually existed
// -- correct use is the caller's responsibility (matching every
// sem_signal() to a genuine unit becoming available), same as a mutex
// can be corrupted by a caller that unlocks one it never locked
// (Mutex.h's own owner_cpu_index check catches THAT specific misuse
// only because ownership is trackable there at all; a semaphore's
// anonymous multi-waiter design has no equivalent check available).
void sem_signal(semaphore_t *s);

#endif // APEX_CORE_SYNCHRONIZATION_SEMAPHORE_SEMAPHORE_H
