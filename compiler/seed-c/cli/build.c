#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif

#include "build.h"

#include <string.h>

#include "run.h"
#include "w_seed_native0.h"

#if defined(_WIN32)
#include "w_seed_windows_config.h"
#elif defined(__linux__)
#include "w_seed_linux_config.h"
#endif

static bool build_source_path_is_valid(const char *path) {
  if (path == NULL || path[0] == '\0' || path[0] == '-') return false;
  const size_t length = strlen(path);
  return length >= 3u && path[length - 2u] == '.' && path[length - 1u] == 'w';
}

bool w_seed_build_parse(int argc, char **argv,
                        w_seed_build_request *request) {
  if (request != NULL)
    *request = (w_seed_build_request){NULL, NULL, NULL};
  if (request == NULL || argv == NULL || argc != 7 || argv[1] == NULL ||
      strcmp(argv[1], "build") != 0 ||
      !build_source_path_is_valid(argv[2]))
    return false;

  const char *target = NULL;
  const char *output = NULL;
  bool target_seen = false;
  bool output_seen = false;
  for (int index = 3; index < argc; index += 2) {
    const char *option = argv[index];
    const char *value = argv[index + 1];
    if (option == NULL || value == NULL || value[0] == '\0' ||
        value[0] == '-')
      return false;
    if (strcmp(option, "--target") == 0) {
      if (target_seen) return false;
      target_seen = true;
      target = value;
    } else if (strcmp(option, "--output") == 0) {
      if (output_seen) return false;
      output_seen = true;
      output = value;
    } else {
      return false;
    }
  }
  if (!target_seen || !output_seen) return false;
  request->path = argv[2];
  request->target = target;
  request->output = output;
  return true;
}

#if defined(__linux__)

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#if W_SEED_LINUX_NATIVE_RUN_ENABLED

static bool build_path_join(char *buffer, size_t capacity,
                            const char *directory, const char *name) {
  if (buffer == NULL || capacity == 0u || directory == NULL || name == NULL)
    return false;
  const int length = snprintf(buffer, capacity, "%s/%s", directory, name);
  return length >= 0 && (size_t)length < capacity;
}

static bool build_split_output(const char *output, char *parent,
                               size_t parent_capacity) {
  if (output == NULL || parent == NULL || parent_capacity == 0u ||
      output[0] == '\0' || output[0] == '-')
    return false;
  const size_t output_length = strlen(output);
  if (output_length >= (size_t)PATH_MAX) return false;
  const char *separator = strrchr(output, '/');
  const char *name = separator == NULL ? output : separator + 1;
  if (name[0] == '\0') return false;
  if (separator == NULL) {
    if (parent_capacity < 2u) return false;
    parent[0] = '.';
    parent[1] = '\0';
    return true;
  }
  const size_t parent_length = separator == output
                                   ? 1u
                                   : (size_t)(separator - output);
  if (parent_length + 1u > parent_capacity) return false;
  if (separator == output)
    parent[0] = '/';
  else
    (void)memcpy(parent, output, parent_length);
  parent[parent_length] = '\0';
  return true;
}

static bool build_parent_is_physical_directory(const char *parent) {
  struct stat status;
  if (parent == NULL || lstat(parent, &status) != 0 ||
      S_ISLNK(status.st_mode) || !S_ISDIR(status.st_mode))
    return false;
  return true;
}

static bool build_output_is_new(const char *output) {
  struct stat status;
  if (output == NULL) return false;
  if (lstat(output, &status) == 0) return false;
  return errno == ENOENT;
}

