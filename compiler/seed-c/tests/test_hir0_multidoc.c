#include "w_seed_hir0.h"
#include "w_seed_native0.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Keep this test on the same private seam as test_hir0.c. The production
 * HIR object is supplied by w_seed_source, but including the implementation
 * gives the test the byte-level text predicates used for identity checks. */
#include "../src/w_seed_hir0.c"

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "hir0 multidoc check failed: %s (%s:%d)\n",     \
                    #condition, __FILE__, __LINE__);                         \
      return false;                                                            \
    }                                                                          \
  } while (0)

enum {
  TEST_DOCUMENTS = 2,
  TEST_SOURCE_BYTES = 2048,
  TEST_LEXER_FRAMES = 256,
  TEST_TOKENS = 1024,
  TEST_NODES = 2048,
  TEST_PARSE_FRAMES = 1024,
  TEST_ISSUES = 64,
  TEST_MODULES = 4,
  TEST_IMPORTS = 8,
  TEST_IMPORT_ITEMS = 8,
  TEST_STRUCTS = 4,
  TEST_FIELDS = 8,
  TEST_ENUMS = 4,
  TEST_ENUM_CASES = 16,
  TEST_ENUM_CASE_PARAMETERS = 8,
  TEST_TYPES = 32,
  TEST_FUNCTIONS = 8,
  TEST_PARAMETERS = 16,
  TEST_ENTRIES = 4,
  TEST_STATEMENTS = 128,
  TEST_EXPRESSIONS = 256,
  TEST_ARGUMENTS = 32,
  TEST_SWITCH_ARMS = 16,
  TEST_INTERPOLATION_SEGMENTS = 16,
  TEST_SYMBOLS = 64,
  TEST_FACTS = 16,
  TEST_DIAGNOSTICS = 8,
  TEST_HIR_RECORDS = 512,
  TEST_HIR_IDENTITIES = 64,
  TEST_HIR_TEXT = 8192,
  TEST_HIR_VALUES = 4096,
  TEST_RECEIPT = 65536,
  TEST_HIR_RECEIPT = W_SEED_HIR0_MAX_RECEIPT_BYTES,
};

typedef struct {
  uint8_t bytes[TEST_SOURCE_BYTES];
  size_t length;
  w_seed_source source;
  w_seed_lexer_frame lexer_frames[TEST_LEXER_FRAMES];
  w_seed_parse_token tokens[TEST_TOKENS];
  w_seed_cst_node nodes[TEST_NODES];
  w_seed_parse_frame parse_frames[TEST_PARSE_FRAMES];
  w_seed_parse_issue issues[TEST_ISSUES];
  w_seed_parser parser;
  w_seed_parse_result parse;
} parsed_document;

