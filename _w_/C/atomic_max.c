#include <stdatomic.h>

inline __uint128_t load_relaxed(_Atomic __uint128_t *obj)
{
  return atomic_load_explicit(obj, memory_order_relaxed);
}

inline _Bool cmpexch_weak_relaxed(_Atomic __uint128_t *obj,
                                  __uint128_t *expected,
                                  __uint128_t desired)
{
  return atomic_compare_exchange_weak_explicit(obj, expected, desired,
    memory_order_relaxed, memory_order_relaxed);
}

// #include <stdatomic.h>
// #include <stdint.h>
// #include <stdbool.h>
// #include <stddef.h>

// typedef struct _uint128_atomic {
//     volatile uint64_t low;
//     volatile uint64_t high;
// } uint128_atomic;


// bool
// cmpexch_weak_relaxed(
//     uint128_atomic *atomic,
//     uint128_atomic *expected,
//     uint128_atomic desired)
// {
//     bool matched;
//     uint128_atomic e = *expected;
//     asm volatile("lock cmpxchg16b %1"
//                  : "=@ccz"(matched), "+m"(*atomic), "+a"(e.low), "+d"(e.high)
//                  : "b"(desired.low), "c"(desired.high)
//                  : "cc");
//     if (!matched)
//         *expected = e;
//     return matched;
// }

// uint128_atomic
// load_relaxed1(uint128_atomic const *atomic)
// {
//     uint128_atomic ret = {0, 0};
//     asm volatile("lock cmpxchg16b %1"
//                  : "+A"(ret)
//                  : "m"(*atomic), "b"(0), "c"(0)
//                  : "cc");
//     return ret;
// }

// void
// store_relaxed1(uint128_atomic *atomic, uint128_atomic val)
// {
//     uint128_atomic old = *atomic;
//     while (!cmpexch_weak_relaxed(atomic, &old, val))
//         ;
// }