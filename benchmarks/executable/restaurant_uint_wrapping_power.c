// C23 reference for the Restaurant UInt wrapping-power workload.

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile uint64_t runtime_base = UINT64_C(3);
static volatile uint64_t runtime_exponent = UINT64_C(40);

static uint64_t wrapping_power_u64(uint64_t base, uint64_t exponent) {
  uint64_t result = UINT64_C(1);
  while (exponent != UINT64_C(0)) {
    if ((exponent & UINT64_C(1)) != UINT64_C(0)) result *= base;
    base *= base;
    exponent >>= 1;
  }
  return result;
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  const uint64_t wrapped =
      wrapping_power_u64(runtime_base, runtime_exponent);
  return printf("Wrapped %" PRIu64 "\n", wrapped) < 0 ? 1 : 0;
}
