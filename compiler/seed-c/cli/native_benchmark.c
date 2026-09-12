#include "w_seed_native_benchmark.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)

int main(void) {
  (void)fprintf(stderr,
                "w_seed_native_benchmark: Windows is required\n");
  return 2;
}

#else

#include <windows.h>

typedef struct {
  const wchar_t *executable_path;
  const wchar_t *working_directory;
  const wchar_t *arguments[W_SEED_NATIVE_BENCHMARK_MAX_ARGUMENTS];
  size_t argument_count;
  uint32_t warmup_count;
  uint32_t sample_count;
  uint32_t timeout_ms;
  bool oracle_enabled;
  uint32_t expected_exit_code;
  uint8_t expected_stdout[W_SEED_NATIVE_BENCHMARK_MAX_CAPTURE_BYTES];
  size_t expected_stdout_bytes;
  uint8_t expected_stderr[W_SEED_NATIVE_BENCHMARK_MAX_CAPTURE_BYTES];
  size_t expected_stderr_bytes;
} native_benchmark_options;

static void print_error(w_seed_native_benchmark_status status,
                        uint32_t os_error) {
  (void)printf(
      "{\"schema\":\"w-native-benchmark/2\",\"status\":\"error\","
      "\"error\":\"%s\",\"osError\":%" PRIu32 "}\n",
      w_seed_native_benchmark_status_name(status), os_error);
}

static void print_usage(void) {
  (void)printf(
      "usage: w_seed_native_benchmark --exe <absolute-path> [options]\n"
      "options: --cwd <absolute-dir> --arg <value> --warmup <n> "
      "--samples <n> --timeout-ms <n> --expect-exit <n> "
      "--expect-stdout-hex <bytes> --expect-stderr-hex <bytes>\n"
      "limits: arguments <= %u, samples/warmups <= %u, timeout <= %u ms, "
      "captured output <= %u bytes\n",
      W_SEED_NATIVE_BENCHMARK_MAX_ARGUMENTS,
      W_SEED_NATIVE_BENCHMARK_MAX_SAMPLES,
      W_SEED_NATIVE_BENCHMARK_MAX_TIMEOUT_MS,
      W_SEED_NATIVE_BENCHMARK_MAX_CAPTURE_BYTES);
}

static bool parse_u32(const wchar_t *text, uint32_t maximum,
                      uint32_t *value_out) {
  if (text == NULL || value_out == NULL || text[0] == L'\0') return false;
  uint64_t value = 0u;
  for (size_t index = 0u; text[index] != L'\0'; index += 1u) {
    if (text[index] < L'0' || text[index] > L'9') return false;
    value = value * 10u + (uint64_t)(text[index] - L'0');
    if (value > (uint64_t)maximum) return false;
  }
  *value_out = (uint32_t)value;
  return true;
}

static int hex_value(wchar_t character) {
  if (character >= L'0' && character <= L'9')
    return (int)(character - L'0');
  if (character >= L'a' && character <= L'f')
    return (int)(character - L'a') + 10;
  if (character >= L'A' && character <= L'F')
    return (int)(character - L'A') + 10;
  return -1;
}

static bool parse_hex(const wchar_t *text, uint8_t *destination,
                      size_t destination_capacity, size_t *bytes_out) {
  if (text == NULL || destination == NULL || bytes_out == NULL) return false;
  size_t length = 0u;
  while (text[length] != L'\0') {
    if (length >= destination_capacity * 2u) return false;
    length += 1u;
  }
  if ((length & 1u) != 0u) return false;
  const size_t bytes = length / 2u;
  for (size_t index = 0u; index < bytes; index += 1u) {
    const int high = hex_value(text[index * 2u]);
    const int low = hex_value(text[index * 2u + 1u]);
    if (high < 0 || low < 0) return false;
    destination[index] = (uint8_t)((high << 4) | low);
  }
  *bytes_out = bytes;
  return true;
}

static bool option_value(int argc, wchar_t **argv, int *index,
                         const wchar_t **value_out) {
  if (argv == NULL || index == NULL || value_out == NULL ||
      *index + 1 >= argc)
    return false;
  *index += 1;
  *value_out = argv[*index];
  return *value_out != NULL;
}

