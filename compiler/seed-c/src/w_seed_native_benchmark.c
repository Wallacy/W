#include "w_seed_native_benchmark.h"

#include <limits.h>
#include <string.h>

const char *w_seed_native_benchmark_status_name(
    w_seed_native_benchmark_status status) {
  switch (status) {
    case W_SEED_NATIVE_BENCHMARK_OK:
      return "ok";
    case W_SEED_NATIVE_BENCHMARK_UNSUPPORTED_PLATFORM:
      return "unsupported-platform";
    case W_SEED_NATIVE_BENCHMARK_INVALID_ARGUMENT:
      return "invalid-argument";
    case W_SEED_NATIVE_BENCHMARK_AMBIGUOUS_PATH:
      return "ambiguous-path";
    case W_SEED_NATIVE_BENCHMARK_PATH_NOT_EXECUTABLE:
      return "path-not-executable";
    case W_SEED_NATIVE_BENCHMARK_LIMIT:
      return "limit";
    case W_SEED_NATIVE_BENCHMARK_COMMAND_LINE_TOO_LONG:
      return "command-line-too-long";
    case W_SEED_NATIVE_BENCHMARK_PIPE:
      return "pipe";
    case W_SEED_NATIVE_BENCHMARK_PROCESS:
      return "process";
    case W_SEED_NATIVE_BENCHMARK_THREAD:
      return "thread";
    case W_SEED_NATIVE_BENCHMARK_WAIT:
      return "wait";
    case W_SEED_NATIVE_BENCHMARK_TIMEOUT:
      return "timeout";
    case W_SEED_NATIVE_BENCHMARK_CAPTURE:
      return "capture";
    case W_SEED_NATIVE_BENCHMARK_METRICS:
      return "metrics";
    case W_SEED_NATIVE_BENCHMARK_ORACLE_MISMATCH:
      return "oracle-mismatch";
    case W_SEED_NATIVE_BENCHMARK_TREE_INCOMPLETE:
      return "tree-incomplete";
    default:
      return "unknown";
  }
}

#if !defined(_WIN32)

w_seed_native_benchmark_status w_seed_native_benchmark_run(
    const w_seed_native_benchmark_config *config,
    w_seed_native_benchmark_sample *samples,
    w_seed_native_benchmark_result *result) {
  (void)config;
  (void)samples;
  if (result != NULL) {
    *result = (w_seed_native_benchmark_result){
        W_SEED_NATIVE_BENCHMARK_UNSUPPORTED_PLATFORM, 0u, 0u, UINT32_MAX,
        0u};
  }
  return W_SEED_NATIVE_BENCHMARK_UNSUPPORTED_PLATFORM;
}

#else

#include <windows.h>
#include <psapi.h>

typedef struct {
  HANDLE read_handle;
  uint8_t *buffer;
  size_t capacity;
  size_t bytes;
  bool overflow;
  bool failed;
  DWORD error;
} native_benchmark_reader;

typedef struct {
  w_seed_native_benchmark_status status;
  DWORD error;
} native_benchmark_one_result;

typedef struct {
  wchar_t *data;
  size_t length;
  size_t capacity;
} native_benchmark_command_line;

static bool bounded_wcslen(const wchar_t *value, size_t limit,
                           size_t *length_out) {
  if (value == NULL || length_out == NULL) return false;
  for (size_t index = 0u; index < limit; index += 1u) {
    if (value[index] == L'\0') {
      *length_out = index;
      return true;
    }
  }
  return false;
}

static bool range_valid(const void *pointer, size_t bytes, uintptr_t *begin,
                        uintptr_t *end) {
  if (begin == NULL || end == NULL) return false;
  if (bytes == 0u) {
    *begin = 0u;
    *end = 0u;
    return true;
  }
  if (pointer == NULL) return false;
  const uintptr_t first = (uintptr_t)pointer;
  if (bytes > (size_t)(UINTPTR_MAX - first)) return false;
  *begin = first;
  *end = first + (uintptr_t)bytes;
  return *end >= first;
}

static bool ranges_overlap(const void *left, size_t left_bytes,
                           const void *right, size_t right_bytes) {
  uintptr_t left_begin = 0u;
  uintptr_t left_end = 0u;
  uintptr_t right_begin = 0u;
  uintptr_t right_end = 0u;
  if (!range_valid(left, left_bytes, &left_begin, &left_end) ||
      !range_valid(right, right_bytes, &right_begin, &right_end))
    return true;
  return left_bytes != 0u && right_bytes != 0u && left_begin < right_end &&
         right_begin < left_end;
}

static bool path_control_or_wildcard_free(const wchar_t *path,
                                          size_t length) {
  for (size_t index = 0u; index < length; index += 1u) {
    const wchar_t character = path[index];
    if (character < 0x20 || character == L'"' || character == L'*' ||
        character == L'?' || character == L'<' || character == L'>' ||
        character == L'|')
      return false;
  }
  return true;
}

static bool is_drive_letter(wchar_t character) {
  return (character >= L'A' && character <= L'Z') ||
         (character >= L'a' && character <= L'z');
}

