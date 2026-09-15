#ifndef W_SEED_PARALLEL_LIFECYCLE1_H
#define W_SEED_PARALLEL_LIFECYCLE1_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_parallel_provider1.h"
#include "w_seed_task_lifecycle0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PARLIFE1 binds successful verified PARPROV1 outcomes to TASKLIFE1.  It is a
 * compiler/component bridge, not a public scheduler or Task ABI. */
#define W_SEED_PARALLEL_LIFECYCLE1_SCHEMA_VERSION \
  "w-seed-parallel-lifecycle1-1"

typedef enum {
  W_SEED_PARALLEL_LIFECYCLE1_OK = 0,
  W_SEED_PARALLEL_LIFECYCLE1_INVALID,
  W_SEED_PARALLEL_LIFECYCLE1_CAPACITY,
  W_SEED_PARALLEL_LIFECYCLE1_ALIAS,
  W_SEED_PARALLEL_LIFECYCLE1_LIFECYCLE,
} w_seed_parallel_lifecycle1_status;

typedef struct {
  const w_seed_parallel_provider1_input *provider_input;
  const w_seed_parallel_provider1_outcome *provider_outcomes;
  size_t provider_outcome_count;
  const w_seed_parallel_provider1_result *provider_result;
  uint32_t scope_generation;
} w_seed_parallel_lifecycle1_input;

typedef struct {
  size_t task_specs;
  size_t events;
  size_t task_records;
  size_t trace_events;
} w_seed_parallel_lifecycle1_counts;

/* Every member is scratch and may change after an attempted run or verify. */
typedef struct {
  w_seed_task_lifecycle0_task_spec *task_specs;
  size_t task_spec_capacity;
  w_seed_task_lifecycle0_event *events;
  size_t event_capacity;
  w_seed_task_lifecycle0_task_record *reducer_tasks;
  size_t reducer_task_capacity;
} w_seed_parallel_lifecycle1_workspace;

typedef struct {
  w_seed_task_lifecycle0_task_record *tasks;
  size_t task_capacity;
  w_seed_task_lifecycle0_event *trace;
  size_t trace_capacity;
} w_seed_parallel_lifecycle1_output;

typedef struct {
  w_seed_parallel_lifecycle1_status status;
  w_seed_parallel_lifecycle1_counts required;
  w_seed_parallel_lifecycle1_counts written;
  char schema[sizeof(W_SEED_PARALLEL_LIFECYCLE1_SCHEMA_VERSION)];
  uint32_t task_count;
  uint32_t event_count;
  uint8_t provider_outcome_digest[32];
  w_seed_task_lifecycle1_result lifecycle;
} w_seed_parallel_lifecycle1_result;

w_seed_parallel_lifecycle1_status w_seed_parallel_lifecycle1_measure(
    const w_seed_parallel_lifecycle1_input *input,
    w_seed_parallel_lifecycle1_counts *counts,
    w_seed_parallel_lifecycle1_result *result);

w_seed_parallel_lifecycle1_status w_seed_parallel_lifecycle1_run(
    const w_seed_parallel_lifecycle1_input *input,
    const w_seed_parallel_lifecycle1_workspace *workspace,
    const w_seed_parallel_lifecycle1_output *output,
    w_seed_parallel_lifecycle1_result *result);

bool w_seed_parallel_lifecycle1_verify(
    const w_seed_parallel_lifecycle1_input *input,
    const w_seed_parallel_lifecycle1_workspace *workspace,
    const w_seed_parallel_lifecycle1_output *output,
    const w_seed_parallel_lifecycle1_result *result);

#ifdef __cplusplus
}
#endif

#endif