static bool build_create_stage(const char *parent, char *stage,
                               size_t stage_capacity) {
  if (stage == NULL || stage_capacity == 0u || parent == NULL) return false;
  char candidate[PATH_MAX] = {0};
  const int length = snprintf(candidate, sizeof(candidate),
                              "%s/.w-build-XXXXXX", parent);
  if (length < 0 || (size_t)length >= sizeof(candidate) ||
      (size_t)length + 1u > stage_capacity || mkdtemp(candidate) == NULL)
    return false;
  if (chmod(candidate, (mode_t)0700) != 0) {
    (void)rmdir(candidate);
    return false;
  }
  (void)memcpy(stage, candidate, (size_t)length + 1u);
  return true;
}

static bool build_hidden_artifact_path(const char *stage, char *hidden,
                                       size_t hidden_capacity) {
  if (stage == NULL || hidden == NULL || hidden_capacity == 0u) return false;
  const int length = snprintf(hidden, hidden_capacity, "%s.artifact", stage);
  return length >= 0 && (size_t)length < hidden_capacity;
}

static int build_rename_no_replace(const char *source, const char *target) {
#if defined(SYS_renameat2) && defined(RENAME_NOREPLACE) && defined(AT_FDCWD)
  const long result = syscall(SYS_renameat2, AT_FDCWD, source, AT_FDCWD,
                              target, (unsigned int)RENAME_NOREPLACE);
  return result == 0L ? 0 : -1;
#else
  (void)source;
  (void)target;
  errno = ENOSYS;
  return -1;
#endif
}

static bool build_remove_hidden(const char *path) {
  return path == NULL || path[0] == '\0' || unlink(path) == 0 ||
         errno == ENOENT;
}

static bool build_rename_no_replace_unavailable(int error) {
  return error == ENOSYS || error == EINVAL || error == EOPNOTSUPP ||
         error == ENOTSUP;
}

static int build_publish_linux(const char *hidden, const char *output,
                               bool *hidden_linked) {
  const int rename_result = build_rename_no_replace(hidden, output);
  if (rename_result == 0) return 0;
  const int rename_error = errno;
  if (!build_rename_no_replace_unavailable(rename_error)) {
    errno = rename_error;
    return -1;
  }
  /* Some WSL-mounted filesystems do not implement renameat2. A hard link is
   * still an atomic no-clobber publication on the same filesystem. The
   * unlink below is post-commit cleanup and is never required for success. */
  if (link(hidden, output) != 0) return -1;
  if (build_remove_hidden(hidden) && hidden_linked != NULL)
    *hidden_linked = false;
  return 0;
}

static bool build_cleanup_linux(const char *stage, const char *artifact,
                                const char *hidden, bool hidden_linked) {
  bool clean = true;
  if (hidden_linked && !build_remove_hidden(hidden)) clean = false;
  if (!w_seed_run_cleanup_compiled(stage, artifact)) clean = false;
  return clean;
}

#endif

static int build_execute_linux(const w_seed_build_request *request) {
#if W_SEED_LINUX_NATIVE_RUN_ENABLED
  if (request == NULL ||
      strcmp(request->target, W_SEED_NATIVE_TARGET_LINUX) != 0 ||
      strlen(request->path) > W_SEED_NATIVE0_MAX_PATH_BYTES)
    return 2;

  char parent[PATH_MAX] = {0};
  if (!build_split_output(request->output, parent, sizeof(parent)) ||
      !build_parent_is_physical_directory(parent) ||
      !build_output_is_new(request->output))
    return 2;

  char stage[PATH_MAX] = {0};
  char artifact[PATH_MAX] = {0};
  if (!build_create_stage(parent, stage, sizeof(stage)) ||
      !build_path_join(artifact, sizeof(artifact), stage, "program")) {
    if (stage[0] != '\0') (void)rmdir(stage);
    return 3;
  }
  const w_seed_run_compile_request compile_request = {
      .source_path = request->path,
      .target = request->target,
      .directory = stage,
      .artifact_path = artifact,
      .profile = W_SEED_RUN_COMPILE_PROFILE_RELEASE};
  int exit_code = w_seed_run_compile(&compile_request);
  char hidden[PATH_MAX] = {0};
  bool hidden_linked = false;
  if (exit_code == 0) {
    if (!build_hidden_artifact_path(stage, hidden, sizeof(hidden)) ||
        link(artifact, hidden) != 0) {
      exit_code = 3;
      goto cleanup;
    }
    hidden_linked = true;
    if (unlink(artifact) != 0 ||
        !w_seed_run_cleanup_compiled(stage, artifact)) {
      exit_code = 3;
      goto cleanup;
    }
    if (build_publish_linux(hidden, request->output, &hidden_linked) == 0)
      return 0;
    const int publish_error = errno;
    exit_code = publish_error == EEXIST || publish_error == EISDIR ? 2 : 3;
  }

cleanup:
  if (!build_cleanup_linux(stage, artifact, hidden, hidden_linked)) return 3;
  return exit_code;
#else
  (void)request;
  return 2;
#endif
}

