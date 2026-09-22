#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

typedef struct {
  bool ok;
  uint64_t value;
} checked_u64;

static volatile uint64_t runtime_value = UINT64_C(4611686018427387904);
static volatile uint64_t runtime_add = UINT64_C(3);
static volatile uint64_t runtime_subtract = UINT64_C(1);
static volatile uint64_t runtime_multiply = UINT64_C(2);
static volatile uint64_t runtime_divide = UINT64_C(2);
static volatile uint64_t runtime_remainder = UINT64_MAX;
static volatile uint64_t runtime_power = UINT64_C(1);
static volatile uint64_t runtime_shift_left = UINT64_C(1);
static volatile uint64_t runtime_shift_right = UINT64_C(1);
static volatile uint64_t runtime_and = UINT64_C(255);
static volatile uint64_t runtime_xor = UINT64_C(85);
static volatile uint64_t runtime_or = UINT64_C(10);

static checked_u64 checked_add_u64(uint64_t left, uint64_t right) {
  if (left > UINT64_MAX - right) return (checked_u64){false, 0};
  return (checked_u64){true, left + right};
}

static checked_u64 checked_subtract_u64(uint64_t left, uint64_t right) {
  if (left < right) return (checked_u64){false, 0};
  return (checked_u64){true, left - right};
}

static checked_u64 checked_multiply_u64(uint64_t left, uint64_t right) {
  if (left != 0 && right > UINT64_MAX / left)
    return (checked_u64){false, 0};
  return (checked_u64){true, left * right};
}

static checked_u64 checked_divide_u64(uint64_t left, uint64_t right) {
  if (right == 0) return (checked_u64){false, 0};
  return (checked_u64){true, left / right};
}

static checked_u64 checked_remainder_u64(uint64_t left, uint64_t right) {
  if (right == 0) return (checked_u64){false, 0};
  return (checked_u64){true, left % right};
}

static checked_u64 checked_power_u64(uint64_t base, uint64_t exponent) {
  uint64_t result = UINT64_C(1);
  while (exponent != 0) {
    if ((exponent & UINT64_C(1)) != 0) {
      checked_u64 product = checked_multiply_u64(result, base);
      if (!product.ok) return product;
      result = product.value;
    }
    exponent >>= 1;
    if (exponent != 0) {
      checked_u64 square = checked_multiply_u64(base, base);
      if (!square.ok) return square;
      base = square.value;
    }
  }
  return (checked_u64){true, result};
}

static checked_u64 checked_shift_left_u64(uint64_t value, uint64_t count) {
  if (count >= 64 || (count != 0 && (value >> (64 - count)) != 0))
    return (checked_u64){false, 0};
  return (checked_u64){true, value << count};
}

static checked_u64 checked_shift_right_u64(uint64_t value, uint64_t count) {
  if (count >= 64) return (checked_u64){false, 0};
  return (checked_u64){true, value >> count};
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  uint64_t value = runtime_value;
  checked_u64 checked;
  checked = checked_add_u64(value, runtime_add);
  if (!checked.ok) return 1;
  value = checked.value;
  const uint64_t after_add = value;
  checked = checked_subtract_u64(value, runtime_subtract);
  if (!checked.ok) return 1;
  value = checked.value;
  const uint64_t after_subtract = value;
  checked = checked_multiply_u64(value, runtime_multiply);
  if (!checked.ok) return 1;
  value = checked.value;
  const uint64_t after_multiply = value;
  checked = checked_divide_u64(value, runtime_divide);
  if (!checked.ok) return 1;
  value = checked.value;
  const uint64_t after_divide = value;
  checked = checked_remainder_u64(value, runtime_remainder);
  if (!checked.ok) return 1;
  value = checked.value;
  const uint64_t after_remainder = value;
  checked = checked_power_u64(value, runtime_power);
  if (!checked.ok) return 1;
  value = checked.value;
  const uint64_t after_power = value;
  checked = checked_shift_left_u64(value, runtime_shift_left);
  if (!checked.ok) return 1;
  value = checked.value;
  const uint64_t after_shift_left = value;
  checked = checked_shift_right_u64(value, runtime_shift_right);
  if (!checked.ok) return 1;
  value = checked.value;
  const uint64_t after_shift_right = value;
  value &= runtime_and;
  const uint64_t after_and = value;
  value ^= runtime_xor;
  const uint64_t after_xor = value;
  value |= runtime_or;
  return printf("UInt compound %" PRIu64 "/%" PRIu64 "/%" PRIu64
                "/%" PRIu64 "/%" PRIu64 "/%" PRIu64 "/%" PRIu64
                "/%" PRIu64 "/%" PRIu64 "/%" PRIu64 "/%" PRIu64 "\n",
                after_add, after_subtract, after_multiply, after_divide,
                after_remainder, after_power, after_shift_left,
                after_shift_right, after_and, after_xor, value) < 0
             ? 1
             : 0;
}
