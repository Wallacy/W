#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define W_LINUX_BENCHMARK_SCHEMA "w-linux-native-benchmark/1"
#define W_LINUX_BENCHMARK_MAX_ARGUMENTS 64u
#define W_LINUX_BENCHMARK_MAX_SAMPLES 1001u
#define W_LINUX_BENCHMARK_MAX_CAPTURE_BYTES 65536u
#define W_LINUX_BENCHMARK_MAX_TIMEOUT_MS 120000u

typedef struct {
  const char *executable;
  const char *working_directory;
  const char *arguments[W_LINUX_BENCHMARK_MAX_ARGUMENTS];
  size_t argument_count;
  uint32_t warmup_count;
  uint32_t sample_count;
  uint32_t timeout_ms;
  bool oracle_enabled;
  uint32_t expected_exit_code;
  uint8_t expected_stdout[W_LINUX_BENCHMARK_MAX_CAPTURE_BYTES];
  size_t expected_stdout_bytes;
  uint8_t expected_stderr[W_LINUX_BENCHMARK_MAX_CAPTURE_BYTES];
  size_t expected_stderr_bytes;
} benchmark_options;

typedef struct {
  uint64_t wall_ns;
  uint64_t user_cpu_ns;
  uint64_t system_cpu_ns;
  uint64_t peak_rss_bytes;
  uint32_t exit_code;
  uint32_t stdout_bytes;
  uint32_t stderr_bytes;
} benchmark_sample;

typedef enum {
  BENCHMARK_OK = 0,
  BENCHMARK_INVALID_ARGUMENT,
  BENCHMARK_PATH,
  BENCHMARK_PIPE,
  BENCHMARK_FORK,
  BENCHMARK_WAIT,
  BENCHMARK_TIMEOUT,
  BENCHMARK_CAPTURE,
  BENCHMARK_EXECUTION,
  BENCHMARK_ORACLE,
  BENCHMARK_TREE,
  BENCHMARK_METRICS,
} benchmark_status;

static const char *status_name(benchmark_status status) {
  switch (status) {
    case BENCHMARK_OK:
      return "ok";
    case BENCHMARK_INVALID_ARGUMENT:
      return "invalid-argument";
    case BENCHMARK_PATH:
      return "invalid-path";
    case BENCHMARK_PIPE:
      return "pipe";
    case BENCHMARK_FORK:
      return "fork";
    case BENCHMARK_WAIT:
      return "wait";
    case BENCHMARK_TIMEOUT:
      return "timeout";
    case BENCHMARK_CAPTURE:
      return "capture";
    case BENCHMARK_EXECUTION:
      return "execution";
    case BENCHMARK_ORACLE:
      return "oracle-mismatch";
    case BENCHMARK_TREE:
      return "process-tree-incomplete";
    case BENCHMARK_METRICS:
      return "metrics";
  }
  return "unknown";
}

static bool parse_u32(const char *text, uint32_t maximum,
                      uint32_t *value_out) {
  if (text == NULL || value_out == NULL || text[0] == '\0') return false;
  uint64_t value = 0u;
  for (size_t index = 0u; text[index] != '\0'; index += 1u) {
    if (text[index] < '0' || text[index] > '9') return false;
    value = value * 10u + (uint64_t)(text[index] - '0');
    if (value > maximum) return false;
  }
  *value_out = (uint32_t)value;
  return true;
}

static int hex_value(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}

static bool parse_hex(const char *text, uint8_t *output, size_t capacity,
                      size_t *written_out) {
  if (text == NULL || output == NULL || written_out == NULL) return false;
  const size_t length = strlen(text);
  if ((length & 1u) != 0u || length / 2u > capacity) return false;
  for (size_t index = 0u; index < length / 2u; index += 1u) {
    const int high = hex_value(text[index * 2u]);
    const int low = hex_value(text[index * 2u + 1u]);
    if (high < 0 || low < 0) return false;
    output[index] = (uint8_t)((high << 4) | low);
  }
  *written_out = length / 2u;
  return true;
}

static bool next_value(int argc, char **argv, int *index,
                       const char **value_out) {
  if (argv == NULL || index == NULL || value_out == NULL ||
      *index + 1 >= argc)
    return false;
  *index += 1;
  *value_out = argv[*index];
  return *value_out != NULL;
}