static bool parse_options(int argc, wchar_t **argv,
                          native_benchmark_options *options) {
  if (argc <= 0 || argv == NULL || options == NULL) return false;
  *options = (native_benchmark_options){
      NULL, NULL, {NULL}, 0u, 1u, 9u,
      W_SEED_NATIVE_BENCHMARK_MAX_TIMEOUT_MS, false, 0u, {0u}, 0u, {0u}, 0u};
  for (int index = 1; index < argc; index += 1) {
    const wchar_t *value = NULL;
    if (wcscmp(argv[index], L"--help") == 0 ||
        wcscmp(argv[index], L"-h") == 0) {
      print_usage();
      return false;
    }
    if (wcscmp(argv[index], L"--exe") == 0) {
      if (options->executable_path != NULL ||
          !option_value(argc, argv, &index, &value))
        return false;
      options->executable_path = value;
    } else if (wcscmp(argv[index], L"--cwd") == 0) {
      if (options->working_directory != NULL ||
          !option_value(argc, argv, &index, &value))
        return false;
      options->working_directory = value;
    } else if (wcscmp(argv[index], L"--arg") == 0) {
      if (!option_value(argc, argv, &index, &value) ||
          options->argument_count >= W_SEED_NATIVE_BENCHMARK_MAX_ARGUMENTS)
        return false;
      options->arguments[options->argument_count] = value;
      options->argument_count += 1u;
    } else if (wcscmp(argv[index], L"--warmup") == 0) {
      if (!option_value(argc, argv, &index, &value) ||
          !parse_u32(value, W_SEED_NATIVE_BENCHMARK_MAX_SAMPLES,
                     &options->warmup_count) ||
          options->warmup_count > W_SEED_NATIVE_BENCHMARK_MAX_SAMPLES)
        return false;
    } else if (wcscmp(argv[index], L"--samples") == 0) {
      if (!option_value(argc, argv, &index, &value) ||
          !parse_u32(value, W_SEED_NATIVE_BENCHMARK_MAX_SAMPLES,
                     &options->sample_count) ||
          options->sample_count == 0u)
        return false;
    } else if (wcscmp(argv[index], L"--timeout-ms") == 0) {
      if (!option_value(argc, argv, &index, &value) ||
          !parse_u32(value, W_SEED_NATIVE_BENCHMARK_MAX_TIMEOUT_MS,
                     &options->timeout_ms) ||
          options->timeout_ms == 0u)
        return false;
    } else if (wcscmp(argv[index], L"--expect-exit") == 0) {
      if (!option_value(argc, argv, &index, &value) ||
          !parse_u32(value, UINT32_MAX, &options->expected_exit_code))
        return false;
      options->oracle_enabled = true;
    } else if (wcscmp(argv[index], L"--expect-stdout-hex") == 0) {
      if (!option_value(argc, argv, &index, &value) ||
          !parse_hex(value, options->expected_stdout,
                     sizeof(options->expected_stdout),
                     &options->expected_stdout_bytes))
        return false;
      options->oracle_enabled = true;
    } else if (wcscmp(argv[index], L"--expect-stderr-hex") == 0) {
      if (!option_value(argc, argv, &index, &value) ||
          !parse_hex(value, options->expected_stderr,
                     sizeof(options->expected_stderr),
                     &options->expected_stderr_bytes))
        return false;
      options->oracle_enabled = true;
    } else {
      return false;
    }
  }
  return options->executable_path != NULL;
}

static int compare_u64(const void *left, const void *right) {
  const uint64_t left_value = *(const uint64_t *)left;
  const uint64_t right_value = *(const uint64_t *)right;
  return left_value < right_value ? -1 : (left_value > right_value ? 1 : 0);
}

typedef enum {
  NATIVE_BENCHMARK_WALL,
  NATIVE_BENCHMARK_DIRECT_CPU,
  NATIVE_BENCHMARK_JOB_CPU,
} native_benchmark_metric;

static uint64_t sample_metric(w_seed_native_benchmark_sample sample,
                              native_benchmark_metric metric) {
  if (metric == NATIVE_BENCHMARK_WALL) return sample.wall_time_ns;
  if (metric == NATIVE_BENCHMARK_DIRECT_CPU)
    return sample.direct_process_cpu_time_ns;
  return sample.job_cpu_time_ns;
}

