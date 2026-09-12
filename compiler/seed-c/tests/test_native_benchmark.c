#include "w_seed_native_benchmark.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "native benchmark check failed: %s (%s:%d)\n", \
                    #condition, __FILE__, __LINE__);                          \
      return false;                                                            \
    }                                                                          \
  } while (0)

#if !defined(_WIN32)

int main(void) {
  (void)puts("SKIP native benchmark: Windows is required");
  return 0;
}

#else

#include <windows.h>

static bool self_path(wchar_t *path, size_t capacity) {
  if (path == NULL || capacity == 0u || capacity > (size_t)UINT32_MAX)
    return false;
  const DWORD length = GetModuleFileNameW(NULL, path, (DWORD)capacity);
  return length != 0u && (size_t)length + 1u < capacity;
}

static w_seed_native_benchmark_config config_for(
    const wchar_t *executable_path, const wchar_t *const *arguments,
    size_t argument_count, uint32_t warmup_count, uint32_t sample_count,
    uint32_t timeout_ms, uint8_t *stdout_buffer, size_t stdout_capacity,
    uint8_t *stderr_buffer, size_t stderr_capacity, bool oracle_enabled,
    uint32_t expected_exit_code, const uint8_t *expected_stdout,
    size_t expected_stdout_bytes, const uint8_t *expected_stderr,
    size_t expected_stderr_bytes) {
  return (w_seed_native_benchmark_config){
      executable_path,
      NULL,
      arguments,
      argument_count,
      warmup_count,
      sample_count,
      timeout_ms,
      stdout_buffer,
      stdout_capacity,
      stderr_buffer,
      stderr_capacity,
      oracle_enabled,
      expected_exit_code,
      expected_stdout,
      expected_stdout_bytes,
      expected_stderr,
      expected_stderr_bytes,
  };
}

static bool test_measurement_and_oracle(const wchar_t *path) {
  static const wchar_t *const arguments[] = {
      L"--native-benchmark-test-child",
  };
  static const uint8_t expected_stdout[] = "native-benchmark-child\r\n";
  static const uint8_t expected_stderr[] = "native-benchmark-child-error\r\n";
  uint8_t stdout_buffer[256];
  uint8_t stderr_buffer[256];
  w_seed_native_benchmark_sample samples[3];
  const w_seed_native_benchmark_config config = config_for(
      path, arguments, 1u, 1u, 3u, 10000u, stdout_buffer,
      sizeof(stdout_buffer), stderr_buffer, sizeof(stderr_buffer), true, 23u,
      expected_stdout, sizeof(expected_stdout) - 1u, expected_stderr,
      sizeof(expected_stderr) - 1u);
  w_seed_native_benchmark_result result;
  CHECK(w_seed_native_benchmark_run(&config, samples, &result) ==
        W_SEED_NATIVE_BENCHMARK_OK);
  CHECK(result.status == W_SEED_NATIVE_BENCHMARK_OK &&
        result.warmups_completed == 1u && result.samples_completed == 3u &&
        result.failed_sample_index == UINT32_MAX);
  for (size_t index = 0u; index < 3u; index += 1u) {
    CHECK(samples[index].wall_time_ns != 0u);
    CHECK(samples[index].exit_code == 23u);
    CHECK(samples[index].stdout_bytes == sizeof(expected_stdout) - 1u);
    CHECK(samples[index].stderr_bytes == sizeof(expected_stderr) - 1u);
  }
  return true;
}

static bool test_literal_argument_quoting(const wchar_t *path) {
  static const wchar_t *const arguments[] = {
      L"--native-benchmark-test-arguments",
      L"space value",
      L"wild*card?",
      L"quote\"and\\trailing\\",
  };
  static const uint8_t expected_stdout[] = "native-benchmark-arguments-ok\r\n";
  uint8_t stdout_buffer[128];
  uint8_t stderr_buffer[16];
  w_seed_native_benchmark_sample sample;
  const w_seed_native_benchmark_config config = config_for(
      path, arguments, 4u, 1u, 1u, 10000u, stdout_buffer,
      sizeof(stdout_buffer), stderr_buffer, sizeof(stderr_buffer), true, 0u,
      expected_stdout, sizeof(expected_stdout) - 1u, NULL, 0u);
  w_seed_native_benchmark_result result;
  CHECK(w_seed_native_benchmark_run(&config, &sample, &result) ==
        W_SEED_NATIVE_BENCHMARK_OK);
  CHECK(sample.exit_code == 0u &&
        sample.stdout_bytes == sizeof(expected_stdout) - 1u &&
        sample.stderr_bytes == 0u);
  return true;
}

