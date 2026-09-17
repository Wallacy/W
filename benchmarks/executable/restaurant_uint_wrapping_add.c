// C23 reference for the Restaurant UInt wrapping-add workload.

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile uint64_t runtime_value = UINT64_MAX;
static volatile uint64_t runtime_increment = UINT64_C(1);

static uint64_t wrapping_add_u64(uint64_t left, uint64_t right) {
  return left + right;
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  const uint64_t wrapped =
      wrapping_add_u64(runtime_value, runtime_increment);
  return printf("Wrapped %" PRIu64 "\n", wrapped) < 0 ? 1 : 0;
}
