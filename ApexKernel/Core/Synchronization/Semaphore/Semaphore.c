// ApexKernel/Core/Synchronization/Semaphore/Semaphore.c
// ApexOS -- Counting semaphore (see Semaphore.h)

#include "Core/Synchronization/Semaphore/Semaphore.h"
#include "Core/Synchronization/Atomic/Atomic.h"

void sem_init(semaphore_t *s, int32_t initial_count)
{
    s->count = initial_count;
}

bool sem_try_wait(semaphore_t *s)
{
    // A plain atomic_dec32() would be wrong here: it unconditionally
    // decrements, so two CPUs racing when count == 1 could both
    // "succeed" (one dropping count to 0, the other to -1), each
    // believing it claimed a real unit when only one existed. This
    // CAS loop instead reads the current count, and only commits the
    // decrement if count hasn't changed since that read -- if another
    // CPU's own sem_wait()/sem_try_wait() got there first, the CAS
    // fails (current has since moved), and this loop re-reads and
    // retries rather than blindly decrementing whatever the new value
    // is.
    int32_t current = s->count;
    while (current > 0)
    {
        if (atomic_cas32((volatile uint32_t *)&s->count, (uint32_t)current, (uint32_t)(current - 1)))
            return true;

        // CAS failed -- some other CPU changed count between this
        // loop's read and its attempted commit. Re-read and retry
        // rather than assuming why it failed; another sem_wait() and
        // a sem_signal() landing at the same moment look identical
        // from here, and either way the only correct move is trying
        // again against whatever count actually is now.
        current = s->count;
    }
    return false;
}

void sem_wait(semaphore_t *s)
{
    // Busy-waits calling sem_try_wait() repeatedly rather than
    // reimplementing its own CAS loop -- see this file's own header
    // comment (Semaphore.h) on why sem_wait() spins at all (no
    // scheduler-level blocking exists yet).
    while (!sem_try_wait(s))
        __asm__ volatile("pause");
}

void sem_signal(semaphore_t *s)
{
    atomic_inc32((volatile uint32_t *)&s->count);
}
