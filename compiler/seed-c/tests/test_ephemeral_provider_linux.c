#if defined(__linux__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#endif

#include "w_seed_ephemeral_provider_linux.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(__linux__)

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#endif

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      (void)fprintf(stderr, "ephemeral adapter check failed: %s (%s:%d)\n", \
                    #condition, __FILE__, __LINE__);                            \
      return false;                                                             \
    }                                                                           \
  } while (0)

enum {
  TEST_REQUEST_CAPACITY = 2,
  TEST_BYTE_CAPACITY = 64,
  TEST_TOKEN_CAPACITY = 64,
  TEST_PATH_CAPACITY = W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES + 1u,
};

#if defined(__linux__)

static const uint8_t expected_empty_digest[
    W_SEED_EPHEMERAL_GRAPH_SHA256_BYTES] = {
    0xBCu, 0x76u, 0x15u, 0x60u, 0x67u, 0x88u, 0xEFu, 0x87u,
    0x80u, 0x3Cu, 0x33u, 0x4Au, 0xA1u, 0x05u, 0xB2u, 0xB7u,
    0x88u, 0xC4u, 0xCBu, 0x25u, 0xDAu, 0x5Du, 0x81u, 0xB0u,
    0xFEu, 0x3Fu, 0x27u, 0xE8u, 0x84u, 0x37u, 0x07u, 0x76u};

static const uint8_t expected_root_digest[
    W_SEED_EPHEMERAL_GRAPH_SHA256_BYTES] = {
    0x95u, 0x0Du, 0x59u, 0x38u, 0xC4u, 0xC0u, 0x6Eu, 0xE8u,
    0x17u, 0xF1u, 0xA6u, 0x8Cu, 0xA5u, 0xB2u, 0x34u, 0xB0u,
    0x9Au, 0x6Cu, 0x35u, 0x6Eu, 0x60u, 0x58u, 0x15u, 0x2Eu,
    0xE4u, 0xFFu, 0x3Fu, 0x91u, 0xDFu, 0xCFu, 0x21u, 0xF4u};

static const uint8_t expected_child_digest[
    W_SEED_EPHEMERAL_GRAPH_SHA256_BYTES] = {
    0xE9u, 0x00u, 0x3Au, 0x69u, 0xE1u, 0x0Eu, 0xB4u, 0x7Au,
    0x94u, 0x3Eu, 0xC4u, 0x51u, 0xFBu, 0xA1u, 0x5Cu, 0x0Au,
    0xDEu, 0x31u, 0x9Eu, 0xBEu, 0x51u, 0xEAu, 0xE1u, 0x2Du,
    0xBDu, 0xB5u, 0xC8u, 0x58u, 0x28u, 0x34u, 0x96u, 0x62u};

#endif

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

static bool copy_c_string(char *destination, size_t capacity,
                          const char *source) {
  if (destination == NULL || source == NULL) return false;
  const size_t length = strlen(source);
  if (length >= capacity) return false;
  if (length != 0u) (void)memcpy(destination, source, length);
  destination[length] = '\0';
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
      !copy_c_string(test_case->root_path, sizeof(test_case->root_path),
                     root_path))
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
    (void)memset(test_case->provider[index], 0x31, TEST_TOKEN_CAPACITY);
    (void)memset(test_case->root_token[index], 0x32, TEST_TOKEN_CAPACITY);
    (void)memset(test_case->owner[index], 0x33, TEST_TOKEN_CAPACITY);
    (void)memset(test_case->canonical[index], 0x34, TEST_TOKEN_CAPACITY);
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

#if defined(__linux__)

static bool text_equals_literal(w_seed_frontend_text text, const char *literal) {
  if (literal == NULL) return false;
  const size_t length = strlen(literal);
  return text.length == length &&
         (length == 0u || memcmp(text.data, literal, length) == 0);
}

#endif

#if defined(__linux__)

