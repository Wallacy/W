#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static int64_t receipt_digits(int64_t value) {
  int64_t remaining = value;
  int64_t digits = 0;
  do {
    digits = digits + 1;
    remaining = remaining / 10;
  } while (remaining > 0);
  return digits;
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  if (printf("Receipt digits %" PRId64 "/%" PRId64 "\n",
             receipt_digits(0), receipt_digits(42424)) < 0) return 1;
  return 0;
}
