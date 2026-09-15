#ifndef W_SEED_PARALLEL_SELECTION1_H
#define W_SEED_PARALLEL_SELECTION1_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_hir0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PARSEL1 replaces PARSEL0's fixed task arrays with measured caller-owned
 * records. Logical task count remains independent from provider capacity. */
#define W_SEED_PARALLEL_SELECTION1_SCHEMA_VERSION \
  "w-seed-parallel-selection1-1"

typedef enum {
  W_SEED_PARALLEL_SELECTION1_OK = 0,
  W_SEED_PARALLEL_SELECTION1_INVALID,
  W_SEED_PARALLEL_SELECTION1_UNSUPPORTED,
  W_SEED_PARALLEL_SELECTION1_ALIAS,
  W_SEED_PARALLEL_SELECTION1_CAPACITY,
} w_seed_parallel_selection1_status;

/* These u32 fields are canonical HIR record indices, not a materialized Task
 * handle or target ABI. A later runtime representation remains target-native
 * and may be completely elided when task identity does not escape. */
typedef struct {
  uint32_t call_index;
  uint32_t function_index;
  uint32_t call_instruction;
  uint32_t launch_binding;
  uint32_t join_binding;
} w_seed_parallel_selection1_task;

typedef struct {
  size_t tasks;
} w_seed_parallel_selection1_counts;

typedef struct {
  w_seed_parallel_selection1_task *tasks;
  size_t task_capacity;
} w_seed_parallel_selection1_output;

typedef struct {
  const w_seed_parallel_selection1_task *tasks;
  size_t task_count;
  size_t task_capacity;
  uint32_t root_function_index;
  w_seed_hir0_call_placement_kind placement;
  char domain_identity[sizeof(W_SEED_FRONTEND_DOMAIN_IDENTITY)];
  w_seed_frontend_domain_mode domain_mode;
  uint32_t domain_capabilities;
  uint8_t hir_semantic_digest[32];
  uint8_t semantic_digest[32];
} w_seed_parallel_selection1_program;

typedef struct {
  w_seed_parallel_selection1_status status;
  w_seed_parallel_selection1_counts required;
  w_seed_parallel_selection1_counts written;
  char schema[sizeof(W_SEED_PARALLEL_SELECTION1_SCHEMA_VERSION)];
  uint32_t root_function_index;
  w_seed_hir0_call_placement_kind placement;
  char domain_identity[sizeof(W_SEED_FRONTEND_DOMAIN_IDENTITY)];
  w_seed_frontend_domain_mode domain_mode;
  uint32_t domain_capabilities;
  uint8_t hir_semantic_digest[32];
  uint8_t semantic_digest[32];
} w_seed_parallel_selection1_result;

w_seed_parallel_selection1_status w_seed_parallel_selection1_measure(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    w_seed_parallel_selection1_counts *counts,
    w_seed_parallel_selection1_result *result);

w_seed_parallel_selection1_status w_seed_parallel_selection1_run(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_output *output,
    w_seed_parallel_selection1_result *result);

bool w_seed_parallel_selection1_program_from_output(
    const w_seed_parallel_selection1_output *output,
    const w_seed_parallel_selection1_result *result,
    w_seed_parallel_selection1_program *program);

bool w_seed_parallel_selection1_verify(
    const w_seed_hir0_program *hir_program,
    const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_program *selection,
    const w_seed_parallel_selection1_result *result);

#ifdef __cplusplus
}
#endif

#endif
