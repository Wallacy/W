#ifndef W_SEED_PARALLEL_INVOCATION1_H
#define W_SEED_PARALLEL_INVOCATION1_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_parallel_selection1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PARINV1 turns measured PARSEL1 relations into a measured scalar invocation
 * plan. It is compiler evidence, not a public Task or provider ABI. */
#define W_SEED_PARALLEL_INVOCATION1_SCHEMA_VERSION \
  "w-seed-parallel-invocation1-1"
#define W_SEED_PARALLEL_INVOCATION1_STEP_BUDGET 4096u

typedef enum {
  W_SEED_PARALLEL_INVOCATION1_OK = 0,
  W_SEED_PARALLEL_INVOCATION1_INVALID,
  W_SEED_PARALLEL_INVOCATION1_UNSUPPORTED,
  W_SEED_PARALLEL_INVOCATION1_ALIAS,
  W_SEED_PARALLEL_INVOCATION1_CAPACITY,
  W_SEED_PARALLEL_INVOCATION1_EVALUATION_FAILURE,
} w_seed_parallel_invocation1_status;

/* Every u32 is a canonical compiler-record index, not a runtime handle. */
typedef struct {
  uint32_t call_index;
  uint32_t function_index;
  uint32_t first_argument;
  uint32_t argument_count;
} w_seed_parallel_invocation1_task;

typedef struct {
  uint32_t owner_task;
  uint32_t parameter_ordinal;
  uint32_t parameter_index;
  uint32_t value_index;
  uint32_t type_index;
} w_seed_parallel_invocation1_argument;

typedef struct {
  size_t tasks;
  size_t arguments;
} w_seed_parallel_invocation1_counts;

typedef struct {
  w_seed_parallel_invocation1_task *tasks;
  size_t task_capacity;
  w_seed_parallel_invocation1_argument *arguments;
  size_t argument_capacity;
} w_seed_parallel_invocation1_output;

typedef struct {
  const w_seed_parallel_invocation1_task *tasks;
  size_t task_count;
  size_t task_capacity;
  const w_seed_parallel_invocation1_argument *arguments;
  size_t argument_count;
  size_t argument_capacity;
  uint32_t root_function_index;
  uint8_t hir_semantic_digest[32];
  uint8_t selection_semantic_digest[32];
  uint8_t semantic_digest[32];
} w_seed_parallel_invocation1_program;

typedef struct {
  w_seed_parallel_invocation1_status status;
  w_seed_parallel_invocation1_counts required;
  w_seed_parallel_invocation1_counts written;
  char schema[sizeof(W_SEED_PARALLEL_INVOCATION1_SCHEMA_VERSION)];
  uint32_t root_function_index;
  uint8_t hir_semantic_digest[32];
  uint8_t selection_semantic_digest[32];
  uint8_t semantic_digest[32];
} w_seed_parallel_invocation1_result;

w_seed_parallel_invocation1_status w_seed_parallel_invocation1_measure(
    const w_seed_hir0_program *hir_program,
    const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_program *selection,
    const w_seed_parallel_selection1_result *selection_result,
    w_seed_parallel_invocation1_counts *counts,
    w_seed_parallel_invocation1_result *result);

w_seed_parallel_invocation1_status w_seed_parallel_invocation1_run(
    const w_seed_hir0_program *hir_program,
    const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_program *selection,
    const w_seed_parallel_selection1_result *selection_result,
    const w_seed_parallel_invocation1_output *output,
    w_seed_parallel_invocation1_result *result);

bool w_seed_parallel_invocation1_program_from_output(
    const w_seed_parallel_invocation1_output *output,
    const w_seed_parallel_invocation1_result *result,
    w_seed_parallel_invocation1_program *program);

bool w_seed_parallel_invocation1_verify(
    const w_seed_hir0_program *hir_program,
    const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_program *selection,
    const w_seed_parallel_selection1_result *selection_result,
    const w_seed_parallel_invocation1_program *invocation,
    const w_seed_parallel_invocation1_result *result);

w_seed_parallel_invocation1_status w_seed_parallel_invocation1_evaluate_task(
    const w_seed_hir0_program *hir_program,
    const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_program *selection,
    const w_seed_parallel_selection1_result *selection_result,
    const w_seed_parallel_invocation1_program *invocation,
    const w_seed_parallel_invocation1_result *result, uint32_t task_index,
    int64_t *value);

#ifdef __cplusplus
}
#endif

#endif
