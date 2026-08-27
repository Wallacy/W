#include "check.h"

#include "check_host.h"
#include "check_pipeline.h"
#include "check_storage.h"

#include "w_seed_ephemeral_graph.h"
#include "w_seed_frontend.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* The public command is a bounded adapter around the caller-owned CHK6 and
 * CHK7 composition. These arrays are the fixed frontend and graph scratch
 * profile. Source bytes, CST nodes, and D0 JSON are owned by the adaptive
 * check storage instead of consuming one large static arena. */
enum {
  CHECK_LEXER_FRAMES = 2048,
  CHECK_TOKENS = 32768,
  CHECK_PARSE_FRAMES = 16384,
  CHECK_ISSUES = 4096,
  CHECK_MODULES = 64,
  CHECK_IMPORTS = 4096,
  CHECK_IMPORT_ITEMS = 4096,
  CHECK_STRUCTS = 4096,
  CHECK_GENERIC_PARAMETERS = 65536,
  CHECK_GENERIC_APPLICATIONS = 65536,
  CHECK_GENERIC_ARGUMENTS = 262144,
  CHECK_TYPED_CONST_EXPRESSIONS = 262144,
  CHECK_CONST_VALUES = 262144,
  CHECK_CONST_ELEMENTS = 262144,
  CHECK_CONST_BYTES = 8 * 1024 * 1024,
  CHECK_ENUMS = 4096,
  CHECK_ENUM_CASES = 16384,
  CHECK_ENUM_CASE_PARAMETERS = 32768,
  CHECK_ENUM_SUBSET_MEMBERS = 65536,
  CHECK_FIELDS = 16384,
  CHECK_DECLARATIONS = 4096,
  CHECK_CONST_DECLARATIONS = 4096,
  CHECK_TYPES = 32768,
  CHECK_FUNCTIONS = 4096,
  CHECK_PARAMETERS = 32768,
  CHECK_ENTRIES = 4096,
  CHECK_STATEMENTS = 65536,
  CHECK_EXPRESSIONS = 262144,
  CHECK_ARGUMENTS = 65536,
  CHECK_SWITCH_ARMS = 65536,
  CHECK_ENUM_MEMBERSHIP_CASES = 262144,
  CHECK_SYMBOLS = 131072,
  CHECK_FACTS = 131072,
  CHECK_DIAGNOSTICS = 65536,
  CHECK_RECEIPT = 8 * 1024 * 1024,
  CHECK_SOURCES = W_SEED_CHECK_STORAGE_MAX_SOURCES,
  CHECK_EDGES = W_SEED_EPHEMERAL_GRAPH_MAX_EDGES,
  CHECK_PATH_BYTES = W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES,
  CHECK_TOKEN_BYTES = W_SEED_EPHEMERAL_PROVIDER_MAX_TOKEN_BYTES,
  CHECK_DISPLAY_PATH_BYTES =
      (2 * W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES) + 2,
};

typedef struct {
  char provider_id[CHECK_TOKEN_BYTES];
  char root_token[CHECK_TOKEN_BYTES];
  char source_provider_owner_token[CHECK_TOKEN_BYTES];
  char canonical_token[CHECK_TOKEN_BYTES];
} check_token_set;

static w_seed_lexer_frame lexer_frames[CHECK_LEXER_FRAMES];
static w_seed_parse_token tokens[CHECK_TOKENS];
static w_seed_parse_frame parse_frames[CHECK_PARSE_FRAMES];
static w_seed_parse_issue issues[CHECK_ISSUES];
static w_seed_ephemeral_driver_slot driver_slots[CHECK_SOURCES];
static w_seed_ephemeral_provider_request requests[CHECK_SOURCES];
static char root_source_id_storage[CHECK_PATH_BYTES];
static char source_ids[CHECK_SOURCES][CHECK_PATH_BYTES];
static char module_ids[CHECK_SOURCES][CHECK_PATH_BYTES];
static check_token_set token_sets[CHECK_SOURCES];
static check_token_set revalidation_token_sets[CHECK_SOURCES];
static w_seed_module_origin driver_origins[CHECK_IMPORTS];
static w_seed_frontend_document candidate_documents[CHECK_SOURCES];
static w_seed_ephemeral_graph_provider_facts candidate_facts[CHECK_SOURCES];

