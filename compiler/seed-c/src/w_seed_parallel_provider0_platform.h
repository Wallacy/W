#ifndef W_SEED_PARALLEL_PROVIDER0_PLATFORM_H
#define W_SEED_PARALLEL_PROVIDER0_PLATFORM_H

#include "w_seed_parallel_provider0.h"

typedef bool (*w_seed_parallel_provider0_internal_task_fn)(void *context,
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

w_seed_parallel_provider0_platform_status
w_seed_parallel_provider0_platform_execute(
    const w_seed_parallel_provider0_internal_job *jobs, size_t job_count,
    uint32_t provider_capacity, int64_t *values, uint32_t *started_count,
    uint32_t *completed_count, uint32_t *maximum_active,
    w_seed_parallel_provider0_kind *provider_kind);

#endif
