#ifndef W_SEED_PARALLEL_PROVIDER0_H
#define W_SEED_PARALLEL_PROVIDER0_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_parallel_invocation0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PARPROV0 is bounded physical-provider evidence for an independently
 * verified PARSEL0 selection. It is not the public Task ABI, a scheduler, a
 * worker pool, cancellation, or a general runtime contract. */
#define W_SEED_PARALLEL_PROVIDER0_SCHEMA_VERSION \
  "w-seed-parallel-provider0-1"
#define W_SEED_PARALLEL_PROVIDER0_MAX_TASKS \
  W_SEED_PARALLEL_SELECTION0_MAX_TASKS
#define W_SEED_PARALLEL_PROVIDER0_MAX_CAPACITY 2u

typedef enum {
  W_SEED_PARALLEL_PROVIDER0_OK = 0,
  W_SEED_PARALLEL_PROVIDER0_INVALID,
  W_SEED_PARALLEL_PROVIDER0_UNSUPPORTED,
  W_SEED_PARALLEL_PROVIDER0_PROVIDER_FAILURE,
  W_SEED_PARALLEL_PROVIDER0_TASK_FAILURE,
} w_seed_parallel_provider0_status;

typedef enum {
  W_SEED_PARALLEL_PROVIDER0_KIND_NONE = 0,
  W_SEED_PARALLEL_PROVIDER0_KIND_WINDOWS_KERNEL32 = 1,
} w_seed_parallel_provider0_kind;

typedef struct {
  const w_seed_hir0_program *program;
  const w_seed_hir0_result *hir_result;
  const w_seed_parallel_selection0 *selection;
  const w_seed_parallel_invocation0_plan *invocation;
  uint32_t provider_capacity;
} w_seed_parallel_provider0_input;

/* This record is semantic: provider kind and capacity are deliberately
 * absent. Equal selected functions and values have an equal outcome digest
 * regardless of the physical provider used to obtain them. */
typedef struct {
  char schema[sizeof(W_SEED_PARALLEL_PROVIDER0_SCHEMA_VERSION)];
  uint32_t task_count;
  uint32_t function_indices[W_SEED_PARALLEL_PROVIDER0_MAX_TASKS];
  int64_t values[W_SEED_PARALLEL_PROVIDER0_MAX_TASKS];
  uint8_t hir_semantic_digest[32];
  uint8_t outcome_digest[32];
} w_seed_parallel_provider0_outcomes;

/* This record is physical evidence and is excluded from semantic identity.
 * overlap_observed is derived from simultaneous active workers at the
 * provider rendezvous, never from callback duration or elapsed time. */
typedef struct {
  w_seed_parallel_provider0_kind provider_kind;
  uint32_t provider_capacity;
  uint32_t task_count;
  uint32_t started_count;
  uint32_t completed_count;
  uint32_t maximum_active_workers;
  bool overlap_observed;
  uint8_t reserved[3];
} w_seed_parallel_provider0_receipt;

/* Execution is transactional for outcomes and receipt: every non-OK return
 * leaves both caller-owned records bitwise unchanged. The closed seed subset
 * admits only HIR-derived pure scalar evaluation, so task effects require no
 * rollback. Outputs must be disjoint from one another, the input records,
 * invocation plan, and all verified-HIR backing ranges. */
w_seed_parallel_provider0_status w_seed_parallel_provider0_execute(
    const w_seed_parallel_provider0_input *input,
    w_seed_parallel_provider0_outcomes *outcomes,
    w_seed_parallel_provider0_receipt *receipt);

/* Recheck canonical semantic bytes without executing callbacks. */
bool w_seed_parallel_provider0_verify_outcomes(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection,
    const w_seed_parallel_provider0_outcomes *outcomes);

#ifdef __cplusplus
}
#endif

#endif