static w_seed_ephemeral_graph_scratch_node graph_nodes[CHECK_SOURCES];
static w_seed_ephemeral_graph_scratch_edge graph_edges[CHECK_EDGES];
static size_t sorted_nodes[CHECK_SOURCES];
static size_t node_ordinals[CHECK_SOURCES];
static size_t sorted_edges[CHECK_EDGES];
static size_t sorted_resolved_edges[CHECK_EDGES];
static w_seed_module_origin graph_origins[CHECK_EDGES];
static uint32_t indegree[CHECK_SOURCES];
static uint32_t queue[CHECK_SOURCES];
static uint32_t depths[CHECK_SOURCES];

static w_seed_ephemeral_graph_scratch graph_scratch;
static w_seed_ephemeral_graph_inventory_item inventory[CHECK_SOURCES];
static w_seed_ephemeral_graph_edge graph_output_edges[CHECK_EDGES];
static uint32_t document_order[CHECK_SOURCES];
static w_seed_frontend_resolved_import resolved_imports[CHECK_EDGES];
static w_seed_frontend_document documents[CHECK_SOURCES];

static w_seed_frontend_module modules[CHECK_MODULES];
static w_seed_frontend_import imports[CHECK_IMPORTS];
static w_seed_frontend_import_item import_items[CHECK_IMPORT_ITEMS];
static w_seed_frontend_struct structs[CHECK_STRUCTS];
static w_seed_frontend_generic_parameter
    generic_parameters[CHECK_GENERIC_PARAMETERS];
static w_seed_frontend_generic_application
    generic_applications[CHECK_GENERIC_APPLICATIONS];
static w_seed_frontend_generic_argument
    generic_arguments[CHECK_GENERIC_ARGUMENTS];
static w_seed_frontend_typed_const_expression
    typed_const_expressions[CHECK_TYPED_CONST_EXPRESSIONS];
static w_seed_frontend_const_value const_values[CHECK_CONST_VALUES];
static w_seed_frontend_const_element const_elements[CHECK_CONST_ELEMENTS];
static uint8_t const_bytes[CHECK_CONST_BYTES];
static w_seed_frontend_enum enums[CHECK_ENUMS];
static w_seed_frontend_enum_case enum_cases[CHECK_ENUM_CASES];
static w_seed_frontend_enum_case_parameter
    enum_case_parameters[CHECK_ENUM_CASE_PARAMETERS];
static w_seed_frontend_enum_subset_member
    enum_subset_members[CHECK_ENUM_SUBSET_MEMBERS];
static w_seed_frontend_field fields[CHECK_FIELDS];
static w_seed_frontend_type_declaration type_declarations[CHECK_DECLARATIONS];
static w_seed_frontend_alias aliases[CHECK_DECLARATIONS];
static w_seed_frontend_const_declaration
    const_declarations[CHECK_CONST_DECLARATIONS];
static w_seed_frontend_type types[CHECK_TYPES];
static w_seed_frontend_function functions[CHECK_FUNCTIONS];
static w_seed_frontend_parameter parameters[CHECK_PARAMETERS];
static w_seed_frontend_entry entries[CHECK_ENTRIES];
static w_seed_frontend_statement statements[CHECK_STATEMENTS];
static w_seed_frontend_expression expressions[CHECK_EXPRESSIONS];
static w_seed_frontend_argument arguments[CHECK_ARGUMENTS];
static w_seed_frontend_switch_arm switch_arms[CHECK_SWITCH_ARMS];
static w_seed_frontend_enum_membership_case
    enum_membership_cases[CHECK_ENUM_MEMBERSHIP_CASES];
