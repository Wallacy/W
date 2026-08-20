#ifndef W_SEED_SHA256_H
#define W_SEED_SHA256_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Internal caller-owned SHA-256 state shared by source adapters. */
typedef struct {
  uint32_t state[8];
  uint64_t bit_count;
  uint8_t block[64];
  size_t block_length;
} w_seed_sha256_state;

void w_seed_sha256_init(w_seed_sha256_state *state);
void w_seed_sha256_update(w_seed_sha256_state *state, const uint8_t *bytes,
                          size_t length);
void w_seed_sha256_final(w_seed_sha256_state *state, uint8_t digest[32]);

#ifdef __cplusplus
}
#endif

#endif
