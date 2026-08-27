#if defined(__linux__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#endif

#include "w_seed_ephemeral_driver.h"
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
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#endif

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      (void)fprintf(stderr, "ephemeral driver adapter check failed: %s (%s:%d)\n", \
                    #condition, __FILE__, __LINE__);                           \
      return false;                                                             \
    }                                                                           \
  } while (0)

enum {
  TEST_SOURCES = 8,
  TEST_BYTES = 4096,
  TEST_TOTAL_BYTES = 16384,
  TEST_PATH = 512,
  TEST_TOKEN = 128,
  TEST_NODES = 1024,
  TEST_LEX_FRAMES = 256,
  TEST_TOKENS = 2048,
  TEST_PARSE_FRAMES = 512,
  TEST_ISSUES = 128,
  TEST_ORIGINS = 64,
  TEST_GRAPH_EDGES = 16,
};

typedef struct {
  char source_ids[TEST_SOURCES][TEST_PATH];
  char module_ids[TEST_SOURCES][TEST_PATH];
  w_seed_cst_node nodes[TEST_SOURCES][TEST_NODES];
  uint8_t staging[TEST_SOURCES][TEST_BYTES];
  uint8_t revalidation[TEST_SOURCES][TEST_BYTES];
  uint8_t bytes[TEST_SOURCES][TEST_BYTES];
  char provider[TEST_SOURCES][TEST_TOKEN];
  char root_token[TEST_SOURCES][TEST_TOKEN];
  char owner[TEST_SOURCES][TEST_TOKEN];
  char canonical[TEST_SOURCES][TEST_TOKEN];
  char revalidation_provider[TEST_SOURCES][TEST_TOKEN];
  char revalidation_root_token[TEST_SOURCES][TEST_TOKEN];
  char revalidation_owner[TEST_SOURCES][TEST_TOKEN];
  char revalidation_canonical[TEST_SOURCES][TEST_TOKEN];
  w_seed_ephemeral_driver_slot slots[TEST_SOURCES];
  w_seed_ephemeral_provider_request requests[TEST_SOURCES];
  w_seed_lexer_frame lexer_frames[TEST_LEX_FRAMES];
  w_seed_parse_token tokens[TEST_TOKENS];
  w_seed_parse_frame parse_frames[TEST_PARSE_FRAMES];
  w_seed_parse_issue issues[TEST_ISSUES];
  w_seed_module_origin origins[TEST_ORIGINS];
  w_seed_frontend_document candidate_documents[TEST_SOURCES];
  w_seed_ephemeral_graph_provider_facts candidate_facts[TEST_SOURCES];
  w_seed_ephemeral_graph_scratch_node graph_nodes[TEST_SOURCES];
  w_seed_ephemeral_graph_scratch_edge graph_edges[TEST_GRAPH_EDGES];
  size_t sorted_nodes[TEST_SOURCES];
  size_t node_ordinals[TEST_SOURCES];
  size_t sorted_edges[TEST_GRAPH_EDGES];
  size_t sorted_resolved_edges[TEST_GRAPH_EDGES];
  w_seed_module_origin graph_origins[TEST_GRAPH_EDGES];
  uint32_t indegree[TEST_SOURCES];
  uint32_t queue[TEST_SOURCES];
  uint32_t depths[TEST_SOURCES];
  w_seed_ephemeral_graph_scratch graph_scratch;
  w_seed_ephemeral_graph_inventory_item inventory[TEST_SOURCES];
  w_seed_ephemeral_graph_edge edges[TEST_GRAPH_EDGES];
  uint32_t document_order[TEST_SOURCES];
  w_seed_frontend_resolved_import resolved[TEST_GRAPH_EDGES];
  w_seed_frontend_document documents[TEST_SOURCES];
  w_seed_ephemeral_driver_scratch scratch;
  w_seed_ephemeral_driver_input input;
  w_seed_ephemeral_driver_output output;
  w_seed_ephemeral_driver_result result;
} driver_fixture;

typedef struct {
  w_seed_ephemeral_graph_inventory_item inventory[TEST_SOURCES];
  w_seed_ephemeral_graph_edge edges[TEST_GRAPH_EDGES];
  uint32_t document_order[TEST_SOURCES];
  w_seed_frontend_resolved_import resolved[TEST_GRAPH_EDGES];
  w_seed_frontend_document documents[TEST_SOURCES];
  size_t document_count;
} output_snapshot;

