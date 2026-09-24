// Expected: exit 0; stdout: "-1,1,3\n"; stderr: ""
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static int64_t walk(int64_t limit) {
  int64_t outer = 0;
  int64_t inner = 0;
  int64_t total = 0;

  while (outer < limit) {
    outer += 1;
    inner = 0;
    while (inner < limit) {
      inner += 1;
      if (inner == 2)
        continue;
      if (inner == 3)
        break;
      if (outer == 4)
        goto next_outer;
      if (outer == 5)
        goto leave_outer;
      total += 1;
    }
  next_outer:;
  }
leave_outer:
  if (total == 0) {
    return -1;
  } else if (total == 1) {
    return 1;
  }
  return total;
}

int main(void) {
  const int64_t zero = walk(0);
  const int64_t one = walk(1);
  const int64_t six = walk(6);
  if (printf("%" PRId64 ",%" PRId64 ",%" PRId64 "\n", zero, one, six) < 0) {
    return 1;
  }
  return 0;
}