static bool path_is_absolute_and_unambiguous(const wchar_t *path,
                                             size_t length) {
  if (length == 0u || !path_control_or_wildcard_free(path, length))
    return false;
  if (length >= 4u && path[0] == L'\\' && path[1] == L'\\' &&
      (path[2] == L'?' || path[2] == L'.'))
    return false;

  size_t component_start = 0u;
  if (length >= 3u && is_drive_letter(path[0]) && path[1] == L':' &&
      (path[2] == L'\\' || path[2] == L'/')) {
    component_start = 3u;
  } else if (length >= 5u && path[0] == L'\\' && path[1] == L'\\') {
    /* Require both a server and a share for a UNC path. */
    size_t components = 0u;
    size_t index = 2u;
    while (index < length) {
      while (index < length && (path[index] == L'\\' || path[index] == L'/'))
        index += 1u;
      const size_t start = index;
      while (index < length && path[index] != L'\\' && path[index] != L'/')
        index += 1u;
      if (index != start) components += 1u;
    }
    if (components < 2u) return false;
    component_start = 2u;
  } else {
    return false;
  }

  size_t index = component_start;
  while (index < length) {
    while (index < length && (path[index] == L'\\' || path[index] == L'/'))
      index += 1u;
    const size_t start = index;
    while (index < length && path[index] != L'\\' && path[index] != L'/')
      index += 1u;
    const size_t component_length = index - start;
    if ((component_length == 1u && path[start] == L'.') ||
        (component_length == 2u && path[start] == L'.' &&
         path[start + 1u] == L'.'))
      return false;
  }
  return true;
}

static w_seed_native_benchmark_status validate_path_components(
    const wchar_t *path, size_t length, DWORD *error_out) {
  wchar_t prefix[W_SEED_NATIVE_BENCHMARK_MAX_COMMAND_LINE_CHARS + 1u];
  size_t root_length = 3u;
  if (!(is_drive_letter(path[0]) && path[1] == L':')) {
    root_length = 2u;
    while (root_length < length && path[root_length] != L'\\' &&
           path[root_length] != L'/')
      root_length += 1u;
  }
  size_t prefix_length = root_length;
  for (;;) {
    (void)memcpy(prefix, path, prefix_length * sizeof(prefix[0]));
    prefix[prefix_length] = L'\0';
    HANDLE handle = CreateFileW(
        prefix, FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
        OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS,
        NULL);
    if (handle == INVALID_HANDLE_VALUE) {
      if (error_out != NULL) *error_out = GetLastError();
      return W_SEED_NATIVE_BENCHMARK_PATH_NOT_EXECUTABLE;
    }
    BY_HANDLE_FILE_INFORMATION information;
    const BOOL has_information =
        GetFileInformationByHandle(handle, &information);
    const DWORD close_error =
        CloseHandle(handle) ? ERROR_SUCCESS : GetLastError();
    if (!has_information) {
      if (error_out != NULL) *error_out = GetLastError();
      return W_SEED_NATIVE_BENCHMARK_PATH_NOT_EXECUTABLE;
    }
    if ((information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u)
      return W_SEED_NATIVE_BENCHMARK_AMBIGUOUS_PATH;
    if (close_error != ERROR_SUCCESS) {
      if (error_out != NULL) *error_out = close_error;
      return W_SEED_NATIVE_BENCHMARK_PATH_NOT_EXECUTABLE;
    }
    if (prefix_length == length) break;
    while (prefix_length < length &&
           (path[prefix_length] == L'\\' || path[prefix_length] == L'/'))
      prefix_length += 1u;
    while (prefix_length < length && path[prefix_length] != L'\\' &&
           path[prefix_length] != L'/')
      prefix_length += 1u;
  }
  return W_SEED_NATIVE_BENCHMARK_OK;
}

static w_seed_native_benchmark_status validate_windows_path(
    const wchar_t *path, bool directory, DWORD *error_out) {
  size_t length = 0u;
  if (!bounded_wcslen(path, W_SEED_NATIVE_BENCHMARK_MAX_COMMAND_LINE_CHARS,
                     &length) || !path_is_absolute_and_unambiguous(path,
                                                                      length))
    return W_SEED_NATIVE_BENCHMARK_AMBIGUOUS_PATH;
  if (!directory && (path[length - 1u] == L'\\' ||
                     path[length - 1u] == L'/'))
    return W_SEED_NATIVE_BENCHMARK_AMBIGUOUS_PATH;

  const DWORD attributes = GetFileAttributesW(path);
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    if (error_out != NULL) *error_out = GetLastError();
    return W_SEED_NATIVE_BENCHMARK_PATH_NOT_EXECUTABLE;
  }
  if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u)
    return W_SEED_NATIVE_BENCHMARK_AMBIGUOUS_PATH;
  if (directory != ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0u))
    return W_SEED_NATIVE_BENCHMARK_PATH_NOT_EXECUTABLE;

  const DWORD flags = FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS;
  HANDLE handle = CreateFileW(path, FILE_READ_ATTRIBUTES,
                              FILE_SHARE_READ | FILE_SHARE_WRITE |
                                  FILE_SHARE_DELETE,
                              NULL, OPEN_EXISTING, flags, NULL);
  if (handle == INVALID_HANDLE_VALUE) {
    if (error_out != NULL) *error_out = GetLastError();
    return W_SEED_NATIVE_BENCHMARK_PATH_NOT_EXECUTABLE;
  }
  const DWORD file_type = GetFileType(handle);
  BY_HANDLE_FILE_INFORMATION information;
  const BOOL has_information = GetFileInformationByHandle(handle, &information);
  const DWORD close_error = CloseHandle(handle) ? ERROR_SUCCESS : GetLastError();
  if (file_type != FILE_TYPE_DISK || !has_information) {
    if (error_out != NULL)
      *error_out = has_information ? ERROR_INVALID_HANDLE : GetLastError();
    return W_SEED_NATIVE_BENCHMARK_PATH_NOT_EXECUTABLE;
  }
  if ((information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u)
    return W_SEED_NATIVE_BENCHMARK_AMBIGUOUS_PATH;
  if (close_error != ERROR_SUCCESS && error_out != NULL) *error_out = close_error;
  return validate_path_components(path, length, error_out);
}

