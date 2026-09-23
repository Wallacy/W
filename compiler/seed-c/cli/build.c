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
    *request = (w_seed_build_request){NULL, NULL, NULL,
                                      W_SEED_RUN_COMPILE_PIE_ON, false, NULL};
  if (request == NULL || argv == NULL || (argc < 7 || argc > 11 ||
                                           argc % 2 == 0) ||
      argv[1] == NULL ||
      strcmp(argv[1], "build") != 0 ||
      !build_source_path_is_valid(argv[2]))
    return false;

  const char *target = NULL;
  const char *output = NULL;
  bool target_seen = false;
  bool output_seen = false;
  bool pie_seen = false;
  bool audit_seen = false;
  const char *audit_directory = NULL;
  w_seed_run_compile_pie_mode pie_mode = W_SEED_RUN_COMPILE_PIE_ON;
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
    } else if (strcmp(option, "--pie") == 0) {
      if (pie_seen) return false;
      pie_seen = true;
      if (strcmp(value, "on") == 0)
        pie_mode = W_SEED_RUN_COMPILE_PIE_ON;
      else if (strcmp(value, "off") == 0)
        pie_mode = W_SEED_RUN_COMPILE_PIE_OFF;
      else
        return false;
    } else if (strcmp(option, "--audit-dir") == 0) {
      if (audit_seen) return false;
      audit_seen = true;
      audit_directory = value;
    } else {
      return false;
    }
  }
  if (!target_seen || !output_seen ||
      (pie_seen &&
       strcmp(target, W_SEED_NATIVE_TARGET_LINUX) != 0) ||
      (audit_seen &&
       strcmp(target, W_SEED_NATIVE_TARGET_LINUX) != 0))
    return false;
  request->path = argv[2];
  request->target = target;
  request->output = output;
  request->pie_mode = pie_mode;
  request->pie_mode_explicit = pie_seen;
  request->audit_directory = audit_directory;
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

static bool build_path_has_parent_component(const char *path) {
  if (path == NULL) return true;
  const size_t length = strlen(path);
  for (size_t start = 0u; start < length;) {
    while (start < length && path[start] == '/') start += 1u;
    size_t end = start;
    while (end < length && path[end] != '/') end += 1u;
    if (end - start == 2u && path[start] == '.' && path[start + 1u] == '.')
      return true;
    start = end;
  }
  return false;
}

static bool build_parent_chain_is_physical(const char *parent) {
  if (parent == NULL || parent[0] == '\0' ||
      strlen(parent) >= (size_t)PATH_MAX ||
      build_path_has_parent_component(parent))
    return false;
  char current[PATH_MAX] = {0};
  const bool absolute = parent[0] == '/';
  if (absolute) {
    current[0] = '/';
    current[1] = '\0';
  } else {
    current[0] = '.';
    current[1] = '\0';
  }
  struct stat status;
  if (lstat(current, &status) != 0 || S_ISLNK(status.st_mode) ||
      !S_ISDIR(status.st_mode))
    return false;
  const char *cursor = parent + (absolute ? 1u : 0u);
  while (*cursor != '\0') {
    while (*cursor == '/') cursor += 1u;
    if (*cursor == '\0') break;
    const char *end = strchr(cursor, '/');
    const size_t component_length =
        end == NULL ? strlen(cursor) : (size_t)(end - cursor);
    if (component_length == 0u ||
        (component_length == 1u && cursor[0] == '.')) {
      cursor += component_length;
      continue;
    }
    const size_t current_length = strlen(current);
    const bool separator = current_length != 0u &&
                           current[current_length - 1u] != '/';
    if (current_length + (separator ? 1u : 0u) + component_length + 1u >
        sizeof(current))
      return false;
    size_t offset = current_length;
    if (separator) current[offset++] = '/';
    (void)memcpy(current + offset, cursor, component_length);
    current[offset + component_length] = '\0';
    if (lstat(current, &status) != 0 || S_ISLNK(status.st_mode) ||
        !S_ISDIR(status.st_mode))
      return false;
    cursor += component_length;
  }
  return true;
}