static driver_fixture fixture;

#if defined(__linux__)

static bool copy_c_string(char *destination, size_t capacity,
                          const char *source) {
  if (destination == NULL || source == NULL) return false;
  const size_t length = strlen(source);
  if (length >= capacity) return false;
  if (length != 0u) (void)memcpy(destination, source, length);
  destination[length] = '\0';
  return true;
}

static bool text_equals(w_seed_frontend_text text, const char *literal) {
  if (literal == NULL) return false;
  const size_t length = strlen(literal);
  return text.length == length &&
         (length == 0u ||
          (text.data != NULL && memcmp(text.data, literal, length) == 0));
}

#endif

static void initialize_fixture(const w_seed_ephemeral_provider_backend *backend) {
  (void)memset(&fixture, 0, sizeof(fixture));
  for (size_t index = 0u; index < TEST_SOURCES; index += 1u) {
    fixture.slots[index].source_id_storage = fixture.source_ids[index];
    fixture.slots[index].source_id_capacity = TEST_PATH;
    fixture.slots[index].module_id_storage = fixture.module_ids[index];
    fixture.slots[index].module_id_capacity = TEST_PATH;
    fixture.slots[index].nodes = fixture.nodes[index];
    fixture.slots[index].node_capacity = TEST_NODES;
    fixture.requests[index].staging_bytes = fixture.staging[index];
    fixture.requests[index].staging_capacity = TEST_BYTES;
    fixture.requests[index].revalidation_bytes = fixture.revalidation[index];
    fixture.requests[index].revalidation_capacity = TEST_BYTES;
    fixture.requests[index].bytes = fixture.bytes[index];
    fixture.requests[index].byte_capacity = TEST_BYTES;
    fixture.requests[index].tokens = (w_seed_ephemeral_provider_token_buffers){
        fixture.provider[index], TEST_TOKEN, fixture.root_token[index],
        TEST_TOKEN, fixture.owner[index], TEST_TOKEN, fixture.canonical[index],
        TEST_TOKEN};
    fixture.requests[index].revalidation_tokens =
        (w_seed_ephemeral_provider_token_buffers){
            fixture.revalidation_provider[index], TEST_TOKEN,
            fixture.revalidation_root_token[index], TEST_TOKEN,
            fixture.revalidation_owner[index], TEST_TOKEN,
            fixture.revalidation_canonical[index], TEST_TOKEN};
  }
  fixture.graph_scratch = (w_seed_ephemeral_graph_scratch){
      fixture.graph_nodes, TEST_SOURCES, fixture.graph_edges,
      TEST_GRAPH_EDGES, fixture.sorted_nodes, TEST_SOURCES,
      fixture.node_ordinals, TEST_SOURCES, fixture.sorted_edges,
      TEST_GRAPH_EDGES, fixture.sorted_resolved_edges, TEST_GRAPH_EDGES,
      fixture.graph_origins, TEST_GRAPH_EDGES, fixture.indegree, TEST_SOURCES,
      fixture.queue, TEST_SOURCES, fixture.depths, TEST_SOURCES};
  fixture.scratch = (w_seed_ephemeral_driver_scratch){
      fixture.slots, TEST_SOURCES, fixture.requests, TEST_SOURCES,
      fixture.lexer_frames, TEST_LEX_FRAMES, fixture.tokens, TEST_TOKENS,
      fixture.parse_frames, TEST_PARSE_FRAMES, fixture.issues, TEST_ISSUES,
      fixture.origins, TEST_ORIGINS, fixture.candidate_documents, TEST_SOURCES,
      fixture.candidate_facts, TEST_SOURCES, &fixture.graph_scratch};
  fixture.output = (w_seed_ephemeral_driver_output){
      {fixture.inventory, TEST_SOURCES, fixture.edges, TEST_GRAPH_EDGES,
       fixture.document_order, TEST_SOURCES, fixture.resolved,
       TEST_GRAPH_EDGES},
      fixture.documents, TEST_SOURCES, 0u};
  fixture.input = (w_seed_ephemeral_driver_input){
      {NULL, 0u},
      {"root.w", 6u},
      {TEST_SOURCES, TEST_BYTES, TEST_TOTAL_BYTES, TEST_PATH, TEST_TOKEN},
      TEST_GRAPH_EDGES,
      W_SEED_EPHEMERAL_GRAPH_MAX_DEPTH,
      {65536u, 256u},
      {0},
  };
  if (backend != NULL) fixture.input.backend = *backend;
}

