// C23 reference for the Restaurant UInt reversed-bits workload.

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile uint64_t runtime_value = UINT64_C(0x0123456789abcdef);

static uint64_t reverse_bits_u64(uint64_t value) {
  uint64_t reversed = UINT64_C(0);
  for (uint64_t bit = UINT64_C(0); bit != UINT64_C(64); bit += 1) {
    reversed = (reversed << 1) | (value & UINT64_C(1));
    value >>= 1;
  }
  return reversed;
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  const uint64_t reversed = reverse_bits_u64(runtime_value);
  return printf("Bits %" PRIu64 "\n", reversed) < 0 ? 1 : 0;
}
