#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0602
#endif
#if defined(__linux__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#endif

#include "w_seed_owner_guard_linux.h"
#include "w_seed_owner_guard_windows.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "owner adapter test failed: %s (%s:%d)\n",       \
                    #condition, __FILE__, __LINE__);                           \
      return false;                                                            \
    }                                                                          \
  } while (0)

typedef struct {
  w_seed_owner_guard_observation staged[W_SEED_OWNER_GUARD_MAX_LEVELS];
  w_seed_owner_guard_observation revalidation[W_SEED_OWNER_GUARD_MAX_LEVELS];
  w_seed_owner_guard_candidate_ref
      candidates[W_SEED_OWNER_GUARD_MAX_LEVELS];
  w_seed_owner_guard_input input;
  w_seed_owner_guard guard;
  w_seed_owner_guard_result result;
} guard_case;

static void guard_case_init(guard_case *test_case,
                            w_seed_owner_guard_backend backend,
                            size_t backend_context_size, const uint8_t *path,
                            size_t path_length, size_t max_levels) {
  (void)memset(test_case, 0, sizeof(*test_case));
  test_case->input.source_path = (w_seed_byte_view){path, path_length};
  test_case->input.max_levels = max_levels;
  test_case->input.storage = (w_seed_owner_guard_storage){
      test_case->staged,
      W_SEED_OWNER_GUARD_MAX_LEVELS,
      test_case->revalidation,
      W_SEED_OWNER_GUARD_MAX_LEVELS,
      test_case->candidates,
      W_SEED_OWNER_GUARD_MAX_LEVELS,
  };
  test_case->input.backend = backend;
  test_case->input.backend_context_size = backend_context_size;
}

#if defined(_WIN32)

#include <fileapi.h>
#include <processthreadsapi.h>
#include <windows.h>

enum { WIDE_CAPACITY = 1024u };

typedef struct {
  WCHAR root[WIDE_CAPACITY];
  WCHAR a[WIDE_CAPACITY];
  WCHAR b[WIDE_CAPACITY];
  WCHAR moved[WIDE_CAPACITY];
  WCHAR source[WIDE_CAPACITY];
  WCHAR nested_marker[WIDE_CAPACITY];
  WCHAR root_marker[WIDE_CAPACITY];
  WCHAR absent_marker[WIDE_CAPACITY];
  WCHAR replacement[WIDE_CAPACITY];
  WCHAR junction[WIDE_CAPACITY];
  HANDLE base;
} windows_environment;

static size_t wide_length(const WCHAR *text) {
  size_t length = 0u;
  while (text != NULL && text[length] != (WCHAR)0) length += 1u;
  return length;
}

static bool wide_copy(WCHAR *destination, size_t capacity,
                      const WCHAR *source) {
  const size_t length = wide_length(source);
  if (destination == NULL || source == NULL || length >= capacity) return false;
  (void)memcpy(destination, source, (length + 1u) * sizeof(WCHAR));
  return true;
}

static bool wide_append(WCHAR *destination, size_t capacity,
                        const WCHAR *source) {
  const size_t left = wide_length(destination);
  const size_t right = wide_length(source);
  if (destination == NULL || source == NULL || left >= capacity ||
      right >= capacity - left)
    return false;
  (void)memcpy(destination + left, source, (right + 1u) * sizeof(WCHAR));
  return true;
}

static bool wide_join(const WCHAR *directory, const WCHAR *leaf,
                      WCHAR *destination) {
  return wide_copy(destination, WIDE_CAPACITY, directory) &&
         wide_append(destination, WIDE_CAPACITY, L"\\") &&
         wide_append(destination, WIDE_CAPACITY, leaf);
}

static bool write_file(const WCHAR *path, const char *bytes) {
  HANDLE file = CreateFileW(path, GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE |
                                FILE_SHARE_DELETE,
                            NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == INVALID_HANDLE_VALUE) return false;
  const size_t length = strlen(bytes);
  DWORD written = 0u;
  const BOOL write_ok =
      WriteFile(file, bytes, (DWORD)length, &written, NULL);
  const BOOL close_ok = CloseHandle(file);
  return write_ok != FALSE && close_ok != FALSE &&
         written == (DWORD)length;
}