typedef struct {
  parsed_document parsed[TEST_DOCUMENTS];
  w_seed_frontend_document documents[TEST_DOCUMENTS];
  w_seed_frontend_input frontend_input;
  w_seed_frontend_module modules[TEST_MODULES];
  w_seed_frontend_import imports[TEST_IMPORTS];
  w_seed_frontend_import_item import_items[TEST_IMPORT_ITEMS];
  w_seed_frontend_struct structs[TEST_STRUCTS];
  w_seed_frontend_field fields[TEST_FIELDS];
  w_seed_frontend_enum enums[TEST_ENUMS];
  w_seed_frontend_enum_case enum_cases[TEST_ENUM_CASES];
  w_seed_frontend_enum_case_parameter enum_case_parameters[
      TEST_ENUM_CASE_PARAMETERS];
  w_seed_frontend_alias aliases[TEST_STRUCTS];
  w_seed_frontend_type_declaration type_declarations[TEST_STRUCTS];
  w_seed_frontend_type types[TEST_TYPES];
  w_seed_frontend_function functions[TEST_FUNCTIONS];
  w_seed_frontend_parameter parameters[TEST_PARAMETERS];
  w_seed_frontend_entry entries[TEST_ENTRIES];
  w_seed_frontend_statement statements[TEST_STATEMENTS];
  w_seed_frontend_expression expressions[TEST_EXPRESSIONS];
  w_seed_frontend_argument arguments[TEST_ARGUMENTS];
  w_seed_frontend_switch_arm switch_arms[TEST_SWITCH_ARMS];
  w_seed_frontend_pattern_capture pattern_captures[TEST_SWITCH_ARMS];
  w_seed_frontend_interpolation_segment interpolation_segments[
      TEST_INTERPOLATION_SEGMENTS];
  w_seed_frontend_symbol symbols[TEST_SYMBOLS];
  w_seed_frontend_fact facts[TEST_FACTS];
  w_seed_frontend_diagnostic diagnostics[TEST_DIAGNOSTICS];
  w_seed_frontend_diagnostic_fact diagnostic_facts[TEST_DIAGNOSTICS * 5];
  w_seed_frontend_diagnostic_item diagnostic_items[TEST_DIAGNOSTICS * 4];
  w_seed_frontend_diagnostic_label diagnostic_labels[TEST_DIAGNOSTICS * 2];
  uint8_t const_bytes[TEST_SOURCE_BYTES];
  w_seed_frontend_resolved_import resolved_imports[TEST_IMPORTS];
  w_seed_frontend_external_parameter host_parameters[1];
  w_seed_frontend_host_requirement host_requirements[1];
  w_seed_frontend_host_prelude_symbol host_symbols[1];
  w_seed_frontend_host_prelude host_scope;
  uint8_t frontend_receipt[TEST_RECEIPT];
  w_seed_frontend_output frontend_output;
  w_seed_frontend_result frontend_result;

  w_seed_hir0_module hir_modules[TEST_HIR_RECORDS];
  w_seed_hir0_identity hir_identities[TEST_HIR_IDENTITIES];
  w_seed_hir0_type hir_types[TEST_HIR_RECORDS];
  w_seed_hir0_enum hir_enums[TEST_HIR_RECORDS];
  w_seed_hir0_enum_case hir_enum_cases[TEST_HIR_RECORDS];
  w_seed_hir0_enum_case_parameter hir_enum_case_parameters[TEST_HIR_RECORDS];
  w_seed_hir0_enum_subset_member hir_enum_subset_members[TEST_HIR_RECORDS];
  w_seed_hir0_function hir_functions[TEST_HIR_RECORDS];
  w_seed_hir0_parameter hir_parameters[TEST_HIR_RECORDS];
  w_seed_hir0_block hir_blocks[TEST_HIR_RECORDS];
  w_seed_hir0_block_argument hir_block_arguments[TEST_HIR_RECORDS];
  w_seed_hir0_edge_argument hir_edge_arguments[TEST_HIR_RECORDS];
  w_seed_hir0_switch_edge hir_switch_edges[TEST_HIR_RECORDS];
  w_seed_hir0_switch_capture hir_switch_captures[TEST_HIR_RECORDS];
  w_seed_hir0_instruction hir_instructions[TEST_HIR_RECORDS];
  w_seed_hir0_binding hir_bindings[TEST_HIR_RECORDS];
  w_seed_hir0_call hir_calls[TEST_HIR_RECORDS];
  w_seed_hir0_host_parameter hir_host_parameters[TEST_HIR_RECORDS];
  w_seed_hir0_argument hir_arguments[TEST_HIR_RECORDS];
  w_seed_hir0_enum_payload hir_enum_payloads[TEST_HIR_RECORDS];
  w_seed_hir0_requirement hir_requirements[TEST_HIR_RECORDS];
  w_seed_hir0_value hir_values[TEST_HIR_VALUES];
  w_seed_hir0_interpolation_segment hir_interpolation_segments[TEST_HIR_RECORDS];
  w_seed_hir0_terminator hir_terminators[TEST_HIR_RECORDS];
  w_seed_hir0_entry hir_entries[TEST_HIR_RECORDS];
  w_seed_hir0_external_module hir_external_modules[1];
  w_seed_hir0_external_symbol hir_external_symbols[1];
  w_seed_hir0_cleanup hir_cleanups[TEST_HIR_RECORDS];
  uint8_t hir_text[TEST_HIR_TEXT];
  uint8_t hir_value_bytes[TEST_HIR_VALUES];
  uint8_t hir_receipt[TEST_HIR_RECEIPT];
  w_seed_hir0_output hir_output;
  w_seed_hir0_result hir_result;
  w_seed_hir0_counts hir_counts;
  w_seed_hir0_program hir_program;
} multidoc_fixture;

static w_seed_native0_storage native_graph_storage;
static w_seed_native0_storage native_graph_storage_repeated;
static uint8_t native_graph_bytes[W_SEED_MLIR0_MAX_BYTES];
static uint8_t native_graph_bytes_repeated[W_SEED_MLIR0_MAX_BYTES];

static const char ROOT_SOURCE[] =
    "import { helper as h } from lib\n"
    "fn run(): i64 { return h() }\n"
    "entry(run)\n";

static const char ROOT_WHITESPACE_SOURCE[] =
    "import { helper as h } from lib\n"
    "\n"
    "fn run(): i64 {   return h()   }\n"
    "entry(run)\n";

static const char LIB_SOURCE[] =
    "module lib\n"
    "// provenance padding keeps this document longer than the root\n"
    "// one two three four five six seven eight nine ten eleven twelve\n"
    "// more padding for an owning-document span check\n"
    "export fn helper(): i64 { return 42 }\n";

static const char LIB_WHITESPACE_SOURCE[] =
    "module lib\n"
    "// provenance padding keeps this document longer than the root\n"
    "// one two three four five six seven eight nine ten eleven twelve\n"
    "// more padding for an owning-document span check\n"
    "export fn helper(): i64 {   return 42   }\n";

static const char NATIVE_ROOT_SOURCE[] =
    "import { helper as h } from lib\n"
    "fn run() { let value = h() print(\"answer ${value}\") }\n"
    "entry(run)\n";

