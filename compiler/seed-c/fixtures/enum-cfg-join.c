/* Expected: exit 0; stdout: "42/0\n"; stderr: "" */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
  LOOKUP_MISSING,
  LOOKUP_FOUND,
} LookupTag;

typedef struct {
  LookupTag tag;
  int64_t value;
} Lookup;

static Lookup choose(bool available, int64_t amount) {
  return available ? (Lookup){.tag = LOOKUP_FOUND, .value = amount}
                   : (Lookup){.tag = LOOKUP_MISSING, .value = 0};
}

static int64_t score(Lookup result) {
  switch (result.tag) {
  case LOOKUP_FOUND:
    return result.value + 1;
  case LOOKUP_MISSING:
    return 0;
  }
  return 1;
}

int main(void) {
  const int64_t present = score(choose(true, 41));
  const int64_t absent = score(choose(false, 41));
  if (printf("%" PRId64 "/%" PRId64 "\n", present, absent) < 0) return 1;
  return 0;
}
