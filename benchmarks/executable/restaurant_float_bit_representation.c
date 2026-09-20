// C23 correctness reference for f32/f64 bit-preserving round trips.
// Expected exit: 0
// Expected stdout:
// Float bits f32 2147483648/2139095040/2143363909 f64 9223372036854775808/9218868437227405312/9221140253039434428

#include <float.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

_Static_assert(sizeof(float) == sizeof(uint32_t), "binary32 requires 32-bit float storage");
_Static_assert(sizeof(double) == sizeof(uint64_t), "binary64 requires 64-bit double storage");
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128,
               "binary32 IEEE-style parameters required");
_Static_assert(DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024,
               "binary64 IEEE-style parameters required");

static volatile uint32_t f32_negative_zero_bits = UINT32_C(0x80000000);
static volatile uint32_t f32_infinity_bits = UINT32_C(0x7f800000);
static volatile uint32_t f32_nan_bits = UINT32_C(0x7fc12345);
static volatile uint64_t f64_negative_zero_bits = UINT64_C(0x8000000000000000);
static volatile uint64_t f64_infinity_bits = UINT64_C(0x7ff0000000000000);
static volatile uint64_t f64_nan_bits = UINT64_C(0x7ff8123456789abc);

static uint32_t f32_roundtrip_bits(uint32_t bits) {
    float value;
    uint32_t result;
    memcpy(&value, &bits, sizeof(value));
    memcpy(&result, &value, sizeof(result));
    return result;
}

static uint64_t f64_roundtrip_bits(uint64_t bits) {
    double value;
    uint64_t result;
    memcpy(&value, &bits, sizeof(value));
    memcpy(&result, &value, sizeof(result));
    return result;
}

int main(void) {
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
    if (printf("Float bits f32 %" PRIu32 "/%" PRIu32 "/%" PRIu32
               " f64 %" PRIu64 "/%" PRIu64 "/%" PRIu64 "\n",
               f32_roundtrip_bits(f32_negative_zero_bits),
               f32_roundtrip_bits(f32_infinity_bits),
               f32_roundtrip_bits(f32_nan_bits),
               f64_roundtrip_bits(f64_negative_zero_bits),
               f64_roundtrip_bits(f64_infinity_bits),
               f64_roundtrip_bits(f64_nan_bits)) < 0)
        return 1;
    return 0;
}
