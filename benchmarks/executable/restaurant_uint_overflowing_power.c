// C23 reference for the Restaurant UInt overflowing-power workload.

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile uint64_t runtime_ordinary_base = UINT64_C(2);
static volatile uint64_t runtime_ordinary_exponent = UINT64_C(63);
static volatile uint64_t runtime_overflow_base = UINT64_C(2);
static volatile uint64_t runtime_overflow_exponent = UINT64_C(64);
static volatile uint64_t runtime_wrapped_base = UINT64_MAX;
static volatile uint64_t runtime_wrapped_exponent = UINT64_C(2);
static volatile uint64_t runtime_identity_base = UINT64_C(0);
static volatile uint64_t runtime_identity_exponent = UINT64_C(0);

typedef struct {
  uint64_t value;
  bool overflow;
} overflowing_power_result;

static overflowing_power_result overflowing_multiply_u64(uint64_t left,
                                                          uint64_t right) {
  const uint64_t wrapped = left * right;
  const bool overflow = left != UINT64_C(0) && right > UINT64_MAX / left;
  return (overflowing_power_result){wrapped, overflow};
}

static overflowing_power_result overflowing_power_u64(uint64_t base,
                                                      uint64_t exponent) {
  overflowing_power_result result = {UINT64_C(1), false};
  while (exponent != UINT64_C(0)) {
    if ((exponent & UINT64_C(1)) != UINT64_C(0)) {
      const overflowing_power_result product =
          overflowing_multiply_u64(result.value, base);
      result.value = product.value;
      result.overflow = result.overflow || product.overflow;
    }
    exponent >>= 1;
    if (exponent != UINT64_C(0)) {
      const overflowing_power_result square =
          overflowing_multiply_u64(base, base);
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
  const overflowing_power_result ordinary = overflowing_power_u64(
      runtime_ordinary_base, runtime_ordinary_exponent);
  const overflowing_power_result overflow = overflowing_power_u64(
      runtime_overflow_base, runtime_overflow_exponent);
  const overflowing_power_result wrapped = overflowing_power_u64(
      runtime_wrapped_base, runtime_wrapped_exponent);
  const overflowing_power_result identity = overflowing_power_u64(
      runtime_identity_base, runtime_identity_exponent);
  return printf("Overflowing power %" PRIu64 "/%s; %" PRIu64 "/%s; %" PRIu64
                "/%s; %" PRIu64 "/%s\n",
                ordinary.value, ordinary.overflow ? "true" : "false",
                overflow.value, overflow.overflow ? "true" : "false",
                wrapped.value, wrapped.overflow ? "true" : "false",
                identity.value, identity.overflow ? "true" : "false") < 0
      ? 1
      : 0;
}
