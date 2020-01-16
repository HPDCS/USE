#include "atomic_16byte.h"

bool cmpexch_weak_relaxed(uint128_atomic *atomic,uint128_atomic *expected,uint128_atomic desired)
{
    bool matched;
    uint128_atomic e = *expected;
    asm volatile("lock cmpxchg16b %1"
                 : "=@ccz"(matched), "+m"(*atomic), "+a"(e.low), "+d"(e.high)
                 : "b"(desired.low), "c"(desired.high)
                 : "cc");
    if (!matched)
        *expected = e;
    return matched;
}

uint128_atomic load_relaxed(uint128_atomic const *atomic)
{
    uint128_atomic ret = {0, 0};
    asm volatile("lock cmpxchg16b %1"
                 : "+A"(ret)
                 : "m"(*atomic), "b"(0), "c"(0)
                 : "cc");
    return ret;
}

void store_relaxed(uint128_atomic *atomic, uint128_atomic val)
{
    uint128_atomic old = *atomic;
    while (!cmpexch_weak_relaxed(atomic, &old, val))
        ;
}
