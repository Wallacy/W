#include "w_seed_ephemeral_driver.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      (void)fprintf(stderr, "ephemeral driver check failed: %s (%s:%d)\n", \
                    #condition, __FILE__, __LINE__);                           \
      return false;                                                             \
    }                                                                           \
  } while (0)

enum {
  TEST_SOURCES = 8,
  TEST_BYTES = 512,
  TEST_TOTAL_BYTES = 4096,
  TEST_PATH = 64,
  TEST_TOKEN = 32,
  TEST_NODES = 1024,
  TEST_LEX_FRAMES = 256,
  TEST_TOKENS = 2048,
  TEST_PARSE_FRAMES = 512,
  TEST_ISSUES = 128,
  TEST_ORIGINS = 32,
};

typedef struct {
  const char *source_id;
  const char *text;
} fake_file;

typedef struct {
  const char *root_text;
  const char *root_first;
  const char *root_after;
  fake_file files[TEST_SOURCES];
  size_t file_count;
  size_t root_open_calls;
  size_t open_source_calls;
  size_t std_open_calls;
  const char *active_root;
} fake_backend;

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
  w_seed_ephemeral_graph_scratch_edge graph_edges[W_SEED_EPHEMERAL_GRAPH_MAX_EDGES];
  size_t sorted_nodes[TEST_SOURCES];
  size_t node_ordinals[TEST_SOURCES];
  size_t sorted_edges[W_SEED_EPHEMERAL_GRAPH_MAX_EDGES];
  size_t sorted_resolved_edges[W_SEED_EPHEMERAL_GRAPH_MAX_EDGES];
  w_seed_module_origin graph_origins[W_SEED_EPHEMERAL_GRAPH_MAX_EDGES];
  uint32_t indegree[TEST_SOURCES];
  uint32_t queue[TEST_SOURCES];
  uint32_t depths[TEST_SOURCES];
  w_seed_ephemeral_graph_scratch graph_scratch;
  w_seed_ephemeral_graph_inventory_item inventory[TEST_SOURCES];
  w_seed_ephemeral_graph_edge edges[W_SEED_EPHEMERAL_GRAPH_MAX_EDGES];
  uint32_t document_order[TEST_SOURCES];
  w_seed_frontend_resolved_import resolved[W_SEED_EPHEMERAL_GRAPH_MAX_EDGES];
  w_seed_frontend_document documents[TEST_SOURCES];
  w_seed_ephemeral_driver_scratch scratch;
  w_seed_ephemeral_driver_input input;
  w_seed_ephemeral_driver_output output;
  w_seed_ephemeral_driver_result result;
} driver_fixture;

static driver_fixture fixture;

static bool copy_token(char *destination, size_t capacity, const char *text,
                       size_t length) {
  if (destination == NULL || text == NULL || length > capacity) return false;
  (void)memcpy(destination, text, length);
  return true;
}

static void observation_tokens(
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_observation *observation, const char *canonical) {
  const char provider[] = "fake";
  const char root[] = "root";
  const char owner[] = "owner";
  const size_t canonical_length = strlen(canonical);
  (void)memset(observation, 0, sizeof(*observation));
  if (!copy_token(tokens->provider_id, tokens->provider_id_capacity, provider,
                 sizeof(provider) - 1u) ||
      !copy_token(tokens->root_token, tokens->root_token_capacity, root,
                  sizeof(root) - 1u) ||
      !copy_token(tokens->source_provider_owner_token,
                  tokens->source_provider_owner_token_capacity, owner,
                  sizeof(owner) - 1u) ||
      !copy_token(tokens->canonical_token, tokens->canonical_token_capacity,
                  canonical, canonical_length))
    return;
  observation->opened = true;
  observation->containment_inside = true;
  observation->symlink = W_SEED_EPHEMERAL_GRAPH_SYMLINK_NONE;
  observation->provider_id_length = sizeof(provider) - 1u;
  observation->root_token_length = sizeof(root) - 1u;
  observation->source_provider_owner_token_length = sizeof(owner) - 1u;
  observation->canonical_token_length = canonical_length;
}

static const char *file_text(const fake_backend *backend, size_t index) {
  if (backend == NULL || index >= backend->file_count) return NULL;
  return backend->files[index].text;
}

