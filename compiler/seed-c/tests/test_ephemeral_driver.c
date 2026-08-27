#include "w_seed_ephemeral_check.h"

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
  TEST_FRONT_MODULES = 8,
  TEST_FRONT_IMPORTS = 32,
  TEST_FRONT_IMPORT_ITEMS = 32,
  TEST_FRONT_STRUCTS = 16,
  TEST_FRONT_FIELDS = 64,
  TEST_FRONT_DECLARATIONS = 32,
  TEST_FRONT_TYPES = 128,
  TEST_FRONT_FUNCTIONS = 32,
  TEST_FRONT_PARAMETERS = 128,
  TEST_FRONT_ENTRIES = 16,
  TEST_FRONT_STATEMENTS = 256,
  TEST_FRONT_EXPRESSIONS = 1024,
  TEST_FRONT_ARGUMENTS = 256,
  TEST_FRONT_SYMBOLS = 512,
  TEST_FRONT_FACTS = 512,
  TEST_FRONT_DIAGNOSTICS = 128,
  TEST_FRONT_DIAGNOSTIC_FACTS = TEST_FRONT_DIAGNOSTICS * 5,
  TEST_FRONT_DIAGNOSTIC_ITEMS = TEST_FRONT_DIAGNOSTICS * 4,
  TEST_FRONT_DIAGNOSTIC_LABELS = TEST_FRONT_DIAGNOSTICS * 2,
  TEST_FRONT_RECEIPT = 128 * 1024,
  TEST_FRONT_GENERIC_PARAMETERS = 64,
  TEST_FRONT_GENERIC_APPLICATIONS = 64,
  TEST_FRONT_GENERIC_ARGUMENTS = 256,
  TEST_FRONT_TYPED_CONST_EXPRESSIONS = 256,
  TEST_FRONT_CONST_VALUES = 512,
  TEST_FRONT_CONST_ELEMENTS = 512,
  TEST_FRONT_CONST_BYTES = 8192,
  TEST_FRONT_ENUMS = 16,
  TEST_FRONT_ENUM_CASES = 128,
  TEST_FRONT_ENUM_CASE_PARAMETERS = 256,
  TEST_FRONT_SWITCH_ARMS = 256,
  TEST_FRONT_ENUM_SUBSET_MEMBERS = 256,
  TEST_FRONT_ENUM_MEMBERSHIP_CASES = 1024,
  TEST_FRONT_CONST_DECLARATIONS = 32,
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
  w_seed_frontend_module frontend_modules[TEST_FRONT_MODULES];
  w_seed_frontend_import frontend_imports[TEST_FRONT_IMPORTS];
  w_seed_frontend_import_item
      frontend_import_items[TEST_FRONT_IMPORT_ITEMS];
  w_seed_frontend_struct frontend_structs[TEST_FRONT_STRUCTS];
  w_seed_frontend_field frontend_fields[TEST_FRONT_FIELDS];
  w_seed_frontend_type_declaration
      frontend_type_declarations[TEST_FRONT_DECLARATIONS];
  w_seed_frontend_alias frontend_aliases[TEST_FRONT_DECLARATIONS];
  w_seed_frontend_type frontend_types[TEST_FRONT_TYPES];
  w_seed_frontend_function frontend_functions[TEST_FRONT_FUNCTIONS];
  w_seed_frontend_parameter frontend_parameters[TEST_FRONT_PARAMETERS];
  w_seed_frontend_entry frontend_entries[TEST_FRONT_ENTRIES];
  w_seed_frontend_statement frontend_statements[TEST_FRONT_STATEMENTS];
  w_seed_frontend_expression frontend_expressions[TEST_FRONT_EXPRESSIONS];
  w_seed_frontend_argument frontend_arguments[TEST_FRONT_ARGUMENTS];
  w_seed_frontend_symbol frontend_symbols[TEST_FRONT_SYMBOLS];
  w_seed_frontend_fact frontend_facts[TEST_FRONT_FACTS];
  w_seed_frontend_diagnostic frontend_diagnostics[TEST_FRONT_DIAGNOSTICS];
  w_seed_frontend_diagnostic_fact
      frontend_diagnostic_facts[TEST_FRONT_DIAGNOSTIC_FACTS];
  w_seed_frontend_diagnostic_item
      frontend_diagnostic_items[TEST_FRONT_DIAGNOSTIC_ITEMS];
  w_seed_frontend_diagnostic_label
      frontend_diagnostic_labels[TEST_FRONT_DIAGNOSTIC_LABELS];
  uint8_t frontend_receipt[TEST_FRONT_RECEIPT];
  w_seed_frontend_enum frontend_enums[TEST_FRONT_ENUMS];
  w_seed_frontend_enum_case frontend_enum_cases[TEST_FRONT_ENUM_CASES];
  w_seed_frontend_enum_case_parameter
      frontend_enum_case_parameters[TEST_FRONT_ENUM_CASE_PARAMETERS];
  w_seed_frontend_const_declaration
      frontend_const_declarations[TEST_FRONT_CONST_DECLARATIONS];
  w_seed_frontend_switch_arm frontend_switch_arms[TEST_FRONT_SWITCH_ARMS];
  w_seed_frontend_enum_subset_member
      frontend_enum_subset_members[TEST_FRONT_ENUM_SUBSET_MEMBERS];
  w_seed_frontend_enum_membership_case
      frontend_enum_membership_cases[TEST_FRONT_ENUM_MEMBERSHIP_CASES];
  w_seed_frontend_generic_parameter
      frontend_generic_parameters[TEST_FRONT_GENERIC_PARAMETERS];
  w_seed_frontend_generic_application
      frontend_generic_applications[TEST_FRONT_GENERIC_APPLICATIONS];
  w_seed_frontend_generic_argument
      frontend_generic_arguments[TEST_FRONT_GENERIC_ARGUMENTS];
  w_seed_frontend_typed_const_expression
      frontend_typed_const_expressions[TEST_FRONT_TYPED_CONST_EXPRESSIONS];
  w_seed_frontend_const_value
      frontend_const_values[TEST_FRONT_CONST_VALUES];
  w_seed_frontend_const_element
      frontend_const_elements[TEST_FRONT_CONST_ELEMENTS];
  uint8_t frontend_const_bytes[TEST_FRONT_CONST_BYTES];
  w_seed_frontend_output frontend_output;
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
  fixture.frontend_output = (w_seed_frontend_output){
      .modules = fixture.frontend_modules,
      .module_capacity = TEST_FRONT_MODULES,
      .imports = fixture.frontend_imports,
      .import_capacity = TEST_FRONT_IMPORTS,
      .import_items = fixture.frontend_import_items,
      .import_item_capacity = TEST_FRONT_IMPORT_ITEMS,
      .structs = fixture.frontend_structs,
      .struct_capacity = TEST_FRONT_STRUCTS,
      .fields = fixture.frontend_fields,
      .field_capacity = TEST_FRONT_FIELDS,
      .type_declarations = fixture.frontend_type_declarations,
      .type_declaration_capacity = TEST_FRONT_DECLARATIONS,
      .aliases = fixture.frontend_aliases,
      .alias_capacity = TEST_FRONT_DECLARATIONS,
      .types = fixture.frontend_types,
      .type_capacity = TEST_FRONT_TYPES,
      .functions = fixture.frontend_functions,
      .function_capacity = TEST_FRONT_FUNCTIONS,
      .parameters = fixture.frontend_parameters,
      .parameter_capacity = TEST_FRONT_PARAMETERS,
      .arguments = fixture.frontend_arguments,
      .argument_capacity = TEST_FRONT_ARGUMENTS,
      .entries = fixture.frontend_entries,
      .entry_capacity = TEST_FRONT_ENTRIES,
      .statements = fixture.frontend_statements,
      .statement_capacity = TEST_FRONT_STATEMENTS,
      .expressions = fixture.frontend_expressions,
      .expression_capacity = TEST_FRONT_EXPRESSIONS,
      .symbols = fixture.frontend_symbols,
      .symbol_capacity = TEST_FRONT_SYMBOLS,
      .facts = fixture.frontend_facts,
      .fact_capacity = TEST_FRONT_FACTS,
      .diagnostics = fixture.frontend_diagnostics,
      .diagnostic_capacity = TEST_FRONT_DIAGNOSTICS,
      .diagnostic_facts = fixture.frontend_diagnostic_facts,
      .diagnostic_fact_capacity = TEST_FRONT_DIAGNOSTIC_FACTS,
      .diagnostic_items = fixture.frontend_diagnostic_items,
      .diagnostic_item_capacity = TEST_FRONT_DIAGNOSTIC_ITEMS,
      .diagnostic_labels = fixture.frontend_diagnostic_labels,
      .diagnostic_label_capacity = TEST_FRONT_DIAGNOSTIC_LABELS,
      .receipt = fixture.frontend_receipt,
      .receipt_capacity = TEST_FRONT_RECEIPT,
      .enums = fixture.frontend_enums,
      .enum_capacity = TEST_FRONT_ENUMS,
      .enum_cases = fixture.frontend_enum_cases,
      .enum_case_capacity = TEST_FRONT_ENUM_CASES,
      .enum_case_parameters = fixture.frontend_enum_case_parameters,
      .enum_case_parameter_capacity = TEST_FRONT_ENUM_CASE_PARAMETERS,
      .const_declarations = fixture.frontend_const_declarations,
      .const_declaration_capacity = TEST_FRONT_CONST_DECLARATIONS,
      .switch_arms = fixture.frontend_switch_arms,
      .switch_arm_capacity = TEST_FRONT_SWITCH_ARMS,
      .enum_subset_members = fixture.frontend_enum_subset_members,
      .enum_subset_member_capacity = TEST_FRONT_ENUM_SUBSET_MEMBERS,
      .enum_membership_cases = fixture.frontend_enum_membership_cases,
      .enum_membership_case_capacity = TEST_FRONT_ENUM_MEMBERSHIP_CASES,
      .generic_parameters = fixture.frontend_generic_parameters,
      .generic_parameter_capacity = TEST_FRONT_GENERIC_PARAMETERS,
      .generic_applications = fixture.frontend_generic_applications,
      .generic_application_capacity = TEST_FRONT_GENERIC_APPLICATIONS,
      .generic_arguments = fixture.frontend_generic_arguments,
      .generic_argument_capacity = TEST_FRONT_GENERIC_ARGUMENTS,
      .typed_const_expressions = fixture.frontend_typed_const_expressions,
      .typed_const_expression_capacity = TEST_FRONT_TYPED_CONST_EXPRESSIONS,
      .const_values = fixture.frontend_const_values,
      .const_value_capacity = TEST_FRONT_CONST_VALUES,
      .const_elements = fixture.frontend_const_elements,
      .const_element_capacity = TEST_FRONT_CONST_ELEMENTS,
      .const_bytes = fixture.frontend_const_bytes,
      .const_bytes_capacity = TEST_FRONT_CONST_BYTES};
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