static const char PRIVATE_LIB_SOURCE[] =
    "module lib\n"
    "fn helper(): i64 { return 42 }\n";

static bool parse_document(parsed_document *document, const char *source_text) {
  if (document == NULL || source_text == NULL) return false;
  document->length = strlen(source_text);
  if (document->length >= sizeof(document->bytes)) return false;
  (void)memcpy(document->bytes, source_text, document->length);
  w_seed_source_error source_error;
  if (!w_seed_source_init(
          (w_seed_byte_view){document->bytes, document->length},
          &document->source, &source_error))
    return false;
  w_seed_lex_error lex_error;
  if (!w_seed_parser_init(
          &document->source, (w_seed_span){0u, document->length},
          (w_seed_foreign_limits){65536u, 256u}, document->lexer_frames,
          TEST_LEXER_FRAMES, document->tokens, TEST_TOKENS, document->nodes,
          TEST_NODES, document->parse_frames, TEST_PARSE_FRAMES,
          document->issues, TEST_ISSUES, &document->parser, &lex_error))
    return false;
  return w_seed_parser_parse(&document->parser, &document->parse);
}

static void setup_frontend_output(multidoc_fixture *fixture) {
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
      .type_declaration_capacity = TEST_STRUCTS,
      .aliases = fixture->aliases,
      .alias_capacity = TEST_STRUCTS,
      .types = fixture->types,
      .type_capacity = TEST_TYPES,
      .functions = fixture->functions,
      .function_capacity = TEST_FUNCTIONS,
      .parameters = fixture->parameters,
      .parameter_capacity = TEST_PARAMETERS,
      .entries = fixture->entries,
      .entry_capacity = TEST_ENTRIES,
      .statements = fixture->statements,
      .statement_capacity = TEST_STATEMENTS,
      .expressions = fixture->expressions,
      .expression_capacity = TEST_EXPRESSIONS,
      .interpolation_segments = fixture->interpolation_segments,
      .interpolation_segment_capacity = TEST_INTERPOLATION_SEGMENTS,
      .arguments = fixture->arguments,
      .argument_capacity = TEST_ARGUMENTS,
      .switch_arms = fixture->switch_arms,
      .switch_arm_capacity = TEST_SWITCH_ARMS,
      .pattern_captures = fixture->pattern_captures,
      .pattern_capture_capacity = TEST_SWITCH_ARMS,
      .symbols = fixture->symbols,
      .symbol_capacity = TEST_SYMBOLS,
      .facts = fixture->facts,
      .fact_capacity = TEST_FACTS,
      .diagnostics = fixture->diagnostics,
      .diagnostic_capacity = TEST_DIAGNOSTICS,
      .diagnostic_facts = fixture->diagnostic_facts,
      .diagnostic_fact_capacity = TEST_DIAGNOSTICS * 5u,
      .diagnostic_items = fixture->diagnostic_items,
      .diagnostic_item_capacity = TEST_DIAGNOSTICS * 4u,
      .diagnostic_labels = fixture->diagnostic_labels,
      .diagnostic_label_capacity = TEST_DIAGNOSTICS * 2u,
      .receipt = fixture->frontend_receipt,
      .receipt_capacity = sizeof(fixture->frontend_receipt),
      .enums = fixture->enums,
      .enum_capacity = TEST_ENUMS,
      .enum_cases = fixture->enum_cases,
      .enum_case_capacity = TEST_ENUM_CASES,
      .enum_case_parameters = fixture->enum_case_parameters,
      .enum_case_parameter_capacity = TEST_ENUM_CASE_PARAMETERS,
      .enum_subset_members = NULL,
      .enum_subset_member_capacity = 0u,
      .enum_membership_cases = NULL,
      .enum_membership_case_capacity = 0u,
      .const_declarations = NULL,
      .const_declaration_capacity = 0u,
      .const_bytes = fixture->const_bytes,
      .const_bytes_capacity = sizeof(fixture->const_bytes)};
}

