// Expected cases (user arguments => exit; stdout; stderr):
// [] => 0; "Mix 16743387770561346575\n"; ""
// ["x"] => 0; "Mix 553619412775969103\n"; ""
// ["alpha", "beta", "gamma"] => 0; "Mix 5608831001354178255\n"; ""

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static uint64_t rotate_and_fold(uint64_t value) {
  const uint64_t rotated = (value << 23u) | (value >> (64u - 23u));
  const uint64_t shifted = rotated >> 17u;
  return rotated ^ shifted;
}

static uint64_t mix_round(uint64_t state, uint64_t lane) {
  const uint64_t added = state + lane;
  const uint64_t folded = rotate_and_fold(added);
  return folded * UINT64_C(0x9e3779b185ebca87);
}

int main(int argc, char **argv) {
  (void)argv;
  const uint64_t lane = argc > 0 ? (uint64_t)(argc - 1) : UINT64_C(0);
  const uint64_t mixed = mix_round(UINT64_C(0x0206f2a26db56cfd), lane);
  return printf("Mix %" PRIu64 "\n", mixed) < 0 ? 1 : 0;
}
