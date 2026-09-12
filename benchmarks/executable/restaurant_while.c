#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static int64_t count_to(int64_t limit) {
  int64_t count = 0;
  while (count < limit) count += 1;
  return count;
}

int main(void) {
  printf("Served %" PRId64 "\n", count_to(3));
  return 0;
}
