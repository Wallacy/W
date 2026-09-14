#ifndef W_SEED_PARALLEL_INVOCATION0_H
#define W_SEED_PARALLEL_INVOCATION0_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_parallel_selection0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PARINV0 closes the seed callback trust gap. It is a compiler-owned,
 * fixed-storage invocation proof, not a public runtime ABI or interpreter. */
#define W_SEED_PARALLEL_INVOCATION0_SCHEMA_VERSION \
  "w-seed-parallel-invocation0-1"
#define W_SEED_PARALLEL_INVOCATION0_MAX_TASKS \
  W_SEED_PARALLEL_SELECTION0_MAX_TASKS
#define W_SEED_PARALLEL_INVOCATION0_MAX_ARGUMENTS 16u
#define W_SEED_PARALLEL_INVOCATION0_STEP_BUDGET 4096u

typedef enum {
  W_SEED_PARALLEL_INVOCATION0_OK = 0,
  W_SEED_PARALLEL_INVOCATION0_INVALID,
  W_SEED_PARALLEL_INVOCATION0_UNSUPPORTED,
  W_SEED_PARALLEL_INVOCATION0_EVALUATION_FAILURE,
} w_seed_parallel_invocation0_status;

typedef struct {
  uint32_t call_index;
  uint32_t function_index;
  uint32_t argument_count;
  uint32_t argument_value_indices[W_SEED_PARALLEL_INVOCATION0_MAX_ARGUMENTS];
} w_seed_parallel_invocation0_task;

typedef struct {
  char schema[sizeof(W_SEED_PARALLEL_INVOCATION0_SCHEMA_VERSION)];
  uint32_t root_function_index;
  uint32_t task_count;
  uint32_t function_count;
  uint32_t instruction_count;
  uint32_t binding_count;
  uint32_t call_count;
  uint32_t value_count;
  w_seed_parallel_invocation0_task
      tasks[W_SEED_PARALLEL_INVOCATION0_MAX_TASKS];
  uint8_t hir_semantic_digest[32];
} w_seed_parallel_invocation0_plan;

/* Selection and evaluation are transactional. A failed selection leaves the
 * plan untouched; evaluation publishes its scalar only on success. */
w_seed_parallel_invocation0_status w_seed_parallel_invocation0_select(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection,
    w_seed_parallel_invocation0_plan *plan);

bool w_seed_parallel_invocation0_verify(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection,
    const w_seed_parallel_invocation0_plan *plan);

w_seed_parallel_invocation0_status w_seed_parallel_invocation0_evaluate_task(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection,
    const w_seed_parallel_invocation0_plan *plan, uint32_t task_index,
    int64_t *value);

#ifdef __cplusplus
}
#endif

#endif
