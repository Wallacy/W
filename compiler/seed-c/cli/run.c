#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif

#include "run.h"

#include <string.h>

bool w_seed_run_parse(int argc, char **argv, w_seed_run_request *request) {
  if (request != NULL) *request = (w_seed_run_request){NULL, 0u, NULL};
  if (request == NULL || argv == NULL || argc < 3 || argv[1] == NULL ||
      strcmp(argv[1], "run") != 0 || argv[2] == NULL ||
      argv[2][0] == '\0' || argv[2][0] == '-')
    return false;
  const size_t path_length = strlen(argv[2]);
  if (path_length < 3u || argv[2][path_length - 2u] != '.' ||
      argv[2][path_length - 1u] != 'w')
    return false;
  size_t argument_count = 0u;
  char *const *arguments = NULL;
  if (argc > 3) {
    if (argv[3] == NULL || strcmp(argv[3], "--") != 0) return false;
    argument_count = (size_t)argc - 4u;
    if (argument_count > W_SEED_RUN_MAX_ARGUMENTS) return false;
    if (argument_count != 0u) arguments = &argv[4];
  }
  for (size_t index = 0u; index < argument_count; index += 1u)
    if (argv[index + 4u] == NULL) return false;
  request->path = argv[2];
  request->argument_count = argument_count;
  request->arguments = arguments;
  return true;
}

#if defined(__linux__)

#include "check_host.h"
#include "w_seed_native0.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static w_seed_native0_storage native_storage;

static const char MLIR_OPT[] = "/usr/bin/mlir-opt-20";
static const char MLIR_TRANSLATE[] = "/usr/bin/mlir-translate-20";
static const char CLANG[] = "/usr/bin/clang-20";

static bool path_join(char *buffer, size_t capacity, const char *directory,
                      const char *name) {
  if (buffer == NULL || capacity == 0u || directory == NULL || name == NULL)
    return false;
  const int length = snprintf(buffer, capacity, "%s/%s", directory, name);
  return length >= 0 && (size_t)length < capacity;
}

static bool write_all(int descriptor, const uint8_t *bytes, size_t length) {
  if (descriptor < 0 || (bytes == NULL && length != 0u)) return false;
  size_t offset = 0u;
  while (offset < length) {
    const ssize_t written = write(descriptor, bytes + offset, length - offset);
    if (written < 0 && errno == EINTR) continue;
    if (written <= 0) return false;
    offset += (size_t)written;
  }
  return true;
}

static bool create_private_file(const char *path, mode_t mode) {
  if (path == NULL) return false;
  const int descriptor = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC,
                              mode);
  if (descriptor < 0) return false;
  const bool closed = close(descriptor) == 0;
  if (!closed) (void)unlink(path);
  return closed;
}

static bool write_private_file(const char *path, const uint8_t *bytes,
                               size_t length) {
  if (path == NULL) return false;
  const int descriptor = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC,
                              (mode_t)0600);
  if (descriptor < 0) return false;
  const bool written = write_all(descriptor, bytes, length);
  const bool closed = close(descriptor) == 0;
  if (!written || !closed) (void)unlink(path);
  return written && closed;
}

static bool remove_file(const char *path) {
  return path == NULL || path[0] == '\0' || unlink(path) == 0 ||
         errno == ENOENT;
}

static bool cleanup_directory(const char *directory, const char *input_path,
                              const char *verified_path, const char *ll_path,
                              const char *program_path) {
  bool clean = remove_file(input_path);
  clean = remove_file(verified_path) && clean;
  clean = remove_file(ll_path) && clean;
  clean = remove_file(program_path) && clean;
  if (directory != NULL && directory[0] != '\0' && rmdir(directory) != 0 &&
      errno != ENOENT)
    clean = false;
  return clean;
}

static int wait_for_tool(pid_t child) {
  int status = 0;
  while (waitpid(child, &status, 0) < 0) {
    if (errno == EINTR) continue;
    return 3;
  }
  if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return 0;
  if (WIFEXITED(status)) return 2;
  return 3;
}

static int run_tool(const char *executable, char *const arguments[]) {
  if (executable == NULL || arguments == NULL || access(executable, X_OK) != 0)
    return 2;
  const pid_t child = fork();
  if (child < 0) return 3;
  if (child == 0) {
    const int null_descriptor =
        open("/dev/null", O_WRONLY | O_CLOEXEC, (mode_t)0600);
    if (null_descriptor < 0 || dup2(null_descriptor, STDOUT_FILENO) < 0)
      _exit(127);
    if (null_descriptor != STDOUT_FILENO) (void)close(null_descriptor);
    execv(executable, arguments);
    _exit(127);
  }
  return wait_for_tool(child);
}

static int wait_for_program(pid_t child) {
  int status = 0;
  while (waitpid(child, &status, 0) < 0) {
    if (errno == EINTR) continue;
    return 3;
  }
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  if (WIFSIGNALED(status)) {
    const int signal_number = WTERMSIG(status);
    return signal_number <= 127 ? 128 + signal_number : 255;
  }
  return 3;
}

