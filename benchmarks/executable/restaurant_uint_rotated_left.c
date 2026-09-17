// C23 reference for the Restaurant UInt rotated-left workload.

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile uint64_t runtime_value = UINT64_C(0x8000000000000001);
static volatile uint64_t runtime_count = UINT64_C(1);

static uint64_t rotated_left_u64(uint64_t value, uint64_t count) {
  const uint64_t shift = count & UINT64_C(63);
  if (shift == 0) return value;
  return (value << shift) | (value >> (UINT64_C(64) - shift));
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  const uint64_t rotated = rotated_left_u64(runtime_value, runtime_count);
  return printf("Rotated %" PRIu64 "\n", rotated) < 0 ? 1 : 0;
}