static bool parse_options(int argc, char **argv, benchmark_options *options) {
  if (argc < 1 || argv == NULL || options == NULL) return false;
  *options = (benchmark_options){.warmup_count = 1u,
                                .sample_count = 9u,
                                .timeout_ms =
                                    W_LINUX_BENCHMARK_MAX_TIMEOUT_MS};
  for (int index = 1; index < argc; index += 1) {
    const char *value = NULL;
    if (strcmp(argv[index], "--exe") == 0) {
      if (options->executable != NULL ||
          !next_value(argc, argv, &index, &value))
        return false;
      options->executable = value;
    } else if (strcmp(argv[index], "--cwd") == 0) {
      if (options->working_directory != NULL ||
          !next_value(argc, argv, &index, &value))
        return false;
      options->working_directory = value;
    } else if (strcmp(argv[index], "--arg") == 0) {
      if (!next_value(argc, argv, &index, &value) ||
          options->argument_count >= W_LINUX_BENCHMARK_MAX_ARGUMENTS)
        return false;
      options->arguments[options->argument_count++] = value;
    } else if (strcmp(argv[index], "--warmup") == 0) {
      if (!next_value(argc, argv, &index, &value) ||
          !parse_u32(value, W_LINUX_BENCHMARK_MAX_SAMPLES,
                     &options->warmup_count))
        return false;
    } else if (strcmp(argv[index], "--samples") == 0) {
      if (!next_value(argc, argv, &index, &value) ||
          !parse_u32(value, W_LINUX_BENCHMARK_MAX_SAMPLES,
                     &options->sample_count) ||
          options->sample_count == 0u)
        return false;
    } else if (strcmp(argv[index], "--timeout-ms") == 0) {
      if (!next_value(argc, argv, &index, &value) ||
          !parse_u32(value, W_LINUX_BENCHMARK_MAX_TIMEOUT_MS,
                     &options->timeout_ms) ||
          options->timeout_ms == 0u)
        return false;
    } else if (strcmp(argv[index], "--expect-exit") == 0) {
      if (!next_value(argc, argv, &index, &value) ||
          !parse_u32(value, 255u, &options->expected_exit_code))
        return false;
      options->oracle_enabled = true;
    } else if (strcmp(argv[index], "--expect-stdout-hex") == 0) {
      if (!next_value(argc, argv, &index, &value) ||
          !parse_hex(value, options->expected_stdout,
                     sizeof(options->expected_stdout),
                     &options->expected_stdout_bytes))
        return false;
      options->oracle_enabled = true;
    } else if (strcmp(argv[index], "--expect-stderr-hex") == 0) {
      if (!next_value(argc, argv, &index, &value) ||
          !parse_hex(value, options->expected_stderr,
                     sizeof(options->expected_stderr),
                     &options->expected_stderr_bytes))
        return false;
      options->oracle_enabled = true;
    } else {
      return false;
    }
  }
  return options->executable != NULL && options->working_directory != NULL;
}

static bool absolute_regular_executable(const char *path) {
  struct stat value;
  return path != NULL && path[0] == '/' && lstat(path, &value) == 0 &&
         S_ISREG(value.st_mode) && !S_ISLNK(value.st_mode) &&
         access(path, X_OK) == 0;
}

static bool absolute_directory(const char *path) {
  struct stat value;
  return path != NULL && path[0] == '/' && lstat(path, &value) == 0 &&
         S_ISDIR(value.st_mode) && !S_ISLNK(value.st_mode);
}

static bool clock_now(uint64_t *value_out) {
  struct timespec value;
  if (value_out == NULL || clock_gettime(CLOCK_MONOTONIC, &value) != 0 ||
      value.tv_sec < 0 || value.tv_nsec < 0)
    return false;
  const uint64_t seconds = (uint64_t)value.tv_sec;
  if (seconds > (UINT64_MAX - (uint64_t)value.tv_nsec) / 1000000000u)
    return false;
  *value_out = seconds * 1000000000u + (uint64_t)value.tv_nsec;
  return true;
}

static uint64_t timeval_ns(struct timeval value) {
  if (value.tv_sec < 0 || value.tv_usec < 0) return UINT64_MAX;
  const uint64_t seconds = (uint64_t)value.tv_sec;
  if (seconds > (UINT64_MAX - (uint64_t)value.tv_usec * 1000u) /
                    1000000000u)
    return UINT64_MAX;
  return seconds * 1000000000u + (uint64_t)value.tv_usec * 1000u;
}