static void set_output_sentinel(void) {
  (void)memset(fixture.inventory, 0xA5, sizeof(fixture.inventory));
  (void)memset(fixture.edges, 0xA5, sizeof(fixture.edges));
  (void)memset(fixture.document_order, 0xA5, sizeof(fixture.document_order));
  (void)memset(fixture.resolved, 0xA5, sizeof(fixture.resolved));
  (void)memset(fixture.documents, 0xA5, sizeof(fixture.documents));
  fixture.output.document_count = 77u;
}

static void snapshot_output(output_snapshot *snapshot) {
  if (snapshot == NULL) return;
  (void)memcpy(snapshot->inventory, fixture.inventory,
               sizeof(snapshot->inventory));
  (void)memcpy(snapshot->edges, fixture.edges, sizeof(snapshot->edges));
  (void)memcpy(snapshot->document_order, fixture.document_order,
               sizeof(snapshot->document_order));
  (void)memcpy(snapshot->resolved, fixture.resolved,
               sizeof(snapshot->resolved));
  (void)memcpy(snapshot->documents, fixture.documents,
               sizeof(snapshot->documents));
  snapshot->document_count = fixture.output.document_count;
}

static bool output_equals_snapshot(const output_snapshot *snapshot) {
  if (snapshot == NULL) return false;
  return memcmp(snapshot->inventory, fixture.inventory,
                sizeof(snapshot->inventory)) == 0 &&
         memcmp(snapshot->edges, fixture.edges, sizeof(snapshot->edges)) == 0 &&
         memcmp(snapshot->document_order, fixture.document_order,
                sizeof(snapshot->document_order)) == 0 &&
         memcmp(snapshot->resolved, fixture.resolved,
                sizeof(snapshot->resolved)) == 0 &&
         memcmp(snapshot->documents, fixture.documents,
                sizeof(snapshot->documents)) == 0 &&
         snapshot->document_count == fixture.output.document_count;
}

#if defined(__linux__)

