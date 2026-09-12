#ifndef W_SEED_NATIVE_BENCHMARK_H
#define W_SEED_NATIVE_BENCHMARK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

/* This is a bounded, caller-owned measurement adapter. It launches one fresh
 * child for every warmup and sample. It is not the executable benchmark
 * catalog publisher and it does not write an artifact or result file. */
#define W_SEED_NATIVE_BENCHMARK_ABI_VERSION "w-seed-native-benchmark-2"

/* The limits are deliberately small enough to make all staging storage
 * bounded and predictable. CreateProcessW accepts at most 32,767 command-line
 * characters including its terminating NUL. */
#define W_SEED_NATIVE_BENCHMARK_MAX_ARGUMENTS 64u
#define W_SEED_NATIVE_BENCHMARK_MAX_SAMPLES 1001u
#define W_SEED_NATIVE_BENCHMARK_MAX_COMMAND_LINE_CHARS 32767u
#define W_SEED_NATIVE_BENCHMARK_MAX_CAPTURE_BYTES 65536u
#define W_SEED_NATIVE_BENCHMARK_MAX_TIMEOUT_MS 120000u

typedef enum {
  W_SEED_NATIVE_BENCHMARK_OK = 0,
  W_SEED_NATIVE_BENCHMARK_UNSUPPORTED_PLATFORM,
  W_SEED_NATIVE_BENCHMARK_INVALID_ARGUMENT,
  W_SEED_NATIVE_BENCHMARK_AMBIGUOUS_PATH,
  W_SEED_NATIVE_BENCHMARK_PATH_NOT_EXECUTABLE,
  W_SEED_NATIVE_BENCHMARK_LIMIT,
  W_SEED_NATIVE_BENCHMARK_COMMAND_LINE_TOO_LONG,
  W_SEED_NATIVE_BENCHMARK_PIPE,
  W_SEED_NATIVE_BENCHMARK_PROCESS,
  W_SEED_NATIVE_BENCHMARK_THREAD,
  W_SEED_NATIVE_BENCHMARK_WAIT,
  W_SEED_NATIVE_BENCHMARK_TIMEOUT,
  W_SEED_NATIVE_BENCHMARK_CAPTURE,
  W_SEED_NATIVE_BENCHMARK_METRICS,
  W_SEED_NATIVE_BENCHMARK_ORACLE_MISMATCH,
  W_SEED_NATIVE_BENCHMARK_TREE_INCOMPLETE,
} w_seed_native_benchmark_status;

/* All values are published only after one child has exited, its Job Object is
 * empty, and both output readers have joined. The direct fields describe the
 * root process only. Job fields include all descendants that stayed in the
 * same containment job. `peak_direct_working_set_bytes` is the direct root's
 * Windows RSS-equivalent peak working set; `peak_job_commit_bytes` is Job
 * Object peak committed memory, not an RSS value. */
typedef struct {
  uint64_t wall_time_ns;
  uint64_t direct_process_user_cpu_time_ns;
  uint64_t direct_process_kernel_cpu_time_ns;
  uint64_t direct_process_cpu_time_ns;
  uint64_t job_user_cpu_time_ns;
  uint64_t job_kernel_cpu_time_ns;
  uint64_t job_cpu_time_ns;
  uint64_t peak_direct_working_set_bytes;
  uint64_t peak_job_commit_bytes;
  uint32_t exit_code;
  uint32_t stdout_bytes;
  uint32_t stderr_bytes;
} w_seed_native_benchmark_sample;

/* Every pointer is borrowed for the duration of
 * w_seed_native_benchmark_run and is never retained. The executable and
 * optional working directory must be explicit absolute Windows paths to
 * regular non-reparse files/directories. Arguments are individual values, not
 * shell text; the implementation quotes them into a CreateProcessW command
 * line. `stdout_buffer` and `stderr_buffer` are reused for every child and
 * must remain writable for the duration of the call. */
typedef struct {
  const wchar_t *executable_path;
  const wchar_t *working_directory;
  const wchar_t *const *arguments;
  size_t argument_count;
  uint32_t warmup_count;
  uint32_t sample_count;
  uint32_t timeout_ms;
  uint8_t *stdout_buffer;
  size_t stdout_capacity;
  uint8_t *stderr_buffer;
  size_t stderr_capacity;
  bool oracle_enabled;
  uint32_t expected_exit_code;
  const uint8_t *expected_stdout;
  size_t expected_stdout_bytes;
  const uint8_t *expected_stderr;
  size_t expected_stderr_bytes;
} w_seed_native_benchmark_config;

typedef struct {
  w_seed_native_benchmark_status status;
  uint32_t warmups_completed;
  uint32_t samples_completed;
  /* UINT32_MAX means that a warmup (rather than a measured sample) failed. */
  uint32_t failed_sample_index;
  uint32_t os_error;
} w_seed_native_benchmark_result;

/* Validate all limits and paths, run warmups followed by a fixed sample count,
 * and fill the caller-owned sample array. A configured oracle is checked for
 * every warmup and sample before that result is accepted. No child is reused.
 * Each timeout is one QPC deadline covering launch, root execution, Job Object
 * descendants, and capture. A root is accepted only after its Job Object is
 * empty. The sample array is all-or-nothing. The result describes ordinary
 * failures, but an invalid alias involving the result itself is rejected
 * without writing through that alias. On non-Windows hosts the function
 * returns UNSUPPORTED_PLATFORM. */
w_seed_native_benchmark_status w_seed_native_benchmark_run(
    const w_seed_native_benchmark_config *config,
    w_seed_native_benchmark_sample *samples,
    w_seed_native_benchmark_result *result);

const char *w_seed_native_benchmark_status_name(
    w_seed_native_benchmark_status status);

#ifdef __cplusplus
}
#endif

#endif
