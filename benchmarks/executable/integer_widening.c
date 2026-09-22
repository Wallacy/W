// Expected exit: 0
// Expected stdout:
// Widen -7/200/202/203
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile int8_t signed_input = -7;
static volatile uint8_t first_unsigned_input = 200u;
static volatile uint8_t binding_input = 202u;
static volatile uint8_t alias_input = 203u;

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  const int16_t returned = signed_input;
  const int16_t called = first_unsigned_input;
  const uint16_t unsigned_value = binding_input;
  const int64_t alias_value = alias_input;
  return printf("Widen %d/%d/%u/%lld\n", (int)returned, (int)called,
                (unsigned)unsigned_value,
                (long long)alias_value) < 0;
}
