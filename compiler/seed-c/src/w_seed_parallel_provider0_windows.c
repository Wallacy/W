#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0602
#endif

#include "w_seed_parallel_provider0_platform.h"

#if defined(_WIN32) && defined(_WIN64)

#include <windows.h>

typedef struct {
  const w_seed_parallel_provider0_internal_job *job;
  volatile LONG *start_state;
  volatile LONG *active;
  volatile LONG *maximum_active;
  LONG wave_size;
  int64_t value;
  bool invoked;
  bool succeeded;
} parallel_windows_task;

static void update_maximum(volatile LONG *maximum, LONG observed) {
  LONG current = InterlockedCompareExchange(maximum, 0, 0);
  while (current < observed) {
    const LONG replaced = InterlockedCompareExchange(maximum, observed, current);
    if (replaced == current) return;
    current = replaced;
  }
}

static DWORD WINAPI parallel_windows_entry(void *raw) {
  parallel_windows_task *task = (parallel_windows_task *)raw;
  if (task == NULL || task->job == NULL || task->start_state == NULL)
    return 1u;
  LONG start = 0;
  while ((start = InterlockedCompareExchange(task->start_state, 0, 0)) == 0)
    (void)SwitchToThread();
  if (start < 0) return 1u;
  const LONG active = InterlockedIncrement(task->active);
  update_maximum(task->maximum_active, active);
  while (task->wave_size > 1 &&
         InterlockedCompareExchange(task->active, 0, 0) < task->wave_size)
    (void)SwitchToThread();
  task->invoked = true;
  task->succeeded = task->job->invoke(task->job->context, &task->value);
  (void)InterlockedDecrement(task->active);
  return task->succeeded ? 0u : 1u;
}

static w_seed_parallel_provider0_platform_status execute_wave(
    const w_seed_parallel_provider0_internal_job *jobs, size_t count,
    int64_t *values,
    uint32_t *started_count, uint32_t *completed_count, volatile LONG *active,
    volatile LONG *maximum_active) {
  volatile LONG start_state = 0;
  HANDLE threads[2] = {NULL, NULL};
  parallel_windows_task tasks[2];
  for (size_t index = 0u; index < count; index += 1u) {
    tasks[index] = (parallel_windows_task){
        &jobs[index], &start_state, active, maximum_active, (LONG)count, 0,
        false, false};
    threads[index] =
        CreateThread(NULL, 0u, parallel_windows_entry, &tasks[index], 0u, NULL);
    if (threads[index] == NULL) {
      (void)InterlockedExchange(&start_state, -1);
      if (index != 0u)
        (void)WaitForMultipleObjects((DWORD)index, threads, TRUE, INFINITE);
      for (size_t close = 0u; close < index; close += 1u)
        (void)CloseHandle(threads[close]);
      return W_SEED_PARALLEL_PROVIDER0_PLATFORM_PROVIDER_FAILURE;
    }
  }
  (void)InterlockedExchange(&start_state, 1);
  if (WaitForMultipleObjects((DWORD)count, threads, TRUE, INFINITE) !=
      WAIT_OBJECT_0) {
    (void)WaitForMultipleObjects((DWORD)count, threads, TRUE, INFINITE);
    for (size_t index = 0u; index < count; index += 1u)
      (void)CloseHandle(threads[index]);
    return W_SEED_PARALLEL_PROVIDER0_PLATFORM_PROVIDER_FAILURE;
  }
  bool ok = true;
  for (size_t index = 0u; index < count; index += 1u) {
    if (tasks[index].invoked) *started_count += 1u;
    if (tasks[index].succeeded) {
      values[index] = tasks[index].value;
      *completed_count += 1u;
    } else {
      ok = false;
    }
    (void)CloseHandle(threads[index]);
  }
  return ok ? W_SEED_PARALLEL_PROVIDER0_PLATFORM_OK
            : W_SEED_PARALLEL_PROVIDER0_PLATFORM_TASK_FAILURE;
}

w_seed_parallel_provider0_platform_status
w_seed_parallel_provider0_platform_execute(
    const w_seed_parallel_provider0_internal_job *jobs, size_t job_count,
    uint32_t provider_capacity, int64_t *values, uint32_t *started_count,
    uint32_t *completed_count, uint32_t *maximum_active,
    w_seed_parallel_provider0_kind *provider_kind) {
  if (jobs == NULL || values == NULL || started_count == NULL ||
      completed_count == NULL || maximum_active == NULL ||
      provider_kind == NULL || job_count == 0u ||
      job_count > W_SEED_PARALLEL_PROVIDER0_MAX_TASKS ||
      (provider_capacity != 1u && provider_capacity != 2u))
    return W_SEED_PARALLEL_PROVIDER0_PLATFORM_PROVIDER_FAILURE;
  *started_count = 0u;
  *completed_count = 0u;
  *maximum_active = 0u;
  *provider_kind = W_SEED_PARALLEL_PROVIDER0_KIND_WINDOWS_KERNEL32;
  if (provider_capacity == 1u) {
    for (size_t index = 0u; index < job_count; index += 1u) {
      int64_t value = 0;
      *started_count += 1u;
      if (!jobs[index].invoke(jobs[index].context, &value))
        return W_SEED_PARALLEL_PROVIDER0_PLATFORM_TASK_FAILURE;
      values[index] = value;
      *completed_count += 1u;
    }
    *maximum_active = 1u;
    return W_SEED_PARALLEL_PROVIDER0_PLATFORM_OK;
  }

  volatile LONG active = 0;
  volatile LONG maximum = 0;
  size_t first = 0u;
  while (first < job_count) {
    const size_t count = job_count - first > 2u ? 2u : job_count - first;
    uint32_t wave_started = 0u;
    uint32_t wave_completed = 0u;
    const w_seed_parallel_provider0_platform_status wave_status =
        execute_wave(jobs + first, count, values + first, &wave_started,
                     &wave_completed, &active, &maximum);
    if (wave_status != W_SEED_PARALLEL_PROVIDER0_PLATFORM_OK) {
      *started_count += wave_started;
      *completed_count += wave_completed;
      return wave_status;
    }
    *started_count += wave_started;
    *completed_count += wave_completed;
    first += count;
  }
  *maximum_active = (uint32_t)InterlockedCompareExchange(&maximum, 0, 0);
  return W_SEED_PARALLEL_PROVIDER0_PLATFORM_OK;
}

#else

w_seed_parallel_provider0_platform_status
w_seed_parallel_provider0_platform_execute(
    const w_seed_parallel_provider0_internal_job *jobs, size_t job_count,
    uint32_t provider_capacity, int64_t *values, uint32_t *started_count,
    uint32_t *completed_count, uint32_t *maximum_active,
    w_seed_parallel_provider0_kind *provider_kind) {
  (void)jobs;
  (void)job_count;
  (void)provider_capacity;
  (void)values;
  (void)started_count;
  (void)completed_count;
  (void)maximum_active;
  (void)provider_kind;
  return W_SEED_PARALLEL_PROVIDER0_PLATFORM_UNSUPPORTED;
}

#endif
