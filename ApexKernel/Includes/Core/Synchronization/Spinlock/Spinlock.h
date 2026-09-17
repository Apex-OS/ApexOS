// ApexKernel/Includes/Core/Synchronization/Spinlock/Spinlock.h
// ApexOS -- Basic SMP spinlock (test-and-set with PAUSE backoff)

#ifndef APEX_CORE_SYNCHRONIZATION_SPINLOCK_SPINLOCK_H
#define APEX_CORE_SYNCHRONIZATION_SPINLOCK_SPINLOCK_H

#include "Core/Types.h"

// 0 == unlocked, 1 == locked. volatile so the compiler never caches
// this in a register across loop iterations in spinlock_acquire() --
// every check must actually re-read memory, since another CPU can
// change it between iterations (that's the whole point of a
// spinlock: this word is the only thing the two cores agree on).
typedef struct
{
    volatile uint32_t locked;
} spinlock_t;

// Static initializer for a spinlock declared as a global/struct
// member -- e.g. `spinlock_t my_lock = SPINLOCK_INIT;`. Equivalent to
// calling spinlock_init() but usable at compile time.
#define SPINLOCK_INIT {0}

void spinlock_init(spinlock_t *lock);

// Spins (busy-waits) until the lock is acquired. Safe to call before
// any other CPU is running (single-CPU boot path) -- it just succeeds
// on the first try, same atomic exchange either way.
void spinlock_acquire(spinlock_t *lock);

// Releases a lock this CPU currently holds. Undefined behavior (data
// corruption on whatever the lock protects) if called on a lock this
// CPU didn't acquire -- there's no ownership tracking here, callers
// are responsible for acquire/release pairing.
void spinlock_release(spinlock_t *lock);

// Attempts to acquire without spinning -- returns true if the lock
// was free and is now held by this CPU, false if it was already
// held (in which case nothing happens; the caller decides what to do
// instead of blocking, e.g. an interrupt handler that can't afford to
// spin).
bool spinlock_try_acquire(spinlock_t *lock);

// ── IRQ-safe variants ──────────────────────────────────────────────────
// Required whenever a lock protects state ALSO touched from inside an
// interrupt handler on the SAME CPU -- e.g. Core/Tasking/Scheduler/
// Scheduler.c's proc_list_lock, held both by ordinary kernel code
// (scheduler_add()/scheduler_remove()) and read by
// Core/Tasking/Scheduler/SchedulerTick.c's scheduler_tick_asm(), which
// runs from the IRQ0 timer handler. Plain spinlock_acquire() is not
// enough there: if this CPU is holding the lock via ordinary code and
// a timer interrupt fires on this SAME CPU before spinlock_release()
// runs, the interrupt handler would call spinlock_acquire() on a lock
// this exact CPU already holds -- an immediate deadlock, since a
// spinlock is not reentrant and there is no other CPU left to ever
// release it. Disabling interrupts for the duration this CPU holds
// the lock closes that window entirely: no interrupt can fire on this
// CPU (and therefore no interrupt handler can attempt to re-acquire)
// until spinlock_release_irqrestore() below explicitly restores
// whatever interrupt state existed before spinlock_acquire_irqsave()
// was called.
//
// The "irqsave"/"irqrestore" naming and RFLAGS-based mechanism follow
// the same convention Linux's own spin_lock_irqsave() uses for
// exactly this problem -- saving and restoring the prior flag state
// (rather than unconditionally sti-ing on release) matters because a
// caller that already had interrupts disabled before calling this
// (e.g. one IRQ-safe critical section nested inside another) must not
// have this function re-enable them early out from under the outer
// caller.

// Saves the current interrupt-enable state (RFLAGS' IF bit) into
// `*flags`, disables interrupts on this CPU, then acquires the lock
// exactly like spinlock_acquire(). Must be paired with
// spinlock_release_irqrestore() using the SAME `flags` value this call
// wrote.
void spinlock_acquire_irqsave(spinlock_t *lock, uint64_t *flags);

// Releases the lock (exactly like spinlock_release()), then restores
// interrupts to whatever state `flags` recorded -- re-enabling them
// only if they were actually enabled before the matching
// spinlock_acquire_irqsave() call, never unconditionally.
void spinlock_release_irqrestore(spinlock_t *lock, uint64_t flags);

#endif // APEX_CORE_SYNCHRONIZATION_SPINLOCK_SPINLOCK_H