static void setup_hir_output(multidoc_fixture *fixture) {
  fixture->hir_output = (w_seed_hir0_output){
      .modules = fixture->hir_modules,
      .module_capacity = TEST_HIR_RECORDS,
      .identities = fixture->hir_identities,
      .identity_capacity = TEST_HIR_IDENTITIES,
      .types = fixture->hir_types,
      .type_capacity = TEST_HIR_RECORDS,
      .enums = fixture->hir_enums,
      .enum_capacity = TEST_HIR_RECORDS,
      .enum_cases = fixture->hir_enum_cases,
      .enum_case_capacity = TEST_HIR_RECORDS,
      .enum_case_parameters = fixture->hir_enum_case_parameters,
      .enum_case_parameter_capacity = TEST_HIR_RECORDS,
      .enum_subset_members = fixture->hir_enum_subset_members,
      .enum_subset_member_capacity = TEST_HIR_RECORDS,
      .functions = fixture->hir_functions,
      .function_capacity = TEST_HIR_RECORDS,
      .parameters = fixture->hir_parameters,
      .parameter_capacity = TEST_HIR_RECORDS,
      .blocks = fixture->hir_blocks,
      .block_capacity = TEST_HIR_RECORDS,
      .block_arguments = fixture->hir_block_arguments,
      .block_argument_capacity = TEST_HIR_RECORDS,
      .edge_arguments = fixture->hir_edge_arguments,
      .edge_argument_capacity = TEST_HIR_RECORDS,
      .switch_edges = fixture->hir_switch_edges,
      .switch_edge_capacity = TEST_HIR_RECORDS,
      .switch_captures = fixture->hir_switch_captures,
      .switch_capture_capacity = TEST_HIR_RECORDS,
      .instructions = fixture->hir_instructions,
      .instruction_capacity = TEST_HIR_RECORDS,
      .bindings = fixture->hir_bindings,
      .binding_capacity = TEST_HIR_RECORDS,
      .calls = fixture->hir_calls,
      .call_capacity = TEST_HIR_RECORDS,
      .host_parameters = fixture->hir_host_parameters,
      .host_parameter_capacity = TEST_HIR_RECORDS,
      .arguments = fixture->hir_arguments,
      .argument_capacity = TEST_HIR_RECORDS,
      .enum_payloads = fixture->hir_enum_payloads,
      .enum_payload_capacity = TEST_HIR_RECORDS,
      .requirements = fixture->hir_requirements,
      .requirement_capacity = TEST_HIR_RECORDS,
      .values = fixture->hir_values,
      .value_capacity = TEST_HIR_VALUES,
      .interpolation_segments = fixture->hir_interpolation_segments,
      .interpolation_segment_capacity = TEST_HIR_RECORDS,
      .terminators = fixture->hir_terminators,
      .terminator_capacity = TEST_HIR_RECORDS,
      .entries = fixture->hir_entries,
      .entry_capacity = TEST_HIR_RECORDS,
      .text_bytes = fixture->hir_text,
      .text_byte_capacity = sizeof(fixture->hir_text),
      .value_bytes = fixture->hir_value_bytes,
      .value_byte_capacity = sizeof(fixture->hir_value_bytes),
      .receipt = fixture->hir_receipt,
      .receipt_capacity = sizeof(fixture->hir_receipt),
      .external_modules = fixture->hir_external_modules,
      .external_module_capacity = 1u,
      .external_symbols = fixture->hir_external_symbols,
      .external_symbol_capacity = 1u,
      .cleanups = fixture->hir_cleanups,
      .cleanup_capacity = TEST_HIR_RECORDS};
}

static bool initialize_fixture(multidoc_fixture *fixture,
                               const char *root_source,
                               const char *library_source) {
  if (fixture == NULL || root_source == NULL || library_source == NULL)
    return false;
  (void)memset(fixture, 0, sizeof(*fixture));
  if (!parse_document(&fixture->parsed[0], root_source) ||
      !parse_document(&fixture->parsed[1], library_source))
    return false;
  fixture->documents[0] = (w_seed_frontend_document){
      .logical_source_id = (w_seed_frontend_text){"app-source", 10u},
      .module_id = (w_seed_frontend_text){"app", 3u},
      .local_module_name = (w_seed_frontend_text){"app", 3u},
      .source = &fixture->parsed[0].source,
      .nodes = fixture->parsed[0].nodes,
      .node_count = fixture->parsed[0].parse.node_count,
      .parse = fixture->parsed[0].parse};
  fixture->documents[1] = (w_seed_frontend_document){
      .logical_source_id = (w_seed_frontend_text){"lib-source", 10u},
      .module_id = (w_seed_frontend_text){"lib", 3u},
      .local_module_name = (w_seed_frontend_text){"lib", 3u},
      .source = &fixture->parsed[1].source,
      .nodes = fixture->parsed[1].nodes,
      .node_count = fixture->parsed[1].parse.node_count,
      .parse = fixture->parsed[1].parse};
  w_seed_module_origin origins[TEST_IMPORTS];
  w_seed_module_scan_result scan_result;
  if (w_seed_module_scan(
          fixture->documents[0].source, fixture->documents[0].nodes,
          fixture->documents[0].parse.node_count, &fixture->documents[0].parse,
          origins, TEST_IMPORTS, &scan_result) != W_SEED_MODULE_SCAN_OK ||
      scan_result.written != 1u)
    return false;
  fixture->resolved_imports[0] = (w_seed_frontend_resolved_import){
      .source_document_index = 0u,
      .direct_import_ordinal = origins[0].direct_import_ordinal,
      .import_declaration_span = origins[0].declaration_span,
      .target_kind = W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT,
      .target_index = 1u};
  fixture->host_symbols[0] = (w_seed_frontend_host_prelude_symbol){
      .name = (w_seed_frontend_text){"noop", 4u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"()", 2u},
      .is_const = false,
      .requirements = NULL,
      .requirement_count = 0u};
  fixture->host_scope = (w_seed_frontend_host_prelude){
      .profile = (w_seed_frontend_text){"multidoc-test@1", 15u},
      .symbols = fixture->host_symbols,
      .symbol_count = 1u};
  fixture->frontend_input = (w_seed_frontend_input){
      .documents = fixture->documents,
      .document_count = TEST_DOCUMENTS,
      .external_modules = NULL,
      .external_module_count = 0u,
      .host_scope = &fixture->host_scope,
      .import_resolution_complete = true,
      .resolved_imports = fixture->resolved_imports,
      .resolved_import_count = 1u};
  setup_frontend_output(fixture);
  return true;
}

