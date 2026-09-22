// C23 reference for checked fixed-width integer arithmetic.
// Expected exit: 0
// Expected stdout:
// i8 -9/-15/-36; divrem -4/0; compound -2
// u8 43/37/120; divrem 13/1; compound 2
// i16 -970/-1030/-30000; divrem -33/-10; compound -12
// u16 1030/970/30000; divrem 33/10; compound 8
// i32 -117000/-123000/-360000000; divrem -40/0; compound -2
// u32 100300/99700/30000000; divrem 333/100; compound 98
// i64 -600000/-1200000/-270000000000; divrem -3/0; compound -2
// u64 6000000000/4000000000/5000000000000000000; divrem 5/0; compound 999999998
// Int -4000000000/-6000000000/-5000000000000000000; divrem -5/0; compound -2
// UInt 9000000000/3000000000/18000000000000000000; divrem 2/0; compound 2999999998

#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define REPORT_SIGNED(LABEL, TYPE, LEFT, RIGHT)                              \
    do {                                                                      \
        volatile TYPE runtime_left = (TYPE)(LEFT);                            \
        volatile TYPE runtime_right = (TYPE)(RIGHT);                          \
        const TYPE sum = (TYPE)(runtime_left + runtime_right);                 \
        const TYPE difference = (TYPE)(runtime_left - runtime_right);          \
        const TYPE product = (TYPE)(runtime_left * runtime_right);             \
        const TYPE quotient = (TYPE)(runtime_left / runtime_right);             \
        const TYPE remainder = (TYPE)(runtime_left % runtime_right);            \
        TYPE compound = runtime_left;                                          \
        compound += runtime_right;                                             \
        compound -= (TYPE)2;                                                   \
        compound *= (TYPE)2;                                                   \
        compound /= (TYPE)2;                                                   \
        compound %= runtime_right;                                            \
        if (printf(LABEL " %lld/%lld/%lld; divrem %lld/%lld; compound %lld\n", \
                   (long long)sum, (long long)difference, (long long)product, \
                   (long long)quotient, (long long)remainder,                \
                   (long long)compound) < 0)                                  \
            return 1;                                                         \
    } while (0)

#define REPORT_UNSIGNED(LABEL, TYPE, LEFT, RIGHT)                              \
    do {                                                                        \
        volatile TYPE runtime_left = (TYPE)(LEFT);                              \
        volatile TYPE runtime_right = (TYPE)(RIGHT);                            \
        const TYPE sum = (TYPE)(runtime_left + runtime_right);                   \
        const TYPE difference = (TYPE)(runtime_left - runtime_right);            \
        const TYPE product = (TYPE)(runtime_left * runtime_right);               \
        const TYPE quotient = (TYPE)(runtime_left / runtime_right);               \
        const TYPE remainder = (TYPE)(runtime_left % runtime_right);              \
        TYPE compound = runtime_left;                                            \
        compound += runtime_right;                                               \
        compound -= (TYPE)2;                                                     \
        compound *= (TYPE)2;                                                     \
        compound /= (TYPE)2;                                                     \
        compound %= runtime_right;                                              \
        if (printf(LABEL " %llu/%llu/%llu; divrem %llu/%llu; compound %llu\n", \
                   (unsigned long long)sum,                                     \
                   (unsigned long long)difference,                               \
                   (unsigned long long)product,                                  \
                   (unsigned long long)quotient,                                 \
                   (unsigned long long)remainder,                                \
                   (unsigned long long)compound) < 0)                            \
            return 1;                                                           \
    } while (0)

int main(void) {
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
    REPORT_SIGNED("i8", int8_t, -12, 3);
    REPORT_UNSIGNED("u8", uint8_t, 40u, 3u);
    REPORT_SIGNED("i16", int16_t, -1000, 30);
    REPORT_UNSIGNED("u16", uint16_t, 1000u, 30u);
    REPORT_SIGNED("i32", int32_t, -120000, 3000);
    REPORT_UNSIGNED("u32", uint32_t, 100000u, 300u);
    REPORT_SIGNED("i64", int64_t, -INT64_C(900000), INT64_C(300000));
    REPORT_UNSIGNED("u64", uint64_t, UINT64_C(5000000000),
                    UINT64_C(1000000000));
    REPORT_SIGNED("Int", int64_t, -INT64_C(5000000000),
                  INT64_C(1000000000));
    REPORT_UNSIGNED("UInt", uint64_t, UINT64_C(6000000000),
                    UINT64_C(3000000000));
    return 0;
}
