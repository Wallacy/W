// C23 reference for the Restaurant UInt saturating-add workload.

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile uint64_t runtime_maximum = UINT64_MAX;
static volatile uint64_t runtime_ordinary = UINT64_C(10);
static volatile uint64_t runtime_increment = UINT64_C(1);

static uint64_t saturating_add_u64(uint64_t left, uint64_t right) {
  if (left > UINT64_MAX - right) return UINT64_MAX;
  return left + right;
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  const uint64_t maximum =
      saturating_add_u64(runtime_maximum, runtime_increment);
  const uint64_t ordinary =
      saturating_add_u64(runtime_ordinary, runtime_increment);
  return printf("Saturated %" PRIu64 "/%" PRIu64 "\n", maximum, ordinary) < 0
      ? 1
      : 0;
}
