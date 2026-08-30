#include "check_pipeline.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "check pipeline test failed: %s (%s:%d)\n",     \
                    #condition, __FILE__, __LINE__);                          \
      return false;                                                            \
    }                                                                          \
  } while (0)

enum {
  TEST_SOURCES = 2,
  TEST_PATH = 64,
  TEST_TOKEN = 32,
  TEST_LEXER_FRAMES = 128,
  TEST_TOKENS = 1024,
  TEST_PARSE_FRAMES = 256,
  TEST_ISSUES = 64,
  TEST_ORIGINS = 16,
  TEST_GRAPH_EDGES = 32,
  TEST_MODULES = 4,
  TEST_IMPORTS = 8,
  TEST_IMPORT_ITEMS = 8,
  TEST_STRUCTS = 4,
  TEST_FIELDS = 16,
  TEST_DECLARATIONS = 8,
  TEST_TYPES = 32,
  TEST_FUNCTIONS = 8,
  TEST_PARAMETERS = 16,
  TEST_ENTRIES = 4,
  TEST_STATEMENTS = 64,
  TEST_EXPRESSIONS = 256,
  TEST_ARGUMENTS = 32,
  TEST_SYMBOLS = 128,
  TEST_FACTS = 128,
  TEST_DIAGNOSTICS = 16,
  TEST_DIAGNOSTIC_FACTS = TEST_DIAGNOSTICS * 5,
  TEST_DIAGNOSTIC_ITEMS = TEST_DIAGNOSTICS * 4,
  TEST_DIAGNOSTIC_LABELS = TEST_DIAGNOSTICS * 2,
  TEST_RECEIPT = 16384,
  TEST_ENUMS = 4,
  TEST_ENUM_CASES = 8,
  TEST_ENUM_CASE_PARAMETERS = 16,
  TEST_CONST_DECLARATIONS = 8,
  TEST_SWITCH_ARMS = 16,
  TEST_ENUM_SUBSET_MEMBERS = 16,
  TEST_ENUM_MEMBERSHIP_CASES = 32,
  TEST_GENERIC_PARAMETERS = 8,
  TEST_GENERIC_APPLICATIONS = 8,
  TEST_GENERIC_ARGUMENTS = 16,
  TEST_TYPED_CONST_EXPRESSIONS = 16,
  TEST_CONST_VALUES = 32,
  TEST_CONST_ELEMENTS = 32,
  TEST_CONST_BYTES = 1024,
};

typedef struct {
  const char *source_id;
  const char *text;
} fake_file;

typedef struct {
  const char *root_text;
  fake_file files[TEST_SOURCES];
  size_t file_count;
  bool unsupported_root;
  size_t root_open_calls;
} fake_backend;

typedef struct {
  char source_ids[TEST_SOURCES][TEST_PATH];
  char module_ids[TEST_SOURCES][TEST_PATH];
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
  w_seed_lexer_frame lexer_frames[TEST_LEXER_FRAMES];
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
  w_seed_ephemeral_driver_scratch driver_scratch;
  w_seed_ephemeral_driver_input driver_input;
  w_seed_ephemeral_driver_output driver_output;
  w_seed_frontend_module modules[TEST_MODULES];
  w_seed_frontend_import imports[TEST_IMPORTS];
  w_seed_frontend_import_item import_items[TEST_IMPORT_ITEMS];
  w_seed_frontend_struct structs[TEST_STRUCTS];
  w_seed_frontend_field fields[TEST_FIELDS];
  w_seed_frontend_type_declaration type_declarations[TEST_DECLARATIONS];
  w_seed_frontend_alias aliases[TEST_DECLARATIONS];
  w_seed_frontend_type types[TEST_TYPES];
  w_seed_frontend_function functions[TEST_FUNCTIONS];
  w_seed_frontend_parameter parameters[TEST_PARAMETERS];
  w_seed_frontend_entry entries[TEST_ENTRIES];
  w_seed_frontend_statement statements[TEST_STATEMENTS];
  w_seed_frontend_expression expressions[TEST_EXPRESSIONS];
  w_seed_frontend_argument arguments[TEST_ARGUMENTS];
  w_seed_frontend_symbol symbols[TEST_SYMBOLS];
  w_seed_frontend_fact facts[TEST_FACTS];
  w_seed_frontend_diagnostic diagnostics[TEST_DIAGNOSTICS];
  w_seed_frontend_diagnostic_fact diagnostic_facts[TEST_DIAGNOSTIC_FACTS];
  w_seed_frontend_diagnostic_item diagnostic_items[TEST_DIAGNOSTIC_ITEMS];
  w_seed_frontend_diagnostic_label diagnostic_labels[TEST_DIAGNOSTIC_LABELS];
  uint8_t receipt[TEST_RECEIPT];
  w_seed_frontend_enum enums[TEST_ENUMS];
  w_seed_frontend_enum_case enum_cases[TEST_ENUM_CASES];
  w_seed_frontend_enum_case_parameter enum_case_parameters
      [TEST_ENUM_CASE_PARAMETERS];
  w_seed_frontend_const_declaration const_declarations
      [TEST_CONST_DECLARATIONS];
  w_seed_frontend_switch_arm switch_arms[TEST_SWITCH_ARMS];
  w_seed_frontend_enum_subset_member enum_subset_members
      [TEST_ENUM_SUBSET_MEMBERS];
  w_seed_frontend_enum_membership_case enum_membership_cases
      [TEST_ENUM_MEMBERSHIP_CASES];
  w_seed_frontend_generic_parameter generic_parameters
      [TEST_GENERIC_PARAMETERS];
  w_seed_frontend_generic_application generic_applications
      [TEST_GENERIC_APPLICATIONS];
  w_seed_frontend_generic_argument generic_arguments[TEST_GENERIC_ARGUMENTS];
  w_seed_frontend_typed_const_expression typed_const_expressions
      [TEST_TYPED_CONST_EXPRESSIONS];
  w_seed_frontend_const_value const_values[TEST_CONST_VALUES];
  w_seed_frontend_const_element const_elements[TEST_CONST_ELEMENTS];
  uint8_t const_bytes[TEST_CONST_BYTES];
  w_seed_frontend_output frontend_output;
} pipeline_fixture;