static bool command_line_append(native_benchmark_command_line *builder,
                                wchar_t character) {
  if (builder == NULL || builder->data == NULL ||
      builder->length + 1u >= builder->capacity)
    return false;
  builder->data[builder->length] = character;
  builder->length += 1u;
  return true;
}

static bool command_line_append_quoted(native_benchmark_command_line *builder,
                                       const wchar_t *value) {
  size_t value_length = 0u;
  if (builder == NULL ||
      !bounded_wcslen(value, W_SEED_NATIVE_BENCHMARK_MAX_COMMAND_LINE_CHARS,
                      &value_length))
    return false;
  if (!command_line_append(builder, L'"')) return false;

  size_t slash_count = 0u;
  for (size_t index = 0u; index < value_length; index += 1u) {
    const wchar_t character = value[index];
    if (character == L'\\') {
      slash_count += 1u;
      continue;
    }
    if (character == L'"') {
      for (size_t slash = 0u; slash < slash_count * 2u + 1u; slash += 1u)
        if (!command_line_append(builder, L'\\')) return false;
      if (!command_line_append(builder, L'"')) return false;
    } else {
      for (size_t slash = 0u; slash < slash_count; slash += 1u)
        if (!command_line_append(builder, L'\\')) return false;
      if (!command_line_append(builder, character)) return false;
    }
    slash_count = 0u;
  }
  for (size_t slash = 0u; slash < slash_count * 2u; slash += 1u)
    if (!command_line_append(builder, L'\\')) return false;
  return command_line_append(builder, L'"');
}

static bool command_line_build(const w_seed_native_benchmark_config *config,
                               wchar_t *storage, size_t storage_capacity,
                               size_t *length_out) {
  if (config == NULL || storage == NULL || length_out == NULL ||
      storage_capacity < W_SEED_NATIVE_BENCHMARK_MAX_COMMAND_LINE_CHARS + 1u)
    return false;
  native_benchmark_command_line builder = {storage, 0u, storage_capacity};
  if (!command_line_append_quoted(&builder, config->executable_path))
    return false;
  for (size_t index = 0u; index < config->argument_count; index += 1u) {
    if (!command_line_append(&builder, L' ')) return false;
    if (!command_line_append_quoted(&builder, config->arguments[index]))
      return false;
  }
  storage[builder.length] = L'\0';
  *length_out = builder.length;
  return true;
}

static DWORD WINAPI native_benchmark_reader_main(LPVOID parameter) {
  native_benchmark_reader *reader = (native_benchmark_reader *)parameter;
  if (reader == NULL || reader->read_handle == NULL) return ERROR_INVALID_PARAMETER;

  uint8_t chunk[4096];
  for (;;) {
    DWORD read_bytes = 0u;
    if (!ReadFile(reader->read_handle, chunk, (DWORD)sizeof(chunk),
                  &read_bytes, NULL)) {
      const DWORD error = GetLastError();
      if (error == ERROR_BROKEN_PIPE || error == ERROR_HANDLE_EOF) return 0u;
      reader->failed = true;
      reader->error = error;
      return error;
    }
    if (read_bytes == 0u) return 0u;
    const size_t available = reader->capacity - reader->bytes;
    const size_t incoming = (size_t)read_bytes;
    const size_t copied = incoming < available ? incoming : available;
    if (copied != 0u) {
      (void)memcpy(reader->buffer + reader->bytes, chunk, copied);
      reader->bytes += copied;
    }
    if (copied != incoming) reader->overflow = true;
  }
}

static uint64_t filetime_units(FILETIME value) {
  ULARGE_INTEGER combined;
  combined.LowPart = value.dwLowDateTime;
  combined.HighPart = value.dwHighDateTime;
  return (uint64_t)combined.QuadPart;
}

static bool qpc_elapsed_ns(LARGE_INTEGER start, LARGE_INTEGER end,
                           LARGE_INTEGER frequency, uint64_t *value_out) {
  if (value_out == NULL || start.QuadPart < 0 || end.QuadPart < start.QuadPart ||
      frequency.QuadPart <= 0)
    return false;
  const long double ticks = (long double)(end.QuadPart - start.QuadPart);
  const long double scale = (long double)frequency.QuadPart;
  const long double nanoseconds = ticks * 1000000000.0L / scale;
  if (nanoseconds < 0.0L || nanoseconds > (long double)UINT64_MAX)
    return false;
  *value_out = (uint64_t)(nanoseconds + 0.5L);
  return true;
}

static bool qpc_deadline(LARGE_INTEGER start, LARGE_INTEGER frequency,
                         uint32_t timeout_ms, LARGE_INTEGER *deadline_out) {
  if (deadline_out == NULL || start.QuadPart < 0 || frequency.QuadPart <= 0 ||
      timeout_ms == 0u)
    return false;
  const long double ticks =
      (long double)frequency.QuadPart * (long double)timeout_ms / 1000.0L;
  if (ticks < 1.0L || ticks > (long double)LLONG_MAX)
    return false;
  const LONGLONG rounded_ticks = (LONGLONG)(ticks + 0.999999L);
  if (rounded_ticks <= 0 || start.QuadPart > LLONG_MAX - rounded_ticks)
    return false;
  deadline_out->QuadPart = start.QuadPart + rounded_ticks;
  return true;
}