typedef struct {
  char directory[TEST_PATH];
  char outside_path[TEST_PATH];
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

static bool write_file(const char *path, const char *text) {
  if (path == NULL || text == NULL) return false;
  const size_t length = strlen(text);
  const int file = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
                        (mode_t)0600);
  if (file < 0) return false;
  size_t offset = 0u;
  while (offset < length) {
    const ssize_t written = write(file, text + offset, length - offset);
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

static void unlink_if_present(const char *path) {
  if (path != NULL) (void)unlink(path);
}

static bool create_environment(linux_environment *environment) {
  if (environment == NULL) return false;
  (void)memset(environment, 0, sizeof(*environment));
  environment->base_dir_fd = -1;
  char directory_template[] = "/tmp/w-seed-ephemeral-driver-XXXXXX";
  char *directory = mkdtemp(directory_template);
  if (directory == NULL ||
      !copy_c_string(environment->directory, sizeof(environment->directory),
                     directory))
    return false;
  char nested[TEST_PATH];
  if (!path_join(environment->directory, "nested", nested, sizeof(nested)) ||
      mkdir(nested, (mode_t)0700) != 0)
    return false;
  char outside_template[] = "/tmp/w-seed-ephemeral-driver-outside-XXXXXX";
  const int outside = mkstemp(outside_template);
  if (outside < 0 || close(outside) != 0 ||
      !copy_c_string(environment->outside_path,
                     sizeof(environment->outside_path), outside_template))
    return false;
  environment->base_dir_fd =
      open(environment->directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  return environment->base_dir_fd >= 0 &&
         w_seed_ephemeral_provider_linux_init(&environment->context,
                                              environment->base_dir_fd) &&
         w_seed_ephemeral_provider_linux_backend(&environment->context,
                                                 &environment->backend);
}

static void destroy_environment(linux_environment *environment) {
  if (environment == NULL) return;
  const char *files[] = {"root.w", "nested/child.w", "nested/leaf.w",
                         "nested/missing.w", "escape.w"};
  for (size_t index = 0u; index < sizeof(files) / sizeof(files[0]);
       index += 1u) {
    char path[TEST_PATH];
    if (path_join(environment->directory, files[index], path, sizeof(path)))
      unlink_if_present(path);
  }
  char nested[TEST_PATH];
  if (path_join(environment->directory, "nested", nested, sizeof(nested)))
    (void)rmdir(nested);
  if (environment->base_dir_fd >= 0) (void)close(environment->base_dir_fd);
  if (environment->directory[0] != '\0') (void)rmdir(environment->directory);
  unlink_if_present(environment->outside_path);
  (void)memset(environment, 0, sizeof(*environment));
  environment->base_dir_fd = -1;
}

static bool reset_tree(const linux_environment *environment) {
  char root[TEST_PATH];
  char child[TEST_PATH];
  char leaf[TEST_PATH];
  char escape[TEST_PATH];
  if (environment == NULL ||
      !path_join(environment->directory, "root.w", root, sizeof(root)) ||
      !path_join(environment->directory, "nested/child.w", child,
                 sizeof(child)) ||
      !path_join(environment->directory, "nested/leaf.w", leaf,
                 sizeof(leaf)) ||
      !path_join(environment->directory, "escape.w", escape,
                 sizeof(escape)))
    return false;
  unlink_if_present(escape);
  return write_file(root, "module app;\nimport nested.child;\n") &&
         write_file(child, "module child;\nimport nested.leaf;\n") &&
         write_file(leaf, "module leaf;\n");
}

static bool slots_drained(const linux_environment *environment) {
  if (environment == NULL) return false;
  for (size_t index = 0u;
       index < W_SEED_EPHEMERAL_PROVIDER_LINUX_MAX_HANDLES; index += 1u)
    if (environment->context.slots[index].used) return false;
  return true;
}

static int process_fd_count(void) {
  DIR *directory = opendir("/proc/self/fd");
  if (directory == NULL) return -1;
  int count = 0;
  struct dirent *entry = NULL;
  while ((entry = readdir(directory)) != NULL)
    if (strcmp(entry->d_name, ".") != 0 &&
        strcmp(entry->d_name, "..") != 0)
      count += 1;
  if (closedir(directory) != 0) return -1;
  return count;
}

static bool run_driver(const linux_environment *environment,
                       w_seed_ephemeral_driver_status *status) {
  if (environment == NULL || status == NULL) return false;
  const int before = process_fd_count();
  *status = w_seed_ephemeral_driver_run(&fixture.input, &fixture.scratch,
                                        &fixture.output, &fixture.result);
  const int after = process_fd_count();
  CHECK(slots_drained(environment));
  CHECK(before < 0 || after < 0 || before == after);
  return true;
}

static bool test_linux_success(linux_environment *environment) {
  char root[TEST_PATH];
  CHECK(path_join(environment->directory, "root.w", root, sizeof(root)));
  CHECK(reset_tree(environment));
  initialize_fixture(&environment->backend);
  fixture.input.root_path = (w_seed_byte_view){
      (const uint8_t *)root, strlen(root)};
  w_seed_ephemeral_driver_status status;
  CHECK(run_driver(environment, &status));
  CHECK(status == W_SEED_EPHEMERAL_DRIVER_OK);
  CHECK(fixture.result.phase == W_SEED_EPHEMERAL_DRIVER_PHASE_COMMIT);
  CHECK(fixture.result.round == 2u);
  CHECK(fixture.output.document_count == 3u);
  CHECK(text_equals(fixture.output.graph.inventory[0].source_id, "root.w"));
  CHECK(text_equals(fixture.output.graph.inventory[1].source_id,
                    "nested/child.w"));
  CHECK(text_equals(fixture.output.graph.inventory[2].source_id,
                    "nested/leaf.w"));
  CHECK(fixture.output.graph.edges[0].source_ordinal == 0u &&
        fixture.output.graph.edges[0].target_ordinal == 1u);
  CHECK(fixture.output.graph.edges[1].source_ordinal == 1u &&
        fixture.output.graph.edges[1].target_ordinal == 2u);
  for (size_t index = 0u; index < fixture.output.document_count; index += 1u)
    CHECK(text_equals(fixture.output.documents[index].logical_source_id,
                      index == 0u ? "root.w"
                      : index == 1u ? "nested/child.w"
                                    : "nested/leaf.w"));
  CHECK(fixture.output.documents[0].source != NULL);
  CHECK(fixture.output.documents[0].source->bytes.length > 0u);
  return true;
}

static bool test_linux_failures(linux_environment *environment) {
  char root[TEST_PATH];
  char escape[TEST_PATH];
  CHECK(path_join(environment->directory, "root.w", root, sizeof(root)));
  CHECK(path_join(environment->directory, "escape.w", escape,
                  sizeof(escape)));

  CHECK(write_file(root, "module app;\nimport nested.missing;\n"));
  initialize_fixture(&environment->backend);
  fixture.input.root_path =
      (w_seed_byte_view){(const uint8_t *)root, strlen(root)};
  set_output_sentinel();
  output_snapshot snapshot;
  snapshot_output(&snapshot);
  w_seed_ephemeral_driver_status status;
  CHECK(run_driver(environment, &status));
  CHECK(status == W_SEED_EPHEMERAL_DRIVER_UNSUPPORTED);
  CHECK(fixture.result.failure ==
        W_SEED_EPHEMERAL_DRIVER_FAILURE_MISSING_LOCAL);
  CHECK(output_equals_snapshot(&snapshot));

  CHECK(unlink(escape) == 0 || errno == ENOENT);
  CHECK(symlink(environment->outside_path, escape) == 0);
  CHECK(write_file(root, "module app;\nimport escape;\n"));
  initialize_fixture(&environment->backend);
  fixture.input.root_path =
      (w_seed_byte_view){(const uint8_t *)root, strlen(root)};
  set_output_sentinel();
  snapshot_output(&snapshot);
  CHECK(run_driver(environment, &status));
  CHECK(status == W_SEED_EPHEMERAL_DRIVER_INVALID);
  CHECK(fixture.result.failure == W_SEED_EPHEMERAL_DRIVER_FAILURE_PROVIDER);
  CHECK(fixture.result.provider_result.failure ==
        W_SEED_EPHEMERAL_PROVIDER_FAILURE_SYMLINK);
  CHECK(output_equals_snapshot(&snapshot));
  return true;
}

static bool run_linux_tests(linux_environment *environment) {
  if (!environment->context.openat2_supported) {
    (void)puts("SKIP adapter-linux-openat2=unsupported");
    return true;
  }
  CHECK(test_linux_success(environment));
  CHECK(test_linux_failures(environment));
  return true;
}

#else

static const uint8_t non_linux_root_path[] = {'r', 'o', 'o', 't', '.', 'w'};

static bool test_non_linux_stub(void) {
  w_seed_ephemeral_provider_linux_context context;
  w_seed_ephemeral_provider_backend backend;
  CHECK(w_seed_ephemeral_provider_linux_init(&context, -1));
  CHECK(w_seed_ephemeral_provider_linux_backend(&context, &backend));
  initialize_fixture(&backend);
  fixture.input.root_path =
      (w_seed_byte_view){non_linux_root_path, sizeof(non_linux_root_path)};
  set_output_sentinel();
  output_snapshot snapshot;
  snapshot_output(&snapshot);
  const w_seed_ephemeral_driver_status status =
      w_seed_ephemeral_driver_run(&fixture.input, &fixture.scratch,
                                  &fixture.output, &fixture.result);
  CHECK(status == W_SEED_EPHEMERAL_DRIVER_UNSUPPORTED);
  CHECK(fixture.result.failure == W_SEED_EPHEMERAL_DRIVER_FAILURE_PROVIDER);
  CHECK(output_equals_snapshot(&snapshot));
  for (size_t index = 0u;
       index < W_SEED_EPHEMERAL_PROVIDER_LINUX_MAX_HANDLES; index += 1u)
    CHECK(!context.slots[index].used);
  (void)puts("SKIP adapter-linux-real=non-linux-stub");
  return true;
}

#endif

int main(void) {
#if defined(__linux__)
  linux_environment environment;
  if (!create_environment(&environment)) {
    destroy_environment(&environment);
    (void)fputs("ephemeral driver adapter environment setup failed\n", stderr);
    return 1;
  }
  const bool passed = run_linux_tests(&environment);
  destroy_environment(&environment);
  if (!passed) return 1;
#else
  if (!test_non_linux_stub()) return 1;
#endif
  (void)puts("w_seed_ephemeral_driver_linux_tests: ok");
  return 0;
}
