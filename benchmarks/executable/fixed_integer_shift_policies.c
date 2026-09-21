// C23 correctness reference for fixed-width named shift policies.
// Expected exit: 0
// Expected stdout:
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

static volatile uint64_t inputs[] = {
    UINT64_C(0x80), UINT64_C(0x80),
    UINT64_C(0x8000), UINT64_C(0x8000),
    UINT64_C(0x80000000), UINT64_C(0x80000000),
    UINT64_C(0x8000000000000000), UINT64_C(0x8000000000000000),
};

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

static int print_row(const char *name, uint64_t bits, unsigned width,
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
  const uint64_t edge_left =
      masked_left(UINT64_C(1), UINT64_C(65), 64U);
  const uint64_t edge_right =
      masked_right(UINT64_C(128), UINT64_C(65), 64U, false);
  const uint64_t logical = logical_right(UINT64_C(128), UINT64_C(1), 64U);
  if (logical == UINT64_MAX) return 1;
  return printf("u64 small-value %" PRIu64 "/%" PRIu64 "/%" PRIu64 "\n",
                edge_left, edge_right, logical) < 0;
}

int main(void) {
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
    if (print_row(rows[index].name, inputs[index], rows[index].width,
                  rows[index].is_signed) != 0)
      return 1;
  if (print_small_value_edges() != 0) return 1;
  return 0;
}
