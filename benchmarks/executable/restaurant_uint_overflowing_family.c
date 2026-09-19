// C23 reference for the Restaurant UInt overflowing-family workload.
// Expected exit: 0
// Expected stdout:
// Overflowing family add 0/true,11/false; subtract 41/false,18446744073709551615/true; multiply 42/false,18446744073709551614/true; negate 0/false,18446744073709551615/true; power 9223372036854775808/false,0/true,1/true,1/false

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile uint64_t runtime_add_maximum = UINT64_MAX;
static volatile uint64_t runtime_add_increment = UINT64_C(1);
static volatile uint64_t runtime_add_ordinary = UINT64_C(10);
static volatile uint64_t runtime_subtract_left = UINT64_C(42);
static volatile uint64_t runtime_subtract_right = UINT64_C(1);
static volatile uint64_t runtime_underflow_left = UINT64_C(0);
static volatile uint64_t runtime_underflow_right = UINT64_C(1);
static volatile uint64_t runtime_multiply_left = UINT64_C(6);
static volatile uint64_t runtime_multiply_right = UINT64_C(7);
static volatile uint64_t runtime_overflow_left = UINT64_MAX;
static volatile uint64_t runtime_overflow_right = UINT64_C(2);
static volatile uint64_t runtime_negate_zero = UINT64_C(0);
static volatile uint64_t runtime_negate_one = UINT64_C(1);
static volatile uint64_t runtime_power_ordinary_base = UINT64_C(2);
static volatile uint64_t runtime_power_ordinary_exponent = UINT64_C(63);
static volatile uint64_t runtime_power_overflow_base = UINT64_C(2);
static volatile uint64_t runtime_power_overflow_exponent = UINT64_C(64);
static volatile uint64_t runtime_power_wrapped_base = UINT64_MAX;
static volatile uint64_t runtime_power_wrapped_exponent = UINT64_C(2);
static volatile uint64_t runtime_power_identity_base = UINT64_C(0);
static volatile uint64_t runtime_power_identity_exponent = UINT64_C(0);

typedef struct {
  uint64_t value;
  bool overflow;
} overflowing_result;

static overflowing_result overflowing_add_u64(uint64_t left,
                                               uint64_t right) {
  const uint64_t wrapped = left + right;
  return (overflowing_result){wrapped, wrapped < left};
}

static overflowing_result overflowing_subtract_u64(uint64_t left,
                                                    uint64_t right) {
  return (overflowing_result){left - right, left < right};
}

static overflowing_result overflowing_multiply_u64(uint64_t left,
                                                    uint64_t right) {
  const bool overflow = left != UINT64_C(0) && right > UINT64_MAX / left;
  return (overflowing_result){left * right, overflow};
}

static overflowing_result overflowing_negate_u64(uint64_t value) {
  return (overflowing_result){UINT64_C(0) - value, value != UINT64_C(0)};
}

static overflowing_result overflowing_power_u64(uint64_t base,
                                                 uint64_t exponent) {
  overflowing_result result = {UINT64_C(1), false};
  while (exponent != UINT64_C(0)) {
    if ((exponent & UINT64_C(1)) != UINT64_C(0)) {
      const overflowing_result product =
          overflowing_multiply_u64(result.value, base);
      result.value = product.value;
      result.overflow = result.overflow || product.overflow;
    }
    exponent >>= 1;
    if (exponent != UINT64_C(0)) {
      const overflowing_result square = overflowing_multiply_u64(base, base);
      base = square.value;
      result.overflow = result.overflow || square.overflow;
    }
  }
  return result;
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  const overflowing_result maximum_add = overflowing_add_u64(
      runtime_add_maximum, runtime_add_increment);
  const overflowing_result ordinary_add = overflowing_add_u64(
      runtime_add_ordinary, runtime_add_increment);
  const overflowing_result ordinary_subtract = overflowing_subtract_u64(
      runtime_subtract_left, runtime_subtract_right);
  const overflowing_result underflow = overflowing_subtract_u64(
      runtime_underflow_left, runtime_underflow_right);
  const overflowing_result ordinary_multiply = overflowing_multiply_u64(
      runtime_multiply_left, runtime_multiply_right);
  const overflowing_result overflow = overflowing_multiply_u64(
      runtime_overflow_left, runtime_overflow_right);
  const overflowing_result zero_negate = overflowing_negate_u64(
      runtime_negate_zero);
  const overflowing_result one_negate = overflowing_negate_u64(
      runtime_negate_one);
  const overflowing_result ordinary_power = overflowing_power_u64(
      runtime_power_ordinary_base, runtime_power_ordinary_exponent);
  const overflowing_result overflow_power = overflowing_power_u64(
      runtime_power_overflow_base, runtime_power_overflow_exponent);
  const overflowing_result wrapped_power = overflowing_power_u64(
      runtime_power_wrapped_base, runtime_power_wrapped_exponent);
  const overflowing_result identity_power = overflowing_power_u64(
      runtime_power_identity_base, runtime_power_identity_exponent);
  return printf("Overflowing family add %" PRIu64 "/%s,%" PRIu64 "/%s; "
                "subtract %" PRIu64 "/%s,%" PRIu64 "/%s; "
                "multiply %" PRIu64 "/%s,%" PRIu64 "/%s; "
                "negate %" PRIu64 "/%s,%" PRIu64 "/%s; "
                "power %" PRIu64 "/%s,%" PRIu64 "/%s,%" PRIu64 "/%s,%"
                PRIu64 "/%s\n",
                maximum_add.value,
                maximum_add.overflow ? "true" : "false",
                ordinary_add.value,
                ordinary_add.overflow ? "true" : "false",
                ordinary_subtract.value,
                ordinary_subtract.overflow ? "true" : "false",
                underflow.value, underflow.overflow ? "true" : "false",
                ordinary_multiply.value,
                ordinary_multiply.overflow ? "true" : "false",
                overflow.value, overflow.overflow ? "true" : "false",
                zero_negate.value, zero_negate.overflow ? "true" : "false",
                one_negate.value, one_negate.overflow ? "true" : "false",
                ordinary_power.value,
                ordinary_power.overflow ? "true" : "false",
                overflow_power.value,
                overflow_power.overflow ? "true" : "false",
                wrapped_power.value,
                wrapped_power.overflow ? "true" : "false",
                identity_power.value,
                identity_power.overflow ? "true" : "false") < 0
      ? 1
      : 0;
}
