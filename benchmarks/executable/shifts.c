// C23 correctness reference for checked ordinary integer shifts.
// Expected exit: 0
// Expected stdout:
// i8 -16/-128
// u8 32/128
// i16 -4096/-32768
// u16 8192/32768
// i32 -268435456/-2147483648
// u32 536870912/2147483648
// i64 -1152921504606846976/-9223372036854775808
// u64 2305843009213693952/9223372036854775808
// Int -1152921504606846976/-9223372036854775808
// UInt 2305843009213693952/9223372036854775808

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile int8_t i8_inputs[] = {-INT8_C(64), -INT8_C(64)};
static volatile uint8_t u8_inputs[] = {UINT8_C(128), UINT8_C(64)};
static volatile int16_t i16_inputs[] = {-INT16_C(16384), -INT16_C(16384)};
static volatile uint16_t u16_inputs[] = {UINT16_C(32768), UINT16_C(16384)};
static volatile int32_t i32_inputs[] = {-INT32_C(1073741824), -INT32_C(1073741824)};
static volatile uint32_t u32_inputs[] = {UINT32_C(2147483648), UINT32_C(1073741824)};
static volatile int64_t i64_inputs[] = {
    -INT64_C(4611686018427387904), -INT64_C(4611686018427387904),
};
static volatile uint64_t u64_inputs[] = {
    UINT64_C(9223372036854775808), UINT64_C(4611686018427387904),
};
static volatile intptr_t int_inputs[] = {
    -INT64_C(4611686018427387904), -INT64_C(4611686018427387904),
};
static volatile uintptr_t uint_inputs[] = {
    UINT64_C(9223372036854775808), UINT64_C(4611686018427387904),
};
static volatile uint32_t right_shift_amount = UINT32_C(2);
static volatile uint32_t left_shift_amount = UINT32_C(1);

_Static_assert(sizeof(intptr_t) == sizeof(int64_t), "Int must be 64-bit");
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "UInt must be 64-bit");

// C does not define left-shifting a negative signed value. Compute the
// mathematical result through checked multiplication for this safe witness.
static bool checked_left_signed(int64_t value, unsigned amount, unsigned width,
                                int64_t minimum, int64_t maximum,
                                int64_t *result) {
  if (amount >= width || amount != 1U) return false;
  const int64_t factor = INT64_C(1) << amount;
  if (value > maximum / factor || value < minimum / factor) return false;
  *result = value * factor;
  return true;
}

static bool checked_left_unsigned(uint64_t value, unsigned amount,
                                  unsigned width, uint64_t maximum,
                                  uint64_t *result) {
  if (amount >= width || amount >= 64U) return false;
  const uint64_t factor = UINT64_C(1) << amount;
  if (value > maximum / factor) return false;
  *result = value * factor;
  return true;
}

// Spell out arithmetic right shift so negative signed behavior is portable.
static bool arithmetic_right_signed(int64_t value, unsigned amount,
                                    unsigned width, int64_t *result) {
  if (amount >= width || width > 64U) return false;
  if (amount == 0U) {
    *result = value;
    return true;
  }
  if (amount == 63U) {
    *result = value < 0 ? -INT64_C(1) : INT64_C(0);
    return true;
  }
  const int64_t divisor = INT64_C(1) << amount;
  *result = value >= 0
                ? value / divisor
                : -INT64_C(1) - ((-INT64_C(1) - value) / divisor);
  return true;
}

#define PRINT_SIGNED_ROW(label, type, width, inputs, minimum, maximum)         \
  do {                                                                         \
    const type right_value = (inputs)[0];                                      \
    const type left_value = (inputs)[1];                                       \
    int64_t right_result;                                                      \
    int64_t left_result;                                                       \
    if (!arithmetic_right_signed((int64_t)right_value, right_count, width,     \
                                 &right_result))                               \
      return 1;                                                                \
    if (!checked_left_signed((int64_t)left_value, left_count, width,           \
                             (int64_t)(minimum), (int64_t)(maximum),           \
                             &left_result))                                    \
      return 1;                                                                \
    if (printf("%s %" PRId64 "/%" PRId64 "\n", label, right_result,         \
               left_result) < 0)                                               \
      return 1;                                                                \
  } while (0)

#define PRINT_UNSIGNED_ROW(label, type, width, inputs, maximum)                \
  do {                                                                         \
    const type right_value = (inputs)[0];                                      \
    const type left_value = (inputs)[1];                                       \
    if (right_count >= width) return 1;                                        \
    const uint64_t right_result =                                              \
        ((uint64_t)right_value) >> right_count;                                \
    uint64_t left_result;                                                      \
    if (!checked_left_unsigned((uint64_t)left_value, left_count, width,        \
                               (uint64_t)(maximum), &left_result))             \
      return 1;                                                                \
    if (printf("%s %" PRIu64 "/%" PRIu64 "\n", label, right_result,         \
               left_result) < 0)                                               \
      return 1;                                                                \
  } while (0)

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  const unsigned right_count = (unsigned)right_shift_amount;
  const unsigned left_count = (unsigned)left_shift_amount;
  PRINT_SIGNED_ROW("i8", int8_t, 8U, i8_inputs, INT8_MIN, INT8_MAX);
  PRINT_UNSIGNED_ROW("u8", uint8_t, 8U, u8_inputs, UINT8_MAX);
  PRINT_SIGNED_ROW("i16", int16_t, 16U, i16_inputs, INT16_MIN, INT16_MAX);
  PRINT_UNSIGNED_ROW("u16", uint16_t, 16U, u16_inputs, UINT16_MAX);
  PRINT_SIGNED_ROW("i32", int32_t, 32U, i32_inputs, INT32_MIN, INT32_MAX);
  PRINT_UNSIGNED_ROW("u32", uint32_t, 32U, u32_inputs, UINT32_MAX);
  PRINT_SIGNED_ROW("i64", int64_t, 64U, i64_inputs, INT64_MIN, INT64_MAX);
  PRINT_UNSIGNED_ROW("u64", uint64_t, 64U, u64_inputs, UINT64_MAX);
  PRINT_SIGNED_ROW("Int", intptr_t, 64U, int_inputs, INTPTR_MIN, INTPTR_MAX);
  PRINT_UNSIGNED_ROW("UInt", uintptr_t, 64U, uint_inputs, UINTPTR_MAX);
  return 0;
}