static bool prepare_fixture(multidoc_fixture *fixture, const char *root_source,
                            const char *library_source) {
  if (!initialize_fixture(fixture, root_source, library_source)) return false;
  if (w_seed_frontend_run(&fixture->frontend_input, &fixture->frontend_output,
                          &fixture->frontend_result) != W_SEED_FRONTEND_OK)
    return false;
  return true;
}

static w_seed_hir0_input hir_input(const multidoc_fixture *fixture) {
  return (w_seed_hir0_input){
      .frontend_input = &fixture->frontend_input,
      .frontend_output = &fixture->frontend_output,
      .frontend_result = &fixture->frontend_result};
}

static bool lower_fixture(multidoc_fixture *fixture) {
  const w_seed_hir0_input input = hir_input(fixture);
  const w_seed_hir0_status measure_status =
      w_seed_hir0_measure(&input, &fixture->hir_counts, &fixture->hir_result);
  if (measure_status != W_SEED_HIR0_OK) return false;
  setup_hir_output(fixture);
  const w_seed_hir0_status run_status =
      w_seed_hir0_run(&input, &fixture->hir_output, &fixture->hir_result);
  if (run_status != W_SEED_HIR0_OK) return false;
  if (!w_seed_hir0_program_from_output(&fixture->hir_output,
                                       &fixture->hir_result,
                                       &fixture->hir_program))
    return false;
  return w_seed_hir0_verify(&fixture->hir_program, &fixture->hir_result);
}

static bool test_native0_frontend_graph(void) {
  static multidoc_fixture graph;
  CHECK(initialize_fixture(&graph, NATIVE_ROOT_SOURCE, LIB_SOURCE));
  graph.frontend_input.host_scope = NULL;
  const w_seed_mlir0_target target = {
      W_SEED_MLIR0_TARGET_X86_64_UNKNOWN_LINUX_GNU};
  const w_seed_native0_output output = {
      native_graph_bytes, sizeof(native_graph_bytes)};
  const w_seed_native0_output repeated_output = {
      native_graph_bytes_repeated, sizeof(native_graph_bytes_repeated)};
  w_seed_native0_result result = {W_SEED_NATIVE0_INVALID, SIZE_MAX, {0}};
  w_seed_native0_result repeated = {W_SEED_NATIVE0_INVALID, SIZE_MAX, {0}};
  CHECK(w_seed_native0_run_frontend_graph(
            &graph.frontend_input, &target, &native_graph_storage, &output,
            &result) == W_SEED_NATIVE0_OK);
  CHECK(result.status == W_SEED_NATIVE0_OK);
  CHECK(result.source_bytes ==
        strlen(NATIVE_ROOT_SOURCE) + strlen(LIB_SOURCE));
  CHECK(result.mlir.written.mlir_bytes != 0u);
  CHECK(w_seed_native0_run_frontend_graph(
            &graph.frontend_input, &target, &native_graph_storage_repeated,
            &repeated_output, &repeated) == W_SEED_NATIVE0_OK);
  CHECK(repeated.mlir.written.mlir_bytes == result.mlir.written.mlir_bytes);
  CHECK(memcmp(native_graph_bytes, native_graph_bytes_repeated,
               result.mlir.written.mlir_bytes) == 0);
  CHECK(memcmp(result.mlir.mlir_sha256, repeated.mlir.mlir_sha256, 32u) == 0);

  (void)memset(native_graph_bytes_repeated, 0xa5, 32u);
  const uint8_t saved_short[32] = {
      0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5,
      0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5,
      0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5,
      0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5};
  const w_seed_native0_output short_output = {native_graph_bytes_repeated,
                                               sizeof(saved_short)};
  const w_seed_native0_result saved_result = result;
  const w_seed_native0_status short_status =
      w_seed_native0_run_frontend_graph(
          &graph.frontend_input, &target, &native_graph_storage_repeated,
          &short_output, &result);
  CHECK(short_status == W_SEED_NATIVE0_CAPACITY);
  CHECK(memcmp(native_graph_bytes_repeated, saved_short,
               sizeof(saved_short)) == 0);
  CHECK(memcmp(&result, &saved_result, sizeof(result)) == 0);

  CHECK(initialize_fixture(&graph, NATIVE_ROOT_SOURCE, PRIVATE_LIB_SOURCE));
  graph.frontend_input.host_scope = NULL;
  (void)memset(native_graph_bytes_repeated, 0x5a, 32u);
  const uint8_t saved_private[32] = {
      0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a,
      0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a,
      0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a,
      0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a};
  result = saved_result;
  const w_seed_native0_status private_status =
      w_seed_native0_run_frontend_graph(
          &graph.frontend_input, &target, &native_graph_storage_repeated,
          &repeated_output, &result);
  CHECK(private_status == W_SEED_NATIVE0_UNSUPPORTED);
  CHECK(memcmp(native_graph_bytes_repeated, saved_private,
               sizeof(saved_private)) == 0);
  CHECK(memcmp(&result, &saved_result, sizeof(result)) == 0);
  return true;
}

