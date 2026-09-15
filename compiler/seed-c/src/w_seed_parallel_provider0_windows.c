#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0602
#endif

#include "w_seed_parallel_provider0_platform.h"

#if defined(_WIN32) && defined(_WIN64)

#include <windows.h>

#include <string.h>

typedef struct {
  const w_seed_parallel_provider0_internal_job *job;
  size_t task_index;
  volatile LONG *start_state;
  volatile LONG *active;
  volatile LONG *maximum_active;
  volatile LONG *ready;
  volatile LONG *release;
  LONG wave_size;
  int64_t value;
  bool invoked;
  bool succeeded;
} parallel_windows_task;

typedef struct {
  const w_seed_parallel_platform1_job *job;
  size_t task_index;
  volatile LONG *start_state;
  volatile LONG *active;
  volatile LONG *maximum_active;
  volatile LONG *ready;
  volatile LONG *release;
  LONG wave_size;
  w_seed_parallel_platform1_completion completion;
  bool invoked;
  bool valid;
} parallel_windows_task1;

static bool completion_valid(
    const w_seed_parallel_platform1_completion *completion) {
  if (completion == NULL) return false;
  switch (completion->kind) {
    case W_SEED_PARALLEL_PLATFORM1_COMPLETION_SUCCESS:
      return completion->error_code == 0u && completion->cancel_reason == 0u &&
             completion->error_case_ordinal == 0u;
    case W_SEED_PARALLEL_PLATFORM1_COMPLETION_ERROR:
      return completion->success_value == 0 && completion->error_code != 0u &&
             completion->cancel_reason == 0u;
    case W_SEED_PARALLEL_PLATFORM1_COMPLETION_CANCELED:
      return completion->success_value == 0 && completion->error_code == 0u &&
             completion->cancel_reason != 0u &&
             completion->error_case_ordinal == 0u;
    default:
      return false;
  }
}

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
  const LONG ready = InterlockedIncrement(task->ready);
  if (ready == task->wave_size)
    (void)InterlockedExchange(task->release, 1);
  while (InterlockedCompareExchange(task->release, 0, 0) == 0)
    (void)SwitchToThread();
  task->invoked = true;
  task->succeeded =
      task->job->invoke(task->job->context, task->task_index, &task->value);
  (void)InterlockedDecrement(task->active);
  return task->succeeded ? 0u : 1u;
}

static DWORD WINAPI parallel_windows_entry1(void *raw) {
  parallel_windows_task1 *task = (parallel_windows_task1 *)raw;
  if (task == NULL || task->job == NULL || task->job->invoke == NULL ||
      task->start_state == NULL)
    return 1u;
  LONG start = 0;
  while ((start = InterlockedCompareExchange(task->start_state, 0, 0)) == 0)
    (void)SwitchToThread();
  if (start < 0) return 1u;
  const LONG active = InterlockedIncrement(task->active);
  update_maximum(task->maximum_active, active);
  const LONG ready = InterlockedIncrement(task->ready);
  if (ready == task->wave_size)
    (void)InterlockedExchange(task->release, 1);
  while (InterlockedCompareExchange(task->release, 0, 0) == 0)
    (void)SwitchToThread();
  task->invoked = true;
  task->valid = task->job->invoke(task->job->context, task->task_index,
                                  &task->completion) &&
                completion_valid(&task->completion);
  (void)InterlockedDecrement(task->active);
  return task->valid ? 0u : 1u;
}

