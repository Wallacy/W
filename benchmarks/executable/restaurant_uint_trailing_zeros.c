// C23 reference for the Restaurant UInt trailing-zeros workload.

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile uint64_t runtime_value = UINT64_C(0x000000000000f000);
static volatile uint64_t runtime_zero = UINT64_C(0);

static uint64_t count_trailing_zeros_u64(uint64_t value) {
  uint64_t count = UINT64_C(0);
  for (uint64_t bit = UINT64_C(0); bit != UINT64_C(64); bit += 1) {
    const uint64_t mask = UINT64_C(1) << bit;
    if ((value & mask) != UINT64_C(0)) break;
    count += 1;
  }
  return count;
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  const uint64_t trailing = count_trailing_zeros_u64(runtime_value);
  const uint64_t zero = count_trailing_zeros_u64(runtime_zero);
  return printf("Trailing %" PRIu64 "/%" PRIu64 "\n", trailing, zero) < 0
             ? 1
             : 0;
}