static w_seed_ephemeral_check_status run_check_fixture(
    fake_backend *backend, uint8_t *jsonl, size_t jsonl_capacity,
    size_t *jsonl_length, uint8_t *json_staging, size_t json_staging_capacity,
    w_seed_ephemeral_check_result *result) {
  fixture.input.backend = fake_backend_vtable(backend);
  const w_seed_ephemeral_check_input input = {
      &fixture.input,
      &fixture.scratch,
      &fixture.output,
      &fixture.frontend_output,
      "D000001",
      7u,
      json_staging,
      json_staging_capacity};
  w_seed_ephemeral_check_output output = {
      jsonl,
      jsonl_capacity,
      jsonl_length == NULL ? 0u : *jsonl_length};
  const w_seed_ephemeral_check_status status =
      w_seed_ephemeral_check_run(&input, &output, result);
  if (jsonl_length != NULL) *jsonl_length = output.jsonl_length;
  return status;
}

static bool frontend_saw_function_from_child(
    const w_seed_frontend_result *frontend_result) {
  if (frontend_result == NULL) return false;
  for (size_t index = 0u; index < frontend_result->written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression =
        &fixture.frontend_expressions[index];
    if (expression->kind != W_SEED_FRONTEND_EXPR_CALL ||
        expression->resolved_function_index == W_SEED_FRONTEND_NONE)
      continue;
    const uint32_t function_index = expression->resolved_function_index;
    if ((size_t)function_index >= frontend_result->written.functions)
      continue;
    const w_seed_frontend_function *function =
        &fixture.frontend_functions[function_index];
    if (function->module_index == 1u && function->name.length == 5u &&
        memcmp(function->name.data, "value", 5u) == 0)
      return true;
  }
  return false;
}