static bool frontend_call_target_is_library(const multidoc_fixture *fixture) {
  for (size_t index = 0u; index < fixture->frontend_result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *expression =
        &fixture->expressions[index];
    if (expression->kind != W_SEED_FRONTEND_EXPR_CALL ||
        expression->resolved_callee_kind !=
            W_SEED_FRONTEND_CALLEE_LOCAL_FUNCTION ||
        expression->resolved_function_index == W_SEED_FRONTEND_NONE ||
        (size_t)expression->resolved_function_index >=
            fixture->frontend_result.written.functions)
      continue;
    const w_seed_frontend_function *target =
        &fixture->functions[expression->resolved_function_index];
    if (target->module_index == 1u && text_is(target->name, "helper"))
      return true;
  }
  return false;
}

static bool check_identity_and_ownership(const multidoc_fixture *fixture) {
  CHECK(fixture->frontend_result.written.modules == 2u);
  CHECK(fixture->frontend_result.written.functions == 2u);
  CHECK(fixture->frontend_result.written.entries == 1u);
  CHECK(fixture->frontend_result.written.imports == 1u);
  CHECK(frontend_call_target_is_library(fixture));
  CHECK(fixture->hir_program.module_count == 2u);
  CHECK(fixture->hir_program.function_count == 2u);
  CHECK(fixture->hir_program.entry_count == 1u);
  CHECK(fixture->hir_program.call_count == 1u);
  CHECK(hir_text_is(&fixture->hir_program,
                   fixture->hir_modules[0].module_id, "app"));
  CHECK(hir_text_is(&fixture->hir_program,
                   fixture->hir_modules[1].module_id, "lib"));
  CHECK(fixture->hir_functions[0].module_index == 0u);
  CHECK(fixture->hir_functions[1].module_index == 1u);
  CHECK(fixture->hir_entries[0].module_index == 0u);
  CHECK(fixture->hir_calls[0].callee_identity ==
        fixture->hir_program.module_count + 1u);
  CHECK(fixture->hir_functions[1].source_span.end_byte <=
        fixture->parsed[1].length);
  CHECK(fixture->hir_functions[1].source_span.start_byte >
        fixture->parsed[0].length);
  return true;
}

