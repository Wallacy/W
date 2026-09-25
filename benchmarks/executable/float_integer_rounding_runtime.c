// Expected cases (user arguments => exit; stdout; stderr):
// [] => 0; "Rounded 42\n"; ""
// ["x"] => 1; ""; ""
// ["x", "y"] => 1; ""; ""

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

_Static_assert(sizeof(double) == sizeof(uint64_t),
               "binary64 requires 64-bit double storage");

static double from_bits(uint64_t bits) {
  double value;
  memcpy(&value, &bits, sizeof(value));
  return value;
}

int main(int argc, char **argv) {
  (void)argv;
  const uint64_t bits = argc <= 1 ? UINT64_C(0x4045600000000000)
                                  : UINT64_C(0x7ff8000000000000);
  const double value = from_bits(bits);
  if (!isfinite(value) || value < -128.0 || value >= 128.0) return 1;
  const int8_t rounded = (int8_t)value;
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  if (printf("Rounded %" PRId8 "\n", rounded) < 0) return 1;
  return 0;
}