static bool bytes_contain(const uint8_t *bytes, size_t byte_count,
                          const char *needle) {
  if (bytes == NULL || needle == NULL) return false;
  const size_t needle_length = strlen(needle);
  if (needle_length > byte_count) return false;
  for (size_t offset = 0u; offset <= byte_count - needle_length;
       offset += 1u) {
    if (memcmp(bytes + offset, needle, needle_length) == 0) return true;
  }
  return false;
}

static bool child_diagnostic_jsonl_is_well_formed(const uint8_t *jsonl,
                                                  size_t length) {
  if (jsonl == NULL || length == 0u) return false;
  size_t line_start = 0u;
  size_t line_count = 0u;
  while (line_start < length) {
    size_t newline = line_start;
    while (newline < length && jsonl[newline] != '\n') newline += 1u;
    if (newline == line_start || newline >= length ||
        jsonl[line_start] != '{' || jsonl[newline - 1u] != '}' ||
        !bytes_contain(jsonl + line_start, newline - line_start,
                       "\"schemaVersion\":1"))
      return false;
    const uint8_t *line = jsonl + line_start;
    const size_t line_length = newline - line_start;
    const char *instance = line_count == 0u
                               ? "\"instance\":\"D000001\""
                               : line_count == 1u
                                     ? "\"instance\":\"D000002\""
                                     : "\"instance\":\"D000003\"";
    const char *source = line_count == 0u ? "\"source\":\"root.w\""
                                         : "\"source\":\"child.w\"";
    const char *code = line_count < 2u ? "\"code\":\"W-SEM-0001\""
                                      : "\"code\":\"W-TYPE-0122\"";
    if (!bytes_contain(line, line_length, instance) ||
        !bytes_contain(line, line_length, source) ||
        !bytes_contain(line, line_length, code))
      return false;
    line_count += 1u;
    line_start = newline + 1u;
  }
  return line_count == 3u && line_start == length;
}