#elif defined(_WIN32)

#include <limits.h>
#include <stdio.h>
#include <wchar.h>
#include <windows.h>
#include <bcrypt.h>

#if W_SEED_WINDOWS_NATIVE_RUN_ENABLED

enum {
  W_SEED_BUILD_WINDOWS_PATH_CAPACITY = 4096,
  W_SEED_BUILD_WINDOWS_RANDOM_BYTES = 16,
};

static bool build_windows_utf8_to_wide(const char *source, wchar_t *destination,
                                       size_t capacity) {
  if (source == NULL || destination == NULL || capacity < 2u ||
      capacity > (size_t)INT_MAX)
    return false;
  const size_t source_length = strlen(source);
  if (source_length > (size_t)INT_MAX) return false;
  const int converted = MultiByteToWideChar(
      CP_UTF8, MB_ERR_INVALID_CHARS, source, (int)source_length, destination,
      (int)(capacity - 1u));
  if (converted <= 0) return false;
  destination[converted] = L'\0';
  return true;
}

static bool build_windows_path_join(char *buffer, size_t capacity,
                                    const char *directory, const char *name) {
  if (buffer == NULL || capacity < 2u || directory == NULL || name == NULL)
    return false;
  const size_t directory_length = strlen(directory);
  const size_t name_length = strlen(name);
  const bool separator = directory_length != 0u &&
                         directory[directory_length - 1u] != '\\' &&
                         directory[directory_length - 1u] != '/';
  const size_t required = directory_length + (separator ? 1u : 0u) +
                          name_length + 1u;
  if (required > capacity) return false;
  (void)memcpy(buffer, directory, directory_length);
  size_t offset = directory_length;
  if (separator) buffer[offset++] = '\\';
  (void)memcpy(buffer + offset, name, name_length);
  buffer[offset + name_length] = '\0';
  return true;
}

static bool build_windows_split_output(const char *output, char *parent,
                                       size_t parent_capacity) {
  if (output == NULL || parent == NULL || parent_capacity == 0u ||
      output[0] == '\0' || output[0] == '-')
    return false;
  const size_t output_length = strlen(output);
  if (output_length >= W_SEED_BUILD_WINDOWS_PATH_CAPACITY) return false;
  const char *slash = strrchr(output, '/');
  const char *backslash = strrchr(output, '\\');
  const char *separator = slash;
  if (separator == NULL ||
      (backslash != NULL && backslash > separator))
    separator = backslash;
  const char *name = separator == NULL ? output : separator + 1;
  if (name[0] == '\0') return false;
  if (separator == NULL) {
    if (parent_capacity < 2u) return false;
    parent[0] = '.';
    parent[1] = '\0';
    return true;
  }
  size_t parent_length = (size_t)(separator - output);
  if (parent_length == 2u && output[1] == ':') parent_length += 1u;
  if (parent_length + 1u > parent_capacity) return false;
  (void)memcpy(parent, output, parent_length);
  parent[parent_length] = '\0';
  return true;
}

