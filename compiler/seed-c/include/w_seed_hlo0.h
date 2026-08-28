#ifndef W_SEED_HLO0_H
#define W_SEED_HLO0_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_hir0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Internal verified-HIR-backed HLO0 plan adapter for one bounded subset. It
 * does not emit, link, run, or allocate. All input and output storage remains
 * caller-owned. */
#define W_SEED_HLO0_SCHEMA_VERSION "w-seed-hlo0-1"
#define W_SEED_HLO0_MAX_TEXT 64u
#define W_SEED_HLO0_MAX_PAYLOAD 256u

typedef enum {
  W_SEED_HLO0_OK = 0,
  W_SEED_HLO0_UNSUPPORTED,
  W_SEED_HLO0_CAPACITY,
  W_SEED_HLO0_INVALID,
} w_seed_hlo0_status;

typedef enum {
  W_SEED_HLO0_NEWLINE_ADD_LF = 0,
} w_seed_hlo0_newline_policy;

typedef struct {
  const w_seed_hir0_program *program;
  const w_seed_hir0_result *hir_result;
} w_seed_hlo0_input;

/* One bounded Hello plan. Text fields are copied into this caller-owned
 * record. The payload has a fixed ceiling and is not a borrowed source
 * pointer. stdout_sha256 covers payload followed by the added LF. */
typedef struct {
  char schema[W_SEED_HLO0_MAX_TEXT];
  char profile[W_SEED_HLO0_MAX_TEXT];
  char slot[W_SEED_HLO0_MAX_TEXT];
  char entry_target[W_SEED_HLO0_MAX_TEXT];
  char handler[W_SEED_HLO0_MAX_TEXT];
  char callee[W_SEED_HLO0_MAX_TEXT];
  char requirement[W_SEED_HLO0_MAX_TEXT];
  bool is_async;
  bool is_throws;
  bool is_unsafe;
  bool has_borrow_clause;
  bool zero_parameters;
  bool unit_return;
  w_seed_hlo0_newline_policy newline_policy;
  uint8_t payload[W_SEED_HLO0_MAX_PAYLOAD];
  size_t payload_bytes;
  size_t stdout_bytes;
  uint8_t stdout_sha256[32];
  bool exit_success;
} w_seed_hlo0_plan;

typedef struct {
  size_t plans;
  size_t payload_bytes;
  size_t receipt_bytes;
} w_seed_hlo0_counts;

typedef struct {
  w_seed_hlo0_status status;
  w_seed_hlo0_counts required;
  w_seed_hlo0_counts written;
} w_seed_hlo0_result;

typedef struct {
  w_seed_hlo0_plan *plans;
  size_t plan_capacity;
  uint8_t *receipt;
  size_t receipt_capacity;
} w_seed_hlo0_output;

/* Measure the one-plan result without writing caller-owned output. */
w_seed_hlo0_status w_seed_hlo0_measure(const w_seed_hlo0_input *input,
                                        w_seed_hlo0_counts *counts,
                                        w_seed_hlo0_result *result);

/* Build one exact verified-HIR-backed plan and receipt. On failure no output
 * byte or plan field is changed. This is a plan boundary only; it never
 * executes W. */
w_seed_hlo0_status w_seed_hlo0_run(const w_seed_hlo0_input *input,
                                   w_seed_hlo0_output *output,
                                   w_seed_hlo0_result *result);

#ifdef __cplusplus
}
#endif

#endif