typedef struct {
  char directory[TEST_PATH_CAPACITY];
  int base_dir_fd;
  w_seed_ephemeral_provider_linux_context context;
  w_seed_ephemeral_provider_backend backend;
} linux_environment;

static bool path_join(const char *directory, const char *name,
                      char *destination, size_t capacity) {
  if (directory == NULL || name == NULL || destination == NULL) return false;
  const int written = snprintf(destination, capacity, "%s/%s", directory,
                               name);
  return written >= 0 && (size_t)written < capacity;
}

static bool write_file(const char *path, const uint8_t *bytes, size_t length) {
  if (path == NULL || (length != 0u && bytes == NULL)) return false;
  const int file = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
                        (mode_t)0600);
  if (file < 0) return false;
  size_t offset = 0u;
  while (offset < length) {
    const ssize_t written = write(file, bytes + offset, length - offset);
    if (written < (ssize_t)0) {
      if (errno == EINTR) continue;
      (void)close(file);
      return false;
    }
    if (written == (ssize_t)0) {
      (void)close(file);
      return false;
    }
    offset += (size_t)written;
  }
  if (close(file) != 0) return false;
  return true;
}

static void unlink_if_present(const char *path) {
  if (path != NULL) (void)unlink(path);
}

static bool reset_files(const linux_environment *environment) {
  static const uint8_t root_bytes[] = {'r', 'o', 'o', 't', '\n'};
  static const uint8_t child_bytes[] = {'c', 'h', 'i', 'l', 'd', '\n'};
  char root[TEST_PATH_CAPACITY];
  char alias[TEST_PATH_CAPACITY];
  char child[TEST_PATH_CAPACITY];
  char nested_root[TEST_PATH_CAPACITY];
  if (environment == NULL ||
      !path_join(environment->directory, "root.w", root, sizeof(root)) ||
      !path_join(environment->directory, "alias.w", alias, sizeof(alias)) ||
      !path_join(environment->directory, "nested/child.w", child,
                 sizeof(child)) ||
      !path_join(environment->directory, "nested/root.w", nested_root,
                 sizeof(nested_root)))
    return false;
  unlink_if_present(root);
  unlink_if_present(alias);
  if (!write_file(root, root_bytes, sizeof(root_bytes)) ||
      link(root, alias) != 0 ||
      !write_file(child, child_bytes, sizeof(child_bytes)) ||
      !write_file(nested_root, root_bytes, sizeof(root_bytes)))
    return false;
  return true;
}