static void close_fd(int *value) {
  if (value != NULL && *value >= 0) {
    (void)close(*value);
    *value = -1;
  }
}

static bool set_nonblocking(int descriptor) {
  const int flags = fcntl(descriptor, F_GETFL, 0);
  return flags >= 0 && fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) == 0;
}

static bool drain_pipe(int *descriptor, uint8_t *output, size_t capacity,
                       size_t *written, bool *open_out) {
  uint8_t buffer[4096];
  if (descriptor == NULL || output == NULL || written == NULL ||
      open_out == NULL)
    return false;
  while (*descriptor >= 0) {
    const ssize_t count = read(*descriptor, buffer, sizeof(buffer));
    if (count > 0) {
      if ((size_t)count > capacity - *written) return false;
      (void)memcpy(output + *written, buffer, (size_t)count);
      *written += (size_t)count;
      continue;
    }
    if (count == 0) {
      close_fd(descriptor);
      *open_out = false;
      return true;
    }
    if (errno == EINTR) continue;
    if (errno == EAGAIN || errno == EWOULDBLOCK) return true;
    return false;
  }
  *open_out = false;
  return true;
}

static void terminate_group(pid_t child) {
  if (child <= 0) return;
  (void)kill(-child, SIGKILL);
  (void)kill(child, SIGKILL);
}