static bool remove_file_if_present(const WCHAR *path) {
  if (DeleteFileW(path) != FALSE) return true;
  const DWORD error = GetLastError();
  return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

static bool remove_directory_if_present(const WCHAR *path) {
  if (RemoveDirectoryW(path) != FALSE) return true;
  const DWORD error = GetLastError();
  return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

static bool create_junction(const WCHAR *link_path, const WCHAR *target_path) {
  WCHAR shell[WIDE_CAPACITY];
  const DWORD shell_length =
      GetEnvironmentVariableW(L"ComSpec", shell, (DWORD)WIDE_CAPACITY);
  if (shell_length == 0u || shell_length >= (DWORD)WIDE_CAPACITY) return false;
  WCHAR command[WIDE_CAPACITY * 2u];
  command[0] = (WCHAR)0;
  if (!wide_append(command, WIDE_CAPACITY * 2u, L"\"") ||
      !wide_append(command, WIDE_CAPACITY * 2u, shell) ||
      !wide_append(command, WIDE_CAPACITY * 2u,
                   L"\" /d /c mklink /J \"") ||
      !wide_append(command, WIDE_CAPACITY * 2u, link_path) ||
      !wide_append(command, WIDE_CAPACITY * 2u, L"\" \"") ||
      !wide_append(command, WIDE_CAPACITY * 2u, target_path) ||
      !wide_append(command, WIDE_CAPACITY * 2u, L"\" >nul 2>nul"))
    return false;
  STARTUPINFOW startup;
  PROCESS_INFORMATION process;
  (void)memset(&startup, 0, sizeof(startup));
  (void)memset(&process, 0, sizeof(process));
  startup.cb = (DWORD)sizeof(startup);
  if (!CreateProcessW(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL,
                      NULL, &startup, &process))
    return false;
  const DWORD waited = WaitForSingleObject(process.hProcess, INFINITE);
  DWORD exit_code = 1u;
  const BOOL read_exit = GetExitCodeProcess(process.hProcess, &exit_code);
  (void)CloseHandle(process.hThread);
  (void)CloseHandle(process.hProcess);
  const DWORD attributes = GetFileAttributesW(link_path);
  return waited == WAIT_OBJECT_0 && read_exit != FALSE && exit_code == 0u &&
         attributes != INVALID_FILE_ATTRIBUTES &&
         (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u;
}

static bool windows_environment_init(windows_environment *environment) {
  (void)memset(environment, 0, sizeof(*environment));
  environment->base = INVALID_HANDLE_VALUE;
  WCHAR temporary[WIDE_CAPACITY];
  const DWORD temporary_length =
      GetTempPathW((DWORD)WIDE_CAPACITY, temporary);
  if (temporary_length == 0u || temporary_length >= (DWORD)WIDE_CAPACITY ||
      GetTempFileNameW(temporary, L"own", 0u, environment->root) == 0u ||
      !remove_file_if_present(environment->root) ||
      !CreateDirectoryW(environment->root, NULL) ||
      !wide_join(environment->root, L"a", environment->a) ||
      !wide_join(environment->a, L"b", environment->b) ||
      !wide_join(environment->a, L"moved", environment->moved) ||
      !wide_join(environment->b, L"main.w", environment->source) ||
      !wide_join(environment->b, L"BUILD.W", environment->nested_marker) ||
      !wide_join(environment->root, L"build.w", environment->root_marker) ||
      !wide_join(environment->a, L"build.w", environment->absent_marker) ||
      !wide_join(environment->b, L"replacement.tmp",
                 environment->replacement) ||
      !wide_join(environment->a, L"junction", environment->junction) ||
      !CreateDirectoryW(environment->a, NULL) ||
      !CreateDirectoryW(environment->b, NULL) ||
      !write_file(environment->source, "source\n") ||
      !create_junction(environment->junction, environment->b))
    return false;
  environment->base = CreateFileW(
      environment->root, FILE_READ_ATTRIBUTES | SYNCHRONIZE,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
      OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
  return environment->base != INVALID_HANDLE_VALUE;
}

static void windows_environment_destroy(windows_environment *environment) {
  if (environment->base != INVALID_HANDLE_VALUE)
    (void)CloseHandle(environment->base);
  (void)remove_file_if_present(environment->nested_marker);
  (void)remove_file_if_present(environment->root_marker);
  (void)remove_file_if_present(environment->absent_marker);
  (void)remove_file_if_present(environment->replacement);
  (void)remove_file_if_present(environment->source);
  (void)remove_directory_if_present(environment->nested_marker);
  (void)remove_directory_if_present(environment->junction);
  (void)remove_directory_if_present(environment->moved);
  (void)remove_directory_if_present(environment->b);
  (void)remove_directory_if_present(environment->a);
  (void)remove_directory_if_present(environment->root);
}

static bool borrowed_windows_handle_live(HANDLE handle) {
  FILE_ID_INFO info;
  return GetFileInformationByHandleEx(handle, FileIdInfo, &info,
                                      (DWORD)sizeof(info)) != FALSE;
}

static bool test_linux_stub_on_windows(void) {
  w_seed_owner_guard_linux_context linux_stub;
  w_seed_owner_guard_backend linux_backend;
  w_seed_owner_guard_observation sentinel = {7u, 9u, true};
  const w_seed_owner_guard_observation snapshot = sentinel;
  if (!w_seed_owner_guard_linux_init(&linux_stub, -1) ||
      !w_seed_owner_guard_linux_backend(&linux_stub, &linux_backend))
    return false;
  const w_seed_owner_guard_backend_result result = linux_backend.begin(
      linux_backend.context,
      (w_seed_byte_view){(const uint8_t *)"a.w", 3u}, &sentinel, 1u);
  return result.status == W_SEED_OWNER_GUARD_BACKEND_UNSUPPORTED &&
         memcmp(&sentinel, &snapshot, sizeof(sentinel)) == 0;
}

static bool test_windows_native(void) {
  windows_environment environment;
  CHECK(windows_environment_init(&environment));
  w_seed_owner_guard_windows_context context;
  w_seed_owner_guard_backend backend;
  guard_case test_case;

  CHECK(w_seed_owner_guard_windows_init(&context,
                                        (uintptr_t)environment.base));
  CHECK(!context.native_supported);
  {
    CHECK(context.parent_probe_status != UINT32_MAX &&
          context.locality_status != UINT32_MAX &&
          ((context.base_local &&
            context.locality_status == ERROR_SUCCESS) ||
           (!context.base_local &&
            context.locality_status != ERROR_SUCCESS)) &&
          borrowed_windows_handle_live(environment.base) &&
          context.slot_count == 0u && !context.session_live &&
          w_seed_owner_guard_windows_backend(&context, &backend));
    w_seed_owner_guard_observation sentinel = {7u, 9u, true};
    const w_seed_owner_guard_observation snapshot = sentinel;
    const w_seed_owner_guard_backend_result unsupported = backend.begin(
        backend.context,
        (w_seed_byte_view){(const uint8_t *)"a/b/main.w",
                           sizeof("a/b/main.w") - 1u},
        &sentinel, 1u);
    CHECK(unsupported.status == W_SEED_OWNER_GUARD_BACKEND_UNSUPPORTED &&
          memcmp(&sentinel, &snapshot, sizeof(sentinel)) == 0 &&
          context.slot_count == 0u && !context.session_live &&
          borrowed_windows_handle_live(environment.base) &&
          test_linux_stub_on_windows());
    guard_case_init(&test_case, backend, sizeof(context),
                    (const uint8_t *)"a/b/main.w",
                    sizeof("a/b/main.w") - 1u,
                    W_SEED_OWNER_GUARD_MAX_LEVELS);
    CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                   &test_case.result) ==
              W_SEED_OWNER_GUARD_UNSUPPORTED &&
          context.slot_count == 0u && !context.session_live &&
          borrowed_windows_handle_live(environment.base));
    windows_environment_destroy(&environment);
    return true;
  }

#if 0 /* Windows native traversal is deliberately not promoted in OWN0. */
  windows_parent_supported = true;

  CHECK(write_file(environment.nested_marker, "nested\n"));
  CHECK(write_file(environment.root_marker, "root\n"));
  CHECK(windows_begin(&environment, &context, &backend, &test_case,
                      "a/b/main.w", W_SEED_OWNER_GUARD_MAX_LEVELS));
  CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                 &test_case.result) == W_SEED_OWNER_GUARD_OK);
  CHECK(test_case.guard.candidate_count == 2u &&
        test_case.candidates[0].directory_ordinal == 0u &&
        test_case.candidates[1].directory_ordinal == 2u);
  CHECK(w_seed_owner_guard_revalidate(&test_case.guard, &test_case.result) ==
        W_SEED_OWNER_GUARD_OK);
  CHECK(test_case.guard.disposition ==
        W_SEED_OWNER_GUARD_CANDIDATES_RECONFIRMED);
  w_seed_owner_guard_destroy(&test_case.guard);
  CHECK(context.slot_count == 0u && !context.session_live &&
        borrowed_windows_handle_live(environment.base));

  CHECK(remove_file_if_present(environment.nested_marker) &&
        remove_file_if_present(environment.root_marker) &&
        windows_begin(&environment, &context, &backend, &test_case,
                      "a/b/main.w", W_SEED_OWNER_GUARD_MAX_LEVELS));
  CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                 &test_case.result) == W_SEED_OWNER_GUARD_OK &&
        test_case.guard.candidate_count == 0u);
  CHECK(w_seed_owner_guard_revalidate(&test_case.guard, &test_case.result) ==
        W_SEED_OWNER_GUARD_OK);
  CHECK(test_case.guard.disposition ==
        W_SEED_OWNER_GUARD_NO_CANDIDATE_RECONFIRMED);
  w_seed_owner_guard_destroy(&test_case.guard);

  CHECK(windows_begin(&environment, &context, &backend, &test_case,
                      "a/b/main.w", W_SEED_OWNER_GUARD_MAX_LEVELS));
  CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                 &test_case.result) == W_SEED_OWNER_GUARD_OK);
  CHECK(write_file(environment.absent_marker, "new\n"));
  CHECK(w_seed_owner_guard_revalidate(&test_case.guard, &test_case.result) ==
        W_SEED_OWNER_GUARD_MUTATED);
  w_seed_owner_guard_destroy(&test_case.guard);
  CHECK(remove_file_if_present(environment.absent_marker));

  CHECK(write_file(environment.nested_marker, "old\n") &&
        windows_begin(&environment, &context, &backend, &test_case,
                      "a/b/main.w", W_SEED_OWNER_GUARD_MAX_LEVELS));
  CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                 &test_case.result) == W_SEED_OWNER_GUARD_OK);
  CHECK(remove_file_if_present(environment.nested_marker));
  CHECK(w_seed_owner_guard_revalidate(&test_case.guard, &test_case.result) ==
        W_SEED_OWNER_GUARD_MUTATED);
  w_seed_owner_guard_destroy(&test_case.guard);

  CHECK(write_file(environment.nested_marker, "old\n") &&
        windows_begin(&environment, &context, &backend, &test_case,
                      "a/b/main.w", W_SEED_OWNER_GUARD_MAX_LEVELS));
  CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                 &test_case.result) == W_SEED_OWNER_GUARD_OK);
  CHECK(write_file(environment.replacement, "replacement\n") &&
        MoveFileExW(environment.replacement, environment.nested_marker,
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) !=
            FALSE);
  CHECK(w_seed_owner_guard_revalidate(&test_case.guard, &test_case.result) ==
        W_SEED_OWNER_GUARD_MUTATED);
  w_seed_owner_guard_destroy(&test_case.guard);
  CHECK(remove_file_if_present(environment.nested_marker));

  CHECK(CreateDirectoryW(environment.nested_marker, NULL) != FALSE &&
        windows_begin(&environment, &context, &backend, &test_case,
                      "a/b/main.w", W_SEED_OWNER_GUARD_MAX_LEVELS));
  CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                 &test_case.result) ==
        W_SEED_OWNER_GUARD_UNSUPPORTED);
  CHECK(context.slot_count == 0u && !context.session_live);
  CHECK(remove_directory_if_present(environment.nested_marker));

  CHECK(windows_begin(&environment, &context, &backend, &test_case,
                      "a/junction/main.w", W_SEED_OWNER_GUARD_MAX_LEVELS));
  CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                 &test_case.result) ==
        W_SEED_OWNER_GUARD_REPARSE);
  CHECK(context.slot_count == 0u && !context.session_live);

  CHECK(windows_begin(&environment, &context, &backend, &test_case,
                      "a/b/main.w", 1u));
  CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                 &test_case.result) ==
        W_SEED_OWNER_GUARD_CAPACITY);
  CHECK(context.slot_count == 0u && !context.session_live &&
        borrowed_windows_handle_live(environment.base));

  CHECK(windows_begin(&environment, &context, &backend, &test_case,
                      "a/b/main.w", W_SEED_OWNER_GUARD_MAX_LEVELS));
  CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                 &test_case.result) == W_SEED_OWNER_GUARD_OK);
  CHECK(MoveFileExW(environment.b, environment.moved, 0u) != FALSE);
  CHECK(w_seed_owner_guard_revalidate(&test_case.guard, &test_case.result) !=
        W_SEED_OWNER_GUARD_OK);
  w_seed_owner_guard_destroy(&test_case.guard);
  CHECK(MoveFileExW(environment.moved, environment.b, 0u) != FALSE);

  static const uint8_t invalid_utf8[] = {'a', '/', 0xffu, '/', 'x'};
  static const struct {
    const uint8_t *bytes;
    size_t length;
  } invalid_paths[] = {
      {(const uint8_t *)"", 0u},
      {(const uint8_t *)"/a/b", 4u},
      {(const uint8_t *)"a\\b", 3u},
      {(const uint8_t *)"a/../b", 6u},
      {(const uint8_t *)"C:/a", 4u},
      {(const uint8_t *)"//server/a", 10u},
      {(const uint8_t *)"a:b", 3u},
      {invalid_utf8, sizeof(invalid_utf8)},
  };
  for (size_t index = 0u;
       index < sizeof(invalid_paths) / sizeof(invalid_paths[0]); index += 1u) {
    CHECK(w_seed_owner_guard_windows_init(&context,
                                          (uintptr_t)environment.base) &&
          w_seed_owner_guard_windows_backend(&context, &backend));
    guard_case_init(&test_case, backend, sizeof(context),
                    invalid_paths[index].bytes, invalid_paths[index].length,
                    W_SEED_OWNER_GUARD_MAX_LEVELS);
    CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                   &test_case.result) ==
          W_SEED_OWNER_GUARD_INVALID);
    CHECK(!context.session_live && context.slot_count == 0u);
  }

  CHECK(test_linux_stub_on_windows());

  windows_environment_destroy(&environment);
  return true;
