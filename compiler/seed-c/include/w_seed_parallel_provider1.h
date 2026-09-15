#ifndef W_SEED_PARALLEL_PROVIDER1_H
#define W_SEED_PARALLEL_PROVIDER1_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_parallel_invocation1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PARPROV1 is measured compiler-lifecycle evidence. It is not a public Task
 * ABI, scheduler, worker pool, cancellation contract, or general runtime. */
#define W_SEED_PARALLEL_PROVIDER1_SCHEMA_VERSION \
  "w-seed-parallel-provider1-1"
#define W_SEED_PARALLEL_PROVIDER1_MAX_CAPACITY 2u

typedef enum {
  W_SEED_PARALLEL_PROVIDER1_OK = 0,
  W_SEED_PARALLEL_PROVIDER1_INVALID,
  W_SEED_PARALLEL_PROVIDER1_UNSUPPORTED,
  W_SEED_PARALLEL_PROVIDER1_ALIAS,
  W_SEED_PARALLEL_PROVIDER1_CAPACITY,
  W_SEED_PARALLEL_PROVIDER1_PROVIDER_FAILURE,
  W_SEED_PARALLEL_PROVIDER1_TASK_FAILURE,
} w_seed_parallel_provider1_status;

typedef enum {
  W_SEED_PARALLEL_PROVIDER1_KIND_NONE = 0,
  W_SEED_PARALLEL_PROVIDER1_KIND_WINDOWS_KERNEL32 = 1,
} w_seed_parallel_provider1_kind;

typedef struct {
  const w_seed_hir0_program *hir_program;
  const w_seed_hir0_result *hir_result;
  const w_seed_parallel_selection1_program *selection;
  const w_seed_parallel_selection1_result *selection_result;
  const w_seed_parallel_invocation1_program *invocation;
  const w_seed_parallel_invocation1_result *invocation_result;
  uint32_t provider_capacity;
} w_seed_parallel_provider1_input;

typedef struct {
  size_t outcomes;
  size_t workspace_values;
} w_seed_parallel_provider1_counts;

typedef struct {
  uint32_t function_index;
  int64_t value;
} w_seed_parallel_provider1_outcome;

/* Workspace is caller-owned physical scratch. A failed provider can modify
 * workspace_values. Outcomes, result, and receipt remain unchanged on failure. */
typedef struct {
  w_seed_parallel_provider1_outcome *outcomes;
  size_t outcome_capacity;
  int64_t *workspace_values;
  size_t workspace_value_capacity;
} w_seed_parallel_provider1_output;

typedef struct {
  w_seed_parallel_provider1_status status;
  w_seed_parallel_provider1_counts required;
  w_seed_parallel_provider1_counts written;
  char schema[sizeof(W_SEED_PARALLEL_PROVIDER1_SCHEMA_VERSION)];
  uint32_t task_count;
  uint8_t hir_semantic_digest[32];
  uint8_t selection_semantic_digest[32];
  uint8_t invocation_semantic_digest[32];
  uint8_t outcome_digest[32];
} w_seed_parallel_provider1_result;

/* Physical facts never participate in outcome_digest. */
typedef struct {
  w_seed_parallel_provider1_kind provider_kind;
  uint32_t provider_capacity;
  uint32_t task_count;
  uint32_t started_count;
  uint32_t completed_count;
  uint32_t maximum_active_workers;
  bool overlap_observed;
  uint8_t reserved[3];
} w_seed_parallel_provider1_receipt;

w_seed_parallel_provider1_status w_seed_parallel_provider1_measure(
    const w_seed_parallel_provider1_input *input,
    w_seed_parallel_provider1_counts *counts,
    w_seed_parallel_provider1_result *result);

w_seed_parallel_provider1_status w_seed_parallel_provider1_execute(
    const w_seed_parallel_provider1_input *input,
    const w_seed_parallel_provider1_output *output,
    w_seed_parallel_provider1_result *result,
    w_seed_parallel_provider1_receipt *receipt);

bool w_seed_parallel_provider1_verify_outcomes(
    const w_seed_parallel_provider1_input *input,
    const w_seed_parallel_provider1_outcome *outcomes, size_t outcome_count,
    const w_seed_parallel_provider1_result *result);

#ifdef __cplusplus
}
#endif

#endif