static benchmark_status run_sample(const benchmark_options *options,
                                   benchmark_sample *sample, int *error_out) {
  int stdout_pipe[2] = {-1, -1};
  int stderr_pipe[2] = {-1, -1};
  uint8_t stdout_bytes[W_LINUX_BENCHMARK_MAX_CAPTURE_BYTES];
  uint8_t stderr_bytes[W_LINUX_BENCHMARK_MAX_CAPTURE_BYTES];
  size_t stdout_written = 0u;
  size_t stderr_written = 0u;
  uint64_t started = 0u;
  uint64_t ended = 0u;
  if (options == NULL || sample == NULL || error_out == NULL)
    return BENCHMARK_METRICS;
  if (pipe2(stdout_pipe, O_CLOEXEC) != 0 ||
      pipe2(stderr_pipe, O_CLOEXEC) != 0) {
    *error_out = errno;
    close_fd(&stdout_pipe[0]);
    close_fd(&stdout_pipe[1]);
    close_fd(&stderr_pipe[0]);
    close_fd(&stderr_pipe[1]);
    return BENCHMARK_PIPE;
  }
  if (!set_nonblocking(stdout_pipe[0]) ||
      !set_nonblocking(stderr_pipe[0])) {
    *error_out = errno;
    close_fd(&stdout_pipe[0]);
    close_fd(&stdout_pipe[1]);
    close_fd(&stderr_pipe[0]);
    close_fd(&stderr_pipe[1]);
    return BENCHMARK_PIPE;
  }
  if (!clock_now(&started)) {
    close_fd(&stdout_pipe[0]);
    close_fd(&stdout_pipe[1]);
    close_fd(&stderr_pipe[0]);
    close_fd(&stderr_pipe[1]);
    return BENCHMARK_METRICS;
  }

  const pid_t child = fork();
  if (child < 0) {
    *error_out = errno;
    close_fd(&stdout_pipe[0]);
    close_fd(&stdout_pipe[1]);
    close_fd(&stderr_pipe[0]);
    close_fd(&stderr_pipe[1]);
    return BENCHMARK_FORK;
  }
  if (child == 0) {
    if (setpgid(0, 0) != 0 || chdir(options->working_directory) != 0 ||
        dup2(stdout_pipe[1], STDOUT_FILENO) < 0 ||
        dup2(stderr_pipe[1], STDERR_FILENO) < 0)
      _exit(126);
    close_fd(&stdout_pipe[0]);
    close_fd(&stdout_pipe[1]);
    close_fd(&stderr_pipe[0]);
    close_fd(&stderr_pipe[1]);
    char *arguments[W_LINUX_BENCHMARK_MAX_ARGUMENTS + 2u];
    arguments[0] = (char *)options->executable;
    for (size_t index = 0u; index < options->argument_count; index += 1u)
      arguments[index + 1u] = (char *)options->arguments[index];
    arguments[options->argument_count + 1u] = NULL;
    execv(options->executable, arguments);
    _exit(127);
  }

  (void)setpgid(child, child);
  close_fd(&stdout_pipe[1]);
  close_fd(&stderr_pipe[1]);
  bool stdout_open = true;
  bool stderr_open = true;
  bool child_done = false;
  int wait_status = 0;
  struct rusage usage;
  (void)memset(&usage, 0, sizeof(usage));
  benchmark_status status = BENCHMARK_OK;
  const uint64_t timeout_ns = (uint64_t)options->timeout_ms * 1000000u;

  while (!child_done || stdout_open || stderr_open) {
    uint64_t now = 0u;
    if (!clock_now(&now)) {
      status = BENCHMARK_METRICS;
      break;
    }
    if (now - started >= timeout_ns) {
      status = BENCHMARK_TIMEOUT;
      break;
    }
    struct pollfd descriptors[2] = {
        {.fd = stdout_pipe[0], .events = POLLIN | POLLHUP},
        {.fd = stderr_pipe[0], .events = POLLIN | POLLHUP}};
    const uint64_t remaining_ns = timeout_ns - (now - started);
    uint64_t remaining_ms = (remaining_ns + 999999u) / 1000000u;
    if (remaining_ms > 10u) remaining_ms = 10u;
    const int poll_status = poll(descriptors, 2u, (int)remaining_ms);
    if (poll_status < 0 && errno != EINTR) {
      *error_out = errno;
      status = BENCHMARK_CAPTURE;
      break;
    }
    if (stdout_open &&
        (poll_status == 0 ||
         (descriptors[0].revents & (POLLIN | POLLHUP | POLLERR)) != 0) &&
        !drain_pipe(&stdout_pipe[0], stdout_bytes, sizeof(stdout_bytes),
                    &stdout_written, &stdout_open)) {
      status = BENCHMARK_CAPTURE;
      break;
    }
    if (stderr_open &&
        (poll_status == 0 ||
         (descriptors[1].revents & (POLLIN | POLLHUP | POLLERR)) != 0) &&
        !drain_pipe(&stderr_pipe[0], stderr_bytes, sizeof(stderr_bytes),
                    &stderr_written, &stderr_open)) {
      status = BENCHMARK_CAPTURE;
      break;
    }
    if (!child_done) {
      const pid_t waited = wait4(child, &wait_status, WNOHANG, &usage);
      if (waited == child) {
        child_done = true;
      } else if (waited < 0 && errno != EINTR) {
        *error_out = errno;
        status = BENCHMARK_WAIT;
        break;
      }
    }
  }

  if (status != BENCHMARK_OK) terminate_group(child);
  if (!child_done) {
    for (;;) {
      const pid_t waited = wait4(child, &wait_status, 0, &usage);
      if (waited == child) {
        child_done = true;
        break;
      }
      if (errno == EINTR) continue;
      if (status == BENCHMARK_OK) {
        *error_out = errno;
        status = BENCHMARK_WAIT;
      }
      break;
    }
  }
  if (status == BENCHMARK_OK) {
    errno = 0;
    if (kill(-child, 0) == 0 || errno == EPERM) {
      terminate_group(child);
      status = BENCHMARK_TREE;
    } else if (errno != ESRCH) {
      *error_out = errno;
      status = BENCHMARK_TREE;
    }
  }
  close_fd(&stdout_pipe[0]);
  close_fd(&stderr_pipe[0]);
  if (status != BENCHMARK_OK) return status;
  if (!clock_now(&ended) || ended <= started || !WIFEXITED(wait_status))
    return BENCHMARK_EXECUTION;

  const uint64_t user_ns = timeval_ns(usage.ru_utime);
  const uint64_t system_ns = timeval_ns(usage.ru_stime);
  if (user_ns == UINT64_MAX || system_ns == UINT64_MAX ||
      user_ns > UINT64_MAX - system_ns || usage.ru_maxrss <= 0 ||
      (uint64_t)usage.ru_maxrss > UINT64_MAX / 1024u)
    return BENCHMARK_METRICS;
  const uint32_t exit_code = (uint32_t)WEXITSTATUS(wait_status);
  if (options->oracle_enabled &&
      (exit_code != options->expected_exit_code ||
       stdout_written != options->expected_stdout_bytes ||
       stderr_written != options->expected_stderr_bytes ||
       memcmp(stdout_bytes, options->expected_stdout, stdout_written) != 0 ||
       memcmp(stderr_bytes, options->expected_stderr, stderr_written) != 0))
    return BENCHMARK_ORACLE;
  *sample = (benchmark_sample){
      .wall_ns = ended - started,
      .user_cpu_ns = user_ns,
      .system_cpu_ns = system_ns,
      .peak_rss_bytes = (uint64_t)usage.ru_maxrss * 1024u,
      .exit_code = exit_code,
      .stdout_bytes = (uint32_t)stdout_written,
      .stderr_bytes = (uint32_t)stderr_written};
  return BENCHMARK_OK;
}

