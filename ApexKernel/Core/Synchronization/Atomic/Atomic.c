// ApexKernel/Core/Synchronization/Atomic/Atomic.c
// ApexOS -- Generic atomic operations (see Atomic.h)

#include "Core/Synchronization/Atomic/Atomic.h"

uint32_t atomic_add32(volatile uint32_t *ptr, uint32_t value)
{
    // XADD with the LOCK prefix: atomically does *ptr += value, and
    // leaves the value *ptr held BEFORE the add in `value` itself
    // (the instruction's own defined behavior) -- which is exactly
    // what this function returns. Unlike Spinlock.c's plain xchg
    // (Spinlock.h itself notes x86's xchg is implicitly locked, no
    // prefix needed), XADD needs the explicit "lock" prefix to be
    // atomic across CPUs -- without it this would be a plain
    // (non-atomic) add.
    __asm__ volatile("lock xadd %0, %1"
                     : "+r"(value), "+m"(*ptr)
                     :
                     : "memory");
    return value;
}

uint64_t atomic_add64(volatile uint64_t *ptr, uint64_t value)
{
    __asm__ volatile("lock xadd %0, %1"
                     : "+r"(value), "+m"(*ptr)
                     :
                     : "memory");
    return value;
}

uint32_t atomic_inc32(volatile uint32_t *ptr)
{
    // Returns the value AFTER incrementing (unlike atomic_add32's own
    // "before" semantics) -- the common case for a caller that just
    // wants to know the new count, not which specific former value it
    // claimed (see Atomic.h's own comment on atomic_add32 for when
    // that distinction matters).
    return atomic_add32(ptr, 1) + 1;
}

uint32_t atomic_dec32(volatile uint32_t *ptr)
{
    return atomic_add32(ptr, (uint32_t)-1) - 1;
}

uint64_t atomic_inc64(volatile uint64_t *ptr)
{
    return atomic_add64(ptr, 1) + 1;
}

uint64_t atomic_dec64(volatile uint64_t *ptr)
{
    return atomic_add64(ptr, (uint64_t)-1) - 1;
}

bool atomic_cas32(volatile uint32_t *ptr, uint32_t expected, uint32_t desired)
{
    // CMPXCHG's own defined behavior: compares *ptr against whatever
    // is in EAX (loaded from `expected` below) -- if equal, *ptr is
    // set to `desired` and ZF is set; if not equal, *ptr is left
    // completely untouched, EAX is loaded with *ptr's actual current
    // value instead, and ZF is cleared. The "=@ccz" output constraint
    // reads ZF directly into the bool this function returns, so no
    // separate branch/setz is needed in this inline asm itself.
    //
    // CRITICAL: named constraints ([ptr]/[desired]) are used here
    // rather than positional (%1/%2) ones -- an earlier version used
    // %1/%2, which let GCC allocate `desired` into %eax, silently
    // colliding with the separate "+a"(expected) constraint that also
    // claims %eax for `expected`. The result compiled and ran without
    // any warning, but every single call reported success (ZF set)
    // while leaving *ptr completely unchanged -- confirmed by an
    // isolated test where a semaphore's count never actually
    // decremented despite sem_try_wait() reporting a claimed unit on
    // every call. Named constraints force GCC to prove it can
    // allocate distinct registers for `expected` (%eax, fixed by
    // "+a") and `desired` (whatever GCC picks via "r", provably NOT
    // %eax since that register is already claimed) rather than
    // silently reusing one for both.
    uint8_t success;
    __asm__ volatile("lock cmpxchg %[desired], %[ptr]"
                     : "=@ccz"(success), [ptr] "+m"(*ptr), "+a"(expected)
                     : [desired] "r"(desired)
                     : "memory");
    return (bool)success;
}

bool atomic_cas64(volatile uint64_t *ptr, uint64_t expected, uint64_t desired)
{
    // Same named-constraint fix as atomic_cas32() above -- see that
    // function's own comment for the full explanation of the bug this
    // avoids.
    uint8_t success;
    __asm__ volatile("lock cmpxchg %[desired], %[ptr]"
                     : "=@ccz"(success), [ptr] "+m"(*ptr), "+a"(expected)
                     : [desired] "r"(desired)
                     : "memory");
    return (bool)success;
}

uint32_t atomic_exchange32(volatile uint32_t *ptr, uint32_t value)
{
    // Same instruction Spinlock.c's own atomic_xchg() uses (see that
    // file's comment: xchg is implicitly locked on x86, no explicit
    // "lock" prefix needed here unlike every other function in this
    // file).
    __asm__ volatile("xchg %0, %1"
                     : "=r"(value), "+m"(*ptr)
                     : "0"(value)
                     : "memory");
    return value;
}

uint64_t atomic_exchange64(volatile uint64_t *ptr, uint64_t value)
{
    __asm__ volatile("xchg %0, %1"
                     : "=r"(value), "+m"(*ptr)
                     : "0"(value)
                     : "memory");
    return value;
}
