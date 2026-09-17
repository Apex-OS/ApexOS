// ApexKernel/Includes/Core/Synchronization/Mutex/Mutex.h
// ApexOS -- Mutual exclusion lock with ownership tracking
//
// Distinct from Core/Synchronization/Spinlock/Spinlock.h in exactly
// one way: a mutex tracks WHICH CPU currently holds it (via
// Core/SMP/PerCPU/PerCpu.h's percpu_get()->cpu_index), so
// mutex_unlock() can catch the specific bug class Spinlock.h's own
// header comment says a plain spinlock cannot -- "called on a lock
// this CPU didn't acquire" is undefined behavior for a spinlock (no
// ownership tracking at all there), but a mutex_unlock() called by
// the wrong CPU is a detectable, loud error here instead of silent
// corruption of whatever the lock protects.
//
// Otherwise this IS a spinlock underneath (mutex_lock() busy-waits
// exactly like spinlock_acquire() does) -- this kernel has no
// scheduler-level blocking primitive yet (no way to mark a process
// NOT_RUNNABLE and switch away from it while it waits, then wake it
// specifically when the mutex frees up -- see Core/Tasking/Scheduler/
// Scheduler.c's own current_proc still being a single global,
// unmigrated to per-CPU as of this phase). A "real" mutex that
// sleeps instead of spinning is follow-up work for once that
// scheduler migration exists; this one is honest about spinning under
// the hood rather than pretending otherwise.

#ifndef APEX_CORE_SYNCHRONIZATION_MUTEX_MUTEX_H
#define APEX_CORE_SYNCHRONIZATION_MUTEX_MUTEX_H

#include "Core/Types.h"
#include "Core/Synchronization/Spinlock/Spinlock.h"

typedef struct
{
    spinlock_t lock;

    // Index (Core/SMP/PerCPU/PerCpu.h's percpu_t.cpu_index) of
    // whichever CPU currently holds this mutex, or
    // MUTEX_NO_OWNER if it's free. Written only by whichever CPU just
    // successfully acquired the underlying spinlock (mutex_lock()/
    // mutex_try_lock()), read by mutex_unlock() to catch the wrong-CPU
    // case described in this file's own header comment -- there is no
    // race on this field despite being unprotected by anything beyond
    // `lock` itself: only the CPU that currently holds `lock` is ever
    // permitted to write owner_cpu_index, and mutex_unlock() only
    // reads it while that same CPU still holds `lock` (immediately
    // before releasing it).
    int32_t owner_cpu_index;
} mutex_t;

#define MUTEX_NO_OWNER (-1)
#define MUTEX_INIT {SPINLOCK_INIT, MUTEX_NO_OWNER}

void mutex_init(mutex_t *m);

// Blocks (busy-waits, see this file's own header comment on why) until
// the mutex is free, then acquires it and records the calling CPU as
// owner. Safe to call before Core/SMP/PerCPU/PerCpu.h's per-CPU array
// exists (percpu_get() would then return garbage) ONLY on
// single-core boot paths that never reach percpu_init_array() at
// all -- once SMP is up, every mutex_lock() call assumes percpu_get()
// is valid, same assumption every other per-CPU-aware piece of this
// kernel already makes post-Phase-4.
void mutex_lock(mutex_t *m);

// Non-blocking version of mutex_lock() -- attempts to acquire once,
// returns immediately either way. Returns true (and the mutex is now
// held, owner recorded) if it was free; false (mutex untouched) if
// another CPU already held it. For call sites that need to do
// something else instead of spinning when contended, e.g. an
// interrupt handler that can't afford to block.
bool mutex_try_lock(mutex_t *m);

// Releases a mutex THIS CPU currently holds. Logs an error and
// refuses to release (the lock stays held) if called by any CPU other
// than the one recorded as owner -- see this file's own header
// comment on why this is specifically what a mutex catches that a
// bare spinlock_release() cannot: releasing a lock a different CPU
// holds would let two CPUs believe they simultaneously own whatever
// this mutex protects, corruption Spinlock.h's own docs describe as
// undefined behavior for the equivalent spinlock misuse.
void mutex_unlock(mutex_t *m);

#endif // APEX_CORE_SYNCHRONIZATION_MUTEX_MUTEX_H
