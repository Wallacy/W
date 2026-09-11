#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif

#include "run.h"

#include <string.h>

#include "w_seed_native0.h"

#if defined(_WIN32)
#include "w_seed_windows_config.h"
#elif defined(__linux__)
#include "w_seed_linux_config.h"
#endif

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

#if (defined(__linux__) && W_SEED_LINUX_NATIVE_RUN_ENABLED) || \
    (defined(_WIN32) && W_SEED_WINDOWS_NATIVE_RUN_ENABLED)
/* Run accepts a source basename as an opaque logical identity. The argv path
 * has already been NUL-terminated by the host, so this helper only derives
 * the final basename and preserves the public .w extension check. */
static bool run_logical_source_id(const char *path, size_t length,
                                  w_seed_frontend_text *source_id) {
  if (source_id != NULL) *source_id = (w_seed_frontend_text){NULL, 0u};
  if (path == NULL || length < 3u || source_id == NULL ||
      path[length - 2u] != '.' || path[length - 1u] != 'w')
    return false;
  size_t basename_start = 0u;
  for (size_t index = 0u; index < length; index += 1u)
    if (path[index] == '/' || path[index] == '\\') basename_start = index + 1u;
  if (basename_start >= length) return false;
  *source_id = (w_seed_frontend_text){path + basename_start,
                                      length - basename_start};
  return true;
}
#endif

#if defined(__linux__) && W_SEED_LINUX_NATIVE_RUN_ENABLED

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

static const char MLIR_OPT[] = W_SEED_LINUX_MLIR_OPT_PATH;
static const char MLIR_TRANSLATE[] = W_SEED_LINUX_MLIR_TRANSLATE_PATH;
static const char LLC[] = W_SEED_LINUX_LLC_PATH;
static const char LINK_DRIVER[] = W_SEED_LINUX_LINK_DRIVER_PATH;

/* WRT0 is the complete runtime closure of the bounded Linux seed executable.
 * It owns process startup, stdout and exit through the x86_64 Linux syscall
 * ABI. The final artifact therefore needs neither a C entry point nor libc,
 * CRT objects, a dynamic loader, or generated C source. */
