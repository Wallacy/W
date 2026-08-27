#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0602
#endif

#include "w_seed_ephemeral_provider_windows.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)

#include <windows.h>
#include <fileapi.h>
#include <processthreadsapi.h>

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      (void)fprintf(stderr, "windows ephemeral check failed: %s (%s:%d)\n", \
                    #condition, __FILE__, __LINE__);                            \
      return false;                                                             \
    }                                                                           \
  } while (0)

enum {
  TEST_REQUEST_CAPACITY = 2,
  TEST_BYTE_CAPACITY = 128,
  TEST_TOKEN_CAPACITY = 96,
  TEST_PATH_CAPACITY = W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES + 1u,
  TEST_WIDE_CAPACITY = 1024,
};

typedef struct {
  char source_ids[TEST_REQUEST_CAPACITY][TEST_PATH_CAPACITY];
  uint8_t staging[TEST_REQUEST_CAPACITY][TEST_BYTE_CAPACITY];
  uint8_t revalidation[TEST_REQUEST_CAPACITY][TEST_BYTE_CAPACITY];
  uint8_t bytes[TEST_REQUEST_CAPACITY][TEST_BYTE_CAPACITY];
  w_seed_source sources[TEST_REQUEST_CAPACITY];
  w_seed_ephemeral_graph_provider_facts facts[TEST_REQUEST_CAPACITY];
  char provider[TEST_REQUEST_CAPACITY][TEST_TOKEN_CAPACITY];
  char root_token[TEST_REQUEST_CAPACITY][TEST_TOKEN_CAPACITY];
  char owner[TEST_REQUEST_CAPACITY][TEST_TOKEN_CAPACITY];
  char canonical[TEST_REQUEST_CAPACITY][TEST_TOKEN_CAPACITY];
  char revalidation_provider[TEST_REQUEST_CAPACITY][TEST_TOKEN_CAPACITY];
  char revalidation_root_token[TEST_REQUEST_CAPACITY][TEST_TOKEN_CAPACITY];
  char revalidation_owner[TEST_REQUEST_CAPACITY][TEST_TOKEN_CAPACITY];
  char revalidation_canonical[TEST_REQUEST_CAPACITY][TEST_TOKEN_CAPACITY];
  char root_path[TEST_PATH_CAPACITY];
  w_seed_ephemeral_provider_request requests[TEST_REQUEST_CAPACITY];
  w_seed_ephemeral_provider_input input;
} provider_case;

typedef struct {
  uint8_t bytes[TEST_REQUEST_CAPACITY][TEST_BYTE_CAPACITY];
  w_seed_source sources[TEST_REQUEST_CAPACITY];
  w_seed_ephemeral_graph_provider_facts facts[TEST_REQUEST_CAPACITY];
} output_snapshot;

typedef struct {
  WCHAR directory[TEST_WIDE_CAPACITY];
  WCHAR root[TEST_WIDE_CAPACITY];
  WCHAR unicode_root[TEST_WIDE_CAPACITY];
  WCHAR child[TEST_WIDE_CAPACITY];
  WCHAR empty[TEST_WIDE_CAPACITY];
  WCHAR alias[TEST_WIDE_CAPACITY];
  WCHAR nested[TEST_WIDE_CAPACITY];
  WCHAR directory_source[TEST_WIDE_CAPACITY];
  WCHAR junction[TEST_WIDE_CAPACITY];
  WCHAR final_junction[TEST_WIDE_CAPACITY];
  WCHAR root_symlink[TEST_WIDE_CAPACITY];
  WCHAR nested_symlink[TEST_WIDE_CAPACITY];
  HANDLE base_handle;
  bool symlink_available;
  bool symlink_skipped;
  w_seed_ephemeral_provider_windows_context context;
  w_seed_ephemeral_provider_backend backend;
} windows_environment;

typedef enum {
  PROXY_MUTATE = 0,
  PROXY_REPLACE,
  PROXY_REMOVE,
} proxy_mode;

typedef struct {
  w_seed_ephemeral_provider_backend delegate;
  WCHAR target_path[TEST_WIDE_CAPACITY];
  proxy_mode mode;
  size_t read_count;
  bool changed;
} adapter_proxy;

static size_t wide_length(const WCHAR *text) {
  if (text == NULL) return 0u;
  size_t length = 0u;
  while (text[length] != (WCHAR)0) length += 1u;
  return length;
}

static bool wide_copy(WCHAR *destination, size_t capacity,
                      const WCHAR *source) {
  if (destination == NULL || source == NULL) return false;
  const size_t length = wide_length(source);
  if (length >= capacity) return false;
  (void)memcpy(destination, source, (length + 1u) * sizeof(WCHAR));
  return true;
}

static bool wide_append(WCHAR *destination, size_t capacity,
                        const WCHAR *suffix) {
  if (destination == NULL || suffix == NULL) return false;
  const size_t left = wide_length(destination);
  const size_t right = wide_length(suffix);
  if (left > capacity || right > capacity - left - 1u) return false;
  (void)memcpy(destination + left, suffix, (right + 1u) * sizeof(WCHAR));
  return true;
}

static bool wide_join(const WCHAR *directory, const WCHAR *name,
                      WCHAR *destination, size_t capacity) {
  if (!wide_copy(destination, capacity, directory)) return false;
  const size_t length = wide_length(destination);
  if (length != 0u && destination[length - 1u] != (WCHAR)L'\\' &&
      destination[length - 1u] != (WCHAR)L'/') {
    if (!wide_append(destination, capacity, L"\\")) return false;
  }
  return wide_append(destination, capacity, name);
}

static bool copy_c_string(char *destination, size_t capacity,
                          const char *source) {
  if (destination == NULL || source == NULL) return false;
  const size_t length = strlen(source);
  if (length >= capacity) return false;
  (void)memcpy(destination, source, length + 1u);
  return true;
}