static w_seed_frontend_symbol symbols[CHECK_SYMBOLS];
static w_seed_frontend_fact facts[CHECK_FACTS];
static w_seed_frontend_diagnostic diagnostics[CHECK_DIAGNOSTICS];
static uint8_t receipt[CHECK_RECEIPT];
static char display_paths[CHECK_SOURCES][CHECK_DISPLAY_PATH_BYTES];

static void reset_driver_storage(void) {
  (void)memset(driver_slots, 0, sizeof(driver_slots));
  (void)memset(requests, 0, sizeof(requests));
  (void)memset(&graph_scratch, 0, sizeof(graph_scratch));
  for (size_t index = 0u; index < (size_t)CHECK_SOURCES; index += 1u) {
    driver_slots[index].source_id_storage = source_ids[index];
    driver_slots[index].source_id_capacity = CHECK_PATH_BYTES;
    driver_slots[index].module_id_storage = module_ids[index];
    driver_slots[index].module_id_capacity = CHECK_PATH_BYTES;
    requests[index].tokens = (w_seed_ephemeral_provider_token_buffers){
        token_sets[index].provider_id, CHECK_TOKEN_BYTES,
        token_sets[index].root_token, CHECK_TOKEN_BYTES,
        token_sets[index].source_provider_owner_token, CHECK_TOKEN_BYTES,
        token_sets[index].canonical_token, CHECK_TOKEN_BYTES};
    requests[index].revalidation_tokens =
        (w_seed_ephemeral_provider_token_buffers){
            revalidation_token_sets[index].provider_id, CHECK_TOKEN_BYTES,
            revalidation_token_sets[index].root_token, CHECK_TOKEN_BYTES,
            revalidation_token_sets[index].source_provider_owner_token,
            CHECK_TOKEN_BYTES, revalidation_token_sets[index].canonical_token,
            CHECK_TOKEN_BYTES};
  }
  graph_scratch = (w_seed_ephemeral_graph_scratch){
      graph_nodes, CHECK_SOURCES, graph_edges, CHECK_EDGES, sorted_nodes,
      CHECK_SOURCES, node_ordinals, CHECK_SOURCES, sorted_edges, CHECK_EDGES,
      sorted_resolved_edges, CHECK_EDGES, graph_origins, CHECK_EDGES, indegree,
      CHECK_SOURCES, queue, CHECK_SOURCES, depths, CHECK_SOURCES};
}

static w_seed_ephemeral_driver_scratch driver_scratch_value(void) {
  return (w_seed_ephemeral_driver_scratch){
      driver_slots,
      CHECK_SOURCES,
      requests,
      CHECK_SOURCES,
      lexer_frames,
      CHECK_LEXER_FRAMES,
      tokens,
      CHECK_TOKENS,
      parse_frames,
      CHECK_PARSE_FRAMES,
      issues,
      CHECK_ISSUES,
      driver_origins,
      CHECK_IMPORTS,
      candidate_documents,
      CHECK_SOURCES,
      candidate_facts,
      CHECK_SOURCES,
      &graph_scratch};
}

static w_seed_ephemeral_driver_output driver_output_value(void) {
  return (w_seed_ephemeral_driver_output){
      {inventory, CHECK_SOURCES, graph_output_edges, CHECK_EDGES,
       document_order, CHECK_SOURCES, resolved_imports, CHECK_EDGES},
      documents,
      CHECK_SOURCES,
      0u};
}