static const uint8_t WRT0_LL[] =
    "target triple = \"x86_64-unknown-linux-gnu\"\n"
    "\n"
    "declare i32 @main()\n"
    "\n"
    "define i64 @write(i32 %fd, ptr %buffer, i64 %count) nounwind {\n"
    "entry:\n"
    "  %result = call i64 asm sideeffect \"syscall\", \"={rax},{rax},{rdi},{rsi},{rdx},~{rcx},~{r11},~{memory}\"(i64 1, i32 %fd, ptr %buffer, i64 %count)\n"
    "  ret i64 %result\n"
    "}\n"
    "\n"
    "define void @_start() noreturn nounwind {\n"
    "entry:\n"
    "  %status = call i32 @main()\n"
    "  call void asm sideeffect \"syscall\", \"{rax},{rdi},~{rcx},~{r11},~{memory}\"(i64 60, i32 %status)\n"
    "  unreachable\n"
    "}\n";

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
                              const char *object_path,
                              const char *runtime_ll_path,
                              const char *runtime_object_path,
                              const char *program_path) {
  bool clean = remove_file(input_path);
  clean = remove_file(verified_path) && clean;
  clean = remove_file(ll_path) && clean;
  clean = remove_file(object_path) && clean;
  clean = remove_file(runtime_ll_path) && clean;
  clean = remove_file(runtime_object_path) && clean;
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

int w_seed_run_compile(const w_seed_run_compile_request *request) {
  if (request == NULL || request->source_path == NULL ||
      request->target == NULL || request->directory == NULL ||
      request->artifact_path == NULL || request->directory[0] == '\0' ||
      request->artifact_path[0] == '\0' ||
      strcmp(request->target, W_SEED_NATIVE_TARGET_LINUX) != 0 ||
      (request->profile != W_SEED_RUN_COMPILE_PROFILE_DEV &&
       request->profile != W_SEED_RUN_COMPILE_PROFILE_RELEASE))
    return 2;
  const size_t path_length = strlen(request->source_path);
  if (path_length == 0u || path_length > W_SEED_NATIVE0_MAX_PATH_BYTES)
    return 2;
  w_seed_frontend_text source_id;
  if (!run_logical_source_id(request->source_path, path_length, &source_id))
    return 2;

  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  const w_seed_native0_input native_input = {
      .path = request->source_path,
      .path_length = path_length,
      .logical_source_id = source_id,
      .target =
          (w_seed_mlir0_target){W_SEED_MLIR0_TARGET_X86_64_UNKNOWN_LINUX_GNU}};
  const w_seed_native0_output native_output = {artifact, sizeof(artifact)};
  w_seed_native0_result native_result;
  const int source_status = native_status_exit(w_seed_native0_run(
      &native_input, &native_storage, &native_output, &native_result));

  char input_path[PATH_MAX] = {0};
  char verified_path[PATH_MAX] = {0};
  char ll_path[PATH_MAX] = {0};
  char object_path[PATH_MAX] = {0};
  char runtime_ll_path[PATH_MAX] = {0};
  char runtime_object_path[PATH_MAX] = {0};
  int exit_code = source_status;
  if (source_status != 0) {
    if (!cleanup_directory(request->directory, NULL, NULL, NULL, NULL, NULL,
                           NULL, request->artifact_path))
      return 3;
    return source_status;
  }
  if (!path_join(input_path, sizeof(input_path), request->directory,
                 "input.mlir") ||
      !path_join(verified_path, sizeof(verified_path), request->directory,
                 "verified.mlir") ||
      !path_join(ll_path, sizeof(ll_path), request->directory, "output.ll") ||
      !path_join(object_path, sizeof(object_path), request->directory,
                 "output.o") ||
      !path_join(runtime_ll_path, sizeof(runtime_ll_path), request->directory,
                 "wrt0.ll") ||
      !path_join(runtime_object_path, sizeof(runtime_object_path),
                 request->directory, "wrt0.o") ||
      !write_private_file(input_path, artifact,
                          native_result.mlir.written.mlir_bytes) ||
      !write_private_file(runtime_ll_path, WRT0_LL, sizeof(WRT0_LL) - 1u) ||
      !create_private_file(verified_path, (mode_t)0600) ||
      !create_private_file(ll_path, (mode_t)0600) ||
      !create_private_file(object_path, (mode_t)0600) ||
      !create_private_file(runtime_object_path, (mode_t)0600) ||
      !create_private_file(request->artifact_path, (mode_t)0700))
    goto cleanup;

  {
    char *arguments[10] = {
        (char *)MLIR_OPT,
        input_path,
        (char *)"-o",
        verified_path,
        (char *)"--convert-scf-to-cf",
        (char *)"--convert-cf-to-llvm",
        (char *)"--verify-each",
        NULL,
        NULL,
        NULL};
    if (request->profile == W_SEED_RUN_COMPILE_PROFILE_RELEASE) {
      arguments[7] = (char *)"--canonicalize";
      arguments[8] = (char *)"--cse";
    }
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
    char *arguments[9] = {
        (char *)LLC,
        (char *)"-mtriple=x86_64-unknown-linux-gnu",
        (char *)"-filetype=obj",
        (char *)"-relocation-model=pic",
        ll_path,
        (char *)"-o",
        object_path,
        NULL,
        NULL};
    if (request->profile == W_SEED_RUN_COMPILE_PROFILE_RELEASE)
      arguments[7] = (char *)"-O3";
    exit_code = run_tool(LLC, arguments);
  }
  if (exit_code != 0) goto cleanup;
  {
    char *arguments[9] = {
        (char *)LLC,
        (char *)"-mtriple=x86_64-unknown-linux-gnu",
        (char *)"-filetype=obj",
        (char *)"-relocation-model=pic",
        runtime_ll_path,
        (char *)"-o",
        runtime_object_path,
        NULL,
        NULL};
    if (request->profile == W_SEED_RUN_COMPILE_PROFILE_RELEASE)
      arguments[7] = (char *)"-O3";
    exit_code = run_tool(LLC, arguments);
  }
  if (exit_code != 0) goto cleanup;
  {
    char *arguments[15] = {0};
    size_t count = 0u;
    arguments[count++] = (char *)LINK_DRIVER;
    arguments[count++] = (char *)"-pie";
    arguments[count++] = (char *)"--no-dynamic-linker";
    arguments[count++] = (char *)"-e";
    arguments[count++] = (char *)"_start";
    arguments[count++] = (char *)"--gc-sections";
    arguments[count++] = (char *)"-z";
    arguments[count++] = (char *)"noexecstack";
    if (request->profile == W_SEED_RUN_COMPILE_PROFILE_RELEASE)
      arguments[count++] = (char *)"-s";
    arguments[count++] = object_path;
    arguments[count++] = runtime_object_path;
    arguments[count++] = (char *)"-o";
    arguments[count++] = (char *)request->artifact_path;
    arguments[count] = NULL;
    exit_code = run_tool(LINK_DRIVER, arguments);
  }
  if (exit_code != 0) goto cleanup;
  if (chmod(request->artifact_path, (mode_t)0700) != 0) goto cleanup;
  if (!remove_file(input_path) || !remove_file(verified_path) ||
      !remove_file(ll_path) || !remove_file(object_path) ||
      !remove_file(runtime_ll_path) || !remove_file(runtime_object_path)) {
    exit_code = 3;
    goto cleanup;
  }
  return 0;

cleanup:
  if (!cleanup_directory(request->directory, input_path, verified_path,
                          ll_path, object_path, runtime_ll_path,
                          runtime_object_path, request->artifact_path))
    return 3;
  return exit_code;
}

bool w_seed_run_cleanup_compiled(const char *directory,
                                 const char *artifact_path) {
  return cleanup_directory(directory, NULL, NULL, NULL, NULL, NULL, NULL,
                           artifact_path);
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
  if (!run_logical_source_id(request->path, path_length, &source_id)) return 2;

  char directory[] = "/tmp/w-run-XXXXXX";
  if (mkdtemp(directory) == NULL) return 3;
  char program_path[PATH_MAX] = {0};
  int exit_code = 3;
  if (!path_join(program_path, sizeof(program_path), directory, "program"))
    goto cleanup;
  {
    const w_seed_run_compile_request compile_request = {
        .source_path = request->path,
        .target = W_SEED_NATIVE_TARGET_LINUX,
        .directory = directory,
        .artifact_path = program_path,
        .profile = W_SEED_RUN_COMPILE_PROFILE_DEV};
    exit_code = w_seed_run_compile(&compile_request);
  }
  if (exit_code != 0) goto cleanup;
  exit_code = run_program(program_path, request);

cleanup:
  if (!w_seed_run_cleanup_compiled(directory, program_path)) return 3;
  return exit_code;
}

#elif defined(_WIN32)

#if W_SEED_WINDOWS_NATIVE_RUN_ENABLED

#include <limits.h>
#include <stdio.h>
#include <wchar.h>
#include <windows.h>
#include <bcrypt.h>

enum {
  /* Long-path opt-in is not part of this seed; retain the bounded path
   * contract until the source reader proves Unicode/extended-path support. */
  W_SEED_WINDOWS_PATH_CAPACITY = 4096,
  W_SEED_WINDOWS_COMMAND_CAPACITY = 32768,
  W_SEED_WINDOWS_ARGUMENT_CAPACITY = 8192,
  W_SEED_WINDOWS_RANDOM_BYTES = 16,
};

static w_seed_native0_storage native_storage;

typedef struct {
  wchar_t *data;
  size_t capacity;
  size_t length;
} windows_command;

static bool windows_utf8_to_wide(const char *source, wchar_t *destination,
                                 size_t capacity) {
  if (source == NULL || destination == NULL || capacity < 2u ||
      capacity > (size_t)INT_MAX)
    return false;
  const size_t source_length = strlen(source);
  if (source_length > (size_t)INT_MAX) return false;
  if (source_length == 0u) {
    destination[0] = L'\0';
    return true;
  }
  const int converted = MultiByteToWideChar(
      CP_UTF8, MB_ERR_INVALID_CHARS, source, (int)source_length, destination,
      (int)(capacity - 1u));
  if (converted <= 0) return false;
  destination[converted] = L'\0';
  return true;
}

static bool windows_wide_to_utf8(const wchar_t *source, char *destination,
                                 size_t capacity) {
  if (source == NULL || destination == NULL || capacity < 2u ||
      capacity > (size_t)INT_MAX)
    return false;
  const size_t source_length = wcslen(source);
  if (source_length > (size_t)INT_MAX) return false;
  const int converted = WideCharToMultiByte(
      CP_UTF8, WC_ERR_INVALID_CHARS, source, (int)source_length, destination,
      (int)(capacity - 1u), NULL, NULL);
  if (converted <= 0) return false;
  destination[converted] = '\0';
  return true;
}

static bool windows_command_append(windows_command *command,
                                   const wchar_t *text, size_t length) {
  if (command == NULL || command->data == NULL || text == NULL ||
      command->length >= command->capacity ||
      length > command->capacity - command->length - 1u)
    return false;
  (void)wmemcpy(command->data + command->length, text, length);
  command->length += length;
  command->data[command->length] = L'\0';
  return true;
}

/* Quote one argv element using the CommandLineToArgvW-compatible rule. */
static bool windows_command_append_quoted(windows_command *command,
                                          const wchar_t *argument) {
  if (command == NULL || argument == NULL ||
      !windows_command_append(command, L"\"", 1u))
    return false;
  size_t slashes = 0u;
  for (size_t index = 0u; argument[index] != L'\0'; index += 1u) {
    const wchar_t character = argument[index];
    if (character == L'\\') {
      slashes += 1u;
      continue;
    }
    if (character == L'\"') {
      for (size_t slash = 0u; slash < slashes * 2u + 1u; slash += 1u)
        if (!windows_command_append(command, L"\\", 1u)) return false;
      if (!windows_command_append(command, L"\"", 1u)) return false;
    } else {
      for (size_t slash = 0u; slash < slashes; slash += 1u)
        if (!windows_command_append(command, L"\\", 1u)) return false;
      if (!windows_command_append(command, &character, 1u)) return false;
    }
    slashes = 0u;
  }
  for (size_t slash = 0u; slash < slashes * 2u; slash += 1u)
    if (!windows_command_append(command, L"\\", 1u)) return false;
  return windows_command_append(command, L"\"", 1u);
}

static bool windows_command_begin(windows_command *command,
                                  const wchar_t *application) {
  if (command == NULL || application == NULL) return false;
  command->length = 0u;
  command->data[0] = L'\0';
  return windows_command_append_quoted(command, application);
}

static bool windows_command_append_utf8(windows_command *command,
                                        const char *argument) {
  wchar_t converted[W_SEED_WINDOWS_ARGUMENT_CAPACITY];
  return windows_utf8_to_wide(argument, converted, sizeof(converted) /
                                                     sizeof(converted[0])) &&
         windows_command_append_quoted(command, converted);
}

static bool windows_duplicate_standard(DWORD standard, HANDLE *duplicate) {
  if (duplicate == NULL) return false;
  *duplicate = NULL;
  const HANDLE source = GetStdHandle(standard);
  if (source == NULL || source == INVALID_HANDLE_VALUE) return false;
  return DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(),
                         duplicate, 0u, TRUE, DUPLICATE_SAME_ACCESS) != 0;
}

