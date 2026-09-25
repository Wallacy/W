// Expected: exit 0; stdout: "13,2,0\n"; stderr: ""
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static int64_t first_at(int64_t limit) {
  int64_t index = 0;
  while (index < limit) {
    index += 1;
    if (index == 3) {
      return index + 10;
    }
  }
  return index;
}

int main(void) {
  const int64_t early = first_at(5);
  const int64_t exhausted = first_at(2);
  const int64_t empty = first_at(0);
  if (printf("%" PRId64 ",%" PRId64 ",%" PRId64 "\n", early,
             exhausted, empty) < 0) {
    return 1;
  }
  return 0;
}