#endif
}

#elif defined(__linux__)

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

enum { PATH_CAPACITY = 1024u };

typedef struct {
  char root[PATH_CAPACITY];
  char a[PATH_CAPACITY];
  char b[PATH_CAPACITY];
  char moved[PATH_CAPACITY];
  char source[PATH_CAPACITY];
  char nested_marker[PATH_CAPACITY];
  char root_marker[PATH_CAPACITY];
  char absent_marker[PATH_CAPACITY];
  char replacement[PATH_CAPACITY];
  char link[PATH_CAPACITY];
  int base_fd;
} linux_environment;

static bool path_join(const char *directory, const char *leaf,
                      char destination[PATH_CAPACITY]) {
  if (directory == NULL || leaf == NULL || destination == NULL) return false;
  const int count = snprintf(destination, PATH_CAPACITY, "%s/%s", directory,
                             leaf);
  return count > 0 && (size_t)count < PATH_CAPACITY;
}

static bool write_file(const char *path, const char *bytes) {
  const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
  if (fd < 0) return false;
  const size_t length = strlen(bytes);
  const ssize_t written = write(fd, bytes, length);
  const int close_status = close(fd);
  return written >= 0 && (size_t)written == length && close_status == 0;
}

static bool unlink_if_present(const char *path) {
  return unlink(path) == 0 || errno == ENOENT;
}

