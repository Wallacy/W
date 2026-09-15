#ifndef W_SEED_PARALLEL_TYPED_LIFECYCLE1_H
#define W_SEED_PARALLEL_TYPED_LIFECYCLE1_H

#include "w_seed_parallel_typed_binding1.h"
#include "w_seed_task_lifecycle0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A separate target-neutral bridge from the verified PARBIND1 witness into
 * TASKLIFE1. It preserves nominal error identity outside TASKLIFE1's legacy
 * generic error code and does not widen either upstream contract. */
#define W_SEED_PARALLEL_TYPED_LIFECYCLE1_SCHEMA_VERSION \
  "w-seed-parallel-typed-lifecycle1-1"

typedef enum {
  W_SEED_PARALLEL_TYPED_LIFECYCLE1_OK = 0,
  W_SEED_PARALLEL_TYPED_LIFECYCLE1_INVALID,
  W_SEED_PARALLEL_TYPED_LIFECYCLE1_CAPACITY,
  W_SEED_PARALLEL_TYPED_LIFECYCLE1_ALIAS,
  W_SEED_PARALLEL_TYPED_LIFECYCLE1_UPSTREAM,
  W_SEED_PARALLEL_TYPED_LIFECYCLE1_LIFECYCLE,
} w_seed_parallel_typed_lifecycle1_status;

typedef struct {
  const w_seed_parallel_typed_binding1_input *binding_input;
  const w_seed_parallel_typed_binding1_workspace *binding_workspace;
  const w_seed_parallel_typed_binding1_output *binding_output;
  const w_seed_parallel_typed_binding1_result *binding_result;
  uint32_t scope_generation;
} w_seed_parallel_typed_lifecycle1_input;

typedef struct {
  size_t task_specs;
  size_t events;
  size_t reducer_tasks;
  size_t tasks;
  size_t trace_events;
  size_t typed_records;
} w_seed_parallel_typed_lifecycle1_counts;

/* Scratch may change after an attempted run or verify. */
typedef struct {
  w_seed_task_lifecycle0_task_spec *task_specs;
  size_t task_spec_capacity;
  w_seed_task_lifecycle0_event *events;
  size_t event_capacity;
  w_seed_task_lifecycle0_task_record *reducer_tasks;
  size_t reducer_task_capacity;
} w_seed_parallel_typed_lifecycle1_workspace;

/* The generic reducer state and the nominal semantic record remain separate.
 * TASKLIFE1_ERROR_BODY is only the reducer's internal category; `semantic`
 * retains the exact W enum/case identity. */
typedef struct {
  w_seed_parallel_typed_binding1_record semantic;
  w_seed_task_lifecycle0_task_state state;
  uint32_t transition_count;
} w_seed_parallel_typed_lifecycle1_record;

typedef struct {
  w_seed_task_lifecycle0_task_record *tasks;
  size_t task_capacity;
  w_seed_task_lifecycle0_event *trace;
  size_t trace_capacity;
  w_seed_parallel_typed_lifecycle1_record *typed_records;
  size_t typed_record_capacity;
} w_seed_parallel_typed_lifecycle1_output;

typedef struct {
  w_seed_parallel_typed_lifecycle1_status status;
  w_seed_parallel_typed_lifecycle1_counts required;
  w_seed_parallel_typed_lifecycle1_counts written;
  char schema[sizeof(W_SEED_PARALLEL_TYPED_LIFECYCLE1_SCHEMA_VERSION)];
  uint32_t task_count;
  uint32_t event_count;
  uint32_t scope_generation;
  uint32_t primary_error_task;
  w_seed_parallel_typed_binding1_outcome_kind scope_outcome;
  w_seed_parallel_typed_binding1_error_identity error_identity;
  uint8_t binding_semantic_digest[32];
  uint8_t binding_provenance_digest[32];
  uint8_t semantic_digest[32];
  uint8_t provenance_digest[32];
  w_seed_task_lifecycle1_result lifecycle;
} w_seed_parallel_typed_lifecycle1_result;

w_seed_parallel_typed_lifecycle1_status
w_seed_parallel_typed_lifecycle1_measure(
    const w_seed_parallel_typed_lifecycle1_input *input,
    w_seed_parallel_typed_lifecycle1_counts *counts,
    w_seed_parallel_typed_lifecycle1_result *result);

w_seed_parallel_typed_lifecycle1_status w_seed_parallel_typed_lifecycle1_run(
    const w_seed_parallel_typed_lifecycle1_input *input,
    const w_seed_parallel_typed_lifecycle1_workspace *workspace,
    const w_seed_parallel_typed_lifecycle1_output *output,
    w_seed_parallel_typed_lifecycle1_result *result);

bool w_seed_parallel_typed_lifecycle1_verify(
    const w_seed_parallel_typed_lifecycle1_input *input,
    const w_seed_parallel_typed_lifecycle1_workspace *workspace,
    const w_seed_parallel_typed_lifecycle1_output *output,
    const w_seed_parallel_typed_lifecycle1_result *result);

#ifdef __cplusplus
}
#endif

#endif