static w_seed_frontend_output frontend_output_value(void) {
  return (w_seed_frontend_output){
      .modules = modules,
      .module_capacity = CHECK_MODULES,
      .imports = imports,
      .import_capacity = CHECK_IMPORTS,
      .import_items = import_items,
      .import_item_capacity = CHECK_IMPORT_ITEMS,
      .structs = structs,
      .struct_capacity = CHECK_STRUCTS,
      .generic_parameters = generic_parameters,
      .generic_parameter_capacity = CHECK_GENERIC_PARAMETERS,
      .generic_applications = generic_applications,
      .generic_application_capacity = CHECK_GENERIC_APPLICATIONS,
      .generic_arguments = generic_arguments,
      .generic_argument_capacity = CHECK_GENERIC_ARGUMENTS,
      .typed_const_expressions = typed_const_expressions,
      .typed_const_expression_capacity = CHECK_TYPED_CONST_EXPRESSIONS,
      .const_values = const_values,
      .const_value_capacity = CHECK_CONST_VALUES,
      .const_elements = const_elements,
      .const_element_capacity = CHECK_CONST_ELEMENTS,
      .const_bytes = const_bytes,
      .const_bytes_capacity = CHECK_CONST_BYTES,
      .enums = enums,
      .enum_capacity = CHECK_ENUMS,
      .enum_cases = enum_cases,
      .enum_case_capacity = CHECK_ENUM_CASES,
      .enum_case_parameters = enum_case_parameters,
      .enum_case_parameter_capacity = CHECK_ENUM_CASE_PARAMETERS,
      .enum_subset_members = enum_subset_members,
      .enum_subset_member_capacity = CHECK_ENUM_SUBSET_MEMBERS,
      .fields = fields,
      .field_capacity = CHECK_FIELDS,
      .type_declarations = type_declarations,
      .type_declaration_capacity = CHECK_DECLARATIONS,
      .aliases = aliases,
      .alias_capacity = CHECK_DECLARATIONS,
      .const_declarations = const_declarations,
      .const_declaration_capacity = CHECK_CONST_DECLARATIONS,
      .types = types,
      .type_capacity = CHECK_TYPES,
      .functions = functions,
      .function_capacity = CHECK_FUNCTIONS,
      .parameters = parameters,
      .parameter_capacity = CHECK_PARAMETERS,
      .arguments = arguments,
      .argument_capacity = CHECK_ARGUMENTS,
      .switch_arms = switch_arms,
      .switch_arm_capacity = CHECK_SWITCH_ARMS,
      .enum_membership_cases = enum_membership_cases,
      .enum_membership_case_capacity = CHECK_ENUM_MEMBERSHIP_CASES,
      .entries = entries,
      .entry_capacity = CHECK_ENTRIES,
      .statements = statements,
      .statement_capacity = CHECK_STATEMENTS,
      .expressions = expressions,
      .expression_capacity = CHECK_EXPRESSIONS,
      .symbols = symbols,
      .symbol_capacity = CHECK_SYMBOLS,
      .facts = facts,
      .fact_capacity = CHECK_FACTS,
      .diagnostics = diagnostics,
      .diagnostic_capacity = CHECK_DIAGNOSTICS,
      .receipt = receipt,
      .receipt_capacity = CHECK_RECEIPT};
}

static bool text_view_valid(w_seed_frontend_text text) {
  if (text.length != 0u && text.data == NULL) return false;
  if (text.data == NULL) return text.length == 0u;
  for (size_t index = 0u; index < text.length; index += 1u)
    if ((uint8_t)text.data[index] == 0u) return false;
  return true;
}

static bool report_failure(const char *path, const char *reason) {
  if (reason == NULL) return false;
  const int result = path == NULL
                         ? fprintf(stderr, "w_seed_check: %s\n", reason)
                         : fprintf(stderr, "%s: %s\n", path, reason);
  return result >= 0;
}

static bool report_text(FILE *stream, w_seed_frontend_text text,
                        size_t limit) {
  if (stream == NULL || (text.length != 0u && text.data == NULL)) return false;
  const size_t count = text.length < limit ? text.length : limit;
  for (size_t index = 0u; index < count; index += 1u) {
    const unsigned char byte = (unsigned char)text.data[index];
    int output = (int)byte;
    if (byte == '\n' || byte == '\r' || byte == '\t')
      output = (int)' ';
    else if (byte < 0x20u)
      output = (int)'?';
    if (fputc(output, stream) == EOF) return false;
  }
  if (count != text.length && fputs("...", stream) == EOF) return false;
  return true;
}