static bool rmdir_if_present(const char *path) {
  return rmdir(path) == 0 || errno == ENOENT;
}

static bool linux_environment_init(linux_environment *environment) {
  (void)memset(environment, 0, sizeof(*environment));
  environment->base_fd = -1;
  (void)memcpy(environment->root, "/tmp/w-own0-XXXXXX",
               sizeof("/tmp/w-own0-XXXXXX"));
  if (mkdtemp(environment->root) == NULL ||
      !path_join(environment->root, "a", environment->a) ||
      !path_join(environment->a, "b", environment->b) ||
      !path_join(environment->a, "moved", environment->moved) ||
      !path_join(environment->b, "main.w", environment->source) ||
      !path_join(environment->b, "build.w", environment->nested_marker) ||
      !path_join(environment->root, "build.w", environment->root_marker) ||
      !path_join(environment->a, "build.w", environment->absent_marker) ||
      !path_join(environment->b, "replacement.tmp",
                 environment->replacement) ||
      !path_join(environment->a, "link", environment->link) ||
      mkdir(environment->a, 0700) != 0 || mkdir(environment->b, 0700) != 0 ||
      !write_file(environment->source, "source\n") ||
      symlink(environment->b, environment->link) != 0)
    return false;
  environment->base_fd = open(environment->root, O_PATH | O_DIRECTORY | O_CLOEXEC);
  return environment->base_fd >= 0;
}