static w_seed_parallel_provider0_platform_status execute_wave(
    const w_seed_parallel_provider0_internal_job *job, size_t first,
    size_t count, int64_t *values,
    uint32_t *started_count, uint32_t *completed_count, volatile LONG *active,
    volatile LONG *maximum_active) {
  volatile LONG start_state = 0;
  volatile LONG ready = 0;
  volatile LONG release = 0;
  HANDLE threads[2] = {NULL, NULL};
  parallel_windows_task tasks[2];
  for (size_t index = 0u; index < count; index += 1u) {
    tasks[index] = (parallel_windows_task){
        job,          first + index, &start_state, active,
        maximum_active, &ready,      &release,      (LONG)count,
        0,            false,         false};
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

static w_seed_parallel_provider0_platform_status execute_wave1(
    const w_seed_parallel_platform1_job *job, size_t first, size_t count,
    w_seed_parallel_platform1_completion *completions,
    w_seed_parallel_platform1_receipt *receipt, volatile LONG *active,
    volatile LONG *maximum_active) {
  volatile LONG start_state = 0;
  volatile LONG ready = 0;
  volatile LONG release = 0;
  HANDLE threads[2] = {NULL, NULL};
  parallel_windows_task1 tasks[2];
  (void)memset(tasks, 0, sizeof(tasks));
  for (size_t index = 0u; index < count; index += 1u) {
    tasks[index].job = job;
    tasks[index].task_index = first + index;
    tasks[index].start_state = &start_state;
    tasks[index].active = active;
    tasks[index].maximum_active = maximum_active;
    tasks[index].ready = &ready;
    tasks[index].release = &release;
    tasks[index].wave_size = (LONG)count;
    threads[index] = CreateThread(NULL, 0u, parallel_windows_entry1,
                                  &tasks[index], 0u, NULL);
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
  bool valid = true;
  for (size_t index = 0u; index < count; index += 1u) {
    if (tasks[index].invoked) receipt->started_count += 1u;
    if (tasks[index].valid) {
      completions[first + index] = tasks[index].completion;
      receipt->settled_count += 1u;
    } else {
      valid = false;
    }
    (void)CloseHandle(threads[index]);
  }
  return valid ? W_SEED_PARALLEL_PROVIDER0_PLATFORM_OK
               : W_SEED_PARALLEL_PROVIDER0_PLATFORM_TASK_FAILURE;
}

w_seed_parallel_provider0_platform_status
w_seed_parallel_provider0_platform_execute(
    const w_seed_parallel_provider0_internal_job *job, size_t job_count,
    uint32_t provider_capacity, int64_t *values, uint32_t *started_count,
    uint32_t *completed_count, uint32_t *maximum_active,
    w_seed_parallel_provider0_kind *provider_kind) {
  if (job == NULL || job->invoke == NULL || values == NULL ||
      started_count == NULL ||
      completed_count == NULL || maximum_active == NULL ||
      provider_kind == NULL || job_count == 0u || job_count > UINT32_MAX ||
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
      if (!job->invoke(job->context, index, &value))
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
        execute_wave(job, first, count, values + first, &wave_started,
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

w_seed_parallel_provider0_platform_status w_seed_parallel_platform1_execute(
    const w_seed_parallel_platform1_job *job, size_t job_count,
    uint32_t provider_capacity,
    w_seed_parallel_platform1_completion *completions,
    w_seed_parallel_platform1_receipt *receipt,
    w_seed_parallel_provider0_kind *provider_kind) {
  if (job == NULL || job->invoke == NULL || completions == NULL ||
      receipt == NULL || provider_kind == NULL || job_count == 0u ||
      job_count > UINT32_MAX ||
      (provider_capacity != 1u && provider_capacity != 2u))
    return W_SEED_PARALLEL_PROVIDER0_PLATFORM_PROVIDER_FAILURE;
  (void)memset(receipt, 0, sizeof(*receipt));
  receipt->cancellation_source_index = UINT32_MAX;
  *provider_kind = W_SEED_PARALLEL_PROVIDER0_KIND_WINDOWS_KERNEL32;

  volatile LONG active = 0;
  volatile LONG maximum = 0;
  size_t first = 0u;
  while (first < job_count && !receipt->cancellation_requested) {
    const size_t count = provider_capacity == 1u
                             ? 1u
                             : (job_count - first > 2u ? 2u
                                                        : job_count - first);
    w_seed_parallel_provider0_platform_status status;
    if (provider_capacity == 1u) {
      w_seed_parallel_platform1_completion completion = {0};
      receipt->started_count += 1u;
      if (!job->invoke(job->context, first, &completion) ||
          !completion_valid(&completion))
        return W_SEED_PARALLEL_PROVIDER0_PLATFORM_TASK_FAILURE;
      completions[first] = completion;
      receipt->settled_count += 1u;
      maximum = 1;
      status = W_SEED_PARALLEL_PROVIDER0_PLATFORM_OK;
    } else {
      status = execute_wave1(job, first, count, completions, receipt, &active,
                             &maximum);
    }
    if (status != W_SEED_PARALLEL_PROVIDER0_PLATFORM_OK) return status;
    for (size_t index = first; index < first + count; index += 1u) {
      if (completions[index].kind ==
              W_SEED_PARALLEL_PLATFORM1_COMPLETION_ERROR ||
          completions[index].kind ==
              W_SEED_PARALLEL_PLATFORM1_COMPLETION_CANCELED) {
        receipt->cancellation_requested = true;
        receipt->cancellation_source_index = (uint32_t)index;
        break;
      }
    }
    first += count;
  }

  if (receipt->cancellation_requested) {
    const w_seed_parallel_platform1_completion *source =
        &completions[receipt->cancellation_source_index];
    const uint32_t reason =
        source->kind == W_SEED_PARALLEL_PLATFORM1_COMPLETION_ERROR
            ? W_SEED_PARALLEL_PLATFORM1_CANCEL_FAIL_FAST
            : source->cancel_reason;
    while (first < job_count) {
      completions[first] = (w_seed_parallel_platform1_completion){
          W_SEED_PARALLEL_PLATFORM1_COMPLETION_CANCELED, 0, 0u, reason, 0u};
      receipt->canceled_before_start_count += 1u;
      first += 1u;
    }
  }
  receipt->maximum_active =
      provider_capacity == 1u
          ? (receipt->started_count == 0u ? 0u : 1u)
          : (uint32_t)InterlockedCompareExchange(&maximum, 0, 0);
  return W_SEED_PARALLEL_PROVIDER0_PLATFORM_OK;
}

#else

w_seed_parallel_provider0_platform_status
w_seed_parallel_provider0_platform_execute(
    const w_seed_parallel_provider0_internal_job *job, size_t job_count,
    uint32_t provider_capacity, int64_t *values, uint32_t *started_count,
    uint32_t *completed_count, uint32_t *maximum_active,
    w_seed_parallel_provider0_kind *provider_kind) {
  (void)job;
  (void)job_count;
  (void)provider_capacity;
  (void)values;
  (void)started_count;
  (void)completed_count;
  (void)maximum_active;
  (void)provider_kind;
  return W_SEED_PARALLEL_PROVIDER0_PLATFORM_UNSUPPORTED;
}

w_seed_parallel_provider0_platform_status w_seed_parallel_platform1_execute(
    const w_seed_parallel_platform1_job *job, size_t job_count,
    uint32_t provider_capacity,
    w_seed_parallel_platform1_completion *completions,
    w_seed_parallel_platform1_receipt *receipt,
    w_seed_parallel_provider0_kind *provider_kind) {
  (void)job;
  (void)job_count;
  (void)provider_capacity;
  (void)completions;
  (void)receipt;
  (void)provider_kind;
  return W_SEED_PARALLEL_PROVIDER0_PLATFORM_UNSUPPORTED;
}

#endif