static bool display_paths_prepare(
    const char *path, size_t path_length, w_seed_frontend_text root_source_id,
    const w_seed_frontend_document *documents_value, size_t document_count) {
  const w_seed_frontend_text root_logical =
      documents_value == NULL ? (w_seed_frontend_text){NULL, 0u}
                               : documents_value[0].logical_source_id;
  if (path == NULL || documents_value == NULL || document_count == 0u ||
      document_count > (size_t)CHECK_SOURCES ||
      !text_view_valid(root_source_id) || root_source_id.length == 0u ||
      root_source_id.length > (size_t)CHECK_PATH_BYTES ||
      !text_view_valid(root_logical) || root_logical.length == 0u ||
      root_logical.length > (size_t)CHECK_PATH_BYTES) {
    return false;
  }
  if (root_source_id.data < path ||
      root_source_id.data > path + path_length) {
    return false;
  }
  const size_t parent_length = (size_t)(root_source_id.data - path);
  if (parent_length > path_length ||
      path_length >= (size_t)CHECK_DISPLAY_PATH_BYTES) {
    return false;
  }
  if (root_logical.length != root_source_id.length ||
      memcmp(root_logical.data, root_source_id.data,
             root_source_id.length) != 0) {
    return false;
  }
  (void)memcpy(display_paths[0], path, path_length);
  display_paths[0][path_length] = '\0';
  for (size_t index = 0u; index < document_count; index += 1u) {
    const w_seed_frontend_text logical =
        documents_value[index].logical_source_id;
    if (!text_view_valid(logical) || logical.length == 0u ||
        logical.length > (size_t)CHECK_PATH_BYTES) {
      return false;
    }
    if (index == 0u) continue;
    if (parent_length > (size_t)CHECK_DISPLAY_PATH_BYTES - 1u ||
        logical.length > (size_t)CHECK_DISPLAY_PATH_BYTES - 1u -
                              parent_length) {
      return false;
    }
    (void)memcpy(display_paths[index], path, parent_length);
    (void)memcpy(display_paths[index] + parent_length, logical.data,
                 logical.length);
    display_paths[index][parent_length + logical.length] = '\0';
  }
  return true;
}

static bool preflight_human(
    const char *path, size_t path_length, w_seed_frontend_text root_source_id,
    const w_seed_ephemeral_driver_output *driver_output,
    const w_seed_frontend_output *frontend_output,
    const w_seed_check_pipeline_result *pipeline_result) {
  if (driver_output == NULL || frontend_output == NULL ||
      pipeline_result == NULL || pipeline_result->status !=
                                     W_SEED_CHECK_PIPELINE_DIAGNOSTICS ||
      driver_output->document_count == 0u ||
      driver_output->document_count > driver_output->document_capacity ||
      driver_output->document_count > (size_t)CHECK_SOURCES ||
      pipeline_result->check_result.frontend_result.written.diagnostics >
          frontend_output->diagnostic_capacity) {
    return false;
  }
  if (!display_paths_prepare(path, path_length, root_source_id,
                             driver_output->documents,
                             driver_output->document_count)) {
    return false;
  }
  const size_t count =
      pipeline_result->check_result.frontend_result.written.diagnostics;
  if (count != 0u && frontend_output->diagnostics == NULL) return false;
  for (size_t index = 0u; index < count; index += 1u) {
    const w_seed_frontend_diagnostic *diagnostic =
        &frontend_output->diagnostics[index];
    if (diagnostic->document_index >= driver_output->document_count ||
        !text_view_valid(diagnostic->code) || diagnostic->code.length == 0u ||
        !text_view_valid(diagnostic->actual) ||
        !text_view_valid(diagnostic->expected) ||
        !text_view_valid(diagnostic->declaration) ||
        !text_view_valid(diagnostic->label) ||
        !text_view_valid(diagnostic->accepted_forms)) {
      return false;
    }
    const w_seed_frontend_document *document =
        &driver_output->documents[diagnostic->document_index];
    if (document->source == NULL ||
        !w_seed_source_validate_span(document->source, diagnostic->primary,
                                     NULL)) {
      return false;
    }
    w_seed_source_point point;
    if (!w_seed_source_offset_to_point(document->source,
                                       diagnostic->primary.start_byte, &point,
                                       NULL)) {
      return false;
    }
  }
  return true;
}

