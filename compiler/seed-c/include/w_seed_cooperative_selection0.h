#ifndef W_SEED_COOPERATIVE_SELECTION0_H
#define W_SEED_COOPERATIVE_SELECTION0_H

#include <stdint.h>

#include "w_seed_hir0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* This is a target-neutral, caller-owned selection proof shared by the
 * NativeSubset0/MLIR0 boundary.  M1 intentionally admits a narrower
 * one-block product-selection subset than the full HIR35 envelope: exactly
 * two scalar async children and their reachable scalar helpers.  It carries
 * only copied HIR indices and proof facts; it is not a frame,
 * scheduler, or runtime ABI. */
#define W_SEED_COOPERATIVE_SELECTION0_SCHEMA_VERSION \
  "w-seed-cooperative-selection0-1"
#define W_SEED_COOPERATIVE_SELECTION0_MAX_TASKS \
  W_SEED_HIR0_COOPERATIVE_MAX_TASKS

typedef struct {
  char schema[sizeof(W_SEED_COOPERATIVE_SELECTION0_SCHEMA_VERSION)];
  uint32_t root_function_index;
  uint32_t task_count;
  uint32_t function_count;
  uint32_t instruction_count;
  uint32_t binding_count;
  uint32_t call_count;
  uint32_t yield_count;
  uint32_t task_call_indices[W_SEED_COOPERATIVE_SELECTION0_MAX_TASKS];
  uint32_t task_function_indices[W_SEED_COOPERATIVE_SELECTION0_MAX_TASKS];
  uint32_t launch_binding_indices[W_SEED_COOPERATIVE_SELECTION0_MAX_TASKS];
  uint32_t join_binding_indices[W_SEED_COOPERATIVE_SELECTION0_MAX_TASKS];
  uint32_t task_yield_counts[W_SEED_COOPERATIVE_SELECTION0_MAX_TASKS];
  w_seed_hir0_execution_profile execution_profile;
  uint8_t reserved[3];
  uint8_t hir_semantic_digest[32];
} w_seed_cooperative_selection0;

#ifdef __cplusplus
}
#endif

#endif
