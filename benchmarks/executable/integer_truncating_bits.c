// Expected exit: 0
// Expected stdout:
// Trunc 2/-7/-6/18446744073709551609/-1
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile int16_t signed_wide_input = 258;
static volatile int8_t signed_narrow_input = -7;
static volatile uint8_t unsigned_narrow_input = 250u;
static volatile int64_t signed_alias_input = -7;
static volatile uint64_t unsigned_alias_max_input = UINT64_MAX;

static int8_t signed_i8_from_bits(uint8_t bits) {
  if (bits <= INT8_MAX) return (int8_t)bits;
  return (int8_t)(-1 - (int16_t)(UINT8_MAX - bits));
}

static int64_t signed_i64_from_bits(uint64_t bits) {
  if (bits <= (uint64_t)INT64_MAX) return (int64_t)bits;
  return -1 - (int64_t)(UINT64_MAX - bits);
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  const int8_t signed_narrow = signed_i8_from_bits((uint8_t)signed_wide_input);
  const int16_t signed_widen = (int16_t)signed_narrow_input;
  const int8_t unsigned_narrow = signed_i8_from_bits(unsigned_narrow_input);
  const uint64_t signed_alias_to_unsigned = (uint64_t)signed_alias_input;
  const int64_t unsigned_alias_to_signed =
      signed_i64_from_bits(unsigned_alias_max_input);
  return printf("Trunc %d/%d/%d/%" PRIu64 "/%" PRId64 "\n",
                (int)signed_narrow, (int)signed_widen, (int)unsigned_narrow,
                signed_alias_to_unsigned, unsigned_alias_to_signed) < 0;
}