static bool test_multidoc_hir0(void) {
  static multidoc_fixture canonical;
  static multidoc_fixture repeated;
  CHECK(prepare_fixture(&canonical, ROOT_SOURCE, LIB_SOURCE));
  CHECK(lower_fixture(&canonical));
  CHECK(check_identity_and_ownership(&canonical));
  CHECK(prepare_fixture(&repeated, ROOT_SOURCE, LIB_SOURCE));
  CHECK(lower_fixture(&repeated));
  CHECK(check_identity_and_ownership(&repeated));
  CHECK(canonical.hir_result.written.modules == repeated.hir_result.written.modules);
  CHECK(memcmp(canonical.hir_result.semantic_digest,
               repeated.hir_result.semantic_digest, 32u) == 0);
  CHECK(memcmp(canonical.hir_result.provenance_digest,
               repeated.hir_result.provenance_digest, 32u) == 0);
  CHECK(memcmp(canonical.hir_receipt, repeated.hir_receipt,
               canonical.hir_result.written.receipt_bytes) == 0);

  CHECK(prepare_fixture(&repeated, ROOT_WHITESPACE_SOURCE,
                        LIB_WHITESPACE_SOURCE));
  CHECK(lower_fixture(&repeated));
  CHECK(check_identity_and_ownership(&repeated));
  CHECK(memcmp(canonical.hir_result.semantic_digest,
               repeated.hir_result.semantic_digest, 32u) == 0);
  CHECK(memcmp(canonical.hir_result.provenance_digest,
               repeated.hir_result.provenance_digest, 32u) != 0);

  /* HIR verification is caller-owned and must not consult frontend storage. */
  (void)memset(&canonical.parsed, 0, sizeof(canonical.parsed));
  (void)memset(&canonical.documents, 0, sizeof(canonical.documents));
  (void)memset(&canonical.frontend_input, 0, sizeof(canonical.frontend_input));
  (void)memset(&canonical.frontend_output, 0,
               sizeof(canonical.frontend_output));
  (void)memset(&canonical.frontend_result, 0,
               sizeof(canonical.frontend_result));
  CHECK(w_seed_hir0_verify(&canonical.hir_program, &canonical.hir_result));

  /* Malformed ownership, entry, call, and resolver records fail before HIR
   * emission, while capacity and alias checks remain transactional. */
  CHECK(prepare_fixture(&canonical, ROOT_SOURCE, LIB_SOURCE));
  CHECK(lower_fixture(&canonical));
  w_seed_hir0_input input = hir_input(&canonical);

  /* Empty-span ownership is relational for nominal/enum records. Only the
   * accepted scalar builtin family may be provenance-free and shared. */
  const w_seed_frontend_type saved_type = canonical.types[2];
  canonical.types[2].kind = W_SEED_FRONTEND_TYPE_NOMINAL;
  canonical.types[2].span = (w_seed_span){0u, 0u};
  canonical.types[2].external_module_index = W_SEED_FRONTEND_NONE;
  canonical.types[2].external_symbol_index = W_SEED_FRONTEND_NONE;
  size_t type_owner = SIZE_MAX;
  CHECK(frontend_type_owner_module(&input, 2u, &type_owner) &&
        type_owner == 1u);
  canonical.types[2].kind = W_SEED_FRONTEND_TYPE_ENUM;
  canonical.types[2].enum_base_index = 0u;
  type_owner = SIZE_MAX;
  CHECK(!frontend_type_owner_module(&input, 2u, &type_owner));
  canonical.types[2] = saved_type;

  const w_seed_hir0_counts saved_measure_counts = canonical.hir_counts;
  const w_seed_hir0_result saved_measure_result = canonical.hir_result;
  const size_t saved_document_count = canonical.frontend_input.document_count;
  canonical.frontend_input.document_count = 0u;
  CHECK(w_seed_hir0_measure(&input, &canonical.hir_counts,
                            &canonical.hir_result) != W_SEED_HIR0_OK);
  CHECK(memcmp(&canonical.hir_counts, &saved_measure_counts,
               sizeof(saved_measure_counts)) == 0);
  CHECK(memcmp(&canonical.hir_result, &saved_measure_result,
               sizeof(saved_measure_result)) == 0);
  canonical.frontend_input.document_count = saved_document_count;
  const w_seed_frontend_module saved_module = canonical.modules[1];
  canonical.modules[1].document_index = 0u;
  CHECK(w_seed_hir0_measure(&input, &canonical.hir_counts,
                            &canonical.hir_result) != W_SEED_HIR0_OK);
  canonical.modules[1] = saved_module;
  const w_seed_frontend_entry saved_entry = canonical.entries[0];
  canonical.entries[0].module_index = 1u;
  CHECK(w_seed_hir0_measure(&input, &canonical.hir_counts,
                            &canonical.hir_result) != W_SEED_HIR0_OK);
  canonical.entries[0] = saved_entry;
  size_t call_expression = 0u;
  while (call_expression < canonical.frontend_result.written.expressions &&
         canonical.expressions[call_expression].kind !=
             W_SEED_FRONTEND_EXPR_CALL)
    call_expression += 1u;
  CHECK(call_expression < canonical.frontend_result.written.expressions);
  const w_seed_frontend_expression saved_call =
      canonical.expressions[call_expression];
  canonical.expressions[call_expression].resolved_function_index = 0u;
  CHECK(w_seed_hir0_measure(&input, &canonical.hir_counts,
                            &canonical.hir_result) != W_SEED_HIR0_OK);
  canonical.expressions[call_expression] = saved_call;
  const w_seed_frontend_resolved_import saved_edge = canonical.resolved_imports[0];
  canonical.resolved_imports[0].target_index = 0u;
  CHECK(w_seed_hir0_measure(&input, &canonical.hir_counts,
                            &canonical.hir_result) != W_SEED_HIR0_OK);
  canonical.resolved_imports[0] = saved_edge;

  const w_seed_frontend_import saved_import = canonical.imports[0];
  canonical.imports[0].path = (w_seed_frontend_text){"app", 3u};
  CHECK(w_seed_hir0_measure(&input, &canonical.hir_counts,
                            &canonical.hir_result) != W_SEED_HIR0_OK);
  CHECK(memcmp(&canonical.hir_counts, &saved_measure_counts,
               sizeof(saved_measure_counts)) == 0);
  CHECK(memcmp(&canonical.hir_result, &saved_measure_result,
               sizeof(saved_measure_result)) == 0);
  canonical.imports[0] = saved_import;
  const w_seed_frontend_import_item saved_import_item = canonical.import_items[0];
  canonical.import_items[0].name = (w_seed_frontend_text){"forged", 6u};
  CHECK(w_seed_hir0_measure(&input, &canonical.hir_counts,
                            &canonical.hir_result) != W_SEED_HIR0_OK);
  canonical.import_items[0] = saved_import_item;

  /* Root entry selection is explicit and unique. Body entries and imported
   * targets cannot be smuggled into the root entry slot. */
  const w_seed_frontend_entry saved_root_entry = canonical.entries[0];
  canonical.entries[0].is_body = true;
  CHECK(w_seed_hir0_measure(&input, &canonical.hir_counts,
                            &canonical.hir_result) != W_SEED_HIR0_OK);
  canonical.entries[0] = saved_root_entry;
  canonical.entries[0].target_function = 1u;
  CHECK(w_seed_hir0_measure(&input, &canonical.hir_counts,
                            &canonical.hir_result) != W_SEED_HIR0_OK);
  canonical.entries[0] = saved_root_entry;
  const w_seed_frontend_module saved_entry_module = canonical.modules[1];
  canonical.modules[1].entry_count = 1u;
  CHECK(w_seed_hir0_measure(&input, &canonical.hir_counts,
                            &canonical.hir_result) != W_SEED_HIR0_OK);
  canonical.modules[1] = saved_entry_module;

  /* A forged reverse edge must be rejected as a graph cycle even though the
   * individual local import records remain otherwise well-shaped. */
  const w_seed_frontend_module saved_cycle_module = canonical.modules[1];
  const w_seed_frontend_import saved_cycle_import = canonical.imports[1];
  const w_seed_frontend_resolved_import saved_cycle_edge =
      canonical.resolved_imports[1];
  const w_seed_frontend_counts saved_cycle_required =
      canonical.frontend_result.required;
  const w_seed_frontend_counts saved_cycle_written =
      canonical.frontend_result.written;
  const size_t saved_cycle_import_count =
      canonical.frontend_input.resolved_import_count;
  canonical.modules[1].first_import = 1u;
  canonical.modules[1].import_count = 1u;
  canonical.imports[1] = (w_seed_frontend_import){
      .module_index = 1u,
      .path = (w_seed_frontend_text){"app", 3u},
      .alias = (w_seed_frontend_text){NULL, 0u},
      .span = (w_seed_span){0u, 0u},
      .first_item = 1u,
      .item_count = 0u,
      .direct_import_ordinal = 0u,
      .target_kind = W_SEED_FRONTEND_IMPORT_LOCAL_DOCUMENT,
      .target_index = 0u};
  canonical.resolved_imports[1] = (w_seed_frontend_resolved_import){
      .source_document_index = 1u,
      .direct_import_ordinal = 0u,
      .import_declaration_span = (w_seed_span){0u, 0u},
      .target_kind = W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT,
      .target_index = 0u};
  canonical.frontend_input.resolved_import_count = 2u;
  canonical.frontend_result.required.imports = 2u;
  canonical.frontend_result.written.imports = 2u;
  CHECK(w_seed_hir0_measure(&input, &canonical.hir_counts,
                            &canonical.hir_result) != W_SEED_HIR0_OK);
  canonical.modules[1] = saved_cycle_module;
  canonical.imports[1] = saved_cycle_import;
  canonical.resolved_imports[1] = saved_cycle_edge;
  canonical.frontend_input.resolved_import_count = saved_cycle_import_count;
  canonical.frontend_result.required = saved_cycle_required;
  canonical.frontend_result.written = saved_cycle_written;

  const w_seed_hir0_module saved_hir_module = canonical.hir_modules[0];
  const w_seed_hir0_function saved_hir_function = canonical.hir_functions[0];
  w_seed_hir0_output aliased_output = canonical.hir_output;
  aliased_output.functions = (w_seed_hir0_function *)aliased_output.modules;
  w_seed_hir0_result unchanged_result = canonical.hir_result;
  CHECK(w_seed_hir0_run(&input, &aliased_output, &unchanged_result) ==
        W_SEED_HIR0_INVALID);
  CHECK(memcmp(&canonical.hir_modules[0], &saved_hir_module,
               sizeof(saved_hir_module)) == 0);
  CHECK(memcmp(&canonical.hir_functions[0], &saved_hir_function,
               sizeof(saved_hir_function)) == 0);
  CHECK(memcmp(&unchanged_result, &saved_measure_result,
               sizeof(saved_measure_result)) == 0);
  w_seed_hir0_output truncated_output = canonical.hir_output;
  truncated_output.module_capacity = 0u;
  unchanged_result = canonical.hir_result;
  CHECK(w_seed_hir0_run(&input, &truncated_output, &unchanged_result) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(memcmp(&canonical.hir_modules[0], &saved_hir_module,
               sizeof(saved_hir_module)) == 0);
  CHECK(memcmp(&canonical.hir_functions[0], &saved_hir_function,
               sizeof(saved_hir_function)) == 0);
  CHECK(memcmp(&unchanged_result, &saved_measure_result,
               sizeof(saved_measure_result)) == 0);
  return true;
}

int main(void) {
  return test_native0_frontend_graph() && test_multidoc_hir0() ? 0 : 1;
}
