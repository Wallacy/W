// C23 reference for checked fixed-width integer arithmetic.
// Expected exit: 0
// Expected stdout:
// i8 -9/-15/-36; compound -22
// u8 43/37/120; compound 82
// i16 -970/-1030/-30000; compound -1944
// u16 1030/970/30000; compound 2056
// i32 -117000/-123000/-360000000; compound -234004
// u32 100300/99700/30000000; compound 200596
// i64 -600000/-1200000/-270000000000; compound -1200004
// u64 6000000000/4000000000/5000000000000000000; compound 11999999996
// Int -4000000000/-6000000000/-5000000000000000000; compound -8000000004
// UInt 9000000000/3000000000/18000000000000000000; compound 17999999996

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
        TYPE compound = runtime_left;                                          \
        compound += runtime_right;                                             \
        compound -= (TYPE)2;                                                   \
        compound *= (TYPE)2;                                                   \
        if (printf(LABEL " %lld/%lld/%lld; compound %lld\n",                \
                   (long long)sum, (long long)difference, (long long)product, \
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
        TYPE compound = runtime_left;                                            \
        compound += runtime_right;                                               \
        compound -= (TYPE)2;                                                     \
        compound *= (TYPE)2;                                                     \
        if (printf(LABEL " %llu/%llu/%llu; compound %llu\n",                  \
                   (unsigned long long)sum,                                     \
                   (unsigned long long)difference,                               \
                   (unsigned long long)product,                                  \
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
