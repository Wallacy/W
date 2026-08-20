#include "w_seed_sha256.h"

#include <limits.h>
#include <string.h>

_Static_assert(CHAR_BIT == 8, "w_seed_sha256 requires 8-bit bytes");

static const uint32_t SHA256_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

static uint32_t rotate_right(uint32_t value, unsigned int amount) {
  return (value >> amount) | (value << (32u - amount));
}

static uint32_t read_u32_be(const uint8_t *bytes) {
  return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
         ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

static void write_u32_be(uint8_t *bytes, uint32_t value) {
  bytes[0] = (uint8_t)(value >> 24);
  bytes[1] = (uint8_t)(value >> 16);
  bytes[2] = (uint8_t)(value >> 8);
  bytes[3] = (uint8_t)value;
}

static void sha256_transform(w_seed_sha256_state *state,
                             const uint8_t *block) {
  uint32_t words[64];
  for (size_t index = 0; index < 16; index += 1) {
    words[index] = read_u32_be(block + index * 4);
  }
  for (size_t index = 16; index < 64; index += 1) {
    const uint32_t first = words[index - 15];
    const uint32_t second = words[index - 2];
    const uint32_t small_sigma0 = rotate_right(first, 7) ^
                                  rotate_right(first, 18) ^ (first >> 3);
    const uint32_t small_sigma1 = rotate_right(second, 17) ^
                                  rotate_right(second, 19) ^ (second >> 10);
    words[index] = words[index - 16] + small_sigma0 + words[index - 7] +
                   small_sigma1;
  }

  uint32_t a = state->state[0];
  uint32_t b = state->state[1];
  uint32_t c = state->state[2];
  uint32_t d = state->state[3];
  uint32_t e = state->state[4];
  uint32_t f = state->state[5];
  uint32_t g = state->state[6];
  uint32_t h = state->state[7];
  for (size_t index = 0; index < 64; index += 1) {
    const uint32_t big_sigma1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^
                                rotate_right(e, 25);
    const uint32_t choose = (e & f) ^ ((~e) & g);
    const uint32_t temporary1 = h + big_sigma1 + choose + SHA256_K[index] +
                                words[index];
    const uint32_t big_sigma0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^
                                rotate_right(a, 22);
    const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
    const uint32_t temporary2 = big_sigma0 + majority;
    h = g;
    g = f;
    f = e;
    e = d + temporary1;
    d = c;
    c = b;
    b = a;
    a = temporary1 + temporary2;
  }
  state->state[0] += a;
  state->state[1] += b;
  state->state[2] += c;
  state->state[3] += d;
  state->state[4] += e;
  state->state[5] += f;
  state->state[6] += g;
  state->state[7] += h;
}

void w_seed_sha256_init(w_seed_sha256_state *state) {
  static const uint32_t initial[8] = {
      0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
      0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
  };
  (void)memset(state, 0, sizeof(*state));
  (void)memcpy(state->state, initial, sizeof(initial));
}

void w_seed_sha256_update(w_seed_sha256_state *state, const uint8_t *bytes,
                          size_t length) {
  state->bit_count += (uint64_t)length * UINT64_C(8);
  while (length != 0) {
    const size_t available = sizeof(state->block) - state->block_length;
    const size_t amount = length < available ? length : available;
    (void)memcpy(state->block + state->block_length, bytes, amount);
    state->block_length += amount;
    bytes += amount;
    length -= amount;
    if (state->block_length == sizeof(state->block)) {
      sha256_transform(state, state->block);
      state->block_length = 0;
    }
  }
}

void w_seed_sha256_final(w_seed_sha256_state *state, uint8_t digest[32]) {
  const uint64_t bit_count = state->bit_count;
  state->block[state->block_length] = 0x80u;
  state->block_length += 1;
  if (state->block_length > 56) {
    (void)memset(state->block + state->block_length, 0,
                 sizeof(state->block) - state->block_length);
    sha256_transform(state, state->block);
    state->block_length = 0;
  }
  (void)memset(state->block + state->block_length, 0,
               56 - state->block_length);
  for (size_t index = 0; index < 8; index += 1) {
    state->block[56 + index] = (uint8_t)(bit_count >> (56 - index * 8));
  }
  sha256_transform(state, state->block);
  for (size_t index = 0; index < 8; index += 1) {
    write_u32_be(digest + index * 4, state->state[index]);
  }
}