static int windows_launch_command(const wchar_t *lpApplicationName,
                                  wchar_t *command_line, bool silent) {
  if (lpApplicationName == NULL || command_line == NULL) return 3;
  STARTUPINFOW startup = {0};
  PROCESS_INFORMATION process = {0};
  HANDLE input = NULL;
  HANDLE output = NULL;
  HANDLE error = NULL;
  SECURITY_ATTRIBUTES security = {0};
  security.nLength = (DWORD)sizeof(security);
  security.bInheritHandle = TRUE;
  startup.cb = (DWORD)sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  bool handles_ready = false;
  if (silent) {
    input = CreateFileW(L"NUL", GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    output = CreateFileW(L"NUL", GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (input == INVALID_HANDLE_VALUE) input = NULL;
    if (output == INVALID_HANDLE_VALUE) output = NULL;
    error = output;
    handles_ready = input != NULL && output != NULL;
  } else {
    handles_ready = windows_duplicate_standard(STD_INPUT_HANDLE, &input) &&
                    windows_duplicate_standard(STD_OUTPUT_HANDLE, &output) &&
                    windows_duplicate_standard(STD_ERROR_HANDLE, &error);
  }
  if (!handles_ready) goto cleanup;
  startup.hStdInput = input;
  startup.hStdOutput = output;
  startup.hStdError = error;
  if (!CreateProcessW(lpApplicationName, command_line, NULL, NULL, TRUE,
                       CREATE_NO_WINDOW, NULL, NULL, &startup, &process))
    goto cleanup;
  (void)CloseHandle(process.hThread);
  process.hThread = NULL;
  if (WaitForSingleObject(process.hProcess, INFINITE) != WAIT_OBJECT_0)
    goto cleanup;
  DWORD exit_status = 0u;
  if (!GetExitCodeProcess(process.hProcess, &exit_status)) goto cleanup;
  (void)CloseHandle(process.hProcess);
  process.hProcess = NULL;
  const int result = silent ? (exit_status == 0u ? 0 : 2)
                           : (exit_status <= 255u ? (int)exit_status : 255);
  if (error != NULL && error != output) (void)CloseHandle(error);
  if (output != NULL) (void)CloseHandle(output);
  if (input != NULL) (void)CloseHandle(input);
  return result;

cleanup:
  if (process.hThread != NULL) (void)CloseHandle(process.hThread);
  if (process.hProcess != NULL) (void)CloseHandle(process.hProcess);
  if (error != NULL && error != output) (void)CloseHandle(error);
  if (output != NULL) (void)CloseHandle(output);
  if (input != NULL) (void)CloseHandle(input);
  return 3;
}

static int windows_run_tool(const wchar_t *application,
                            const wchar_t *const arguments[],
                            size_t argument_count) {
  wchar_t command_line[W_SEED_WINDOWS_COMMAND_CAPACITY];
  windows_command command = {command_line, sizeof(command_line) /
                                               sizeof(command_line[0]), 0u};
  if (!windows_command_begin(&command, application)) return 3;
  for (size_t index = 0u; index < argument_count; index += 1u)
    if (arguments == NULL ||
        !windows_command_append(&command, L" ", 1u) ||
        !windows_command_append_quoted(&command, arguments[index]))
      return 3;
  return windows_launch_command(application, command_line, true);
}

static bool windows_path_join(wchar_t *buffer, size_t capacity,
                              const wchar_t *directory, const wchar_t *name) {
  if (buffer == NULL || capacity < 2u || directory == NULL || name == NULL)
    return false;
  const size_t directory_length = wcslen(directory);
  const size_t name_length = wcslen(name);
  const bool separator = directory_length != 0u &&
                         directory[directory_length - 1u] != L'\\' &&
                         directory[directory_length - 1u] != L'/';
  if (directory_length > capacity - 1u) return false;
  size_t available = capacity - directory_length - 1u;
  if (separator) {
    if (available == 0u) return false;
    available -= 1u;
  }
  if (name_length > available) return false;
  size_t offset = 0u;
  (void)wmemcpy(buffer + offset, directory, directory_length);
  offset += directory_length;
  if (separator) buffer[offset++] = L'\\';
  (void)wmemcpy(buffer + offset, name, name_length);
  offset += name_length;
  buffer[offset] = L'\0';
  return true;
}

static bool windows_random_name(wchar_t *buffer, size_t capacity) {
  static const wchar_t hex[] = L"0123456789abcdef";
  uint8_t bytes[W_SEED_WINDOWS_RANDOM_BYTES];
  if (buffer == NULL || capacity < 2u ||
      BCryptGenRandom(NULL, bytes, (ULONG)sizeof(bytes),
                      BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
    return false;
  if (capacity < sizeof(bytes) * 2u + 1u) return false;
  buffer[0] = L'\0';
  for (size_t index = 0u; index < sizeof(bytes); index += 1u) {
    buffer[index * 2u] = hex[bytes[index] >> 4u];
    buffer[index * 2u + 1u] = hex[bytes[index] & 0x0fu];
  }
  buffer[sizeof(bytes) * 2u] = L'\0';
  return true;
}

static bool windows_create_temp_directory(
    wchar_t *directory, size_t capacity) {
  if (directory == NULL || capacity < 64u) return false;
  wchar_t base[W_SEED_WINDOWS_PATH_CAPACITY];
  const DWORD base_length = GetTempPathW((DWORD)(sizeof(base) /
                                                 sizeof(base[0])), base);
  if (base_length == 0u || base_length >= sizeof(base) / sizeof(base[0]))
    return false;
  wchar_t random_name[W_SEED_WINDOWS_RANDOM_BYTES * 2u + 1u];
  wchar_t candidate[W_SEED_WINDOWS_PATH_CAPACITY];
  for (size_t attempt = 0u; attempt < 32u; attempt += 1u) {
    if (!windows_random_name(random_name, sizeof(random_name) /
                                           sizeof(random_name[0])) ||
        !windows_path_join(candidate, sizeof(candidate) / sizeof(candidate[0]),
                           base, L"w-run-temp") ||
        wcslen(candidate) + 1u + wcslen(random_name) + 1u >
            sizeof(candidate) / sizeof(candidate[0]))
      return false;
    const size_t prefix_length = wcslen(candidate);
    candidate[prefix_length] = L'-';
    (void)wmemcpy(candidate + prefix_length + 1u, random_name,
                  wcslen(random_name) + 1u);
    if (CreateDirectoryW(candidate, NULL) != 0) {
      if (wcslen(candidate) + 1u > capacity) {
        (void)RemoveDirectoryW(candidate);
        return false;
      }
      (void)wmemcpy(directory, candidate, wcslen(candidate) + 1u);
      return true;
    }
    if (GetLastError() != ERROR_ALREADY_EXISTS) return false;
  }
  return false;
}

static bool windows_write_new_file(const wchar_t *path, const uint8_t *bytes,
                                   size_t length) {
  if (path == NULL || (bytes == NULL && length != 0u)) return false;
  HANDLE file = CreateFileW(path, GENERIC_WRITE, 0u, NULL, CREATE_NEW,
                            FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_HIDDEN,
                            NULL);
  if (file == INVALID_HANDLE_VALUE) return false;
  bool success = true;
  size_t offset = 0u;
  while (offset < length) {
    const size_t remaining = length - offset;
    const DWORD request = remaining > (size_t)MAXDWORD
                              ? MAXDWORD
                              : (DWORD)remaining;
    DWORD written = 0u;
    if (!WriteFile(file, bytes + offset, request, &written, NULL) ||
        written == 0u) {
      success = false;
      break;
    }
    offset += (size_t)written;
  }
  if (success && !FlushFileBuffers(file)) success = false;
  if (!CloseHandle(file)) success = false;
  if (!success) (void)DeleteFileW(path);
  return success;
}

static bool windows_remove_file(const wchar_t *path) {
  if (path == NULL || path[0] == L'\0') return true;
  if (DeleteFileW(path) != 0) return true;
  const DWORD error = GetLastError();
  return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

static bool windows_cleanup_directory(const wchar_t *directory,
                                      const wchar_t *input_path,
                                      const wchar_t *verified_path,
                                      const wchar_t *ll_path,
                                      const wchar_t *object_path,
                                      const wchar_t *program_path) {
  bool clean = windows_remove_file(input_path);
  clean = windows_remove_file(verified_path) && clean;
  clean = windows_remove_file(ll_path) && clean;
  clean = windows_remove_file(object_path) && clean;
  clean = windows_remove_file(program_path) && clean;
  if (directory != NULL && directory[0] != L'\0' &&
      !RemoveDirectoryW(directory) && GetLastError() != ERROR_PATH_NOT_FOUND &&
      GetLastError() != ERROR_FILE_NOT_FOUND)
    clean = false;
  return clean;
}

static int windows_native_status_exit(w_seed_native0_status status) {
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

static int windows_run_program(const wchar_t *application,
                               const w_seed_run_request *request) {
  if (application == NULL || request == NULL ||
      request->argument_count > W_SEED_RUN_MAX_ARGUMENTS ||
      (request->argument_count != 0u && request->arguments == NULL))
    return 2;
  wchar_t command_line[W_SEED_WINDOWS_COMMAND_CAPACITY];
  windows_command command = {command_line, sizeof(command_line) /
                                               sizeof(command_line[0]), 0u};
  if (!windows_command_begin(&command, application)) return 3;
  for (size_t index = 0u; index < request->argument_count; index += 1u)
    if (!windows_command_append(&command, L" ", 1u) ||
        !windows_command_append_utf8(&command, request->arguments[index]))
      return 2;
  return windows_launch_command(application, command_line, false);
}

int w_seed_run_compile(const w_seed_run_compile_request *request) {
  if (request == NULL || request->source_path == NULL ||
      request->target == NULL || request->directory == NULL ||
      request->artifact_path == NULL || request->directory[0] == '\0' ||
      request->artifact_path[0] == '\0' ||
      strcmp(request->target, W_SEED_NATIVE_TARGET_WINDOWS) != 0 ||
      (request->profile != W_SEED_RUN_COMPILE_PROFILE_DEV &&
       request->profile != W_SEED_RUN_COMPILE_PROFILE_RELEASE))
    return 2;
  const size_t path_length = strlen(request->source_path);
  if (path_length == 0u || path_length > W_SEED_NATIVE0_MAX_PATH_BYTES)
    return 2;
  w_seed_frontend_text source_id;
  if (!run_logical_source_id(request->source_path, path_length, &source_id))
    return 2;

  wchar_t directory[W_SEED_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t artifact_path[W_SEED_WINDOWS_PATH_CAPACITY] = {0};
  if (!windows_utf8_to_wide(request->directory, directory,
                            sizeof(directory) / sizeof(directory[0])) ||
      !windows_utf8_to_wide(request->artifact_path, artifact_path,
                            sizeof(artifact_path) / sizeof(artifact_path[0])))
    return 2;

  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  const w_seed_native0_input native_input = {
      .path = request->source_path,
      .path_length = path_length,
      .logical_source_id = source_id,
      .target =
          (w_seed_mlir0_target){W_SEED_MLIR0_TARGET_X86_64_PC_WINDOWS_MSVC}};
  const w_seed_native0_output native_output = {artifact, sizeof(artifact)};
  w_seed_native0_result native_result;
  const int source_status = windows_native_status_exit(w_seed_native0_run(
      &native_input, &native_storage, &native_output, &native_result));

  wchar_t input_path[W_SEED_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t verified_path[W_SEED_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t ll_path[W_SEED_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t object_path[W_SEED_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t mlir_opt[W_SEED_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t mlir_translate[W_SEED_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t llc[W_SEED_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t lld_link[W_SEED_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t kernel32[W_SEED_WINDOWS_PATH_CAPACITY] = {0};
  int exit_code = source_status;
  if (source_status != 0) {
    if (!windows_cleanup_directory(directory, NULL, NULL, NULL, NULL,
                                   artifact_path))
      return 3;
    return source_status;
  }
  if (!windows_utf8_to_wide(W_SEED_WINDOWS_MLIR_OPT_PATH, mlir_opt,
                            sizeof(mlir_opt) / sizeof(mlir_opt[0])) ||
      !windows_utf8_to_wide(W_SEED_WINDOWS_MLIR_TRANSLATE_PATH, mlir_translate,
                            sizeof(mlir_translate) / sizeof(mlir_translate[0])) ||
      !windows_utf8_to_wide(W_SEED_WINDOWS_LLC_PATH, llc,
                            sizeof(llc) / sizeof(llc[0])) ||
      !windows_utf8_to_wide(W_SEED_WINDOWS_LLD_LINK_PATH, lld_link,
                            sizeof(lld_link) / sizeof(lld_link[0])) ||
      !windows_utf8_to_wide(W_SEED_WINDOWS_KERNEL32_LIB_PATH, kernel32,
                            sizeof(kernel32) / sizeof(kernel32[0])) ||
      !windows_path_join(input_path, sizeof(input_path) / sizeof(input_path[0]),
                         directory, L"input.mlir") ||
      !windows_path_join(verified_path,
                         sizeof(verified_path) / sizeof(verified_path[0]),
                         directory, L"verified.mlir") ||
      !windows_path_join(ll_path, sizeof(ll_path) / sizeof(ll_path[0]),
                         directory, L"output.ll") ||
      !windows_path_join(object_path,
                         sizeof(object_path) / sizeof(object_path[0]), directory,
                         L"output.obj") ||
      !windows_write_new_file(input_path, artifact,
                              native_result.mlir.written.mlir_bytes))
    goto cleanup;

  {
    const wchar_t *arguments[8] = {
        input_path, L"-o", verified_path, L"--convert-scf-to-cf",
        L"--convert-cf-to-llvm", L"--verify-each", NULL, NULL};
    size_t argument_count = 6u;
    if (request->profile == W_SEED_RUN_COMPILE_PROFILE_RELEASE) {
      arguments[argument_count++] = L"--canonicalize";
      arguments[argument_count++] = L"--cse";
    }
    exit_code = windows_run_tool(mlir_opt, arguments,
                                 argument_count);
  }
  if (exit_code != 0) goto cleanup;
  {
    const wchar_t *arguments[] = {L"--mlir-to-llvmir", verified_path, L"-o",
                                  ll_path};
    exit_code = windows_run_tool(
        mlir_translate, arguments, sizeof(arguments) / sizeof(arguments[0]));
  }
  if (exit_code != 0) goto cleanup;
  {
    const wchar_t *arguments[6] = {L"-filetype=obj",
                                   L"-mtriple=x86_64-pc-windows-msvc",
                                   ll_path, L"-o", object_path, NULL};
    size_t argument_count = 5u;
    if (request->profile == W_SEED_RUN_COMPILE_PROFILE_RELEASE)
      arguments[argument_count++] = L"-O3";
    exit_code = windows_run_tool(llc, arguments,
                                 argument_count);
  }
  if (exit_code != 0) goto cleanup;
  {
    wchar_t out_argument[W_SEED_WINDOWS_PATH_CAPACITY + 6u];
    const wchar_t *link_arguments[10];
    size_t link_argument_count = 7u;
    if (swprintf(out_argument, sizeof(out_argument) / sizeof(out_argument[0]),
                 L"/out:%ls", artifact_path) < 0)
      goto cleanup;
    link_arguments[0] = L"/entry:mainCRTStartup";
    link_arguments[1] = L"/subsystem:console";
    link_arguments[2] = L"/nodefaultlib";
    link_arguments[3] = L"/machine:x64";
    link_arguments[4] = out_argument;
    link_arguments[5] = object_path;
    link_arguments[6] = kernel32;
    if (request->profile == W_SEED_RUN_COMPILE_PROFILE_RELEASE) {
      link_arguments[link_argument_count++] = L"/opt:ref";
      link_arguments[link_argument_count++] = L"/opt:icf";
      link_arguments[link_argument_count++] = L"/incremental:no";
    }
    exit_code = windows_run_tool(
        lld_link, link_arguments, link_argument_count);
  }
  if (exit_code != 0) goto cleanup;
  if (!windows_remove_file(input_path) ||
      !windows_remove_file(verified_path) || !windows_remove_file(ll_path) ||
      !windows_remove_file(object_path)) {
    exit_code = 3;
    goto cleanup;
  }
  return 0;

cleanup:
  if (!windows_cleanup_directory(directory, input_path, verified_path, ll_path,
                                 object_path, artifact_path))
    return 3;
  return exit_code;
}

bool w_seed_run_cleanup_compiled(const char *directory,
                                 const char *artifact_path) {
  wchar_t wide_directory[W_SEED_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t wide_artifact[W_SEED_WINDOWS_PATH_CAPACITY] = {0};
  if (!windows_utf8_to_wide(directory, wide_directory,
                            sizeof(wide_directory) / sizeof(wide_directory[0])) ||
      !windows_utf8_to_wide(artifact_path, wide_artifact,
                            sizeof(wide_artifact) / sizeof(wide_artifact[0])))
    return false;
  return windows_cleanup_directory(wide_directory, NULL, NULL, NULL, NULL,
                                   wide_artifact);
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
  if (!run_logical_source_id(request->path, path_length, &source_id)) return 2;

  wchar_t wide_directory[W_SEED_WINDOWS_PATH_CAPACITY] = {0};
  char directory[W_SEED_WINDOWS_PATH_CAPACITY] = {0};
  char program_path[W_SEED_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t wide_program_path[W_SEED_WINDOWS_PATH_CAPACITY] = {0};
  if (!windows_create_temp_directory(
          wide_directory, sizeof(wide_directory) / sizeof(wide_directory[0])) ||
      !windows_wide_to_utf8(wide_directory, directory, sizeof(directory)) ||
      !windows_path_join(wide_program_path,
                         sizeof(wide_program_path) / sizeof(wide_program_path[0]),
                         wide_directory, L"program.exe") ||
      !windows_wide_to_utf8(wide_program_path, program_path,
                            sizeof(program_path))) {
    if (wide_directory[0] != L'\0') (void)RemoveDirectoryW(wide_directory);
    return 3;
  }
  int exit_code = 3;
  {
    const w_seed_run_compile_request compile_request = {
        .source_path = request->path,
        .target = W_SEED_NATIVE_TARGET_WINDOWS,
        .directory = directory,
        .artifact_path = program_path,
        .profile = W_SEED_RUN_COMPILE_PROFILE_DEV};
    exit_code = w_seed_run_compile(&compile_request);
  }
  if (exit_code == 0) exit_code = windows_run_program(wide_program_path, request);
  if (!w_seed_run_cleanup_compiled(directory, program_path)) return 3;
  return exit_code;
}

#else /* W_SEED_WINDOWS_NATIVE_RUN_ENABLED */

int w_seed_run_compile(const w_seed_run_compile_request *request) {
  (void)request;
  return 2;
}

bool w_seed_run_cleanup_compiled(const char *directory,
                                 const char *artifact_path) {
  (void)directory;
  (void)artifact_path;
  return true;
}

int w_seed_run_execute(const w_seed_run_request *request) {
  (void)request;
  return 2;
}

#endif

#elif defined(__linux__) /* W_SEED_LINUX_NATIVE_RUN_ENABLED */

int w_seed_run_compile(const w_seed_run_compile_request *request) {
  (void)request;
  return 2;
}

bool w_seed_run_cleanup_compiled(const char *directory,
                                 const char *artifact_path) {
  (void)directory;
  (void)artifact_path;
  return true;
}

int w_seed_run_execute(const w_seed_run_request *request) {
  (void)request;
  return 2;
}

#else /* unsupported host */

int w_seed_run_compile(const w_seed_run_compile_request *request) {
  (void)request;
  return 2;
}

bool w_seed_run_cleanup_compiled(const char *directory,
                                 const char *artifact_path) {
  (void)directory;
  (void)artifact_path;
  return true;
}

int w_seed_run_execute(const w_seed_run_request *request) {
  (void)request;
  return 2;
}

#endif