static bool qpc_remaining_ms(LARGE_INTEGER deadline, LARGE_INTEGER frequency,
                             DWORD *milliseconds_out) {
  if (milliseconds_out == NULL || deadline.QuadPart < 0 ||
      frequency.QuadPart <= 0)
    return false;
  LARGE_INTEGER now;
  if (!QueryPerformanceCounter(&now)) return false;
  if (now.QuadPart >= deadline.QuadPart) {
    *milliseconds_out = 0u;
    return true;
  }
  const long double ticks = (long double)(deadline.QuadPart - now.QuadPart);
  const long double milliseconds =
      ticks * 1000.0L / (long double)frequency.QuadPart;
  if (milliseconds < 0.0L || milliseconds > (long double)UINT32_MAX)
    return false;
  DWORD rounded = (DWORD)milliseconds;
  if ((long double)rounded < milliseconds) rounded += 1u;
  *milliseconds_out = rounded == 0u ? 1u : rounded;
  return true;
}

static bool job_accounting(HANDLE job,
                           JOBOBJECT_BASIC_ACCOUNTING_INFORMATION *basic_out,
                           JOBOBJECT_EXTENDED_LIMIT_INFORMATION *extended_out,
                           DWORD *error_out) {
  if (job == NULL || basic_out == NULL || extended_out == NULL) return false;
  (void)memset(basic_out, 0, sizeof(*basic_out));
  (void)memset(extended_out, 0, sizeof(*extended_out));
  if (!QueryInformationJobObject(job, JobObjectBasicAccountingInformation,
                                 basic_out, (DWORD)sizeof(*basic_out), NULL)) {
    if (error_out != NULL) *error_out = GetLastError();
    return false;
  }
  if (!QueryInformationJobObject(job, JobObjectExtendedLimitInformation,
                                 extended_out, (DWORD)sizeof(*extended_out),
                                 NULL)) {
    if (error_out != NULL) *error_out = GetLastError();
    return false;
  }
  if (basic_out->TotalUserTime.QuadPart < 0 ||
      basic_out->TotalKernelTime.QuadPart < 0) {
    if (error_out != NULL) *error_out = ERROR_ARITHMETIC_OVERFLOW;
    return false;
  }
  const uint64_t user_units = (uint64_t)basic_out->TotalUserTime.QuadPart;
  const uint64_t kernel_units = (uint64_t)basic_out->TotalKernelTime.QuadPart;
  if (user_units > UINT64_MAX - kernel_units ||
      user_units + kernel_units > UINT64_MAX / 100u) {
    if (error_out != NULL) *error_out = ERROR_ARITHMETIC_OVERFLOW;
    return false;
  }
  return true;
}

static bool job_cpu_times_ns(
    const JOBOBJECT_BASIC_ACCOUNTING_INFORMATION *basic,
    uint64_t *user_out, uint64_t *kernel_out, uint64_t *total_out) {
  if (basic == NULL || user_out == NULL || kernel_out == NULL ||
      total_out == NULL || basic->TotalUserTime.QuadPart < 0 ||
      basic->TotalKernelTime.QuadPart < 0)
    return false;
  const uint64_t user_units = (uint64_t)basic->TotalUserTime.QuadPart;
  const uint64_t kernel_units = (uint64_t)basic->TotalKernelTime.QuadPart;
  if (user_units > UINT64_MAX - kernel_units ||
      user_units + kernel_units > UINT64_MAX / 100u)
    return false;
  *user_out = user_units * 100u;
  *kernel_out = kernel_units * 100u;
  *total_out = (user_units + kernel_units) * 100u;
  return true;
}

static bool join_reader_threads_until_deadline(
    HANDLE reader_threads[2], LARGE_INTEGER deadline, LARGE_INTEGER frequency,
    DWORD *error_out) {
  if (reader_threads == NULL) return false;
  for (size_t index = 0u; index < 2u; index += 1u) {
    if (reader_threads[index] == NULL) return false;
    DWORD remaining_ms = 0u;
    if (!qpc_remaining_ms(deadline, frequency, &remaining_ms)) {
      if (error_out != NULL) *error_out = GetLastError();
      return false;
    }
    DWORD wait_result = WAIT_FAILED;
    if (remaining_ms != 0u)
      wait_result = WaitForSingleObject(reader_threads[index], remaining_ms);
    if (wait_result == WAIT_TIMEOUT || remaining_ms == 0u) {
      /* ReadFile is synchronous. Cancel it before the failure path joins the
       * thread, so capture cannot outlive the caller-owned buffers. */
      (void)CancelSynchronousIo(reader_threads[index]);
      if (WaitForSingleObject(reader_threads[index], INFINITE) != WAIT_OBJECT_0) {
        if (error_out != NULL) *error_out = GetLastError();
        return false;
      }
      if (error_out != NULL) *error_out = ERROR_TIMEOUT;
      return false;
    }
    if (wait_result != WAIT_OBJECT_0) {
      if (error_out != NULL) *error_out = GetLastError();
      return false;
    }
  }
  return true;
}

static native_benchmark_one_result one_failure(
    w_seed_native_benchmark_status status, DWORD error) {
  return (native_benchmark_one_result){status, error};
}

