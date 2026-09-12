#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static int64_t count_to(int64_t limit) {
  int64_t count = 0;
  while (count < limit) count += 1;
  return count;
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  if (printf("Served %" PRId64 "\n", count_to(3)) < 0) return 1;
  return 0;
}
