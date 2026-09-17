// C23 reference for the Restaurant UInt reversed-bytes workload.

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile uint64_t runtime_value = UINT64_C(0x0123456789abcdef);

static uint64_t reverse_bytes_u64(uint64_t value) {
  uint64_t reversed = UINT64_C(0);
  for (uint64_t byte = UINT64_C(0); byte != UINT64_C(8); byte += 1) {
    reversed = (reversed << 8) | (value & UINT64_C(0xff));
    value >>= 8;
  }
  return reversed;
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  const uint64_t reversed = reverse_bytes_u64(runtime_value);
  return printf("Bytes %" PRIu64 "\n", reversed) < 0 ? 1 : 0;
}