static bool render_human_diagnostics(
    const w_seed_ephemeral_driver_output *driver_output,
    const w_seed_frontend_output *frontend_output,
    const w_seed_check_pipeline_result *pipeline_result) {
  if (driver_output == NULL || frontend_output == NULL ||
      pipeline_result == NULL) {
    return false;
  }
  const size_t count =
      pipeline_result->check_result.frontend_result.written.diagnostics;
  for (size_t index = 0u; index < count; index += 1u) {
    const w_seed_frontend_diagnostic *diagnostic =
        &frontend_output->diagnostics[index];
    const w_seed_frontend_document *document =
        &driver_output->documents[diagnostic->document_index];
    w_seed_source_point point;
    if (!w_seed_source_offset_to_point(document->source,
                                       diagnostic->primary.start_byte, &point,
                                       NULL)) {
      return false;
    }
    if (fprintf(stderr, "%s:%" PRIuMAX ":%" PRIuMAX ":",
                display_paths[diagnostic->document_index],
                (uintmax_t)(point.line + 1u),
                (uintmax_t)(point.byte_column + 1u)) < 0 ||
        !report_text(stderr, diagnostic->code, SIZE_MAX) ||
        fputs(": actual=", stderr) == EOF ||
        !report_text(stderr, diagnostic->actual, 160u) ||
        fputs(" expected=", stderr) == EOF ||
        !report_text(stderr, diagnostic->expected, 160u) ||
        fputc('\n', stderr) == EOF) {
      return false;
    }
  }
  return true;
}

static bool emit_json_diagnostics(const w_seed_check_storage *storage,
                                  size_t length) {
  if (storage == NULL || length == 0u || storage->json_final == NULL ||
      length > storage->json_final_capacity) {
    return false;
  }
  /* The pipeline has already staged and committed every record. This is the
   * sole public write for a successful JSON diagnostic result. */
  return fwrite(storage->json_final, 1u, length, stdout) == length;
}

static int pipeline_exit_code(w_seed_check_pipeline_status status) {
  switch (status) {
    case W_SEED_CHECK_PIPELINE_CLEAN:
      return 0;
    case W_SEED_CHECK_PIPELINE_DIAGNOSTICS:
      return 1;
    case W_SEED_CHECK_PIPELINE_FAULT:
      return 3;
    case W_SEED_CHECK_PIPELINE_CAPACITY:
    case W_SEED_CHECK_PIPELINE_UNSUPPORTED:
    case W_SEED_CHECK_PIPELINE_INVALID:
    case W_SEED_CHECK_PIPELINE_IO:
    default:
      return 2;
  }
}

static const char *pipeline_failure_reason(
    w_seed_check_pipeline_status status) {
  switch (status) {
    case W_SEED_CHECK_PIPELINE_CAPACITY:
      return "source check capacity exceeded";
    case W_SEED_CHECK_PIPELINE_UNSUPPORTED:
      return "source check uses unsupported input";
    case W_SEED_CHECK_PIPELINE_INVALID:
      return "source check input is invalid";
    case W_SEED_CHECK_PIPELINE_IO:
      return "source check I/O failed";
    case W_SEED_CHECK_PIPELINE_FAULT:
      return "internal check pipeline fault";
    case W_SEED_CHECK_PIPELINE_CLEAN:
    case W_SEED_CHECK_PIPELINE_DIAGNOSTICS:
    default:
      return "source check failed";
  }
}