static bool test_ephemeral_check_cross_module(void) {
  fake_backend backend = {0};
  backend.root_text =
      "module app;\nimport { value } from child\n"
      "fn use(): i64 { return value() }\n";
  add_file(&backend, 0u, "child.w",
           "module child;\nexport fn value(): i64 { return 42 }\n");
  init_fixture(&backend);
  uint8_t jsonl[4096];
  uint8_t staging[4096];
  (void)memset(jsonl, 0xA5, sizeof(jsonl));
  size_t jsonl_length = 73u;
  w_seed_ephemeral_check_result result;
  CHECK(run_check_fixture(&backend, jsonl, sizeof(jsonl), &jsonl_length,
                          staging, sizeof(staging), &result) ==
        W_SEED_EPHEMERAL_CHECK_OK);
  CHECK(jsonl_length == 0u);
  CHECK(result.driver_status == W_SEED_EPHEMERAL_DRIVER_OK);
  CHECK(result.frontend_status == W_SEED_FRONTEND_OK);
  CHECK(fixture.output.document_count == 2u);
  CHECK(result.frontend_result.written.modules == 2u);
  CHECK(frontend_saw_function_from_child(&result.frontend_result));
  return true;
}

static bool test_ephemeral_check_diagnostic_determinism(void) {
  const char root_source[] =
      "module app;\nimport { bad } from child\n"
      "fn use(): i64 { return bad() }\n";
  const char child_source[] =
      "module child;\n"
      "export fn bad(): i64 { if 1 { return 42 } return 0 }\n";
  fake_backend backend = {0};
  backend.root_text = root_source;
  add_file(&backend, 0u, "child.w", child_source);
  init_fixture(&backend);
  uint8_t first[4096];
  uint8_t first_staging[4096];
  size_t first_length = 73u;
  w_seed_ephemeral_check_result first_result;
  CHECK(run_check_fixture(&backend, first, sizeof(first), &first_length,
                          first_staging, sizeof(first_staging),
                          &first_result) ==
        W_SEED_EPHEMERAL_CHECK_DIAGNOSTICS);
  CHECK(first_length != 0u);
  CHECK(first_result.frontend_status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(first_result.diagnostic_index == 0u);
  CHECK(first_result.frontend_result.written.diagnostics >= 1u);
  CHECK(fixture.frontend_diagnostics[0].document_index == 1u);
  CHECK(fixture.frontend_diagnostics[0].code.length == 10u &&
        memcmp(fixture.frontend_diagnostics[0].code.data, "W-SEM-0001",
               10u) == 0);
  /* There is no post-frontend injection seam in this composition API. The
   * adapter receives the frontend-owned index and the assertion above proves
   * the honest index-to-document binding; direct forged-index coverage stays
   * in test_diagnostic.c. */
  CHECK(first_result.diagnostic_status == W_SEED_DIAGNOSTIC_OK);
  CHECK(first_length + 1u < sizeof(first));
  CHECK(bytes_contain(first, first_length, "\"source\":\"child.w\""));

  fake_backend second_backend = {0};
  second_backend.root_text = root_source;
  add_file(&second_backend, 0u, "child.w", child_source);
  init_fixture(&second_backend);
  uint8_t second[4096];
  uint8_t second_staging[4096];
  size_t second_length = 11u;
  w_seed_ephemeral_check_result second_result;
  CHECK(run_check_fixture(&second_backend, second, sizeof(second),
                          &second_length, second_staging, sizeof(second_staging),
                          &second_result) ==
        W_SEED_EPHEMERAL_CHECK_DIAGNOSTICS);
  CHECK(second_length == first_length &&
        memcmp(second, first, first_length) == 0);
  return true;
}

static bool test_ephemeral_check_publication_barriers(void) {
  const char root_source[] =
      "module app;\nimport { bad } from child\n"
      "fn use(): i64 { return bad() }\n";
  const char child_source[] =
      "module child;\n"
      "export fn bad(): u16 { if 1 { return 1 } return 70_000_u32 }\n";
  const char supported_root_source[] =
      "module app;\nimport { bad } from child\n"
      "fn use(): i64 { return bad() }\n";
  const char supported_child_source[] =
      "module child;\n"
      "export fn bad(): i64 { if 1 { return 42 } return 0 }\n";

  fake_backend backend = {0};
  backend.root_text = root_source;
  add_file(&backend, 0u, "child.w", child_source);
  init_fixture(&backend);
  uint8_t final_json[4096];
  uint8_t staging[4096];
  (void)memset(final_json, 0xA5, sizeof(final_json));
  size_t final_length = 91u;
  w_seed_ephemeral_check_result result;
  CHECK(run_check_fixture(&backend, final_json, sizeof(final_json),
                          &final_length, staging, sizeof(staging), &result) ==
        W_SEED_EPHEMERAL_CHECK_DIAGNOSTICS);
  CHECK(result.status == W_SEED_EPHEMERAL_CHECK_DIAGNOSTICS);
  CHECK(result.frontend_status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(result.frontend_result.written.diagnostics == 3u);
  CHECK(result.diagnostic_index == 2u);
  CHECK(result.diagnostic_status == W_SEED_DIAGNOSTIC_OK);
  CHECK(result.diagnostic_result.status == W_SEED_DIAGNOSTIC_OK);
  CHECK(result.required_capacity == final_length && final_length != 91u);
  CHECK(fixture.frontend_diagnostics[0].document_index == 0u);
  CHECK(fixture.frontend_diagnostics[1].document_index == 1u);
  CHECK(fixture.frontend_diagnostics[2].document_index == 1u);
  CHECK(fixture.frontend_diagnostics[0].code.length == 10u &&
        memcmp(fixture.frontend_diagnostics[0].code.data, "W-SEM-0001",
               10u) == 0);
  CHECK(fixture.frontend_diagnostics[1].code.length == 10u &&
        memcmp(fixture.frontend_diagnostics[1].code.data, "W-SEM-0001",
               10u) == 0);
  CHECK(fixture.frontend_diagnostics[2].code.length == 11u &&
        memcmp(fixture.frontend_diagnostics[2].code.data, "W-TYPE-0122",
               11u) == 0);
  CHECK(child_diagnostic_jsonl_is_well_formed(final_json, final_length));
  uint8_t first_child_json[4096];
  const size_t first_child_length = final_length;
  (void)memcpy(first_child_json, final_json, first_child_length);

  backend = (fake_backend){0};
  backend.root_text = "module app;\n";
  init_fixture(&backend);
  (void)memset(final_json, 0x5Au, sizeof(final_json));
  final_length = 37u;
  const w_seed_ephemeral_check_status no_diag_status =
      run_check_fixture(&backend, final_json, sizeof(final_json),
                        &final_length, staging, sizeof(staging), &result);
  CHECK(no_diag_status == W_SEED_EPHEMERAL_CHECK_OK);
  CHECK(final_length == 0u);

  backend = (fake_backend){0};
  backend.root_text = supported_root_source;
  add_file(&backend, 0u, "child.w", supported_child_source);
  init_fixture(&backend);
  (void)memset(final_json, 0x3Cu, sizeof(final_json));
  final_length = 53u;
  CHECK(run_check_fixture(&backend, final_json, sizeof(final_json),
                          &final_length, staging, 1u, &result) ==
        W_SEED_EPHEMERAL_CHECK_CAPACITY);
  CHECK(result.failure == W_SEED_EPHEMERAL_CHECK_FAILURE_OUTPUT);
  CHECK(result.required_capacity > 1u);
  CHECK(result.frontend_result.written.diagnostics == 1u);
  CHECK(result.diagnostic_status == W_SEED_DIAGNOSTIC_CAPACITY);
  CHECK(result.diagnostic_result.status == W_SEED_DIAGNOSTIC_CAPACITY);
  CHECK(result.required_capacity == result.diagnostic_result.required_bytes + 1u);
  CHECK(final_length == 53u);
  for (size_t index = 0u; index < sizeof(final_json); index += 1u)
    CHECK(final_json[index] == 0x3Cu);

  backend = (fake_backend){0};
  backend.root_text = root_source;
  add_file(&backend, 0u, "child.w", child_source);
  init_fixture(&backend);
  (void)memset(final_json, 0xC3, sizeof(final_json));
  final_length = 59u;
  CHECK(run_check_fixture(&backend, final_json, sizeof(final_json),
                          &final_length, staging, sizeof(staging), &result) ==
        W_SEED_EPHEMERAL_CHECK_DIAGNOSTICS);
  CHECK(result.status == W_SEED_EPHEMERAL_CHECK_DIAGNOSTICS);
  CHECK(result.frontend_status == W_SEED_FRONTEND_DIAGNOSTICS);
  CHECK(result.frontend_result.written.diagnostics == 3u);
  CHECK(result.diagnostic_index == 2u);
  CHECK(result.diagnostic_status == W_SEED_DIAGNOSTIC_OK);
  CHECK(result.diagnostic_result.status == W_SEED_DIAGNOSTIC_OK);
  CHECK(result.required_capacity == final_length && final_length != 59u);
  CHECK(fixture.frontend_diagnostics[0].document_index == 0u);
  CHECK(fixture.frontend_diagnostics[1].document_index == 1u);
  CHECK(fixture.frontend_diagnostics[2].document_index == 1u);
  CHECK(child_diagnostic_jsonl_is_well_formed(final_json, final_length));
  CHECK(final_length == first_child_length &&
        memcmp(final_json, first_child_json, first_child_length) == 0);

  backend = (fake_backend){0};
  backend.root_text = supported_root_source;
  add_file(&backend, 0u, "child.w", supported_child_source);
  init_fixture(&backend);
  (void)memset(final_json, 0x96, sizeof(final_json));
  final_length = 61u;
  CHECK(run_check_fixture(&backend, final_json, 1u, &final_length, staging,
                          sizeof(staging), &result) ==
        W_SEED_EPHEMERAL_CHECK_CAPACITY);
  CHECK(result.failure == W_SEED_EPHEMERAL_CHECK_FAILURE_OUTPUT);
  CHECK(result.required_capacity > 1u);
  CHECK(result.frontend_result.written.diagnostics == 1u);
  CHECK(result.diagnostic_status == W_SEED_DIAGNOSTIC_CAPACITY);
  CHECK(result.diagnostic_result.status == W_SEED_DIAGNOSTIC_CAPACITY);
  CHECK(result.required_capacity == result.diagnostic_result.required_bytes + 1u);
  CHECK(final_length == 61u);
  for (size_t index = 0u; index < sizeof(final_json); index += 1u)
    CHECK(final_json[index] == 0x96u);

  backend = (fake_backend){0};
  backend.root_text = "module app;\nimport missing;\n";
  init_fixture(&backend);
  (void)memset(final_json, 0x69, sizeof(final_json));
  final_length = 67u;
  CHECK(run_check_fixture(&backend, final_json, sizeof(final_json),
                          &final_length, staging, sizeof(staging), &result) ==
        W_SEED_EPHEMERAL_CHECK_UNSUPPORTED);
  CHECK(final_length == 67u);
  for (size_t index = 0u; index < sizeof(final_json); index += 1u)
    CHECK(final_json[index] == 0x69u);

  backend = (fake_backend){0};
  backend.root_text = "module app;\nimport std.io;\n";
  init_fixture(&backend);
  (void)memset(final_json, 0x78, sizeof(final_json));
  final_length = 71u;
  CHECK(run_check_fixture(&backend, final_json, sizeof(final_json),
                          &final_length, staging, sizeof(staging), &result) ==
        W_SEED_EPHEMERAL_CHECK_UNSUPPORTED);
  CHECK(final_length == 71u);
  for (size_t index = 0u; index < sizeof(final_json); index += 1u)
    CHECK(final_json[index] == 0x78u);

  backend = (fake_backend){0};
  backend.root_text = "module app;\nimport child;\n";
  add_file(&backend, 0u, "child.w", "module child;\nimport app;\n");
  init_fixture(&backend);
  (void)memset(final_json, 0x87, sizeof(final_json));
  final_length = 73u;
  CHECK(run_check_fixture(&backend, final_json, sizeof(final_json),
                          &final_length, staging, sizeof(staging), &result) ==
        W_SEED_EPHEMERAL_CHECK_INVALID);
  CHECK(final_length == 73u);
  for (size_t index = 0u; index < sizeof(final_json); index += 1u)
    CHECK(final_json[index] == 0x87u);

  backend = (fake_backend){0};
  backend.root_text = supported_root_source;
  add_file(&backend, 0u, "child.w", supported_child_source);
  init_fixture(&backend);
  fixture.frontend_output.module_capacity = 0u;
  (void)memset(final_json, 0x4Bu, sizeof(final_json));
  final_length = 77u;
  CHECK(run_check_fixture(&backend, final_json, sizeof(final_json),
                          &final_length, staging, sizeof(staging), &result) ==
        W_SEED_EPHEMERAL_CHECK_CAPACITY);
  CHECK(final_length == 77u);
  for (size_t index = 0u; index < sizeof(final_json); index += 1u)
    CHECK(final_json[index] == 0x4Bu);
  return true;
}

static bool test_ephemeral_check_result_alias(void) {
  fake_backend backend = {0};
  backend.root_text = "module app;\n";
  uint8_t final_json[4096];
  uint8_t staging[4096];
  init_fixture(&backend);
  (void)memset(final_json, 0xA1, sizeof(final_json));
  const size_t final_length = 83u;
  w_seed_ephemeral_check_input input = {
      &fixture.input, &fixture.scratch, &fixture.output,
      &fixture.frontend_output, "D000001", 7u, staging, sizeof(staging)};
  w_seed_ephemeral_check_output output = {final_json, sizeof(final_json),
                                          final_length};
  const size_t output_size = sizeof(fixture.output);
  uint8_t output_before[sizeof(fixture.output)];
  (void)memcpy(output_before, &fixture.output, output_size);
  const w_seed_ephemeral_check_status alias_status =
      w_seed_ephemeral_check_run(
          &input, &output,
          (w_seed_ephemeral_check_result *)(void *)&fixture.output);
  CHECK(alias_status == W_SEED_EPHEMERAL_CHECK_INVALID);
  CHECK(memcmp(output_before, &fixture.output, output_size) == 0);
  CHECK(output.jsonl_length == final_length);

  w_seed_ephemeral_check_result overlap_result;
  (void)memset(&overlap_result, 0xCD, sizeof(overlap_result));
  w_seed_ephemeral_check_result overlap_before = overlap_result;
  (void)memset(final_json, 0xB2, sizeof(final_json));
  uint8_t final_before[sizeof(final_json)];
  (void)memcpy(final_before, final_json, sizeof(final_json));
  output.jsonl_length = final_length;
  input.json_staging = final_json;
  input.json_staging_capacity = sizeof(final_json);
  const w_seed_ephemeral_check_status overlap_status =
      w_seed_ephemeral_check_run(&input, &output, &overlap_result);
  CHECK(overlap_status == W_SEED_EPHEMERAL_CHECK_INVALID);
  CHECK(memcmp(final_json, final_before, sizeof(final_json)) == 0);
  CHECK(memcmp(&overlap_result, &overlap_before, sizeof(overlap_result)) == 0);
  return true;
}

int main(void) {
  if (!test_root_only() || !test_transitive_and_sorted() ||
      !test_chain_and_root_header() || !test_failures_and_output_atomicity() ||
      !test_cycle_capacity_overlap_and_unicode() ||
      !test_mutated_extra_candidate() || !test_parser_capacity_fields() ||
      !test_ephemeral_check_cross_module() ||
      !test_ephemeral_check_diagnostic_determinism() ||
      !test_ephemeral_check_publication_barriers() ||
      !test_ephemeral_check_result_alias())
    return 1;
  (void)puts("w_seed_ephemeral_driver_tests: ok");
  return 0;
}
