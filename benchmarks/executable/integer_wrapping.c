// C23 reference for the Restaurant fixed-width integer wrapping workload.
// Expected exit: 0
// Expected stdout:
// i8/u8 -128/0
// i16/u16 32767/2
// i32/u32 -2/4294967295
// i64/u64 -9223372036854775808/0
// Int/UInt -9223372036854775808/18446744073709551615

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile uint64_t runtime_inputs[] = {
    UINT64_C(127), UINT64_C(1), UINT64_C(0x8001), UINT64_C(2),
    UINT64_C(0x7fffffff), UINT64_C(2), UINT64_C(0x7fffffffffffffff),
    UINT64_C(1), UINT64_C(2), UINT64_C(8), UINT64_C(0x8001),
    UINT64_C(1), UINT64_C(1), UINT64_MAX, UINT64_C(1), UINT64_C(1),
    UINT64_C(0xfffffffffffffffe), UINT64_C(63), UINT64_C(0), UINT64_C(1),
};

static uint64_t width_mask(unsigned width) {
  return width == 64 ? UINT64_MAX : (UINT64_C(1) << width) - UINT64_C(1);
}

static uint64_t wrapping_power(uint64_t base, uint64_t exponent,
                               unsigned width) {
  const uint64_t mask = width_mask(width);
  uint64_t result = UINT64_C(1);
  base &= mask;
  while (exponent != UINT64_C(0)) {
    if ((exponent & UINT64_C(1)) != UINT64_C(0)) result = (result * base) & mask;
    exponent >>= 1;
    if (exponent != UINT64_C(0)) base = (base * base) & mask;
  }
  return result;
}

static int print_integer(uint64_t bits, unsigned width, int is_signed) {
  const uint64_t mask = width_mask(width);
  const uint64_t value = bits & mask;
  const uint64_t sign = UINT64_C(1) << (width - 1u);
  if (!is_signed || (value & sign) == UINT64_C(0)) return printf("%" PRIu64, value);
  return printf("-%" PRIu64, ((~value) + UINT64_C(1)) & mask);
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  const uint64_t signed8 =
      (runtime_inputs[0] + runtime_inputs[1]) & width_mask(8);
  const uint64_t signed16 =
      (runtime_inputs[2] - runtime_inputs[3]) & width_mask(16);
  const uint64_t signed32 =
      (runtime_inputs[4] * runtime_inputs[5]) & width_mask(32);
  const uint64_t signed64_min =
      (runtime_inputs[6] + runtime_inputs[7]) & width_mask(64);
  const uint64_t signed64 = UINT64_C(0) - signed64_min;
  const uint64_t unsigned8 =
      wrapping_power(runtime_inputs[8], runtime_inputs[9], 8);
  const uint64_t unsigned16 =
      (runtime_inputs[10] << runtime_inputs[11]) & width_mask(16);
  const uint64_t unsigned32 =
      (UINT64_C(0) - runtime_inputs[12]) & width_mask(32);
  const uint64_t unsigned64 = runtime_inputs[13] + runtime_inputs[14];
  const uint64_t signed_alias =
      wrapping_power(runtime_inputs[16], runtime_inputs[17], 64);
  const uint64_t unsigned_alias = runtime_inputs[18] - runtime_inputs[19];

#define PRINT_PAIR(label, left, left_width, left_signed, right, right_width,  \
                   right_signed)                                             \
  do {                                                                        \
    if (fputs(label " ", stdout) == EOF ||                                   \
        print_integer((left), (left_width), (left_signed)) < 0 ||             \
        putchar('/') == EOF ||                                                \
        print_integer((right), (right_width), (right_signed)) < 0 ||          \
        putchar('\n') == EOF)                                                 \
      return 1;                                                               \
  } while (0)

  PRINT_PAIR("i8/u8", signed8, 8, 1, unsigned8, 8, 0);
  PRINT_PAIR("i16/u16", signed16, 16, 1, unsigned16, 16, 0);
  PRINT_PAIR("i32/u32", signed32, 32, 1, unsigned32, 32, 0);
  PRINT_PAIR("i64/u64", signed64, 64, 1, unsigned64, 64, 0);
  PRINT_PAIR("Int/UInt", signed_alias, 64, 1, unsigned_alias, 64, 0);
#undef PRINT_PAIR
  return 0;
}