static bool build_same_physical_target(const char *left, const char *right) {
  char left_parent[PATH_MAX] = {0};
  char right_parent[PATH_MAX] = {0};
  if (!build_split_output(left, left_parent, sizeof(left_parent)) ||
      !build_split_output(right, right_parent, sizeof(right_parent)))
    return false;
  const char *left_name = strrchr(left, '/');
  const char *right_name = strrchr(right, '/');
  left_name = left_name == NULL ? left : left_name + 1;
  right_name = right_name == NULL ? right : right_name + 1;
  char left_resolved[PATH_MAX] = {0};
  char right_resolved[PATH_MAX] = {0};
  if (realpath(left_parent, left_resolved) == NULL ||
      realpath(right_parent, right_resolved) == NULL)
    return false;
  char left_target[PATH_MAX] = {0};
  char right_target[PATH_MAX] = {0};
  if (!build_path_join(left_target, sizeof(left_target), left_resolved,
                       left_name) ||
      !build_path_join(right_target, sizeof(right_target), right_resolved,
                       right_name))
    return false;
  return strcmp(left_target, right_target) == 0;
}

static bool build_output_is_new(const char *output) {
  struct stat status;
  if (output == NULL) return false;
  if (lstat(output, &status) == 0) return false;
  return errno == ENOENT;
}

static bool build_audit_target_is_safe(const char *audit_path, char *parent,
                                      size_t parent_capacity) {
  if (audit_path == NULL || audit_path[0] == '\0' || audit_path[0] == '-' ||
      strlen(audit_path) >= (size_t)PATH_MAX ||
      build_path_has_parent_component(audit_path) ||
      !build_split_output(audit_path, parent, parent_capacity) ||
      !build_parent_chain_is_physical(parent) ||
      !build_output_is_new(audit_path))
    return false;
  const char *name = strrchr(audit_path, '/');
  name = name == NULL ? audit_path : name + 1;
  return name[0] != '\0' && strcmp(name, ".") != 0 &&
         strcmp(name, "..") != 0;
}