int w_seed_check_run(const char *path, bool json) {
  if (path == NULL || path[0] == '\0') {
    return report_failure(path, "path is empty") ? 2 : 3;
  }
  const size_t path_length = strlen(path);
  w_seed_frontend_text root_source_id;
  const w_seed_check_host_status source_id_status =
      w_seed_check_root_source_id(path, path_length, &root_source_id);
  if (source_id_status != W_SEED_CHECK_HOST_OK) {
    const char *reason = source_id_status == W_SEED_CHECK_HOST_UNSUPPORTED
                             ? "root basename uses unsupported Unicode"
                             : "path is not a valid W source path";
    return report_failure(path, reason) ? 2 : 3;
  }

  w_seed_check_host host = {0};
  w_seed_check_storage storage = {0};
  bool host_initialized = false;
  bool storage_initialized = false;
  int exit_code = 3;
  const char *failure_reason = NULL;
  w_seed_ephemeral_driver_scratch driver_scratch;
  w_seed_ephemeral_driver_output driver_output;
  w_seed_frontend_output frontend_output;
  w_seed_check_pipeline_result pipeline_result;

  if (w_seed_check_host_init(&host) != W_SEED_CHECK_HOST_OK) {
    failure_reason = "host initialization failed";
    goto cleanup;
  }
  host_initialized = true;
  if (!w_seed_check_storage_init(&storage)) {
    failure_reason = "check storage initialization failed";
    goto cleanup;
  }
  storage_initialized = true;
  const w_seed_check_host_status host_status =
      w_seed_check_host_open(&host, &host.backend);
  if (host_status != W_SEED_CHECK_HOST_OK) {
    failure_reason = host_status == W_SEED_CHECK_HOST_UNSUPPORTED
                         ? "host provider is unsupported"
                         : "cannot open the check host";
    exit_code = 2;
    goto cleanup;
  }

  reset_driver_storage();
  (void)memcpy(root_source_id_storage, root_source_id.data,
               root_source_id.length);
  const w_seed_frontend_text driver_root_source_id =
      (w_seed_frontend_text){root_source_id_storage, root_source_id.length};
  driver_scratch = driver_scratch_value();
  driver_output = driver_output_value();
  frontend_output = frontend_output_value();
  const w_seed_ephemeral_driver_input driver_input = {
      {(const uint8_t *)path, path_length},
      driver_root_source_id,
      {CHECK_SOURCES, W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCE_BYTES,
       W_SEED_EPHEMERAL_PROVIDER_MAX_TOTAL_SOURCE_BYTES,
       W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES,
       W_SEED_EPHEMERAL_PROVIDER_MAX_TOKEN_BYTES},
      W_SEED_EPHEMERAL_GRAPH_MAX_EDGES,
      W_SEED_EPHEMERAL_GRAPH_MAX_DEPTH,
      {65536u, 256u},
      host.backend};
  const w_seed_check_pipeline_input pipeline_input = {
      &driver_input, &driver_scratch, &driver_output, &frontend_output,
      &storage, "D000001", 7u};
  const w_seed_check_pipeline_status pipeline_status =
      w_seed_check_pipeline_run(&pipeline_input, &pipeline_result);
  exit_code = pipeline_exit_code(pipeline_status);
  if (pipeline_status == W_SEED_CHECK_PIPELINE_CLEAN) goto cleanup;
  if (pipeline_status != W_SEED_CHECK_PIPELINE_DIAGNOSTICS) {
    failure_reason = pipeline_failure_reason(pipeline_status);
    goto cleanup;
  }
  if (json) {
    if (!emit_json_diagnostics(&storage, pipeline_result.json_length)) {
      exit_code = 3;
      goto cleanup;
    }
    goto cleanup;
  }
  if (!preflight_human(path, path_length, root_source_id, &driver_output,
                       &frontend_output, &pipeline_result)) {
    exit_code = 3;
    failure_reason = "cannot render diagnostic source location";
    goto cleanup;
  }
  if (!render_human_diagnostics(&driver_output, &frontend_output,
                                &pipeline_result)) {
    exit_code = 3;
    failure_reason = "cannot write diagnostic";
    goto cleanup;
  }

cleanup:
  if (storage_initialized) w_seed_check_storage_destroy(&storage);
  if (host_initialized) w_seed_check_host_close(&host);
  if (failure_reason != NULL && !report_failure(path, failure_reason))
    exit_code = 3;
  return exit_code;
}
