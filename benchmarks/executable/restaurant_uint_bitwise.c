// C23 reference for the consolidated Restaurant UInt bit-primitives family.
// Expected exit: 0
// Expected stdout:
// Not 18446744073709551615
// And 0
// Or 18446744073709551615
// Xor 18446744073709551615
// Ones 32
// Zeros 32
// Leading 56
// Leading zero 64
// Trailing 12
// Trailing zero 64

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile uint64_t runtime_inputs[] = {
    UINT64_C(0), UINT64_C(9223372036854775808), UINT64_C(9223372036854775807),
    UINT64_C(0xf0f0f0f00f0f0f0f), UINT64_C(0x00000000000000f0),
    UINT64_C(0x000000000000f000),
};

static uint64_t count_ones_u64(uint64_t value) {
  uint64_t count = UINT64_C(0);
  while (value != UINT64_C(0)) {
    count += value & UINT64_C(1);
    value >>= 1;
  }
  return count;
}

static uint64_t count_leading_zeros_u64(uint64_t value) {
  uint64_t count = UINT64_C(0);
  for (uint64_t bit = UINT64_C(64); bit != UINT64_C(0); bit -= 1) {
    const uint64_t mask = UINT64_C(1) << (bit - UINT64_C(1));
    if ((value & mask) != UINT64_C(0)) break;
    count += 1;
  }
  return count;
}

static uint64_t count_trailing_zeros_u64(uint64_t value) {
  uint64_t count = UINT64_C(0);
  for (uint64_t bit = UINT64_C(0); bit != UINT64_C(64); bit += 1) {
    const uint64_t mask = UINT64_C(1) << bit;
    if ((value & mask) != UINT64_C(0)) break;
    count += 1;
  }
  return count;
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  const uint64_t zero = runtime_inputs[0];
  const uint64_t high = runtime_inputs[1];
  const uint64_t lower = runtime_inputs[2];
  const uint64_t population_input = runtime_inputs[3];
  const uint64_t leading_input = runtime_inputs[4];
  const uint64_t trailing_input = runtime_inputs[5];
  const uint64_t bit_not = ~zero;
  const uint64_t bit_and = high & lower;
  const uint64_t bit_or = high | lower;
  const uint64_t bit_xor = high ^ lower;
  const uint64_t ones = count_ones_u64(population_input);
  const uint64_t zeros = count_ones_u64(~population_input);
  const uint64_t leading = count_leading_zeros_u64(leading_input);
  const uint64_t leading_zero = count_leading_zeros_u64(zero);
  const uint64_t trailing = count_trailing_zeros_u64(trailing_input);
  const uint64_t trailing_zero = count_trailing_zeros_u64(zero);
  return printf(
             "Not %" PRIu64 "\nAnd %" PRIu64 "\nOr %" PRIu64
             "\nXor %" PRIu64 "\nOnes %" PRIu64 "\nZeros %" PRIu64
             "\nLeading %" PRIu64 "\nLeading zero %" PRIu64
             "\nTrailing %" PRIu64 "\nTrailing zero %" PRIu64 "\n",
             bit_not, bit_and, bit_or, bit_xor, ones, zeros, leading,
             leading_zero, trailing, trailing_zero) < 0
             ? 1
             : 0;
}
