// C23 correctness reference for the fixed-width W bit-primitives family.
// Expected exit: 0
// Expected stdout:
// i8 3/5/1/1 74/127 82 -92/41
// u8 4/4/0/1 105 150 45/75
// i16 5/11/3/2 11336/32767 13330 9320/2330
// u16 8/8/0/0 54673 43913 4951/50389
// i32 13/19/3/3 510274632/2147483647 2018915346 610839792/152709948
// u32 20/12/0/0 4155757969 4023233417 324508639/3302352631
// i64 30/34/7/1 8553414939923104896/9223372036854775807 7984226321029210881 163971058432973532/40992764608243383
// u64 32/32/0/4 597899502893742975 1167088121787636990 18282773015276577825/9182379272246532360

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile int8_t runtime_i8 = INT8_C(0x52);
static volatile uint8_t runtime_u8 = UINT8_C(0x96);
static volatile int16_t runtime_i16 = INT16_C(0x1234);
static volatile uint16_t runtime_u16 = UINT16_C(0x89ab);
static volatile int32_t runtime_i32 = INT32_C(0x12345678);
static volatile uint32_t runtime_u32 = UINT32_C(0x89abcdef);
static volatile int64_t runtime_i64 = INT64_C(0x0123456789abcd6e);
static volatile uint64_t runtime_u64 = UINT64_C(0xfedcba9876543210);

static volatile int8_t zero_i8 = INT8_C(0);
static volatile int8_t negative_i8 = -INT8_C(2);
static volatile uint8_t zero_u8 = UINT8_C(0);
static volatile int16_t zero_i16 = INT16_C(0);
static volatile int16_t negative_i16 = -INT16_C(2);
static volatile uint16_t zero_u16 = UINT16_C(0);
static volatile int32_t zero_i32 = INT32_C(0);
static volatile int32_t negative_i32 = -INT32_C(2);
static volatile uint32_t zero_u32 = UINT32_C(0);
static volatile int64_t zero_i64 = INT64_C(0);
static volatile int64_t negative_i64 = -INT64_C(2);
static volatile uint64_t zero_u64 = UINT64_C(0);

static volatile uint64_t rotations8[] = {UINT64_C(0), UINT64_C(8), UINT64_C(9)};
static volatile uint64_t rotations16[] = {UINT64_C(0), UINT64_C(16), UINT64_C(17)};
static volatile uint64_t rotations32[] = {UINT64_C(0), UINT64_C(32), UINT64_C(33)};
static volatile uint64_t rotations64[] = {UINT64_C(0), UINT64_C(64), UINT64_C(65)};

static uint64_t width_mask(unsigned width) {
  return width == 64 ? UINT64_MAX : (UINT64_C(1) << width) - UINT64_C(1);
}

static uint64_t count_ones_width(uint64_t value, unsigned width) {
  uint64_t count = UINT64_C(0);
  for (unsigned bit = 0; bit < width; bit += 1)
    count += (value >> bit) & UINT64_C(1);
  return count;
}

static uint64_t count_leading_zeros_width(uint64_t value, unsigned width) {
  uint64_t count = UINT64_C(0);
  for (unsigned bit = width; bit != 0; bit -= 1) {
    if (((value >> (bit - 1)) & UINT64_C(1)) != UINT64_C(0)) break;
    count += UINT64_C(1);
  }
  return count;
}

static uint64_t count_trailing_zeros_width(uint64_t value, unsigned width) {
  uint64_t count = UINT64_C(0);
  for (unsigned bit = 0; bit < width; bit += 1) {
    if (((value >> bit) & UINT64_C(1)) != UINT64_C(0)) break;
    count += UINT64_C(1);
  }
  return count;
}

static uint64_t reverse_bits_width(uint64_t value, unsigned width) {
  uint64_t reversed = UINT64_C(0);
  for (unsigned bit = 0; bit < width; bit += 1)
    reversed = (reversed << 1) | ((value >> bit) & UINT64_C(1));
  return reversed;
}

static uint64_t reverse_bytes_width(uint64_t value, unsigned width) {
  uint64_t reversed = UINT64_C(0);
  for (unsigned bit = 0; bit < width; bit += 8)
    reversed = (reversed << 8) | ((value >> bit) & UINT64_C(0xff));
  return reversed;
}

static intmax_t signed_bits_value(uint64_t value, unsigned width) {
  if (width == 64) {
    if (value <= (uint64_t)INT64_MAX) return (intmax_t)value;
    return -INTMAX_C(1) - (intmax_t)(UINT64_MAX - value);
  }
  const uint64_t sign = UINT64_C(1) << (width - 1);
  if ((value & sign) == 0) return (intmax_t)value;
  return (intmax_t)value - (intmax_t)(UINT64_C(1) << width);
}

static uint64_t rotate_left_width(uint64_t value, uint64_t count,
                                  unsigned width) {
  const uint64_t mask = width_mask(width);
  const unsigned shift = (unsigned)(count % width);
  value &= mask;
  if (shift == 0) return value;
  return ((value << shift) | (value >> (width - shift))) & mask;
}

static uint64_t rotate_right_width(uint64_t value, uint64_t count,
                                   unsigned width) {
  const uint64_t mask = width_mask(width);
  const unsigned shift = (unsigned)(count % width);
  value &= mask;
  if (shift == 0) return value;
  return ((value >> shift) | (value << (width - shift))) & mask;
}