static bool copy_text(char *destination, size_t capacity, const char *text,
                      size_t length) {
  if (destination == NULL || text == NULL || length > capacity) return false;
  (void)memcpy(destination, text, length);
  return true;
}

static void fill_observation(
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_observation *observation,
    const char *canonical) {
  static const char provider[] = "fake";
  static const char root[] = "root";
  static const char owner[] = "owner";
  const size_t canonical_length = strlen(canonical);
  (void)memset(observation, 0, sizeof(*observation));
  if (!copy_text(tokens->provider_id, tokens->provider_id_capacity, provider,
                 sizeof(provider) - 1u) ||
      !copy_text(tokens->root_token, tokens->root_token_capacity, root,
                 sizeof(root) - 1u) ||
      !copy_text(tokens->source_provider_owner_token,
                 tokens->source_provider_owner_token_capacity, owner,
                 sizeof(owner) - 1u) ||
      !copy_text(tokens->canonical_token, tokens->canonical_token_capacity,
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

static size_t fake_file_index(const fake_backend *backend,
                              w_seed_frontend_text source_id) {
  if (backend == NULL || source_id.data == NULL) return SIZE_MAX;
  for (size_t index = 0u; index < backend->file_count; index += 1u) {
    const size_t length = strlen(backend->files[index].source_id);
    if (source_id.length == length &&
        memcmp(source_id.data, backend->files[index].source_id, length) == 0)
      return index;
  }
  return SIZE_MAX;
}

static const char *fake_handle_text(const fake_backend *backend,
                                    w_seed_ephemeral_provider_handle handle) {
  if (handle.value == (uintptr_t)2u) return backend->root_text;
  if (handle.value >= (uintptr_t)100u)
    return backend->files[handle.value - (uintptr_t)100u].text;
  return NULL;
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
  if (backend->unsupported_root)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
  *root_handle = (w_seed_ephemeral_provider_handle){1u};
  *root_source_handle = (w_seed_ephemeral_provider_handle){2u};
  fill_observation(tokens, observation, "root");
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
  if (backend == NULL || root_handle.value != (uintptr_t)1u ||
      tokens == NULL || source_handle == NULL || observation == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  if (source_id.length >= 4u &&
      memcmp(source_id.data, "std/", sizeof("std/") - 1u) == 0)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_NOT_FOUND;
  const size_t index = fake_file_index(backend, source_id);
  if (index == SIZE_MAX)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_NOT_FOUND;
  *source_handle = (w_seed_ephemeral_provider_handle){
      (uintptr_t)100u + (uintptr_t)index};
  char canonical[TEST_TOKEN];
  canonical[0] = 'c';
  canonical[1] = (char)('0' + (char)index);
  canonical[2] = '\0';
  fill_observation(tokens, observation, canonical);
  return observation->opened ? W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK
                             : W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
}

static w_seed_ephemeral_provider_backend_status fake_read_source(
    void *context, w_seed_ephemeral_provider_handle source_handle,
    uint8_t *bytes, size_t capacity, size_t *written) {
  const fake_backend *backend = (const fake_backend *)context;
  const char *text = fake_handle_text(backend, source_handle);
  if (written == NULL || text == NULL) return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  const size_t length = strlen(text);
  *written = length;
  if (length > capacity) return W_SEED_EPHEMERAL_PROVIDER_BACKEND_CAPACITY;
  if (length != 0u && bytes == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
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
  if (backend == NULL || root_handle.value != (uintptr_t)1u ||
      tokens == NULL || observation == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  const w_seed_ephemeral_provider_backend_status read_status =
      fake_read_source(context, source_handle, bytes, capacity, written);
  if (read_status != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) return read_status;
  const size_t index = source_handle.value == (uintptr_t)2u
                           ? SIZE_MAX
                           : (size_t)(source_handle.value - (uintptr_t)100u);
  if (index == SIZE_MAX) {
    if (source_id.length != 6u ||
        memcmp(source_id.data, "root.w", 6u) != 0)
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_NOT_FOUND;
    fill_observation(tokens, observation, "root");
  } else {
    if (fake_file_index(backend, source_id) != index)
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_NOT_FOUND;
    char canonical[TEST_TOKEN];
    canonical[0] = 'c';
    canonical[1] = (char)('0' + (char)index);
    canonical[2] = '\0';
    fill_observation(tokens, observation, canonical);
  }
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
      .context = backend,
      .open_root = fake_open_root,
      .open_source = fake_open_source,
      .read_source = fake_read_source,
      .revalidate_source = fake_revalidate_source,
      .close_source = fake_close_source,
      .close_root = fake_close_root,
      .metadata = {{8u, 8u}, {8u, 8u}, {8u, 8u}, {8u, 8u}}};
}

static void initialize_frontend_output(pipeline_fixture *fixture) {
  fixture->frontend_output = (w_seed_frontend_output){
      .modules = fixture->modules,
      .module_capacity = TEST_MODULES,
      .imports = fixture->imports,
      .import_capacity = TEST_IMPORTS,
      .import_items = fixture->import_items,
      .import_item_capacity = TEST_IMPORT_ITEMS,
      .structs = fixture->structs,
      .struct_capacity = TEST_STRUCTS,
      .fields = fixture->fields,
      .field_capacity = TEST_FIELDS,
      .type_declarations = fixture->type_declarations,
      .type_declaration_capacity = TEST_DECLARATIONS,
      .aliases = fixture->aliases,
      .alias_capacity = TEST_DECLARATIONS,
      .types = fixture->types,
      .type_capacity = TEST_TYPES,
      .functions = fixture->functions,
      .function_capacity = TEST_FUNCTIONS,
      .parameters = fixture->parameters,
      .parameter_capacity = TEST_PARAMETERS,
      .arguments = fixture->arguments,
      .argument_capacity = TEST_ARGUMENTS,
      .entries = fixture->entries,
      .entry_capacity = TEST_ENTRIES,
      .statements = fixture->statements,
      .statement_capacity = TEST_STATEMENTS,
      .expressions = fixture->expressions,
      .expression_capacity = TEST_EXPRESSIONS,
      .symbols = fixture->symbols,
      .symbol_capacity = TEST_SYMBOLS,
      .facts = fixture->facts,
      .fact_capacity = TEST_FACTS,
      .diagnostics = fixture->diagnostics,
      .diagnostic_capacity = TEST_DIAGNOSTICS,
      .diagnostic_facts = fixture->diagnostic_facts,
      .diagnostic_fact_capacity = TEST_DIAGNOSTIC_FACTS,
      .diagnostic_items = fixture->diagnostic_items,
      .diagnostic_item_capacity = TEST_DIAGNOSTIC_ITEMS,
      .diagnostic_labels = fixture->diagnostic_labels,
      .diagnostic_label_capacity = TEST_DIAGNOSTIC_LABELS,
      .receipt = fixture->receipt,
      .receipt_capacity = TEST_RECEIPT,
      .enums = fixture->enums,
      .enum_capacity = TEST_ENUMS,
      .enum_cases = fixture->enum_cases,
      .enum_case_capacity = TEST_ENUM_CASES,
      .enum_case_parameters = fixture->enum_case_parameters,
      .enum_case_parameter_capacity = TEST_ENUM_CASE_PARAMETERS,
      .const_declarations = fixture->const_declarations,
      .const_declaration_capacity = TEST_CONST_DECLARATIONS,
      .switch_arms = fixture->switch_arms,
      .switch_arm_capacity = TEST_SWITCH_ARMS,
      .enum_subset_members = fixture->enum_subset_members,
      .enum_subset_member_capacity = TEST_ENUM_SUBSET_MEMBERS,
      .enum_membership_cases = fixture->enum_membership_cases,
      .enum_membership_case_capacity = TEST_ENUM_MEMBERSHIP_CASES,
      .generic_parameters = fixture->generic_parameters,
      .generic_parameter_capacity = TEST_GENERIC_PARAMETERS,
      .generic_applications = fixture->generic_applications,
      .generic_application_capacity = TEST_GENERIC_APPLICATIONS,
      .generic_arguments = fixture->generic_arguments,
      .generic_argument_capacity = TEST_GENERIC_ARGUMENTS,
      .typed_const_expressions = fixture->typed_const_expressions,
      .typed_const_expression_capacity = TEST_TYPED_CONST_EXPRESSIONS,
      .const_values = fixture->const_values,
      .const_value_capacity = TEST_CONST_VALUES,
      .const_elements = fixture->const_elements,
      .const_element_capacity = TEST_CONST_ELEMENTS,
      .const_bytes = fixture->const_bytes,
      .const_bytes_capacity = TEST_CONST_BYTES};
}

static void initialize_fixture(pipeline_fixture *fixture,
                               fake_backend *backend) {
  (void)memset(fixture, 0, sizeof(*fixture));
  for (size_t index = 0u; index < TEST_SOURCES; index += 1u) {
    fixture->slots[index].source_id_storage = fixture->source_ids[index];
    fixture->slots[index].source_id_capacity = TEST_PATH;
    fixture->slots[index].module_id_storage = fixture->module_ids[index];
    fixture->slots[index].module_id_capacity = TEST_PATH;
    fixture->requests[index].tokens =
        (w_seed_ephemeral_provider_token_buffers){
            fixture->provider[index], TEST_TOKEN, fixture->root_token[index],
            TEST_TOKEN, fixture->owner[index], TEST_TOKEN,
            fixture->canonical[index], TEST_TOKEN};
    fixture->requests[index].revalidation_tokens =
        (w_seed_ephemeral_provider_token_buffers){
            fixture->revalidation_provider[index], TEST_TOKEN,
            fixture->revalidation_root_token[index], TEST_TOKEN,
            fixture->revalidation_owner[index], TEST_TOKEN,
            fixture->revalidation_canonical[index], TEST_TOKEN};
  }
  fixture->graph_scratch = (w_seed_ephemeral_graph_scratch){
      fixture->graph_nodes, TEST_SOURCES, fixture->graph_edges,
      TEST_GRAPH_EDGES, fixture->sorted_nodes, TEST_SOURCES,
      fixture->node_ordinals, TEST_SOURCES, fixture->sorted_edges,
      TEST_GRAPH_EDGES, fixture->sorted_resolved_edges, TEST_GRAPH_EDGES,
      fixture->graph_origins, TEST_GRAPH_EDGES, fixture->indegree,
      TEST_SOURCES, fixture->queue, TEST_SOURCES, fixture->depths,
      TEST_SOURCES};
  fixture->driver_scratch = (w_seed_ephemeral_driver_scratch){
      fixture->slots, TEST_SOURCES, fixture->requests, TEST_SOURCES,
      fixture->lexer_frames, TEST_LEXER_FRAMES, fixture->tokens, TEST_TOKENS,
      fixture->parse_frames, TEST_PARSE_FRAMES, fixture->issues, TEST_ISSUES,
      fixture->origins, TEST_ORIGINS, fixture->candidate_documents,
      TEST_SOURCES, fixture->candidate_facts, TEST_SOURCES,
      &fixture->graph_scratch};
  fixture->driver_output = (w_seed_ephemeral_driver_output){
      {fixture->inventory, TEST_SOURCES, fixture->edges, TEST_GRAPH_EDGES,
       fixture->document_order, TEST_SOURCES, fixture->resolved,
       TEST_GRAPH_EDGES},
      fixture->documents, TEST_SOURCES, 0u};
  fixture->driver_input = (w_seed_ephemeral_driver_input){
      {(const uint8_t *)"virtual-root", sizeof("virtual-root") - 1u},
      {"root.w", sizeof("root.w") - 1u},
      {TEST_SOURCES, W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCE_BYTES,
       W_SEED_EPHEMERAL_PROVIDER_MAX_TOTAL_SOURCE_BYTES,
       W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES,
       W_SEED_EPHEMERAL_PROVIDER_MAX_TOKEN_BYTES},
      TEST_GRAPH_EDGES,
      W_SEED_EPHEMERAL_GRAPH_MAX_DEPTH,
      {65536u, 256u},
      fake_backend_vtable(backend)};
  initialize_frontend_output(fixture);
}

static w_seed_check_pipeline_input pipeline_input(
    pipeline_fixture *fixture, w_seed_check_storage *storage) {
  return (w_seed_check_pipeline_input){
      &fixture->driver_input,
      &fixture->driver_scratch,
      &fixture->driver_output,
      &fixture->frontend_output,
      storage,
      "D000001",
      7u};
}

static bool bytes_contain(const uint8_t *bytes, size_t length,
                          const char *needle) {
  if (bytes == NULL || needle == NULL) return false;
  const size_t needle_length = strlen(needle);
  if (needle_length > length) return false;
  for (size_t offset = 0u; offset <= length - needle_length; offset += 1u)
    if (memcmp(bytes + offset, needle, needle_length) == 0) return true;
  return false;
}

static bool run_pipeline_case(
    const char *root_text, const char *child_text,
    w_seed_check_pipeline_status expected_status,
    w_seed_check_pipeline_result *result, w_seed_check_storage *storage,
    pipeline_fixture *fixture, fake_backend *backend) {
  (void)memset(backend, 0, sizeof(*backend));
  backend->root_text = root_text;
  if (child_text != NULL) {
    backend->files[0] = (fake_file){"child.w", child_text};
    backend->file_count = 1u;
  }
  initialize_fixture(fixture, backend);
  CHECK(w_seed_check_storage_init(storage));
  const w_seed_check_pipeline_input input = pipeline_input(fixture, storage);
  const w_seed_check_pipeline_status actual =
      w_seed_check_pipeline_run(&input, result);
  CHECK(actual == expected_status);
  return true;
}

static bool test_root_child_clean_retries(void) {
  static const char root[] =
      "module app;\nimport { value } from child\n"
      "fn use(): i64 { return value() }\n";
  static const char child[] =
      "module child;\nexport fn value(): i64 { return 42 }\n";
  pipeline_fixture fixture;
  fake_backend backend;
  w_seed_check_storage storage = {0};
  w_seed_check_pipeline_result result;
  CHECK(run_pipeline_case(root, child, W_SEED_CHECK_PIPELINE_CLEAN, &result,
                          &storage, &fixture, &backend));
  CHECK(result.attempts >= 4u);
  CHECK(result.json_length == 0u);
  CHECK(storage.acquisition.staging_capacity[0u] != 0u &&
        storage.acquisition.staging_capacity[1u] != 0u);
  CHECK(storage.acquisition.node_capacity[0u] != 0u && storage.acquisition.node_capacity[1u] != 0u);
  CHECK(storage.json_staging_capacity == 0u &&
        storage.json_final_capacity == 0u);
  w_seed_check_storage_destroy(&storage);
  return true;
}

static bool test_child_diagnostic_is_deterministic(void) {
  static const char root[] =
      "module app;\nimport { bad } from child\n"
      "fn use(): i64 { return bad() }\n";
  static const char child[] =
      "module child;\n"
      "export fn bad(): i64 { if 1 { return 42 } return 0 }\n";
  pipeline_fixture first_fixture;
  pipeline_fixture second_fixture;
  fake_backend first_backend;
  fake_backend second_backend;
  w_seed_check_storage first_storage = {0};
  w_seed_check_storage second_storage = {0};
  w_seed_check_pipeline_result first_result;
  w_seed_check_pipeline_result second_result;
  CHECK(run_pipeline_case(root, child, W_SEED_CHECK_PIPELINE_DIAGNOSTICS,
                          &first_result, &first_storage, &first_fixture,
                          &first_backend));
  CHECK(run_pipeline_case(root, child, W_SEED_CHECK_PIPELINE_DIAGNOSTICS,
                          &second_result, &second_storage, &second_fixture,
                          &second_backend));
  CHECK(first_result.json_length != 0u &&
        first_result.json_length == second_result.json_length);
  CHECK(first_result.attempts >= 4u && second_result.attempts >= 4u);
  CHECK(first_storage.acquisition.staging_capacity[0u] != 0u &&
        first_storage.acquisition.staging_capacity[1u] != 0u);
  CHECK(first_storage.acquisition.node_capacity[0u] != 0u &&
        first_storage.acquisition.node_capacity[1u] != 0u);
  CHECK(memcmp(first_storage.json_final, second_storage.json_final,
               first_result.json_length) == 0);
  CHECK(bytes_contain(first_storage.json_final, first_result.json_length,
                      "\"source\":\"child.w\""));
  CHECK(bytes_contain(first_storage.json_final, first_result.json_length,
                      "\"code\":\"W-SEM-0001\""));
  w_seed_check_storage_destroy(&first_storage);
  w_seed_check_storage_destroy(&second_storage);
  return true;
}

static bool test_failures_do_not_publish_json(void) {
  pipeline_fixture fixture;
  fake_backend backend;
  w_seed_check_storage storage = {0};
  w_seed_check_pipeline_result result;
  static const char cycle_child[] = "module child;\nimport root;\n";
  CHECK(run_pipeline_case("module app;\nimport missing;\n", NULL,
                          W_SEED_CHECK_PIPELINE_UNSUPPORTED, &result, &storage,
                          &fixture, &backend));
  CHECK(result.json_length == SIZE_MAX && result.attempts >= 1u &&
        storage.json_final == NULL);
  w_seed_check_storage_destroy(&storage);

  CHECK(run_pipeline_case("module app;\nimport std.io;\n", NULL,
                          W_SEED_CHECK_PIPELINE_UNSUPPORTED, &result, &storage,
                          &fixture, &backend));
  CHECK(result.json_length == SIZE_MAX && storage.json_final == NULL);
  w_seed_check_storage_destroy(&storage);

  CHECK(run_pipeline_case("module app;\nimport child;\n", cycle_child,
                          W_SEED_CHECK_PIPELINE_INVALID, &result, &storage,
                          &fixture, &backend));
  CHECK(result.json_length == SIZE_MAX);
  w_seed_check_storage_destroy(&storage);
  return true;
}

static bool test_backend_unsupported_and_frontend_capacity(void) {
  pipeline_fixture fixture;
  fake_backend backend = {0};
  w_seed_check_storage storage = {0};
  w_seed_check_pipeline_result result;
  backend.root_text = "module app;\n";
  backend.unsupported_root = true;
  initialize_fixture(&fixture, &backend);
  CHECK(w_seed_check_storage_init(&storage));
  w_seed_check_pipeline_input input = pipeline_input(&fixture, &storage);
  CHECK(w_seed_check_pipeline_run(&input, &result) ==
        W_SEED_CHECK_PIPELINE_UNSUPPORTED);
  CHECK(result.json_length == SIZE_MAX && storage.json_final == NULL);
  w_seed_check_storage_destroy(&storage);

  backend = (fake_backend){0};
  backend.root_text = "module app;\n";
  initialize_fixture(&fixture, &backend);
  fixture.frontend_output.module_capacity = 0u;
  CHECK(w_seed_check_storage_init(&storage));
  input = pipeline_input(&fixture, &storage);
  CHECK(w_seed_check_pipeline_run(&input, &result) ==
        W_SEED_CHECK_PIPELINE_CAPACITY);
  CHECK(result.json_length == SIZE_MAX && storage.json_final == NULL);
  CHECK(result.retry.action == W_SEED_CHECK_RETRY_TERMINAL_CAPACITY);
  w_seed_check_storage_destroy(&storage);
  return true;
}

typedef struct {
  size_t allocations;
  size_t fail_after;
} allocator_probe;

static allocator_probe probe;

static void *probe_allocate(size_t size) {
  if (probe.allocations >= probe.fail_after) return NULL;
  probe.allocations += 1u;
  return malloc(size);
}

static void probe_deallocate(void *pointer) { free(pointer); }

static bool test_allocator_fault_and_input_fault(void) {
  pipeline_fixture fixture;
  fake_backend backend = {0};
  w_seed_check_storage storage = {0};
  w_seed_check_pipeline_result result;
  backend.root_text = "module app;\n";
  initialize_fixture(&fixture, &backend);
  probe.allocations = 0u;
  probe.fail_after = 0u;
  CHECK(w_seed_check_storage_init_with_allocator(
      &storage, probe_allocate, probe_deallocate));
  w_seed_check_pipeline_input input = pipeline_input(&fixture, &storage);
  CHECK(w_seed_check_pipeline_run(&input, &result) ==
        W_SEED_CHECK_PIPELINE_FAULT);
  CHECK(result.json_length == SIZE_MAX &&
        result.retry.detail == W_SEED_CHECK_RETRY_DETAIL_ALLOCATION &&
        storage.json_final == NULL);
  w_seed_check_storage_destroy(&storage);

  initialize_fixture(&fixture, &backend);
  fixture.driver_scratch.slots = NULL;
  CHECK(w_seed_check_storage_init(&storage));
  input = pipeline_input(&fixture, &storage);
  CHECK(w_seed_check_pipeline_run(&input, &result) ==
        W_SEED_CHECK_PIPELINE_FAULT);
  CHECK(result.json_length == SIZE_MAX && result.attempts == 0u);
  w_seed_check_storage_destroy(&storage);
  return true;
}

static bool test_real_driver_acquisition_envelopes(void) {
  static const char root_only[] = "module app;\n";
  const w_seed_ephemeral_provider_capacity_field fields[3] = {
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES,
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_BYTES,
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_OUTPUT_BYTES};
  for (size_t field_index = 0u; field_index < 3u; field_index += 1u) {
    pipeline_fixture fixture;
    fake_backend backend = {0};
    uint8_t staging[256] = {0};
    uint8_t revalidation[256] = {0};
    uint8_t published[256] = {0};
    backend.root_text = root_only;
    initialize_fixture(&fixture, &backend);
    if (field_index != 0u) {
      fixture.requests[0u].staging_bytes = staging;
      fixture.requests[0u].staging_capacity = sizeof(staging);
    }
    if (field_index == 2u) {
      fixture.requests[0u].revalidation_bytes = revalidation;
      fixture.requests[0u].revalidation_capacity = sizeof(revalidation);
    }
    if (field_index != 2u) {
      fixture.requests[0u].bytes = published;
      fixture.requests[0u].byte_capacity = sizeof(published);
    }
    w_seed_ephemeral_driver_result driver_result;
    const w_seed_ephemeral_driver_status driver_status =
        w_seed_ephemeral_driver_run(
            &fixture.driver_input, &fixture.driver_scratch,
            &fixture.driver_output, &driver_result);
    CHECK(driver_status == W_SEED_EPHEMERAL_DRIVER_CAPACITY &&
          driver_result.failure == W_SEED_EPHEMERAL_DRIVER_FAILURE_PROVIDER &&
          driver_result.provider_result.capacity_field == fields[field_index]);
    w_seed_acquisition_storage storage = {0};
    CHECK(w_seed_acquisition_storage_init(&storage));
    w_seed_ephemeral_driver_result forged = driver_result;
    forged.graph_status = W_SEED_EPHEMERAL_GRAPH_OK;
    CHECK(w_seed_acquisition_retry_apply(&storage, driver_status, &forged)
              .status == W_SEED_ACQUISITION_RETRY_INVALID);
    const w_seed_acquisition_retry_outcome outcome =
        w_seed_acquisition_retry_apply(&storage, driver_status,
                                       &driver_result);
    CHECK(outcome.status == W_SEED_ACQUISITION_RETRY_OK &&
          outcome.action == W_SEED_ACQUISITION_RETRY_RETRY);
    w_seed_acquisition_storage_destroy(&storage);
  }

  {
    pipeline_fixture limit_fixture;
    fake_backend limit_backend = {0};
    uint8_t limit_staging[256] = {0};
    uint8_t limit_revalidation[256] = {0};
    uint8_t limit_published[256] = {0};
    limit_backend.root_text = root_only;
    initialize_fixture(&limit_fixture, &limit_backend);
    limit_fixture.driver_input.provider_limits.max_source_bytes = 1u;
    limit_fixture.requests[0u].staging_bytes = limit_staging;
    limit_fixture.requests[0u].staging_capacity = sizeof(limit_staging);
    limit_fixture.requests[0u].revalidation_bytes = limit_revalidation;
    limit_fixture.requests[0u].revalidation_capacity =
        sizeof(limit_revalidation);
    limit_fixture.requests[0u].bytes = limit_published;
    limit_fixture.requests[0u].byte_capacity = sizeof(limit_published);
    w_seed_ephemeral_driver_result limit_result;
    const w_seed_ephemeral_driver_status limit_status =
        w_seed_ephemeral_driver_run(
            &limit_fixture.driver_input, &limit_fixture.driver_scratch,
            &limit_fixture.driver_output, &limit_result);
    CHECK(limit_status == W_SEED_EPHEMERAL_DRIVER_CAPACITY &&
          limit_result.failure ==
              W_SEED_EPHEMERAL_DRIVER_FAILURE_PROVIDER &&
          limit_result.provider_result.capacity_field ==
              W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES &&
          limit_result.provider_result.backend_status ==
              W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK);
    w_seed_acquisition_storage limit_storage = {0};
    CHECK(w_seed_acquisition_storage_init(&limit_storage));
    const w_seed_acquisition_retry_outcome limit_outcome =
        w_seed_acquisition_retry_apply(&limit_storage, limit_status,
                                       &limit_result);
    CHECK(limit_outcome.status == W_SEED_ACQUISITION_RETRY_CAPACITY &&
          limit_outcome.action == W_SEED_ACQUISITION_RETRY_TERMINAL &&
          limit_outcome.detail ==
              W_SEED_ACQUISITION_RETRY_DETAIL_NON_RESIZABLE &&
          limit_storage.staging_capacity[0u] == 0u);
    w_seed_acquisition_storage_destroy(&limit_storage);
  }

  pipeline_fixture fixture;
  fake_backend backend = {0};
  uint8_t staging[256] = {0};
  uint8_t revalidation[256] = {0};
  uint8_t published[256] = {0};
  backend.root_text = root_only;
  initialize_fixture(&fixture, &backend);
  fixture.requests[0u].staging_bytes = staging;
  fixture.requests[0u].staging_capacity = sizeof(staging);
  fixture.requests[0u].revalidation_bytes = revalidation;
  fixture.requests[0u].revalidation_capacity = sizeof(revalidation);
  fixture.requests[0u].bytes = published;
  fixture.requests[0u].byte_capacity = sizeof(published);
  w_seed_ephemeral_driver_result node_result;
  const w_seed_ephemeral_driver_status node_status =
      w_seed_ephemeral_driver_run(&fixture.driver_input,
                                  &fixture.driver_scratch,
                                  &fixture.driver_output, &node_result);
  CHECK(node_status == W_SEED_EPHEMERAL_DRIVER_CAPACITY &&
        node_result.capacity_field ==
            W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_NODE);
  w_seed_acquisition_storage storage = {0};
  CHECK(w_seed_acquisition_storage_init(&storage));
  w_seed_ephemeral_driver_result forged_node = node_result;
  forged_node.parser_status = W_SEED_PARSE_COMPLETE;
  CHECK(w_seed_acquisition_retry_apply(&storage, node_status, &forged_node)
            .status == W_SEED_ACQUISITION_RETRY_INVALID);
  CHECK(w_seed_acquisition_retry_apply(&storage, node_status, &node_result)
            .action == W_SEED_ACQUISITION_RETRY_RETRY);
  w_seed_acquisition_storage_destroy(&storage);

  backend = (fake_backend){0};
  backend.root_text = root_only;
  initialize_fixture(&fixture, &backend);
  fixture.graph_scratch.node_capacity = 0u;
  storage = (w_seed_acquisition_storage){0};
  CHECK(w_seed_acquisition_storage_init(&storage));
  bool saw_zero_graph = false;
  for (size_t attempt = 0u;
       attempt < (size_t)W_SEED_ACQUISITION_MAX_ATTEMPTS; attempt += 1u) {
    CHECK(w_seed_acquisition_storage_bind_driver(
        &storage, &fixture.driver_scratch));
    w_seed_ephemeral_driver_result zero_graph_result;
    const w_seed_ephemeral_driver_status zero_graph_status =
        w_seed_ephemeral_driver_run(
            &fixture.driver_input, &fixture.driver_scratch,
            &fixture.driver_output, &zero_graph_result);
    CHECK(zero_graph_status == W_SEED_EPHEMERAL_DRIVER_CAPACITY);
    const w_seed_acquisition_retry_outcome outcome =
        w_seed_acquisition_retry_apply(&storage, zero_graph_status,
                                       &zero_graph_result);
    if (zero_graph_result.failure ==
        W_SEED_EPHEMERAL_DRIVER_FAILURE_GRAPH) {
      CHECK(zero_graph_result.required_capacity == 0u &&
            zero_graph_result.graph_result.required.sources == 0u &&
            outcome.status == W_SEED_ACQUISITION_RETRY_CAPACITY &&
            outcome.detail ==
                W_SEED_ACQUISITION_RETRY_DETAIL_NON_RESIZABLE);
      w_seed_ephemeral_driver_result forged_zero = zero_graph_result;
      forged_zero.graph_result.status = W_SEED_EPHEMERAL_GRAPH_OK;
      CHECK(w_seed_acquisition_retry_apply(&storage, zero_graph_status,
                                           &forged_zero)
                .status == W_SEED_ACQUISITION_RETRY_INVALID);
      forged_zero = zero_graph_result;
      forged_zero.document_index = 0u;
      CHECK(w_seed_acquisition_retry_apply(&storage, zero_graph_status,
                                           &forged_zero)
                .status == W_SEED_ACQUISITION_RETRY_INVALID);
      saw_zero_graph = true;
      break;
    }
    CHECK(outcome.status == W_SEED_ACQUISITION_RETRY_OK &&
          outcome.action == W_SEED_ACQUISITION_RETRY_RETRY);
  }
  CHECK(saw_zero_graph);
  w_seed_acquisition_storage_destroy(&storage);

  static const char root[] =
      "module app;\nimport { value } from child\n"
      "fn use(): i64 { return value() }\n";
  static const char child[] =
      "module child;\nexport fn value(): i64 { return 42 }\n";
  backend = (fake_backend){0};
  backend.root_text = root;
  backend.files[0] = (fake_file){"child.w", child};
  backend.file_count = 1u;
  initialize_fixture(&fixture, &backend);
  fixture.driver_input.max_edges = 0u;
  storage = (w_seed_acquisition_storage){0};
  CHECK(w_seed_acquisition_storage_init(&storage));
  bool saw_provider = false;
  bool saw_node = false;
  bool saw_graph = false;
  for (size_t attempt = 0u;
       attempt < (size_t)W_SEED_ACQUISITION_MAX_ATTEMPTS; attempt += 1u) {
    CHECK(w_seed_acquisition_storage_bind_driver(
        &storage, &fixture.driver_scratch));
    w_seed_ephemeral_driver_result result;
    const w_seed_ephemeral_driver_status status =
        w_seed_ephemeral_driver_run(&fixture.driver_input,
                                    &fixture.driver_scratch,
                                    &fixture.driver_output, &result);
    CHECK(status == W_SEED_EPHEMERAL_DRIVER_CAPACITY);
    if (result.failure == W_SEED_EPHEMERAL_DRIVER_FAILURE_PROVIDER)
      saw_provider = true;
    if (result.capacity_field ==
        W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_NODE)
      saw_node = true;
    const w_seed_acquisition_retry_outcome outcome =
        w_seed_acquisition_retry_apply(&storage, status, &result);
    if (result.failure == W_SEED_EPHEMERAL_DRIVER_FAILURE_GRAPH) {
      CHECK(result.capacity_field ==
                W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_NONE &&
            result.required_capacity != 0u &&
            outcome.status == W_SEED_ACQUISITION_RETRY_CAPACITY &&
            outcome.action == W_SEED_ACQUISITION_RETRY_TERMINAL &&
            outcome.detail ==
                W_SEED_ACQUISITION_RETRY_DETAIL_NON_RESIZABLE);
      w_seed_ephemeral_driver_result forged_graph = result;
      forged_graph.graph_result.failure =
          W_SEED_EPHEMERAL_GRAPH_FAILURE_NONE;
      CHECK(w_seed_acquisition_retry_apply(&storage, status, &forged_graph)
                .status == W_SEED_ACQUISITION_RETRY_INVALID);
      saw_graph = true;
      break;
    }
    CHECK(outcome.status == W_SEED_ACQUISITION_RETRY_OK &&
          outcome.action == W_SEED_ACQUISITION_RETRY_RETRY);
  }
  CHECK(saw_provider && saw_node && saw_graph);
  w_seed_acquisition_storage_destroy(&storage);

  backend = (fake_backend){0};
  backend.root_text = root;
  backend.files[0] = (fake_file){"child.w", child};
  backend.file_count = 1u;
  initialize_fixture(&fixture, &backend);
  fixture.driver_output.graph.edge_capacity = 0u;
  storage = (w_seed_acquisition_storage){0};
  CHECK(w_seed_acquisition_storage_init(&storage));
  bool saw_fixed_graph = false;
  for (size_t attempt = 0u;
       attempt < (size_t)W_SEED_ACQUISITION_MAX_ATTEMPTS; attempt += 1u) {
    CHECK(w_seed_acquisition_storage_bind_driver(
        &storage, &fixture.driver_scratch));
    w_seed_ephemeral_driver_result result;
    const w_seed_ephemeral_driver_status status =
        w_seed_ephemeral_driver_run(&fixture.driver_input,
                                    &fixture.driver_scratch,
                                    &fixture.driver_output, &result);
    CHECK(status == W_SEED_EPHEMERAL_DRIVER_CAPACITY);
    const w_seed_acquisition_retry_outcome outcome =
        w_seed_acquisition_retry_apply(&storage, status, &result);
    if (result.capacity_field ==
        W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_GRAPH_EDGE) {
      CHECK(result.failure == W_SEED_EPHEMERAL_DRIVER_FAILURE_STORAGE &&
            result.required_capacity == result.graph_result.required.edges &&
            result.required_capacity != 0u &&
            outcome.status == W_SEED_ACQUISITION_RETRY_CAPACITY &&
            outcome.detail ==
                W_SEED_ACQUISITION_RETRY_DETAIL_NON_RESIZABLE);
      w_seed_ephemeral_driver_result forged = result;
      forged.required_capacity += 1u;
      CHECK(w_seed_acquisition_retry_apply(&storage, status, &forged)
                .status == W_SEED_ACQUISITION_RETRY_INVALID);
      saw_fixed_graph = true;
      break;
    }
    CHECK(outcome.status == W_SEED_ACQUISITION_RETRY_OK &&
          outcome.action == W_SEED_ACQUISITION_RETRY_RETRY);
  }
  CHECK(saw_fixed_graph);
  w_seed_acquisition_storage_destroy(&storage);
  return true;
}

static bool test_real_fixed_storage_envelopes(void) {
  static const char root_only[] = "module app;\n";
  pipeline_fixture fixture;
  fake_backend backend = {0};
  backend.root_text = root_only;
  initialize_fixture(&fixture, &backend);
  fixture.slots[0u].source_id_capacity = 1u;
  w_seed_ephemeral_driver_result result;
  w_seed_ephemeral_driver_status status = w_seed_ephemeral_driver_run(
      &fixture.driver_input, &fixture.driver_scratch, &fixture.driver_output,
      &result);
  CHECK(status == W_SEED_EPHEMERAL_DRIVER_CAPACITY &&
        result.capacity_field ==
            W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_SOURCE_ID &&
        result.phase == W_SEED_EPHEMERAL_DRIVER_PHASE_VALIDATE);
  w_seed_acquisition_storage storage = {0};
  CHECK(w_seed_acquisition_storage_init(&storage));
  CHECK(w_seed_acquisition_retry_apply(&storage, status, &result).status ==
        W_SEED_ACQUISITION_RETRY_CAPACITY);
  w_seed_ephemeral_driver_result forged = result;
  forged.origin_index = 0u;
  CHECK(w_seed_acquisition_retry_apply(&storage, status, &forged).status ==
        W_SEED_ACQUISITION_RETRY_INVALID);
  forged = result;
  forged.span.end_byte = 1u;
  CHECK(w_seed_acquisition_retry_apply(&storage, status, &forged).status ==
        W_SEED_ACQUISITION_RETRY_INVALID);
  w_seed_acquisition_storage_destroy(&storage);

  backend = (fake_backend){0};
  backend.root_text = root_only;
  initialize_fixture(&fixture, &backend);
  fixture.slots[0u].module_id_capacity = 1u;
  storage = (w_seed_acquisition_storage){0};
  CHECK(w_seed_acquisition_storage_init(&storage));
  bool saw_module_id = false;
  for (size_t attempt = 0u;
       attempt < (size_t)W_SEED_ACQUISITION_MAX_ATTEMPTS; attempt += 1u) {
    CHECK(w_seed_acquisition_storage_bind_driver(
        &storage, &fixture.driver_scratch));
    status = w_seed_ephemeral_driver_run(
        &fixture.driver_input, &fixture.driver_scratch, &fixture.driver_output,
        &result);
    CHECK(status == W_SEED_EPHEMERAL_DRIVER_CAPACITY);
    const w_seed_acquisition_retry_outcome outcome =
        w_seed_acquisition_retry_apply(&storage, status, &result);
    if (result.capacity_field ==
        W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_MODULE_ID) {
      CHECK(result.origin_index == SIZE_MAX &&
            result.span.start_byte ==
                result.scan_result.module_header_name_span.start_byte &&
            result.span.end_byte ==
                result.scan_result.module_header_name_span.end_byte &&
            outcome.status == W_SEED_ACQUISITION_RETRY_CAPACITY);
      forged = result;
      forged.origin_index = 0u;
      CHECK(w_seed_acquisition_retry_apply(&storage, status, &forged)
                .status == W_SEED_ACQUISITION_RETRY_INVALID);
      forged = result;
      forged.span.end_byte += 1u;
      CHECK(w_seed_acquisition_retry_apply(&storage, status, &forged)
                .status == W_SEED_ACQUISITION_RETRY_INVALID);
      saw_module_id = true;
      break;
    }
    CHECK(outcome.action == W_SEED_ACQUISITION_RETRY_RETRY);
  }
  CHECK(saw_module_id);
  w_seed_acquisition_storage_destroy(&storage);

  static const char root_import[] = "module app;\nimport child;\n";
  backend = (fake_backend){0};
  backend.root_text = root_import;
  backend.files[0] = (fake_file){"child.w", "module child;\n"};
  backend.file_count = 1u;
  initialize_fixture(&fixture, &backend);
  fixture.driver_scratch.slot_capacity = 1u;
  fixture.driver_scratch.request_capacity = 1u;
  storage = (w_seed_acquisition_storage){0};
  CHECK(w_seed_acquisition_storage_init(&storage));
  bool saw_slot = false;
  for (size_t attempt = 0u;
       attempt < (size_t)W_SEED_ACQUISITION_MAX_ATTEMPTS; attempt += 1u) {
    CHECK(w_seed_acquisition_storage_bind_driver(
        &storage, &fixture.driver_scratch));
    status = w_seed_ephemeral_driver_run(
        &fixture.driver_input, &fixture.driver_scratch, &fixture.driver_output,
        &result);
    CHECK(status == W_SEED_EPHEMERAL_DRIVER_CAPACITY);
    const w_seed_acquisition_retry_outcome outcome =
        w_seed_acquisition_retry_apply(&storage, status, &result);
    if (result.capacity_field ==
        W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_SLOT) {
      CHECK(result.origin_index < result.scan_result.written &&
            result.span.start_byte < result.span.end_byte &&
            outcome.status == W_SEED_ACQUISITION_RETRY_CAPACITY);
      forged = result;
      forged.origin_index = result.scan_result.written;
      CHECK(w_seed_acquisition_retry_apply(&storage, status, &forged)
                .status == W_SEED_ACQUISITION_RETRY_INVALID);
      forged = result;
      forged.span.end_byte = forged.span.start_byte;
      CHECK(w_seed_acquisition_retry_apply(&storage, status, &forged)
                .status == W_SEED_ACQUISITION_RETRY_INVALID);
      saw_slot = true;
      break;
    }
    CHECK(outcome.action == W_SEED_ACQUISITION_RETRY_RETRY);
  }
  CHECK(saw_slot);
  w_seed_acquisition_storage_destroy(&storage);
  return true;
}

int main(void) {
  if (!test_root_child_clean_retries() ||
      !test_child_diagnostic_is_deterministic() ||
      !test_failures_do_not_publish_json() ||
      !test_backend_unsupported_and_frontend_capacity() ||
      !test_allocator_fault_and_input_fault() ||
      !test_real_driver_acquisition_envelopes() ||
      !test_real_fixed_storage_envelopes())
    return 1;
  (void)puts("w_seed_check_pipeline_tests: ok");
  return 0;
}
