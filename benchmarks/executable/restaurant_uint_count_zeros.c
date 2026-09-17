// C23 reference for the Restaurant UInt count-zeros workload.

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile uint64_t runtime_value = UINT64_C(0xf0f0f0f00f0f0f0f);

static uint64_t count_zeros_u64(uint64_t value) {
  uint64_t count = UINT64_C(0);
  for (uint64_t bit = UINT64_C(0); bit < UINT64_C(64); bit += 1) {
    count += (value & UINT64_C(1)) == UINT64_C(0);
    value >>= 1;
  }
  return count;
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  const uint64_t zeros = count_zeros_u64(runtime_value);
  return printf("Zeros %" PRIu64 "\n", zeros) < 0 ? 1 : 0;
}