static bool metric_statistics(const w_seed_native_benchmark_sample *samples,
                              size_t count, native_benchmark_metric metric,
                              uint64_t *minimum_out, uint64_t *median_out,
                              uint64_t *p95_out, uint64_t *mean_out) {
  uint64_t sorted[W_SEED_NATIVE_BENCHMARK_MAX_SAMPLES];
  if (samples == NULL || minimum_out == NULL || median_out == NULL ||
      p95_out == NULL || mean_out == NULL || count == 0u ||
      count > W_SEED_NATIVE_BENCHMARK_MAX_SAMPLES)
    return false;
  uint64_t quotient_sum = 0u;
  uint64_t remainder_sum = 0u;
  for (size_t index = 0u; index < count; index += 1u) {
    const uint64_t value = sample_metric(samples[index], metric);
    sorted[index] = value;
    quotient_sum += value / (uint64_t)count;
    remainder_sum += value % (uint64_t)count;
  }
  qsort(sorted, count, sizeof(sorted[0]), compare_u64);
  *minimum_out = sorted[0];
  *median_out = sorted[count / 2u];
  const size_t nearest_rank = (95u * count + 99u) / 100u;
  *p95_out = sorted[nearest_rank - 1u];
  *mean_out = quotient_sum + remainder_sum / (uint64_t)count;
  return true;
}

static uint64_t maximum_rss(const w_seed_native_benchmark_sample *samples,
                            size_t count) {
  uint64_t value = samples[0].peak_direct_working_set_bytes;
  for (size_t index = 1u; index < count; index += 1u)
    if (samples[index].peak_direct_working_set_bytes > value)
      value = samples[index].peak_direct_working_set_bytes;
  return value;
}

static uint64_t maximum_job_commit(
    const w_seed_native_benchmark_sample *samples, size_t count) {
  uint64_t value = samples[0].peak_job_commit_bytes;
  for (size_t index = 1u; index < count; index += 1u)
    if (samples[index].peak_job_commit_bytes > value)
      value = samples[index].peak_job_commit_bytes;
  return value;
}

