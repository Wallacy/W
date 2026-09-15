#ifndef W_SEED_PARALLEL_PROVIDER0_PLATFORM_H
#define W_SEED_PARALLEL_PROVIDER0_PLATFORM_H

#include "w_seed_parallel_provider0.h"

typedef bool (*w_seed_parallel_provider0_internal_task_fn)(void *context,
                                                           size_t task_index,
                                                           int64_t *value);

typedef struct {
  w_seed_parallel_provider0_internal_task_fn invoke;
  void *context;
} w_seed_parallel_provider0_internal_job;

typedef enum {
  W_SEED_PARALLEL_PROVIDER0_PLATFORM_OK = 0,
  W_SEED_PARALLEL_PROVIDER0_PLATFORM_UNSUPPORTED,
  W_SEED_PARALLEL_PROVIDER0_PLATFORM_PROVIDER_FAILURE,
  W_SEED_PARALLEL_PROVIDER0_PLATFORM_TASK_FAILURE,
} w_seed_parallel_provider0_platform_status;

/* PLATFORM1 is a private provider primitive. A completion is a value returned
 * by a task body, not a thread exit code. Panic/fault containment is
 * deliberately absent and must not be encoded as ERROR or CANCELED. */
typedef enum {
  W_SEED_PARALLEL_PLATFORM1_COMPLETION_SUCCESS = 1,
  W_SEED_PARALLEL_PLATFORM1_COMPLETION_ERROR,
  W_SEED_PARALLEL_PLATFORM1_COMPLETION_CANCELED,
} w_seed_parallel_platform1_completion_kind;

#define W_SEED_PARALLEL_PLATFORM1_CANCEL_FAIL_FAST 1u

typedef struct {
  w_seed_parallel_platform1_completion_kind kind;
  int64_t success_value;
  uint32_t error_code;
  uint32_t cancel_reason;
} w_seed_parallel_platform1_completion;

typedef bool (*w_seed_parallel_platform1_task_fn)(
    void *context, size_t task_index,
    w_seed_parallel_platform1_completion *completion);

typedef struct {
  w_seed_parallel_platform1_task_fn invoke;
  void *context;
} w_seed_parallel_platform1_job;

typedef struct {
  uint32_t started_count;
  uint32_t settled_count;
  uint32_t canceled_before_start_count;
  uint32_t maximum_active;
  uint32_t cancellation_source_index;
  bool cancellation_requested;
} w_seed_parallel_platform1_receipt;

w_seed_parallel_provider0_platform_status
w_seed_parallel_provider0_platform_execute(
    const w_seed_parallel_provider0_internal_job *job, size_t job_count,
    uint32_t provider_capacity, int64_t *values, uint32_t *started_count,
    uint32_t *completed_count, uint32_t *maximum_active,
    w_seed_parallel_provider0_kind *provider_kind);

w_seed_parallel_provider0_platform_status
w_seed_parallel_platform1_execute(
    const w_seed_parallel_platform1_job *job, size_t job_count,
    uint32_t provider_capacity,
    w_seed_parallel_platform1_completion *completions,
    w_seed_parallel_platform1_receipt *receipt,
    w_seed_parallel_provider0_kind *provider_kind);

#endif