static size_t file_for_id(const fake_backend *backend,
                          w_seed_frontend_text source_id) {
  if (backend == NULL) return SIZE_MAX;
  for (size_t index = 0u; index < backend->file_count; index += 1u) {
    const size_t length = strlen(backend->files[index].source_id);
    if (source_id.length == length &&
        memcmp(source_id.data, backend->files[index].source_id, length) == 0)
      return index;
  }
  return SIZE_MAX;
}

static const char *handle_text(const fake_backend *backend,
                               w_seed_ephemeral_provider_handle handle) {
  if (handle.value == (uintptr_t)2u) return backend->active_root;
  if (handle.value < (uintptr_t)100u) return NULL;
  return file_text(backend, (size_t)(handle.value - (uintptr_t)100u));
}

static w_seed_ephemeral_provider_backend_status fake_open_root(
    void *context, w_seed_byte_view root_path,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_handle *root_handle,
    w_seed_ephemeral_provider_handle *root_source_handle,
    w_seed_ephemeral_provider_observation *observation) {
  fake_backend *backend = (fake_backend *)context;
  if (backend == NULL || root_path.data == NULL || root_path.length == 0u ||
      tokens == NULL || root_handle == NULL || root_source_handle == NULL ||
      observation == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  backend->root_open_calls += 1u;
  backend->active_root = backend->root_text;
  if (backend->root_first != NULL) {
    backend->active_root = backend->root_open_calls == 1u
                               ? backend->root_first
                               : backend->root_after;
  }
  *root_handle = (w_seed_ephemeral_provider_handle){1u};
  *root_source_handle = (w_seed_ephemeral_provider_handle){2u};
  observation_tokens(tokens, observation, "root");
  return observation->opened ? W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK
                             : W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
}

static w_seed_ephemeral_provider_backend_status fake_open_source(
    void *context, w_seed_ephemeral_provider_handle root_handle,
    w_seed_frontend_text source_id,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_handle *source_handle,
    w_seed_ephemeral_provider_observation *observation) {
  fake_backend *backend = (fake_backend *)context;
  if (backend == NULL || root_handle.value != (uintptr_t)1u || tokens == NULL ||
      source_handle == NULL || observation == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  backend->open_source_calls += 1u;
  if (source_id.length >= 3u && source_id.data != NULL &&
      memcmp(source_id.data, "std", 3u) == 0) {
    backend->std_open_calls += 1u;
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_NOT_FOUND;
  }
  const size_t index = file_for_id(backend, source_id);
  if (index == SIZE_MAX)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_NOT_FOUND;
  *source_handle = (w_seed_ephemeral_provider_handle){
      (uintptr_t)100u + (uintptr_t)index};
  char canonical[TEST_TOKEN];
  canonical[0] = 'c';
  canonical[1] = (char)('0' + (char)index);
  canonical[2] = '\0';
  observation_tokens(tokens, observation, canonical);
  return observation->opened ? W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK
                             : W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
}

static w_seed_ephemeral_provider_backend_status fake_read_source(
    void *context, w_seed_ephemeral_provider_handle source_handle,
    uint8_t *bytes, size_t capacity, size_t *written) {
  const fake_backend *backend = (const fake_backend *)context;
  const char *text = handle_text(backend, source_handle);
  if (written == NULL || text == NULL) return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  const size_t length = strlen(text);
  *written = length;
  if (length > capacity) return W_SEED_EPHEMERAL_PROVIDER_BACKEND_CAPACITY;
  if (length != 0u && bytes == NULL) return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  if (length != 0u) (void)memcpy(bytes, text, length);
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
}

static w_seed_ephemeral_provider_backend_status fake_revalidate_source(
    void *context, w_seed_ephemeral_provider_handle root_handle,
    w_seed_ephemeral_provider_handle source_handle,
    w_seed_frontend_text source_id,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_observation *observation, uint8_t *bytes,
  size_t capacity, size_t *written) {
  fake_backend *backend = (fake_backend *)context;
  if (backend == NULL || root_handle.value != (uintptr_t)1u || tokens == NULL ||
      observation == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_NOT_FOUND;
  if (source_handle.value == (uintptr_t)2u) {
    if (source_id.length != 6u ||
        memcmp(source_id.data, "root.w", 6u) != 0)
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_NOT_FOUND;
    const size_t root_length = strlen(backend->active_root);
    if (written == NULL || root_length > capacity) {
      if (written != NULL) *written = root_length;
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_CAPACITY;
    }
    *written = root_length;
    if (root_length != 0u && bytes == NULL)
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
    if (root_length != 0u) (void)memcpy(bytes, backend->active_root, root_length);
    observation_tokens(tokens, observation, "root");
    return observation->opened ? W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK
                               : W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  }
  if (file_for_id(backend, source_id) == SIZE_MAX)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_NOT_FOUND;
  const w_seed_ephemeral_provider_backend_status status =
      fake_read_source(context, source_handle, bytes, capacity, written);
  if (status != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) return status;
  const size_t index = file_for_id(backend, source_id);
  char canonical[TEST_TOKEN];
  canonical[0] = 'c';
  canonical[1] = (char)('0' + (char)index);
  canonical[2] = '\0';
  observation_tokens(tokens, observation, canonical);
  return observation->opened ? W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK
                             : W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
}

static void fake_close_source(void *context,
                              w_seed_ephemeral_provider_handle source_handle) {
  (void)context;
  (void)source_handle;
}

static void fake_close_root(void *context,
                            w_seed_ephemeral_provider_handle root_handle) {
  (void)context;
  (void)root_handle;
}

static w_seed_ephemeral_provider_backend fake_backend_vtable(
    fake_backend *backend) {
  return (w_seed_ephemeral_provider_backend){
      backend,
      fake_open_root,
      fake_open_source,
      fake_read_source,
      fake_revalidate_source,
      fake_close_source,
      fake_close_root,
      {{8u, 8u}, {8u, 8u}, {8u, 8u}, {8u, 8u}}};
}

static void init_fixture(fake_backend *backend) {
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
      W_SEED_EPHEMERAL_GRAPH_MAX_EDGES, fixture.sorted_nodes, TEST_SOURCES,
      fixture.node_ordinals, TEST_SOURCES, fixture.sorted_edges,
      W_SEED_EPHEMERAL_GRAPH_MAX_EDGES, fixture.sorted_resolved_edges,
      W_SEED_EPHEMERAL_GRAPH_MAX_EDGES, fixture.graph_origins,
      W_SEED_EPHEMERAL_GRAPH_MAX_EDGES, fixture.indegree, TEST_SOURCES,
      fixture.queue, TEST_SOURCES, fixture.depths, TEST_SOURCES};
  fixture.scratch = (w_seed_ephemeral_driver_scratch){
      fixture.slots, TEST_SOURCES, fixture.requests, TEST_SOURCES,
      fixture.lexer_frames, TEST_LEX_FRAMES, fixture.tokens, TEST_TOKENS,
      fixture.parse_frames, TEST_PARSE_FRAMES, fixture.issues, TEST_ISSUES,
      fixture.origins, TEST_ORIGINS, fixture.candidate_documents, TEST_SOURCES,
      fixture.candidate_facts, TEST_SOURCES, &fixture.graph_scratch};
  fixture.output = (w_seed_ephemeral_driver_output){
      {fixture.inventory, TEST_SOURCES, fixture.edges,
       W_SEED_EPHEMERAL_GRAPH_MAX_EDGES, fixture.document_order, TEST_SOURCES,
       fixture.resolved, W_SEED_EPHEMERAL_GRAPH_MAX_EDGES},
      fixture.documents, TEST_SOURCES, 0u};
  fixture.input = (w_seed_ephemeral_driver_input){
      {(const uint8_t *)"virtual-root", 12u},
      {"root.w", 6u},
      {TEST_SOURCES, TEST_BYTES, TEST_TOTAL_BYTES, TEST_PATH, TEST_TOKEN},
      W_SEED_EPHEMERAL_GRAPH_MAX_EDGES,
      W_SEED_EPHEMERAL_GRAPH_MAX_DEPTH,
      {65536u, 256u},
      fake_backend_vtable(backend)};
}

static w_seed_ephemeral_driver_status run_fixture(fake_backend *backend) {
  fixture.input.backend = fake_backend_vtable(backend);
  return w_seed_ephemeral_driver_run(&fixture.input, &fixture.scratch,
                                     &fixture.output, &fixture.result);
}

static void add_file(fake_backend *backend, size_t index, const char *source_id,
                     const char *text) {
  backend->files[index] = (fake_file){source_id, text};
  if (backend->file_count <= index) backend->file_count = index + 1u;
}

static bool test_root_only(void) {
  fake_backend backend = {0};
  backend.root_text = "module app;\n";
  init_fixture(&backend);
  const w_seed_ephemeral_driver_status status = run_fixture(&backend);
  CHECK(status == W_SEED_EPHEMERAL_DRIVER_OK);
  CHECK(fixture.output.document_count == 1u);
  CHECK(fixture.output.graph.inventory[0].source_id.length == 6u);
  CHECK(memcmp(fixture.output.graph.inventory[0].module_id.data, "app", 3u) == 0);
  CHECK(backend.open_source_calls == 0u);
  return true;
}

static bool test_transitive_and_sorted(void) {
  fake_backend backend = {0};
  backend.root_text = "module app;\nimport zeta;\nimport alpha;\n";
  add_file(&backend, 0u, "zeta.w", "module zeta;\n");
  add_file(&backend, 1u, "alpha.w", "module alpha;\n");
  init_fixture(&backend);
  const w_seed_ephemeral_driver_status status = run_fixture(&backend);
  CHECK(status == W_SEED_EPHEMERAL_DRIVER_OK);
  CHECK(fixture.output.document_count == 3u);
  CHECK(fixture.output.graph.inventory[0].source_id.length == 6u);
  CHECK(memcmp(fixture.output.graph.inventory[1].source_id.data, "alpha.w", 7u) == 0);
  CHECK(memcmp(fixture.output.graph.inventory[2].source_id.data, "zeta.w", 6u) == 0);
  CHECK(fixture.output.graph.edges[0].source_ordinal == 0u);
  CHECK(fixture.output.graph.edges[0].target_ordinal == 1u);
  CHECK(fixture.output.graph.edges[1].target_ordinal == 2u);
  return true;
}

static bool test_chain_and_root_header(void) {
  fake_backend backend = {0};
  backend.root_text = "module renamed;\nimport zeta;\n";
  add_file(&backend, 0u, "zeta.w", "module zeta;\nimport leaf;\n");
  add_file(&backend, 1u, "leaf.w", "module leaf;\n");
  init_fixture(&backend);
  CHECK(run_fixture(&backend) == W_SEED_EPHEMERAL_DRIVER_OK);
  CHECK(fixture.output.document_count == 3u);
  CHECK(fixture.output.graph.edges[0].source_ordinal !=
        fixture.output.graph.edges[0].target_ordinal);
  CHECK(fixture.output.graph.edges[1].source_ordinal !=
        fixture.output.graph.edges[1].target_ordinal);
  return true;
}

static bool test_failures_and_output_atomicity(void) {
  fake_backend backend = {0};
  backend.root_text = "module app;\nimport missing;\n";
  init_fixture(&backend);
  (void)memset(fixture.inventory, 0xA5, sizeof(fixture.inventory));
  (void)memset(fixture.documents, 0xA5, sizeof(fixture.documents));
  fixture.output.document_count = 91u;
  const unsigned char inventory_before = ((const unsigned char *)fixture.inventory)[0];
  CHECK(run_fixture(&backend) == W_SEED_EPHEMERAL_DRIVER_UNSUPPORTED);
  CHECK(fixture.result.failure == W_SEED_EPHEMERAL_DRIVER_FAILURE_MISSING_LOCAL);
  CHECK(((const unsigned char *)fixture.inventory)[0] == inventory_before);
  CHECK(((const unsigned char *)fixture.documents)[0] == 0xA5u);
  CHECK(fixture.output.document_count == 91u);

  backend = (fake_backend){0};
  backend.root_text = "module app;\nimport std.io;\n";
  init_fixture(&backend);
  CHECK(run_fixture(&backend) == W_SEED_EPHEMERAL_DRIVER_UNSUPPORTED);
  CHECK(fixture.result.failure == W_SEED_EPHEMERAL_DRIVER_FAILURE_STD_PROVIDER);
  CHECK(backend.std_open_calls == 0u);

  backend = (fake_backend){0};
  backend.root_text = "module app;\nimport child;\n";
  add_file(&backend, 0u, "child.w", "module wrong;\n");
  init_fixture(&backend);
  CHECK(run_fixture(&backend) == W_SEED_EPHEMERAL_DRIVER_INVALID);
  CHECK(fixture.result.failure == W_SEED_EPHEMERAL_DRIVER_FAILURE_HEADER);

  backend = (fake_backend){0};
  backend.root_text = "module app;\nimport ;\n";
  init_fixture(&backend);
  CHECK(run_fixture(&backend) == W_SEED_EPHEMERAL_DRIVER_INVALID);
  CHECK(fixture.result.failure == W_SEED_EPHEMERAL_DRIVER_FAILURE_PARSE);
  return true;
}

static bool test_cycle_capacity_overlap_and_unicode(void) {
  fake_backend backend = {0};
  backend.root_text = "module app;\nimport child;\n";
  add_file(&backend, 0u, "child.w", "module child;\nimport app;\n");
  init_fixture(&backend);
  CHECK(run_fixture(&backend) == W_SEED_EPHEMERAL_DRIVER_INVALID);
  CHECK(fixture.result.graph_result.failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_CYCLE);

  backend = (fake_backend){0};
  backend.root_text = "module app;\nimport child;\n";
  add_file(&backend, 0u, "child.w", "module child;\n");
  init_fixture(&backend);
  fixture.input.provider_limits.max_sources = 1u;
  CHECK(run_fixture(&backend) == W_SEED_EPHEMERAL_DRIVER_CAPACITY);
  CHECK(fixture.result.capacity_field == W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_SLOT);

  init_fixture(&backend);
  fixture.input.max_edges = 0u;
  fixture.output.document_count = 73u;
  CHECK(run_fixture(&backend) == W_SEED_EPHEMERAL_DRIVER_CAPACITY);
  CHECK(fixture.result.graph_status == W_SEED_EPHEMERAL_GRAPH_CAPACITY);
  CHECK(fixture.output.document_count == 73u);

  init_fixture(&backend);
  fixture.input.max_depth = 0u;
  CHECK(run_fixture(&backend) == W_SEED_EPHEMERAL_DRIVER_CAPACITY);
  CHECK(fixture.result.graph_status == W_SEED_EPHEMERAL_GRAPH_CAPACITY);

  init_fixture(&backend);
  fixture.input.provider_limits.max_source_bytes = 1u;
  CHECK(run_fixture(&backend) == W_SEED_EPHEMERAL_DRIVER_CAPACITY);
  CHECK(fixture.result.capacity_field ==
        W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PROVIDER);

  init_fixture(&backend);
  fixture.input.provider_limits.max_total_source_bytes = 1u;
  CHECK(run_fixture(&backend) == W_SEED_EPHEMERAL_DRIVER_CAPACITY);
  CHECK(fixture.result.capacity_field ==
        W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PROVIDER);

  init_fixture(&backend);
  fixture.slots[0].source_id_capacity = 3u;
  CHECK(run_fixture(&backend) == W_SEED_EPHEMERAL_DRIVER_CAPACITY);
  CHECK(fixture.result.capacity_field ==
        W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_SOURCE_ID);

  init_fixture(&backend);
  fixture.output.documents = NULL;
  fixture.output.document_capacity = 0u;
  CHECK(run_fixture(&backend) == W_SEED_EPHEMERAL_DRIVER_CAPACITY);
  CHECK(fixture.result.capacity_field ==
        W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_DOCUMENT);

  init_fixture(&backend);
  fixture.output.documents = fixture.candidate_documents;
  CHECK(run_fixture(&backend) == W_SEED_EPHEMERAL_DRIVER_INVALID);
  CHECK(fixture.result.failure == W_SEED_EPHEMERAL_DRIVER_FAILURE_STORAGE);

  init_fixture(&backend);
  w_seed_ephemeral_driver_input input_before = fixture.input;
  const w_seed_ephemeral_driver_status alias_status =
      w_seed_ephemeral_driver_run(
          &fixture.input, &fixture.scratch, &fixture.output,
          (w_seed_ephemeral_driver_result *)&fixture.input);
  CHECK(alias_status == W_SEED_EPHEMERAL_DRIVER_INVALID);
  CHECK(memcmp(&fixture.input, &input_before, sizeof(input_before)) == 0);

  init_fixture(&backend);
  fixture.input.root_source_id = (w_seed_frontend_text){"café.w", 7u};
  CHECK(run_fixture(&backend) == W_SEED_EPHEMERAL_DRIVER_UNSUPPORTED);
  CHECK(fixture.result.failure == W_SEED_EPHEMERAL_DRIVER_FAILURE_UNSUPPORTED_NFC);
  return true;
}

static bool test_mutated_extra_candidate(void) {
  fake_backend backend = {0};
  backend.root_first = "module app;\nimport zeta;\n";
  backend.root_after = "module app;\nimport alpha;\n";
  add_file(&backend, 0u, "zeta.w", "module zeta;\n");
  add_file(&backend, 1u, "alpha.w", "module alpha;\n");
  init_fixture(&backend);
  CHECK(run_fixture(&backend) == W_SEED_EPHEMERAL_DRIVER_OK);
  CHECK(backend.root_open_calls == 3u);
  CHECK(fixture.result.round < W_SEED_EPHEMERAL_DRIVER_MAX_ROUNDS);
  CHECK(fixture.output.document_count == 2u);
  for (size_t index = 0u; index < fixture.output.document_count; index += 1u) {
    CHECK(fixture.output.graph.inventory[index].source_id.length != 6u ||
          memcmp(fixture.output.graph.inventory[index].source_id.data, "zeta.w",
                 6u) != 0);
  }
  return true;
}

static bool test_parser_capacity_fields(void) {
  fake_backend backend = {0};
  backend.root_text = "module app;\n";
  init_fixture(&backend);
  fixture.slots[0].node_capacity = 1u;
  CHECK(run_fixture(&backend) == W_SEED_EPHEMERAL_DRIVER_CAPACITY);
  CHECK(fixture.result.failure == W_SEED_EPHEMERAL_DRIVER_FAILURE_PARSE);
  CHECK(fixture.result.capacity_field ==
        W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_NODE);
  CHECK(fixture.result.required_capacity == 2u);

  backend.root_text = "export fn f() {}\n";
  init_fixture(&backend);
  fixture.scratch.token_capacity = 1u;
  CHECK(run_fixture(&backend) == W_SEED_EPHEMERAL_DRIVER_CAPACITY);
  CHECK(fixture.result.failure == W_SEED_EPHEMERAL_DRIVER_FAILURE_PARSE);
  CHECK(fixture.result.capacity_field ==
        W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_TOKEN);
  CHECK(fixture.result.required_capacity == 2u);

  backend.root_text = "module app;\n";
  init_fixture(&backend);
  fixture.scratch.parse_frame_capacity = 1u;
  CHECK(run_fixture(&backend) == W_SEED_EPHEMERAL_DRIVER_CAPACITY);
  CHECK(fixture.result.failure == W_SEED_EPHEMERAL_DRIVER_FAILURE_PARSE);
  CHECK(fixture.result.capacity_field ==
        W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_FRAME);
  CHECK(fixture.result.required_capacity == 2u);

  backend.root_text = "module ;\n";
  init_fixture(&backend);
  fixture.scratch.issue_capacity = 0u;
  CHECK(run_fixture(&backend) == W_SEED_EPHEMERAL_DRIVER_CAPACITY);
  CHECK(fixture.result.failure == W_SEED_EPHEMERAL_DRIVER_FAILURE_PARSE);
  CHECK(fixture.result.capacity_field ==
        W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_ISSUE);
  CHECK(fixture.result.required_capacity == 1u);

  backend.root_text = "module app;\nfn f() { \"outer ${ \\\"inner ${x}\\\" }\" }\n";
  init_fixture(&backend);
  fixture.scratch.lexer_frame_capacity = 1u;
  CHECK(run_fixture(&backend) == W_SEED_EPHEMERAL_DRIVER_CAPACITY);
  CHECK(fixture.result.failure == W_SEED_EPHEMERAL_DRIVER_FAILURE_PARSE);
  CHECK(fixture.result.capacity_field ==
        W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_LEXER_FRAME);
  CHECK(fixture.result.required_capacity == 2u);
  return true;
}

int main(void) {
  if (!test_root_only() || !test_transitive_and_sorted() ||
      !test_chain_and_root_header() || !test_failures_and_output_atomicity() ||
      !test_cycle_capacity_overlap_and_unicode() ||
      !test_mutated_extra_candidate() || !test_parser_capacity_fields())
    return 1;
  (void)puts("w_seed_ephemeral_driver_tests: ok");
  return 0;
}