static bool test_oracle_mismatch(const wchar_t *path) {
  static const wchar_t *const arguments[] = {
      L"--native-benchmark-test-child",
  };
  static const uint8_t wrong_stdout[] = "wrong\n";
  uint8_t stdout_buffer[64];
  uint8_t stderr_buffer[64];
  const w_seed_native_benchmark_config config = config_for(
      path, arguments, 1u, 1u, 1u, 10000u, stdout_buffer,
      sizeof(stdout_buffer), stderr_buffer, sizeof(stderr_buffer), true, 23u,
      wrong_stdout, sizeof(wrong_stdout) - 1u, NULL, 0u);
  w_seed_native_benchmark_sample before = {
      11u, 12u, 13u, 14u, 15u, 16u, 17u, 18u};
  w_seed_native_benchmark_sample sample = before;
  w_seed_native_benchmark_result result;
  CHECK(w_seed_native_benchmark_run(&config, &sample, &result) ==
        W_SEED_NATIVE_BENCHMARK_ORACLE_MISMATCH);
  CHECK(result.status == W_SEED_NATIVE_BENCHMARK_ORACLE_MISMATCH &&
        result.warmups_completed == 0u && result.samples_completed == 0u &&
        result.failed_sample_index == UINT32_MAX);
  CHECK(memcmp(&sample, &before, sizeof(sample)) == 0);
  return true;
}

static bool test_rejects_ambiguous_path_and_limits(const wchar_t *path) {
  static const wchar_t *const arguments[] = {L"--native-benchmark-test-child"};
  uint8_t stdout_buffer[16];
  uint8_t stderr_buffer[16];
  w_seed_native_benchmark_sample sample;
  w_seed_native_benchmark_result result;
  w_seed_native_benchmark_config config = config_for(
      L"relative-child.exe", arguments, 1u, 1u, 1u, 10000u, stdout_buffer,
      sizeof(stdout_buffer), stderr_buffer, sizeof(stderr_buffer), false, 0u,
      NULL, 0u, NULL, 0u);
  CHECK(w_seed_native_benchmark_run(&config, &sample, &result) ==
        W_SEED_NATIVE_BENCHMARK_AMBIGUOUS_PATH);
  config = config_for(
      path, arguments, 1u, 1u, W_SEED_NATIVE_BENCHMARK_MAX_SAMPLES + 1u,
      10000u, stdout_buffer, sizeof(stdout_buffer), stderr_buffer,
      sizeof(stderr_buffer), false, 0u, NULL, 0u, NULL, 0u);
  CHECK(w_seed_native_benchmark_run(&config, &sample, &result) ==
        W_SEED_NATIVE_BENCHMARK_LIMIT);
  config = config_for(path, arguments, 1u, 1u, 1u, 0u, stdout_buffer,
                      sizeof(stdout_buffer), stderr_buffer, sizeof(stderr_buffer),
                      false, 0u, NULL, 0u, NULL, 0u);
  CHECK(w_seed_native_benchmark_run(&config, &sample, &result) ==
        W_SEED_NATIVE_BENCHMARK_LIMIT);
  return true;
}

static bool test_result_alias_rejection_is_transactional(const wchar_t *path) {
  static const wchar_t *const arguments[] = {L"--native-benchmark-test-child"};
  uint8_t stdout_buffer[64];
  uint8_t stderr_buffer[64];
  w_seed_native_benchmark_sample sample;
  union {
    w_seed_native_benchmark_config config;
    w_seed_native_benchmark_result result;
  } descriptor_storage;
  descriptor_storage.config = config_for(
      path, arguments, 1u, 1u, 1u, 10000u, stdout_buffer,
      sizeof(stdout_buffer), stderr_buffer, sizeof(stderr_buffer), false, 0u,
      NULL, 0u, NULL, 0u);
  uint8_t descriptor_before[sizeof(descriptor_storage)];
  (void)memcpy(descriptor_before, &descriptor_storage,
               sizeof(descriptor_storage));
  CHECK(w_seed_native_benchmark_run(&descriptor_storage.config, &sample,
                                     &descriptor_storage.result) ==
        W_SEED_NATIVE_BENCHMARK_INVALID_ARGUMENT);
  CHECK(memcmp(&descriptor_storage, descriptor_before,
               sizeof(descriptor_storage)) == 0);

  const w_seed_native_benchmark_config config = config_for(
      path, arguments, 1u, 1u, 1u, 10000u, stdout_buffer,
      sizeof(stdout_buffer), stderr_buffer, sizeof(stderr_buffer), false, 0u,
      NULL, 0u, NULL, 0u);
  union {
    w_seed_native_benchmark_sample sample;
    w_seed_native_benchmark_result result;
  } output_storage;
  (void)memset(&output_storage, 0xA5, sizeof(output_storage));
  uint8_t output_before[sizeof(output_storage)];
  (void)memcpy(output_before, &output_storage, sizeof(output_storage));
  CHECK(w_seed_native_benchmark_run(&config, &output_storage.sample,
                                     &output_storage.result) ==
        W_SEED_NATIVE_BENCHMARK_INVALID_ARGUMENT);
  CHECK(memcmp(&output_storage, output_before, sizeof(output_storage)) == 0);
  return true;
}

