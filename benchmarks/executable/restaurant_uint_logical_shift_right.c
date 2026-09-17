// C23 reference for the Restaurant UInt logical-shift-right workload.

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile uint64_t runtime_value = UINT64_C(128);
static volatile uint64_t runtime_count = UINT64_C(1);

static bool logical_shift_right_u64(uint64_t value, uint64_t count,
                                    uint64_t *result) {
  const bool valid_count = count < UINT64_C(64);
  if (!valid_count) return false;
  *result = value >> count;
  return true;
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  uint64_t logical = UINT64_C(0);
  if (!logical_shift_right_u64(runtime_value, runtime_count, &logical)) {
    return 1;
  }
  return printf("Logical %" PRIu64 "\n", logical) < 0 ? 1 : 0;
}
