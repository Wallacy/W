// C23 reference for the Restaurant UInt overflowing-add workload.

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile uint64_t runtime_maximum = UINT64_MAX;
static volatile uint64_t runtime_ordinary = UINT64_C(10);
static volatile uint64_t runtime_increment = UINT64_C(1);

typedef struct {
  uint64_t wrapped;
  bool overflow;
} overflowing_add_result;

static overflowing_add_result overflowing_add_u64(uint64_t left,
                                                   uint64_t right) {
  const uint64_t wrapped = left + right;
  return (overflowing_add_result){wrapped, wrapped < left};
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  const overflowing_add_result maximum =
      overflowing_add_u64(runtime_maximum, runtime_increment);
  const overflowing_add_result ordinary =
      overflowing_add_u64(runtime_ordinary, runtime_increment);
  return printf("Overflowing %" PRIu64 "/%s/%" PRIu64 "/%s\n",
                maximum.wrapped, maximum.overflow ? "true" : "false",
                ordinary.wrapped, ordinary.overflow ? "true" : "false") < 0
      ? 1
      : 0;
}