static bool create_environment(linux_environment *environment) {
  if (environment == NULL) return false;
  (void)memset(environment, 0, sizeof(*environment));
  environment->base_dir_fd = -1;
  char template_path[] = "/tmp/w-seed-ephemeral-provider-XXXXXX";
  char *directory = mkdtemp(template_path);
  if (directory == NULL ||
      !copy_c_string(environment->directory, sizeof(environment->directory),
                     directory))
    return false;
  char nested[TEST_PATH_CAPACITY];
  char dir_source[TEST_PATH_CAPACITY];
  char fifo_source[TEST_PATH_CAPACITY];
  char root_link[TEST_PATH_CAPACITY];
  char nested_link[TEST_PATH_CAPACITY];
  char child_link[TEST_PATH_CAPACITY];
  char empty_source[TEST_PATH_CAPACITY];
  if (!path_join(environment->directory, "nested", nested, sizeof(nested)) ||
      !path_join(environment->directory, "dir.w", dir_source,
                 sizeof(dir_source)) ||
      !path_join(environment->directory, "fifo.w", fifo_source,
                 sizeof(fifo_source)) ||
      !path_join(environment->directory, "root-link.w", root_link,
                 sizeof(root_link)) ||
      !path_join(environment->directory, "nested_link", nested_link,
                 sizeof(nested_link)) ||
      !path_join(environment->directory, "child_link.w", child_link,
                 sizeof(child_link)) ||
      !path_join(environment->directory, "empty.w", empty_source,
                 sizeof(empty_source)) ||
      mkdir(nested, (mode_t)0700) != 0 ||
      mkdir(dir_source, (mode_t)0700) != 0 ||
      mkfifo(fifo_source, (mode_t)0600) != 0 ||
      !write_file(empty_source, NULL, 0u) || !reset_files(environment) ||
      symlink("root.w", root_link) != 0 ||
      symlink("nested", nested_link) != 0 ||
      symlink("nested/child.w", child_link) != 0)
    return false;
  environment->base_dir_fd =
      open(environment->directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (environment->base_dir_fd < 0 ||
      !w_seed_ephemeral_provider_linux_init(&environment->context,
                                            environment->base_dir_fd) ||
      !w_seed_ephemeral_provider_linux_backend(&environment->context,
                                               &environment->backend))
    return false;
  return true;
}

static void destroy_environment(linux_environment *environment) {
  if (environment == NULL) return;
  if (environment->base_dir_fd >= 0) {
    (void)close(environment->base_dir_fd);
    environment->base_dir_fd = -1;
  }
  const char *files[] = {"root.w",       "alias.w",      "empty.w",
                         "fifo.w",      "root-link.w",  "child_link.w",
                         "nested_link", "nested/child.w", "nested/root.w"};
  for (size_t index = 0u; index < sizeof(files) / sizeof(files[0]);
       index += 1u) {
    char path[TEST_PATH_CAPACITY];
    if (path_join(environment->directory, files[index], path, sizeof(path)))
      unlink_if_present(path);
  }
  char dir_source[TEST_PATH_CAPACITY];
  char nested[TEST_PATH_CAPACITY];
  if (path_join(environment->directory, "dir.w", dir_source,
                sizeof(dir_source)))
    (void)rmdir(dir_source);
  if (path_join(environment->directory, "nested", nested, sizeof(nested)))
    (void)rmdir(nested);
  if (environment->directory[0] != '\0') (void)rmdir(environment->directory);
  (void)memset(environment, 0, sizeof(*environment));
  environment->base_dir_fd = -1;
}

static bool slots_drained(const linux_environment *environment) {
  if (environment == NULL) return false;
  for (size_t index = 0u;
       index < W_SEED_EPHEMERAL_PROVIDER_LINUX_MAX_HANDLES; index += 1u) {
    if (environment->context.slots[index].used) return false;
  }
  return true;
}

static int process_fd_count(void) {
  DIR *directory = opendir("/proc/self/fd");
  if (directory == NULL) return -1;
  int count = 0;
  struct dirent *entry = NULL;
  while ((entry = readdir(directory)) != NULL) {
    if (strcmp(entry->d_name, ".") != 0 &&
        strcmp(entry->d_name, "..") != 0)
      count += 1;
  }
  if (closedir(directory) != 0) return -1;
  return count;
}

static bool acquire_linux_case(linux_environment *environment,
                               provider_case *test_case,
                               w_seed_ephemeral_provider_result *result) {
  if (environment == NULL || test_case == NULL || result == NULL) return false;
  if (test_case->input.backend.open_root == NULL)
    test_case->input.backend = environment->backend;
  const int before_fds = process_fd_count();
  const w_seed_ephemeral_provider_status status =
      w_seed_ephemeral_provider_acquire(&test_case->input, result);
  const int after_fds = process_fd_count();
  CHECK(slots_drained(environment));
  CHECK(before_fds < 0 || after_fds < 0 || before_fds == after_fds);
  CHECK(status == result->status);
  return true;
}

static bool stat_token(const char *path, char prefix, char token[35]) {
  if (path == NULL || token == NULL) return false;
  struct stat stat_buffer;
  if (stat(path, &stat_buffer) != 0) return false;
  const uintmax_t raw_device = (uintmax_t)stat_buffer.st_dev;
  const uintmax_t raw_inode = (uintmax_t)stat_buffer.st_ino;
  if (raw_device > (uintmax_t)UINT64_MAX ||
      raw_inode > (uintmax_t)UINT64_MAX)
    return false;
  const int written = snprintf(token, 35u, "%c%016" PRIx64 "-%016" PRIx64,
                               prefix, (uint64_t)raw_device,
                               (uint64_t)raw_inode);
  return written == 34;
}

static bool assert_success(const linux_environment *environment,
                           const provider_case *test_case,
                           size_t root_index, bool has_child) {
  static const char provider[] = "linux-openat2-v1";
  if (environment == NULL || test_case == NULL) return false;
  const size_t root_length = 5u;
  const size_t child_length = 6u;
  CHECK(test_case->sources[root_index].bytes.length == root_length);
  CHECK(memcmp(test_case->bytes[root_index], "root\n", root_length) == 0);
  CHECK(test_case->facts[root_index].snapshot_before_byte_count ==
        root_length);
  CHECK(test_case->facts[root_index].snapshot_after_byte_count == root_length);
  CHECK(memcmp(test_case->facts[root_index].snapshot_before_digest,
               expected_root_digest, sizeof(expected_root_digest)) == 0);
  CHECK(memcmp(test_case->facts[root_index].snapshot_after_digest,
               expected_root_digest, sizeof(expected_root_digest)) == 0);
  CHECK(text_equals_literal(test_case->facts[root_index].provider_id,
                            provider));
  char root_path[TEST_PATH_CAPACITY];
  char root_token[35];
  char owner_token[35];
  char canonical_root[35];
  CHECK(copy_c_string(root_path, sizeof(root_path), environment->directory));
  CHECK(stat_token(root_path, 'r', root_token));
  CHECK(stat_token(root_path, 'o', owner_token));
  CHECK(path_join(environment->directory, "root.w", root_path,
                  sizeof(root_path)));
  CHECK(stat_token(root_path, 'c', canonical_root));
  CHECK(text_equals_literal(test_case->facts[root_index].root_token,
                            root_token));
  CHECK(text_equals_literal(
      test_case->facts[root_index].source_provider_owner_token, owner_token));
  CHECK(text_equals_literal(test_case->facts[root_index].canonical_token,
                            canonical_root));
  CHECK(test_case->facts[root_index].opened &&
        test_case->facts[root_index].containment_inside);
  if (!has_child) return true;
  const size_t child_index = 1u - root_index;
  CHECK(test_case->sources[child_index].bytes.length == child_length);
  CHECK(memcmp(test_case->bytes[child_index], "child\n", child_length) == 0);
  CHECK(test_case->facts[child_index].snapshot_before_byte_count ==
        child_length);
  CHECK(test_case->facts[child_index].snapshot_after_byte_count == child_length);
  CHECK(memcmp(test_case->facts[child_index].snapshot_before_digest,
               expected_child_digest, sizeof(expected_child_digest)) == 0);
  CHECK(memcmp(test_case->facts[child_index].snapshot_after_digest,
               expected_child_digest, sizeof(expected_child_digest)) == 0);
  CHECK(text_equals_literal(test_case->facts[child_index].provider_id,
                            provider));
  char child_path[TEST_PATH_CAPACITY];
  char canonical_child[35];
  CHECK(path_join(environment->directory, "nested/child.w", child_path,
                  sizeof(child_path)));
  CHECK(stat_token(child_path, 'c', canonical_child));
  CHECK(text_equals_literal(test_case->facts[child_index].root_token,
                            root_token));
  CHECK(text_equals_literal(
      test_case->facts[child_index].source_provider_owner_token, owner_token));
  CHECK(text_equals_literal(test_case->facts[child_index].canonical_token,
                            canonical_child));
  CHECK(test_case->facts[child_index].opened &&
        test_case->facts[child_index].containment_inside);
  return true;
}

static bool expect_unchanged_failure(linux_environment *environment,
                                     provider_case *test_case,
                                     w_seed_ephemeral_provider_status status,
                                     w_seed_ephemeral_provider_failure failure,
                                     w_seed_ephemeral_provider_phase phase) {
  output_snapshot snapshot;
  snapshot_outputs(test_case, &snapshot);
  w_seed_ephemeral_provider_result result;
  CHECK(acquire_linux_case(environment, test_case, &result));
  CHECK(result.status == status && result.failure == failure &&
        result.phase == phase);
  CHECK(outputs_unchanged(test_case, &snapshot));
  return true;
}

static bool test_success_cases(linux_environment *environment) {
  provider_case test_case;
  CHECK(reset_files(environment));
  CHECK(initialize_provider_case(&test_case, 2u, 1u, "root.w"));
  w_seed_ephemeral_provider_result result;
  CHECK(acquire_linux_case(environment, &test_case, &result));
  CHECK(result.status == W_SEED_EPHEMERAL_PROVIDER_OK);
  CHECK(assert_success(environment, &test_case, 1u, true));

  CHECK(reset_files(environment));
  char absolute_root[TEST_PATH_CAPACITY];
  CHECK(path_join(environment->directory, "root.w", absolute_root,
                  sizeof(absolute_root)));
  CHECK(initialize_provider_case(&test_case, 2u, 1u, absolute_root));
  CHECK(acquire_linux_case(environment, &test_case, &result));
  CHECK(result.status == W_SEED_EPHEMERAL_PROVIDER_OK);
  CHECK(assert_success(environment, &test_case, 1u, true));
  return true;
}

static bool test_capacity_and_limits(linux_environment *environment) {
  provider_case test_case;
  w_seed_ephemeral_provider_result result;
  CHECK(reset_files(environment));
  CHECK(initialize_provider_case(&test_case, 2u, 1u, "root.w"));
  test_case.requests[1].staging_capacity = 5u;
  test_case.requests[1].revalidation_capacity = 5u;
  test_case.requests[1].byte_capacity = 5u;
  test_case.requests[0].staging_capacity = 6u;
  test_case.requests[0].revalidation_capacity = 6u;
  test_case.requests[0].byte_capacity = 6u;
  CHECK(acquire_linux_case(environment, &test_case, &result));
  CHECK(result.status == W_SEED_EPHEMERAL_PROVIDER_OK);
  CHECK(assert_success(environment, &test_case, 1u, true));

  CHECK(reset_files(environment));
  CHECK(initialize_provider_case(&test_case, 2u, 1u, "root.w"));
  test_case.requests[0].staging_capacity = 5u;
  output_snapshot snapshot;
  snapshot_outputs(&test_case, &snapshot);
  CHECK(acquire_linux_case(environment, &test_case, &result));
  CHECK(result.status == W_SEED_EPHEMERAL_PROVIDER_CAPACITY &&
        result.failure == W_SEED_EPHEMERAL_PROVIDER_FAILURE_LIMIT &&
        result.phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_READ &&
        result.capacity_field ==
            W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES &&
        result.required_capacity == 6u);
  CHECK(outputs_unchanged(&test_case, &snapshot));

  CHECK(reset_files(environment));
  CHECK(initialize_provider_case(&test_case, 2u, 1u, "root.w"));
  test_case.requests[0].revalidation_capacity = 5u;
  snapshot_outputs(&test_case, &snapshot);
  CHECK(acquire_linux_case(environment, &test_case, &result));
  CHECK(result.status == W_SEED_EPHEMERAL_PROVIDER_CAPACITY &&
        result.capacity_field ==
            W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_BYTES &&
        result.required_capacity == 6u);
  CHECK(outputs_unchanged(&test_case, &snapshot));

  CHECK(reset_files(environment));
  CHECK(initialize_provider_case(&test_case, 2u, 1u, "root.w"));
  test_case.requests[0].byte_capacity = 5u;
  snapshot_outputs(&test_case, &snapshot);
  CHECK(acquire_linux_case(environment, &test_case, &result));
  CHECK(result.status == W_SEED_EPHEMERAL_PROVIDER_CAPACITY &&
        result.capacity_field ==
            W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_OUTPUT_BYTES &&
        result.required_capacity == 6u);
  CHECK(outputs_unchanged(&test_case, &snapshot));

  CHECK(reset_files(environment));
  CHECK(initialize_provider_case(&test_case, 2u, 1u, "root.w"));
  test_case.input.limits.max_total_source_bytes = 10u;
  snapshot_outputs(&test_case, &snapshot);
  CHECK(acquire_linux_case(environment, &test_case, &result));
  CHECK(result.status == W_SEED_EPHEMERAL_PROVIDER_CAPACITY &&
        result.capacity_field ==
            W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_AGGREGATE_SOURCE_BYTES &&
        result.required_capacity == 11u);
  CHECK(outputs_unchanged(&test_case, &snapshot));
  return true;
}

static bool test_special_files_and_symlinks(linux_environment *environment) {
  provider_case test_case;
  CHECK(initialize_provider_case(&test_case, 1u, 0u, "missing.w"));
  CHECK(expect_unchanged_failure(
      environment, &test_case, W_SEED_EPHEMERAL_PROVIDER_IO,
      W_SEED_EPHEMERAL_PROVIDER_FAILURE_ROOT,
      W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_ROOT));

  CHECK(initialize_provider_case(&test_case, 1u, 0u, "dir.w"));
  CHECK(expect_unchanged_failure(
      environment, &test_case, W_SEED_EPHEMERAL_PROVIDER_UNSUPPORTED,
      W_SEED_EPHEMERAL_PROVIDER_FAILURE_BACKEND,
      W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_ROOT));

  CHECK(initialize_provider_case(&test_case, 1u, 0u, "fifo.w"));
  CHECK(expect_unchanged_failure(
      environment, &test_case, W_SEED_EPHEMERAL_PROVIDER_UNSUPPORTED,
      W_SEED_EPHEMERAL_PROVIDER_FAILURE_BACKEND,
      W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_ROOT));

  CHECK(initialize_provider_case(&test_case, 1u, 0u, "root-link.w"));
  CHECK(expect_unchanged_failure(
      environment, &test_case, W_SEED_EPHEMERAL_PROVIDER_INVALID,
      W_SEED_EPHEMERAL_PROVIDER_FAILURE_SYMLINK,
      W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_ROOT));

  CHECK(initialize_provider_case(&test_case, 1u, 0u, "nested_link/root.w"));
  CHECK(expect_unchanged_failure(
      environment, &test_case, W_SEED_EPHEMERAL_PROVIDER_INVALID,
      W_SEED_EPHEMERAL_PROVIDER_FAILURE_SYMLINK,
      W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_ROOT));

  CHECK(reset_files(environment));
  CHECK(initialize_provider_case(&test_case, 2u, 0u, "root.w"));
  CHECK(set_request_id(&test_case, 1u, "child_link.w"));
  CHECK(expect_unchanged_failure(
      environment, &test_case, W_SEED_EPHEMERAL_PROVIDER_INVALID,
      W_SEED_EPHEMERAL_PROVIDER_FAILURE_SYMLINK,
      W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_SOURCE));

  CHECK(initialize_provider_case(&test_case, 2u, 0u, "root.w"));
  CHECK(set_request_id(&test_case, 1u, "nested_link/child.w"));
  CHECK(expect_unchanged_failure(
      environment, &test_case, W_SEED_EPHEMERAL_PROVIDER_INVALID,
      W_SEED_EPHEMERAL_PROVIDER_FAILURE_SYMLINK,
      W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_SOURCE));
  return true;
}

static bool test_alias_and_zero_byte(linux_environment *environment) {
  provider_case test_case;
  CHECK(reset_files(environment));
  CHECK(initialize_provider_case(&test_case, 2u, 0u, "root.w"));
  CHECK(set_request_id(&test_case, 1u, "alias.w"));
  CHECK(expect_unchanged_failure(
      environment, &test_case, W_SEED_EPHEMERAL_PROVIDER_INVALID,
      W_SEED_EPHEMERAL_PROVIDER_FAILURE_CANONICAL_ALIAS,
      W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_SOURCE));

  CHECK(initialize_provider_case(&test_case, 1u, 0u, "empty.w"));
  test_case.requests[0].staging_bytes = NULL;
  test_case.requests[0].staging_capacity = 0u;
  test_case.requests[0].revalidation_bytes = NULL;
  test_case.requests[0].revalidation_capacity = 0u;
  test_case.requests[0].bytes = NULL;
  test_case.requests[0].byte_capacity = 0u;
  w_seed_ephemeral_provider_result result;
  CHECK(acquire_linux_case(environment, &test_case, &result));
  CHECK(result.status == W_SEED_EPHEMERAL_PROVIDER_OK);
  CHECK(test_case.sources[0].bytes.data == NULL &&
        test_case.sources[0].bytes.length == 0u);
  CHECK(test_case.facts[0].snapshot_before_byte_count == 0u &&
        test_case.facts[0].snapshot_after_byte_count == 0u);
  CHECK(memcmp(test_case.facts[0].snapshot_before_digest, expected_empty_digest,
               sizeof(expected_empty_digest)) == 0);
  CHECK(memcmp(test_case.facts[0].snapshot_after_digest, expected_empty_digest,
               sizeof(expected_empty_digest)) == 0);
  return true;
}

typedef enum {
  PROXY_MUTATE = 0,
  PROXY_REPLACE,
} proxy_mode;

typedef struct {
  w_seed_ephemeral_provider_backend delegate;
  char target_path[TEST_PATH_CAPACITY];
  proxy_mode mode;
  size_t read_count;
  bool changed;
} adapter_proxy;

static bool mutate_in_place(const char *path) {
  static const uint8_t changed[] = {'M', 'U', 'T', '!', '\n'};
  if (path == NULL) return false;
  const int file = open(path, O_WRONLY | O_CLOEXEC);
  if (file < 0) return false;
  size_t offset = 0u;
  while (offset < sizeof(changed)) {
    const ssize_t written = write(file, changed + offset,
                                  sizeof(changed) - offset);
    if (written < (ssize_t)0) {
      if (errno == EINTR) continue;
      (void)close(file);
      return false;
    }
    if (written == (ssize_t)0) {
      (void)close(file);
      return false;
    }
    offset += (size_t)written;
  }
  return close(file) == 0;
}

static bool replace_file_atomically(const char *path) {
  static const uint8_t changed[] = {'N', 'E', 'W', '!', '\n'};
  if (path == NULL) return false;
  char replacement[TEST_PATH_CAPACITY];
  const int written = snprintf(replacement, sizeof(replacement), "%s.replace",
                               path);
  if (written < 0 || (size_t)written >= sizeof(replacement) ||
      !write_file(replacement, changed, sizeof(changed)))
    return false;
  if (rename(replacement, path) != 0) {
    unlink_if_present(replacement);
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
                           : replace_file_atomically(proxy->target_path);
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

static bool test_revalidation_mutation_and_replacement(
    linux_environment *environment) {
  provider_case test_case;
  char root_path[TEST_PATH_CAPACITY];
  CHECK(path_join(environment->directory, "root.w", root_path,
                  sizeof(root_path)));
  for (proxy_mode mode = PROXY_MUTATE; mode <= PROXY_REPLACE;
       mode = (proxy_mode)(mode + 1)) {
    CHECK(reset_files(environment));
    CHECK(initialize_provider_case(&test_case, 2u, 0u, "root.w"));
    adapter_proxy proxy = {environment->backend, {0}, mode, 0u, false};
    CHECK(copy_c_string(proxy.target_path, sizeof(proxy.target_path),
                        root_path));
    test_case.input.backend = proxy_backend(&proxy);
    output_snapshot snapshot;
    snapshot_outputs(&test_case, &snapshot);
    w_seed_ephemeral_provider_result result;
    CHECK(acquire_linux_case(environment, &test_case, &result));
    CHECK(result.status == W_SEED_EPHEMERAL_PROVIDER_INVALID &&
          result.failure == W_SEED_EPHEMERAL_PROVIDER_FAILURE_SNAPSHOT &&
          result.phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_REVALIDATE);
    CHECK(proxy.read_count >= 1u && proxy.changed);
    CHECK(outputs_unchanged(&test_case, &snapshot));
  }
  CHECK(reset_files(environment));
  return true;
}

static bool run_linux_tests(linux_environment *environment) {
  if (!environment->context.openat2_supported) {
    (void)printf("SKIP adapter-linux-openat2=unsupported\n");
    (void)printf("SKIP cross-mount=not-created-without-privilege\n");
    return true;
  }
  CHECK(test_success_cases(environment));
  CHECK(test_capacity_and_limits(environment));
  CHECK(test_special_files_and_symlinks(environment));
  CHECK(test_alias_and_zero_byte(environment));
  CHECK(test_revalidation_mutation_and_replacement(environment));
  (void)printf("SKIP cross-mount=not-created-without-privilege\n");
  return true;
}

#else

static bool test_non_linux_stub(void) {
  provider_case test_case;
  CHECK(initialize_provider_case(&test_case, 1u, 0u, "root.w"));
  w_seed_ephemeral_provider_linux_context context;
  w_seed_ephemeral_provider_backend backend;
  CHECK(w_seed_ephemeral_provider_linux_init(&context, -1));
  CHECK(w_seed_ephemeral_provider_linux_backend(&context, &backend));
  test_case.input.backend = backend;
  output_snapshot snapshot;
  snapshot_outputs(&test_case, &snapshot);
  w_seed_ephemeral_provider_result result;
  CHECK(w_seed_ephemeral_provider_acquire(&test_case.input, &result) ==
        W_SEED_EPHEMERAL_PROVIDER_UNSUPPORTED);
  CHECK(result.phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_ROOT &&
        result.backend_status ==
            W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED);
  CHECK(outputs_unchanged(&test_case, &snapshot));
  for (size_t index = 0u;
       index < W_SEED_EPHEMERAL_PROVIDER_LINUX_MAX_HANDLES; index += 1u)
    CHECK(!context.slots[index].used);

  CHECK(initialize_provider_case(&test_case, 1u, 0u, "root.w"));
  test_case.requests[0].tokens.provider_id_capacity = 15u;
  test_case.requests[0].revalidation_tokens.provider_id_capacity = 15u;
  test_case.input.backend = backend;
  snapshot_outputs(&test_case, &snapshot);
  CHECK(w_seed_ephemeral_provider_acquire(&test_case.input, &result) ==
        W_SEED_EPHEMERAL_PROVIDER_CAPACITY);
  CHECK(result.phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_VALIDATE &&
        result.capacity_field ==
            W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_PROVIDER_ID &&
        result.required_capacity == 16u);
  CHECK(outputs_unchanged(&test_case, &snapshot));
  for (size_t index = 0u;
       index < W_SEED_EPHEMERAL_PROVIDER_LINUX_MAX_HANDLES; index += 1u)
    CHECK(!context.slots[index].used);
  (void)printf("SKIP adapter-linux-real=non-linux-stub\n");
  return true;
}

#endif

int main(void) {
#if defined(__linux__)
  linux_environment environment;
  if (!create_environment(&environment)) {
    destroy_environment(&environment);
    (void)fprintf(stderr, "ephemeral adapter environment setup failed\n");
    return 1;
  }
  const bool passed = run_linux_tests(&environment);
  destroy_environment(&environment);
  if (!passed) return 1;
  (void)printf("RESULT provider-adapter=pass\n");
  return 0;
#else
  if (!test_non_linux_stub()) return 1;
  (void)printf("RESULT provider-adapter=pass\n");
  return 0;
#endif
}
