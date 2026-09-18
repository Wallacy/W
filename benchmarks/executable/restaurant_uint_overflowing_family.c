// C23 reference for the Restaurant UInt overflowing-family workload.

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

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

typedef struct {
  uint64_t value;
  bool overflow;
} overflowing_result;

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

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
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
  return printf("Overflowing family %" PRIu64 "/%s/%" PRIu64 "/%s/%" PRIu64
                "/%s/%" PRIu64 "/%s/%" PRIu64 "/%s/%" PRIu64 "/%s\n",
                ordinary_subtract.value,
                ordinary_subtract.overflow ? "true" : "false",
                underflow.value, underflow.overflow ? "true" : "false",
                ordinary_multiply.value,
                ordinary_multiply.overflow ? "true" : "false",
                overflow.value, overflow.overflow ? "true" : "false",
                zero_negate.value, zero_negate.overflow ? "true" : "false",
                one_negate.value, one_negate.overflow ? "true" : "false") < 0
      ? 1
      : 0;
}
