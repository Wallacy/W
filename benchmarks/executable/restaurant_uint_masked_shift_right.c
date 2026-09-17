// C23 reference for the Restaurant UInt masked-shift-right workload.

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile uint64_t runtime_value = UINT64_C(128);
static volatile uint64_t runtime_count = UINT64_C(65);

static uint64_t masked_shift_right_u64(uint64_t value, uint64_t count) {
  const uint64_t masked_count = count & UINT64_C(63);
  return value >> masked_count;
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  const uint64_t masked =
      masked_shift_right_u64(runtime_value, runtime_count);
  return printf("Masked %" PRIu64 "\n", masked) < 0 ? 1 : 0;
}