static bool set_request_id(provider_case *test_case, size_t index,
                           const char *source_id) {
  if (test_case == NULL || index >= TEST_REQUEST_CAPACITY ||
      !copy_c_string(test_case->source_ids[index],
                     sizeof(test_case->source_ids[index]), source_id))
    return false;
  test_case->requests[index].source_id =
      (w_seed_frontend_text){test_case->source_ids[index],
                             strlen(test_case->source_ids[index])};
  return true;
}

static bool initialize_provider_case(provider_case *test_case,
                                     size_t request_count,
                                     size_t root_request_index,
                                     const char *root_path) {
  if (test_case == NULL || request_count == 0u ||
      request_count > TEST_REQUEST_CAPACITY ||
      root_request_index >= request_count || root_path == NULL ||
      strlen(root_path) >= sizeof(test_case->root_path))
    return false;
  (void)memset(test_case, 0, sizeof(*test_case));
  if (!copy_c_string(test_case->root_path, sizeof(test_case->root_path),
                     root_path))
    return false;
  for (size_t index = 0u; index < request_count; index += 1u) {
    const char *source_id = index == root_request_index
                                ? "root.w"
                                : "nested/child.w";
    if (!set_request_id(test_case, index, source_id)) return false;
    (void)memset(test_case->staging[index], 0x11, TEST_BYTE_CAPACITY);
    (void)memset(test_case->revalidation[index], 0x22, TEST_BYTE_CAPACITY);
    (void)memset(test_case->bytes[index], 0xA5, TEST_BYTE_CAPACITY);
    (void)memset(&test_case->sources[index], 0xB6,
                 sizeof(test_case->sources[index]));
    (void)memset(&test_case->facts[index], 0xC7,
                 sizeof(test_case->facts[index]));
    (void)memset(test_case->provider[index], 0x5A, TEST_TOKEN_CAPACITY);
    (void)memset(test_case->root_token[index], 0x5A, TEST_TOKEN_CAPACITY);
    (void)memset(test_case->owner[index], 0x5A, TEST_TOKEN_CAPACITY);
    (void)memset(test_case->canonical[index], 0x5A, TEST_TOKEN_CAPACITY);
    (void)memset(test_case->revalidation_provider[index], 0x41,
                 TEST_TOKEN_CAPACITY);
    (void)memset(test_case->revalidation_root_token[index], 0x42,
                 TEST_TOKEN_CAPACITY);
    (void)memset(test_case->revalidation_owner[index], 0x43,
                 TEST_TOKEN_CAPACITY);
    (void)memset(test_case->revalidation_canonical[index], 0x44,
                 TEST_TOKEN_CAPACITY);
    test_case->requests[index].staging_bytes = test_case->staging[index];
    test_case->requests[index].staging_capacity = TEST_BYTE_CAPACITY;
    test_case->requests[index].revalidation_bytes =
        test_case->revalidation[index];
    test_case->requests[index].revalidation_capacity = TEST_BYTE_CAPACITY;
    test_case->requests[index].bytes = test_case->bytes[index];
    test_case->requests[index].byte_capacity = TEST_BYTE_CAPACITY;
    test_case->requests[index].source = &test_case->sources[index];
    test_case->requests[index].facts = &test_case->facts[index];
    test_case->requests[index].tokens =
        (w_seed_ephemeral_provider_token_buffers){
            test_case->provider[index], TEST_TOKEN_CAPACITY,
            test_case->root_token[index], TEST_TOKEN_CAPACITY,
            test_case->owner[index], TEST_TOKEN_CAPACITY,
            test_case->canonical[index], TEST_TOKEN_CAPACITY};
    test_case->requests[index].revalidation_tokens =
        (w_seed_ephemeral_provider_token_buffers){
            test_case->revalidation_provider[index], TEST_TOKEN_CAPACITY,
            test_case->revalidation_root_token[index], TEST_TOKEN_CAPACITY,
            test_case->revalidation_owner[index], TEST_TOKEN_CAPACITY,
            test_case->revalidation_canonical[index], TEST_TOKEN_CAPACITY};
  }
  test_case->input.root_path =
      (w_seed_byte_view){(const uint8_t *)test_case->root_path,
                         strlen(test_case->root_path)};
  test_case->input.requests = test_case->requests;
  test_case->input.request_count = request_count;
  test_case->input.root_request_index = root_request_index;
  test_case->input.limits = (w_seed_ephemeral_provider_limits){
      TEST_REQUEST_CAPACITY, TEST_BYTE_CAPACITY,
      TEST_REQUEST_CAPACITY * TEST_BYTE_CAPACITY,
      W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES, TEST_TOKEN_CAPACITY};
  return true;
}

static void snapshot_outputs(const provider_case *test_case,
                             output_snapshot *snapshot) {
  if (test_case == NULL || snapshot == NULL) return;
  for (size_t index = 0u; index < TEST_REQUEST_CAPACITY; index += 1u) {
    (void)memcpy(snapshot->bytes[index], test_case->bytes[index],
                 TEST_BYTE_CAPACITY);
    snapshot->sources[index] = test_case->sources[index];
    snapshot->facts[index] = test_case->facts[index];
  }
}

static bool outputs_unchanged(const provider_case *test_case,
                              const output_snapshot *snapshot) {
  if (test_case == NULL || snapshot == NULL) return false;
  for (size_t index = 0u; index < TEST_REQUEST_CAPACITY; index += 1u) {
    if (memcmp(snapshot->bytes[index], test_case->bytes[index],
               TEST_BYTE_CAPACITY) != 0 ||
        memcmp(&snapshot->sources[index], &test_case->sources[index],
               sizeof(snapshot->sources[index])) != 0 ||
        memcmp(&snapshot->facts[index], &test_case->facts[index],
               sizeof(snapshot->facts[index])) != 0)
      return false;
  }
  return true;
}

