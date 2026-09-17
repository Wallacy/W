// C23 reference for the Restaurant UInt saturating-subtract workload.

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile uint64_t runtime_zero = UINT64_C(0);
static volatile uint64_t runtime_ordinary = UINT64_C(11);
static volatile uint64_t runtime_decrement = UINT64_C(1);

static uint64_t saturating_subtract_u64(uint64_t value, uint64_t amount) {
  if (value < amount) return 0;
  return value - amount;
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  const uint64_t zero =
      saturating_subtract_u64(runtime_zero, runtime_decrement);
  const uint64_t ordinary =
      saturating_subtract_u64(runtime_ordinary, runtime_decrement);
  return printf("Saturated subtract %" PRIu64 "/%" PRIu64 "\n", zero, ordinary) < 0
      ? 1
      : 0;
}
