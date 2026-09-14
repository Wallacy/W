#ifndef W_SEED_PARALLEL_SELECTION0_H
#define W_SEED_PARALLEL_SELECTION0_H

#include <stdbool.h>
#include <stdint.h>

#include "w_seed_hir0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PARSEL0 is a fixed, caller-owned proof that verified HIR requested one
 * explicit parallel domain. It is not a scheduler, provider, frame, task
 * handle, runtime ABI, or performance claim. Provider capacity intentionally
 * does not appear in this record. */
#define W_SEED_PARALLEL_SELECTION0_SCHEMA_VERSION \
  "w-seed-parallel-selection0-1"
#define W_SEED_PARALLEL_SELECTION0_MAX_TASKS W_SEED_HIR0_PARALLEL_MAX_TASKS

typedef enum {
  W_SEED_PARALLEL_SELECTION0_OK = 0,
  W_SEED_PARALLEL_SELECTION0_INVALID,
  W_SEED_PARALLEL_SELECTION0_UNSUPPORTED,
} w_seed_parallel_selection0_status;

typedef struct {
  char schema[sizeof(W_SEED_PARALLEL_SELECTION0_SCHEMA_VERSION)];
  uint32_t root_function_index;
  uint32_t task_count;
  uint32_t function_count;
  uint32_t instruction_count;
  uint32_t binding_count;
  uint32_t call_count;
  uint32_t task_call_indices[W_SEED_PARALLEL_SELECTION0_MAX_TASKS];
  uint32_t task_function_indices[W_SEED_PARALLEL_SELECTION0_MAX_TASKS];
  uint32_t launch_binding_indices[W_SEED_PARALLEL_SELECTION0_MAX_TASKS];
  uint32_t join_binding_indices[W_SEED_PARALLEL_SELECTION0_MAX_TASKS];
  w_seed_hir0_call_placement_kind placement;
  char domain_identity[sizeof(W_SEED_FRONTEND_DOMAIN_IDENTITY)];
  w_seed_frontend_domain_mode domain_mode;
  uint32_t domain_capabilities;
  uint8_t reserved[4];
  uint8_t hir_semantic_digest[32];
} w_seed_parallel_selection0;

/* Selection is transactional: INVALID and UNSUPPORTED leave selection
 * untouched. The fixed record has no caller-supplied output capacity to
 * truncate; all variable ranges belong to the independently verified HIR. */
w_seed_parallel_selection0_status w_seed_parallel_selection0_select(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    w_seed_parallel_selection0 *selection);

bool w_seed_parallel_selection0_verify(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection);

#ifdef __cplusplus
}
#endif

#endif