static bool text_equals_literal(w_seed_frontend_text text,
                                const char *literal) {
  if (literal == NULL) return false;
  const size_t length = strlen(literal);
  return text.length == length &&
         (length == 0u || memcmp(text.data, literal, length) == 0);
}

static bool write_file(const WCHAR *path, const uint8_t *bytes, size_t length) {
  if (path == NULL || (length != 0u && bytes == NULL) ||
      length > (size_t)UINT32_MAX)
    return false;
  HANDLE file = CreateFileW(path, GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE |
                                FILE_SHARE_DELETE,
                            NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0u;
  const BOOL success = WriteFile(file, bytes, (DWORD)length, &written, NULL);
  const BOOL closed = CloseHandle(file);
  return success != FALSE && closed != FALSE && written == (DWORD)length;
}

static bool remove_file(const WCHAR *path) {
  if (path == NULL) return false;
  if (DeleteFileW(path) != FALSE) return true;
  return GetLastError() == ERROR_FILE_NOT_FOUND ||
         GetLastError() == ERROR_PATH_NOT_FOUND;
}

static bool remove_directory(const WCHAR *path) {
  if (path == NULL) return false;
  if (RemoveDirectoryW(path) != FALSE) return true;
  return GetLastError() == ERROR_FILE_NOT_FOUND ||
         GetLastError() == ERROR_PATH_NOT_FOUND;
}

static bool append_command(WCHAR *command, size_t capacity,
                           const WCHAR *text) {
  return wide_append(command, capacity, text);
}

static bool create_junction(const WCHAR *link_path, const WCHAR *target_path) {
  if (link_path == NULL || target_path == NULL) return false;
  WCHAR shell[TEST_WIDE_CAPACITY];
  const DWORD shell_length =
      GetEnvironmentVariableW(L"ComSpec", shell, (DWORD)TEST_WIDE_CAPACITY);
  if (shell_length == 0u || shell_length >= (DWORD)TEST_WIDE_CAPACITY)
    return false;
  WCHAR command[TEST_WIDE_CAPACITY * 2u];
  command[0] = (WCHAR)0;
  if (!append_command(command, sizeof(command) / sizeof(command[0]), L"\"") ||
      !append_command(command, sizeof(command) / sizeof(command[0]), shell) ||
      !append_command(command, sizeof(command) / sizeof(command[0]),
                      L"\" /d /c mklink /J \"") ||
      !append_command(command, sizeof(command) / sizeof(command[0]), link_path) ||
      !append_command(command, sizeof(command) / sizeof(command[0]), L"\" \"") ||
      !append_command(command, sizeof(command) / sizeof(command[0]), target_path) ||
      !append_command(command, sizeof(command) / sizeof(command[0]),
                      L"\" >nul 2>nul"))
    return false;
  STARTUPINFOW startup;
  PROCESS_INFORMATION process;
  (void)memset(&startup, 0, sizeof(startup));
  (void)memset(&process, 0, sizeof(process));
  startup.cb = (DWORD)sizeof(startup);
  if (!CreateProcessW(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL,
                      NULL, &startup, &process))
    return false;
  const DWORD wait_status = WaitForSingleObject(process.hProcess, INFINITE);
  DWORD exit_code = 1u;
  const BOOL exit_read = GetExitCodeProcess(process.hProcess, &exit_code);
  const BOOL process_closed = CloseHandle(process.hProcess);
  const BOOL thread_closed = CloseHandle(process.hThread);
  if (wait_status != WAIT_OBJECT_0 || exit_read == FALSE ||
      process_closed == FALSE || thread_closed == FALSE || exit_code != 0u)
    return false;
  const DWORD attributes = GetFileAttributesW(link_path);
  return attributes != INVALID_FILE_ATTRIBUTES &&
         (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u &&
         (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0u;
}

static bool wide_to_utf8(const WCHAR *source, char *destination,
                         size_t capacity) {
  if (source == NULL || destination == NULL || capacity == 0u ||
      capacity > (size_t)INT_MAX)
    return false;
  const size_t source_length = wide_length(source);
  if (source_length > (size_t)INT_MAX) return false;
  const int written = WideCharToMultiByte(
      CP_UTF8, WC_ERR_INVALID_CHARS, source, (int)source_length, destination,
      (int)(capacity - 1u), NULL, NULL);
  if (written <= 0) return false;
  destination[(size_t)written] = '\0';
  return true;
}

static bool create_environment(windows_environment *environment) {
  if (environment == NULL) return false;
  (void)memset(environment, 0, sizeof(*environment));
  environment->base_handle = INVALID_HANDLE_VALUE;
  WCHAR temporary[TEST_WIDE_CAPACITY];
  const DWORD temporary_length =
      GetTempPathW((DWORD)TEST_WIDE_CAPACITY, temporary);
  if (temporary_length == 0u || temporary_length >= (DWORD)TEST_WIDE_CAPACITY ||
      GetTempFileNameW(temporary, L"wsp", 0u, environment->directory) == 0u ||
      !remove_file(environment->directory) ||
      !CreateDirectoryW(environment->directory, NULL))
    return false;
  if (!wide_join(environment->directory, L"root.w", environment->root,
                 TEST_WIDE_CAPACITY) ||
      !wide_join(environment->directory, L"caf\u00e9.w",
                 environment->unicode_root, TEST_WIDE_CAPACITY) ||
      !wide_join(environment->directory, L"nested", environment->nested,
                 TEST_WIDE_CAPACITY) ||
      !wide_join(environment->nested, L"child.w", environment->child,
                 TEST_WIDE_CAPACITY) ||
      !wide_join(environment->directory, L"empty.w", environment->empty,
                 TEST_WIDE_CAPACITY) ||
      !wide_join(environment->directory, L"alias.w", environment->alias,
                 TEST_WIDE_CAPACITY) ||
      !wide_join(environment->directory, L"directory.w",
                 environment->directory_source, TEST_WIDE_CAPACITY) ||
      !wide_join(environment->directory, L"junction_link",
                 environment->junction, TEST_WIDE_CAPACITY) ||
      !wide_join(environment->directory, L"final_junction.w",
                 environment->final_junction, TEST_WIDE_CAPACITY) ||
      !wide_join(environment->directory, L"root_link.w",
                 environment->root_symlink, TEST_WIDE_CAPACITY) ||
      !wide_join(environment->directory, L"nested_link",
                 environment->nested_symlink, TEST_WIDE_CAPACITY) ||
      !CreateDirectoryW(environment->nested, NULL) ||
      !CreateDirectoryW(environment->directory_source, NULL) ||
      !write_file(environment->root, (const uint8_t *)"root\n", 5u) ||
      !write_file(environment->unicode_root, (const uint8_t *)"utf8\n", 5u) ||
      !write_file(environment->child, (const uint8_t *)"child\n", 6u) ||
      !write_file(environment->empty, NULL, 0u) ||
      !CreateHardLinkW(environment->alias, environment->root, NULL))
    return false;
  if (!create_junction(environment->junction, environment->nested) ||
      !create_junction(environment->final_junction, environment->nested))
    return false;
  const BOOL root_link = CreateSymbolicLinkW(
      environment->root_symlink, environment->root, 0u);
  const DWORD root_link_error = root_link == FALSE ? GetLastError() : ERROR_SUCCESS;
  const BOOL nested_link = CreateSymbolicLinkW(
      environment->nested_symlink, environment->nested,
      SYMBOLIC_LINK_FLAG_DIRECTORY);
  const DWORD nested_link_error =
      nested_link == FALSE ? GetLastError() : ERROR_SUCCESS;
  if (root_link != FALSE && nested_link != FALSE) {
    environment->symlink_available = true;
  } else if ((root_link == FALSE && root_link_error != ERROR_ACCESS_DENIED &&
              root_link_error != ERROR_PRIVILEGE_NOT_HELD) ||
             (nested_link == FALSE && nested_link_error != ERROR_ACCESS_DENIED &&
              nested_link_error != ERROR_PRIVILEGE_NOT_HELD)) {
    return false;
  } else {
    (void)remove_file(environment->root_symlink);
    (void)remove_directory(environment->nested_symlink);
    environment->symlink_skipped = true;
  }
  environment->base_handle = CreateFileW(
      environment->directory,
      GENERIC_READ | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (environment->base_handle == INVALID_HANDLE_VALUE ||
      !w_seed_ephemeral_provider_windows_init(
          &environment->context, (uintptr_t)environment->base_handle) ||
      !w_seed_ephemeral_provider_windows_backend(&environment->context,
                                                 &environment->backend))
    return false;
  return true;
}

static void destroy_environment(windows_environment *environment) {
  if (environment == NULL) return;
  if (environment->base_handle != INVALID_HANDLE_VALUE) {
    (void)CloseHandle(environment->base_handle);
    environment->base_handle = INVALID_HANDLE_VALUE;
  }
  (void)remove_file(environment->root_symlink);
  (void)remove_directory(environment->nested_symlink);
  (void)remove_directory(environment->junction);
  (void)remove_directory(environment->final_junction);
  (void)remove_file(environment->alias);
  (void)remove_file(environment->root);
  (void)remove_file(environment->unicode_root);
  (void)remove_file(environment->empty);
  (void)remove_file(environment->child);
  (void)remove_directory(environment->directory_source);
  (void)remove_directory(environment->nested);
  (void)remove_directory(environment->directory);
  (void)memset(environment, 0, sizeof(*environment));
  environment->base_handle = INVALID_HANDLE_VALUE;
}

static bool reset_fixture(const windows_environment *environment) {
  if (environment == NULL) return false;
  return remove_file(environment->root) &&
         remove_file(environment->child) &&
         remove_file(environment->empty) &&
         remove_file(environment->alias) &&
         write_file(environment->root, (const uint8_t *)"root\n", 5u) &&
         write_file(environment->child, (const uint8_t *)"child\n", 6u) &&
         write_file(environment->empty, NULL, 0u) &&
         CreateHardLinkW(environment->alias, environment->root, NULL) != FALSE;
}

static bool slots_drained(const windows_environment *environment) {
  if (environment == NULL) return false;
  for (size_t index = 0u;
       index < W_SEED_EPHEMERAL_PROVIDER_WINDOWS_MAX_HANDLES; index += 1u) {
    if (environment->context.slots[index].used) return false;
  }
  return true;
}

static bool acquire_case(windows_environment *environment,
                         provider_case *test_case,
                         w_seed_ephemeral_provider_result *result) {
  if (environment == NULL || test_case == NULL || result == NULL) return false;
  if (test_case->input.backend.open_root == NULL)
    test_case->input.backend = environment->backend;
  DWORD before = 0u;
  DWORD after = 0u;
  const BOOL have_before = GetProcessHandleCount(GetCurrentProcess(), &before);
  const w_seed_ephemeral_provider_status status =
      w_seed_ephemeral_provider_acquire(&test_case->input, result);
  const BOOL have_after = GetProcessHandleCount(GetCurrentProcess(), &after);
  CHECK(status == result->status);
  CHECK(slots_drained(environment));
  CHECK(have_before == FALSE || have_after == FALSE || before == after);
  return true;
}

static bool expected_token_from_handle(HANDLE handle, char prefix,
                                       char destination[51]) {
  if (handle == NULL || handle == INVALID_HANDLE_VALUE || destination == NULL)
    return false;
  FILE_ID_INFO info;
  (void)memset(&info, 0, sizeof(info));
  if (!GetFileInformationByHandleEx(handle, FileIdInfo, &info,
                                    (DWORD)sizeof(info)))
    return false;
  static const char hex[] = "0123456789abcdef";
  destination[0] = prefix;
  for (size_t index = 0u; index < 16u; index += 1u) {
    const unsigned int shift = (unsigned int)((15u - index) * 4u);
    destination[1u + index] =
        hex[(size_t)((info.VolumeSerialNumber >> shift) & UINT64_C(0x0f))];
    destination[18u + (2u * index)] =
        hex[(size_t)((info.FileId.Identifier[index] >> 4u) & 0x0fu)];
    destination[19u + (2u * index)] =
        hex[(size_t)(info.FileId.Identifier[index] & 0x0fu)];
  }
  destination[17u] = '-';
  destination[50u] = '\0';
  return true;
}

static bool assert_token_sentinel(const char *buffer, size_t length) {
  if (buffer == NULL || length >= TEST_TOKEN_CAPACITY) return false;
  for (size_t index = length; index < TEST_TOKEN_CAPACITY; index += 1u)
    if ((unsigned char)buffer[index] != 0x5Au) return false;
  return true;
}

static bool assert_success(const windows_environment *environment,
                           const provider_case *test_case,
                           const WCHAR *root_path, bool has_child) {
  if (environment == NULL || test_case == NULL || root_path == NULL) return false;
  CHECK(text_equals_literal(test_case->facts[0].provider_id,
                            "windows-ntcreatefile-v1"));
  CHECK(test_case->facts[0].opened && test_case->facts[0].containment_inside);
  CHECK(test_case->sources[0].bytes.length == 5u);
  CHECK(memcmp(test_case->bytes[0], "root\n", 5u) == 0);
  CHECK(assert_token_sentinel(test_case->provider[0], 23u));
  CHECK(assert_token_sentinel(test_case->root_token[0], 50u));
  CHECK(assert_token_sentinel(test_case->owner[0], 50u));
  CHECK(assert_token_sentinel(test_case->canonical[0], 50u));
  char expected_root[51];
  char expected_owner[51];
  char expected_canonical[51];
  CHECK(expected_token_from_handle(environment->base_handle, 'r', expected_root));
  CHECK(expected_token_from_handle(environment->base_handle, 'o', expected_owner));
  HANDLE root_file = CreateFileW(
      root_path, GENERIC_READ | FILE_READ_ATTRIBUTES,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL, NULL);
  CHECK(root_file != INVALID_HANDLE_VALUE);
  CHECK(expected_token_from_handle(root_file, 'c', expected_canonical));
  CHECK(CloseHandle(root_file) != FALSE);
  CHECK(text_equals_literal(test_case->facts[0].root_token, expected_root));
  CHECK(text_equals_literal(test_case->facts[0].source_provider_owner_token,
                            expected_owner));
  CHECK(text_equals_literal(test_case->facts[0].canonical_token,
                            expected_canonical));
  if (!has_child) return true;
  CHECK(test_case->facts[1].opened && test_case->facts[1].containment_inside);
  CHECK(test_case->sources[1].bytes.length == 6u);
  CHECK(memcmp(test_case->bytes[1], "child\n", 6u) == 0);
  CHECK(text_equals_literal(test_case->facts[1].root_token, expected_root));
  CHECK(text_equals_literal(test_case->facts[1].source_provider_owner_token,
                            expected_owner));
  return true;
}

static bool expect_failure(windows_environment *environment,
                           provider_case *test_case,
                           w_seed_ephemeral_provider_status status,
                           w_seed_ephemeral_provider_failure failure,
                           w_seed_ephemeral_provider_phase phase) {
  output_snapshot snapshot;
  snapshot_outputs(test_case, &snapshot);
  w_seed_ephemeral_provider_result result;
  CHECK(acquire_case(environment, test_case, &result));
  CHECK(result.status == status && result.failure == failure &&
        result.phase == phase);
  CHECK(outputs_unchanged(test_case, &snapshot));
  return true;
}

static bool test_success_cases(windows_environment *environment) {
  provider_case test_case;
  CHECK(reset_fixture(environment));
  CHECK(initialize_provider_case(&test_case, 2u, 0u, "root.w"));
  w_seed_ephemeral_provider_result result;
  CHECK(acquire_case(environment, &test_case, &result));
  CHECK(result.status == W_SEED_EPHEMERAL_PROVIDER_OK);
  CHECK(assert_success(environment, &test_case, environment->root, true));
  char absolute_root[TEST_PATH_CAPACITY];
  CHECK(wide_to_utf8(environment->root, absolute_root, sizeof(absolute_root)));
  CHECK(initialize_provider_case(&test_case, 2u, 0u, absolute_root));
  CHECK(acquire_case(environment, &test_case, &result));
  CHECK(result.status == W_SEED_EPHEMERAL_PROVIDER_OK);
  CHECK(assert_success(environment, &test_case, environment->root, true));
  return true;
}

static bool test_valid_utf8_physical_root(windows_environment *environment) {
  provider_case test_case;
  char unicode_root[TEST_PATH_CAPACITY];
  static const uint8_t expected_bytes[] = {'u', 't', 'f', '8', '\n'};
  CHECK(reset_fixture(environment));
  CHECK(wide_to_utf8(L"caf\u00e9.w", unicode_root, sizeof(unicode_root)));
  CHECK(initialize_provider_case(&test_case, 1u, 0u, unicode_root));
  w_seed_ephemeral_provider_result result;
  CHECK(acquire_case(environment, &test_case, &result));
  CHECK(result.status == W_SEED_EPHEMERAL_PROVIDER_OK);
  CHECK(test_case.sources[0].bytes.length == sizeof(expected_bytes));
  CHECK(memcmp(test_case.bytes[0], expected_bytes, sizeof(expected_bytes)) ==
        0);
  CHECK(test_case.facts[0].opened && test_case.facts[0].containment_inside);
  CHECK(text_equals_literal(test_case.facts[0].provider_id,
                            "windows-ntcreatefile-v1"));
  CHECK(slots_drained(environment));
  return true;
}

static bool test_missing_zero_and_directory(windows_environment *environment) {
  provider_case test_case;
  CHECK(reset_fixture(environment));
  CHECK(initialize_provider_case(&test_case, 2u, 0u, "root.w"));
  CHECK(set_request_id(&test_case, 1u, "missing.w"));
  CHECK(expect_failure(environment, &test_case, W_SEED_EPHEMERAL_PROVIDER_IO,
                       W_SEED_EPHEMERAL_PROVIDER_FAILURE_SOURCE_MISSING,
                       W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_SOURCE));
  CHECK(initialize_provider_case(&test_case, 1u, 0u, "empty.w"));
  CHECK(set_request_id(&test_case, 0u, "empty.w"));
  test_case.requests[0].staging_bytes = NULL;
  test_case.requests[0].staging_capacity = 0u;
  test_case.requests[0].revalidation_bytes = NULL;
  test_case.requests[0].revalidation_capacity = 0u;
  test_case.requests[0].bytes = NULL;
  test_case.requests[0].byte_capacity = 0u;
  w_seed_ephemeral_provider_result result;
  CHECK(acquire_case(environment, &test_case, &result));
  CHECK(result.status == W_SEED_EPHEMERAL_PROVIDER_OK);
  CHECK(test_case.sources[0].bytes.data == NULL &&
        test_case.sources[0].bytes.length == 0u);
  CHECK(reset_fixture(environment));
  CHECK(initialize_provider_case(&test_case, 1u, 0u, "directory.w"));
  CHECK(expect_failure(environment, &test_case,
                       W_SEED_EPHEMERAL_PROVIDER_UNSUPPORTED,
                       W_SEED_EPHEMERAL_PROVIDER_FAILURE_BACKEND,
                       W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_ROOT));
  return true;
}

static bool initialize_direct_tokens(
    w_seed_ephemeral_provider_token_buffers *tokens,
    char provider[TEST_TOKEN_CAPACITY], char root_token[TEST_TOKEN_CAPACITY],
    char owner[TEST_TOKEN_CAPACITY], char canonical[TEST_TOKEN_CAPACITY]) {
  if (tokens == NULL || provider == NULL || root_token == NULL ||
      owner == NULL || canonical == NULL)
    return false;
  (void)memset(provider, 0x5A, TEST_TOKEN_CAPACITY);
  (void)memset(root_token, 0x5A, TEST_TOKEN_CAPACITY);
  (void)memset(owner, 0x5A, TEST_TOKEN_CAPACITY);
  (void)memset(canonical, 0x5A, TEST_TOKEN_CAPACITY);
  *tokens = (w_seed_ephemeral_provider_token_buffers){
      provider, TEST_TOKEN_CAPACITY, root_token, TEST_TOKEN_CAPACITY,
      owner, TEST_TOKEN_CAPACITY, canonical, TEST_TOKEN_CAPACITY};
  return true;
}

static bool expect_direct_root_status(
    windows_environment *environment, const uint8_t *bytes, size_t length,
    w_seed_ephemeral_provider_backend_status expected) {
  char provider[TEST_TOKEN_CAPACITY];
  char root_token[TEST_TOKEN_CAPACITY];
  char owner[TEST_TOKEN_CAPACITY];
  char canonical[TEST_TOKEN_CAPACITY];
  w_seed_ephemeral_provider_token_buffers tokens;
  CHECK(initialize_direct_tokens(&tokens, provider, root_token, owner, canonical));
  w_seed_ephemeral_provider_handle root = {(uintptr_t)0u};
  w_seed_ephemeral_provider_handle source = {(uintptr_t)0u};
  w_seed_ephemeral_provider_observation observation;
  const w_seed_ephemeral_provider_backend_status status =
      environment->backend.open_root(
          environment->backend.context, (w_seed_byte_view){bytes, length},
          &tokens, &root, &source, &observation);
  CHECK(status == expected);
  CHECK(root.value == (uintptr_t)0u && source.value == (uintptr_t)0u);
  CHECK(slots_drained(environment));
  return true;
}

static bool test_path_classification(windows_environment *environment) {
  static const uint8_t invalid_utf8[] = {0xC3u, 0x28u};
  static const char common_unc[] = "\\\\server\\share\\root.w";
  static const char namespace_path[] = "\\\\?\\C:\\root.w";
  static const char device_path[] = "\\\\.\\NUL";
  static const char nt_path[] = "\\??\\C:\\root.w";
  static const char bare_rooted[] = "\\root.w";
  static const char drive_relative[] = "C:root.w";
  static const char ads_path[] = "root.w:stream";
  static const char dot_path[] = "nested/./root.w";
  static const char trailing_path[] = "root.w.";
  CHECK(expect_direct_root_status(environment, invalid_utf8,
                                  sizeof(invalid_utf8),
                                  W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID));
  CHECK(expect_direct_root_status(
      environment, (const uint8_t *)common_unc, strlen(common_unc),
      W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED));
  CHECK(expect_direct_root_status(
      environment, (const uint8_t *)namespace_path, strlen(namespace_path),
      W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID));
  CHECK(expect_direct_root_status(environment, (const uint8_t *)device_path,
                                  strlen(device_path),
                                  W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID));
  CHECK(expect_direct_root_status(environment, (const uint8_t *)nt_path,
                                  strlen(nt_path),
                                  W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID));
  CHECK(expect_direct_root_status(environment, (const uint8_t *)bare_rooted,
                                  strlen(bare_rooted),
                                  W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID));
  CHECK(expect_direct_root_status(
      environment, (const uint8_t *)drive_relative, strlen(drive_relative),
      W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID));
  CHECK(expect_direct_root_status(environment, (const uint8_t *)ads_path,
                                  strlen(ads_path),
                                  W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID));
  CHECK(expect_direct_root_status(environment, (const uint8_t *)dot_path,
                                  strlen(dot_path),
                                  W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID));
  CHECK(expect_direct_root_status(
      environment, (const uint8_t *)trailing_path, strlen(trailing_path),
      W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID));
  return true;
}

static bool test_capacity_and_token_bytes(windows_environment *environment) {
  provider_case test_case;
  CHECK(reset_fixture(environment));
  CHECK(initialize_provider_case(&test_case, 1u, 0u, "root.w"));
  test_case.requests[0].tokens.provider_id_capacity = 22u;
  test_case.requests[0].revalidation_tokens.provider_id_capacity = 22u;
  output_snapshot snapshot;
  snapshot_outputs(&test_case, &snapshot);
  w_seed_ephemeral_provider_result result;
  CHECK(acquire_case(environment, &test_case, &result));
  CHECK(result.status == W_SEED_EPHEMERAL_PROVIDER_CAPACITY &&
        result.phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_VALIDATE &&
        result.capacity_field ==
            W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_PROVIDER_ID &&
        result.required_capacity == 23u);
  CHECK(outputs_unchanged(&test_case, &snapshot));
  CHECK(initialize_provider_case(&test_case, 1u, 0u, "root.w"));
  test_case.requests[0].staging_capacity = 1u;
  test_case.requests[0].staging_bytes = test_case.staging[0];
  snapshot_outputs(&test_case, &snapshot);
  CHECK(acquire_case(environment, &test_case, &result));
  CHECK(result.status == W_SEED_EPHEMERAL_PROVIDER_CAPACITY &&
        result.phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_READ &&
        result.required_capacity == 5u);
  CHECK(outputs_unchanged(&test_case, &snapshot));
  CHECK(initialize_provider_case(&test_case, 1u, 0u, "root.w"));
  CHECK(acquire_case(environment, &test_case, &result));
  CHECK(result.status == W_SEED_EPHEMERAL_PROVIDER_OK);
  CHECK(assert_success(environment, &test_case, environment->root, false));
  return true;
}

static bool test_alias_and_reparse(windows_environment *environment) {
  provider_case test_case;
  CHECK(reset_fixture(environment));
  CHECK(initialize_provider_case(&test_case, 2u, 0u, "root.w"));
  CHECK(set_request_id(&test_case, 1u, "alias.w"));
  CHECK(expect_failure(environment, &test_case,
                       W_SEED_EPHEMERAL_PROVIDER_INVALID,
                       W_SEED_EPHEMERAL_PROVIDER_FAILURE_CANONICAL_ALIAS,
                       W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_SOURCE));
  CHECK(initialize_provider_case(&test_case, 2u, 0u, "root.w"));
  CHECK(set_request_id(&test_case, 1u, "junction_link/child.w"));
  CHECK(expect_failure(environment, &test_case,
                       W_SEED_EPHEMERAL_PROVIDER_INVALID,
                       W_SEED_EPHEMERAL_PROVIDER_FAILURE_SYMLINK,
                       W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_SOURCE));
  CHECK(initialize_provider_case(&test_case, 2u, 0u, "root.w"));
  CHECK(set_request_id(&test_case, 1u, "final_junction.w"));
  CHECK(expect_failure(environment, &test_case,
                       W_SEED_EPHEMERAL_PROVIDER_INVALID,
                       W_SEED_EPHEMERAL_PROVIDER_FAILURE_SYMLINK,
                       W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_SOURCE));
  if (environment->symlink_available) {
    CHECK(initialize_provider_case(&test_case, 1u, 0u, "root_link.w"));
    CHECK(expect_failure(environment, &test_case,
                         W_SEED_EPHEMERAL_PROVIDER_INVALID,
                         W_SEED_EPHEMERAL_PROVIDER_FAILURE_SYMLINK,
                         W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_ROOT));
    CHECK(initialize_provider_case(&test_case, 2u, 0u, "root.w"));
    CHECK(set_request_id(&test_case, 1u, "nested_link/child.w"));
    CHECK(expect_failure(environment, &test_case,
                         W_SEED_EPHEMERAL_PROVIDER_INVALID,
                         W_SEED_EPHEMERAL_PROVIDER_FAILURE_SYMLINK,
                         W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_SOURCE));
  }
  return true;
}

static bool mutate_in_place(const WCHAR *path) {
  static const uint8_t changed[] = {'M', 'U', 'T', '!', '\n'};
  HANDLE file = CreateFileW(path, GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE |
                                FILE_SHARE_DELETE,
                            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0u;
  const BOOL success = WriteFile(file, changed, (DWORD)sizeof(changed),
                                 &written, NULL);
  const BOOL closed = CloseHandle(file);
  return success != FALSE && closed != FALSE && written == sizeof(changed);
}

static bool replace_file_atomically(const WCHAR *path) {
  static const uint8_t changed[] = {'N', 'E', 'W', '!', '\n'};
  WCHAR replacement[TEST_WIDE_CAPACITY];
  if (!wide_copy(replacement, TEST_WIDE_CAPACITY, path) ||
      !wide_append(replacement, TEST_WIDE_CAPACITY, L".replace") ||
      !write_file(replacement, changed, sizeof(changed)) ||
      DeleteFileW(path) == FALSE ||
      MoveFileExW(replacement, path, MOVEFILE_WRITE_THROUGH) == FALSE) {
    (void)remove_file(replacement);
    return false;
  }
  return true;
}

static w_seed_ephemeral_provider_backend_status proxy_open_root(
    void *context, w_seed_byte_view root_path,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_handle *root_handle,
    w_seed_ephemeral_provider_handle *root_source_handle,
    w_seed_ephemeral_provider_observation *observation) {
  adapter_proxy *proxy = context;
  return proxy->delegate.open_root(proxy->delegate.context, root_path, tokens,
                                   root_handle, root_source_handle, observation);
}

static w_seed_ephemeral_provider_backend_status proxy_open_source(
    void *context, w_seed_ephemeral_provider_handle root_handle,
    w_seed_frontend_text source_id,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_handle *source_handle,
    w_seed_ephemeral_provider_observation *observation) {
  adapter_proxy *proxy = context;
  return proxy->delegate.open_source(proxy->delegate.context, root_handle,
                                     source_id, tokens, source_handle,
                                     observation);
}

static w_seed_ephemeral_provider_backend_status proxy_read_source(
    void *context, w_seed_ephemeral_provider_handle source_handle,
    uint8_t *bytes, size_t capacity, size_t *written) {
  adapter_proxy *proxy = context;
  const w_seed_ephemeral_provider_backend_status status =
      proxy->delegate.read_source(proxy->delegate.context, source_handle, bytes,
                                  capacity, written);
  if (status == W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) {
    proxy->read_count += 1u;
    if (!proxy->changed && proxy->read_count == 1u) {
      proxy->changed = proxy->mode == PROXY_MUTATE
                           ? mutate_in_place(proxy->target_path)
                           : proxy->mode == PROXY_REPLACE
                                 ? replace_file_atomically(proxy->target_path)
                                 : DeleteFileW(proxy->target_path) != FALSE;
    }
  }
  return status;
}

static w_seed_ephemeral_provider_backend_status proxy_revalidate_source(
    void *context, w_seed_ephemeral_provider_handle root_handle,
    w_seed_ephemeral_provider_handle source_handle,
    w_seed_frontend_text source_id,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_observation *observation, uint8_t *bytes,
    size_t capacity, size_t *written) {
  adapter_proxy *proxy = context;
  return proxy->delegate.revalidate_source(
      proxy->delegate.context, root_handle, source_handle, source_id, tokens,
      observation, bytes, capacity, written);
}

static void proxy_close_source(void *context,
                               w_seed_ephemeral_provider_handle source_handle) {
  adapter_proxy *proxy = context;
  proxy->delegate.close_source(proxy->delegate.context, source_handle);
}

static void proxy_close_root(void *context,
                             w_seed_ephemeral_provider_handle root_handle) {
  adapter_proxy *proxy = context;
  proxy->delegate.close_root(proxy->delegate.context, root_handle);
}

static w_seed_ephemeral_provider_backend proxy_backend(adapter_proxy *proxy) {
  w_seed_ephemeral_provider_backend backend = proxy->delegate;
  backend.context = proxy;
  backend.open_root = proxy_open_root;
  backend.open_source = proxy_open_source;
  backend.read_source = proxy_read_source;
  backend.revalidate_source = proxy_revalidate_source;
  backend.close_source = proxy_close_source;
  backend.close_root = proxy_close_root;
  return backend;
}

static bool test_revalidation(windows_environment *environment) {
  provider_case test_case;
  for (proxy_mode mode = PROXY_MUTATE; mode <= PROXY_REMOVE;
       mode = (proxy_mode)(mode + 1)) {
    CHECK(reset_fixture(environment));
    CHECK(initialize_provider_case(&test_case, 1u, 0u, "root.w"));
    adapter_proxy proxy;
    (void)memset(&proxy, 0, sizeof(proxy));
    proxy.delegate = environment->backend;
    proxy.mode = mode;
    CHECK(wide_copy(proxy.target_path, TEST_WIDE_CAPACITY, environment->root));
    test_case.input.backend = proxy_backend(&proxy);
    output_snapshot snapshot;
    snapshot_outputs(&test_case, &snapshot);
    w_seed_ephemeral_provider_result result;
    CHECK(acquire_case(environment, &test_case, &result));
    if (mode == PROXY_REMOVE) {
      CHECK(result.status == W_SEED_EPHEMERAL_PROVIDER_IO);
    } else {
      CHECK(result.status == W_SEED_EPHEMERAL_PROVIDER_INVALID);
    }
    CHECK(result.failure == W_SEED_EPHEMERAL_PROVIDER_FAILURE_SNAPSHOT &&
          result.phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_REVALIDATE);
    CHECK(proxy.read_count >= 1u && proxy.changed);
    CHECK(outputs_unchanged(&test_case, &snapshot));
  }
  CHECK(reset_fixture(environment));
  return true;
}

static bool run_windows_tests(windows_environment *environment) {
  CHECK(environment->context.ntcreatefile_supported);
  CHECK(test_success_cases(environment));
  CHECK(test_valid_utf8_physical_root(environment));
  CHECK(test_missing_zero_and_directory(environment));
  CHECK(test_path_classification(environment));
  CHECK(test_capacity_and_token_bytes(environment));
  CHECK(test_alias_and_reparse(environment));
  CHECK(test_revalidation(environment));
  return true;
}

int main(void) {
  windows_environment environment;
  if (!create_environment(&environment)) {
    destroy_environment(&environment);
    (void)fprintf(stderr, "windows ephemeral environment setup failed\n");
    return 1;
  }
  const bool passed = run_windows_tests(&environment);
  const bool symlink_skipped = environment.symlink_skipped;
  destroy_environment(&environment);
  if (!passed) return 1;
  if (symlink_skipped)
    (void)printf("SKIP symlink=not-created-without-privilege\n");
  else
    (void)printf("RESULT symlink=created\n");
  (void)printf("RESULT provider-adapter-windows=pass\n");
  return 0;
}

#else

int main(void) {
  w_seed_ephemeral_provider_windows_context context;
  w_seed_ephemeral_provider_backend backend;
  if (!w_seed_ephemeral_provider_windows_init(&context, (uintptr_t)0u) ||
      !w_seed_ephemeral_provider_windows_backend(&context, &backend) ||
      backend.open_root == NULL ||
      backend.open_root(backend.context, (w_seed_byte_view){NULL, 0u}, NULL,
                        NULL, NULL, NULL) !=
          W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED)
    return 1;
  (void)printf("SKIP adapter-windows-real=non-windows-stub\n");
  (void)printf("RESULT provider-adapter-windows=pass\n");
  return 0;
}

#endif