static native_benchmark_one_result run_one(
    const w_seed_native_benchmark_config *config, const wchar_t *command_line,
    size_t command_line_length, LARGE_INTEGER frequency,
    w_seed_native_benchmark_sample *sample_out) {
  HANDLE stdout_read = NULL;
  HANDLE stdout_write = NULL;
  HANDLE stderr_read = NULL;
  HANDLE stderr_write = NULL;
  HANDLE stdin_handle = INVALID_HANDLE_VALUE;
  HANDLE job = NULL;
  HANDLE reader_threads[2] = {NULL, NULL};
  PROCESS_INFORMATION process_information = {0};
  native_benchmark_reader readers[2] = {
      {NULL, config->stdout_buffer, config->stdout_capacity, 0u, false, false,
       ERROR_SUCCESS},
      {NULL, config->stderr_buffer, config->stderr_capacity, 0u, false, false,
       ERROR_SUCCESS},
  };
  SECURITY_ATTRIBUTES security_attributes = {
      (DWORD)sizeof(security_attributes), NULL, TRUE};
  JOBOBJECT_BASIC_ACCOUNTING_INFORMATION job_accounting_information = {0};
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_memory_information = {0};
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_limits = {0};
  PROCESS_MEMORY_COUNTERS memory_counters = {0};
  FILETIME creation_time = {0};
  FILETIME exit_time = {0};
  FILETIME kernel_time = {0};
  FILETIME user_time = {0};
  LARGE_INTEGER start = {0};
  LARGE_INTEGER deadline = {0};
  LARGE_INTEGER end = {0};
  wchar_t mutable_command_line[W_SEED_NATIVE_BENCHMARK_MAX_COMMAND_LINE_CHARS +
                               1u];
  STARTUPINFOW startup = {0};
  w_seed_native_benchmark_sample sample = {0};
  native_benchmark_one_result outcome =
      one_failure(W_SEED_NATIVE_BENCHMARK_PROCESS, ERROR_SUCCESS);
  DWORD exit_code = 0u;
  DWORD remaining_ms = 0u;
  DWORD accounting_error = ERROR_SUCCESS;
  DWORD reader_error = ERROR_SUCCESS;
  bool process_waited = false;
  bool readers_joined = false;

  if (sample_out == NULL || !QueryPerformanceCounter(&start) ||
      !qpc_deadline(start, frequency, config->timeout_ms, &deadline))
    return one_failure(W_SEED_NATIVE_BENCHMARK_METRICS, GetLastError());
  if (command_line_length > W_SEED_NATIVE_BENCHMARK_MAX_COMMAND_LINE_CHARS ||
      command_line_length + 1u > sizeof(mutable_command_line) / sizeof(wchar_t))
    goto command_line_failure;

  if (!CreatePipe(&stdout_read, &stdout_write, &security_attributes, 0u))
    goto pipe_failure;
  if (!SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0u))
    goto pipe_failure;
  if (!CreatePipe(&stderr_read, &stderr_write, &security_attributes, 0u))
    goto pipe_failure;
  if (!SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0u))
    goto pipe_failure;
  stdin_handle = CreateFileW(L"NUL", GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             &security_attributes, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, NULL);
  if (stdin_handle == INVALID_HANDLE_VALUE) goto pipe_failure;

  job = CreateJobObjectW(NULL, NULL);
  if (job == NULL) goto process_failure;
  job_limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                               &job_limits, (DWORD)sizeof(job_limits)))
    goto process_failure;
  if (!qpc_remaining_ms(deadline, frequency, &remaining_ms))
    goto metrics_failure;
  if (remaining_ms == 0u) {
    outcome = one_failure(W_SEED_NATIVE_BENCHMARK_TIMEOUT, ERROR_TIMEOUT);
    goto cleanup;
  }

  (void)memcpy(mutable_command_line, command_line,
               (command_line_length + 1u) * sizeof(wchar_t));
  startup.cb = (DWORD)sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = stdin_handle;
  startup.hStdOutput = stdout_write;
  startup.hStdError = stderr_write;
  if (!CreateProcessW(config->executable_path, mutable_command_line, NULL, NULL,
                      TRUE, CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED |
                          CREATE_NO_WINDOW,
                      NULL, config->working_directory, &startup,
                      &process_information))
    goto process_failure;
  (void)CloseHandle(stdout_write);
  stdout_write = NULL;
  (void)CloseHandle(stderr_write);
  stderr_write = NULL;
  (void)CloseHandle(stdin_handle);
  stdin_handle = INVALID_HANDLE_VALUE;
  if (!AssignProcessToJobObject(job, process_information.hProcess))
    goto process_failure_after_create;

  readers[0].read_handle = stdout_read;
  readers[1].read_handle = stderr_read;
  reader_threads[0] = CreateThread(NULL, 0u, native_benchmark_reader_main,
                                   &readers[0], 0u, NULL);
  if (reader_threads[0] == NULL) goto thread_failure;
  reader_threads[1] = CreateThread(NULL, 0u, native_benchmark_reader_main,
                                   &readers[1], 0u, NULL);
  if (reader_threads[1] == NULL) goto thread_failure;
  if (!qpc_remaining_ms(deadline, frequency, &remaining_ms))
    goto metrics_failure_after_create;
  if (remaining_ms == 0u) {
    outcome = one_failure(W_SEED_NATIVE_BENCHMARK_TIMEOUT, ERROR_TIMEOUT);
    goto terminate_after_create;
  }
  if (ResumeThread(process_information.hThread) == (DWORD)-1)
    goto process_failure_after_create;
  (void)CloseHandle(process_information.hThread);
  process_information.hThread = NULL;

  {
    const DWORD wait_result =
        WaitForSingleObject(process_information.hProcess, remaining_ms);
    if (wait_result == WAIT_TIMEOUT) {
      outcome = one_failure(W_SEED_NATIVE_BENCHMARK_TIMEOUT, ERROR_TIMEOUT);
      (void)TerminateJobObject(job, 1u);
      (void)TerminateProcess(process_information.hProcess, 1u);
      if (WaitForSingleObject(process_information.hProcess, INFINITE) !=
          WAIT_OBJECT_0)
        goto wait_failure;
      process_waited = true;
      goto cleanup;
    }
    if (wait_result != WAIT_OBJECT_0) goto wait_failure;
  }
  process_waited = true;

  for (;;) {
    if (!job_accounting(job, &job_accounting_information,
                        &job_memory_information, &accounting_error))
      goto metrics_failure_with_error;
    if (job_accounting_information.ActiveProcesses == 0u) break;
    if (!qpc_remaining_ms(deadline, frequency, &remaining_ms))
      goto metrics_failure;
    if (remaining_ms == 0u) {
      /* A root that leaves a live descendant is not a valid finite sample. */
      (void)TerminateJobObject(job, 1u);
      outcome = one_failure(W_SEED_NATIVE_BENCHMARK_TREE_INCOMPLETE,
                            ERROR_TIMEOUT);
      goto cleanup;
    }
    Sleep(remaining_ms < 2u ? remaining_ms : 1u);
  }

  if (!GetExitCodeProcess(process_information.hProcess, &exit_code))
    goto metrics_failure;
  sample.exit_code = (uint32_t)exit_code;
  if (!GetProcessTimes(process_information.hProcess, &creation_time, &exit_time,
                       &kernel_time, &user_time))
    goto metrics_failure;
  memory_counters.cb = (DWORD)sizeof(memory_counters);
  if (!GetProcessMemoryInfo(process_information.hProcess, &memory_counters,
                            (DWORD)sizeof(memory_counters)))
    goto metrics_failure;
  if (!job_cpu_times_ns(&job_accounting_information,
                        &sample.job_user_cpu_time_ns,
                        &sample.job_kernel_cpu_time_ns,
                        &sample.job_cpu_time_ns))
    goto metrics_failure;
  const uint64_t direct_kernel_units = filetime_units(kernel_time);
  const uint64_t direct_user_units = filetime_units(user_time);
  if (direct_kernel_units > UINT64_MAX - direct_user_units ||
      direct_kernel_units + direct_user_units > UINT64_MAX / 100u)
    goto metrics_failure;
  sample.direct_process_user_cpu_time_ns = direct_user_units * 100u;
  sample.direct_process_kernel_cpu_time_ns = direct_kernel_units * 100u;
  sample.direct_process_cpu_time_ns =
      sample.direct_process_user_cpu_time_ns +
      sample.direct_process_kernel_cpu_time_ns;
  sample.peak_direct_working_set_bytes =
      (uint64_t)memory_counters.PeakWorkingSetSize;
  sample.peak_job_commit_bytes =
      (uint64_t)job_memory_information.PeakJobMemoryUsed;

  (void)CloseHandle(job);
  job = NULL;
  if (!join_reader_threads_until_deadline(reader_threads, deadline, frequency,
                                          &reader_error)) {
    outcome = one_failure(reader_error == ERROR_TIMEOUT
                              ? W_SEED_NATIVE_BENCHMARK_TIMEOUT
                              : W_SEED_NATIVE_BENCHMARK_WAIT,
                          reader_error);
    goto cleanup;
  }
  readers_joined = true;
  if (readers[0].failed || readers[1].failed || readers[0].overflow ||
      readers[1].overflow)
    goto capture_failure;
  if (!QueryPerformanceCounter(&end) ||
      !qpc_elapsed_ns(start, end, frequency, &sample.wall_time_ns))
    goto metrics_failure;
  sample.stdout_bytes = (uint32_t)readers[0].bytes;
  sample.stderr_bytes = (uint32_t)readers[1].bytes;
  if (config->oracle_enabled &&
      (sample.exit_code != config->expected_exit_code ||
       sample.stdout_bytes != config->expected_stdout_bytes ||
       sample.stderr_bytes != config->expected_stderr_bytes ||
       (sample.stdout_bytes != 0u &&
        memcmp(config->stdout_buffer, config->expected_stdout,
               sample.stdout_bytes) != 0) ||
       (sample.stderr_bytes != 0u &&
        memcmp(config->stderr_buffer, config->expected_stderr,
               sample.stderr_bytes) != 0))) {
    outcome = one_failure(W_SEED_NATIVE_BENCHMARK_ORACLE_MISMATCH,
                          ERROR_SUCCESS);
    goto cleanup;
  }
  *sample_out = sample;
  outcome = one_failure(W_SEED_NATIVE_BENCHMARK_OK, ERROR_SUCCESS);
  goto cleanup;

