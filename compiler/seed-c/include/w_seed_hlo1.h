#ifndef W_SEED_HLO1_H
#define W_SEED_HLO1_H

#include <stddef.h>
#include <stdint.h>

#include "w_seed_hlo0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Internal HLO1 C11 emitter. It does not allocate, execute, link, or prove
 * source provenance. All input and output storage remains caller-owned. */
#define W_SEED_HLO1_SCHEMA_VERSION "w-seed-hlo1-1"
#define W_SEED_HLO1_MAX_C_BYTES 2048u

typedef enum {
  W_SEED_HLO1_OK = 0,
  W_SEED_HLO1_INVALID_PLAN,
  W_SEED_HLO1_CAPACITY,
  W_SEED_HLO1_ALIAS,
} w_seed_hlo1_status;

typedef struct {
  size_t c_bytes;
} w_seed_hlo1_counts;

typedef struct {
  w_seed_hlo1_status status;
  w_seed_hlo1_counts required;
  w_seed_hlo1_counts written;
  uint8_t c_sha256[32];
} w_seed_hlo1_result;

typedef struct {
  uint8_t *bytes;
  size_t capacity;
} w_seed_hlo1_output;

/* Build and measure the deterministic C11 artifact without writing output.
 * On any failure, counts and result are not modified. */
w_seed_hlo1_status w_seed_hlo1_measure(const w_seed_hlo0_plan *plan,
                                        w_seed_hlo1_counts *counts,
                                        w_seed_hlo1_result *result);

/* Emit one exact C11 artifact. Any non-OK return leaves all caller-owned
 * records and buffers unchanged. The return value carries the failure status. */
w_seed_hlo1_status w_seed_hlo1_emit(const w_seed_hlo0_plan *plan,
                                     const w_seed_hlo1_output *output,
                                     w_seed_hlo1_result *result);

#ifdef __cplusplus
}
#endif

#endif
