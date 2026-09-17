// C23 reference for the Restaurant UInt saturating-multiply workload.

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile uint64_t runtime_maximum = UINT64_MAX;
static volatile uint64_t runtime_overflow_factor = UINT64_C(2);
static volatile uint64_t runtime_ordinary = UINT64_C(6);
static volatile uint64_t runtime_ordinary_factor = UINT64_C(7);

static uint64_t saturating_multiply_u64(uint64_t left, uint64_t right) {
  if (left != 0 && right > UINT64_MAX / left) return UINT64_MAX;
  return left * right;
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  const uint64_t maximum =
      saturating_multiply_u64(runtime_maximum, runtime_overflow_factor);
  const uint64_t ordinary =
      saturating_multiply_u64(runtime_ordinary, runtime_ordinary_factor);
  return printf("Saturated multiply %" PRIu64 "/%" PRIu64 "\n", maximum, ordinary) < 0
      ? 1
      : 0;
}
