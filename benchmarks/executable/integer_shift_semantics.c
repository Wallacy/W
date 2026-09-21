// C23 correctness reference for checked ordinary shifts and named fixed-width policies.
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
// i8 -128/0/-64/64
// u8 128/0/64/64
// i16 -32768/0/-16384/16384
// u16 32768/0/16384/16384
// i32 -2147483648/0/-1073741824/1073741824
// u32 2147483648/0/1073741824/1073741824
// i64 -9223372036854775808/0/-4611686018427387904/4611686018427387904
// u64 9223372036854775808/0/4611686018427387904/4611686018427387904
// u64 small-value 2/64/64

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

static volatile uint64_t policy_inputs[] = {
    UINT64_C(0x80), UINT64_C(0x80),
    UINT64_C(0x8000), UINT64_C(0x8000),
    UINT64_C(0x80000000), UINT64_C(0x80000000),
    UINT64_C(0x8000000000000000), UINT64_C(0x8000000000000000),
};

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

#define PRINT_CHECKED_SIGNED_ROW(label, type, width, inputs, minimum, maximum) \
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
    if (printf("%s %" PRId64 "/%" PRId64 "\n", label, right_result,       \
               left_result) < 0)                                               \
      return 1;                                                                \
  } while (0)

#define PRINT_CHECKED_UNSIGNED_ROW(label, type, width, inputs, maximum)        \
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
    if (printf("%s %" PRIu64 "/%" PRIu64 "\n", label, right_result,       \
               left_result) < 0)                                               \
      return 1;                                                                \
  } while (0)

static uint64_t width_mask(unsigned width) {
  return width == 64U ? UINT64_MAX : (UINT64_C(1) << width) - UINT64_C(1);
}

static uint64_t masked_left(uint64_t bits, uint64_t count, unsigned width) {
  const uint64_t mask = width_mask(width);
  const unsigned shift = (unsigned)(count & (uint64_t)(width - 1U));
  return ((bits & mask) << shift) & mask;
}

static uint64_t masked_right(uint64_t bits, uint64_t count, unsigned width,
                             bool is_signed) {
  const uint64_t mask = width_mask(width);
  const uint64_t value = bits & mask;
  const unsigned shift = (unsigned)(count & (uint64_t)(width - 1U));
  uint64_t result = value >> shift;
  if (is_signed && shift != 0U && (value & (UINT64_C(1) << (width - 1U))) != 0U)
    result |= mask ^ (mask >> shift);
  return result & mask;
}

static uint64_t logical_right(uint64_t bits, uint64_t count, unsigned width) {
  if (count >= width) return UINT64_MAX;
  return (bits & width_mask(width)) >> count;
}

static int64_t signed_value(uint64_t bits, unsigned width) {
  const uint64_t mask = width_mask(width);
  const uint64_t value = bits & mask;
  const uint64_t sign = UINT64_C(1) << (width - 1U);
  if ((value & sign) == 0U) return (int64_t)value;
  return -INT64_C(1) - (int64_t)(mask - value);
}

static int print_policy_row(const char *name, uint64_t bits, unsigned width,
                            bool is_signed) {
  const uint64_t identity = masked_left(bits, width, width);
  const uint64_t wrapped = masked_left(bits, (uint64_t)width + 1U, width);
  const uint64_t arithmetic =
      masked_right(bits, (uint64_t)width + 1U, width, is_signed);
  const uint64_t logical_zero = logical_right(bits, 0U, width);
  const uint64_t logical_one = logical_right(bits, 1U, width);
  const uint64_t logical_max = logical_right(bits, width - 1U, width);
  if (logical_zero != (bits & width_mask(width)) || logical_max != UINT64_C(1))
    return 1;
  if (is_signed)
    return printf("%s %" PRId64 "/%" PRId64 "/%" PRId64 "/%" PRId64 "\n",
                  name, signed_value(identity, width), signed_value(wrapped, width),
                  signed_value(arithmetic, width),
                  signed_value(logical_one, width)) < 0;
  return printf("%s %" PRIu64 "/%" PRIu64 "/%" PRIu64 "/%" PRIu64 "\n",
                name, identity, wrapped, arithmetic, logical_one) < 0;
}

static int print_small_value_edges(void) {
  const uint64_t edge_left = masked_left(UINT64_C(1), UINT64_C(65), 64U);
  const uint64_t edge_right = masked_right(UINT64_C(128), UINT64_C(65), 64U, false);
  const uint64_t logical = logical_right(UINT64_C(128), UINT64_C(1), 64U);
  if (logical == UINT64_MAX) return 1;
  return printf("u64 small-value %" PRIu64 "/%" PRIu64 "/%" PRIu64 "\n",
                edge_left, edge_right, logical) < 0;
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  const unsigned right_count = (unsigned)right_shift_amount;
  const unsigned left_count = (unsigned)left_shift_amount;
  PRINT_CHECKED_SIGNED_ROW("i8", int8_t, 8U, i8_inputs, INT8_MIN, INT8_MAX);
  PRINT_CHECKED_UNSIGNED_ROW("u8", uint8_t, 8U, u8_inputs, UINT8_MAX);
  PRINT_CHECKED_SIGNED_ROW("i16", int16_t, 16U, i16_inputs, INT16_MIN, INT16_MAX);
  PRINT_CHECKED_UNSIGNED_ROW("u16", uint16_t, 16U, u16_inputs, UINT16_MAX);
  PRINT_CHECKED_SIGNED_ROW("i32", int32_t, 32U, i32_inputs, INT32_MIN, INT32_MAX);
  PRINT_CHECKED_UNSIGNED_ROW("u32", uint32_t, 32U, u32_inputs, UINT32_MAX);
  PRINT_CHECKED_SIGNED_ROW("i64", int64_t, 64U, i64_inputs, INT64_MIN, INT64_MAX);
  PRINT_CHECKED_UNSIGNED_ROW("u64", uint64_t, 64U, u64_inputs, UINT64_MAX);
  PRINT_CHECKED_SIGNED_ROW("Int", intptr_t, 64U, int_inputs, INTPTR_MIN, INTPTR_MAX);
  PRINT_CHECKED_UNSIGNED_ROW("UInt", uintptr_t, 64U, uint_inputs, UINTPTR_MAX);

  const struct {
    const char *name;
    unsigned width;
    bool is_signed;
  } rows[] = {
      {"i8", 8U, true}, {"u8", 8U, false},
      {"i16", 16U, true}, {"u16", 16U, false},
      {"i32", 32U, true}, {"u32", 32U, false},
      {"i64", 64U, true}, {"u64", 64U, false},
  };
  for (size_t index = 0U; index < sizeof(rows) / sizeof(rows[0]); index += 1U)
    if (print_policy_row(rows[index].name, policy_inputs[index], rows[index].width,
                         rows[index].is_signed) != 0)
      return 1;
  if (print_small_value_edges() != 0) return 1;
  return 0;
}