static void print_success(const native_benchmark_options *options,
                          const w_seed_native_benchmark_sample *samples) {
  uint64_t wall_minimum = 0u;
  uint64_t wall_median = 0u;
  uint64_t wall_p95 = 0u;
  uint64_t wall_mean = 0u;
  uint64_t cpu_minimum = 0u;
  uint64_t cpu_median = 0u;
  uint64_t cpu_p95 = 0u;
  uint64_t cpu_mean = 0u;
  uint64_t job_cpu_minimum = 0u;
  uint64_t job_cpu_median = 0u;
  uint64_t job_cpu_p95 = 0u;
  uint64_t job_cpu_mean = 0u;
  (void)metric_statistics(samples, options->sample_count,
                          NATIVE_BENCHMARK_WALL, &wall_minimum, &wall_median,
                          &wall_p95, &wall_mean);
  (void)metric_statistics(samples, options->sample_count,
                          NATIVE_BENCHMARK_DIRECT_CPU, &cpu_minimum,
                          &cpu_median, &cpu_p95, &cpu_mean);
  (void)metric_statistics(samples, options->sample_count,
                          NATIVE_BENCHMARK_JOB_CPU, &job_cpu_minimum,
                          &job_cpu_median, &job_cpu_p95, &job_cpu_mean);
  (void)printf(
      "{\"schema\":\"w-native-benchmark/2\",\"status\":\"ok\","
      "\"warmupCount\":%" PRIu32 ",\"sampleCount\":%" PRIu32
      ",\"oracle\":%s,\"samples\":[",
      options->warmup_count, options->sample_count,
      options->oracle_enabled ? "true" : "false");
  for (size_t index = 0u; index < options->sample_count; index += 1u) {
    const w_seed_native_benchmark_sample sample = samples[index];
    if (index != 0u) (void)fputc(',', stdout);
    (void)printf(
        "{\"wallNs\":%" PRIu64
        ",\"directProcessUserCpuNs\":%" PRIu64
        ",\"directProcessKernelCpuNs\":%" PRIu64
        ",\"directProcessCpuNs\":%" PRIu64
        ",\"jobUserCpuNs\":%" PRIu64
        ",\"jobKernelCpuNs\":%" PRIu64
        ",\"jobCpuNs\":%" PRIu64
        ",\"peakDirectWorkingSetBytes\":%" PRIu64
        ",\"peakJobCommitBytes\":%" PRIu64
        ",\"exitCode\":%" PRIu32
        ",\"stdoutBytes\":%" PRIu32 ",\"stderrBytes\":%" PRIu32 "}",
        sample.wall_time_ns, sample.direct_process_user_cpu_time_ns,
        sample.direct_process_kernel_cpu_time_ns,
        sample.direct_process_cpu_time_ns, sample.job_user_cpu_time_ns,
        sample.job_kernel_cpu_time_ns, sample.job_cpu_time_ns,
        sample.peak_direct_working_set_bytes,
        sample.peak_job_commit_bytes, sample.exit_code, sample.stdout_bytes,
        sample.stderr_bytes);
  }
  (void)printf(
      "],\"summary\":{\"wallNs\":{\"min\":%" PRIu64
      ",\"median\":%" PRIu64 ",\"p95\":%" PRIu64 ",\"mean\":%" PRIu64
      "},\"directProcessCpuNs\":{\"min\":%" PRIu64
      ",\"median\":%" PRIu64 ",\"p95\":%" PRIu64 ",\"mean\":%" PRIu64
      "},\"peakDirectWorkingSetBytes\":%" PRIu64
      ",\"jobCpuNs\":{\"min\":%" PRIu64 ",\"median\":%" PRIu64
      ",\"p95\":%" PRIu64 ",\"mean\":%" PRIu64
      "},\"peakJobCommitBytes\":%" PRIu64
      "},\"measurement\":\"Windows QPC wall time; direct process CPU and "
      "working set; Job Object CPU and peak commit with kill-on-close "
      "containment; one deadline covers launch, execution, descendants, and "
      "capture\"}\n",
      wall_minimum, wall_median, wall_p95, wall_mean,
      cpu_minimum, cpu_median, cpu_p95, cpu_mean,
      maximum_rss(samples, options->sample_count),
      job_cpu_minimum, job_cpu_median, job_cpu_p95, job_cpu_mean,
      maximum_job_commit(samples, options->sample_count));
}

int wmain(int argc, wchar_t **argv) {
  native_benchmark_options options;
  if (!parse_options(argc, argv, &options)) {
    if (argc > 1 && (wcscmp(argv[1], L"--help") == 0 ||
                     wcscmp(argv[1], L"-h") == 0))
      return 0;
    print_error(W_SEED_NATIVE_BENCHMARK_INVALID_ARGUMENT, 0u);
    return 2;
  }

  uint8_t stdout_buffer[W_SEED_NATIVE_BENCHMARK_MAX_CAPTURE_BYTES];
  uint8_t stderr_buffer[W_SEED_NATIVE_BENCHMARK_MAX_CAPTURE_BYTES];
  w_seed_native_benchmark_sample samples[W_SEED_NATIVE_BENCHMARK_MAX_SAMPLES];
  const w_seed_native_benchmark_config config = {
      options.executable_path,
      options.working_directory,
      options.arguments,
      options.argument_count,
      options.warmup_count,
      options.sample_count,
      options.timeout_ms,
      stdout_buffer,
      sizeof(stdout_buffer),
      stderr_buffer,
      sizeof(stderr_buffer),
      options.oracle_enabled,
      options.expected_exit_code,
      options.expected_stdout,
      options.expected_stdout_bytes,
      options.expected_stderr,
      options.expected_stderr_bytes,
  };
  w_seed_native_benchmark_result result;
  const w_seed_native_benchmark_status status =
      w_seed_native_benchmark_run(&config, samples, &result);
  if (status != W_SEED_NATIVE_BENCHMARK_OK) {
    print_error(status, result.os_error);
    return 1;
  }
  print_success(&options, samples);
  return 0;
}

#endif
