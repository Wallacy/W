// Expected: exit 0; stdout: "7,5,26\n"; stderr: ""
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
  int64_t first;
  int64_t second;
} TuplePair;

typedef struct {
  int64_t left;
  int64_t right;
} ValuePair;

static TuplePair make_tuple_pair(int64_t first, int64_t second) {
  const TuplePair pair = {first, second};
  return pair;
}

static int64_t combine_tuple_pair(TuplePair pair, int64_t scale) {
  const int64_t product = pair.first * scale;
  return product + pair.second;
}

static ValuePair make_value_pair(int64_t left, int64_t right) {
  const ValuePair pair = {.right = right, .left = left};
  return pair;
}

static int64_t combine_value_pair(ValuePair pair, int64_t scale) {
  const int64_t product = pair.left * scale;
  return product + pair.right;
}

int main(void) {
  const TuplePair tuple = make_tuple_pair(7, 5);
  const ValuePair value = make_value_pair(tuple.first, tuple.second);
  const int64_t tuple_result = combine_tuple_pair(tuple, 3);
  const int64_t value_result = combine_value_pair(value, 3);
  const int64_t result = tuple_result + value_result - 26;
  if (printf("%" PRId64 ",%" PRId64 ",%" PRId64 "\n", value.left,
             value.right, result) < 0) {
    return 1;
  }
  return 0;
}