pipe_failure:
  outcome = one_failure(W_SEED_NATIVE_BENCHMARK_PIPE, GetLastError());
  goto cleanup;
command_line_failure:
  outcome = one_failure(W_SEED_NATIVE_BENCHMARK_COMMAND_LINE_TOO_LONG,
                        ERROR_INSUFFICIENT_BUFFER);
  goto cleanup;
thread_failure:
  outcome = one_failure(W_SEED_NATIVE_BENCHMARK_THREAD, GetLastError());
  goto terminate_after_create;
process_failure_after_create:
  outcome = one_failure(W_SEED_NATIVE_BENCHMARK_PROCESS, GetLastError());
metrics_failure_after_create:
  outcome = one_failure(W_SEED_NATIVE_BENCHMARK_METRICS, GetLastError());
terminate_after_create:
  if (process_information.hProcess != NULL) {
    if (job != NULL) (void)TerminateJobObject(job, 1u);
    (void)TerminateProcess(process_information.hProcess, 1u);
    (void)WaitForSingleObject(process_information.hProcess, INFINITE);
    process_waited = true;
  }
  goto cleanup;
wait_failure:
  outcome = one_failure(W_SEED_NATIVE_BENCHMARK_WAIT, GetLastError());
  goto cleanup;
capture_failure:
  outcome = one_failure(
      W_SEED_NATIVE_BENCHMARK_CAPTURE,
      readers[0].failed ? readers[0].error
                        : (readers[1].failed ? readers[1].error
                                             : ERROR_BUFFER_OVERFLOW));
  goto cleanup;
