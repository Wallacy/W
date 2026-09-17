// C23 reference for the Restaurant UInt leading-zeros workload.

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile uint64_t runtime_value = UINT64_C(0x00000000000000f0);
static volatile uint64_t runtime_zero = UINT64_C(0);

static uint64_t count_leading_zeros_u64(uint64_t value) {
  uint64_t count = UINT64_C(0);
  for (uint64_t bit = UINT64_C(64); bit != UINT64_C(0); bit -= 1) {
    const uint64_t mask = UINT64_C(1) << (bit - UINT64_C(1));
    if ((value & mask) != UINT64_C(0)) break;
    count += 1;
  }
  return count;
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  const uint64_t leading = count_leading_zeros_u64(runtime_value);
  const uint64_t zero = count_leading_zeros_u64(runtime_zero);
  return printf("Leading %" PRIu64 "/%" PRIu64 "\n", leading, zero) < 0
             ? 1
             : 0;
}
