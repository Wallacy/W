#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static int64_t settle(int64_t limit) {
  int64_t served = 0;
  int64_t total = 0;
  while (served < limit) {
    total = total + 2;
    served = served + 1;
  }
  total = total + served;
  return total;
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  if (printf("Final %" PRId64 "\n", settle(3)) < 0) return 1;
  return 0;
}