metrics_failure_with_error:
  outcome = one_failure(W_SEED_NATIVE_BENCHMARK_METRICS, accounting_error);
  goto cleanup;
metrics_failure:
  outcome = one_failure(W_SEED_NATIVE_BENCHMARK_METRICS, GetLastError());
  goto cleanup;
process_failure:
  outcome = one_failure(W_SEED_NATIVE_BENCHMARK_PROCESS, GetLastError());

cleanup:
  if (process_information.hThread != NULL) {
    (void)CloseHandle(process_information.hThread);
    process_information.hThread = NULL;
  }
  if (process_information.hProcess != NULL) {
    if (!process_waited && outcome.status != W_SEED_NATIVE_BENCHMARK_OK) {
      if (job != NULL) (void)TerminateJobObject(job, 1u);
      (void)TerminateProcess(process_information.hProcess, 1u);
      (void)WaitForSingleObject(process_information.hProcess, INFINITE);
    }
    (void)CloseHandle(process_information.hProcess);
    process_information.hProcess = NULL;
  }
  if (job != NULL) {
    (void)CloseHandle(job);
    job = NULL;
  }
  if (!readers_joined) {
    for (size_t index = 0u; index < 2u; index += 1u)
      if (reader_threads[index] != NULL) (void)CancelSynchronousIo(reader_threads[index]);
  }
  for (size_t index = 0u; index < 2u; index += 1u) {
    if (reader_threads[index] != NULL) {
      (void)WaitForSingleObject(reader_threads[index], INFINITE);
      (void)CloseHandle(reader_threads[index]);
      reader_threads[index] = NULL;
    }
  }
  if (stdout_read != NULL) (void)CloseHandle(stdout_read);
  if (stderr_read != NULL) (void)CloseHandle(stderr_read);
  if (stdout_write != NULL) (void)CloseHandle(stdout_write);
  if (stderr_write != NULL) (void)CloseHandle(stderr_write);
  if (stdin_handle != INVALID_HANDLE_VALUE) (void)CloseHandle(stdin_handle);
  return outcome;
}

static void publish_result(w_seed_native_benchmark_result *result,
                           w_seed_native_benchmark_status status,
                           uint32_t warmups_completed,
                           uint32_t samples_completed,
                           uint32_t failed_sample_index, DWORD os_error) {
  if (result == NULL) return;
  *result = (w_seed_native_benchmark_result){
      status, warmups_completed, samples_completed, failed_sample_index,
      (uint32_t)os_error};
}

static w_seed_native_benchmark_status validate_config(
    const w_seed_native_benchmark_config *config,
    const w_seed_native_benchmark_sample *samples,
    const w_seed_native_benchmark_result *result, DWORD *error_out,
    bool *result_publishable_out) {
  if (result_publishable_out != NULL) *result_publishable_out = false;
  if (config == NULL || samples == NULL || result == NULL)
    return W_SEED_NATIVE_BENCHMARK_INVALID_ARGUMENT;
  if (ranges_overlap(config, sizeof(*config), result, sizeof(*result)))
    return W_SEED_NATIVE_BENCHMARK_INVALID_ARGUMENT;
  if (result_publishable_out != NULL) *result_publishable_out = true;
  if (config->argument_count > W_SEED_NATIVE_BENCHMARK_MAX_ARGUMENTS ||
      (config->argument_count != 0u && config->arguments == NULL) ||
      config->warmup_count > W_SEED_NATIVE_BENCHMARK_MAX_SAMPLES ||
      config->sample_count == 0u ||
      config->sample_count > W_SEED_NATIVE_BENCHMARK_MAX_SAMPLES ||
      config->timeout_ms == 0u ||
      config->timeout_ms > W_SEED_NATIVE_BENCHMARK_MAX_TIMEOUT_MS)
    return W_SEED_NATIVE_BENCHMARK_LIMIT;
  if (config->stdout_buffer == NULL || config->stderr_buffer == NULL ||
      config->stdout_capacity == 0u || config->stderr_capacity == 0u ||
      config->stdout_capacity > W_SEED_NATIVE_BENCHMARK_MAX_CAPTURE_BYTES ||
      config->stderr_capacity > W_SEED_NATIVE_BENCHMARK_MAX_CAPTURE_BYTES)
    return W_SEED_NATIVE_BENCHMARK_LIMIT;
  if (!config->oracle_enabled &&
      (config->expected_stdout_bytes != 0u || config->expected_stderr_bytes != 0u))
    return W_SEED_NATIVE_BENCHMARK_INVALID_ARGUMENT;
  if (config->expected_stdout_bytes > config->stdout_capacity ||
      config->expected_stderr_bytes > config->stderr_capacity ||
      config->expected_stdout_bytes > W_SEED_NATIVE_BENCHMARK_MAX_CAPTURE_BYTES ||
      config->expected_stderr_bytes > W_SEED_NATIVE_BENCHMARK_MAX_CAPTURE_BYTES ||
      (config->expected_stdout_bytes != 0u && config->expected_stdout == NULL) ||
      (config->expected_stderr_bytes != 0u && config->expected_stderr == NULL))
    return W_SEED_NATIVE_BENCHMARK_LIMIT;

  const size_t sample_bytes =
      (size_t)config->sample_count * sizeof(w_seed_native_benchmark_sample);
  if (ranges_overlap(samples, sample_bytes, result, sizeof(*result)) ||
      ranges_overlap(result, sizeof(*result), config->stdout_buffer,
                     config->stdout_capacity) ||
      ranges_overlap(result, sizeof(*result), config->stderr_buffer,
                     config->stderr_capacity)) {
    if (result_publishable_out != NULL) *result_publishable_out = false;
    return W_SEED_NATIVE_BENCHMARK_INVALID_ARGUMENT;
  }
  if (ranges_overlap(config, sizeof(*config), samples, sample_bytes) ||
      ranges_overlap(samples, sample_bytes, config->stdout_buffer,
                     config->stdout_capacity) ||
      ranges_overlap(samples, sample_bytes, config->stderr_buffer,
                     config->stderr_capacity) ||
      ranges_overlap(config->stdout_buffer, config->stdout_capacity,
                     config->stderr_buffer, config->stderr_capacity) ||
      ranges_overlap(config->expected_stdout, config->expected_stdout_bytes,
                     config->stdout_buffer, config->stdout_capacity) ||
      ranges_overlap(config->expected_stderr, config->expected_stderr_bytes,
                     config->stderr_buffer, config->stderr_capacity))
    return W_SEED_NATIVE_BENCHMARK_INVALID_ARGUMENT;

  w_seed_native_benchmark_status status =
      validate_windows_path(config->executable_path, false, error_out);
  if (status != W_SEED_NATIVE_BENCHMARK_OK) return status;
  if (config->working_directory != NULL) {
    status = validate_windows_path(config->working_directory, true, error_out);
    if (status != W_SEED_NATIVE_BENCHMARK_OK) return status;
  }
  size_t executable_length = 0u;
  if (!bounded_wcslen(config->executable_path,
                     W_SEED_NATIVE_BENCHMARK_MAX_COMMAND_LINE_CHARS,
                     &executable_length))
    return W_SEED_NATIVE_BENCHMARK_COMMAND_LINE_TOO_LONG;
  for (size_t index = 0u; index < config->argument_count; index += 1u) {
    size_t argument_length = 0u;
    if (!bounded_wcslen(config->arguments[index],
                        W_SEED_NATIVE_BENCHMARK_MAX_COMMAND_LINE_CHARS,
                       &argument_length))
      return W_SEED_NATIVE_BENCHMARK_INVALID_ARGUMENT;
    (void)argument_length;
  }
  (void)executable_length;
  return W_SEED_NATIVE_BENCHMARK_OK;
}