static bool launch_benchmark_grandchild(const wchar_t *path,
                                        const wchar_t *argument) {
  wchar_t command_line[32768];
  const size_t path_length = wcslen(path);
  const size_t argument_length = wcslen(argument);
  const size_t required = path_length + argument_length + 4u;
  if (required >= sizeof(command_line) / sizeof(command_line[0])) return false;
  command_line[0] = L'"';
  (void)memcpy(command_line + 1u, path, path_length * sizeof(path[0]));
  command_line[path_length + 1u] = L'"';
  command_line[path_length + 2u] = L' ';
  (void)memcpy(command_line + path_length + 3u, argument,
               argument_length * sizeof(argument[0]));
  command_line[required - 1u] = L'\0';
  STARTUPINFOW startup = {0};
  startup.cb = (DWORD)sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
  startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  PROCESS_INFORMATION process_information = {0};
  const BOOL created = CreateProcessW(
      path, command_line, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL,
      &startup, &process_information);
  if (!created) return false;
  (void)CloseHandle(process_information.hThread);
  (void)CloseHandle(process_information.hProcess);
  return true;
}

static bool test_timeout_and_tree_deadline(const wchar_t *path) {
  static const wchar_t *const hang_arguments[] = {
      L"--native-benchmark-test-hang",
  };
  uint8_t stdout_buffer[64];
  uint8_t stderr_buffer[64];
  w_seed_native_benchmark_sample before = {
      21u, 22u, 23u, 24u, 25u, 26u, 27u, 28u};
  w_seed_native_benchmark_sample sample = before;
  w_seed_native_benchmark_config config = config_for(
      path, hang_arguments, 1u, 1u, 1u, 50u, stdout_buffer,
      sizeof(stdout_buffer), stderr_buffer, sizeof(stderr_buffer), false, 0u,
      NULL, 0u, NULL, 0u);
  w_seed_native_benchmark_result result;
  CHECK(w_seed_native_benchmark_run(&config, &sample, &result) ==
        W_SEED_NATIVE_BENCHMARK_TIMEOUT);
  CHECK(memcmp(&sample, &before, sizeof(sample)) == 0);

  static const wchar_t *const tree_arguments[] = {
      L"--native-benchmark-test-tree-long",
  };
  sample = before;
  config = config_for(path, tree_arguments, 1u, 1u, 1u, 50u, stdout_buffer,
                      sizeof(stdout_buffer), stderr_buffer, sizeof(stderr_buffer),
                      false, 0u, NULL, 0u, NULL, 0u);
  CHECK(w_seed_native_benchmark_run(&config, &sample, &result) ==
        W_SEED_NATIVE_BENCHMARK_TREE_INCOMPLETE);
  CHECK(memcmp(&sample, &before, sizeof(sample)) == 0);
  return true;
}

static bool test_capture_overflow_preserves_samples(const wchar_t *path) {
  static const wchar_t *const arguments[] = {
      L"--native-benchmark-test-overflow",
  };
  uint8_t stdout_buffer[16];
  uint8_t stderr_buffer[16];
  w_seed_native_benchmark_sample before = {
      31u, 32u, 33u, 34u, 35u, 36u, 37u, 38u};
  w_seed_native_benchmark_sample sample = before;
  const w_seed_native_benchmark_config config = config_for(
      path, arguments, 1u, 1u, 1u, 1000u, stdout_buffer,
      sizeof(stdout_buffer), stderr_buffer, sizeof(stderr_buffer), false, 0u,
      NULL, 0u, NULL, 0u);
  w_seed_native_benchmark_result result;
  CHECK(w_seed_native_benchmark_run(&config, &sample, &result) ==
        W_SEED_NATIVE_BENCHMARK_CAPTURE);
  CHECK(memcmp(&sample, &before, sizeof(sample)) == 0);
  return true;
}