static bool build_windows_parent_is_physical_directory(const char *parent) {
  wchar_t wide_parent[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  if (!build_windows_utf8_to_wide(
          parent, wide_parent,
          sizeof(wide_parent) / sizeof(wide_parent[0])))
    return false;
  const DWORD attributes = GetFileAttributesW(wide_parent);
  return attributes != INVALID_FILE_ATTRIBUTES &&
         (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0u &&
         (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0u;
}

static bool build_windows_output_is_new(const char *output) {
  wchar_t wide_output[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  if (!build_windows_utf8_to_wide(
          output, wide_output,
          sizeof(wide_output) / sizeof(wide_output[0])))
    return false;
  if (GetFileAttributesW(wide_output) != INVALID_FILE_ATTRIBUTES) return false;
  const DWORD error = GetLastError();
  if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND)
    return false;
  const HANDLE reparse = CreateFileW(
      wide_output, 0u, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      NULL, OPEN_EXISTING,
      FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (reparse != INVALID_HANDLE_VALUE) {
    (void)CloseHandle(reparse);
    return false;
  }
  const DWORD reparse_error = GetLastError();
  return reparse_error == ERROR_FILE_NOT_FOUND ||
         reparse_error == ERROR_PATH_NOT_FOUND;
}

static bool build_windows_random_name(char *buffer, size_t capacity) {
  static const char hex[] = "0123456789abcdef";
  uint8_t bytes[W_SEED_BUILD_WINDOWS_RANDOM_BYTES];
  if (buffer == NULL || capacity < sizeof(bytes) * 2u + 1u ||
      BCryptGenRandom(NULL, bytes, (ULONG)sizeof(bytes),
                      BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
    return false;
  for (size_t index = 0u; index < sizeof(bytes); index += 1u) {
    buffer[index * 2u] = hex[bytes[index] >> 4u];
    buffer[index * 2u + 1u] = hex[bytes[index] & 0x0fu];
  }
  buffer[sizeof(bytes) * 2u] = '\0';
  return true;
}

static bool build_windows_create_stage(const char *parent, char *stage,
                                       size_t stage_capacity) {
  wchar_t wide_candidate[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  char random_name[W_SEED_BUILD_WINDOWS_RANDOM_BYTES * 2u + 1u];
  char candidate[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  for (size_t attempt = 0u; attempt < 32u; attempt += 1u) {
    if (!build_windows_random_name(random_name, sizeof(random_name)) ||
        !build_windows_path_join(candidate, sizeof(candidate), parent,
                                 ".w-build") ||
        strlen(candidate) + 1u + strlen(random_name) + 1u >
            sizeof(candidate) ||
        snprintf(candidate + strlen(candidate),
                 sizeof(candidate) - strlen(candidate), "-%s", random_name) <
            0 ||
        !build_windows_utf8_to_wide(
            candidate, wide_candidate,
            sizeof(wide_candidate) / sizeof(wide_candidate[0])))
      return false;
    if (CreateDirectoryW(wide_candidate, NULL) != 0) {
      if (strlen(candidate) + 1u > stage_capacity) {
        (void)RemoveDirectoryW(wide_candidate);
        return false;
      }
      (void)memcpy(stage, candidate, strlen(candidate) + 1u);
      return true;
    }
    if (GetLastError() != ERROR_ALREADY_EXISTS) return false;
  }
  return false;
}

static bool build_windows_hidden_artifact_path(const char *stage,
                                               char *hidden,
                                               size_t hidden_capacity) {
  if (stage == NULL || hidden == NULL || hidden_capacity == 0u) return false;
  const int length = snprintf(hidden, hidden_capacity, "%s.artifact.exe",
                              stage);
  return length >= 0 && (size_t)length < hidden_capacity;
}

static bool build_windows_remove_hidden(const wchar_t *path) {
  if (path == NULL || path[0] == L'\0') return true;
  if (DeleteFileW(path) != 0) return true;
  const DWORD error = GetLastError();
  return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

static bool build_cleanup_windows(const char *stage, const char *artifact,
                                  const wchar_t *hidden,
                                  bool hidden_moved) {
  bool clean = true;
  if (hidden_moved && !build_windows_remove_hidden(hidden)) clean = false;
  if (!w_seed_run_cleanup_compiled(stage, artifact)) clean = false;
  return clean;
}

static int build_execute_windows(const w_seed_build_request *request) {
  if (request == NULL ||
      strcmp(request->target, W_SEED_NATIVE_TARGET_WINDOWS) != 0 ||
      strlen(request->path) > W_SEED_NATIVE0_MAX_PATH_BYTES)
    return 2;

  char parent[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  if (!build_windows_split_output(request->output, parent, sizeof(parent)) ||
      !build_windows_parent_is_physical_directory(parent) ||
      !build_windows_output_is_new(request->output))
    return 2;

  char stage[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  char artifact[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  if (!build_windows_create_stage(parent, stage, sizeof(stage)) ||
      !build_windows_path_join(artifact, sizeof(artifact), stage,
                               "program.exe")) {
    if (stage[0] != '\0') {
      wchar_t wide_stage[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
      if (build_windows_utf8_to_wide(
              stage, wide_stage,
              sizeof(wide_stage) / sizeof(wide_stage[0])))
        (void)RemoveDirectoryW(wide_stage);
    }
    return 3;
  }

  char hidden[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t wide_artifact[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t wide_hidden[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t wide_output[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  bool hidden_moved = false;
  const w_seed_run_compile_request compile_request = {
      .source_path = request->path,
      .target = request->target,
      .directory = stage,
      .artifact_path = artifact,
      .profile = W_SEED_RUN_COMPILE_PROFILE_RELEASE};
  int exit_code = w_seed_run_compile(&compile_request);
  if (exit_code == 0) {
    if (!build_windows_hidden_artifact_path(
            stage, hidden, sizeof(hidden)) ||
        !build_windows_utf8_to_wide(
            artifact, wide_artifact,
            sizeof(wide_artifact) / sizeof(wide_artifact[0])) ||
        !build_windows_utf8_to_wide(
            hidden, wide_hidden,
            sizeof(wide_hidden) / sizeof(wide_hidden[0])) ||
        !build_windows_utf8_to_wide(
            request->output, wide_output,
            sizeof(wide_output) / sizeof(wide_output[0])) ||
        MoveFileExW(wide_artifact, wide_hidden, MOVEFILE_WRITE_THROUGH) == 0) {
      exit_code = 3;
      goto cleanup;
    }
    hidden_moved = true;
    if (!w_seed_run_cleanup_compiled(stage, artifact)) {
      exit_code = 3;
      goto cleanup;
    }
    if (MoveFileExW(wide_hidden, wide_output, MOVEFILE_WRITE_THROUGH) == 0) {
      const DWORD error = GetLastError();
      exit_code = error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS ||
                          GetFileAttributesW(wide_output) != INVALID_FILE_ATTRIBUTES
                      ? 2
                      : 3;
      goto cleanup;
    }
    return 0;
  }

cleanup:
  if (!build_cleanup_windows(stage, artifact, wide_hidden, hidden_moved))
    return 3;
  return exit_code;
}

#else

static int build_execute_windows(const w_seed_build_request *request) {
  (void)request;
  return 2;
}

#endif

#endif

int w_seed_build_execute(const w_seed_build_request *request) {
  if (request == NULL || !build_source_path_is_valid(request->path) ||
      request->target == NULL || request->output == NULL ||
      request->target[0] == '\0' || request->output[0] == '\0')
    return 2;
#if defined(__linux__)
  return build_execute_linux(request);
#elif defined(_WIN32)
  return build_execute_windows(request);
#else
  (void)request;
  return 2;
#endif
}