static bool build_create_stage(const char *parent, const char *prefix,
                               char *stage,
                               size_t stage_capacity) {
  if (stage == NULL || stage_capacity == 0u || parent == NULL ||
      prefix == NULL || prefix[0] == '\0')
    return false;
  char candidate[PATH_MAX] = {0};
  const int length = snprintf(candidate, sizeof(candidate),
                              "%s/%s-XXXXXX", parent, prefix);
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

static bool build_copy_file(const char *source, const char *destination,
                            mode_t mode) {
  if (source == NULL || destination == NULL) return false;
  const int input = open(source, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  if (input < 0) return false;
  struct stat source_status;
  if (fstat(input, &source_status) != 0 ||
      !S_ISREG(source_status.st_mode)) {
    (void)close(input);
    return false;
  }
  const int output = open(destination, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC |
                                           O_NOFOLLOW,
                          mode);
  if (output < 0) {
    (void)close(input);
    return false;
  }
  uint8_t bytes[16384];
  bool copied = true;
  for (;;) {
    const ssize_t read_count = read(input, bytes, sizeof(bytes));
    if (read_count < 0 && errno == EINTR) continue;
    if (read_count < 0) {
      copied = false;
      break;
    }
    if (read_count == 0) break;
    size_t offset = 0u;
    while (offset < (size_t)read_count) {
      const ssize_t written = write(output, bytes + offset,
                                    (size_t)read_count - offset);
      if (written < 0 && errno == EINTR) continue;
      if (written <= 0) {
        copied = false;
        break;
      }
      offset += (size_t)written;
    }
    if (!copied) break;
  }
  struct stat after_status;
  if (fstat(input, &after_status) != 0 ||
      after_status.st_dev != source_status.st_dev ||
      after_status.st_ino != source_status.st_ino ||
      after_status.st_size != source_status.st_size)
    copied = false;
  if (copied && fchmod(output, mode) != 0) copied = false;
  if (close(input) != 0) copied = false;
  if (close(output) != 0) copied = false;
  if (!copied) (void)unlink(destination);
  return copied;
}

static bool build_copy_audit_bundle(const char *source_directory,
                                   const char *audit_directory,
                                   const char *final_artifact_name) {
  static const char *const files[] = {
      "input.mlir", "verified.mlir", "output.ll", "optimized.ll",
      "output.o", "wrt0.ll", "wrt0.o", "manifest.json"};
  char source[PATH_MAX] = {0};
  char destination[PATH_MAX] = {0};
  for (size_t index = 0u; index < sizeof(files) / sizeof(files[0]);
       index += 1u) {
    if (!build_path_join(source, sizeof(source), source_directory, files[index]) ||
        !build_path_join(destination, sizeof(destination), audit_directory,
                         files[index]) ||
        !build_copy_file(source, destination, (mode_t)0600))
      return false;
  }
  if (!build_path_join(source, sizeof(source), source_directory,
                       "final-artifact") ||
      !build_path_join(destination, sizeof(destination), audit_directory,
                       final_artifact_name) ||
      !build_copy_file(source, destination, (mode_t)0700))
    return false;
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
                                const char *hidden, bool hidden_linked,
                                const char *audit_stage,
                                const char *audit_artifact) {
  bool clean = true;
  if (hidden_linked && !build_remove_hidden(hidden)) clean = false;
  if (!w_seed_run_cleanup_compiled(stage, artifact)) clean = false;
  if (audit_stage != NULL && audit_stage[0] != '\0' &&
      !w_seed_run_cleanup_compiled(audit_stage, audit_artifact))
    clean = false;
  return clean;
}

static bool build_remove_published_if_same(const char *output,
                                           const struct stat *identity) {
  struct stat current;
  if (output == NULL || identity == NULL || lstat(output, &current) != 0 ||
      !S_ISREG(current.st_mode) || current.st_dev != identity->st_dev ||
      current.st_ino != identity->st_ino)
    return false;
  return unlink(output) == 0;
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

  char audit_parent[PATH_MAX] = {0};
  if (request->audit_directory != NULL &&
      (!build_audit_target_is_safe(request->audit_directory, audit_parent,
                                   sizeof(audit_parent)) ||
       !build_parent_chain_is_physical(parent) ||
       build_same_physical_target(request->audit_directory, request->output)))
    return 2;

  char stage[PATH_MAX] = {0};
  char artifact[PATH_MAX] = {0};
  char audit_stage[PATH_MAX] = {0};
  char audit_artifact[PATH_MAX] = {0};
  if (!build_create_stage(parent, ".w-build", stage, sizeof(stage)) ||
      !build_path_join(artifact, sizeof(artifact), stage, "program")) {
    if (stage[0] != '\0') (void)rmdir(stage);
    return 3;
  }
  if (request->audit_directory != NULL &&
      (!build_create_stage(audit_parent, ".w-audit", audit_stage,
                           sizeof(audit_stage)) ||
       !build_path_join(audit_artifact, sizeof(audit_artifact), audit_stage,
                        "final-artifact"))) {
    if (audit_stage[0] != '\0') (void)rmdir(audit_stage);
    (void)w_seed_run_cleanup_compiled(stage, artifact);
    return 3;
  }
  const w_seed_run_compile_request compile_request = {
      .source_path = request->path,
      .target = request->target,
      .directory = stage,
      .artifact_path = artifact,
      .profile = W_SEED_RUN_COMPILE_PROFILE_RELEASE,
      .pie_mode = request->pie_mode,
      .retain_audit_trace = request->audit_directory != NULL};
  int exit_code = w_seed_run_compile(&compile_request);
  char hidden[PATH_MAX] = {0};
  bool hidden_linked = false;
  bool published = false;
  struct stat publication_identity;
  if (exit_code == 0) {
    if (request->audit_directory != NULL &&
        !build_copy_audit_bundle(stage, audit_stage, "final-artifact")) {
      exit_code = 3;
      goto cleanup;
    }
    if (!build_hidden_artifact_path(stage, hidden, sizeof(hidden)) ||
        link(artifact, hidden) != 0) {
      exit_code = 3;
      goto cleanup;
    }
    hidden_linked = true;
    if (lstat(hidden, &publication_identity) != 0 ||
        !w_seed_run_cleanup_compiled(stage, artifact)) {
      exit_code = 3;
      goto cleanup;
    }
    if (build_publish_linux(hidden, request->output, &hidden_linked) == 0) {
      published = true;
      if (request->audit_directory == NULL) return 0;
      if (build_rename_no_replace(audit_stage, request->audit_directory) == 0)
        return 0;
      const int audit_error = errno;
      if (build_remove_published_if_same(request->output,
                                         &publication_identity))
        published = false;
      exit_code = audit_error == EEXIST || audit_error == ENOTEMPTY ? 2 : 3;
      goto cleanup;
    }
    const int publish_error = errno;
    exit_code = publish_error == EEXIST || publish_error == EISDIR ? 2 : 3;
  }

cleanup:
  if (!build_cleanup_linux(stage, artifact, hidden, hidden_linked,
                           audit_stage, audit_artifact))
    return 3;
  (void)published;
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

static bool build_windows_path_has_parent_component(const char *path) {
  if (path == NULL) return true;
  const size_t length = strlen(path);
  for (size_t start = 0u; start < length;) {
    while (start < length && (path[start] == '/' || path[start] == '\\'))
      start += 1u;
    size_t end = start;
    while (end < length && path[end] != '/' && path[end] != '\\') end += 1u;
    if (end - start == 2u && path[start] == '.' && path[start + 1u] == '.')
      return true;
    start = end;
  }
  return false;
}

static bool build_windows_parent_chain_is_physical(const char *parent) {
  wchar_t wide_parent[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t full_parent[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  if (parent == NULL || parent[0] == '\0' ||
      build_windows_path_has_parent_component(parent) ||
      !build_windows_utf8_to_wide(
          parent, wide_parent,
          sizeof(wide_parent) / sizeof(wide_parent[0])))
    return false;
  const DWORD full_length = GetFullPathNameW(
      wide_parent, (DWORD)(sizeof(full_parent) / sizeof(full_parent[0])),
      full_parent, NULL);
  if (full_length == 0u ||
      full_length >= sizeof(full_parent) / sizeof(full_parent[0]) ||
      full_parent[0] == L'\\' || full_parent[1] != L':' ||
      full_parent[2] != L'\\')
    return false;
  wchar_t current[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  current[0] = full_parent[0];
  current[1] = L':';
  current[2] = L'\\';
  current[3] = L'\0';
  DWORD attributes = GetFileAttributesW(current);
  if (attributes == INVALID_FILE_ATTRIBUTES ||
      (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0u ||
      (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u)
    return false;
  const wchar_t *cursor = full_parent + 3u;
  while (*cursor != L'\0') {
    while (*cursor == L'\\' || *cursor == L'/') cursor += 1u;
    if (*cursor == L'\0') break;
    const wchar_t *end = cursor;
    while (*end != L'\0' && *end != L'\\' && *end != L'/') end += 1u;
    const size_t component_length = (size_t)(end - cursor);
    const size_t current_length = wcslen(current);
    if (component_length == 0u ||
        current_length + component_length + 2u >=
            sizeof(current) / sizeof(current[0]))
      return false;
    current[current_length] = L'\\';
    (void)wmemcpy(current + current_length + 1u, cursor,
                  component_length);
    current[current_length + component_length + 1u] = L'\0';
    attributes = GetFileAttributesW(current);
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0u ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u)
      return false;
    cursor = end;
  }
  return true;
}

static bool build_windows_resolve_target(const char *path, wchar_t *resolved,
                                         size_t capacity) {
  char parent[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t wide_parent[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t full_parent[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  if (resolved == NULL || capacity < 4u ||
      !build_windows_split_output(path, parent, sizeof(parent)) ||
      !build_windows_utf8_to_wide(
          parent, wide_parent,
          sizeof(wide_parent) / sizeof(wide_parent[0])))
    return false;
  const DWORD full_length = GetFullPathNameW(
      wide_parent, (DWORD)(sizeof(full_parent) / sizeof(full_parent[0])),
      full_parent, NULL);
  if (full_length == 0u ||
      full_length >= sizeof(full_parent) / sizeof(full_parent[0]))
    return false;
  const char *name = strrchr(path, '/');
  const char *backslash = strrchr(path, '\\');
  if (name == NULL || (backslash != NULL && backslash > name)) name = backslash;
  name = name == NULL ? path : name + 1;
  wchar_t wide_name[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  if (!build_windows_utf8_to_wide(
          name, wide_name, sizeof(wide_name) / sizeof(wide_name[0])))
    return false;
  const size_t parent_length = wcslen(full_parent);
  const size_t name_length = wcslen(wide_name);
  const bool separator = parent_length != 0u &&
                         full_parent[parent_length - 1u] != L'\\' &&
                         full_parent[parent_length - 1u] != L'/';
  if (parent_length + (separator ? 1u : 0u) + name_length + 1u > capacity)
    return false;
  (void)wmemcpy(resolved, full_parent, parent_length);
  size_t offset = parent_length;
  if (separator) resolved[offset++] = L'\\';
  (void)wmemcpy(resolved + offset, wide_name, name_length);
  resolved[offset + name_length] = L'\0';
  return true;
}

static bool build_windows_same_target(const char *left, const char *right) {
  wchar_t left_resolved[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t right_resolved[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  return build_windows_resolve_target(
             left, left_resolved,
             sizeof(left_resolved) / sizeof(left_resolved[0])) &&
         build_windows_resolve_target(
             right, right_resolved,
             sizeof(right_resolved) / sizeof(right_resolved[0])) &&
         CompareStringOrdinal(left_resolved, -1, right_resolved, -1, TRUE) ==
             CSTR_EQUAL;
}

static bool build_windows_audit_target_is_safe(const char *audit_path,
                                               char *parent,
                                               size_t parent_capacity) {
  return audit_path != NULL && audit_path[0] != '\0' &&
         audit_path[0] != '-' &&
         strlen(audit_path) < W_SEED_BUILD_WINDOWS_PATH_CAPACITY &&
         !build_windows_path_has_parent_component(audit_path) &&
         build_windows_split_output(audit_path, parent, parent_capacity) &&
         build_windows_parent_chain_is_physical(parent) &&
         build_windows_output_is_new(audit_path);
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

static bool build_windows_create_stage(const char *parent, const char *prefix,
                                       char *stage,
                                       size_t stage_capacity) {
  wchar_t wide_candidate[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  char random_name[W_SEED_BUILD_WINDOWS_RANDOM_BYTES * 2u + 1u];
  char candidate[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  for (size_t attempt = 0u; attempt < 32u; attempt += 1u) {
    if (prefix == NULL || prefix[0] == '\0' ||
        !build_windows_random_name(random_name, sizeof(random_name)) ||
        !build_windows_path_join(candidate, sizeof(candidate), parent, prefix) ||
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

static bool build_windows_copy_audit_bundle(const char *source_directory,
                                            const char *audit_directory) {
  static const char *const files[] = {
      "input.mlir", "verified.mlir", "output.ll", "optimized.ll",
      "output.obj", "wrt0.ll", "wrt0.obj", "final-artifact",
      "manifest.json"};
  char source_path[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  char destination_path[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t wide_source[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t wide_destination[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  for (size_t index = 0u; index < sizeof(files) / sizeof(files[0]);
       index += 1u) {
    if (!build_windows_path_join(source_path, sizeof(source_path),
                                 source_directory, files[index]) ||
        !build_windows_path_join(destination_path, sizeof(destination_path),
                                 audit_directory, files[index]) ||
        !build_windows_utf8_to_wide(
            source_path, wide_source,
            sizeof(wide_source) / sizeof(wide_source[0])) ||
        !build_windows_utf8_to_wide(
            destination_path, wide_destination,
            sizeof(wide_destination) / sizeof(wide_destination[0])))
      return false;
    const DWORD source_attributes = GetFileAttributesW(wide_source);
    if (source_attributes == INVALID_FILE_ATTRIBUTES ||
        (source_attributes &
         (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0u ||
        !CopyFileW(wide_source, wide_destination, TRUE) ||
        !SetFileAttributesW(wide_destination, FILE_ATTRIBUTE_NORMAL))
      return false;
  }
  return true;
}

static bool build_windows_hidden_artifact_path(const char *stage,
                                               char *hidden,
                                               size_t hidden_capacity,
                                               bool windows_target) {
  if (stage == NULL || hidden == NULL || hidden_capacity == 0u) return false;
  const int length = snprintf(hidden, hidden_capacity,
                              windows_target ? "%s.artifact.exe"
                                             : "%s.artifact",
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
                                  bool hidden_moved,
                                  const char *audit_stage,
                                  const char *audit_artifact) {
  bool clean = true;
  if (hidden_moved && !build_windows_remove_hidden(hidden)) clean = false;
  if (!w_seed_run_cleanup_compiled(stage, artifact)) clean = false;
  if (audit_stage != NULL && audit_stage[0] != '\0' &&
      !w_seed_run_cleanup_compiled(audit_stage, audit_artifact))
    clean = false;
  return clean;
}

static bool build_windows_file_identity(const wchar_t *path,
                                        BY_HANDLE_FILE_INFORMATION *identity) {
  if (path == NULL || identity == NULL) return false;
  HANDLE file = CreateFileW(path, FILE_READ_ATTRIBUTES,
                            FILE_SHARE_READ | FILE_SHARE_WRITE |
                                FILE_SHARE_DELETE,
                            NULL, OPEN_EXISTING,
                            FILE_FLAG_OPEN_REPARSE_POINT, NULL);
  if (file == INVALID_HANDLE_VALUE) return false;
  const bool valid = GetFileInformationByHandle(file, identity) != 0 &&
                     (identity->dwFileAttributes &
                      (FILE_ATTRIBUTE_DIRECTORY |
                       FILE_ATTRIBUTE_REPARSE_POINT)) == 0u;
  (void)CloseHandle(file);
  return valid;
}

static bool build_windows_remove_published_if_same(
    const wchar_t *output, const BY_HANDLE_FILE_INFORMATION *identity) {
  BY_HANDLE_FILE_INFORMATION current;
  if (identity == NULL || !build_windows_file_identity(output, &current) ||
      current.dwVolumeSerialNumber != identity->dwVolumeSerialNumber ||
      current.nFileIndexHigh != identity->nFileIndexHigh ||
      current.nFileIndexLow != identity->nFileIndexLow)
    return false;
  return DeleteFileW(output) != 0;
}

static int build_execute_windows(const w_seed_build_request *request) {
  const bool windows_target =
      request != NULL && request->target != NULL &&
      strcmp(request->target, W_SEED_NATIVE_TARGET_WINDOWS) == 0;
  const bool linux_target =
      request != NULL && request->target != NULL &&
      strcmp(request->target, W_SEED_NATIVE_TARGET_LINUX) == 0;
  if (request == NULL ||
      (!windows_target && !linux_target) ||
      strlen(request->path) > W_SEED_NATIVE0_MAX_PATH_BYTES)
    return 2;

  char parent[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  if (!build_windows_split_output(request->output, parent, sizeof(parent)) ||
      !build_windows_parent_is_physical_directory(parent) ||
      !build_windows_output_is_new(request->output))
    return 2;

  char audit_parent[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  if (request->audit_directory != NULL &&
      (!build_windows_audit_target_is_safe(request->audit_directory,
                                           audit_parent,
                                           sizeof(audit_parent)) ||
       windows_target ||
       !build_windows_parent_chain_is_physical(parent) ||
       build_windows_same_target(request->audit_directory, request->output)))
    return 2;

  char stage[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  char artifact[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  char audit_stage[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  char audit_artifact[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  if (!build_windows_create_stage(parent, ".w-build", stage, sizeof(stage)) ||
      !build_windows_path_join(artifact, sizeof(artifact), stage,
                               windows_target ? "program.exe" : "program")) {
    if (stage[0] != '\0') {
      wchar_t wide_stage[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
      if (build_windows_utf8_to_wide(
              stage, wide_stage,
              sizeof(wide_stage) / sizeof(wide_stage[0])))
        (void)RemoveDirectoryW(wide_stage);
    }
    return 3;
  }
  if (request->audit_directory != NULL &&
      (!build_windows_create_stage(audit_parent, ".w-audit", audit_stage,
                                   sizeof(audit_stage)) ||
       !build_windows_path_join(audit_artifact, sizeof(audit_artifact),
                                audit_stage, "final-artifact"))) {
    if (audit_stage[0] != '\0') {
      wchar_t wide_stage[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
      if (build_windows_utf8_to_wide(
              audit_stage, wide_stage,
              sizeof(wide_stage) / sizeof(wide_stage[0])))
        (void)RemoveDirectoryW(wide_stage);
    }
    (void)w_seed_run_cleanup_compiled(stage, artifact);
    return 3;
  }

  char hidden[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t wide_artifact[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t wide_hidden[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t wide_output[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t wide_audit_stage[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  wchar_t wide_audit_target[W_SEED_BUILD_WINDOWS_PATH_CAPACITY] = {0};
  if (request->audit_directory != NULL &&
      (!build_windows_utf8_to_wide(
           audit_stage, wide_audit_stage,
           sizeof(wide_audit_stage) / sizeof(wide_audit_stage[0])) ||
       !build_windows_utf8_to_wide(
           request->audit_directory, wide_audit_target,
           sizeof(wide_audit_target) / sizeof(wide_audit_target[0])))) {
    if (!build_cleanup_windows(stage, artifact, NULL, false, audit_stage,
                               audit_artifact))
      return 3;
    return 2;
  }
  bool hidden_moved = false;
  bool published = false;
  BY_HANDLE_FILE_INFORMATION publication_identity;
  const w_seed_run_compile_request compile_request = {
      .source_path = request->path,
      .target = request->target,
      .directory = stage,
      .artifact_path = artifact,
      .profile = W_SEED_RUN_COMPILE_PROFILE_RELEASE,
      .pie_mode = request->pie_mode,
      .retain_audit_trace = request->audit_directory != NULL};
  int exit_code = w_seed_run_compile(&compile_request);
  if (exit_code == 0) {
    if (request->audit_directory != NULL &&
        !build_windows_copy_audit_bundle(stage, audit_stage)) {
      exit_code = 3;
      goto cleanup;
    }
    if (!build_windows_hidden_artifact_path(
            stage, hidden, sizeof(hidden), windows_target) ||
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
    if (!build_windows_file_identity(wide_hidden, &publication_identity)) {
      exit_code = 3;
      goto cleanup;
    }
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
    hidden_moved = false;
    published = true;
    if (request->audit_directory == NULL) return 0;
    if (MoveFileExW(wide_audit_stage, wide_audit_target,
                    MOVEFILE_WRITE_THROUGH) != 0)
      return 0;
    const DWORD audit_error = GetLastError();
    if (build_windows_remove_published_if_same(wide_output,
                                               &publication_identity))
      published = false;
    exit_code = audit_error == ERROR_ALREADY_EXISTS ||
                        audit_error == ERROR_FILE_EXISTS
                    ? 2
                    : 3;
    goto cleanup;
  }
cleanup:
  if (!build_cleanup_windows(stage, artifact, wide_hidden, hidden_moved,
                             audit_stage, audit_artifact))
    return 3;
  (void)published;
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
      request->target[0] == '\0' || request->output[0] == '\0' ||
      (request->pie_mode != W_SEED_RUN_COMPILE_PIE_ON &&
       request->pie_mode != W_SEED_RUN_COMPILE_PIE_OFF) ||
      (request->pie_mode_explicit &&
       strcmp(request->target, W_SEED_NATIVE_TARGET_LINUX) != 0) ||
      (request->pie_mode == W_SEED_RUN_COMPILE_PIE_OFF &&
       strcmp(request->target, W_SEED_NATIVE_TARGET_LINUX) != 0) ||
      (request->audit_directory != NULL &&
       (request->audit_directory[0] == '\0' ||
        request->audit_directory[0] == '-' ||
        strcmp(request->target, W_SEED_NATIVE_TARGET_LINUX) != 0)))
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
