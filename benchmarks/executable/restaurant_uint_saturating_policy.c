// C23 reference for the Restaurant UInt saturating-policy workload.
// Expected exit: 0
// Expected stdout:
// Saturating policy add 18446744073709551615/11; subtract 0/10; multiply 18446744073709551615/42; negate 0/0; power 8/18446744073709551615/1

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile uint64_t runtime_add_maximum = UINT64_MAX;
static volatile uint64_t runtime_add_increment = UINT64_C(1);
static volatile uint64_t runtime_add_ordinary = UINT64_C(10);
static volatile uint64_t runtime_subtract_zero = UINT64_C(0);
static volatile uint64_t runtime_subtract_amount = UINT64_C(1);
static volatile uint64_t runtime_subtract_ordinary = UINT64_C(11);
static volatile uint64_t runtime_multiply_maximum = UINT64_MAX;
static volatile uint64_t runtime_multiply_overflow_factor = UINT64_C(2);
static volatile uint64_t runtime_multiply_ordinary = UINT64_C(6);
static volatile uint64_t runtime_multiply_ordinary_factor = UINT64_C(7);
static volatile uint64_t runtime_negate_zero = UINT64_C(0);
static volatile uint64_t runtime_negate_maximum = UINT64_MAX;
static volatile uint64_t runtime_ordinary_base = UINT64_C(2);
static volatile uint64_t runtime_ordinary_exponent = UINT64_C(3);
static volatile uint64_t runtime_clamped_base = UINT64_C(2);
static volatile uint64_t runtime_clamped_exponent = UINT64_C(64);
static volatile uint64_t runtime_identity_base = UINT64_C(0);
static volatile uint64_t runtime_identity_exponent = UINT64_C(0);

static uint64_t saturating_add_u64(uint64_t left, uint64_t right) {
  if (left > UINT64_MAX - right) return UINT64_MAX;
  return left + right;
}

static uint64_t saturating_subtract_u64(uint64_t value, uint64_t amount) {
  if (value < amount) return UINT64_C(0);
  return value - amount;
}

static uint64_t saturating_negate_u64(uint64_t value) {
  const uint64_t wrapped = UINT64_C(0) - value;
  return wrapped > UINT64_C(0) ? UINT64_C(0) : wrapped;
}

static uint64_t saturating_multiply_u64(uint64_t left, uint64_t right) {
  if (left != UINT64_C(0) && right > UINT64_MAX / left) return UINT64_MAX;
  return left * right;
}

static uint64_t saturating_power_u64(uint64_t base, uint64_t exponent) {
  uint64_t result = UINT64_C(1);
  while (exponent != UINT64_C(0)) {
    if ((exponent & UINT64_C(1)) != UINT64_C(0))
      result = saturating_multiply_u64(result, base);
    exponent >>= 1;
    if (exponent != UINT64_C(0))
      base = saturating_multiply_u64(base, base);
  }
  return result;
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  const uint64_t maximum_add =
      saturating_add_u64(runtime_add_maximum, runtime_add_increment);
  const uint64_t ordinary_add =
      saturating_add_u64(runtime_add_ordinary, runtime_add_increment);
  const uint64_t underflow_subtract = saturating_subtract_u64(
      runtime_subtract_zero, runtime_subtract_amount);
  const uint64_t ordinary_subtract = saturating_subtract_u64(
      runtime_subtract_ordinary, runtime_subtract_amount);
  const uint64_t overflow_multiply = saturating_multiply_u64(
      runtime_multiply_maximum, runtime_multiply_overflow_factor);
  const uint64_t ordinary_multiply = saturating_multiply_u64(
      runtime_multiply_ordinary, runtime_multiply_ordinary_factor);
  const uint64_t neg_zero = saturating_negate_u64(runtime_negate_zero);
  const uint64_t neg_maximum = saturating_negate_u64(runtime_negate_maximum);
  const uint64_t ordinary = saturating_power_u64(
      runtime_ordinary_base, runtime_ordinary_exponent);
  const uint64_t clamped = saturating_power_u64(
      runtime_clamped_base, runtime_clamped_exponent);
  const uint64_t identity = saturating_power_u64(
      runtime_identity_base, runtime_identity_exponent);
  return printf("Saturating policy add %" PRIu64 "/%" PRIu64 "; "
                "subtract %" PRIu64 "/%" PRIu64 "; "
                "multiply %" PRIu64 "/%" PRIu64 "; "
                "negate %" PRIu64 "/%" PRIu64 "; "
                "power %" PRIu64 "/%" PRIu64 "/%" PRIu64 "\n",
                maximum_add, ordinary_add, underflow_subtract,
                ordinary_subtract, overflow_multiply, ordinary_multiply,
                neg_zero, neg_maximum, ordinary, clamped, identity) < 0
      ? 1
      : 0;
}
