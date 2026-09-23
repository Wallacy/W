// Expected: exit 0; stdout: "7,5,26\n"; stderr: ""
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
  int64_t left;
  int64_t right;
} Pair;

static Pair make_pair(int64_t left, int64_t right) {
  const Pair pair = {.left = left, .right = right};
  return pair;
}

static int64_t combine(Pair pair, int64_t scale) {
  const int64_t product = pair.left * scale;
  return product + pair.right;
}

int main(void) {
  const Pair original = make_pair(7, 5);
  const int64_t result = combine(original, 3);
  if (printf("%" PRId64 ",%" PRId64 ",%" PRId64 "\n", original.left,
             original.right, result) < 0) {
    return 1;
  }
  return 0;
}
