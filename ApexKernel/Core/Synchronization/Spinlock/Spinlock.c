// ApexKernel/Core/Synchronization/Spinlock/Spinlock.c
// ApexOS -- Basic SMP spinlock (see Spinlock.h)

#include "Core/Synchronization/Spinlock/Spinlock.h"

void spinlock_init(spinlock_t *lock)
{
    lock->locked = 0;
}

// xchg on x86 is implicitly atomic (no "lock" prefix needed, unlike
// most other read-modify-write instructions) -- it always locks the
// bus/cache line by itself. This swaps 1 into lock->locked and
// returns whatever value was there before: if that was 0, we just
// transitioned it from unlocked to locked ourselves, so the lock is
// ours; if it was already 1, someone else holds it.
static inline uint32_t atomic_xchg(volatile uint32_t *ptr, uint32_t val)
{
    uint32_t old;
    __asm__ volatile("xchg %0, %1"
                     : "=r"(old), "+m"(*ptr)
                     : "0"(val)
                     : "memory");
    return old;
}

void spinlock_acquire(spinlock_t *lock)
{
    // First attempt is a bare xchg -- the common uncontended case
    // (no other CPU wants this lock right now) succeeds immediately
    // without ever entering the spin loop below.
    while (atomic_xchg(&lock->locked, 1) != 0)
    {
        // Contended: spin on a plain read (not another xchg) while
        // waiting -- xchg's implicit bus lock is expensive and would
        // otherwise be issued every single iteration by every waiting
        // CPU, hammering the cache-coherency bus even though nothing
        // has changed. A plain load lets each waiting CPU spin out of
        // its own cached copy of the line until the holder's release
        // actually invalidates it, only then re-attempting the xchg.
        while (lock->locked != 0)
        {
            // PAUSE hints the CPU that this is a spin-wait loop --
            // reduces power draw and, on hyperthreaded cores, yields
            // execution resources to the sibling thread instead of
            // burning full-speed cycles on a loop that's deliberately
            // doing nothing useful.
            __asm__ volatile("pause");
        }
    }
}

void spinlock_release(spinlock_t *lock)
{
    // A plain store (not xchg) is enough here -- releasing doesn't
    // need to know the previous value, just needs every earlier write
    // made while holding the lock to become visible to other CPUs
    // before lock->locked visibly flips back to 0. "memory" clobber
    // stops the compiler reordering those earlier writes past this
    // store; x86's own memory model doesn't reorder stores past
    // stores, so no explicit fence instruction is needed on top.
    __asm__ volatile("" ::: "memory");
    lock->locked = 0;
}

bool spinlock_try_acquire(spinlock_t *lock)
{
    return atomic_xchg(&lock->locked, 1) == 0;
}

void spinlock_acquire_irqsave(spinlock_t *lock, uint64_t *flags)
{
    // Reads RFLAGS into `*flags` BEFORE the cli below -- this is the
    // state to restore later, i.e. whether interrupts were enabled
    // right up until this exact call, not after this function has
    // already disabled them.
    __asm__ volatile("pushfq; pop %0" : "=r"(*flags));
    __asm__ volatile("cli");
    spinlock_acquire(lock);
}

void spinlock_release_irqrestore(spinlock_t *lock, uint64_t flags)
{
    spinlock_release(lock);

    // Bit 9 of RFLAGS is IF (Interrupt Flag) -- only re-enables
    // interrupts if that bit was actually set in the flags this
    // specific call was handed, never unconditionally. See Spinlock.h's
    // own comment on why an unconditional sti here would be wrong for
    // nested IRQ-safe critical sections.
    if (flags & (1u << 9))
        __asm__ volatile("sti");
}