static int run_program(const char *path, const w_seed_run_request *request) {
  if (path == NULL || request == NULL ||
      request->argument_count > W_SEED_RUN_MAX_ARGUMENTS ||
      (request->argument_count != 0u && request->arguments == NULL))
    return 2;
  char *arguments[W_SEED_RUN_MAX_ARGUMENTS + 2u];
  arguments[0] = (char *)path;
  for (size_t index = 0u; index < request->argument_count; index += 1u)
    arguments[index + 1u] = request->arguments[index];
  arguments[request->argument_count + 1u] = NULL;
  const pid_t child = fork();
  if (child < 0) return 3;
  if (child == 0) {
    execv(path, arguments);
    _exit(127);
  }
  return wait_for_program(child);
}

static int native_status_exit(w_seed_native0_status status) {
  switch (status) {
    case W_SEED_NATIVE0_SOURCE:
    case W_SEED_NATIVE0_PARSE:
    case W_SEED_NATIVE0_FRONTEND:
    case W_SEED_NATIVE0_HIR:
    case W_SEED_NATIVE0_CAPACITY:
    case W_SEED_NATIVE0_UNSUPPORTED:
      return 2;
    case W_SEED_NATIVE0_MLIR:
    case W_SEED_NATIVE0_INVALID:
    case W_SEED_NATIVE0_OK:
      return status == W_SEED_NATIVE0_OK ? 0 : 3;
  }
  return 3;
}

int w_seed_run_execute(const w_seed_run_request *request) {
  if (request == NULL || request->path == NULL ||
      request->argument_count > W_SEED_RUN_MAX_ARGUMENTS ||
      (request->argument_count != 0u && request->arguments == NULL))
    return 2;
  const size_t path_length = strlen(request->path);
  if (path_length == 0u || path_length > W_SEED_NATIVE0_MAX_PATH_BYTES)
    return 2;
  w_seed_frontend_text source_id;
  if (w_seed_check_root_source_id(request->path, path_length, &source_id) !=
      W_SEED_CHECK_HOST_OK)
    return 2;

  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  const w_seed_native0_input native_input = {
      .path = request->path,
      .path_length = path_length,
      .logical_source_id = source_id,
      .target =
          (w_seed_mlir0_target){W_SEED_MLIR0_TARGET_X86_64_UNKNOWN_LINUX_GNU}};
  const w_seed_native0_output native_output = {artifact, sizeof(artifact)};
  w_seed_native0_result native_result;
  const int source_status = native_status_exit(w_seed_native0_run(
      &native_input, &native_storage, &native_output, &native_result));
  if (source_status != 0) return source_status;

  char directory[] = "/tmp/w-run-XXXXXX";
  if (mkdtemp(directory) == NULL) return 3;
  char input_path[PATH_MAX] = {0};
  char verified_path[PATH_MAX] = {0};
  char ll_path[PATH_MAX] = {0};
  char program_path[PATH_MAX] = {0};
  int exit_code = 3;
  if (!path_join(input_path, sizeof(input_path), directory, "input.mlir") ||
      !path_join(verified_path, sizeof(verified_path), directory,
                 "verified.mlir") ||
      !path_join(ll_path, sizeof(ll_path), directory, "output.ll") ||
      !path_join(program_path, sizeof(program_path), directory, "program") ||
      !write_private_file(input_path, artifact,
                          native_result.mlir.written.mlir_bytes) ||
      !create_private_file(verified_path, (mode_t)0600) ||
      !create_private_file(ll_path, (mode_t)0600) ||
      !create_private_file(program_path, (mode_t)0700))
    goto cleanup;

  {
    char *arguments[] = {(char *)MLIR_OPT, input_path, (char *)"-o",
                         verified_path, (char *)"--verify-each", NULL};
    exit_code = run_tool(MLIR_OPT, arguments);
  }
  if (exit_code != 0) goto cleanup;
  {
    char *arguments[] = {(char *)MLIR_TRANSLATE, (char *)"--mlir-to-llvmir",
                         verified_path, (char *)"-o", ll_path, NULL};
    exit_code = run_tool(MLIR_TRANSLATE, arguments);
  }
  if (exit_code != 0) goto cleanup;
  {
    char *arguments[] = {
        (char *)CLANG,
        (char *)"-x",
        (char *)"ir",
        (char *)"--target=x86_64-unknown-linux-gnu",
        ll_path,
        (char *)"-o",
        program_path,
        NULL};
    exit_code = run_tool(CLANG, arguments);
  }
  if (exit_code != 0) goto cleanup;
  if (chmod(program_path, (mode_t)0700) != 0) goto cleanup;
  exit_code = run_program(program_path, request);

cleanup:
  if (!cleanup_directory(directory, input_path, verified_path, ll_path,
                          program_path))
    return 3;
  return exit_code;
}

#else

int w_seed_run_execute(const w_seed_run_request *request) {
  (void)request;
  return 2;
}

#endif
