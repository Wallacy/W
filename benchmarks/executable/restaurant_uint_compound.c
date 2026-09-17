#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile uint64_t runtime_value = UINT64_MAX;
static volatile uint64_t runtime_mask = UINT64_C(240);
static volatile uint64_t runtime_toggle = UINT64_C(170);
static volatile uint64_t runtime_set = UINT64_C(5);

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  uint64_t value = runtime_value;
  value &= runtime_mask;
  value ^= runtime_toggle;
  value |= runtime_set;
  return printf("UInt compound %" PRIu64 "\n", value) < 0 ? 1 : 0;
}
