// ApexKernel/Core/Synchronization/Mutex/Mutex.c
// ApexOS -- Mutual exclusion lock (see Mutex.h)

#include "Core/Synchronization/Mutex/Mutex.h"
#include "Core/SMP/PerCPU/PerCpu.h"
#include "Logs/Log.h"

void mutex_init(mutex_t *m)
{
    spinlock_init(&m->lock);
    m->owner_cpu_index = MUTEX_NO_OWNER;
}

void mutex_lock(mutex_t *m)
{
    spinlock_acquire(&m->lock);
    m->owner_cpu_index = (int32_t)percpu_get()->cpu_index;
}

bool mutex_try_lock(mutex_t *m)
{
    if (!spinlock_try_acquire(&m->lock))
        return false;

    m->owner_cpu_index = (int32_t)percpu_get()->cpu_index;
    return true;
}

void mutex_unlock(mutex_t *m)
{
    int32_t calling_cpu = (int32_t)percpu_get()->cpu_index;

    if (m->owner_cpu_index != calling_cpu)
    {
        // Deliberately does NOT release the lock in this case -- see
        // Mutex.h's own header comment on why this specific
        // misuse (unlocking a mutex a different CPU holds) is exactly
        // what this type exists to catch rather than silently
        // allowing. Releasing anyway here would just turn a loud,
        // debuggable error into the exact silent double-ownership
        // corruption this check was written to prevent.
        klog_error_n("mutex_unlock: called by wrong CPU, owner was", (uint64_t)m->owner_cpu_index);
        return;
    }

    m->owner_cpu_index = MUTEX_NO_OWNER;
    spinlock_release(&m->lock);
}