w_seed_native_benchmark_status w_seed_native_benchmark_run(
    const w_seed_native_benchmark_config *config,
    w_seed_native_benchmark_sample *samples,
    w_seed_native_benchmark_result *result) {
  DWORD validation_error = ERROR_SUCCESS;
  bool result_publishable = false;
  const w_seed_native_benchmark_status validation = validate_config(
      config, samples, result, &validation_error, &result_publishable);
  if (validation != W_SEED_NATIVE_BENCHMARK_OK) {
    if (result_publishable)
      publish_result(result, validation, 0u, 0u, UINT32_MAX,
                     validation_error);
    return validation;
  }

  LARGE_INTEGER frequency;
  if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0) {
    publish_result(result, W_SEED_NATIVE_BENCHMARK_METRICS, 0u, 0u,
                   UINT32_MAX, GetLastError());
    return W_SEED_NATIVE_BENCHMARK_METRICS;
  }
  wchar_t command_line[W_SEED_NATIVE_BENCHMARK_MAX_COMMAND_LINE_CHARS + 1u];
  size_t command_line_length = 0u;
  if (!command_line_build(config, command_line, sizeof(command_line) /
                                             sizeof(command_line[0]),
                          &command_line_length)) {
    publish_result(result, W_SEED_NATIVE_BENCHMARK_COMMAND_LINE_TOO_LONG, 0u,
                   0u, UINT32_MAX, ERROR_INSUFFICIENT_BUFFER);
    return W_SEED_NATIVE_BENCHMARK_COMMAND_LINE_TOO_LONG;
  }

  /* Keep the caller's sample array untouched until every warmup and measured
   * process has passed its oracle and metric checks. This makes a failed run
   * an all-or-nothing publication boundary. */
  w_seed_native_benchmark_sample staged_samples[
      W_SEED_NATIVE_BENCHMARK_MAX_SAMPLES];
  uint32_t warmups_completed = 0u;
  for (; warmups_completed < config->warmup_count; warmups_completed += 1u) {
    w_seed_native_benchmark_sample ignored_sample;
    const native_benchmark_one_result one =
        run_one(config, command_line, command_line_length, frequency,
                &ignored_sample);
    if (one.status != W_SEED_NATIVE_BENCHMARK_OK) {
      publish_result(result, one.status, warmups_completed, 0u, UINT32_MAX,
                     one.error);
      return one.status;
    }
  }

  uint32_t samples_completed = 0u;
  for (; samples_completed < config->sample_count; samples_completed += 1u) {
    w_seed_native_benchmark_sample sample;
    const native_benchmark_one_result one =
        run_one(config, command_line, command_line_length, frequency, &sample);
    if (one.status != W_SEED_NATIVE_BENCHMARK_OK) {
      publish_result(result, one.status, warmups_completed, samples_completed,
                     samples_completed, one.error);
      return one.status;
    }
    staged_samples[samples_completed] = sample;
  }
  (void)memcpy(samples, staged_samples,
               (size_t)samples_completed * sizeof(staged_samples[0]));
  publish_result(result, W_SEED_NATIVE_BENCHMARK_OK, warmups_completed,
                 samples_completed, UINT32_MAX, ERROR_SUCCESS);
  return W_SEED_NATIVE_BENCHMARK_OK;
}

#endif