static bool print_error(benchmark_status status, int error_number,
                        uint32_t failed_index, bool warmup) {
  if (printf("{\"schema\":\"%s\",\"status\":\"error\"," 
               "\"error\":\"%s\",\"errno\":%d,\"failedIndex\":%" PRIu32
               ",\"phase\":\"%s\"}\n",
               W_LINUX_BENCHMARK_SCHEMA, status_name(status), error_number,
               failed_index, warmup ? "warmup" : "sample") < 0)
    return false;
  return fflush(stdout) == 0 && !ferror(stdout);
}

static bool print_success(const benchmark_options *options,
                          const benchmark_sample *samples) {
  if (printf("{\"schema\":\"%s\",\"status\":\"ok\"," 
               "\"warmupCount\":%" PRIu32 ",\"sampleCount\":%" PRIu32
               ",\"oracle\":%s,\"samples\":[",
               W_LINUX_BENCHMARK_SCHEMA, options->warmup_count,
               options->sample_count, options->oracle_enabled ? "true" : "false") < 0)
    return false;
  for (size_t index = 0u; index < options->sample_count; index += 1u) {
    const benchmark_sample sample = samples[index];
    if (index != 0u && fputc(',', stdout) == EOF) return false;
    if (printf("{\"wallNs\":%" PRIu64 ",\"userCpuNs\":%" PRIu64
                 ",\"systemCpuNs\":%" PRIu64 ",\"cpuNs\":%" PRIu64
                 ",\"peakRssBytes\":%" PRIu64 ",\"exitCode\":%" PRIu32
                 ",\"stdoutBytes\":%" PRIu32 ",\"stderrBytes\":%" PRIu32 "}",
                 sample.wall_ns, sample.user_cpu_ns, sample.system_cpu_ns,
                 sample.user_cpu_ns + sample.system_cpu_ns,
                 sample.peak_rss_bytes, sample.exit_code,
                 sample.stdout_bytes, sample.stderr_bytes) < 0)
      return false;
  }
  if (printf("],\"measurement\":\"Linux CLOCK_MONOTONIC wall time; one "
               "fresh fork/exec/wait4 child per sample; root-process rusage "
               "CPU and peak RSS; direct-child process-group best-effort "
               "timeout containment\"}\n") < 0)
    return false;
  return fflush(stdout) == 0 && !ferror(stdout);
}

int main(int argc, char **argv) {
  benchmark_options options;
  if (!parse_options(argc, argv, &options)) {
    (void)print_error(BENCHMARK_INVALID_ARGUMENT, 0, 0u, false);
    return 2;
  }
  if (!absolute_regular_executable(options.executable) ||
      !absolute_directory(options.working_directory)) {
    (void)print_error(BENCHMARK_PATH, errno, 0u, false);
    return 2;
  }

  benchmark_sample samples[W_LINUX_BENCHMARK_MAX_SAMPLES];
  benchmark_sample ignored;
  for (uint32_t index = 0u; index < options.warmup_count; index += 1u) {
    int error_number = 0;
    const benchmark_status status = run_sample(&options, &ignored, &error_number);
    if (status != BENCHMARK_OK) {
      (void)print_error(status, error_number, index, true);
      return 1;
    }
  }
  for (uint32_t index = 0u; index < options.sample_count; index += 1u) {
    int error_number = 0;
    const benchmark_status status =
        run_sample(&options, &samples[index], &error_number);
    if (status != BENCHMARK_OK) {
      (void)print_error(status, error_number, index, false);
      return 1;
    }
  }
  return print_success(&options, samples) ? 0 : 1;
}