static int print_signed_row(const char *label, intmax_t input,
                            intmax_t zero_input, intmax_t negative_input,
                            const volatile uint64_t *rotations,
                            unsigned width) {
  const uint64_t mask = width_mask(width);
  const uint64_t value = (uint64_t)input & mask;
  const uint64_t zero = (uint64_t)zero_input & mask;
  const uint64_t negative = (uint64_t)negative_input & mask;
  const uint64_t count = count_ones_width(value, width);
  const uint64_t zeros = width - count;
  const uint64_t leading = count_leading_zeros_width(value, width);
  const uint64_t trailing = count_trailing_zeros_width(value, width);
  const uint64_t zero_leading = count_leading_zeros_width(zero, width);
  const uint64_t zero_trailing = count_trailing_zeros_width(zero, width);
  const intmax_t bits = signed_bits_value(reverse_bits_width(value, width), width);
  const intmax_t negative_bits =
      signed_bits_value(reverse_bits_width(negative, width), width);
  const intmax_t bytes = signed_bits_value(reverse_bytes_width(value, width), width);
  const intmax_t left0 =
      signed_bits_value(rotate_left_width(value, rotations[0], width), width);
  const intmax_t left_width =
      signed_bits_value(rotate_left_width(value, rotations[1], width), width);
  const intmax_t left_width_plus_one =
      signed_bits_value(rotate_left_width(value, rotations[2], width), width);
  const intmax_t right0 =
      signed_bits_value(rotate_right_width(value, rotations[0], width), width);
  const intmax_t right_width =
      signed_bits_value(rotate_right_width(value, rotations[1], width), width);
  const intmax_t right_width_plus_one =
      signed_bits_value(rotate_right_width(value, rotations[2], width), width);
  if (zero_leading != width || zero_trailing != width || left0 != input ||
      left_width != input || right0 != input || right_width != input)
    return 1;
  return printf(
             "%s %" PRIuMAX "/%" PRIuMAX "/%" PRIuMAX "/%" PRIuMAX
             " %" PRIdMAX "/%" PRIdMAX " %" PRIdMAX " %" PRIdMAX
             "/%" PRIdMAX "\n",
             label, (uintmax_t)count, (uintmax_t)zeros, (uintmax_t)leading,
             (uintmax_t)trailing, bits, negative_bits, bytes,
             left_width_plus_one, right_width_plus_one) < 0
             ? 1
             : 0;
}

static int print_unsigned_row(const char *label, uint64_t input,
                              uint64_t zero_input,
                              const volatile uint64_t *rotations,
                              unsigned width) {
  const uint64_t mask = width_mask(width);
  const uint64_t value = input & mask;
  const uint64_t zero = zero_input & mask;
  const uint64_t count = count_ones_width(value, width);
  const uint64_t zeros = width - count;
  const uint64_t leading = count_leading_zeros_width(value, width);
  const uint64_t trailing = count_trailing_zeros_width(value, width);
  const uint64_t zero_leading = count_leading_zeros_width(zero, width);
  const uint64_t zero_trailing = count_trailing_zeros_width(zero, width);
  const uint64_t bits = reverse_bits_width(value, width);
  const uint64_t bytes = reverse_bytes_width(value, width);
  const uint64_t left0 = rotate_left_width(value, rotations[0], width);
  const uint64_t left_width = rotate_left_width(value, rotations[1], width);
  const uint64_t left_width_plus_one = rotate_left_width(value, rotations[2], width);
  const uint64_t right0 = rotate_right_width(value, rotations[0], width);
  const uint64_t right_width = rotate_right_width(value, rotations[1], width);
  const uint64_t right_width_plus_one = rotate_right_width(value, rotations[2], width);
  if (zero_leading != width || zero_trailing != width || left0 != value ||
      left_width != value || right0 != value || right_width != value)
    return 1;
  return printf(
             "%s %" PRIuMAX "/%" PRIuMAX "/%" PRIuMAX "/%" PRIuMAX
             " %" PRIuMAX " %" PRIuMAX " %" PRIuMAX "/%" PRIuMAX
             "\n",
             label, (uintmax_t)count, (uintmax_t)zeros, (uintmax_t)leading,
             (uintmax_t)trailing, (uintmax_t)bits, (uintmax_t)bytes,
             (uintmax_t)left_width_plus_one,
             (uintmax_t)right_width_plus_one) < 0
             ? 1
             : 0;
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  if (print_signed_row("i8", runtime_i8, zero_i8, negative_i8,
                       rotations8, 8) != 0) return 1;
  if (print_unsigned_row("u8", runtime_u8, zero_u8, rotations8, 8) != 0)
    return 1;
  if (print_signed_row("i16", runtime_i16, zero_i16, negative_i16,
                       rotations16, 16) != 0) return 1;
  if (print_unsigned_row("u16", runtime_u16, zero_u16, rotations16, 16) != 0)
    return 1;
  if (print_signed_row("i32", runtime_i32, zero_i32, negative_i32,
                       rotations32, 32) != 0) return 1;
  if (print_unsigned_row("u32", runtime_u32, zero_u32, rotations32, 32) != 0)
    return 1;
  if (print_signed_row("i64", runtime_i64, zero_i64, negative_i64,
                       rotations64, 64) != 0) return 1;
  if (print_unsigned_row("u64", runtime_u64, zero_u64, rotations64, 64) != 0)
    return 1;
  return 0;
}