static bool test_descendant_completion(const wchar_t *path) {
  static const wchar_t *const arguments[] = {
      L"--native-benchmark-test-tree-short",
  };
  uint8_t stdout_buffer[64];
  uint8_t stderr_buffer[64];
  w_seed_native_benchmark_sample sample;
  const w_seed_native_benchmark_config config = config_for(
      path, arguments, 1u, 1u, 1u, 1000u, stdout_buffer,
      sizeof(stdout_buffer), stderr_buffer, sizeof(stderr_buffer), false, 0u,
      NULL, 0u, NULL, 0u);
  w_seed_native_benchmark_result result;
  CHECK(w_seed_native_benchmark_run(&config, &sample, &result) ==
        W_SEED_NATIVE_BENCHMARK_OK);
  CHECK(sample.exit_code == 0u && sample.job_cpu_time_ns >=
                                      sample.direct_process_cpu_time_ns);
  return true;
}

int wmain(int argc, wchar_t **argv) {
  if (argc > 1 &&
      (wcscmp(argv[1], L"--native-benchmark-test-child") == 0)) {
    (void)fputs("native-benchmark-child\n", stdout);
    (void)fputs("native-benchmark-child-error\n", stderr);
    (void)fflush(stdout);
    (void)fflush(stderr);
    return 23;
  }
  if (argc > 1 &&
      wcscmp(argv[1], L"--native-benchmark-test-arguments") == 0) {
    if (argc != 5 || wcscmp(argv[2], L"space value") != 0 ||
        wcscmp(argv[3], L"wild*card?") != 0 ||
        wcscmp(argv[4], L"quote\"and\\trailing\\") != 0)
      return 90;
    (void)puts("native-benchmark-arguments-ok");
    return 0;
  }
  if (argc > 1 && wcscmp(argv[1], L"--native-benchmark-test-hang") == 0) {
    Sleep(5000u);
    return 0;
  }
  if (argc > 1 && wcscmp(argv[1], L"--native-benchmark-test-overflow") == 0) {
    for (size_t index = 0u; index < 128u; index += 1u) {
      (void)fputc('o', stdout);
      (void)fputc('e', stderr);
    }
    (void)fflush(stdout);
    (void)fflush(stderr);
    return 0;
  }
  if (argc > 1 && (wcscmp(argv[1], L"--native-benchmark-test-tree-short") == 0 ||
                   wcscmp(argv[1], L"--native-benchmark-test-tree-long") == 0)) {
    wchar_t child_path[32768];
    if (!self_path(child_path, sizeof(child_path) / sizeof(child_path[0])))
      return 91;
    const wchar_t *const child_argument =
        wcscmp(argv[1], L"--native-benchmark-test-tree-short") == 0
            ? L"--native-benchmark-test-grandchild-short"
            : L"--native-benchmark-test-grandchild-long";
    if (!launch_benchmark_grandchild(child_path, child_argument)) return 92;
    return 0;
  }
  if (argc > 1 &&
      (wcscmp(argv[1], L"--native-benchmark-test-grandchild-short") == 0 ||
       wcscmp(argv[1], L"--native-benchmark-test-grandchild-long") == 0)) {
    Sleep(wcscmp(argv[1], L"--native-benchmark-test-grandchild-short") == 0
              ? 100u
              : 5000u);
    return 0;
  }
  wchar_t path[32768];
  if (!self_path(path, sizeof(path) / sizeof(path[0]))) return 1;
  if (!test_measurement_and_oracle(path)) return 1;
  if (!test_literal_argument_quoting(path)) return 1;
  if (!test_oracle_mismatch(path)) return 1;
  if (!test_rejects_ambiguous_path_and_limits(path)) return 1;
  if (!test_result_alias_rejection_is_transactional(path)) return 1;
  if (!test_descendant_completion(path)) return 1;
  if (!test_timeout_and_tree_deadline(path)) return 1;
  if (!test_capture_overflow_preserves_samples(path)) return 1;
  (void)puts("w_seed_native_benchmark_tests: ok");
  return 0;
}

#endif