static void linux_environment_destroy(linux_environment *environment) {
  if (environment->base_fd >= 0) (void)close(environment->base_fd);
  (void)unlink_if_present(environment->nested_marker);
  (void)unlink_if_present(environment->root_marker);
  (void)unlink_if_present(environment->absent_marker);
  (void)unlink_if_present(environment->replacement);
  (void)unlink_if_present(environment->link);
  (void)unlink_if_present(environment->source);
  (void)rmdir_if_present(environment->nested_marker);
  (void)rmdir_if_present(environment->moved);
  (void)rmdir_if_present(environment->b);
  (void)rmdir_if_present(environment->a);
  (void)rmdir_if_present(environment->root);
}

static bool linux_begin(
    linux_environment *environment, w_seed_owner_guard_linux_context *context,
    w_seed_owner_guard_backend *backend, guard_case *test_case,
    const char *source_path, size_t max_levels) {
  if (!w_seed_owner_guard_linux_init(context, environment->base_fd) ||
      !context->native_supported ||
      !w_seed_owner_guard_linux_backend(context, backend))
    return false;
  guard_case_init(test_case, *backend, sizeof(*context),
                  (const uint8_t *)source_path, strlen(source_path),
                  max_levels);
  return true;
}

static bool test_linux_native(void) {
  linux_environment environment;
  CHECK(linux_environment_init(&environment));
  w_seed_owner_guard_linux_context context;
  w_seed_owner_guard_backend backend;
  guard_case test_case;

  CHECK(write_file(environment.nested_marker, "nested\n") &&
        write_file(environment.root_marker, "root\n") &&
        linux_begin(&environment, &context, &backend, &test_case,
                    "a/b/main.w", W_SEED_OWNER_GUARD_MAX_LEVELS));
  CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                 &test_case.result) == W_SEED_OWNER_GUARD_OK);
  CHECK(test_case.guard.candidate_count == 2u &&
        test_case.candidates[0].directory_ordinal == 0u &&
        test_case.candidates[1].directory_ordinal == 2u);
  CHECK(w_seed_owner_guard_revalidate(&test_case.guard, &test_case.result) ==
        W_SEED_OWNER_GUARD_OK);
  w_seed_owner_guard_destroy(&test_case.guard);
  CHECK(context.slot_count == 0u && !context.session_live &&
        fcntl(environment.base_fd, F_GETFD) >= 0);

  CHECK(unlink_if_present(environment.nested_marker) &&
        unlink_if_present(environment.root_marker) &&
        linux_begin(&environment, &context, &backend, &test_case,
                    "a/b/main.w", W_SEED_OWNER_GUARD_MAX_LEVELS));
  CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                 &test_case.result) == W_SEED_OWNER_GUARD_OK &&
        test_case.guard.candidate_count == 0u);
  CHECK(w_seed_owner_guard_revalidate(&test_case.guard, &test_case.result) ==
        W_SEED_OWNER_GUARD_OK &&
        test_case.guard.disposition ==
            W_SEED_OWNER_GUARD_NO_CANDIDATE_RECONFIRMED);
  w_seed_owner_guard_destroy(&test_case.guard);

  CHECK(linux_begin(&environment, &context, &backend, &test_case,
                    "a/b/main.w", W_SEED_OWNER_GUARD_MAX_LEVELS));
  CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                 &test_case.result) == W_SEED_OWNER_GUARD_OK);
  CHECK(write_file(environment.absent_marker, "new\n"));
  CHECK(w_seed_owner_guard_revalidate(&test_case.guard, &test_case.result) ==
        W_SEED_OWNER_GUARD_MUTATED);
  w_seed_owner_guard_destroy(&test_case.guard);
  CHECK(unlink_if_present(environment.absent_marker));

  CHECK(write_file(environment.nested_marker, "old\n") &&
        linux_begin(&environment, &context, &backend, &test_case,
                    "a/b/main.w", W_SEED_OWNER_GUARD_MAX_LEVELS));
  CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                 &test_case.result) == W_SEED_OWNER_GUARD_OK);
  CHECK(unlink_if_present(environment.nested_marker));
  CHECK(w_seed_owner_guard_revalidate(&test_case.guard, &test_case.result) ==
        W_SEED_OWNER_GUARD_MUTATED);
  w_seed_owner_guard_destroy(&test_case.guard);

  CHECK(write_file(environment.nested_marker, "old\n") &&
        linux_begin(&environment, &context, &backend, &test_case,
                    "a/b/main.w", W_SEED_OWNER_GUARD_MAX_LEVELS));
  CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                 &test_case.result) == W_SEED_OWNER_GUARD_OK);
  CHECK(write_file(environment.replacement, "replacement\n") &&
        rename(environment.replacement, environment.nested_marker) == 0);
  CHECK(w_seed_owner_guard_revalidate(&test_case.guard, &test_case.result) ==
        W_SEED_OWNER_GUARD_MUTATED);
  w_seed_owner_guard_destroy(&test_case.guard);
  CHECK(unlink_if_present(environment.nested_marker));

  CHECK(mkdir(environment.nested_marker, 0700) == 0 &&
        linux_begin(&environment, &context, &backend, &test_case,
                    "a/b/main.w", W_SEED_OWNER_GUARD_MAX_LEVELS));
  CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                 &test_case.result) ==
        W_SEED_OWNER_GUARD_UNSUPPORTED);
  CHECK(!context.session_live && context.slot_count == 0u);
  CHECK(rmdir_if_present(environment.nested_marker));

  CHECK(mkfifo(environment.nested_marker, 0600) == 0 &&
        linux_begin(&environment, &context, &backend, &test_case,
                    "a/b/main.w", W_SEED_OWNER_GUARD_MAX_LEVELS));
  CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                 &test_case.result) ==
        W_SEED_OWNER_GUARD_UNSUPPORTED);
  CHECK(unlink_if_present(environment.nested_marker));

  CHECK(symlink("main.w", environment.nested_marker) == 0 &&
        linux_begin(&environment, &context, &backend, &test_case,
                    "a/b/main.w", W_SEED_OWNER_GUARD_MAX_LEVELS));
  CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                 &test_case.result) ==
        W_SEED_OWNER_GUARD_REPARSE);
  CHECK(unlink_if_present(environment.nested_marker));

  CHECK(rename(environment.source, environment.replacement) == 0 &&
        symlink("replacement.tmp", environment.source) == 0 &&
        linux_begin(&environment, &context, &backend, &test_case,
                    "a/b/main.w", W_SEED_OWNER_GUARD_MAX_LEVELS));
  CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                 &test_case.result) ==
        W_SEED_OWNER_GUARD_REPARSE);
  CHECK(unlink_if_present(environment.source) &&
        rename(environment.replacement, environment.source) == 0);

  CHECK(rename(environment.source, environment.replacement) == 0 &&
        mkdir(environment.source, 0700) == 0 &&
        linux_begin(&environment, &context, &backend, &test_case,
                    "a/b/main.w", W_SEED_OWNER_GUARD_MAX_LEVELS));
  CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                 &test_case.result) ==
        W_SEED_OWNER_GUARD_UNSUPPORTED);
  CHECK(rmdir_if_present(environment.source) &&
        rename(environment.replacement, environment.source) == 0);

  CHECK(linux_begin(&environment, &context, &backend, &test_case,
                    "a/link/main.w", W_SEED_OWNER_GUARD_MAX_LEVELS));
  CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                 &test_case.result) ==
        W_SEED_OWNER_GUARD_REPARSE);

  CHECK(linux_begin(&environment, &context, &backend, &test_case,
                    "a/b/main.w", 1u));
  CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                 &test_case.result) ==
        W_SEED_OWNER_GUARD_CAPACITY);
  CHECK(!context.session_live && context.slot_count == 0u &&
        fcntl(environment.base_fd, F_GETFD) >= 0);

  CHECK(linux_begin(&environment, &context, &backend, &test_case,
                    "a/b/main.w", W_SEED_OWNER_GUARD_MAX_LEVELS));
  CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                 &test_case.result) == W_SEED_OWNER_GUARD_OK);
  CHECK(rename(environment.b, environment.moved) == 0);
  CHECK(w_seed_owner_guard_revalidate(&test_case.guard, &test_case.result) !=
        W_SEED_OWNER_GUARD_OK);
  w_seed_owner_guard_destroy(&test_case.guard);
  CHECK(rename(environment.moved, environment.b) == 0);

  const int root_fd = open("/", O_PATH | O_DIRECTORY | O_CLOEXEC);
  CHECK(root_fd >= 0);
  w_seed_owner_guard_linux_context root_context;
  CHECK(w_seed_owner_guard_linux_init(&root_context, root_fd) &&
        root_context.native_supported &&
        w_seed_owner_guard_linux_backend(&root_context, &backend));
  guard_case_init(&test_case, backend, sizeof(root_context),
                  (const uint8_t *)"proc/version", sizeof("proc/version") - 1u,
                  W_SEED_OWNER_GUARD_MAX_LEVELS);
  CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                 &test_case.result) ==
        W_SEED_OWNER_GUARD_BOUNDARY);
  CHECK(close(root_fd) == 0);

  static const uint8_t invalid_utf8[] = {'a', '/', 0xffu, '/', 'x'};
  static const uint8_t embedded_nul[] = {'a', '/', 0u, '/', 'x'};
  static const struct {
    const uint8_t *bytes;
    size_t length;
  } invalid_paths[] = {
      {(const uint8_t *)"", 0u},
      {(const uint8_t *)"/a/b", 4u},
      {(const uint8_t *)"a\\b", 3u},
      {(const uint8_t *)"a/./b", 5u},
      {(const uint8_t *)"a/../b", 6u},
      {invalid_utf8, sizeof(invalid_utf8)},
      {embedded_nul, sizeof(embedded_nul)},
  };
  for (size_t index = 0u;
       index < sizeof(invalid_paths) / sizeof(invalid_paths[0]); index += 1u) {
    CHECK(w_seed_owner_guard_linux_init(&context, environment.base_fd) &&
          w_seed_owner_guard_linux_backend(&context, &backend));
    guard_case_init(&test_case, backend, sizeof(context),
                    invalid_paths[index].bytes, invalid_paths[index].length,
                    W_SEED_OWNER_GUARD_MAX_LEVELS);
    CHECK(w_seed_owner_guard_begin(&test_case.input, &test_case.guard,
                                   &test_case.result) ==
          W_SEED_OWNER_GUARD_INVALID);
    CHECK(!context.session_live && context.slot_count == 0u);
  }

  w_seed_owner_guard_windows_context windows_stub;
  w_seed_owner_guard_backend windows_backend;
  w_seed_owner_guard_observation sentinel = {7u, 9u, true};
  const w_seed_owner_guard_observation snapshot = sentinel;
  CHECK(w_seed_owner_guard_windows_init(&windows_stub, (uintptr_t)0u) &&
        w_seed_owner_guard_windows_backend(&windows_stub, &windows_backend));
  const w_seed_owner_guard_backend_result stub_result = windows_backend.begin(
      windows_backend.context,
      (w_seed_byte_view){(const uint8_t *)"a.w", 3u}, &sentinel, 1u);
  CHECK(stub_result.status == W_SEED_OWNER_GUARD_BACKEND_UNSUPPORTED &&
        memcmp(&sentinel, &snapshot, sizeof(sentinel)) == 0);

  linux_environment_destroy(&environment);
  return true;
}

#else

static bool test_stub_only(void) {
  w_seed_owner_guard_linux_context linux_context;
  w_seed_owner_guard_windows_context windows_context;
  w_seed_owner_guard_backend linux_backend;
  w_seed_owner_guard_backend windows_backend;
  return w_seed_owner_guard_linux_init(&linux_context, -1) &&
         w_seed_owner_guard_windows_init(&windows_context, (uintptr_t)0u) &&
         w_seed_owner_guard_linux_backend(&linux_context, &linux_backend) &&
         w_seed_owner_guard_windows_backend(&windows_context,
                                            &windows_backend);
}

#endif

int main(void) {
#if defined(_WIN32)
  if (!test_windows_native()) return 1;
  (void)puts("w_seed_owner_guard_adapters: windows-disabled unsupported; "
             "linux-stub ok");
#elif defined(__linux__)
  if (!test_linux_native()) return 1;
  (void)puts("w_seed_owner_guard_adapters: linux-native ok; windows-stub ok");
#else
  if (!test_stub_only()) return 1;
  (void)puts("w_seed_owner_guard_adapters: crossed stubs ok");
#endif
  return 0;
}
