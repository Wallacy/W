#include "w_seed_mlir0.h"

#include "w_seed_product_closure0.h"
#include "../src/w_seed_native_subset0.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "mlir0 check failed: %s (%s:%d)\n", #condition, \
                    __FILE__, __LINE__);                                      \
      return false;                                                            \
    }                                                                          \
  } while (0)

enum {
  TEST_SOURCE = 4096,
  TEST_LEXER_FRAMES = 256,
  TEST_TOKENS = 2048,
  TEST_NODES = 4096,
  TEST_PARSE_FRAMES = 2048,
  TEST_ISSUES = 128,
  TEST_MODULES = 8,
  TEST_IMPORTS = 8,
  TEST_IMPORT_ITEMS = 8,
  TEST_STRUCTS = 4,
  TEST_FIELDS = 8,
  TEST_TYPES = 64,
  TEST_FUNCTIONS = 16,
  TEST_PARAMETERS = 32,
  TEST_ENTRIES = 4,
  TEST_STATEMENTS = 64,
  TEST_EXPRESSIONS = 256,
  TEST_ARGUMENTS = 64,
  TEST_TUPLE_COMPONENTS = TEST_TYPES + (2 * TEST_ARGUMENTS),
  TEST_TUPLE_ELEMENTS = TEST_EXPRESSIONS / 2,
  TEST_SYMBOLS = 128,
  TEST_FACTS = 16,
  TEST_DIAGNOSTICS = 8,
  TEST_RECEIPT = 65536,
  TEST_HIR_IDENTITIES = 32,
  TEST_HIR_RECORDS = 256,
  TEST_HIR_TEXT = 4096,
  TEST_HIR_VALUES = 4096,
  TEST_HIR_RECEIPT = W_SEED_HIR0_MAX_RECEIPT_BYTES,
};

typedef struct {
  uint8_t source_bytes[TEST_SOURCE];
  size_t source_length;
  w_seed_source source;
  w_seed_lexer_frame lexer_frames[TEST_LEXER_FRAMES];
  w_seed_parse_token tokens[TEST_TOKENS];
  w_seed_cst_node nodes[TEST_NODES];
  w_seed_parse_frame parse_frames[TEST_PARSE_FRAMES];
  w_seed_parse_issue issues[TEST_ISSUES];
  w_seed_parser parser;
  w_seed_parse_result parse;
  w_seed_frontend_document document;
  w_seed_frontend_input input;
  w_seed_frontend_module modules[TEST_MODULES];
  w_seed_frontend_import imports[TEST_IMPORTS];
  w_seed_frontend_import_item import_items[TEST_IMPORT_ITEMS];
  w_seed_frontend_struct structs[TEST_STRUCTS];
  w_seed_frontend_field fields[TEST_FIELDS];
  w_seed_frontend_enum enums[TEST_HIR_RECORDS];
  w_seed_frontend_enum_case enum_cases[TEST_HIR_RECORDS];
  w_seed_frontend_enum_case_parameter
      enum_case_parameters[TEST_HIR_RECORDS];
  w_seed_frontend_switch_arm switch_arms[TEST_HIR_RECORDS];
  w_seed_frontend_pattern_capture pattern_captures[TEST_HIR_RECORDS];
  w_seed_frontend_enum_subset_member
      enum_subset_members[TEST_HIR_RECORDS];
  w_seed_frontend_type_declaration type_declarations[TEST_STRUCTS];
  w_seed_frontend_alias aliases[TEST_STRUCTS];
  w_seed_frontend_type types[TEST_TYPES];
  w_seed_frontend_function functions[TEST_FUNCTIONS];
  w_seed_frontend_parameter parameters[TEST_PARAMETERS];
  w_seed_frontend_entry entries[TEST_ENTRIES];
  w_seed_frontend_statement statements[TEST_STATEMENTS];
  w_seed_frontend_expression expressions[TEST_EXPRESSIONS];
  w_seed_frontend_argument arguments[TEST_ARGUMENTS];
  w_seed_frontend_tuple_component
      tuple_components[TEST_TUPLE_COMPONENTS];
  w_seed_frontend_tuple_element tuple_elements[TEST_TUPLE_ELEMENTS];
  w_seed_frontend_interpolation_segment
      interpolation_segments[TEST_EXPRESSIONS];
  w_seed_frontend_symbol symbols[TEST_SYMBOLS];
  w_seed_frontend_fact facts[TEST_FACTS];
  w_seed_frontend_diagnostic diagnostics[TEST_DIAGNOSTICS];
  w_seed_frontend_diagnostic_fact diagnostic_facts[TEST_DIAGNOSTICS * 5];
  w_seed_frontend_diagnostic_item diagnostic_items[TEST_DIAGNOSTICS * 4];
  w_seed_frontend_diagnostic_label diagnostic_labels[TEST_DIAGNOSTICS * 2];
  w_seed_frontend_host_requirement host_requirements[2];
  w_seed_frontend_external_parameter host_parameters[2];
  w_seed_frontend_host_prelude_symbol host_symbols[2];
  w_seed_frontend_host_prelude host_scope;
  w_seed_frontend_external_parameter external_parameters[8];
  w_seed_frontend_external_symbol external_symbols[8];
  w_seed_frontend_external_module external_modules[2];
  w_seed_frontend_resolved_import resolved_imports[4];
  uint8_t const_bytes[TEST_SOURCE];
  uint8_t frontend_receipt[TEST_RECEIPT];
  w_seed_frontend_output output;
  w_seed_frontend_result frontend_result;
  w_seed_hir0_module hir_modules[TEST_HIR_RECORDS];
  w_seed_hir0_identity hir_identities[TEST_HIR_IDENTITIES];
  w_seed_hir0_type hir_types[TEST_HIR_RECORDS];
  w_seed_hir0_enum hir_enums[TEST_HIR_RECORDS];
  w_seed_hir0_enum_case hir_enum_cases[TEST_HIR_RECORDS];
  w_seed_hir0_enum_case_parameter
      hir_enum_case_parameters[TEST_HIR_RECORDS];
  w_seed_hir0_enum_payload hir_enum_payloads[TEST_HIR_RECORDS];
  w_seed_hir0_enum_subset_member
      hir_enum_subset_members[TEST_HIR_RECORDS];
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
  w_seed_hir0_requirement hir_requirements[TEST_HIR_RECORDS];
  w_seed_hir0_value hir_values[TEST_HIR_RECORDS];
  w_seed_hir0_interpolation_segment
      hir_interpolation_segments[TEST_HIR_RECORDS];
  w_seed_hir0_terminator hir_terminators[TEST_HIR_RECORDS];
  w_seed_hir0_entry hir_entries[TEST_HIR_RECORDS];
  w_seed_hir0_external_module hir_external_modules[2];
  w_seed_hir0_external_symbol hir_external_symbols[8];
  w_seed_hir0_cleanup hir_cleanups[TEST_HIR_RECORDS];
  uint8_t hir_text[TEST_HIR_TEXT];
  uint8_t hir_value_bytes[TEST_HIR_VALUES];
  uint8_t hir_receipt[TEST_HIR_RECEIPT];
  w_seed_hir0_output hir_output;
  w_seed_hir0_result hir_result;
  w_seed_hir0_program hir_program;
} mlir_fixture;

static mlir_fixture fixture;

static uint32_t edge_value_at(const w_seed_hir0_program *program,
                              size_t terminator_index) {
  const w_seed_hir0_terminator *terminator =
      &program->terminators[terminator_index];
  if (terminator->edge_argument_count == 0u ||
      terminator->first_edge_argument == W_SEED_HIR0_NONE)
    return W_SEED_HIR0_NONE;
  return program->edge_arguments[terminator->first_edge_argument].value_index;
}

static uint32_t edge_value_for(const w_seed_hir0_program *program,
                               const w_seed_hir0_terminator *terminator) {
  return edge_value_at(
      program, (size_t)(terminator - program->terminators));
}

#define EDGE_VALUE_SLOT(program, terminator_index)                            \
  ((program)->edge_arguments[(program)->terminators[(terminator_index)]       \
                                 .first_edge_argument]                        \
       .value_index)

#define FIXTURE_EDGE_VALUE_SLOT(terminator_index)                             \
  (fixture.hir_edge_arguments[fixture.hir_terminators[(terminator_index)]      \
                                  .first_edge_argument]                        \
       .value_index)

static const w_seed_mlir0_target TARGET = {
    W_SEED_MLIR0_TARGET_X86_64_UNKNOWN_LINUX_GNU};
static const w_seed_mlir0_target WINDOWS_TARGET = {
    W_SEED_MLIR0_TARGET_X86_64_PC_WINDOWS_MSVC};

static bool process_frontend_mode;
static bool process_input_frontend_mode;
static bool process_input_float_rounding_mode;
static bool process_input_checked_integer_helper_mode;
static void configure_process_external(void);
static void configure_process_input_external(void);
static bool resolve_process_import(void);
static bool contains_bytes(const uint8_t *bytes, size_t length,
                           const char *needle);
static size_t count_bytes(const uint8_t *bytes, size_t length,
                          const char *needle);
static size_t find_bytes(const uint8_t *bytes, size_t length,
                         const char *needle, size_t start);

static bool parse_source(const uint8_t *source_bytes, size_t source_length) {
  if (source_bytes == NULL || source_length == 0u ||
      source_length >= sizeof(fixture.source_bytes))
    return false;
  (void)memset(&fixture, 0, sizeof(fixture));
  fixture.source_length = source_length;
  (void)memcpy(fixture.source_bytes, source_bytes, source_length);
  w_seed_source_error source_error;
  if (!w_seed_source_init(
          (w_seed_byte_view){fixture.source_bytes, fixture.source_length},
          &fixture.source, &source_error))
    return false;
  w_seed_lex_error lex_error;
  if (!w_seed_parser_init(
          &fixture.source, (w_seed_span){0u, fixture.source_length},
          (w_seed_foreign_limits){65536u, 256u}, fixture.lexer_frames,
          TEST_LEXER_FRAMES, fixture.tokens, TEST_TOKENS, fixture.nodes,
          TEST_NODES, fixture.parse_frames, TEST_PARSE_FRAMES, fixture.issues,
          TEST_ISSUES, &fixture.parser, &lex_error) ||
      !w_seed_parser_parse(&fixture.parser, &fixture.parse))
    return false;
  fixture.document = (w_seed_frontend_document){
      .logical_source_id = (w_seed_frontend_text){"mlir0-test", 10u},
      .module_id = (w_seed_frontend_text){"mlir0-test", 10u},
      .local_module_name = (w_seed_frontend_text){"mlir0-test", 10u},
      .source = &fixture.source,
      .nodes = fixture.nodes,
      .node_count = fixture.parse.node_count,
      .parse = fixture.parse};
  fixture.input = (w_seed_frontend_input){
      .documents = &fixture.document,
      .document_count = 1u,
      .external_modules = NULL,
      .external_module_count = 0u,
      .host_scope = NULL,
      .import_resolution_complete = false,
      .resolved_imports = NULL,
      .resolved_import_count = 0u};
  fixture.output = (w_seed_frontend_output){
      .modules = fixture.modules,
      .module_capacity = TEST_MODULES,
      .imports = fixture.imports,
      .import_capacity = TEST_IMPORTS,
      .import_items = fixture.import_items,
      .import_item_capacity = TEST_IMPORT_ITEMS,
      .structs = fixture.structs,
      .struct_capacity = TEST_STRUCTS,
      .fields = fixture.fields,
      .field_capacity = TEST_FIELDS,
      .enums = fixture.enums,
      .enum_capacity = TEST_HIR_RECORDS,
      .enum_cases = fixture.enum_cases,
      .enum_case_capacity = TEST_HIR_RECORDS,
      .enum_case_parameters = fixture.enum_case_parameters,
      .enum_case_parameter_capacity = TEST_HIR_RECORDS,
      .switch_arms = fixture.switch_arms,
      .switch_arm_capacity = TEST_HIR_RECORDS,
      .pattern_captures = fixture.pattern_captures,
      .pattern_capture_capacity = TEST_HIR_RECORDS,
      .enum_subset_members = fixture.enum_subset_members,
      .enum_subset_member_capacity = TEST_HIR_RECORDS,
      .type_declarations = fixture.type_declarations,
      .type_declaration_capacity = TEST_STRUCTS,
      .aliases = fixture.aliases,
      .alias_capacity = TEST_STRUCTS,
      .types = fixture.types,
      .type_capacity = TEST_TYPES,
      .functions = fixture.functions,
      .function_capacity = TEST_FUNCTIONS,
      .parameters = fixture.parameters,
      .parameter_capacity = TEST_PARAMETERS,
      .entries = fixture.entries,
      .entry_capacity = TEST_ENTRIES,
      .statements = fixture.statements,
      .statement_capacity = TEST_STATEMENTS,
      .expressions = fixture.expressions,
      .expression_capacity = TEST_EXPRESSIONS,
      .arguments = fixture.arguments,
      .argument_capacity = TEST_ARGUMENTS,
      .tuple_components = fixture.tuple_components,
      .tuple_component_capacity = TEST_TUPLE_COMPONENTS,
      .tuple_elements = fixture.tuple_elements,
      .tuple_element_capacity = TEST_TUPLE_ELEMENTS,
      .interpolation_segments = fixture.interpolation_segments,
      .interpolation_segment_capacity = TEST_EXPRESSIONS,
      .symbols = fixture.symbols,
      .symbol_capacity = TEST_SYMBOLS,
      .facts = fixture.facts,
      .fact_capacity = TEST_FACTS,
      .diagnostics = fixture.diagnostics,
      .diagnostic_capacity = TEST_DIAGNOSTICS,
      .diagnostic_facts = fixture.diagnostic_facts,
      .diagnostic_fact_capacity = TEST_DIAGNOSTICS * 5u,
      .diagnostic_items = fixture.diagnostic_items,
      .diagnostic_item_capacity = TEST_DIAGNOSTICS * 4u,
      .diagnostic_labels = fixture.diagnostic_labels,
      .diagnostic_label_capacity = TEST_DIAGNOSTICS * 2u,
      .const_bytes = fixture.const_bytes,
      .const_bytes_capacity = sizeof(fixture.const_bytes),
      .receipt = fixture.frontend_receipt,
      .receipt_capacity = sizeof(fixture.frontend_receipt)};
  fixture.host_requirements[0] = (w_seed_frontend_host_requirement){
      .name = (w_seed_frontend_text){"Console", 7u}};
  fixture.host_parameters[0] = (w_seed_frontend_external_parameter){
      .name = (w_seed_frontend_text){"message", 7u},
      .type = (w_seed_frontend_text){"String", 6u},
      .label_kind = W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY};
  fixture.host_symbols[0] = (w_seed_frontend_host_prelude_symbol){
      .name = (w_seed_frontend_text){"noop", 4u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"()", 2u},
      .is_const = false,
      .requirements = NULL,
      .requirement_count = 0u};
  fixture.host_symbols[1] = (w_seed_frontend_host_prelude_symbol){
      .name = (w_seed_frontend_text){"print", 5u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .parameters = fixture.host_parameters,
      .parameter_count = 1u,
      .return_type = (w_seed_frontend_text){"()", 2u},
      .is_const = false,
      .requirements = fixture.host_requirements,
      .requirement_count = 1u};
  fixture.host_scope = (w_seed_frontend_host_prelude){
      .profile = (w_seed_frontend_text){"native-process@1", 16u},
      .symbols = fixture.host_symbols,
      .symbol_count = 2u};
  fixture.input.host_scope = &fixture.host_scope;
  if (process_frontend_mode) {
    configure_process_external();
    if (process_input_frontend_mode) configure_process_input_external();
    if (!resolve_process_import()) return false;
  }
  const w_seed_frontend_status status =
      w_seed_frontend_run(&fixture.input, &fixture.output,
                          &fixture.frontend_result);
  if (status != W_SEED_FRONTEND_OK)
    (void)fprintf(stderr,
                  "mlir0 frontend status=%d parse=%d issues=%lu required expressions=%lu written=%lu diagnostics=%lu\n",
                  (int)status, (int)fixture.parse.status,
                  (unsigned long)fixture.parse.issue_count,
                  (unsigned long)fixture.frontend_result.required.expressions,
                  (unsigned long)fixture.frontend_result.written.expressions,
                  (unsigned long)fixture.frontend_result.written.diagnostics);
  return status == W_SEED_FRONTEND_OK;
}

static void configure_process_external(void) {
  static const w_seed_frontend_text empty = {NULL, 0u};
  fixture.external_symbols[0] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"Arguments", 9u},
      .kind = W_SEED_FRONTEND_EXTERNAL_TYPE,
      .exported = true,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"Arguments", 9u},
      .is_const = false,
      .receiver_type = empty};
  fixture.external_symbols[1] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"Context", 7u},
      .kind = W_SEED_FRONTEND_EXTERNAL_TYPE,
      .exported = true,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"Context", 7u},
      .is_const = false,
      .receiver_type = empty};
  fixture.external_symbols[2] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"ExitCode", 8u},
      .kind = W_SEED_FRONTEND_EXTERNAL_TYPE,
      .exported = true,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"ExitCode", 8u},
      .is_const = false,
      .receiver_type = empty};
  fixture.external_symbols[3] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"success", 7u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .exported = true,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"ExitCode", 8u},
      .is_const = true,
      .receiver_type = (w_seed_frontend_text){"ExitCode", 8u}};
  fixture.external_modules[0] = (w_seed_frontend_external_module){
      .module_id = (w_seed_frontend_text){"std.process", 11u},
      .symbols = fixture.external_symbols,
      .symbol_count = 4u};
  fixture.input.external_modules = fixture.external_modules;
  fixture.input.external_module_count = 1u;
}

static void configure_process_input_external(void) {
  fixture.external_parameters[0] = (w_seed_frontend_external_parameter){
      .name = (w_seed_frontend_text){"code", 4u},
      .type = (w_seed_frontend_text){"i64", 3u},
      .label_kind = W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY};
  fixture.external_symbols[4] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"isEmpty", 7u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .exported = true,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"Bool", 4u},
      .is_const = true,
      .receiver_type = (w_seed_frontend_text){"Arguments", 9u}};
  fixture.external_symbols[5] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"failure", 7u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .exported = true,
      .parameters = fixture.external_parameters,
      .parameter_count = 1u,
      .return_type = (w_seed_frontend_text){"ExitCode", 8u},
      .is_const = true,
      .receiver_type = (w_seed_frontend_text){"ExitCode", 8u}};
  fixture.external_symbols[6] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"count", 5u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .exported = true,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"usize", 5u},
      .is_const = true,
      .receiver_type = (w_seed_frontend_text){"Arguments", 9u}};
  fixture.external_modules[0].symbol_count = 7u;
}

static bool resolve_process_import(void) {
  w_seed_module_origin origins[4];
  w_seed_module_scan_result scan_result;
  CHECK(w_seed_module_scan(&fixture.source, fixture.nodes,
                          fixture.parse.node_count, &fixture.parse, origins,
                          4u, &scan_result) == W_SEED_MODULE_SCAN_OK);
  CHECK(scan_result.written == 1u);
  fixture.resolved_imports[0] = (w_seed_frontend_resolved_import){
      .source_document_index = 0u,
      .direct_import_ordinal = origins[0].direct_import_ordinal,
      .import_declaration_span = origins[0].declaration_span,
      .target_kind = W_SEED_FRONTEND_RESOLVED_IMPORT_EXTERNAL_MODULE,
      .target_index = 0u};
  fixture.input.import_resolution_complete = true;
  fixture.input.resolved_imports = fixture.resolved_imports;
  fixture.input.resolved_import_count = 1u;
  return true;
}

static bool parse_process_source(const uint8_t *source_bytes,
                                 size_t source_length) {
  process_frontend_mode = true;
  const bool parsed = parse_source(source_bytes, source_length);
  process_frontend_mode = false;
  return parsed;
}

static bool lower_hir(const uint8_t *source_bytes, size_t source_length) {
  CHECK(parse_source(source_bytes, source_length));
  fixture.hir_output = (w_seed_hir0_output){
      .modules = fixture.hir_modules,
      .module_capacity = TEST_HIR_RECORDS,
      .identities = fixture.hir_identities,
      .identity_capacity = TEST_HIR_IDENTITIES,
      .types = fixture.hir_types,
      .type_capacity = TEST_HIR_RECORDS,
      .enums = fixture.hir_enums,
      .enum_capacity = TEST_HIR_RECORDS,
      .enum_cases = fixture.hir_enum_cases,
      .enum_case_capacity = TEST_HIR_RECORDS,
      .enum_case_parameters = fixture.hir_enum_case_parameters,
      .enum_case_parameter_capacity = TEST_HIR_RECORDS,
      .enum_payloads = fixture.hir_enum_payloads,
      .enum_payload_capacity = TEST_HIR_RECORDS,
      .enum_subset_members = fixture.hir_enum_subset_members,
      .enum_subset_member_capacity = TEST_HIR_RECORDS,
      .functions = fixture.hir_functions,
      .function_capacity = TEST_HIR_RECORDS,
      .parameters = fixture.hir_parameters,
      .parameter_capacity = TEST_HIR_RECORDS,
      .blocks = fixture.hir_blocks,
      .block_capacity = TEST_HIR_RECORDS,
      .block_arguments = fixture.hir_block_arguments,
      .block_argument_capacity = TEST_HIR_RECORDS,
      .edge_arguments = fixture.hir_edge_arguments,
      .edge_argument_capacity = TEST_HIR_RECORDS,
      .switch_edges = fixture.hir_switch_edges,
      .switch_edge_capacity = TEST_HIR_RECORDS,
      .switch_captures = fixture.hir_switch_captures,
      .switch_capture_capacity = TEST_HIR_RECORDS,
      .instructions = fixture.hir_instructions,
      .instruction_capacity = TEST_HIR_RECORDS,
      .bindings = fixture.hir_bindings,
      .binding_capacity = TEST_HIR_RECORDS,
      .calls = fixture.hir_calls,
      .call_capacity = TEST_HIR_RECORDS,
      .host_parameters = fixture.hir_host_parameters,
      .host_parameter_capacity = TEST_HIR_RECORDS,
      .arguments = fixture.hir_arguments,
      .argument_capacity = TEST_HIR_RECORDS,
      .requirements = fixture.hir_requirements,
      .requirement_capacity = TEST_HIR_RECORDS,
      .values = fixture.hir_values,
      .value_capacity = TEST_HIR_RECORDS,
      .interpolation_segments = fixture.hir_interpolation_segments,
      .interpolation_segment_capacity = TEST_HIR_RECORDS,
      .terminators = fixture.hir_terminators,
      .terminator_capacity = TEST_HIR_RECORDS,
      .entries = fixture.hir_entries,
      .entry_capacity = TEST_HIR_RECORDS,
      .text_bytes = fixture.hir_text,
      .text_byte_capacity = sizeof(fixture.hir_text),
      .value_bytes = fixture.hir_value_bytes,
      .value_byte_capacity = sizeof(fixture.hir_value_bytes),
      .receipt = fixture.hir_receipt,
      .receipt_capacity = sizeof(fixture.hir_receipt),
      .external_modules = fixture.hir_external_modules,
      .external_module_capacity = 2u,
      .external_symbols = fixture.hir_external_symbols,
      .external_symbol_capacity = 8u,
      .cleanups = fixture.hir_cleanups,
      .cleanup_capacity = TEST_HIR_RECORDS};
  const w_seed_hir0_input input = {
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.frontend_result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  w_seed_hir0_counts counts;
  w_seed_hir0_result result;
  const w_seed_hir0_status measured_status =
      w_seed_hir0_measure(&input, &counts, &result);
  CHECK(measured_status == W_SEED_HIR0_OK);
  const w_seed_hir0_status hir_status =
      w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result);
  CHECK(hir_status == W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output,
                                        &fixture.hir_result,
                                        &fixture.hir_program));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool lower_process_hir(const uint8_t *source_bytes,
                              size_t source_length) {
  CHECK(parse_process_source(source_bytes, source_length));
  fixture.hir_output = (w_seed_hir0_output){
      .modules = fixture.hir_modules,
      .module_capacity = TEST_HIR_RECORDS,
      .identities = fixture.hir_identities,
      .identity_capacity = TEST_HIR_IDENTITIES,
      .types = fixture.hir_types,
      .type_capacity = TEST_HIR_RECORDS,
      .enums = fixture.hir_enums,
      .enum_capacity = TEST_HIR_RECORDS,
      .enum_cases = fixture.hir_enum_cases,
      .enum_case_capacity = TEST_HIR_RECORDS,
      .enum_subset_members = fixture.hir_enum_subset_members,
      .enum_subset_member_capacity = TEST_HIR_RECORDS,
      .functions = fixture.hir_functions,
      .function_capacity = TEST_HIR_RECORDS,
      .parameters = fixture.hir_parameters,
      .parameter_capacity = TEST_HIR_RECORDS,
      .blocks = fixture.hir_blocks,
      .block_capacity = TEST_HIR_RECORDS,
      .block_arguments = fixture.hir_block_arguments,
      .block_argument_capacity = TEST_HIR_RECORDS,
      .edge_arguments = fixture.hir_edge_arguments,
      .edge_argument_capacity = TEST_HIR_RECORDS,
      .instructions = fixture.hir_instructions,
      .instruction_capacity = TEST_HIR_RECORDS,
      .bindings = fixture.hir_bindings,
      .binding_capacity = TEST_HIR_RECORDS,
      .calls = fixture.hir_calls,
      .call_capacity = TEST_HIR_RECORDS,
      .host_parameters = fixture.hir_host_parameters,
      .host_parameter_capacity = TEST_HIR_RECORDS,
      .arguments = fixture.hir_arguments,
      .argument_capacity = TEST_HIR_RECORDS,
      .requirements = fixture.hir_requirements,
      .requirement_capacity = TEST_HIR_RECORDS,
      .values = fixture.hir_values,
      .value_capacity = TEST_HIR_RECORDS,
      .interpolation_segments = fixture.hir_interpolation_segments,
      .interpolation_segment_capacity = TEST_HIR_RECORDS,
      .terminators = fixture.hir_terminators,
      .terminator_capacity = TEST_HIR_RECORDS,
      .entries = fixture.hir_entries,
      .entry_capacity = TEST_HIR_RECORDS,
      .text_bytes = fixture.hir_text,
      .text_byte_capacity = sizeof(fixture.hir_text),
      .value_bytes = fixture.hir_value_bytes,
      .value_byte_capacity = sizeof(fixture.hir_value_bytes),
      .receipt = fixture.hir_receipt,
      .receipt_capacity = sizeof(fixture.hir_receipt),
      .external_modules = fixture.hir_external_modules,
      .external_module_capacity = 2u,
      .external_symbols = fixture.hir_external_symbols,
      .external_symbol_capacity = 8u,
      .cleanups = fixture.hir_cleanups,
      .cleanup_capacity = TEST_HIR_RECORDS};
  const w_seed_hir0_input input = {
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.frontend_result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  w_seed_hir0_counts counts;
  w_seed_hir0_result result;
  CHECK(w_seed_hir0_measure(&input, &counts, &result) == W_SEED_HIR0_OK);
  bool has_panic = false;
  for (size_t index = 0u; index < fixture.frontend_result.written.expressions;
       index += 1u) {
    if (fixture.expressions[index].kind == W_SEED_FRONTEND_EXPR_PANIC) {
      has_panic = true;
      break;
    }
  }
  const size_t expected_external_symbols =
      process_input_frontend_mode ? 7u : 4u;
  const size_t expected_types =
      (process_input_frontend_mode
           ? (process_input_float_rounding_mode
                  ? 11u
                  : (process_input_checked_integer_helper_mode ? 10u : 8u))
           : 7u) +
      (has_panic ? 1u : 0u);
  CHECK(counts.external_modules == 1u &&
        counts.external_symbols == expected_external_symbols &&
        counts.types == expected_types);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output,
                                        &fixture.hir_result,
                                        &fixture.hir_program));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool lower_process_input_hir(const uint8_t *source_bytes,
                                    size_t source_length) {
  const bool float_rounding = process_input_float_rounding_mode;
  process_input_frontend_mode = true;
  const bool lowered = lower_process_hir(source_bytes, source_length);
  process_input_frontend_mode = false;
  if (!lowered) return false;
  bool has_never = false;
  for (size_t index = 0u; index < fixture.hir_program.type_count;
       index += 1u) {
    if (fixture.hir_program.types[index].kind == W_SEED_HIR0_TYPE_NEVER) {
      has_never = true;
      break;
    }
  }
  return fixture.hir_program.external_module_count == 1u &&
         fixture.hir_program.external_symbol_count == 7u &&
         fixture.hir_program.type_count ==
             (float_rounding
                  ? 11u
                  : (process_input_checked_integer_helper_mode
                         ? 10u
                         : (has_never ? 9u : 8u)));
}

static w_seed_mlir0_input mlir_input(void) {
  return (w_seed_mlir0_input){
      &fixture.hir_program, &fixture.hir_result,
      W_SEED_MLIR0_ARTIFACT_EXECUTABLE};
}

static w_seed_mlir0_input process_mlir_input(void) {
  return (w_seed_mlir0_input){
      &fixture.hir_program, &fixture.hir_result,
      W_SEED_MLIR0_ARTIFACT_PROCESS_HANDLER};
}

static bool test_process_hir_is_closed_to_mlir(void) {
  static const uint8_t SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return .success }\n"
      "entry(run)\n";
  CHECK(lower_process_hir(SOURCE, sizeof(SOURCE) - 1u));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  CHECK(fixture.hir_program.functions[0].direct_entry ==
        W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE);

  /* HIR/MLIR consumption must not retain frontend or source storage. */
  (void)memset(fixture.source_bytes, 0, sizeof(fixture.source_bytes));
  (void)memset(&fixture.source, 0, sizeof(fixture.source));
  (void)memset(&fixture.document, 0, sizeof(fixture.document));
  (void)memset(&fixture.input, 0, sizeof(fixture.input));
  (void)memset(&fixture.output, 0, sizeof(fixture.output));
  (void)memset(&fixture.frontend_result, 0, sizeof(fixture.frontend_result));
  (void)memset(fixture.external_symbols, 0, sizeof(fixture.external_symbols));
  (void)memset(fixture.external_modules, 0, sizeof(fixture.external_modules));
  (void)memset(fixture.resolved_imports, 0,
               sizeof(fixture.resolved_imports));

  const w_seed_mlir0_input input = mlir_input();
  w_seed_mlir0_counts counts = {0x11u};
  w_seed_mlir0_result measure_result;
  (void)memset(&measure_result, 0x44, sizeof(measure_result));
  const w_seed_mlir0_counts counts_before = counts;
  const w_seed_mlir0_result measure_before = measure_result;
  CHECK(w_seed_mlir0_measure(&input, &TARGET, &counts, &measure_result) ==
        W_SEED_MLIR0_UNSUPPORTED);
  CHECK(memcmp(&counts, &counts_before, sizeof(counts_before)) == 0 &&
        memcmp(&measure_result, &measure_before, sizeof(measure_before)) == 0);

  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  (void)memset(output, 0xa5u, sizeof(output));
  w_seed_mlir0_result emit_result;
  (void)memset(&emit_result, 0x5a, sizeof(emit_result));
  const w_seed_mlir0_result emit_before = emit_result;
  CHECK(w_seed_mlir0_emit(&input, &TARGET,
                         &(w_seed_mlir0_output){output, sizeof(output)},
                         &emit_result) == W_SEED_MLIR0_UNSUPPORTED);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0xa5u);
  CHECK(memcmp(&emit_result, &emit_before, sizeof(emit_before)) == 0);

  const w_seed_mlir0_input process_input = process_mlir_input();
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  CHECK(w_seed_mlir0_measure(&process_input, &TARGET, &counts,
                             &measure_result) == W_SEED_MLIR0_OK);
  CHECK(counts.mlir_bytes != 0u &&
        measure_result.required.mlir_bytes == counts.mlir_bytes);
  (void)memset(output, 0xa5u, sizeof(output));
  w_seed_mlir0_result process_result;
  CHECK(w_seed_mlir0_emit(
            &process_input, &TARGET,
            &(w_seed_mlir0_output){output, counts.mlir_bytes}, &process_result) ==
        W_SEED_MLIR0_OK);
  CHECK(process_result.written.mlir_bytes == counts.mlir_bytes &&
        contains_bytes(output, process_result.written.mlir_bytes,
                       "// " W_SEED_MLIR0_PROCESS_SCHEMA_VERSION "\n") &&
        contains_bytes(output, process_result.written.mlir_bytes,
                       "llvm.target_triple = \"" W_SEED_MLIR0_TARGET_TRIPLE
                       "\"") &&
        contains_bytes(output, process_result.written.mlir_bytes,
                       "llvm.func @w_seed_process_entry0_handler(%arguments: !llvm.ptr, %context: !llvm.ptr) -> i32") &&
        contains_bytes(output, process_result.written.mlir_bytes,
                       "llvm.call @w_seed_process_entry0_context_drop(%context)") &&
        contains_bytes(output, process_result.written.mlir_bytes,
                       "llvm.call @w_seed_process_entry0_arguments_drop(%arguments)") &&
        !contains_bytes(output, process_result.written.mlir_bytes,
                        "llvm.func @main") &&
        !contains_bytes(output, process_result.written.mlir_bytes,
                        "mainCRTStartup") &&
        !contains_bytes(output, process_result.written.mlir_bytes,
                        "GetStdHandle"));
  const size_t context_call = find_bytes(
      output, process_result.written.mlir_bytes,
      "llvm.call @w_seed_process_entry0_context_drop", 0u);
  const size_t arguments_call = find_bytes(
      output, process_result.written.mlir_bytes,
      "llvm.call @w_seed_process_entry0_arguments_drop", 0u);
  CHECK(context_call != SIZE_MAX && arguments_call != SIZE_MAX &&
        context_call < arguments_call);

  (void)memset(output, 0xb6u, sizeof(output));
  CHECK(w_seed_mlir0_emit(
            &process_input, &WINDOWS_TARGET,
            &(w_seed_mlir0_output){output, sizeof(output)}, &process_result) ==
        W_SEED_MLIR0_OK);
  CHECK(contains_bytes(output, process_result.written.mlir_bytes,
                       "llvm.target_triple = \"" W_SEED_MLIR0_TARGET_TRIPLE_WINDOWS
                       "\"") &&
        !contains_bytes(output, process_result.written.mlir_bytes,
                        "mainCRTStartup"));

  (void)memset(output, 0xc7u, sizeof(output));
  (void)memset(&process_result, 0x7cu, sizeof(process_result));
  uint8_t process_snapshot[sizeof(process_result)];
  (void)memcpy(process_snapshot, &process_result, sizeof(process_snapshot));
  CHECK(w_seed_mlir0_emit(
            &process_input, &TARGET,
            &(w_seed_mlir0_output){output, counts.mlir_bytes - 1u},
            &process_result) == W_SEED_MLIR0_CAPACITY);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0xc7u);
  CHECK(memcmp(&process_result, process_snapshot, sizeof(process_snapshot)) ==
        0);

  (void)memset(output, 0xd8u, sizeof(output));
  (void)memset(&process_result, 0x8du, sizeof(process_result));
  uint8_t process_alias_snapshot[sizeof(process_result)];
  (void)memcpy(process_alias_snapshot, &process_result,
               sizeof(process_alias_snapshot));
  uint8_t external_symbols_snapshot[sizeof(fixture.hir_external_symbols)];
  (void)memcpy(external_symbols_snapshot, fixture.hir_external_symbols,
               sizeof(external_symbols_snapshot));
  CHECK(w_seed_mlir0_emit(
            &process_input, &TARGET,
            &(w_seed_mlir0_output){(uint8_t *)&fixture.hir_external_symbols,
                                   sizeof(fixture.hir_external_symbols)},
            &process_result) == W_SEED_MLIR0_ALIAS);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0xd8u);
  CHECK(memcmp(&process_result, process_alias_snapshot,
               sizeof(process_alias_snapshot)) == 0);
  CHECK(memcmp(fixture.hir_external_symbols, external_symbols_snapshot,
               sizeof(external_symbols_snapshot)) == 0);

  (void)memset(output, 0xe9u, sizeof(output));
  (void)memset(&process_result, 0x3au, sizeof(process_result));
  uint8_t external_modules_snapshot[sizeof(fixture.hir_external_modules)];
  (void)memcpy(external_modules_snapshot, fixture.hir_external_modules,
               sizeof(external_modules_snapshot));
  uint8_t module_alias_result_snapshot[sizeof(process_result)];
  (void)memcpy(module_alias_result_snapshot, &process_result,
               sizeof(module_alias_result_snapshot));
  CHECK(w_seed_mlir0_emit(
            &process_input, &TARGET,
            &(w_seed_mlir0_output){(uint8_t *)&fixture.hir_external_modules,
                                   sizeof(fixture.hir_external_modules)},
            &process_result) == W_SEED_MLIR0_ALIAS);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0xe9u);
  CHECK(memcmp(&process_result, module_alias_result_snapshot,
               sizeof(module_alias_result_snapshot)) == 0);
  CHECK(memcmp(fixture.hir_external_modules, external_modules_snapshot,
               sizeof(external_modules_snapshot)) == 0);

  const w_seed_hir0_entry saved_entry = fixture.hir_entries[0];
  fixture.hir_entries[0].adapter_kind =
      W_SEED_HIR0_ENTRY_ADAPTER_DEFAULT_UNIT;
  CHECK(w_seed_mlir0_measure(&process_input, &TARGET, &counts,
                             &measure_result) == W_SEED_MLIR0_INVALID_HIR);
  fixture.hir_entries[0] = saved_entry;
  const w_seed_hir0_type saved_type = fixture.hir_types[4];
  fixture.hir_types[4].release_contract =
      W_SEED_HIR0_RELEASE_CONTRACT_UNKNOWN;
  CHECK(w_seed_mlir0_measure(&process_input, &TARGET, &counts,
                             &measure_result) == W_SEED_MLIR0_INVALID_HIR);
  fixture.hir_types[4] = saved_type;
  const w_seed_hir0_function saved_function = fixture.hir_functions[0];
  fixture.hir_functions[0].direct_entry = W_SEED_HIR0_DIRECT_ENTRY_ABSENT;
  CHECK(w_seed_mlir0_measure(&process_input, &TARGET, &counts,
                             &measure_result) == W_SEED_MLIR0_INVALID_HIR);
  fixture.hir_functions[0] = saved_function;
  fixture.hir_functions[0].is_throws = true;
  CHECK(w_seed_mlir0_measure(&process_input, &TARGET, &counts,
                             &measure_result) == W_SEED_MLIR0_INVALID_HIR);
  fixture.hir_functions[0] = saved_function;

  const w_seed_mlir0_input invalid_kind = {
      &fixture.hir_program, &fixture.hir_result,
      (w_seed_mlir0_artifact_kind)-1};
  CHECK(w_seed_mlir0_measure(&invalid_kind, &TARGET, &counts,
                             &measure_result) == W_SEED_MLIR0_UNSUPPORTED);
  return true;
}

static bool measure_current(w_seed_mlir0_counts *counts,
                            w_seed_mlir0_result *result) {
  const w_seed_mlir0_input input = mlir_input();
  return w_seed_mlir0_measure(&input, &TARGET, counts, result) ==
         W_SEED_MLIR0_OK;
}

static bool emit_current(uint8_t *bytes, size_t capacity,
                         w_seed_mlir0_result *result) {
  const w_seed_mlir0_input input = mlir_input();
  return w_seed_mlir0_emit(&input, &TARGET,
                           &(w_seed_mlir0_output){bytes, capacity}, result) ==
         W_SEED_MLIR0_OK;
}

static bool test_reachable_panic_mlir(void) {
  static const uint8_t ordinary_source[] =
      "entry { panic(\"ordinary panic\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  CHECK(lower_hir(ordinary_source, sizeof(ordinary_source) - 1u));
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_reachable_panic && selection.maximum_stdout_bytes == 0u);
  w_seed_mlir0_input input = mlir_input();
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result result;
  CHECK(w_seed_mlir0_measure(&input, &TARGET, &counts, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_emit(
            &input, &TARGET,
            &(w_seed_mlir0_output){artifact, sizeof(artifact)}, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(result.written.mlir_bytes == counts.mlir_bytes &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.target_triple = \"" W_SEED_MLIR0_TARGET_TRIPLE
                       "\"") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "    \"llvm.intr.trap\"() : () -> ()\n"
                       "    llvm.unreachable\n") &&
        !contains_bytes(artifact, result.written.mlir_bytes, "ordinary panic"));
  CHECK(w_seed_mlir0_emit(
            &input, &WINDOWS_TARGET,
            &(w_seed_mlir0_output){artifact, sizeof(artifact)}, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.target_triple = \""
                       W_SEED_MLIR0_TARGET_TRIPLE_WINDOWS "\"") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "    \"llvm.intr.trap\"() : () -> ()\n"
                       "    llvm.unreachable\n") &&
        !contains_bytes(artifact, result.written.mlir_bytes, "ordinary panic"));

  static const uint8_t dead_source[] =
      "fn dead() { panic(\"dead panic\") }\nentry {}\n";
  CHECK(lower_hir(dead_source, sizeof(dead_source) - 1u));
  CHECK(w_seed_native_subset0_select_program(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_UNSUPPORTED);
  input = mlir_input();
  CHECK(w_seed_mlir0_measure(&input, &TARGET, &counts, &result) ==
        W_SEED_MLIR0_UNSUPPORTED);

  CHECK(lower_hir(ordinary_source, sizeof(ordinary_source) - 1u));
  const w_seed_hir0_terminator saved_terminator = fixture.hir_terminators[0];
  fixture.hir_terminators[0].panic_code = W_SEED_HIR0_PANIC_CODE_INVALID;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_terminators[0] = saved_terminator;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  input = mlir_input();
  CHECK(w_seed_mlir0_emit(
            &input, &TARGET,
            &(w_seed_mlir0_output){artifact, sizeof(artifact)}, &result) ==
        W_SEED_MLIR0_OK);
  const size_t written = result.written.mlir_bytes;
  const w_seed_mlir0_result result_snapshot = result;
  (void)memset(artifact, 0xa5u, sizeof(artifact));
  CHECK(w_seed_mlir0_emit(
            &input, &TARGET, &(w_seed_mlir0_output){artifact, written - 1u},
            &result) == W_SEED_MLIR0_CAPACITY);
  for (size_t index = 0u; index < sizeof(artifact); index += 1u)
    CHECK(artifact[index] == 0xa5u);
  CHECK(memcmp(&result, &result_snapshot, sizeof(result)) == 0);
  uint8_t values_snapshot[sizeof(fixture.hir_values)];
  (void)memcpy(values_snapshot, fixture.hir_values, sizeof(values_snapshot));
  CHECK(w_seed_mlir0_emit(
            &input, &TARGET,
            &(w_seed_mlir0_output){(uint8_t *)fixture.hir_values,
                                   sizeof(fixture.hir_values)},
            &result) == W_SEED_MLIR0_ALIAS);
  CHECK(memcmp(fixture.hir_values, values_snapshot, sizeof(values_snapshot)) ==
        0);
  return true;
}

static bool test_process_panic_mlir(void) {
  static const uint8_t source[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { panic(\"process panic\") }\n"
      "entry(run)\n";
  CHECK(lower_process_input_hir(source, sizeof(source) - 1u));
  const w_seed_mlir0_input input = {
      &fixture.hir_program, &fixture.hir_result,
      W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE};
  w_seed_native_subset0_process selection;
  CHECK(w_seed_native_subset0_select_process_executable(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_reachable_panic && selection.maximum_stdout_bytes == 0u);
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result result;
  CHECK(w_seed_mlir0_measure(&input, &TARGET, &counts, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_emit(
            &input, &TARGET,
            &(w_seed_mlir0_output){artifact, sizeof(artifact)}, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(result.written.mlir_bytes == counts.mlir_bytes &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.target_triple = \"" W_SEED_MLIR0_TARGET_TRIPLE
                       "\"") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "    \"llvm.intr.trap\"() : () -> ()\n"
                       "    llvm.unreachable\n") &&
        !contains_bytes(artifact, result.written.mlir_bytes, "process panic"));
  CHECK(w_seed_mlir0_emit(
            &input, &WINDOWS_TARGET,
            &(w_seed_mlir0_output){artifact, sizeof(artifact)}, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.target_triple = \""
                       W_SEED_MLIR0_TARGET_TRIPLE_WINDOWS "\"") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "    \"llvm.intr.trap\"() : () -> ()\n"
                       "    llvm.unreachable\n") &&
        !contains_bytes(artifact, result.written.mlir_bytes, "process panic"));

  static const uint8_t branch_source[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { if true { panic(\"process branch\") } "
      "else { return .success } }\n"
      "entry(run)\n";
  CHECK(lower_process_input_hir(branch_source, sizeof(branch_source) - 1u));
  CHECK(w_seed_native_subset0_select_process_executable(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_reachable_panic);
  CHECK(w_seed_mlir0_emit(
            &input, &TARGET,
            &(w_seed_mlir0_output){artifact, sizeof(artifact)}, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.cond_br") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "    \"llvm.intr.trap\"() : () -> ()\n"
                       "    llvm.unreachable\n") &&
        !contains_bytes(artifact, result.written.mlir_bytes, "process branch"));

  return true;
}

static bool test_process_checked_integer_helper_fault_mlir(void) {
  static const uint8_t source[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "fn checkedOffset(value: u8): u8 { return value + 255_u8 }\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode throws NumericConversionError { "
      "let count = try u8(exactly: args.count)\n"
      "print(\"Begin ${checkedOffset(value: count)}\")\n"
      "return .success }\n"
      "entry(run)\n";
  process_input_checked_integer_helper_mode = true;
  const bool lowered =
      lower_process_input_hir(source, sizeof(source) - 1u);
  process_input_checked_integer_helper_mode = false;
  CHECK(lowered);
  const w_seed_mlir0_input input = {
      &fixture.hir_program, &fixture.hir_result,
      W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE};
  w_seed_native_subset0_process selection;
  CHECK(w_seed_native_subset0_select_process_executable(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_integer_exactly &&
        selection.checked_fault_relation.present &&
        selection.checked_fault_relation.conversion_failure_status == 1u &&
        selection.checked_fault_relation.arithmetic_failure_status == 2u &&
        selection.checked_fault_relation.operation_count == 1u &&
        w_seed_native_subset0_verify_process_checked_fault_relation(
            &fixture.hir_program, &fixture.hir_result, &selection));
  w_seed_native_subset0_process forged_selection = selection;
  forged_selection.checked_fault_relation.arithmetic_failure_status = 1u;
  CHECK(!w_seed_native_subset0_verify_process_checked_fault_relation(
      &fixture.hir_program, &fixture.hir_result, &forged_selection));
  forged_selection = selection;
  (void)memset(&forged_selection.checked_fault_relation, 0,
               sizeof(forged_selection.checked_fault_relation));
  forged_selection.checked_fault_relation.source_conversion_terminator_index =
      W_SEED_HIR0_NONE;
  forged_selection.checked_fault_relation.normal_successor_block_index =
      W_SEED_HIR0_NONE;
  CHECK(!w_seed_native_subset0_verify_process_checked_fault_relation(
      &fixture.hir_program, &fixture.hir_result, &forged_selection));
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result result;
  CHECK(w_seed_mlir0_measure(&input, &TARGET, &counts, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_emit(
            &input, &TARGET,
            &(w_seed_mlir0_output){artifact, sizeof(artifact)}, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(result.written.mlir_bytes == counts.mlir_bytes);
  CHECK(contains_bytes(artifact, result.written.mlir_bytes,
                       "@w_seed_process_checked_add_u64(%left: i64, %right: i64, %width: i64, %fault: !llvm.ptr) -> i64"));
  CHECK(contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.store %process_fault_one, %fault : i64, !llvm.ptr"));
  CHECK(contains_bytes(artifact, result.written.mlir_bytes,
                       ", %fault_address) : (i64, i64, i64, !llvm.ptr) -> i64"));
  CHECK(contains_bytes(artifact, result.written.mlir_bytes,
                       "%process_checked_fault_status = llvm.mlir.constant(2 : i32)"));
  CHECK(contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.store %process_zero, %process_fault_address"));
  CHECK(contains_bytes(artifact, result.written.mlir_bytes,
                       "\\42\\65\\67\\69\\6e\\20"));
  CHECK(!contains_bytes(artifact, result.written.mlir_bytes,
                        "@w_seed_checked_add_u64"));
  CHECK(!contains_bytes(artifact, result.written.mlir_bytes,
                        "llvm.intr.trap"));
  size_t context_release = find_bytes(
      artifact, result.written.mlir_bytes,
      "llvm.call @w_seed_process_context_drop(", 0u);
  size_t arguments_release = find_bytes(
      artifact, result.written.mlir_bytes,
      "llvm.call @w_seed_process_arguments_drop(", 0u);
  size_t root_finalize = find_bytes(
      artifact, result.written.mlir_bytes,
      "llvm.call @w_seed_process_root_finalize(", 0u);
  size_t stdout_write = find_bytes(artifact, result.written.mlir_bytes,
                                   "llvm.call @write(", 0u);
  CHECK(count_bytes(artifact, result.written.mlir_bytes,
                    "llvm.call @w_seed_process_context_drop(") == 1u &&
        count_bytes(artifact, result.written.mlir_bytes,
                    "llvm.call @w_seed_process_arguments_drop(") == 1u &&
        count_bytes(artifact, result.written.mlir_bytes,
                    "llvm.call @w_seed_process_root_finalize(") == 1u &&
        context_release < arguments_release &&
        arguments_release < root_finalize && root_finalize < stdout_write);
  CHECK(w_seed_mlir0_measure(&input, &WINDOWS_TARGET, &counts, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_emit(
            &input, &WINDOWS_TARGET,
            &(w_seed_mlir0_output){artifact, sizeof(artifact)}, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(result.written.mlir_bytes == counts.mlir_bytes);
  CHECK(contains_bytes(artifact, result.written.mlir_bytes,
                       "@w_seed_process_checked_add_u64(%left: i64, %right: i64, %width: i64, %fault: !llvm.ptr) -> i64"));
  CHECK(contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.store %process_fault_one, %fault : i64, !llvm.ptr"));
  CHECK(contains_bytes(artifact, result.written.mlir_bytes,
                       ", %fault_address) : (i64, i64, i64, !llvm.ptr) -> i64"));
  CHECK(!contains_bytes(artifact, result.written.mlir_bytes,
                        "@w_seed_checked_add_u64"));
  CHECK(!contains_bytes(artifact, result.written.mlir_bytes,
                        "llvm.intr.trap"));
  context_release = find_bytes(artifact, result.written.mlir_bytes,
                               "llvm.call @w_seed_process_context_drop(",
                               0u);
  arguments_release = find_bytes(
      artifact, result.written.mlir_bytes,
      "llvm.call @w_seed_process_arguments_drop(", 0u);
  root_finalize = find_bytes(
      artifact, result.written.mlir_bytes,
      "llvm.call @w_seed_process_root_finalize(", 0u);
  stdout_write = find_bytes(artifact, result.written.mlir_bytes,
                            "llvm.call @write(", 0u);
  CHECK(count_bytes(artifact, result.written.mlir_bytes,
                    "llvm.call @w_seed_process_context_drop(") == 1u &&
        count_bytes(artifact, result.written.mlir_bytes,
                    "llvm.call @w_seed_process_arguments_drop(") == 1u &&
        count_bytes(artifact, result.written.mlir_bytes,
                    "llvm.call @w_seed_process_root_finalize(") == 1u &&
        context_release < arguments_release &&
        arguments_release < root_finalize && root_finalize < stdout_write);

  static const uint8_t signed_source[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "fn checkedOffset(value: i8): i8 { return value + 127_i8 }\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode throws NumericConversionError { "
      "let count = try i8(exactly: args.count)\n"
      "print(\"Begin ${checkedOffset(value: count)}\")\n"
      "return .success }\n"
      "entry(run)\n";
  process_input_checked_integer_helper_mode = true;
  const bool signed_lowered =
      lower_process_input_hir(signed_source, sizeof(signed_source) - 1u);
  process_input_checked_integer_helper_mode = false;
  CHECK(signed_lowered);
  CHECK(w_seed_native_subset0_select_process_executable(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.checked_fault_relation.present &&
        selection.checked_fault_relation.operation_count == 1u);
  const w_seed_mlir0_input signed_input = {
      &fixture.hir_program, &fixture.hir_result,
      W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE};
  CHECK(w_seed_mlir0_measure(&signed_input, &TARGET, &counts, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_emit(
            &signed_input, &TARGET,
            &(w_seed_mlir0_output){artifact, sizeof(artifact)}, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(contains_bytes(artifact, result.written.mlir_bytes,
                       "@w_seed_process_checked_add_i64(%left: i64, %right: i64, %width: i64, %fault: !llvm.ptr) -> i64"));
  CHECK(contains_bytes(artifact, result.written.mlir_bytes,
                       ", %fault_address) : (i64, i64, i64, !llvm.ptr) -> i64"));
  CHECK(!contains_bytes(artifact, result.written.mlir_bytes,
                        "@w_seed_checked_add_i64"));
  CHECK(!contains_bytes(artifact, result.written.mlir_bytes,
                        "llvm.intr.trap"));
  return true;
}

static bool test_process_float_rounding_native_subset(void) {
  static const uint8_t source[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode throws NumericConversionError { "
      "let rounded = try i8(rounding: 2.5_f64, mode: .nearestEven) "
      "print(\"Rounded ${rounded}\") "
      "return .success }\n"
      "entry(run)\n";
  process_input_float_rounding_mode = true;
  const bool lowered = lower_process_input_hir(source, sizeof(source) - 1u);
  process_input_float_rounding_mode = false;
  CHECK(lowered);

  w_seed_native_subset0_process selection;
  CHECK(w_seed_native_subset0_select_process_executable(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_float_to_integer_rounding &&
        !selection.has_integer_exactly);
  CHECK(selection.maximum_stdout_bytes == 29u);
  CHECK(selection.function != NULL && selection.function->block_count == 4u);
  CHECK(selection.entry != NULL &&
        selection.entry->cleanup_obligation ==
            W_SEED_HIR0_ENTRY_CLEANUP_RELEASE_HANDLER_OWNERS_REVERSE_ON_ALL_OUTCOMES &&
        selection.entry->cleanup_owner_parameter_count == 2u);

  const w_seed_hir0_program *program = &fixture.hir_program;
  const uint32_t split_index = selection.rounding_split_block_index;
  const w_seed_hir0_block *split = &program->blocks[split_index];
  const w_seed_hir0_terminator *conversion =
      &program->terminators[split->terminator_index];
  CHECK(selection.rounding_source_value ==
            &program->values[conversion->value_index] &&
        selection.rounding_conversion == conversion &&
        selection.rounding_normal_block_index ==
            conversion->target_block &&
        selection.rounding_non_finite_block_index ==
            conversion->else_block &&
        selection.rounding_out_of_range_block_index ==
            conversion->third_block &&
        selection.rounding_normal_return->kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        selection.rounding_non_finite_throw->kind ==
            W_SEED_HIR0_TERMINATOR_THROW &&
        selection.rounding_out_of_range_throw->kind ==
            W_SEED_HIR0_TERMINATOR_THROW &&
        selection.rounding_source_type_index < program->type_count &&
        program->types[selection.rounding_source_type_index].kind ==
            W_SEED_HIR0_TYPE_F64 &&
        selection.rounding_source_bit_width == 64u &&
        selection.rounding_destination_bit_width == 8u &&
        selection.rounding_destination_is_signed &&
        selection.rounding_mode == W_SEED_HIR0_ROUNDING_MODE_NEAREST_EVEN &&
        selection.rounding_error_type_index == selection.function->error_type);

  const w_seed_mlir0_input input = {
      &fixture.hir_program, &fixture.hir_result,
      W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE};
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result result;
  CHECK(w_seed_mlir0_measure(&input, &TARGET, &counts, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_emit(
            &input, &TARGET,
            &(w_seed_mlir0_output){artifact, sizeof(artifact)}, &result) ==
        W_SEED_MLIR0_OK);
  const w_seed_hir0_block *normal_block =
      &program->blocks[selection.rounding_normal_block_index];
  char expected_normal_edge[128];
  char expected_print_value[160];
  const int normal_edge_length = snprintf(
      expected_normal_edge, sizeof(expected_normal_edge),
      "llvm.br ^w_fn_%u_b_%u(%%process_round_result : i64)",
      selection.function_index, selection.rounding_normal_block_index);
  const int print_value_length = snprintf(
      expected_print_value, sizeof(expected_print_value),
      "%%integer_materialized_masked_1 = llvm.and %%arg%u, "
      "%%integer_materialized_mask_1",
      normal_block->first_block_argument);
  CHECK(normal_edge_length > 0 &&
        (size_t)normal_edge_length < sizeof(expected_normal_edge) &&
        print_value_length > 0 &&
        (size_t)print_value_length < sizeof(expected_print_value) &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       expected_normal_edge) &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       expected_print_value) &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.call @w_seed_append_i64(%buffer, "
                       "%cursor0_1, %integer_materialized_1)"));
  CHECK(result.written.mlir_bytes == counts.mlir_bytes &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "// " W_SEED_MLIR0_PROCESS_EXECUTABLE_SCHEMA_VERSION
                       "\n") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.target_triple = \"" W_SEED_MLIR0_TARGET_TRIPLE
                       "\"") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "\"llvm.intr.is.fpclass\"(%v") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "<{bit = 519 : i32}>") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "\"llvm.intr.roundeven\"") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "0xc060000000000000 : f64") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "0x4060000000000000 : f64") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "%process_round_narrow = llvm.fptosi") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "%process_round_result = llvm.sext") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.call @w_seed_append_i64") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "\\52\\6f\\75\\6e\\64\\65\\64\\20") &&
        !contains_bytes(artifact, result.written.mlir_bytes,
                        "\\52\\6f\\75\\6e\\64\\65\\64\\20\\32\\0a") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "%process_round_non_finite_carrier = llvm.mlir.constant(4294967297 : i64)") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "%process_round_out_of_range_carrier = llvm.mlir.constant(4294967297 : i64)") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.call @write(%process_fd") &&
        !contains_bytes(artifact, result.written.mlir_bytes, "@_fltused"));
  const size_t context_drop =
      find_bytes(artifact, result.written.mlir_bytes,
                 "llvm.call @w_seed_process_context_drop", 0u);
  const size_t arguments_drop =
      find_bytes(artifact, result.written.mlir_bytes,
                 "llvm.call @w_seed_process_arguments_drop", 0u);
  const size_t root_finalize =
      find_bytes(artifact, result.written.mlir_bytes,
                 "llvm.call @w_seed_process_root_finalize", 0u);
  const size_t outcome_map =
      find_bytes(artifact, result.written.mlir_bytes,
                 "^process_map_outcome", 0u);
  const size_t stdout_write =
      find_bytes(artifact, result.written.mlir_bytes,
                 "llvm.call @write(%process_fd", 0u);
  CHECK(context_drop != SIZE_MAX && arguments_drop != SIZE_MAX &&
        root_finalize != SIZE_MAX && outcome_map != SIZE_MAX &&
        stdout_write != SIZE_MAX &&
        context_drop < arguments_drop && arguments_drop < root_finalize &&
        root_finalize < outcome_map && outcome_map < stdout_write);

  CHECK(w_seed_mlir0_measure(&input, &WINDOWS_TARGET, &counts, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_emit(
            &input, &WINDOWS_TARGET,
            &(w_seed_mlir0_output){artifact, sizeof(artifact)}, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(result.written.mlir_bytes == counts.mlir_bytes &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.target_triple = \""
                       W_SEED_MLIR0_TARGET_TRIPLE_WINDOWS "\"") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.mlir.global @_fltused(0 : i32) : i32") &&
        contains_bytes(
            artifact, result.written.mlir_bytes,
            "llvm.func @ExitProcess(%code: i32) attributes "
            "{passthrough = [\"noreturn\"]}\n") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.call @ExitProcess(%process_code)") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.call @w_seed_write(%process_buffer, %process_length)"));

  /* Typed failures are mapped only after cleanup and skip the success-only
   * write; the formatter consumes the HIR result binding on the normal edge. */
  const size_t windows_context_drop =
      find_bytes(artifact, result.written.mlir_bytes,
                 "llvm.call @w_seed_process_context_drop", 0u);
  const size_t windows_arguments_drop =
      find_bytes(artifact, result.written.mlir_bytes,
                 "llvm.call @w_seed_process_arguments_drop", 0u);
  const size_t windows_root_finalize =
      find_bytes(artifact, result.written.mlir_bytes,
                 "llvm.call @w_seed_process_root_finalize", 0u);
  const size_t windows_write =
      find_bytes(artifact, result.written.mlir_bytes,
                 "llvm.call @w_seed_write(%process_buffer", 0u);
  const size_t windows_outcome_map =
      find_bytes(artifact, result.written.mlir_bytes,
                 "^process_map_outcome", 0u);
  CHECK(windows_context_drop != SIZE_MAX &&
        windows_arguments_drop != SIZE_MAX &&
        windows_root_finalize != SIZE_MAX && windows_outcome_map != SIZE_MAX &&
        windows_write != SIZE_MAX &&
        windows_context_drop < windows_arguments_drop &&
        windows_arguments_drop < windows_root_finalize &&
        windows_root_finalize < windows_outcome_map &&
        windows_outcome_map < windows_write);

  /* A verified HIR result is the only admission input.  Reusing the same
   * result after changing a successor role must therefore be rejected as
   * invalid, rather than allowing a forged four-block claim through. */
  w_seed_hir0_terminator *mutable_split =
      &fixture.hir_terminators[split->terminator_index];
  const w_seed_hir0_terminator saved_split = *mutable_split;
  mutable_split->third_block = mutable_split->else_block;
  CHECK(w_seed_native_subset0_select_process_executable(
            program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  *mutable_split = saved_split;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_terminator *rounding_split =
      &program->terminators[split->terminator_index];
  const w_seed_hir0_call *print = &program->calls[0];
  const uint32_t message_index =
      program->arguments[print->first_argument].value_index;
  const w_seed_hir0_value *message = &program->values[message_index];
  w_seed_hir0_interpolation_segment *rounded_segment =
      &fixture.hir_interpolation_segments[
          (size_t)message->first_interpolation_segment + 1u];
  const w_seed_hir0_interpolation_segment saved_rounded_segment =
      *rounded_segment;
  rounded_segment->value_index = rounding_split->value_index;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  CHECK(w_seed_native_subset0_select_process_executable(
            program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  *rounded_segment = saved_rounded_segment;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  w_seed_hir0_entry *mutable_entry = &fixture.hir_entries[0];
  const w_seed_hir0_entry saved_entry = *mutable_entry;
  mutable_entry->cleanup_obligation =
      W_SEED_HIR0_ENTRY_CLEANUP_RELEASE_HANDLER_OWNERS;
  CHECK(w_seed_native_subset0_select_process_executable(
            program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  *mutable_entry = saved_entry;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_process_float_rounding_join_mlir(void) {
  static const uint8_t source[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode throws NumericConversionError { "
      "let rounded = try i8(rounding: if args.count == 0 { 2.5_f64 } "
      "else { 3.5_f64 }, mode: .nearestEven) "
      "print(\"Rounded ${rounded}\") "
      "return .success }\n"
      "entry(run)\n";
  process_input_float_rounding_mode = true;
  const bool lowered = lower_process_input_hir(source, sizeof(source) - 1u);
  process_input_float_rounding_mode = false;
  CHECK(lowered && w_seed_hir0_verify(&fixture.hir_program,
                                      &fixture.hir_result));

  w_seed_native_subset0_process selection;
  CHECK(w_seed_native_subset0_select_process_executable(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  const w_seed_hir0_program *program = &fixture.hir_program;
  const uint32_t branch_index = selection.function->first_block;
  const uint32_t then_index = branch_index + 1u;
  const uint32_t else_index = branch_index + 2u;
  const uint32_t join_index = selection.rounding_split_block_index;
  const w_seed_hir0_block *join = &program->blocks[join_index];
  const w_seed_hir0_value *source_value = selection.rounding_source_value;
  const w_seed_hir0_terminator *branch =
      &program->terminators[program->blocks[branch_index].terminator_index];
  const w_seed_hir0_terminator *then_jump =
      &program->terminators[program->blocks[then_index].terminator_index];
  const w_seed_hir0_terminator *else_jump =
      &program->terminators[program->blocks[else_index].terminator_index];
  CHECK(selection.has_float_to_integer_rounding &&
        selection.function->block_count == 7u &&
        join_index == branch_index + 3u && join->block_argument_count == 1u &&
        join->first_block_argument < program->block_argument_count &&
        branch->kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        branch->target_block == then_index && branch->else_block == else_index &&
        then_jump->kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        else_jump->kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        then_jump->target_block == join_index &&
        else_jump->target_block == join_index &&
        then_jump->edge_argument_count == 1u &&
        else_jump->edge_argument_count == 1u &&
        source_value != NULL &&
        source_value->kind == W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        source_value->block_argument_index == join->first_block_argument &&
        source_value->type_index < program->type_count &&
        program->types[source_value->type_index].kind ==
            W_SEED_HIR0_TYPE_F64);
  const uint32_t then_value = edge_value_for(program, then_jump);
  const uint32_t else_value = edge_value_for(program, else_jump);
  CHECK(then_value < program->value_count && else_value < program->value_count &&
        program->values[then_value].kind == W_SEED_HIR0_VALUE_CONST_FLOAT &&
        program->values[else_value].kind == W_SEED_HIR0_VALUE_CONST_FLOAT &&
        program->values[then_value].type_index == source_value->type_index &&
        program->values[else_value].type_index == source_value->type_index);

  const w_seed_mlir0_input input = {
      &fixture.hir_program, &fixture.hir_result,
      W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE};
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_result result;
  CHECK(w_seed_mlir0_emit(
            &input, &TARGET,
            &(w_seed_mlir0_output){artifact, sizeof(artifact)}, &result) ==
        W_SEED_MLIR0_OK);

  char join_signature[128];
  char then_edge_text[160];
  char else_edge_text[160];
  char classification[192];
  char roundeven[160];
  const int join_signature_length = snprintf(
      join_signature, sizeof(join_signature),
      "  ^w_fn_%u_b_%u(%%arg%u: f64):", selection.function_index, join_index,
      join->first_block_argument);
  const int then_edge_length = snprintf(
      then_edge_text, sizeof(then_edge_text),
      "llvm.br ^w_fn_%u_b_%u(%%v%u : f64)", selection.function_index,
      join_index, then_value);
  const int else_edge_length = snprintf(
      else_edge_text, sizeof(else_edge_text),
      "llvm.br ^w_fn_%u_b_%u(%%v%u : f64)", selection.function_index,
      join_index, else_value);
  const int classification_length = snprintf(
      classification, sizeof(classification),
      "\"llvm.intr.is.fpclass\"(%%arg%u) <{bit = 519 : i32}> : (f64) -> i1",
      join->first_block_argument);
  const int roundeven_length = snprintf(
      roundeven, sizeof(roundeven),
      "\"llvm.intr.roundeven\"(%%arg%u) : (f64) -> f64",
      join->first_block_argument);
  CHECK(join_signature_length > 0 &&
        (size_t)join_signature_length < sizeof(join_signature) &&
        then_edge_length > 0 &&
        (size_t)then_edge_length < sizeof(then_edge_text) &&
        else_edge_length > 0 && (size_t)else_edge_length < sizeof(else_edge_text) &&
        classification_length > 0 &&
        (size_t)classification_length < sizeof(classification) &&
        roundeven_length > 0 && (size_t)roundeven_length < sizeof(roundeven) &&
        contains_bytes(artifact, result.written.mlir_bytes, join_signature) &&
        contains_bytes(artifact, result.written.mlir_bytes, then_edge_text) &&
        contains_bytes(artifact, result.written.mlir_bytes, else_edge_text) &&
        contains_bytes(artifact, result.written.mlir_bytes, classification) &&
        contains_bytes(artifact, result.written.mlir_bytes, roundeven) &&
        !contains_bytes(artifact, result.written.mlir_bytes, "fastmath") &&
        !contains_bytes(artifact, result.written.mlir_bytes, "snprintf") &&
        !contains_bytes(artifact, result.written.mlir_bytes, "llvm.call @printf"));

  /* This floating join is admitted only as part of the verified process
   * rounding artifact; ordinary executable selection remains closed. */
  const w_seed_mlir0_input ordinary_input = {
      &fixture.hir_program, &fixture.hir_result,
      W_SEED_MLIR0_ARTIFACT_EXECUTABLE};
  CHECK(w_seed_mlir0_emit(
            &ordinary_input, &TARGET,
            &(w_seed_mlir0_output){artifact, sizeof(artifact)}, &result) ==
        W_SEED_MLIR0_UNSUPPORTED);

  w_seed_hir0_block_argument *mutable_join_argument =
      &fixture.hir_block_arguments[join->first_block_argument];
  const w_seed_hir0_block_argument saved_join_argument =
      *mutable_join_argument;
  mutable_join_argument->type_index = W_SEED_HIR0_TYPE_I64;
  uint8_t rejected_artifact[32];
  (void)memset(rejected_artifact, 0xa5u, sizeof(rejected_artifact));
  w_seed_mlir0_result rejected_result;
  (void)memset(&rejected_result, 0x5au, sizeof(rejected_result));
  const w_seed_mlir0_result rejected_snapshot = rejected_result;
  CHECK(w_seed_mlir0_emit(
            &input, &TARGET,
            &(w_seed_mlir0_output){rejected_artifact,
                                   sizeof(rejected_artifact)},
            &rejected_result) == W_SEED_MLIR0_INVALID_HIR);
  for (size_t index = 0u; index < sizeof(rejected_artifact); index += 1u)
    CHECK(rejected_artifact[index] == 0xa5u);
  CHECK(memcmp(&rejected_result, &rejected_snapshot,
               sizeof(rejected_snapshot)) == 0);
  *mutable_join_argument = saved_join_argument;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool test_process_arguments_count_comparison_mlir(void) {
  static const uint8_t source[] =
      "import std.process\n"
      "\n"
      "async fn run(args: Arguments, ctx: Context): ExitCode {\n"
      "  if args.count == 2 {\n"
      "    print(\"Exactly two arguments\")\n"
      "    return .success\n"
      "  } else {\n"
      "    print(\"Argument count ${args.count}\")\n"
      "    return .success\n"
      "  }\n"
      "}\n"
      "entry(run)\n";
  CHECK(lower_process_input_hir(source, sizeof(source) - 1u));
  const w_seed_mlir0_input input = {
      &fixture.hir_program, &fixture.hir_result,
      W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE};
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result result;
  CHECK(w_seed_mlir0_measure(&input, &WINDOWS_TARGET, &counts, &result) ==
        W_SEED_MLIR0_OK);
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  CHECK(w_seed_mlir0_emit(
            &input, &WINDOWS_TARGET,
            &(w_seed_mlir0_output){output, sizeof(output)}, &result) ==
        W_SEED_MLIR0_OK);
  w_seed_native_subset0_process process_selection;
  CHECK(w_seed_native_subset0_select_process_executable(
            &fixture.hir_program, &fixture.hir_result,
            &process_selection) == W_SEED_NATIVE_SUBSET0_OK);
  CHECK(process_selection.maximum_stdout_bytes == 36u);
  CHECK(result.written.mlir_bytes == counts.mlir_bytes &&
        contains_bytes(output, result.written.mlir_bytes,
                       "@w_seed_mlir0_buffer() : !llvm.array<37 x i8>") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "!llvm.array<37 x i8>") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "llvm.getelementptr %process_buffer[%process_length]") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "llvm.func internal @w_seed_process_count_arguments(%command_line: !llvm.ptr) -> i64") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "llvm.call @w_seed_process_count_arguments(%process_command_line)") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "llvm.store %process_zero, %process_vector_items_address") &&
        !contains_bytes(output, result.written.mlir_bytes,
                        "@w_seed_process_items") &&
        !contains_bytes(output, result.written.mlir_bytes,
                        "%field_encoding_address") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "llvm.mlir.constant(0 : i64) : i64") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "llvm.call @w_seed_process_arguments_count") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "llvm.icmp \"ne\"") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "llvm.cond_br"));

  CHECK(w_seed_mlir0_measure(&input, &TARGET, &counts, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_emit(
            &input, &TARGET,
            &(w_seed_mlir0_output){output, sizeof(output)}, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(result.written.mlir_bytes == counts.mlir_bytes &&
        contains_bytes(output, result.written.mlir_bytes,
                       "llvm.target_triple = \"" W_SEED_MLIR0_TARGET_TRIPLE
                       "\"") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "llvm.func @main() -> i32") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "llvm.call @w_seed_process_argc()") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "llvm.func internal @w_seed_process_count_arguments(%argument_count: i64) -> i64") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "llvm.call @w_seed_process_count_arguments(%process_argc)") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "llvm.icmp \"ult\" %argument_count, %one : i64") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "llvm.icmp \"ugt\" %argument_count, %max_total : i64") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "llvm.sub %argument_count, %one : i64") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "llvm.store %process_zero, %process_vector_items_address") &&
        !contains_bytes(output, result.written.mlir_bytes,
                        "w_seed_process_argv") &&
        !contains_bytes(output, result.written.mlir_bytes,
                        "%argv_address") &&
        !contains_bytes(output, result.written.mlir_bytes,
                        "%item_data") &&
        !contains_bytes(output, result.written.mlir_bytes,
                        "@w_seed_process_items") &&
        !contains_bytes(output, result.written.mlir_bytes,
                        "no-builtin-strlen") &&
        !contains_bytes(output, result.written.mlir_bytes,
                        "%scan_data") &&
        !contains_bytes(output, result.written.mlir_bytes,
                        "%byte_address") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "llvm.store %process_one, %process_vector_encoding_address") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "llvm.call @write(%process_fd") &&
        !contains_bytes(output, result.written.mlir_bytes,
                        "mainCRTStartup") &&
        !contains_bytes(output, result.written.mlir_bytes,
                        "GetCommandLineW") &&
        !contains_bytes(output, result.written.mlir_bytes,
                        "ExitProcess"));
  return true;
}

static bool test_process_arguments_value_lane_mlir(void) {
  static const uint8_t source[] =
      "import {\n"
      "  Arguments as ProcessArguments,\n"
      "  Context as ProcessContext,\n"
      "  ExitCode as ProcessExitCode,\n"
      "} from std.process\n"
      "\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode {\n"
      "  if args.isEmpty {\n"
      "    print(\"missing\")\n"
      "    return .failure(2)\n"
      "  } else {\n"
      "    print(\"received\")\n"
      "    return .success\n"
      "  }\n"
      "}\n"
      "entry(run)\n";
  CHECK(lower_process_input_hir(source, sizeof(source) - 1u));
  const w_seed_mlir0_input input = {
      &fixture.hir_program, &fixture.hir_result,
      W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE};
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result result;
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];

  CHECK(w_seed_mlir0_measure(&input, &WINDOWS_TARGET, &counts, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_emit(
            &input, &WINDOWS_TARGET,
            &(w_seed_mlir0_output){output, sizeof(output)}, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(contains_bytes(output, result.written.mlir_bytes,
                       "@w_seed_process_items") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "llvm.call @w_seed_process_count_arguments(%process_command_line, %process_items)") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "%field_encoding_address"));

  CHECK(w_seed_mlir0_measure(&input, &TARGET, &counts, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_emit(
            &input, &TARGET,
            &(w_seed_mlir0_output){output, sizeof(output)}, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(contains_bytes(output, result.written.mlir_bytes,
                       "@w_seed_process_items") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "llvm.call @w_seed_process_count_arguments(%process_argc, %process_argv, %process_items)") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "no-builtin-strlen") &&
        contains_bytes(output, result.written.mlir_bytes,
                       "%scan_data"));
  return true;
}

static bool expect_process_count_unsigned_predicate(
    const uint8_t *source, size_t source_length, const char *predicate) {
  CHECK(lower_process_input_hir(source, source_length));
  const w_seed_mlir0_input input = {
      &fixture.hir_program, &fixture.hir_result,
      W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE};
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result result;
  CHECK(w_seed_mlir0_measure(&input, &WINDOWS_TARGET, &counts, &result) ==
        W_SEED_MLIR0_OK);
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  CHECK(w_seed_mlir0_emit(
            &input, &WINDOWS_TARGET,
            &(w_seed_mlir0_output){output, sizeof(output)}, &result) ==
        W_SEED_MLIR0_OK);
  uint32_t comparison_value = UINT32_MAX;
  for (uint32_t index = 0u; index < fixture.hir_program.value_count; ++index) {
    if (fixture.hir_program.values[index].kind ==
        W_SEED_HIR0_VALUE_USIZE_COUNT_COMPARISON) {
      comparison_value = index;
      break;
    }
  }
  CHECK(comparison_value != UINT32_MAX);
  const w_seed_hir0_value *comparison =
      &fixture.hir_program.values[comparison_value];
  char expected[160];
  const int expected_length = snprintf(expected, sizeof(expected),
                                       "%%v%u = %s %%v%u, %%v%u : i64",
                                       comparison_value, predicate,
                                       comparison->left_value,
                                       comparison->right_value);
  CHECK(expected_length > 0 && (size_t)expected_length < sizeof(expected));
  CHECK(contains_bytes(output, result.written.mlir_bytes, expected));
  return true;
}

static bool test_process_arguments_count_ordered_mlir(void) {
  static const uint8_t LESS_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${args.count < 2}\") "
      "return .success }\n"
      "entry(run)\n";
  static const uint8_t LESS_EQUAL_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${args.count <= 2}\") "
      "return .success }\n"
      "entry(run)\n";
  static const uint8_t GREATER_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${args.count > 2}\") "
      "return .success }\n"
      "entry(run)\n";
  static const uint8_t GREATER_EQUAL_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${args.count >= 2}\") "
      "return .success }\n"
      "entry(run)\n";
  static const uint8_t REVERSED_LESS_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${2 < args.count}\") "
      "return .success }\n"
      "entry(run)\n";
  static const uint8_t REVERSED_LESS_EQUAL_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${2 <= args.count}\") "
      "return .success }\n"
      "entry(run)\n";
  static const uint8_t REVERSED_GREATER_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${2 > args.count}\") "
      "return .success }\n"
      "entry(run)\n";
  static const uint8_t REVERSED_GREATER_EQUAL_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${2 >= args.count}\") "
      "return .success }\n"
      "entry(run)\n";
  CHECK(expect_process_count_unsigned_predicate(
      LESS_SOURCE, sizeof(LESS_SOURCE) - 1u, "llvm.icmp \"ult\""));
  CHECK(expect_process_count_unsigned_predicate(
      LESS_EQUAL_SOURCE, sizeof(LESS_EQUAL_SOURCE) - 1u,
      "llvm.icmp \"ule\""));
  CHECK(expect_process_count_unsigned_predicate(
      GREATER_SOURCE, sizeof(GREATER_SOURCE) - 1u, "llvm.icmp \"ugt\""));
  CHECK(expect_process_count_unsigned_predicate(
      GREATER_EQUAL_SOURCE, sizeof(GREATER_EQUAL_SOURCE) - 1u,
      "llvm.icmp \"uge\""));
  CHECK(expect_process_count_unsigned_predicate(
      REVERSED_LESS_SOURCE, sizeof(REVERSED_LESS_SOURCE) - 1u,
      "llvm.icmp \"ult\""));
  CHECK(expect_process_count_unsigned_predicate(
      REVERSED_LESS_EQUAL_SOURCE, sizeof(REVERSED_LESS_EQUAL_SOURCE) - 1u,
      "llvm.icmp \"ule\""));
  CHECK(expect_process_count_unsigned_predicate(
      REVERSED_GREATER_SOURCE, sizeof(REVERSED_GREATER_SOURCE) - 1u,
      "llvm.icmp \"ugt\""));
  CHECK(expect_process_count_unsigned_predicate(
      REVERSED_GREATER_EQUAL_SOURCE,
      sizeof(REVERSED_GREATER_EQUAL_SOURCE) - 1u,
      "llvm.icmp \"uge\""));
  return true;
}

static bool test_enum_switch_mlir(void) {
  static const uint8_t SOURCE[] =
      "enum Course { starter main dessert }\n"
      "fn price(course: Course): i64 { return switch course { "
      "case .dessert: twenty() case .starter: ten() "
      "case .main: thirty(value: 30) } }\n"
      "fn ten(): i64 { return 10 }\n"
      "fn thirty(value: i64): i64 { return value }\n"
      "fn twenty(): i64 { return 20 }\n"
       "entry {\n"
       "  let starter = price(course: .starter)\n"
       "  let main = price(course: .main)\n"
       "  let dessert = price(course: .dessert)\n"
       "  print(\"x\")\n"
       "}\n";
  CHECK(lower_hir(SOURCE, sizeof(SOURCE) - 1u));
  CHECK(fixture.hir_program.function_count == 5u &&
        fixture.hir_program.functions[0].parameter_count == 1u);
  const w_seed_hir0_function *price = &fixture.hir_program.functions[0];
  CHECK(price->first_parameter < fixture.hir_program.parameter_count);
  const uint32_t enum_type =
      fixture.hir_program.parameters[price->first_parameter].type_index;
  const size_t dispatch_block = price->first_block;
  CHECK(dispatch_block < fixture.hir_program.block_count);
  const w_seed_hir0_terminator *dispatch =
      &fixture.hir_program.terminators[
          fixture.hir_program.blocks[dispatch_block].terminator_index];
  CHECK(dispatch->kind == W_SEED_HIR0_TERMINATOR_SWITCH_ENUM &&
        dispatch->value_index < fixture.hir_program.value_count &&
        dispatch->switch_carrier_width == 2u &&
        dispatch->switch_edge_count == 3u);
  const w_seed_hir0_value *subject =
      &fixture.hir_program.values[dispatch->value_index];
  CHECK(subject->type_index == enum_type &&
        fixture.hir_program.types[enum_type].kind ==
            W_SEED_HIR0_TYPE_ENUM);

  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_result result;
  const w_seed_mlir0_input input = mlir_input();
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measure_result;
  CHECK(w_seed_mlir0_measure(&input, &TARGET, &counts, &measure_result) ==
        W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_emit(
            &input, &TARGET,
            &(w_seed_mlir0_output){artifact, sizeof(artifact)}, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(result.required.mlir_bytes == counts.mlir_bytes &&
        result.written.mlir_bytes == counts.mlir_bytes);
  CHECK(contains_bytes(
            artifact, result.written.mlir_bytes,
            "llvm.func internal @w_fn_0(%buffer: !llvm.ptr, "
            "%cursor_address: !llvm.ptr, %p0: i2) -> i64") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "cf.switch %p0 : i2, [") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "      default: ^w_fn_0_switch_default,") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "      0: ^w_fn_0_b_1,") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "      1: ^w_fn_0_b_2,") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "      -2: ^w_fn_0_b_3\n") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "^w_fn_0_switch_default:\n    llvm.unreachable\n") &&
        count_bytes(artifact, result.written.mlir_bytes,
                    "^w_fn_0_switch_default:") == 1u &&
        count_bytes(artifact, result.written.mlir_bytes,
                    "llvm.unreachable\n") == 1u &&
        !contains_bytes(artifact, result.written.mlir_bytes, "llvm.switch") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.mlir.constant(0 : i2) : i2") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.mlir.constant(1 : i2) : i2") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.mlir.constant(2 : i2) : i2") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.call @w_fn_1") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.call @w_fn_2") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.call @w_fn_3"));

  uint8_t forged_output[W_SEED_MLIR0_MAX_BYTES];
  (void)memset(forged_output, 0xa5u, sizeof(forged_output));
  w_seed_mlir0_result forged_result;
  (void)memset(&forged_result, 0x5au, sizeof(forged_result));
  const w_seed_mlir0_result forged_snapshot = forged_result;
  const uint32_t dispatch_terminator =
      fixture.hir_program.blocks[dispatch_block].terminator_index;
  const uint32_t saved_carrier =
      fixture.hir_terminators[dispatch_terminator].switch_carrier_width;
  fixture.hir_terminators[dispatch_terminator].switch_carrier_width = 1u;
  CHECK(w_seed_mlir0_emit(
            &input, &TARGET,
            &(w_seed_mlir0_output){forged_output, sizeof(forged_output)},
            &forged_result) == W_SEED_MLIR0_INVALID_HIR);
  for (size_t index = 0u; index < sizeof(forged_output); index += 1u)
    CHECK(forged_output[index] == 0xa5u);
  CHECK(memcmp(&forged_result, &forged_snapshot, sizeof(forged_result)) == 0);
  fixture.hir_terminators[dispatch_terminator].switch_carrier_width =
      saved_carrier;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool test_enum_if_value_join_mlir(void) {
  static const uint8_t SOURCE[] =
      "enum Lookup { missing found(value: i64) }\n"
      "fn choose(available: Bool, amount: i64): Lookup { "
      "return if available { .found(value: amount) } else { .missing } }\n"
      "fn score(result: Lookup): i64 { return switch result { "
      "case .found(value: let value): value + 1 case .missing: 0 } }\n"
      "entry { let present_value = choose(available: true, amount: 41) "
      "let present = score(result: present_value) "
      "let absent_value = choose(available: false, amount: 41) "
      "let absent = score(result: absent_value) "
      "print(\"${present}/${absent}\") }\n";
  CHECK(lower_hir(SOURCE, sizeof(SOURCE) - 1u));
  const w_seed_hir0_program *program = &fixture.hir_program;
  uint32_t lookup_type = W_SEED_HIR0_NONE;
  for (size_t type = 0u; type < program->type_count; type += 1u)
    if (program->types[type].kind == W_SEED_HIR0_TYPE_ENUM)
      lookup_type = (uint32_t)type;
  CHECK(lookup_type != W_SEED_HIR0_NONE && program->function_count == 3u &&
        program->functions[0].return_type == lookup_type &&
        program->functions[1].parameter_count == 1u &&
        program->parameters[program->functions[1].first_parameter].type_index ==
            lookup_type);

  uint32_t join_branch = W_SEED_HIR0_NONE;
  for (size_t block = program->functions[0].first_block;
       block < (size_t)program->functions[0].first_block +
                   program->functions[0].block_count;
       block += 1u)
    if (program->terminators[block].kind ==
            W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[block].result_type == lookup_type)
      join_branch = (uint32_t)block;
  CHECK(join_branch != W_SEED_HIR0_NONE &&
        program->terminators[join_branch].target_block == join_branch + 1u &&
        program->terminators[join_branch].else_block == join_branch + 2u);
  const uint32_t join_block = join_branch + 3u;
  CHECK(program->blocks[join_block].block_argument_count == 1u &&
        program->block_arguments[
            program->blocks[join_block].first_block_argument].type_index ==
            lookup_type &&
        program->edge_arguments[
            program->terminators[join_branch + 1u].first_edge_argument]
                .type_index == lookup_type &&
        program->edge_arguments[
            program->terminators[join_branch + 2u].first_edge_argument]
                .type_index == lookup_type);

  bool saw_switch = false;
  for (size_t block = program->functions[1].first_block;
       block < (size_t)program->functions[1].first_block +
                   program->functions[1].block_count;
       block += 1u)
    if (program->terminators[block].kind ==
        W_SEED_HIR0_TERMINATOR_SWITCH_ENUM)
      saw_switch = true;
  CHECK(saw_switch);

  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK && selection.has_cfg &&
        selection.has_local_calls);
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0);
  const size_t choose_start = find_bytes(
      artifact, emitted.written.mlir_bytes,
      "llvm.func internal @w_fn_0(", 0u);
  const size_t score_start = find_bytes(
      artifact, emitted.written.mlir_bytes,
      "llvm.func internal @w_fn_1(", choose_start);
  CHECK(contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.func internal @w_fn_0(") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "%p0: i1, %p1: i64) -> !llvm.struct<(i1, array<1 x i64>)>") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.cond_br") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.br ^w_fn_0_b_3(") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       ": !llvm.struct<(i1, array<1 x i64>)>)") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.insertvalue") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "cf.switch %switch_tag_") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.extractvalue") &&
        choose_start != SIZE_MAX && score_start != SIZE_MAX &&
        choose_start < score_start &&
        find_bytes(artifact, score_start, "llvm.select", choose_start) ==
            SIZE_MAX);
  return true;
}

static bool test_enum_subset_switch_mlir(void) {
  static const uint8_t SOURCE[] =
      "enum Stage {\n"
      "  accepted\n"
      "  reserving\n"
      "  preparing\n"
      "  serving\n"
      "  completed\n"
      "}\n"
      "\n"
      "alias WorkStage = Stage<[.serving, .preparing]>\n"
      "\n"
      "fn label(stage: WorkStage): i64 {\n"
      "  return switch stage {\n"
      "    case .serving: 2\n"
      "    case .preparing: 1\n"
      "  }\n"
      "}\n"
      "\n"
      "entry {\n"
      "  let preparing = label(stage: .preparing)\n"
      "  let serving = label(stage: .serving)\n"
      "  print(\"Work ${preparing}/${serving}\")\n"
      "}\n";
  CHECK(lower_hir(SOURCE, sizeof(SOURCE) - 1u));
  CHECK(fixture.hir_program.function_count == 2u &&
        fixture.hir_program.functions[0].parameter_count == 1u);
  const w_seed_hir0_function *label = &fixture.hir_program.functions[0];
  CHECK(label->first_parameter < fixture.hir_program.parameter_count);
  const uint32_t subset_type =
      fixture.hir_program.parameters[label->first_parameter].type_index;
  CHECK(subset_type < fixture.hir_program.type_count &&
        fixture.hir_program.types[subset_type].kind ==
            W_SEED_HIR0_TYPE_ENUM_SUBSET);
  const w_seed_hir0_type *subset = &fixture.hir_program.types[subset_type];
  CHECK(subset->enum_index < fixture.hir_program.enum_count &&
        subset->subset_member_count == 2u &&
        subset->first_subset_member <
            fixture.hir_program.enum_subset_member_count);
  const w_seed_hir0_enum *base =
      &fixture.hir_program.enums[subset->enum_index];
  CHECK(base->case_count == 5u &&
        fixture.hir_program.enum_subset_members[subset->first_subset_member]
                .enum_case_index == base->first_case + 2u &&
        fixture.hir_program.enum_subset_members[subset->first_subset_member + 1u]
                .enum_case_index == base->first_case + 3u);
  const size_t dispatch_block = label->first_block;
  CHECK(dispatch_block < fixture.hir_program.block_count);
  const w_seed_hir0_terminator *dispatch =
      &fixture.hir_program.terminators[
          fixture.hir_program.blocks[dispatch_block].terminator_index];
  CHECK(dispatch->kind == W_SEED_HIR0_TERMINATOR_SWITCH_ENUM &&
        dispatch->value_index < fixture.hir_program.value_count &&
        dispatch->switch_carrier_width == 3u &&
        dispatch->switch_edge_count == 2u &&
        fixture.hir_program.values[dispatch->value_index].type_index ==
            subset_type);
  for (size_t ordinal = 0u; ordinal < dispatch->switch_edge_count;
       ordinal += 1u) {
    const w_seed_hir0_switch_edge *edge =
        &fixture.hir_program.switch_edges[
            (size_t)dispatch->first_switch_edge + ordinal];
    CHECK(edge->ordinal == ordinal && edge->enum_index == subset->enum_index &&
          edge->enum_case_index == base->first_case + 2u + ordinal &&
          edge->target_block == dispatch_block + 1u + ordinal);
  }

  const w_seed_mlir0_input input = mlir_input();
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result result;
  CHECK(w_seed_mlir0_measure(&input, &TARGET, &counts, &measured) ==
        W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_emit(
            &input, &TARGET,
            &(w_seed_mlir0_output){artifact, sizeof(artifact)}, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(result.required.mlir_bytes == counts.mlir_bytes &&
        result.written.mlir_bytes == counts.mlir_bytes &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.func internal @w_fn_0(%buffer: !llvm.ptr, "
                       "%cursor_address: !llvm.ptr, %p0: i3) -> i64") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "cf.switch %p0 : i3, [") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "      default: ^w_fn_0_switch_default,") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "      2: ^w_fn_0_b_1,") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "      3: ^w_fn_0_b_2\n") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "^w_fn_0_switch_default:\n    llvm.unreachable\n") &&
        count_bytes(artifact, result.written.mlir_bytes,
                    "^w_fn_0_switch_default:") == 1u &&
        count_bytes(artifact, result.written.mlir_bytes,
                    "llvm.unreachable\n") == 1u &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.mlir.constant(2 : i3) : i3") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "llvm.mlir.constant(3 : i3) : i3") &&
        !contains_bytes(artifact, result.written.mlir_bytes,
                        "llvm.mlir.constant(0 : i3) : i3") &&
        !contains_bytes(artifact, result.written.mlir_bytes,
                        "llvm.mlir.constant(1 : i3) : i3") &&
        !contains_bytes(artifact, result.written.mlir_bytes,
                        "llvm.mlir.constant(4 : i3) : i3") &&
        !contains_bytes(artifact, result.written.mlir_bytes, "llvm.switch"));

  uint8_t forged_output[W_SEED_MLIR0_MAX_BYTES];
  (void)memset(forged_output, 0xa5u, sizeof(forged_output));
  w_seed_mlir0_result forged_result;
  (void)memset(&forged_result, 0x5au, sizeof(forged_result));
  const w_seed_mlir0_result forged_snapshot = forged_result;
  const uint32_t member_index = subset->first_subset_member;
  const uint32_t saved_case =
      fixture.hir_enum_subset_members[member_index].enum_case_index;
  fixture.hir_enum_subset_members[member_index].enum_case_index =
      base->first_case;
  CHECK(w_seed_mlir0_emit(
            &input, &TARGET,
            &(w_seed_mlir0_output){forged_output, sizeof(forged_output)},
            &forged_result) == W_SEED_MLIR0_INVALID_HIR);
  for (size_t index = 0u; index < sizeof(forged_output); index += 1u)
    CHECK(forged_output[index] == 0xa5u);
  CHECK(memcmp(&forged_result, &forged_snapshot, sizeof(forged_result)) == 0);
  fixture.hir_enum_subset_members[member_index].enum_case_index = saved_case;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));

  uint8_t windows_artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_result windows_result;
  CHECK(w_seed_mlir0_emit(
            &input, &WINDOWS_TARGET,
            &(w_seed_mlir0_output){windows_artifact, sizeof(windows_artifact)},
            &windows_result) == W_SEED_MLIR0_OK);
  CHECK(contains_bytes(windows_artifact, windows_result.written.mlir_bytes,
                       "llvm.func internal @w_fn_0(%buffer: !llvm.ptr, "
                       "%cursor_address: !llvm.ptr, %p0: i3) -> i64") &&
        contains_bytes(windows_artifact, windows_result.written.mlir_bytes,
                       "cf.switch %p0 : i3, [") &&
        contains_bytes(windows_artifact, windows_result.written.mlir_bytes,
                       "      2: ^w_fn_0_b_1,") &&
        contains_bytes(windows_artifact, windows_result.written.mlir_bytes,
                       "      3: ^w_fn_0_b_2\n"));
  return true;
}

static bool contains_bytes(const uint8_t *bytes, size_t length,
                           const char *needle) {
  if (bytes == NULL || needle == NULL) return false;
  const size_t needle_length = strlen(needle);
  if (needle_length == 0u || needle_length > length) return false;
  for (size_t offset = 0u; offset + needle_length <= length; offset += 1u)
    if (memcmp(bytes + offset, needle, needle_length) == 0) return true;
  return false;
}

static bool append_mlir_test_source(char *source, size_t capacity,
                                    size_t *length, const char *format, ...) {
  if (source == NULL || length == NULL || format == NULL || *length > capacity)
    return false;
  va_list arguments;
  va_start(arguments, format);
  const int written = vsnprintf(source + *length, capacity - *length, format,
                                arguments);
  va_end(arguments);
  if (written < 0 || (size_t)written >= capacity - *length) return false;
  *length += (size_t)written;
  return true;
}

static size_t count_bytes(const uint8_t *bytes, size_t length,
                          const char *needle) {
  if (bytes == NULL || needle == NULL) return 0u;
  const size_t needle_length = strlen(needle);
  if (needle_length == 0u || needle_length > length) return 0u;
  size_t count = 0u;
  for (size_t offset = 0u; offset + needle_length <= length; offset += 1u)
    if (memcmp(bytes + offset, needle, needle_length) == 0) count += 1u;
  return count;
}

static size_t find_bytes(const uint8_t *bytes, size_t length,
                         const char *needle, size_t start) {
  if (bytes == NULL || needle == NULL || start > length) return SIZE_MAX;
  const size_t needle_length = strlen(needle);
  if (needle_length == 0u || needle_length > length - start) return SIZE_MAX;
  for (size_t offset = start; offset + needle_length <= length; offset += 1u)
    if (memcmp(bytes + offset, needle, needle_length) == 0) return offset;
  return SIZE_MAX;
}

static size_t count_mlir_lines_with_fragment_and_type(
    const uint8_t *bytes, size_t length, const char *fragment,
    const char *type_suffix) {
  if (bytes == NULL || fragment == NULL || type_suffix == NULL) return 0u;
  size_t count = 0u;
  size_t cursor = 0u;
  while (cursor < length) {
    size_t line_end = find_bytes(bytes, length, "\n", cursor);
    if (line_end == SIZE_MAX) line_end = length;
    const size_t line_length = line_end - cursor;
    if (contains_bytes(bytes + cursor, line_length, fragment) &&
        contains_bytes(bytes + cursor, line_length, type_suffix))
      count += 1u;
    cursor = line_end == length ? length : line_end + 1u;
  }
  return count;
}

static bool append_text(char *buffer, size_t capacity, size_t *offset,
                        const char *text) {
  if (buffer == NULL || offset == NULL || text == NULL || *offset > capacity)
    return false;
  const size_t length = strlen(text);
  if (length > capacity - *offset) return false;
  (void)memcpy(buffer + *offset, text, length);
  *offset += length;
  return true;
}

static bool build_call_source(char *buffer, size_t capacity, size_t calls) {
  size_t offset = 0u;
  if (!append_text(buffer, capacity, &offset, "fn main() { ")) return false;
  for (size_t call = 0u; call < calls; call += 1u)
    if (!append_text(buffer, capacity, &offset, "print(\"x\")\n"))
      return false;
  if (!append_text(buffer, capacity, &offset, "}\nentry(main)\n")) return false;
  if (offset >= capacity) return false;
  buffer[offset] = '\0';
  return true;
}

static bool build_binding_call_source(char *buffer, size_t capacity,
                                      size_t payload_bytes, size_t calls) {
  size_t offset = 0u;
  if (!append_text(buffer, capacity, &offset,
                   "fn main() { let message = \""))
    return false;
  if (payload_bytes > capacity - offset) return false;
  (void)memset(buffer + offset, 'x', payload_bytes);
  offset += payload_bytes;
  if (!append_text(buffer, capacity, &offset, "\"\n")) return false;
  for (size_t call = 0u; call < calls; call += 1u)
    if (!append_text(buffer, capacity, &offset, "print(message)\n"))
      return false;
  if (!append_text(buffer, capacity, &offset, "}\nentry(main)\n")) return false;
  if (offset >= capacity) return false;
  buffer[offset] = '\0';
  return true;
}

static bool build_binding_interpolation_source(char *buffer, size_t capacity,
                                               size_t payload_bytes,
                                               size_t calls) {
  size_t offset = 0u;
  if (!append_text(buffer, capacity, &offset,
                   "fn main() { let message = \""))
    return false;
  if (payload_bytes > capacity - offset) return false;
  (void)memset(buffer + offset, 'x', payload_bytes);
  offset += payload_bytes;
  if (!append_text(buffer, capacity, &offset, "\"\n")) return false;
  for (size_t call = 0u; call < calls; call += 1u)
    if (!append_text(buffer, capacity, &offset,
                     "print(\"${message}\")\n"))
      return false;
  if (!append_text(buffer, capacity, &offset, "}\nentry(main)\n")) return false;
  if (offset >= capacity) return false;
  buffer[offset] = '\0';
  return true;
}

static bool test_direct_products(void) {
  static const uint8_t hello[] =
      "fn main() { print(\"Hello, world!\") }\nentry(main)\n";
  CHECK(lower_hir(hello, sizeof(hello) - 1u));
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  CHECK(measure_current(&counts, &measured));
  CHECK(counts.mlir_bytes > 0u && counts.mlir_bytes < W_SEED_MLIR0_MAX_BYTES);
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  (void)memset(output, 0xa5u, sizeof(output));
  w_seed_mlir0_result result;
  CHECK(emit_current(output, sizeof(output), &result));
  CHECK(result.required.mlir_bytes == counts.mlir_bytes &&
        result.written.mlir_bytes == counts.mlir_bytes &&
        memcmp(result.mlir_sha256, measured.mlir_sha256,
               sizeof(result.mlir_sha256)) == 0);
  CHECK(contains_bytes(output, counts.mlir_bytes,
                       "// " W_SEED_MLIR0_SCHEMA_VERSION "\nmodule "));
  CHECK(contains_bytes(output, counts.mlir_bytes, "\\48\\65\\6c\\6c"));
  CHECK(!contains_bytes(output, counts.mlir_bytes, "@w_seed_checked_"));
  CHECK(contains_bytes(output, counts.mlir_bytes, "!llvm.array<14 x i8>"));
  CHECK(output[counts.mlir_bytes] == 0xa5u);
  for (size_t index = 0u; index < counts.mlir_bytes; index += 1u)
    CHECK(output[index] != 0u);

  static const uint8_t empty[] =
      "fn main() { print(\"\") }\nentry(main)\n";
  CHECK(lower_hir(empty, sizeof(empty) - 1u));
  CHECK(measure_current(&counts, &measured));
  (void)memset(output, 0x5au, sizeof(output));
  CHECK(emit_current(output, sizeof(output), &result));
  CHECK(contains_bytes(output, counts.mlir_bytes, "\\0a"));
  CHECK(!contains_bytes(output, counts.mlir_bytes, "@w_seed_checked_"));
  CHECK(contains_bytes(output, counts.mlir_bytes, "!llvm.array<1 x i8>"));
  CHECK(output[counts.mlir_bytes] == 0x5au);
  return true;
}

static bool test_strict_float_mlir(void) {
  static const uint8_t SOURCE[] =
      "fn main() {\n"
      "  if 1.5_f32 == 1.5_f32 && 1.5_f32 != 2.0_f32 && "
      "1.5_f32 < 2.0_f32 && 1.5_f32 <= 1.5_f32 && "
      "2.0_f32 > 1.5_f32 && 2.0_f32 >= 2.0_f32 && "
      "(-1.5_f32) < 0.0_f32 && (1.5_f32 + 2.25_f32) > 0.0_f32 && "
      "(9.5_f32 - 5.5_f32) > 0.0_f32 && "
      "(1.5_f32 * 2.0_f32) > 0.0_f32 && "
      "(7.5_f32 / 2.5_f32) > 0.0_f32 && "
      "(0.0_f32 / 0.0_f32) != (0.0_f32 / 0.0_f32) && "
      "1.5_f64 == 1.5_f64 && 1.5_f64 != 2.0_f64 && "
      "1.5_f64 < 2.0_f64 && 1.5_f64 <= 1.5_f64 && "
      "2.0_f64 > 1.5_f64 && 2.0_f64 >= 2.0_f64 && "
      "(-1.5_f64) < 0.0_f64 && (1.5_f64 + 2.25_f64) > 0.0_f64 && "
      "(9.5_f64 - 5.5_f64) > 0.0_f64 && "
      "(1.5_f64 * 2.0_f64) > 0.0_f64 && "
      "(7.5_f64 / 2.5_f64) > 0.0_f64 && "
      "(0.0_f64 / 0.0_f64) != (0.0_f64 / 0.0_f64) { "
      "print(\"strict floats\") } else { print(\"strict floats\") }\n"
      "}\nentry(main)\n";
  CHECK(lower_hir(SOURCE, sizeof(SOURCE) - 1u));
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  CHECK(measure_current(&counts, &measured));
  static uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_result result;
  CHECK(emit_current(artifact, sizeof(artifact), &result));
  CHECK(result.written.mlir_bytes == counts.mlir_bytes &&
        memcmp(result.mlir_sha256, measured.mlir_sha256,
               sizeof(result.mlir_sha256)) == 0);
  CHECK(contains_bytes(artifact, counts.mlir_bytes,
                       "llvm.mlir.constant(0x3fc00000 : f32) : f32"));
  CHECK(contains_bytes(artifact, counts.mlir_bytes,
                       "llvm.mlir.constant(0x3ff8000000000000 : f64) : f64"));
  CHECK(count_mlir_lines_with_fragment_and_type(
            artifact, counts.mlir_bytes, " = llvm.fadd ", ": f32") == 1u);
  CHECK(count_mlir_lines_with_fragment_and_type(
            artifact, counts.mlir_bytes, " = llvm.fadd ", ": f64") == 1u);
  CHECK(count_mlir_lines_with_fragment_and_type(
            artifact, counts.mlir_bytes, " = llvm.fsub ", ": f32") == 1u);
  CHECK(count_mlir_lines_with_fragment_and_type(
            artifact, counts.mlir_bytes, " = llvm.fsub ", ": f64") == 1u);
  CHECK(count_mlir_lines_with_fragment_and_type(
            artifact, counts.mlir_bytes, " = llvm.fmul ", ": f32") == 1u);
  CHECK(count_mlir_lines_with_fragment_and_type(
            artifact, counts.mlir_bytes, " = llvm.fmul ", ": f64") == 1u);
  CHECK(count_mlir_lines_with_fragment_and_type(
            artifact, counts.mlir_bytes, " = llvm.fdiv ", ": f32") == 3u);
  CHECK(count_mlir_lines_with_fragment_and_type(
            artifact, counts.mlir_bytes, " = llvm.fdiv ", ": f64") == 3u);
  CHECK(count_mlir_lines_with_fragment_and_type(
            artifact, counts.mlir_bytes, " = llvm.fneg ", ": f32") == 1u);
  CHECK(count_mlir_lines_with_fragment_and_type(
            artifact, counts.mlir_bytes, " = llvm.fneg ", ": f64") == 1u);
  CHECK(count_mlir_lines_with_fragment_and_type(
            artifact, counts.mlir_bytes, " = llvm.fcmp ", ": f32") == 12u);
  CHECK(count_mlir_lines_with_fragment_and_type(
            artifact, counts.mlir_bytes, " = llvm.fcmp ", ": f64") == 12u);
  CHECK(count_bytes(artifact, counts.mlir_bytes, "llvm.fcmp \"oeq\"") ==
        2u);
  CHECK(count_bytes(artifact, counts.mlir_bytes, "llvm.fcmp \"une\"") ==
        4u);
  CHECK(count_bytes(artifact, counts.mlir_bytes, "llvm.fcmp \"olt\"") ==
        4u);
  CHECK(count_bytes(artifact, counts.mlir_bytes, "llvm.fcmp \"ole\"") ==
        2u);
  CHECK(count_bytes(artifact, counts.mlir_bytes, "llvm.fcmp \"ogt\"") ==
        10u);
  CHECK(count_bytes(artifact, counts.mlir_bytes, "llvm.fcmp \"oge\"") ==
        2u);
  CHECK(contains_bytes(artifact, counts.mlir_bytes, "llvm.cond_br "));
  CHECK(!contains_bytes(artifact, counts.mlir_bytes, "fastmath"));

  size_t comparison_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u) {
    const w_seed_hir0_value *value = &fixture.hir_values[index];
    if (value->kind != W_SEED_HIR0_VALUE_BINARY_FLOAT ||
        value->binary_operator < W_SEED_HIR0_BINARY_EQUAL ||
        value->binary_operator > W_SEED_HIR0_BINARY_GREATER_EQUAL)
      continue;
    CHECK(value->type_index < fixture.hir_program.type_count &&
          fixture.hir_program.types[value->type_index].kind ==
              W_SEED_HIR0_TYPE_BOOL);
    comparison_count += 1u;
  }
  CHECK(comparison_count == 24u);

  static const uint8_t ADVERSARIAL_SOURCE[] =
      "fn main() { if (1.0_f32 + 2.0_f32) > 0.0_f32 && "
      "(1.0_f64 + 2.0_f64) > 0.0_f64 { print(\"x\") } "
      "else { print(\"x\") } }\nentry(main)\n";
  CHECK(lower_hir(ADVERSARIAL_SOURCE, sizeof(ADVERSARIAL_SOURCE) - 1u));
  uint32_t f32_add = W_SEED_HIR0_NONE;
  uint32_t f64_constant = W_SEED_HIR0_NONE;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u) {
    const w_seed_hir0_value *value = &fixture.hir_values[index];
    if (value->kind == W_SEED_HIR0_VALUE_BINARY_FLOAT &&
        value->binary_operator == W_SEED_HIR0_BINARY_ADD &&
        fixture.hir_program.types[value->type_index].kind ==
            W_SEED_HIR0_TYPE_F32)
      f32_add = (uint32_t)index;
    if (value->kind == W_SEED_HIR0_VALUE_CONST_FLOAT &&
        fixture.hir_program.types[value->type_index].kind ==
            W_SEED_HIR0_TYPE_F64)
      f64_constant = (uint32_t)index;
  }
  CHECK(f32_add != W_SEED_HIR0_NONE &&
        f64_constant != W_SEED_HIR0_NONE);
  const w_seed_hir0_value original = fixture.hir_values[f32_add];
  fixture.hir_values[f32_add].right_value = f64_constant;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  w_seed_mlir0_input input = mlir_input();
  CHECK(w_seed_mlir0_measure(&input, &TARGET, &counts, &measured) ==
        W_SEED_MLIR0_INVALID_HIR);
  CHECK(w_seed_native_subset0_select_program(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  fixture.hir_values[f32_add] = original;

  fixture.hir_values[f32_add].binary_operator =
      W_SEED_HIR0_BINARY_POWER;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  CHECK(w_seed_mlir0_measure(&input, &TARGET, &counts, &measured) ==
        W_SEED_MLIR0_INVALID_HIR);
  CHECK(w_seed_native_subset0_select_program(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  return true;
}

static bool numeric_widen_expected_operand(
    const w_seed_hir0_program *program, uint32_t value_index, char *buffer,
    size_t capacity, size_t depth) {
  if (program == NULL || buffer == NULL || capacity == 0u ||
      value_index >= program->value_count || depth > 256u)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->kind == W_SEED_HIR0_VALUE_BINDING_READ) {
    return value->binding_index < program->binding_count &&
           numeric_widen_expected_operand(
               program,
               program->bindings[value->binding_index].initializer_value,
               buffer, capacity, depth + 1u);
  }
  int written = -1;
  if (value->kind == W_SEED_HIR0_VALUE_PARAMETER_READ) {
    if (value->parameter_index >= program->parameter_count) return false;
    written = snprintf(
        buffer, capacity, "%%p%u",
        (unsigned int)program->parameters[value->parameter_index].ordinal);
  } else if (value->kind == W_SEED_HIR0_VALUE_CALL_RESULT) {
    if (value->call_index >= program->call_count) return false;
    written = snprintf(buffer, capacity, "%%call%u",
                       (unsigned int)value->call_index);
  } else if (value->kind == W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ) {
    if (value->block_argument_index >= program->block_argument_count)
      return false;
    written = snprintf(buffer, capacity, "%%arg%u",
                       (unsigned int)value->block_argument_index);
  } else {
    written = snprintf(buffer, capacity, "%%v%u",
                       (unsigned int)value_index);
  }
  return written >= 0 && (size_t)written < capacity;
}

static bool test_numeric_widen_mlir(void) {
  static const uint8_t SEQUENCE_SOURCE[] =
      "entry { let value: f64 = 1.5_f32 print(\"Sequence widen ok\") }\n";
  static const uint8_t COMPOSITION_SOURCE[] =
      "fn explicitAndImplicit(a: f32, b: f32): f64 { "
      "return f64(a) + b }\n"
      "fn explicitBoth(a: f32, c: f32): f64 { "
      "return f64(a) + f64(c) }\n"
      "fn main() {\n"
      "  let oneCast = explicitAndImplicit(a: 1.5_f32, b: 2.25_f32)\n"
      "  let bothCast = explicitBoth(a: 1.5_f32, c: 2.25_f32)\n"
      "  if oneCast == 3.75_f64 && bothCast == 3.75_f64 { "
      "print(\"Numeric composition ok\") } else { "
      "print(\"Numeric composition bad\") }\n"
      "}\nentry(main)\n";
  static const uint8_t SOURCE[] =
      "fn widenParameter(value: f32): f64 { return value }\n"
      "fn produceF32(): f32 { return 1.5_f32 }\n"
      "fn main() {\n"
      "  let i8F32Min: f32 = -127_i8 - 1_i8\n"
      "  let i8F32Max: f32 = 127_i8\n"
      "  let u8F32Max: f32 = 255_u8\n"
      "  let i16F32Min: f32 = -32767_i16 - 1_i16\n"
      "  let i16F32Max: f32 = 32767_i16\n"
      "  let u16F32Max: f32 = 65535_u16\n"
      "  let i8F64Min: f64 = -127_i8 - 1_i8\n"
      "  let i8F64Max: f64 = 127_i8\n"
      "  let u8F64Max: f64 = 255_u8\n"
      "  let i16F64Min: f64 = -32767_i16 - 1_i16\n"
      "  let i16F64Max: f64 = 32767_i16\n"
      "  let u16F64Max: f64 = 65535_u16\n"
      "  let i32F64Min: f64 = -2147483647_i32 - 1_i32\n"
      "  let i32F64Max: f64 = 2147483647_i32\n"
      "  let u32F64Max: f64 = 4294967295_u32\n"
      "  let f32F64: f64 = 1.5_f32\n"
      "  let parameterF64: f64 = widenParameter(value: 2.25_f32)\n"
      "  let callF64: f64 = produceF32()\n"
      "  let valid = i8F32Min == -128.0_f32 && "
      "i8F32Max == 127.0_f32 && u8F32Max == 255.0_f32 && "
      "i16F32Min == -32768.0_f32 && i16F32Max == 32767.0_f32 && "
      "u16F32Max == 65535.0_f32 && i8F64Min == -128.0_f64 && "
      "i8F64Max == 127.0_f64 && u8F64Max == 255.0_f64 && "
      "i16F64Min == -32768.0_f64 && i16F64Max == 32767.0_f64 && "
      "u16F64Max == 65535.0_f64 && i32F64Min == -2147483648.0_f64 && "
      "i32F64Max == 2147483647.0_f64 && "
      "u32F64Max == 4294967295.0_f64 && f32F64 == 1.5_f64 && "
      "parameterF64 == 2.25_f64 && callF64 == 1.5_f64\n"
      "  if valid { print(\"Numeric widen ok\") } else { "
      "print(\"Numeric widen bad\") }\n"
      "}\nentry(main)\n";
  static uint8_t sequence_artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts sequence_counts;
  w_seed_mlir0_result sequence_measured;
  w_seed_mlir0_result sequence_emitted;
  CHECK(lower_hir(SEQUENCE_SOURCE, sizeof(SEQUENCE_SOURCE) - 1u));
  CHECK(measure_current(&sequence_counts, &sequence_measured));
  CHECK(emit_current(sequence_artifact, sizeof(sequence_artifact),
                     &sequence_emitted));
  CHECK(sequence_counts.mlir_bytes == sequence_emitted.written.mlir_bytes &&
        count_bytes(sequence_artifact, sequence_counts.mlir_bytes,
                    "llvm.fpext %v") == 1u &&
        !contains_bytes(sequence_artifact, sequence_counts.mlir_bytes,
                        "fastmath"));
  CHECK(lower_hir(COMPOSITION_SOURCE, sizeof(COMPOSITION_SOURCE) - 1u));
  w_seed_mlir0_counts composition_counts;
  w_seed_mlir0_result composition_measured;
  w_seed_mlir0_result composition_emitted;
  static uint8_t composition_artifact[W_SEED_MLIR0_MAX_BYTES];
  CHECK(measure_current(&composition_counts, &composition_measured));
  CHECK(emit_current(composition_artifact, sizeof(composition_artifact),
                     &composition_emitted));
  CHECK(composition_counts.mlir_bytes ==
            composition_emitted.written.mlir_bytes &&
        count_bytes(composition_artifact, composition_counts.mlir_bytes,
                    "llvm.fpext ") == 4u &&
        count_bytes(composition_artifact, composition_counts.mlir_bytes,
                    "llvm.fadd ") == 2u &&
        !contains_bytes(composition_artifact, composition_counts.mlir_bytes,
                        "fastmath"));
  const w_seed_hir0_program *composition_program = &fixture.hir_program;
  size_t composition_wrappers = 0u;
  size_t composition_additions = 0u;
  for (size_t index = 0u; index < composition_program->value_count;
       index += 1u) {
    const w_seed_hir0_value *value = &composition_program->values[index];
    char expected[192];
    int expected_length = -1;
    if (value->kind == W_SEED_HIR0_VALUE_NUMERIC_WIDEN) {
      CHECK(value->source_type < composition_program->type_count &&
            value->type_index < composition_program->type_count &&
            composition_program->types[value->source_type].kind ==
                W_SEED_HIR0_TYPE_F32 &&
            composition_program->types[value->type_index].kind ==
                W_SEED_HIR0_TYPE_F64);
      char operand[32];
      CHECK(numeric_widen_expected_operand(
          composition_program, value->left_value, operand, sizeof(operand),
          0u));
      expected_length = snprintf(
          expected, sizeof(expected),
          "    %%v%u = llvm.fpext %s : f32 to f64\n",
          (unsigned int)index, operand);
      composition_wrappers += 1u;
    } else if (value->kind == W_SEED_HIR0_VALUE_BINARY_FLOAT &&
               value->binary_operator == W_SEED_HIR0_BINARY_ADD) {
      char left_operand[32];
      char right_operand[32];
      CHECK(numeric_widen_expected_operand(
                composition_program, value->left_value, left_operand,
                sizeof(left_operand), 0u) &&
            numeric_widen_expected_operand(
                composition_program, value->right_value, right_operand,
                sizeof(right_operand), 0u));
      expected_length = snprintf(
          expected, sizeof(expected),
          "    %%v%u = llvm.fadd %s, %s : f64\n",
          (unsigned int)index, left_operand, right_operand);
      composition_additions += 1u;
    } else {
      continue;
    }
    CHECK(expected_length > 0 && (size_t)expected_length < sizeof(expected) &&
          contains_bytes(composition_artifact, composition_counts.mlir_bytes,
                         expected));
  }
  CHECK(composition_wrappers == 4u && composition_additions == 2u);
  CHECK(lower_hir(SOURCE, sizeof(SOURCE) - 1u));
  const w_seed_hir0_program *program = &fixture.hir_program;
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);

  static uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0);
  CHECK(contains_bytes(artifact, counts.mlir_bytes,
                       "// " W_SEED_MLIR0_SCHEMA_VERSION "\nmodule ") &&
        count_bytes(artifact, counts.mlir_bytes, "llvm.sitofp ") == 10u &&
        count_bytes(artifact, counts.mlir_bytes, "llvm.uitofp ") == 5u &&
        count_bytes(artifact, counts.mlir_bytes, "llvm.fpext ") == 3u &&
        count_bytes(artifact, counts.mlir_bytes,
                    "_numeric_widen_bits = llvm.trunc") >= 15u &&
        !contains_bytes(artifact, counts.mlir_bytes, "fastmath") &&
        !contains_bytes(artifact, counts.mlir_bytes, "@_fltused") &&
        !contains_bytes(artifact, counts.mlir_bytes,
                        "w_seed_numeric_widen"));

  bool routes[11] = {false};
  size_t wrapper_count = 0u;
  uint32_t forged_wrapper = W_SEED_HIR0_NONE;
  for (size_t index = 0u; index < program->value_count; index += 1u) {
    const w_seed_hir0_value *value = &program->values[index];
    if (value->kind != W_SEED_HIR0_VALUE_NUMERIC_WIDEN) continue;
    CHECK(value->source_type < program->type_count &&
          value->type_index < program->type_count &&
          value->left_value < program->value_count &&
          value->right_value == W_SEED_HIR0_NONE);
    const w_seed_hir0_type *source_type =
        &program->types[value->source_type];
    const w_seed_hir0_type *destination_type =
        &program->types[value->type_index];
    const w_seed_hir0_value *child = &program->values[value->left_value];
    CHECK(child->type_index == value->source_type &&
          child->owner_kind == W_SEED_HIR0_VALUE_OWNER_NUMERIC_WIDEN &&
          child->owner_index == index && child->owner_ordinal == 0u);

    char operand[32];
    CHECK(numeric_widen_expected_operand(program, value->left_value, operand,
                                         sizeof(operand), 0u));
    char expected[192];
    int expected_length = -1;
    size_t route = SIZE_MAX;
    const char *destination_name =
        destination_type->kind == W_SEED_HIR0_TYPE_F32 ? "f32" :
        destination_type->kind == W_SEED_HIR0_TYPE_F64 ? "f64" : NULL;
    if (source_type->kind == W_SEED_HIR0_TYPE_F32) {
      CHECK(destination_type->kind == W_SEED_HIR0_TYPE_F64);
      route = 10u;
      expected_length = snprintf(
          expected, sizeof(expected),
          "    %%v%u = llvm.fpext %s : f32 to f64\n",
          (unsigned int)index, operand);
    } else {
      CHECK(source_type->kind == W_SEED_HIR0_TYPE_INTEGER &&
            destination_name != NULL);
      const bool is_signed = source_type->integer_is_signed;
      const uint16_t width = source_type->integer_bit_width;
      if (destination_type->kind == W_SEED_HIR0_TYPE_F32) {
        route = width == 8u ? (is_signed ? 0u : 1u)
                : width == 16u ? (is_signed ? 2u : 3u)
                               : SIZE_MAX;
      } else if (destination_type->kind == W_SEED_HIR0_TYPE_F64) {
        route = width == 8u ? (is_signed ? 4u : 5u)
                : width == 16u ? (is_signed ? 6u : 7u)
                : width == 32u ? (is_signed ? 8u : 9u)
                               : SIZE_MAX;
      }
      CHECK(route != SIZE_MAX && width != 0u && width < 64u);
      expected_length = snprintf(
          expected, sizeof(expected),
          "    %%v%u_numeric_widen_bits = llvm.trunc %s : i64 to i%u\n"
          "    %%v%u = llvm.%s %%v%u_numeric_widen_bits : i%u to %s\n",
          (unsigned int)index, operand, (unsigned int)width,
          (unsigned int)index,
          is_signed ? "sitofp" : "uitofp", (unsigned int)index,
          (unsigned int)width, destination_name);
    }
    CHECK(route < sizeof(routes) / sizeof(routes[0]) && expected_length > 0 &&
          (size_t)expected_length < sizeof(expected) &&
          contains_bytes(artifact, counts.mlir_bytes, expected));
    routes[route] = true;
    if (forged_wrapper == W_SEED_HIR0_NONE)
      forged_wrapper = (uint32_t)index;
    wrapper_count += 1u;
  }
  CHECK(wrapper_count == 18u);
  for (size_t route = 0u; route < sizeof(routes) / sizeof(routes[0]);
       route += 1u)
    CHECK(routes[route]);
  CHECK(forged_wrapper != W_SEED_HIR0_NONE);

  const w_seed_hir0_value saved_wrapper = fixture.hir_values[forged_wrapper];
  const w_seed_hir0_value saved_child =
      fixture.hir_values[saved_wrapper.left_value];
  const w_seed_mlir0_input input = mlir_input();
  static uint8_t windows_artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts windows_counts;
  w_seed_mlir0_result windows_measured;
  w_seed_mlir0_result windows_emitted;
  CHECK(w_seed_mlir0_measure(&input, &WINDOWS_TARGET, &windows_counts,
                             &windows_measured) == W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_emit(
            &input, &WINDOWS_TARGET,
            &(w_seed_mlir0_output){windows_artifact,
                                   sizeof(windows_artifact)},
            &windows_emitted) == W_SEED_MLIR0_OK);
  CHECK(windows_counts.mlir_bytes == windows_emitted.written.mlir_bytes &&
        memcmp(windows_measured.mlir_sha256, windows_emitted.mlir_sha256,
               sizeof(windows_measured.mlir_sha256)) == 0 &&
        count_bytes(windows_artifact, windows_counts.mlir_bytes,
                    "llvm.mlir.global @_fltused(0 : i32) : i32") == 1u &&
        count_bytes(windows_artifact, windows_counts.mlir_bytes,
                    "@_fltused") == 1u);

  w_seed_mlir0_counts rejected_counts = {0x17u};
  w_seed_mlir0_result rejected_result;
  (void)memset(&rejected_result, 0x2au, sizeof(rejected_result));
  const w_seed_mlir0_result rejected_result_snapshot = rejected_result;

  fixture.hir_values[forged_wrapper].source_type = saved_wrapper.type_index;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result) &&
        w_seed_mlir0_measure(&input, &TARGET, &rejected_counts,
                             &rejected_result) == W_SEED_MLIR0_INVALID_HIR &&
        rejected_counts.mlir_bytes == 0x17u &&
        memcmp(&rejected_result, &rejected_result_snapshot,
               sizeof(rejected_result)) == 0 &&
        w_seed_native_subset0_select_program(
            program, &fixture.hir_result, &selection) ==
            W_SEED_NATIVE_SUBSET0_INVALID);
  fixture.hir_values[forged_wrapper] = saved_wrapper;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[forged_wrapper].type_index = saved_child.type_index;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result) &&
        w_seed_mlir0_measure(&input, &TARGET, &rejected_counts,
                             &rejected_result) == W_SEED_MLIR0_INVALID_HIR &&
        w_seed_native_subset0_select_program(
            program, &fixture.hir_result, &selection) ==
            W_SEED_NATIVE_SUBSET0_INVALID);
  fixture.hir_values[forged_wrapper] = saved_wrapper;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[saved_wrapper.left_value].type_index =
      saved_wrapper.type_index;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result) &&
        w_seed_mlir0_measure(&input, &TARGET, &rejected_counts,
                             &rejected_result) == W_SEED_MLIR0_INVALID_HIR);
  fixture.hir_values[saved_wrapper.left_value] = saved_child;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[saved_wrapper.left_value].owner_index =
      W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result) &&
        w_seed_mlir0_measure(&input, &TARGET, &rejected_counts,
                             &rejected_result) == W_SEED_MLIR0_INVALID_HIR);
  fixture.hir_values[saved_wrapper.left_value] = saved_child;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool mlir_test_unsigned_integer_width(
    const w_seed_hir0_program *program, uint32_t type_index,
    uint16_t bit_width) {
  if (program == NULL || type_index >= program->type_count) return false;
  const w_seed_hir0_type *type = &program->types[type_index];
  return (type->kind == W_SEED_HIR0_TYPE_I64 ||
          type->kind == W_SEED_HIR0_TYPE_U64 ||
          type->kind == W_SEED_HIR0_TYPE_INTEGER) &&
         !type->integer_is_signed && type->integer_bit_width == bit_width;
}

static bool test_float_bits_mlir(void) {
  static const uint8_t SOURCE[] =
      "fn main() {\n"
      "  let f32PositiveZero: f32 = f32.fromBits(0x00000000_u32)\n"
      "  let f32NegativeZero: f32 = f32.fromBits(0x80000000_u32)\n"
      "  let f32Subnormal: f32 = f32.fromBits(0x00000001_u32)\n"
      "  let f32Infinity: f32 = f32.fromBits(0x7f800000_u32)\n"
      "  let f32NanPayload: f32 = f32.fromBits(0x7fc01234_u32)\n"
      "  let f32PositiveZeroBits: u32 = f32PositiveZero.toBits()\n"
      "  let f32NegativeZeroBits: u32 = f32NegativeZero.toBits()\n"
      "  let f32SubnormalBits: u32 = f32Subnormal.toBits()\n"
      "  let f32InfinityBits: u32 = f32Infinity.toBits()\n"
      "  let f32NanPayloadBits: u32 = f32NanPayload.toBits()\n"
      "  let f64PositiveZero: f64 = f64.fromBits(0x0000000000000000_u64)\n"
      "  let f64NegativeZero: f64 = f64.fromBits(0x8000000000000000_u64)\n"
      "  let f64Subnormal: f64 = f64.fromBits(0x0000000000000001_u64)\n"
      "  let f64Infinity: f64 = f64.fromBits(0x7ff0000000000000_u64)\n"
      "  let f64NanPayload: f64 = f64.fromBits(0x7ff8123456789abc_u64)\n"
      "  let f64PositiveZeroBits: u64 = f64PositiveZero.toBits()\n"
      "  let f64NegativeZeroBits: u64 = f64NegativeZero.toBits()\n"
      "  let f64SubnormalBits: u64 = f64Subnormal.toBits()\n"
      "  let f64InfinityBits: u64 = f64Infinity.toBits()\n"
      "  let f64NanPayloadBits: u64 = f64NanPayload.toBits()\n"
      "  print(\"float bits\")\n"
      "}\nentry(main)\n";
  static const uint64_t F32_PATTERNS[] = {
      UINT64_C(0x00000000), UINT64_C(0x80000000), UINT64_C(0x00000001),
      UINT64_C(0x7f800000), UINT64_C(0x7fc01234)};
  static const uint64_t F64_PATTERNS[] = {
      UINT64_C(0x0000000000000000), UINT64_C(0x8000000000000000),
      UINT64_C(0x0000000000000001), UINT64_C(0x7ff0000000000000),
      UINT64_C(0x7ff8123456789abc)};

  CHECK(lower_hir(SOURCE, sizeof(SOURCE) - 1u));
  const w_seed_hir0_program *program = &fixture.hir_program;
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);

  static uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0 &&
        !contains_bytes(artifact, counts.mlir_bytes, "@_fltused"));

  bool f32_patterns[sizeof(F32_PATTERNS) / sizeof(F32_PATTERNS[0])] = {false};
  bool f64_patterns[sizeof(F64_PATTERNS) / sizeof(F64_PATTERNS[0])] = {false};
  size_t route_counts[4] = {0u, 0u, 0u, 0u};
  uint32_t from32 = W_SEED_HIR0_NONE;
  uint32_t to32 = W_SEED_HIR0_NONE;
  uint32_t from64 = W_SEED_HIR0_NONE;
  uint32_t to64 = W_SEED_HIR0_NONE;
  for (size_t index = 0u; index < program->value_count; index += 1u) {
    const w_seed_hir0_value *value = &program->values[index];
    if (value->kind != W_SEED_HIR0_VALUE_FLOAT_FROM_BITS &&
        value->kind != W_SEED_HIR0_VALUE_FLOAT_TO_BITS)
      continue;
    CHECK(value->left_value < program->value_count &&
          value->right_value == W_SEED_HIR0_NONE &&
          value->source_type < program->type_count &&
          value->type_index < program->type_count);
    const w_seed_hir0_value *child = &program->values[value->left_value];
    CHECK(child->type_index == value->source_type &&
          child->owner_kind ==
              W_SEED_HIR0_VALUE_OWNER_FLOAT_BITS_CONVERSION &&
          child->owner_index == index && child->owner_ordinal == 0u);

    const w_seed_hir0_type *source_type =
        &program->types[value->source_type];
    const w_seed_hir0_type *destination_type =
        &program->types[value->type_index];
    uint32_t route = W_SEED_HIR0_NONE;
    uint16_t bit_width = 0u;
    if (value->kind == W_SEED_HIR0_VALUE_FLOAT_FROM_BITS) {
      CHECK(child->kind == W_SEED_HIR0_VALUE_CONST_U64);
      if (mlir_test_unsigned_integer_width(program, value->source_type,
                                           32u) &&
          destination_type->kind == W_SEED_HIR0_TYPE_F32) {
        route = 0u;
        bit_width = 32u;
        for (size_t pattern = 0u;
             pattern < sizeof(F32_PATTERNS) / sizeof(F32_PATTERNS[0]);
             pattern += 1u)
          if (child->unsigned_integer_value == F32_PATTERNS[pattern])
            f32_patterns[pattern] = true;
        if (from32 == W_SEED_HIR0_NONE) from32 = (uint32_t)index;
      } else if (mlir_test_unsigned_integer_width(
                     program, value->source_type, 64u) &&
                 destination_type->kind == W_SEED_HIR0_TYPE_F64) {
        route = 1u;
        bit_width = 64u;
        for (size_t pattern = 0u;
             pattern < sizeof(F64_PATTERNS) / sizeof(F64_PATTERNS[0]);
             pattern += 1u)
          if (child->unsigned_integer_value == F64_PATTERNS[pattern])
            f64_patterns[pattern] = true;
        if (from64 == W_SEED_HIR0_NONE) from64 = (uint32_t)index;
      }
    } else if (source_type->kind == W_SEED_HIR0_TYPE_F32 &&
               mlir_test_unsigned_integer_width(
                   program, value->type_index, 32u)) {
      route = 2u;
      bit_width = 32u;
      if (to32 == W_SEED_HIR0_NONE) to32 = (uint32_t)index;
    } else if (source_type->kind == W_SEED_HIR0_TYPE_F64 &&
               mlir_test_unsigned_integer_width(
                   program, value->type_index, 64u)) {
      route = 3u;
      bit_width = 64u;
      if (to64 == W_SEED_HIR0_NONE) to64 = (uint32_t)index;
    }
    CHECK(route != W_SEED_HIR0_NONE);

    char operand[32];
    CHECK(numeric_widen_expected_operand(
        program, value->left_value, operand, sizeof(operand), 0u));
    char expected[256];
    int expected_length = -1;
    if (value->kind == W_SEED_HIR0_VALUE_FLOAT_FROM_BITS &&
        bit_width == 32u) {
      expected_length = snprintf(
          expected, sizeof(expected),
          "    %%v%u_float_bits_narrow = llvm.trunc %s : i64 to i32\n"
          "    %%v%u = llvm.bitcast %%v%u_float_bits_narrow : i32 to f32\n",
          (unsigned int)index, operand, (unsigned int)index,
          (unsigned int)index);
    } else if (value->kind == W_SEED_HIR0_VALUE_FLOAT_FROM_BITS) {
      expected_length = snprintf(
          expected, sizeof(expected),
          "    %%v%u = llvm.bitcast %s : i64 to f64\n",
          (unsigned int)index, operand);
    } else if (bit_width == 32u) {
      expected_length = snprintf(
          expected, sizeof(expected),
          "    %%v%u_float_bits_i32 = llvm.bitcast %s : f32 to i32\n"
          "    %%v%u = llvm.zext %%v%u_float_bits_i32 : i32 to i64\n",
          (unsigned int)index, operand, (unsigned int)index,
          (unsigned int)index);
    } else {
      expected_length = snprintf(
          expected, sizeof(expected),
          "    %%v%u = llvm.bitcast %s : f64 to i64\n",
          (unsigned int)index, operand);
    }
    CHECK(expected_length > 0 &&
          (size_t)expected_length < sizeof(expected) &&
          contains_bytes(artifact, counts.mlir_bytes, expected));
    route_counts[route] += 1u;
  }
  CHECK(route_counts[0] == 5u && route_counts[1] == 5u &&
        route_counts[2] == 5u && route_counts[3] == 5u &&
        from32 != W_SEED_HIR0_NONE && to32 != W_SEED_HIR0_NONE &&
        from64 != W_SEED_HIR0_NONE && to64 != W_SEED_HIR0_NONE);
  for (size_t pattern = 0u;
       pattern < sizeof(F32_PATTERNS) / sizeof(F32_PATTERNS[0]);
       pattern += 1u)
    CHECK(f32_patterns[pattern]);
  for (size_t pattern = 0u;
       pattern < sizeof(F64_PATTERNS) / sizeof(F64_PATTERNS[0]);
       pattern += 1u)
    CHECK(f64_patterns[pattern]);
  CHECK(count_bytes(artifact, counts.mlir_bytes,
                    "_float_bits_narrow = llvm.trunc") == 5u &&
        count_bytes(artifact, counts.mlir_bytes,
                    "_float_bits_i32 = llvm.bitcast") == 5u &&
        count_bytes(artifact, counts.mlir_bytes, "llvm.bitcast ") == 20u &&
        count_bytes(artifact, counts.mlir_bytes, "llvm.zext ") == 5u &&
        !contains_bytes(artifact, counts.mlir_bytes, "llvm.sitofp") &&
        !contains_bytes(artifact, counts.mlir_bytes, "llvm.uitofp") &&
        !contains_bytes(artifact, counts.mlir_bytes, "llvm.fpext") &&
        !contains_bytes(artifact, counts.mlir_bytes, "llvm.fptrunc") &&
        !contains_bytes(artifact, counts.mlir_bytes, "llvm.fptosi") &&
        !contains_bytes(artifact, counts.mlir_bytes, "llvm.fptoui") &&
        !contains_bytes(artifact, counts.mlir_bytes, "llvm.fadd") &&
        !contains_bytes(artifact, counts.mlir_bytes, "llvm.fsub") &&
        !contains_bytes(artifact, counts.mlir_bytes, "llvm.fmul") &&
        !contains_bytes(artifact, counts.mlir_bytes, "llvm.fdiv") &&
        !contains_bytes(artifact, counts.mlir_bytes, "llvm.fneg") &&
        !contains_bytes(artifact, counts.mlir_bytes, "fastmath") &&
        !contains_bytes(artifact, counts.mlir_bytes, "@malloc") &&
        !contains_bytes(artifact, counts.mlir_bytes, "@free") &&
        !contains_bytes(artifact, counts.mlir_bytes, "@memcpy") &&
        !contains_bytes(artifact, counts.mlir_bytes, "@w_seed_float_bits"));

  static uint8_t windows_artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts windows_counts;
  w_seed_mlir0_result windows_measured;
  w_seed_mlir0_result windows_emitted;
  const w_seed_mlir0_input input = mlir_input();
  CHECK(w_seed_mlir0_measure(&input, &WINDOWS_TARGET, &windows_counts,
                             &windows_measured) == W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_emit(
            &input, &WINDOWS_TARGET,
            &(w_seed_mlir0_output){windows_artifact,
                                   sizeof(windows_artifact)},
            &windows_emitted) == W_SEED_MLIR0_OK);
  CHECK(windows_counts.mlir_bytes == windows_emitted.written.mlir_bytes &&
        memcmp(windows_measured.mlir_sha256, windows_emitted.mlir_sha256,
               sizeof(windows_measured.mlir_sha256)) == 0 &&
        count_bytes(windows_artifact, windows_counts.mlir_bytes,
                    "llvm.mlir.global @_fltused(0 : i32) : i32") == 1u &&
        count_bytes(windows_artifact, windows_counts.mlir_bytes,
                    "@_fltused") == 1u);

  const w_seed_hir0_value saved_from32 = fixture.hir_values[from32];
  const w_seed_hir0_value saved_to64 = fixture.hir_values[to64];
  const w_seed_hir0_value saved_to64_child =
      fixture.hir_values[saved_to64.left_value];
  const w_seed_hir0_type saved_u32 =
      fixture.hir_types[saved_from32.source_type];
  w_seed_mlir0_counts rejected_counts;
  (void)memset(&rejected_counts, 0xa5u, sizeof(rejected_counts));
  const w_seed_mlir0_counts rejected_counts_snapshot = rejected_counts;
  w_seed_mlir0_result rejected_result;
  (void)memset(&rejected_result, 0x2au, sizeof(rejected_result));
  const w_seed_mlir0_result rejected_result_snapshot = rejected_result;

  fixture.hir_values[from32].source_type = saved_to64.source_type;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result) &&
        w_seed_native_subset0_select_program(
            program, &fixture.hir_result, &selection) ==
            W_SEED_NATIVE_SUBSET0_INVALID &&
        w_seed_mlir0_measure(&input, &TARGET, &rejected_counts,
                             &rejected_result) == W_SEED_MLIR0_INVALID_HIR &&
        memcmp(&rejected_counts, &rejected_counts_snapshot,
               sizeof(rejected_counts)) == 0 &&
        memcmp(&rejected_result, &rejected_result_snapshot,
               sizeof(rejected_result)) == 0);
  fixture.hir_values[from32] = saved_from32;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[from32].type_index = saved_to64.type_index;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result) &&
        w_seed_native_subset0_select_program(
            program, &fixture.hir_result, &selection) ==
            W_SEED_NATIVE_SUBSET0_INVALID &&
        w_seed_mlir0_measure(&input, &TARGET, &rejected_counts,
                             &rejected_result) == W_SEED_MLIR0_INVALID_HIR);
  fixture.hir_values[from32] = saved_from32;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[to64].type_index = saved_from32.source_type;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result) &&
        w_seed_native_subset0_select_program(
            program, &fixture.hir_result, &selection) ==
            W_SEED_NATIVE_SUBSET0_INVALID &&
        w_seed_mlir0_measure(&input, &TARGET, &rejected_counts,
                             &rejected_result) == W_SEED_MLIR0_INVALID_HIR);
  fixture.hir_values[to64] = saved_to64;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_types[saved_from32.source_type].integer_bit_width = 64u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result) &&
        w_seed_native_subset0_select_program(
            program, &fixture.hir_result, &selection) ==
            W_SEED_NATIVE_SUBSET0_INVALID &&
        w_seed_mlir0_measure(&input, &TARGET, &rejected_counts,
                             &rejected_result) == W_SEED_MLIR0_INVALID_HIR);
  fixture.hir_types[saved_from32.source_type] = saved_u32;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[saved_to64.left_value].owner_kind =
      W_SEED_HIR0_VALUE_OWNER_ARGUMENT;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result) &&
        w_seed_native_subset0_select_program(
            program, &fixture.hir_result, &selection) ==
            W_SEED_NATIVE_SUBSET0_INVALID &&
        w_seed_mlir0_measure(&input, &TARGET, &rejected_counts,
                             &rejected_result) == W_SEED_MLIR0_INVALID_HIR);
  fixture.hir_values[saved_to64.left_value] = saved_to64_child;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[saved_to64.left_value].owner_index = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result) &&
        w_seed_native_subset0_select_program(
            program, &fixture.hir_result, &selection) ==
            W_SEED_NATIVE_SUBSET0_INVALID &&
        w_seed_mlir0_measure(&input, &TARGET, &rejected_counts,
                             &rejected_result) == W_SEED_MLIR0_INVALID_HIR);
  fixture.hir_values[saved_to64.left_value] = saved_to64_child;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[saved_to64.left_value].owner_ordinal = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result) &&
        w_seed_mlir0_measure(&input, &TARGET, &rejected_counts,
                             &rejected_result) == W_SEED_MLIR0_INVALID_HIR);
  fixture.hir_values[saved_to64.left_value] = saved_to64_child;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_windows_target_runtime_surface(void) {
  static const uint8_t source[] =
      "fn main() { print(\"Hello, Windows!\") }\nentry(main)\n";
  CHECK(w_seed_mlir0_target_is_supported(&WINDOWS_TARGET));
  CHECK(lower_hir(source, sizeof(source) - 1u));
  const w_seed_mlir0_input input = mlir_input();
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  CHECK(w_seed_mlir0_measure(&input, &WINDOWS_TARGET, &counts, &measured) ==
        W_SEED_MLIR0_OK);
  uint8_t first[W_SEED_MLIR0_MAX_BYTES];
  uint8_t second[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_result first_result;
  w_seed_mlir0_result second_result;
  CHECK(w_seed_mlir0_emit(&input, &WINDOWS_TARGET,
                          &(w_seed_mlir0_output){first, sizeof(first)},
                          &first_result) == W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_emit(&input, &WINDOWS_TARGET,
                          &(w_seed_mlir0_output){second, sizeof(second)},
                          &second_result) == W_SEED_MLIR0_OK);
  CHECK(first_result.written.mlir_bytes == counts.mlir_bytes &&
        second_result.written.mlir_bytes == counts.mlir_bytes &&
        memcmp(first, second, counts.mlir_bytes) == 0 &&
        memcmp(first_result.mlir_sha256, measured.mlir_sha256,
               sizeof(first_result.mlir_sha256)) == 0);
  CHECK(contains_bytes(first, counts.mlir_bytes,
                       "// " W_SEED_MLIR0_WINDOWS_SCHEMA_VERSION "\nmodule "));
  CHECK(contains_bytes(first, counts.mlir_bytes,
                       "x86_64-pc-windows-msvc"));
  CHECK(contains_bytes(first, counts.mlir_bytes, "@GetStdHandle"));
  CHECK(contains_bytes(first, counts.mlir_bytes, "@WriteFile"));
  CHECK(contains_bytes(first, counts.mlir_bytes, "@ExitProcess"));
  CHECK(contains_bytes(
      first, counts.mlir_bytes,
      "llvm.func @ExitProcess(%code: i32) attributes "
      "{passthrough = [\"noreturn\"]}\n"));
  CHECK(contains_bytes(first, counts.mlir_bytes, "@mainCRTStartup"));
  CHECK(!contains_bytes(first, counts.mlir_bytes, "@w_seed_mlir0_buffer"));
  CHECK(!contains_bytes(first, counts.mlir_bytes, "@_fltused"));
  CHECK(!contains_bytes(first, counts.mlir_bytes, "llvm.mlir.zero"));
  CHECK(contains_bytes(first, counts.mlir_bytes,
                       "llvm.load %written : !llvm.ptr -> i32"));
  CHECK(contains_bytes(first, counts.mlir_bytes, "%write_complete"));
  CHECK(contains_bytes(first, counts.mlir_bytes, "-11 : i32"));
  CHECK(!contains_bytes(first, counts.mlir_bytes, "@write("));
  CHECK(!contains_bytes(first, counts.mlir_bytes, "HeapAlloc"));
  CHECK(!contains_bytes(first, counts.mlir_bytes, "HeapFree"));
  CHECK(!contains_bytes(first, counts.mlir_bytes,
                       W_SEED_MLIR0_TARGET_TRIPLE_LINUX));
  (void)memset(first, 0x5bu, sizeof(first));
  (void)memset(&first_result, 0x24u, sizeof(first_result));
  const w_seed_mlir0_result snapshot = first_result;
  CHECK(w_seed_mlir0_emit(
            &input, &WINDOWS_TARGET,
            &(w_seed_mlir0_output){first, counts.mlir_bytes - 1u},
            &first_result) == W_SEED_MLIR0_CAPACITY);
  for (size_t index = 0u; index < sizeof(first); index += 1u)
    CHECK(first[index] == 0x5bu);
  CHECK(memcmp(&first_result, &snapshot, sizeof(snapshot)) == 0);

  static const uint8_t interpolated[] =
      "fn main() { let message = \"Open\" "
      "print(\"${message}\") }\nentry(main)\n";
  CHECK(lower_hir(interpolated, sizeof(interpolated) - 1u));
  const w_seed_mlir0_input dynamic_input = mlir_input();
  w_seed_mlir0_counts dynamic_counts;
  w_seed_mlir0_result dynamic_result;
  CHECK(w_seed_mlir0_measure(&dynamic_input, &WINDOWS_TARGET, &dynamic_counts,
                             &dynamic_result) == W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_emit(
            &dynamic_input, &WINDOWS_TARGET,
            &(w_seed_mlir0_output){second, sizeof(second)}, &dynamic_result) ==
        W_SEED_MLIR0_OK);
  w_seed_native_subset0_program dynamic_selection;
  CHECK(w_seed_native_subset0_select_program(
            &fixture.hir_program, &fixture.hir_result, &dynamic_selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(dynamic_selection.maximum_stdout_bytes == 5u);
  CHECK(contains_bytes(second, dynamic_counts.mlir_bytes,
                       "@w_seed_mlir0_buffer() : !llvm.array<6 x i8>"));
  CHECK(contains_bytes(second, dynamic_counts.mlir_bytes,
                       "llvm.getelementptr %buffer_base[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<6 x i8>"));
  CHECK(contains_bytes(second, dynamic_counts.mlir_bytes,
                       "llvm.getelementptr %buffer[%cursor2]"));
  CHECK(contains_bytes(second, dynamic_counts.mlir_bytes,
                       "llvm.store %output_terminator_zero, %output_terminator_address"));
  CHECK(contains_bytes(second, dynamic_counts.mlir_bytes, "@w_seed_write"));
  CHECK(contains_bytes(second, dynamic_counts.mlir_bytes, "@mainCRTStartup"));
  CHECK(contains_bytes(second, dynamic_counts.mlir_bytes, "%write_complete"));
  CHECK(!contains_bytes(second, dynamic_counts.mlir_bytes, "@write("));
  CHECK(!contains_bytes(second, dynamic_counts.mlir_bytes, "HeapAlloc"));
  CHECK(!contains_bytes(second, dynamic_counts.mlir_bytes, "HeapFree"));
  return true;
}

static bool test_restaurant_and_nul(void) {
  static const uint8_t literal[] =
      "fn serve() { print(\"Table 42 remains open\") }\nentry(serve)\n";
  static const uint8_t binding[] =
      "fn serve() { let message = \"Table 42 remains open\" "
      "print(message) }\nentry(serve)\n";
  uint8_t literal_artifact[W_SEED_MLIR0_MAX_BYTES];
  uint8_t binding_artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_result literal_result;
  w_seed_mlir0_result binding_result;
  w_seed_mlir0_counts literal_counts;
  w_seed_mlir0_counts binding_counts;
  CHECK(lower_hir(literal, sizeof(literal) - 1u));
  CHECK(measure_current(&literal_counts, &literal_result));
  CHECK(emit_current(literal_artifact, sizeof(literal_artifact),
                     &literal_result));
  CHECK(lower_hir(binding, sizeof(binding) - 1u));
  CHECK(fixture.hir_program.binding_count == 1u);
  CHECK(measure_current(&binding_counts, &binding_result));
  CHECK(emit_current(binding_artifact, sizeof(binding_artifact),
                     &binding_result));
  CHECK(literal_counts.mlir_bytes == binding_counts.mlir_bytes &&
        memcmp(literal_artifact, binding_artifact, literal_counts.mlir_bytes) ==
            0 &&
        memcmp(literal_result.mlir_sha256, binding_result.mlir_sha256,
               sizeof(literal_result.mlir_sha256)) == 0);
  CHECK(contains_bytes(binding_artifact, binding_counts.mlir_bytes,
                       "\\54\\61\\62\\6c"));

  static const uint8_t nul_source[] =
      "fn main() { print(\"A\0B\") }\nentry(main)\n";
  CHECK(lower_hir(nul_source, sizeof(nul_source) - 1u));
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result result;
  CHECK(measure_current(&counts, &result));
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  (void)memset(output, 0x6du, sizeof(output));
  CHECK(emit_current(output, sizeof(output), &result));
  CHECK(contains_bytes(output, counts.mlir_bytes, "\\41\\00\\42\\0a"));
  CHECK(contains_bytes(output, counts.mlir_bytes, "!llvm.array<4 x i8>"));
  CHECK(output[counts.mlir_bytes] == 0x6du);
  return true;
}

static bool expect_sequence_unsupported(void);

static bool test_typed_interpolation_artifact(void) {
  static const uint8_t source[] =
      "fn main() { print(\"The answer is ${6 * 7}!\") }\nentry(main)\n";
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(fixture.hir_program.value_count == 4u);
  CHECK(fixture.hir_program.interpolation_segment_count == 3u);
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  CHECK(measure_current(&counts, &measured));
  CHECK(counts.mlir_bytes > 0u && counts.mlir_bytes < W_SEED_MLIR0_MAX_BYTES);
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  (void)memset(output, 0xa7u, sizeof(output));
  w_seed_mlir0_result emitted;
  CHECK(emit_current(output, sizeof(output), &emitted));
  CHECK(emitted.written.mlir_bytes == counts.mlir_bytes &&
        memcmp(emitted.mlir_sha256, measured.mlir_sha256,
               sizeof(emitted.mlir_sha256)) == 0);
  CHECK(contains_bytes(output, counts.mlir_bytes,
                       "llvm.call @w_seed_checked_multiply_i64(%v0, %v1, "
                       "%v2_checked_width) : (i64, i64, i64) -> i64"));
  CHECK(!contains_bytes(output, counts.mlir_bytes, "llvm.add %v") &&
        !contains_bytes(output, counts.mlir_bytes, "llvm.sub %v") &&
        !contains_bytes(output, counts.mlir_bytes, "llvm.mul %v"));
  CHECK(contains_bytes(output, counts.mlir_bytes,
                       "llvm.call @w_seed_append_i64"));
  CHECK(contains_bytes(output, counts.mlir_bytes,
                       "llvm.func internal @w_seed_copy"));
  CHECK(!contains_bytes(output, counts.mlir_bytes,
                        "llvm.func internal @w_seed_append_bool"));
  CHECK(!contains_bytes(output, counts.mlir_bytes, "snprintf"));
  CHECK(!contains_bytes(output, counts.mlir_bytes, "%ld"));
  CHECK(!contains_bytes(output, counts.mlir_bytes, "vararg"));
  CHECK(!contains_bytes(output, counts.mlir_bytes, "The answer is 42"));
  CHECK(output[counts.mlir_bytes] == 0xa7u);

  (void)memset(output, 0xb7u, sizeof(output));
  (void)memset(&emitted, 0xc7u, sizeof(emitted));
  const w_seed_mlir0_result result_snapshot = emitted;
  const w_seed_mlir0_input input = mlir_input();
  CHECK(w_seed_mlir0_emit(
            &input, &TARGET,
            &(w_seed_mlir0_output){output, counts.mlir_bytes - 1u},
            &emitted) == W_SEED_MLIR0_CAPACITY);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0xb7u);
  CHECK(memcmp(&emitted, &result_snapshot, sizeof(emitted)) == 0);

  static const uint8_t nul_text[] =
      "fn main() { print(\"A\0${6 * 7}\") }\nentry(main)\n";
  CHECK(lower_hir(nul_text, sizeof(nul_text) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(output, sizeof(output), &emitted));
  CHECK(contains_bytes(output, emitted.written.mlir_bytes, "\\41\\00"));
  CHECK(contains_bytes(output, emitted.written.mlir_bytes,
                       "llvm.call @w_seed_append_i64"));

  static const uint8_t builtin_display[] =
      "fn main() { let state = \"open\" "
      "print(\"Kitchen ${true}/${false}; table: ${state}\") }\n"
      "entry(main)\n";
  CHECK(lower_hir(builtin_display, sizeof(builtin_display) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(output, sizeof(output), &emitted));
  CHECK(contains_bytes(output, emitted.written.mlir_bytes,
                       "llvm.func internal @w_seed_append_bool"));
  CHECK(contains_bytes(output, emitted.written.mlir_bytes,
                       "llvm.call @w_seed_append_bool"));
  CHECK(contains_bytes(output, emitted.written.mlir_bytes,
                       "llvm.mlir.constant(true) : i1"));
  CHECK(contains_bytes(output, emitted.written.mlir_bytes,
                       "llvm.mlir.constant(false) : i1"));
  CHECK(contains_bytes(output, emitted.written.mlir_bytes,
                       "\\6f\\70\\65\\6e"));

  static const uint8_t typed_bindings[] =
      "fn serve() { let table = 6 * 7 let isOpen = true "
      "let state = \"open\" "
      "print(\"Table ${table}; open: ${isOpen}; state: ${state}\") }\n"
      "entry(serve)\n";
  CHECK(lower_hir(typed_bindings, sizeof(typed_bindings) - 1u));
  CHECK(fixture.hir_program.binding_count == 3u &&
        fixture.hir_program.bindings[0].initializer_value == 2u &&
        fixture.hir_program.bindings[1].initializer_value == 3u &&
        fixture.hir_program.bindings[2].initializer_value == 4u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(output, sizeof(output), &emitted));
  CHECK(contains_bytes(output, emitted.written.mlir_bytes,
                       "llvm.call @w_seed_checked_multiply_i64(%v0, %v1, "
                       "%v2_checked_width) : (i64, i64, i64) -> i64"));
  CHECK(!contains_bytes(output, emitted.written.mlir_bytes, "llvm.add %v") &&
        !contains_bytes(output, emitted.written.mlir_bytes, "llvm.sub %v") &&
        !contains_bytes(output, emitted.written.mlir_bytes, "llvm.mul %v"));
  CHECK(contains_bytes(output, emitted.written.mlir_bytes,
                       "@w_seed_append_i64(%buffer, %cursor1, %v2)"));
  CHECK(contains_bytes(output, emitted.written.mlir_bytes,
                       "@w_seed_append_bool(%buffer, %cursor3, %v3)"));
  CHECK(contains_bytes(output, emitted.written.mlir_bytes,
                       "\\6f\\70\\65\\6e"));

  static const uint8_t nul_string_value[] =
      "fn main() { let state = \"A\0B\" print(\"${state}\") }\n"
      "entry(main)\n";
  CHECK(lower_hir(nul_string_value, sizeof(nul_string_value) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(output, sizeof(output), &emitted));
  CHECK(contains_bytes(output, emitted.written.mlir_bytes,
                       "\\41\\00\\42\\0a"));
  CHECK(!contains_bytes(output, emitted.written.mlir_bytes,
                        "llvm.func internal @w_seed_append_bool"));
  return true;
}

static bool test_fixed_integer_family_artifact(void) {
  static const uint8_t source[] =
      "entry { "
      "let signed8: i8 = i8.wrappingAdd(127_i8, 1_i8) "
      "let unsigned8: u8 = u8.wrappingSubtract(0_u8, 1_u8) "
      "let signed16: i16 = i16.wrappingMultiply(32767_i16, 2_i16) "
      "let unsigned16: u16 = u16.wrappingShiftLeft(1_u16, 15_u64) "
      "let signed32: i32 = i32.wrappingNegate(1_i32) "
      "let unsigned32: u32 = u32.wrappingPower(2_u32, 31_u64) "
      "print(\"${signed8}/${unsigned8}/${signed16}/${unsigned16}/"
      "${signed32}/${unsigned32}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0);
  CHECK(count_bytes(artifact, emitted.written.mlir_bytes,
                    "integer_materialized_mask_") >= 6u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "integer_materialized_sign_fill_") >= 3u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.select %integer_materialized_negative_") == 3u &&
        contains_bytes(artifact, emitted.written.mlir_bytes, "llvm.add ") &&
        contains_bytes(artifact, emitted.written.mlir_bytes, "llvm.sub ") &&
        contains_bytes(artifact, emitted.written.mlir_bytes, "llvm.mul ") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.call @w_seed_wrapping_shift_left_integer") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.call @w_seed_wrapping_power_integer"));

  static const uint8_t non_entry_source[] =
      "fn keep(value: i8): i8 { return value }\n"
      "fn display(value: i8) { print(\"${value}\") }\n"
      "entry { let retained: i8 = keep(value: -1_i8) "
      "display(value: retained) }\n";
  CHECK(lower_hir(non_entry_source, sizeof(non_entry_source) - 1u));
  CHECK(fixture.hir_program.function_count == 3u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  const size_t display_start =
      find_bytes(artifact, emitted.written.mlir_bytes,
                 "llvm.func internal @w_fn_1", 0u);
  const size_t entry_start =
      find_bytes(artifact, emitted.written.mlir_bytes,
                 "llvm.func internal @w_fn_2", display_start);
  CHECK(display_start != SIZE_MAX && entry_start != SIZE_MAX &&
        contains_bytes(artifact + display_start, entry_start - display_start,
                       "llvm.and %p0, %integer_materialized_mask_"));
  return true;
}

static bool test_interpolation_semantic_barriers(void) {
  static const uint8_t division_by_zero[] =
      "fn main() { print(\"${8 / 0}\") }\nentry(main)\n";
  CHECK(lower_hir(division_by_zero, sizeof(division_by_zero) - 1u));
  CHECK(expect_sequence_unsupported());

  static const uint8_t remainder_by_zero[] =
      "fn main() { print(\"${8 % 0}\") }\nentry(main)\n";
  CHECK(lower_hir(remainder_by_zero, sizeof(remainder_by_zero) - 1u));
  CHECK(expect_sequence_unsupported());

  static const uint8_t minimum_divided_by_negative_one[] =
      "fn main() { print(\"${(0 - 9223372036854775807 - 1) / (0 - 1)}\") }\n"
      "entry(main)\n";
  CHECK(lower_hir(minimum_divided_by_negative_one,
                  sizeof(minimum_divided_by_negative_one) - 1u));
  CHECK(expect_sequence_unsupported());

  static const uint8_t safe_constants[] =
      "fn main() { print(\"${8 / 2} ${8 % 3}\") }\nentry(main)\n";
  CHECK(lower_hir(safe_constants, sizeof(safe_constants) - 1u));
  w_seed_mlir0_result safe_constants_result;
  uint8_t safe_constants_artifact[W_SEED_MLIR0_MAX_BYTES];
  CHECK(emit_current(safe_constants_artifact, sizeof(safe_constants_artifact),
                     &safe_constants_result));
  CHECK(contains_bytes(safe_constants_artifact,
                        safe_constants_result.written.mlir_bytes,
                        "llvm.sdiv %v") &&
        contains_bytes(safe_constants_artifact,
                        safe_constants_result.written.mlir_bytes,
                        "llvm.srem %v") &&
        !contains_bytes(safe_constants_artifact,
                        safe_constants_result.written.mlir_bytes,
                        "@w_seed_checked_divide_i64") &&
        !contains_bytes(safe_constants_artifact,
                        safe_constants_result.written.mlir_bytes,
                        "@w_seed_checked_remainder_i64"));

  static const uint8_t overflow[] =
      "fn main() { print(\"${9223372036854775807 * 2}\") }\nentry(main)\n";
  CHECK(lower_hir(overflow, sizeof(overflow) - 1u));
  CHECK(expect_sequence_unsupported());

  static const uint8_t runtime_division[] =
      "fn divide(value: i64): i64 { return value / 2 }\n"
      "fn main() { let result = divide(value: 5) "
      "print(\"${result}\") }\nentry(main)\n";
  CHECK(lower_hir(runtime_division, sizeof(runtime_division) - 1u));
  w_seed_mlir0_result runtime_division_result;
  uint8_t runtime_division_artifact[W_SEED_MLIR0_MAX_BYTES];
  CHECK(emit_current(runtime_division_artifact,
                     sizeof(runtime_division_artifact),
                     &runtime_division_result));
  CHECK(contains_bytes(runtime_division_artifact,
                        runtime_division_result.written.mlir_bytes,
                        "llvm.sdiv ") &&
        !contains_bytes(runtime_division_artifact,
                         runtime_division_result.written.mlir_bytes,
                         "@w_seed_checked_divide_i64"));

  static const uint8_t runtime_remainder[] =
      "fn remainder(value: i64): i64 { return value % 2 }\n"
      "fn main() { let result = remainder(value: 5) "
      "print(\"${result}\") }\nentry(main)\n";
  CHECK(lower_hir(runtime_remainder, sizeof(runtime_remainder) - 1u));
  w_seed_mlir0_result runtime_remainder_result;
  uint8_t runtime_remainder_artifact[W_SEED_MLIR0_MAX_BYTES];
  CHECK(emit_current(runtime_remainder_artifact,
                     sizeof(runtime_remainder_artifact),
                     &runtime_remainder_result));
  CHECK(contains_bytes(runtime_remainder_artifact,
                        runtime_remainder_result.written.mlir_bytes,
                        "llvm.srem %p0, %v") &&
        !contains_bytes(runtime_remainder_artifact,
                        runtime_remainder_result.written.mlir_bytes,
                        "@w_seed_checked_remainder_i64"));

  return true;
}

static bool test_checked_division_remainder_lowering(void) {
  static const uint8_t source[] =
      "fn signed_divide(left: i64, right: i64): i64 { return left / right }\n"
      "fn signed_remainder(left: i64, right: i64): i64 { return left % right }\n"
      "fn unsigned_divide(left: u64, right: u64): u64 { return left / right }\n"
      "fn unsigned_remainder(left: u64, right: u64): u64 { return left % right }\n"
      "fn minimum_remainder(): i64 { return (0 - 9223372036854775807 - 1) % "
      "(0 - 1) }\n"
      "fn main() { let signed_q = signed_divide(left: 5, right: 2) "
      "let signed_r = signed_remainder(left: 5, right: 2) "
      "let unsigned_q = unsigned_divide(left: 5_u64, right: 2_u64) "
      "let unsigned_r = unsigned_remainder(left: 5_u64, right: 2_u64) "
      "let minimum_r = minimum_remainder() "
      "print(\"${signed_q} ${signed_r} ${unsigned_q} ${unsigned_r} "
      "${minimum_r}\") }\n"
      "entry(main)\n";
  CHECK(lower_hir(source, sizeof(source) - 1u));
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_result result;
  CHECK(emit_current(artifact, sizeof(artifact), &result));
  const size_t length = result.written.mlir_bytes;
  CHECK(contains_bytes(artifact, length,
                      "llvm.func internal @w_seed_checked_divide_i64("
                      "%left: i64, %right: i64, %width: i64) -> i64") &&
        contains_bytes(artifact, length,
                      "llvm.func internal @w_seed_checked_remainder_i64("
                      "%left: i64, %right: i64, %width: i64) -> i64") &&
        contains_bytes(artifact, length,
                      "llvm.func internal @w_seed_checked_divide_u64("
                      "%left: i64, %right: i64, %width: i64) -> i64") &&
        contains_bytes(artifact, length,
                      "llvm.func internal @w_seed_checked_remainder_u64("
                      "%left: i64, %right: i64, %width: i64) -> i64") &&
        contains_bytes(artifact, length,
                      "llvm.call @w_seed_checked_divide_i64(") &&
        contains_bytes(artifact, length,
                      "llvm.call @w_seed_checked_remainder_i64(") &&
        contains_bytes(artifact, length,
                      "llvm.call @w_seed_checked_divide_u64(") &&
        contains_bytes(artifact, length,
                      "llvm.call @w_seed_checked_remainder_u64(") &&
        contains_bytes(artifact, length,
                      ") : (i64, i64, i64) -> i64"));

  const size_t signed_divide = find_bytes(
      artifact, length,
      "llvm.func internal @w_seed_checked_divide_i64(", 0u);
  const size_t signed_divide_guard = find_bytes(
      artifact, length,
      "llvm.cond_br %invalid, ^checked_fault, ^checked_ok", signed_divide);
  const size_t signed_divide_fault = find_bytes(
      artifact, length,
      "^checked_fault:\n    \"llvm.intr.trap\"() : () -> ()",
      signed_divide);
  const size_t signed_divide_operation = find_bytes(
      artifact, length, "llvm.sdiv %left, %right : i64", signed_divide);
  CHECK(signed_divide != SIZE_MAX && signed_divide_guard > signed_divide &&
        signed_divide_fault > signed_divide_guard &&
        signed_divide_operation > signed_divide_fault &&
        contains_bytes(artifact, length,
                      "llvm.icmp \"eq\" %left, %minimum : i64") &&
        contains_bytes(artifact, length,
                      "llvm.icmp \"eq\" %right, %negative_one : i64") &&
        contains_bytes(artifact, length,
                      "%minimum = llvm.ashr %minimum_i64, %minimum_shift : i64"));

  const size_t signed_remainder = find_bytes(
      artifact, length,
      "llvm.func internal @w_seed_checked_remainder_i64(", 0u);
  const size_t signed_remainder_zero_guard = find_bytes(
      artifact, length,
      "llvm.cond_br %zero_divisor, ^checked_fault, ^checked_nonzero",
      signed_remainder);
  const size_t signed_remainder_fault = find_bytes(
      artifact, length,
      "^checked_fault:\n    \"llvm.intr.trap\"() : () -> ()",
      signed_remainder);
  const size_t signed_remainder_minimum_branch = find_bytes(
      artifact, length,
      "llvm.cond_br %overflow_pair, ^checked_minimum, ^checked_ok",
      signed_remainder);
  const size_t signed_remainder_minimum_result = find_bytes(
      artifact, length,
      "^checked_minimum:\n    llvm.return %zero : i64", signed_remainder);
  const size_t signed_remainder_operation = find_bytes(
      artifact, length, "llvm.srem %left, %right : i64", signed_remainder);
  CHECK(signed_remainder != SIZE_MAX &&
        signed_remainder_zero_guard > signed_remainder &&
        signed_remainder_fault > signed_remainder_zero_guard &&
        signed_remainder_minimum_branch > signed_remainder_fault &&
        signed_remainder_minimum_result > signed_remainder_minimum_branch &&
        signed_remainder_operation > signed_remainder_minimum_result &&
        !contains_bytes(artifact, length, "llvm.srem %v"));

  const size_t unsigned_divide = find_bytes(
      artifact, length,
      "llvm.func internal @w_seed_checked_divide_u64(", 0u);
  const size_t unsigned_divide_guard = find_bytes(
      artifact, length,
      "llvm.cond_br %zero_divisor, ^u64_checked_fault, ^u64_checked_ok",
      unsigned_divide);
  const size_t unsigned_divide_fault = find_bytes(
      artifact, length,
      "^u64_checked_fault:\n    \"llvm.intr.trap\"() : () -> ()",
      unsigned_divide);
  const size_t unsigned_divide_operation = find_bytes(
      artifact, length, "llvm.udiv %left, %right : i64", unsigned_divide);
  CHECK(unsigned_divide != SIZE_MAX &&
        unsigned_divide_guard > unsigned_divide &&
        unsigned_divide_fault > unsigned_divide_guard &&
        unsigned_divide_operation > unsigned_divide_fault);

  const size_t unsigned_remainder = find_bytes(
      artifact, length,
      "llvm.func internal @w_seed_checked_remainder_u64(", 0u);
  const size_t unsigned_remainder_guard = find_bytes(
      artifact, length,
      "llvm.cond_br %zero_divisor, ^u64_checked_fault, ^u64_checked_ok",
      unsigned_remainder);
  const size_t unsigned_remainder_fault = find_bytes(
      artifact, length,
      "^u64_checked_fault:\n    \"llvm.intr.trap\"() : () -> ()",
      unsigned_remainder);
  const size_t unsigned_remainder_operation = find_bytes(
      artifact, length, "llvm.urem %left, %right : i64", unsigned_remainder);
  CHECK(unsigned_remainder != SIZE_MAX &&
        unsigned_remainder_guard > unsigned_remainder &&
        unsigned_remainder_fault > unsigned_remainder_guard &&
        unsigned_remainder_operation > unsigned_remainder_fault);
  return true;
}

static bool test_direct_unit_call(void) {
  static const uint8_t source[] =
      "fn announce(table: i64, isOpen: Bool) {\n"
      "  print(\"Table ${table}; open: ${isOpen}\")\n"
      "}\n"
      "fn main() {\n"
      "  announce(isOpen: true, table: 6 * 7)\n"
      "}\n"
      "entry(main)\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(fixture.hir_program.function_count == 2u &&
        fixture.hir_program.parameter_count == 2u &&
        fixture.hir_program.call_count == 2u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes);
  CHECK(contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.func internal @w_fn_0"));
  CHECK(contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.func internal @w_fn_1"));
  CHECK(contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.call @w_fn_0(%buffer, %cursor_address, %v6, %v3)"));
  CHECK(contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.call @w_seed_checked_multiply_i64(%v4, %v5, "
                       "%v6_checked_width) : (i64, i64, i64) -> i64"));
  CHECK(!contains_bytes(artifact, emitted.written.mlir_bytes, "llvm.add %v") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes, "llvm.sub %v") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes, "llvm.mul %v"));
  CHECK(contains_bytes(artifact, emitted.written.mlir_bytes,
                       "@w_seed_append_i64(%buffer, %cursor0_1, %p0)"));
  CHECK(contains_bytes(artifact, emitted.written.mlir_bytes,
                       "@w_seed_append_bool(%buffer, %cursor0_3, %p1)"));
  return true;
}

static bool test_checked_runtime_arithmetic(void) {
  static const uint8_t source[] =
      "fn serve(isOpen: Bool, guests: i64): i64 { "
      "return if isOpen { guests + 1 } else { guests - 1 } }\n"
      "fn scale(value: i64): i64 { return value * 2 }\n"
      "fn main() { let open = serve(isOpen: true, guests: 5) "
      "let closed = serve(isOpen: false, guests: 2) "
      "let doubled = scale(value: 3) "
      "print(\"Open ${open}; closed ${closed}; doubled ${doubled}\") }\n"
      "entry(main)\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(fixture.hir_program.function_count == 3u &&
        fixture.hir_program.parameter_count == 3u);
  const w_seed_mlir0_input input = mlir_input();
  const w_seed_mlir0_target *targets[] = {&TARGET, &WINDOWS_TARGET};
  for (size_t target = 0u; target < sizeof(targets) / sizeof(targets[0]);
       target += 1u) {
    w_seed_mlir0_counts counts;
    w_seed_mlir0_result measured;
    CHECK(w_seed_mlir0_measure(&input, targets[target], &counts, &measured) ==
          W_SEED_MLIR0_OK);
    w_seed_mlir0_result emitted;
    CHECK(w_seed_mlir0_emit(
              &input, targets[target],
              &(w_seed_mlir0_output){artifact, sizeof(artifact)}, &emitted) ==
          W_SEED_MLIR0_OK);
    CHECK(emitted.written.mlir_bytes == counts.mlir_bytes &&
          memcmp(emitted.mlir_sha256, measured.mlir_sha256,
                 sizeof(emitted.mlir_sha256)) == 0);
    CHECK(count_bytes(artifact, counts.mlir_bytes,
                      "llvm.call @w_seed_checked_add_i64") == 1u &&
          count_bytes(artifact, counts.mlir_bytes,
                      "llvm.call @w_seed_checked_subtract_i64") == 1u &&
          count_bytes(artifact, counts.mlir_bytes,
                      "llvm.call @w_seed_checked_multiply_i64") == 1u);
    CHECK(contains_bytes(artifact, counts.mlir_bytes,
                         "llvm.intr.sadd.with.overflow") &&
          contains_bytes(artifact, counts.mlir_bytes,
                         "llvm.intr.ssub.with.overflow") &&
          contains_bytes(artifact, counts.mlir_bytes,
                         "llvm.intr.smul.with.overflow"));
    CHECK(count_bytes(artifact, counts.mlir_bytes,
                      "llvm.cond_br %invalid, ^checked_overflow, ^checked_ok") ==
              3u &&
          count_bytes(artifact, counts.mlir_bytes,
                      "\"llvm.intr.trap\"() : () -> ()") == 3u);
    CHECK(!contains_bytes(artifact, counts.mlir_bytes, "llvm.add %v") &&
          !contains_bytes(artifact, counts.mlir_bytes, "llvm.sub %v") &&
          !contains_bytes(artifact, counts.mlir_bytes, "llvm.mul %v") &&
          !contains_bytes(artifact, counts.mlir_bytes, "llvm.sdiv %v") &&
          !contains_bytes(artifact, counts.mlir_bytes, "llvm.srem %v"));
  }
  return true;
}

static bool test_checked_integer_width_matrix(void) {
  typedef struct {
    const char *name;
    const char *type;
    const char *suffix;
  } integer_case;
  static const integer_case INTEGERS[] = {
      {"i8", "i8", "i8"},       {"u8", "u8", "u8"},
      {"i16", "i16", "i16"},    {"u16", "u16", "u16"},
      {"i32", "i32", "i32"},    {"u32", "u32", "u32"},
      {"i64", "i64", "i64"},    {"u64", "u64", "u64"},
      {"int_alias", "Int", "i64"},
      {"uint_alias", "UInt", "u64"},
  };
  char source[TEST_SOURCE];
  size_t source_length = 0u;
  (void)memset(source, 0, sizeof(source));
  for (size_t index = 0u; index < sizeof(INTEGERS) / sizeof(INTEGERS[0]);
       index += 1u) {
    const integer_case *integer = &INTEGERS[index];
    CHECK(append_mlir_test_source(
        source, sizeof(source), &source_length,
        "fn checked_%s(left: %s, right: %s): %s { let sum = left + right "
        "let difference = left - right let product = left * right return sum }\n",
        integer->name, integer->type, integer->type, integer->type));
  }
  CHECK(append_mlir_test_source(source, sizeof(source), &source_length,
                                "entry { "));
  for (size_t index = 0u; index < sizeof(INTEGERS) / sizeof(INTEGERS[0]);
       index += 1u) {
    const integer_case *integer = &INTEGERS[index];
    CHECK(append_mlir_test_source(
        source, sizeof(source), &source_length,
        "let use_%s = checked_%s(left: 6_%s, right: 3_%s) ",
        integer->name, integer->name, integer->suffix, integer->suffix));
  }
  CHECK(append_mlir_test_source(source, sizeof(source), &source_length,
                                "print(\"checked\") }\n"));
  CHECK(lower_hir((const uint8_t *)source, source_length));

  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  const w_seed_mlir0_target *targets[] = {&TARGET, &WINDOWS_TARGET};
  for (size_t target_index = 0u;
       target_index < sizeof(targets) / sizeof(targets[0]);
       target_index += 1u) {
    const w_seed_mlir0_input input = mlir_input();
    w_seed_mlir0_counts counts;
    w_seed_mlir0_result measured;
    CHECK(w_seed_mlir0_measure(&input, targets[target_index], &counts,
                               &measured) == W_SEED_MLIR0_OK);
    w_seed_mlir0_result emitted;
    CHECK(w_seed_mlir0_emit(
              &input, targets[target_index],
              &(w_seed_mlir0_output){artifact, sizeof(artifact)}, &emitted) ==
          W_SEED_MLIR0_OK);
    CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
          memcmp(emitted.mlir_sha256, measured.mlir_sha256,
                 sizeof(emitted.mlir_sha256)) == 0);
    CHECK(count_bytes(artifact, counts.mlir_bytes,
                      "llvm.call @w_seed_checked_add_i64(") == 5u &&
          count_bytes(artifact, counts.mlir_bytes,
                      "llvm.call @w_seed_checked_subtract_i64(") == 5u &&
          count_bytes(artifact, counts.mlir_bytes,
                      "llvm.call @w_seed_checked_multiply_i64(") == 5u &&
          count_bytes(artifact, counts.mlir_bytes,
                      "llvm.call @w_seed_checked_add_u64(") == 5u &&
          count_bytes(artifact, counts.mlir_bytes,
                      "llvm.call @w_seed_checked_subtract_u64(") == 5u &&
          count_bytes(artifact, counts.mlir_bytes,
                      "llvm.call @w_seed_checked_multiply_u64(") == 5u &&
          count_bytes(artifact, counts.mlir_bytes,
                      ") : (i64, i64, i64) -> i64\n") == 30u);
    CHECK(count_bytes(artifact, counts.mlir_bytes,
                      "_checked_width = llvm.mlir.constant(8 : i64)") == 6u &&
          count_bytes(artifact, counts.mlir_bytes,
                      "_checked_width = llvm.mlir.constant(16 : i64)") ==
              6u &&
          count_bytes(artifact, counts.mlir_bytes,
                      "_checked_width = llvm.mlir.constant(32 : i64)") ==
              6u &&
          count_bytes(artifact, counts.mlir_bytes,
                      "_checked_width = llvm.mlir.constant(64 : i64)") ==
              12u);
    CHECK(contains_bytes(artifact, counts.mlir_bytes,
                         "%width: i64) -> i64") &&
          contains_bytes(artifact, counts.mlir_bytes,
                         "%shift = llvm.sub %width64, %width : i64") &&
          contains_bytes(artifact, counts.mlir_bytes,
                         "%narrow_overflow = llvm.icmp \"ne\" %value, "
                         "%roundtrip : i64") &&
          contains_bytes(artifact, counts.mlir_bytes,
                         "%mask = llvm.lshr %minus_one, %shift : i64") &&
          !contains_bytes(artifact, counts.mlir_bytes, " =     %v"));
  }
  return true;
}

static bool test_checked_helper_reachability(void) {
  static const uint8_t source[] =
      "fn deadArithmetic(value: i64): i64 { return value + 1 }\n"
      "fn deadDivision(value: i64): i64 { return value / 2 }\n"
      "fn deadRemainder(value: i64): i64 { return value % 2 }\n"
      "fn deadNegate(value: i64): i64 { return -value }\n"
      "fn secret() { print(\"secret\") }\n"
      "fn dead() { secret() }\n"
      "fn main() { print(\"Hello, world!\") }\n"
      "entry(main)\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  CHECK(lower_hir(source, sizeof(source) - 1u));
  w_seed_mlir0_result result;
  CHECK(emit_current(artifact, sizeof(artifact), &result));
  CHECK(!contains_bytes(artifact, result.written.mlir_bytes,
                        "@w_seed_checked_add_i64") &&
        !contains_bytes(artifact, result.written.mlir_bytes,
                        "@w_seed_checked_subtract_i64") &&
        !contains_bytes(artifact, result.written.mlir_bytes,
                        "@w_seed_checked_multiply_i64") &&
        !contains_bytes(artifact, result.written.mlir_bytes,
                        "@w_seed_checked_divide_i64") &&
        !contains_bytes(artifact, result.written.mlir_bytes,
                        "@w_seed_checked_remainder_i64") &&
        !contains_bytes(artifact, result.written.mlir_bytes, "@w_fn_0(") &&
        !contains_bytes(artifact, result.written.mlir_bytes, "@w_fn_1(") &&
        !contains_bytes(artifact, result.written.mlir_bytes, "@w_fn_2(") &&
        !contains_bytes(artifact, result.written.mlir_bytes, "@w_fn_3(") &&
        !contains_bytes(artifact, result.written.mlir_bytes, "@w_fn_4(") &&
        !contains_bytes(artifact, result.written.mlir_bytes, "@w_fn_5(") &&
        !contains_bytes(artifact, result.written.mlir_bytes,
                        "\\73\\65\\63\\72\\65\\74") &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "\\48\\65\\6c\\6c\\6f"));
  return true;
}

static bool test_scalar_return_call_result(void) {
  static const uint8_t source[] =
      "fn tableNumber(): i64 { return 6 * 7 }\n"
      "fn main() { let table = tableNumber() "
      "print(\"Table ${table}\") }\n"
      "entry(main)\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(fixture.hir_program.function_count == 2u &&
        fixture.hir_program.calls[0].result_type == 2u &&
        fixture.hir_program.terminators[0].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_VALUE);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes);
  CHECK(contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.func internal @w_fn_0(%buffer: !llvm.ptr, "
                       "%cursor_address: !llvm.ptr) -> i64"));
  CHECK(contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.call @w_seed_checked_multiply_i64"));
  CHECK(!contains_bytes(artifact, emitted.written.mlir_bytes, "llvm.add %v") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes, "llvm.sub %v") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes, "llvm.mul %v"));
  CHECK(contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.return %v"));
  CHECK(contains_bytes(artifact, emitted.written.mlir_bytes,
                       "%call0 = llvm.call @w_fn_0(%buffer, "
                       "%cursor_address) : (!llvm.ptr, !llvm.ptr) -> i64"));
  CHECK(contains_bytes(artifact, emitted.written.mlir_bytes,
                       "@w_seed_append_i64(%buffer, %cursor1_1, %call0)"));
  return true;
}

static bool test_unsigned_scalar_return_call_result(void) {
  static const uint8_t source[] =
      "fn identity(value: UInt): UInt { return value }\n"
      "entry { let value = identity(value: 18446744073709551615_u64) "
      "print(\"Unsigned ${value}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.mlir.constant(-1 : i64) : i64") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.func internal @w_seed_append_u64") &&
        contains_bytes(artifact, emitted.written.mlir_bytes, "llvm.udiv") &&
        contains_bytes(artifact, emitted.written.mlir_bytes, "llvm.urem") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "%call0 = llvm.call @w_fn_0") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.call @w_seed_append_u64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.call @w_seed_append_i64"));
  return true;
}

static bool test_unsigned_binary_u64_slice(void) {
  static const uint8_t source[] =
      "fn arithmetic(left: UInt, right: UInt): UInt { return (((left + right) "
      "- right) * right) / right % right }\n"
      "fn equal(value: UInt): Bool { return value == 9223372036854775808_u64 }\n"
      "fn notEqual(value: UInt): Bool { return value != 9223372036854775807_u64 }\n"
      "fn less(value: UInt): Bool { return value < 18446744073709551615_u64 }\n"
      "fn lessEqual(value: UInt): Bool { return value <= 9223372036854775808_u64 }\n"
      "fn greater(value: UInt): Bool { return value > 9223372036854775807_u64 }\n"
      "fn greaterEqual(value: UInt): Bool { return value >= 9223372036854775808_u64 }\n"
      "entry { let high = 9223372036854775808_u64 "
      "let maximum = 18446744073709551615_u64 "
      "let calculated = arithmetic(left: 23_u64, right: 3_u64) "
      "let eq = equal(value: high) "
      "let ne = notEqual(value: high) "
      "let lt = less(value: high) "
      "let le = lessEqual(value: high) "
      "let gt = greater(value: maximum) "
      "let ge = greaterEqual(value: high) "
      "print(\"${calculated} "
      "${eq} ${ne} ${lt} ${le} ${gt} ${ge}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  size_t binary_u64_count = 0u;
  size_t integer_comparison_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u) {
    if (fixture.hir_program.values[index].kind ==
        W_SEED_HIR0_VALUE_BINARY_U64)
      binary_u64_count += 1u;
    if (fixture.hir_program.values[index].kind ==
        W_SEED_HIR0_VALUE_BINARY_INTEGER_COMPARISON)
      integer_comparison_count += 1u;
  }
  CHECK(binary_u64_count == 5u && integer_comparison_count == 6u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0);
  CHECK(contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.mlir.constant(-9223372036854775808 : i64) : i64") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.mlir.constant(-1 : i64) : i64"));
  CHECK(count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.call @w_seed_checked_add_u64") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.call @w_seed_checked_subtract_u64") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.call @w_seed_checked_multiply_u64") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.call @w_seed_checked_divide_u64") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.call @w_seed_checked_remainder_u64") == 1u);
  CHECK(contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.intr.uadd.with.overflow") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.intr.usub.with.overflow") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.intr.umul.with.overflow") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.udiv %left, %right : i64") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.urem %left, %right : i64"));
  CHECK(contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.icmp \"eq\"") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.icmp \"ne\"") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.icmp \"ult\"") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.icmp \"ule\"") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.icmp \"ugt\"") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.icmp \"uge\""));
  CHECK(!contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_checked_add_i64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_checked_subtract_i64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_checked_multiply_i64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_checked_divide_i64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_checked_remainder_i64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.intr.sadd.with.overflow") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.intr.ssub.with.overflow") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.intr.smul.with.overflow") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.icmp \"slt\"") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.icmp \"sle\"") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.icmp \"sgt\"") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.icmp \"sge\"") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_append_i64"));

  static const uint8_t reachability_source[] =
      "fn dead(value: UInt): UInt { return value + 1_u64 }\n"
      "entry { print(\"${18446744073709551615_u64 == "
      "18446744073709551615_u64}\") }\n";
  CHECK(lower_hir(reachability_source, sizeof(reachability_source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes);
  CHECK(contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.icmp \"eq\""));
  CHECK(!contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_checked_add_u64"));
  CHECK(!contains_bytes(artifact, emitted.written.mlir_bytes, "@w_fn_0("));
  CHECK(!contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_append_i64"));
  return true;
}

static bool test_u64_wrapping_add_artifact(void) {
  static const uint8_t source[] =
      "fn wrap(value: UInt): UInt { return u64.wrappingAdd(value, 1_u64) }\n"
      "entry { let result = wrap(value: 18446744073709551615_u64) "
      "print(\"${result}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  size_t wrapping_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u)
    if (fixture.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        fixture.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_WRAPPING_ADD)
      wrapping_count += 1u;
  CHECK(wrapping_count == 1u && fixture.hir_program.call_count == 2u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0);
  const size_t wrapper_start =
      find_bytes(artifact, emitted.written.mlir_bytes,
                 "llvm.func internal @w_fn_0", 0u);
  const size_t entry_start =
      find_bytes(artifact, emitted.written.mlir_bytes,
                 "llvm.func internal @w_fn_1", wrapper_start);
  CHECK(wrapper_start != SIZE_MAX && entry_start != SIZE_MAX &&
        count_bytes(artifact + wrapper_start, entry_start - wrapper_start,
                    "llvm.add ") == 1u &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.call @w_seed_checked_add_u64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.intr.uadd.with.overflow") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.call @w_seed_append_u64"));
  return true;
}

static bool test_u64_saturating_add_artifact(void) {
  static const uint8_t source[] =
      "fn clamp(left: UInt, right: UInt): UInt { return "
      "u64.saturatingAdd(left, right) }\n"
      "entry { let maximum = clamp(left: 18446744073709551615_u64, "
      "right: 1_u64) let ordinary = clamp(left: 7_u64, right: 5_u64) "
      "print(\"${maximum}/${ordinary}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  size_t saturating_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u)
    if (fixture.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        fixture.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_SATURATING_ADD)
      saturating_count += 1u;
  CHECK(saturating_count == 1u && fixture.hir_program.call_count == 3u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0 &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.intr.uadd.sat") == 1u &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_checked_add_u64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "\"llvm.intr.trap\"() : () -> ()"));

  static const uint8_t unreachable_source[] =
      "entry { print(\"No saturating add\") }\n";
  CHECK(lower_hir(unreachable_source, sizeof(unreachable_source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.intr.uadd.sat"));
  return true;
}

static bool test_u64_overflowing_products_artifact(void) {
  static const uint8_t source[] =
      "entry { "
      "let added = u64.overflowingAdd(18446744073709551615_u64, 1_u64) "
      "let subtracted = u64.overflowingSubtract(0_u64, 1_u64) "
      "let multiplied = u64.overflowingMultiply(18446744073709551615_u64, 2_u64) "
      "let negated = u64.overflowingNegate(1_u64) "
      "let addedValue = added.0 let addedOverflow = added.1 "
      "let subtractedValue = subtracted.0 let subtractedOverflow = subtracted.1 "
      "let multipliedValue = multiplied.0 let multipliedOverflow = multiplied.1 "
      "let negatedValue = negated.0 let negatedOverflow = negated.1 "
      "print(\"${addedValue}/${addedOverflow}/${subtractedValue}/${subtractedOverflow}/${multipliedValue}/${multipliedOverflow}/${negatedValue}/${negatedOverflow}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0 &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.intr.uadd.with.overflow") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.intr.usub.with.overflow") == 2u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.intr.umul.with.overflow") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.extractvalue") == 8u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "!llvm.struct<(i64, i1)>") == 12u &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.call @w_seed_append_u64") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.call @w_seed_append_bool") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_checked_add_u64"));
  return true;
}

static bool test_u64_saturating_subtract_artifact(void) {
  static const uint8_t source[] =
      "fn clamp(left: UInt, right: UInt): UInt { return "
      "u64.saturatingSubtract(left, right) }\n"
      "entry { let zero = clamp(left: 0_u64, right: 1_u64) "
      "let ordinary = clamp(left: 12_u64, right: 5_u64) "
      "print(\"${zero}/${ordinary}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  size_t saturating_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u)
    if (fixture.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        fixture.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_SATURATING_SUBTRACT)
      saturating_count += 1u;
  CHECK(saturating_count == 1u && fixture.hir_program.call_count == 3u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0 &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.intr.usub.sat") == 1u &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_checked_subtract_u64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "\"llvm.intr.trap\"() : () -> ()"));

  static const uint8_t unreachable_source[] =
      "entry { print(\"No saturating subtract\") }\n";
  CHECK(lower_hir(unreachable_source, sizeof(unreachable_source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.intr.usub.sat"));
  return true;
}

static bool test_u64_saturating_multiply_artifact(void) {
  static const uint8_t source[] =
      "fn clamp(left: UInt, right: UInt): UInt { return "
      "u64.saturatingMultiply(left, right) }\n"
      "entry { let maximum = clamp(left: 18446744073709551615_u64, "
      "right: 2_u64) let ordinary = clamp(left: 6_u64, right: 7_u64) "
      "print(\"${maximum}/${ordinary}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  size_t saturating_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u)
    if (fixture.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        fixture.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_SATURATING_MULTIPLY)
      saturating_count += 1u;
  CHECK(saturating_count == 1u && fixture.hir_program.call_count == 3u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0 &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.intr.umul.with.overflow") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.extractvalue") == 2u &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.select") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.mlir.constant(-1 : i64)") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_checked_multiply_u64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "\"llvm.intr.umul.sat\"") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "\"llvm.intr.trap\"() : () -> ()"));

  static const uint8_t unreachable_source[] =
      "entry { print(\"No saturating multiply\") }\n";
  CHECK(lower_hir(unreachable_source, sizeof(unreachable_source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.intr.umul.with.overflow"));
  return true;
}

static bool test_u64_saturating_policy_artifact(void) {
  static const uint8_t source[] =
      "fn saturatingNegate(value: UInt): UInt { return "
      "u64.saturatingNegate(value) }\n"
      "fn saturatingPower(base: UInt, exponent: UInt): UInt { return "
      "u64.saturatingPower(base, exponent) }\n"
      "entry { let negZero = saturatingNegate(value: 0_u64) "
      "let negMaximum = saturatingNegate(value: 18446744073709551615_u64) "
      "let ordinary = saturatingPower(base: 2_u64, exponent: 3_u64) "
      "let clamped = saturatingPower(base: 2_u64, exponent: 64_u64) "
      "let zeroPowerZero = saturatingPower(base: 0_u64, exponent: 0_u64) "
      "print(\"\x24{negZero}/\x24{negMaximum}/\x24{ordinary}/\x24{clamped}/\x24{zeroPowerZero}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  size_t negate_count = 0u;
  size_t power_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u) {
    const w_seed_hir0_value *value = &fixture.hir_program.values[index];
    if (value->kind == W_SEED_HIR0_VALUE_UNARY_U64 &&
        value->unary_operator == W_SEED_HIR0_UNARY_SATURATING_NEGATE)
      negate_count += 1u;
    if (value->kind == W_SEED_HIR0_VALUE_BINARY_U64 &&
        value->binary_operator == W_SEED_HIR0_BINARY_SATURATING_POWER)
      power_count += 1u;
  }
  CHECK(negate_count == 1u && power_count == 1u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0 &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.intr.usub.sat") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.func internal @w_seed_saturating_power_u64") ==
            1u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.call @w_seed_saturating_power_u64") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.intr.umul.with.overflow") == 2u &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.cond_br %last, ^saturating_power_done") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.select %acc_overflow, %max") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_checked_power_u64"));

  static const uint8_t unreachable_source[] =
      "entry { print(\"Hello\") }\n";
  CHECK(lower_hir(unreachable_source, sizeof(unreachable_source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_saturating_power_u64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.intr.usub.sat"));
  return true;
}

static bool test_u64_wrapping_subtract_artifact(void) {
  static const uint8_t source[] =
      "fn wrap(value: UInt): UInt { return u64.wrappingSubtract(value, 1_u64) }\n"
      "entry { let result = wrap(value: 0_u64) print(\"${result}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  size_t wrapping_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u)
    if (fixture.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        fixture.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_WRAPPING_SUBTRACT)
      wrapping_count += 1u;
  CHECK(wrapping_count == 1u && fixture.hir_program.call_count == 2u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0);
  const size_t wrapper_start =
      find_bytes(artifact, emitted.written.mlir_bytes,
                 "llvm.func internal @w_fn_0", 0u);
  const size_t entry_start =
      find_bytes(artifact, emitted.written.mlir_bytes,
                 "llvm.func internal @w_fn_1", wrapper_start);
  CHECK(wrapper_start != SIZE_MAX && entry_start != SIZE_MAX &&
        count_bytes(artifact + wrapper_start, entry_start - wrapper_start,
                    "llvm.sub ") == 1u &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.call @w_seed_checked_subtract_u64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.intr.usub.with.overflow") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.call @w_seed_append_u64"));
  return true;
}

static bool test_u64_wrapping_multiply_artifact(void) {
  static const uint8_t source[] =
      "fn wrap(value: UInt): UInt { return u64.wrappingMultiply(value, 2_u64) }\n"
      "entry { let result = wrap(value: 18446744073709551615_u64) "
      "print(\"${result}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  size_t wrapping_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u)
    if (fixture.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        fixture.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_WRAPPING_MULTIPLY)
      wrapping_count += 1u;
  CHECK(wrapping_count == 1u && fixture.hir_program.call_count == 2u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0);
  const size_t wrapper_start =
      find_bytes(artifact, emitted.written.mlir_bytes,
                 "llvm.func internal @w_fn_0", 0u);
  const size_t entry_start =
      find_bytes(artifact, emitted.written.mlir_bytes,
                 "llvm.func internal @w_fn_1", wrapper_start);
  CHECK(wrapper_start != SIZE_MAX && entry_start != SIZE_MAX &&
        count_bytes(artifact + wrapper_start, entry_start - wrapper_start,
                    "llvm.mul ") == 1u &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.call @w_seed_checked_multiply_u64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.intr.umul.with.overflow") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.call @w_seed_append_u64"));
  return true;
}

static bool test_u64_wrapping_negate_artifact(void) {
  static const uint8_t source[] =
      "fn wrap(value: UInt): UInt { return u64.wrappingNegate(value) }\n"
      "entry { let result = wrap(value: 1_u64) print(\"${result}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  size_t wrapping_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u)
    if (fixture.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_UNARY_U64 &&
        fixture.hir_program.values[index].unary_operator ==
            W_SEED_HIR0_UNARY_WRAPPING_NEGATE)
      wrapping_count += 1u;
  CHECK(wrapping_count == 1u && fixture.hir_program.call_count == 2u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0);
  const size_t wrapper_start =
      find_bytes(artifact, emitted.written.mlir_bytes,
                 "llvm.func internal @w_fn_0", 0u);
  const size_t entry_start =
      find_bytes(artifact, emitted.written.mlir_bytes,
                 "llvm.func internal @w_fn_1", wrapper_start);
  CHECK(wrapper_start != SIZE_MAX && entry_start != SIZE_MAX &&
        count_bytes(artifact + wrapper_start, entry_start - wrapper_start,
                    "_wrapping_negate_zero = llvm.mlir.constant(0 : i64)") ==
            1u &&
        count_bytes(artifact + wrapper_start, entry_start - wrapper_start,
                    " = llvm.sub ") == 1u &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.call @w_seed_checked_subtract_u64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.intr.usub.with.overflow") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.call @w_seed_append_u64"));
  return true;
}

static bool test_u64_wrapping_power_artifact(void) {
  static const uint8_t source[] =
      "fn power(base: UInt, exponent: UInt): UInt { "
      "return u64.wrappingPower(base, exponent) }\n"
      "entry { let large = power(base: 3_u64, exponent: 40_u64) "
      "let zero = power(base: 999_u64, exponent: 0_u64) "
      "print(\"${large}/${zero}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  size_t wrapping_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u)
    if (fixture.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        fixture.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_WRAPPING_POWER)
      wrapping_count += 1u;
  CHECK(wrapping_count == 1u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0 &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.func internal @w_seed_wrapping_power_u64") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.call @w_seed_wrapping_power_u64") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.mul ") == 2u &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.lshr %remaining, %one : i64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_checked_power_u64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.intr.umul.with.overflow"));
  return true;
}

static bool test_u64_overflowing_power_artifact(void) {
  static const uint8_t source[] =
      "entry { "
      "let ordinary = u64.overflowingPower(2_u64, 3_u64) "
      "let overflow = u64.overflowingPower(2_u64, 64_u64) "
      "let zero = u64.overflowingPower(0_u64, 0_u64) "
      "let ordinaryValue = ordinary.0 let ordinaryOverflow = ordinary.1 "
      "let overflowValue = overflow.0 let overflowFlag = overflow.1 "
      "let zeroValue = zero.0 let zeroFlag = zero.1 "
      "print(\"\x24{ordinaryValue}/\x24{ordinaryOverflow}/\x24{overflowValue}/"
      "\x24{overflowFlag}/\x24{zeroValue}/\x24{zeroFlag}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  size_t overflowing_power_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u)
    if (fixture.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        fixture.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_OVERFLOWING_POWER)
      overflowing_power_count += 1u;
  CHECK(overflowing_power_count == 3u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0 &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.func internal @w_seed_overflowing_power_u64") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.call @w_seed_overflowing_power_u64") == 3u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "\"llvm.intr.umul.with.overflow\"") == 2u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.extractvalue") >= 10u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "!llvm.struct<(i64, i1)>") >= 10u &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.or %overflowed, %acc_overflow : i1") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.lshr %remaining, %one : i64") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.insertvalue %overflowed_result") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.call @w_seed_append_u64") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.call @w_seed_append_bool") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_checked_power_u64"));
  return true;
}

static bool test_u64_wrapping_shift_left_artifact(void) {
  static const uint8_t source[] =
      "fn shift(value: UInt, count: UInt): UInt { "
      "return u64.wrappingShiftLeft(value, count) }\n"
      "entry { let zero = shift(value: 1_u64, count: 0_u64) "
      "let edge = shift(value: 1_u64, count: 63_u64) "
      "print(\"${zero}/${edge}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  size_t wrapping_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u)
    if (fixture.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        fixture.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_WRAPPING_SHIFT_LEFT)
      wrapping_count += 1u;
  CHECK(wrapping_count == 1u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0 &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.func internal @w_seed_wrapping_shift_left_u64") ==
            1u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.call @w_seed_wrapping_shift_left_u64") == 1u &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.icmp \"uge\" %count, %width : i64") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.shl %left, %count : i64") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "\"llvm.intr.trap\"() : () -> ()") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.unreachable") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_checked_shift_left") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.intr.ushl.with.overflow"));

  static const uint8_t unreachable_source[] =
      "entry { print(\"No wrapping shift\") }\n";
  CHECK(lower_hir(unreachable_source, sizeof(unreachable_source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_wrapping_shift_left_u64"));
  return true;
}

static bool test_u64_masked_shift_left_artifact(void) {
  static const uint8_t source[] =
      "fn shift(value: UInt, count: UInt): UInt { "
      "return u64.maskedShiftLeft(value, count) }\n"
      "entry { let width = shift(value: 7_u64, count: 64_u64) "
      "let next = shift(value: 1_u64, count: 65_u64) "
      "print(\"${width}/${next}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  size_t masked_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u)
    if (fixture.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        fixture.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_MASKED_SHIFT_LEFT)
      masked_count += 1u;
  CHECK(masked_count == 1u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0 &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "@w_seed_masked_shift_left_u64") == 0u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.call @w_seed_masked_shift_left_u64") == 0u &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "_shift_mask = llvm.mlir.constant(63 : i64)") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "_shift_count_mod = llvm.and ") &&
        contains_bytes(artifact, emitted.written.mlir_bytes, "llvm.shl ") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_checked_shift_left") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.intr.ushl.with.overflow"));

  static const uint8_t unreachable_source[] =
      "entry { print(\"No masked shift\") }\n";
  CHECK(lower_hir(unreachable_source, sizeof(unreachable_source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_masked_shift_left_u64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_logical_shift_right_integer"));
  return true;
}

static bool test_u64_masked_shift_right_artifact(void) {
  static const uint8_t source[] =
      "fn shift(value: UInt, count: UInt): UInt { "
      "return u64.maskedShiftRight(value, count) }\n"
      "entry { let width = shift(value: 7_u64, count: 64_u64) "
      "let next = shift(value: 128_u64, count: 65_u64) "
      "print(\"${width}/${next}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  size_t masked_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u)
    if (fixture.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        fixture.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_MASKED_SHIFT_RIGHT)
      masked_count += 1u;
  CHECK(masked_count == 1u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0 &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "@w_seed_masked_shift_right_u64") == 0u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.call @w_seed_masked_shift_right_u64") == 0u &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "_shift_mask = llvm.mlir.constant(63 : i64)") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "_shift_count_mod = llvm.and ") &&
        contains_bytes(artifact, emitted.written.mlir_bytes, "llvm.lshr ") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_checked_shift_right"));

  static const uint8_t unreachable_source[] =
      "entry { print(\"No masked shift right\") }\n";
  CHECK(lower_hir(unreachable_source, sizeof(unreachable_source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_masked_shift_right_u64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_logical_shift_right_integer"));
  return true;
}

static bool test_u64_logical_shift_right_artifact(void) {
  static const uint8_t source[] =
      "fn shift(value: UInt, count: UInt): UInt { "
      "return u64.logicalShiftRight(value, count) }\n"
      "entry { let next = shift(value: 128_u64, count: 1_u64) "
      "let edge = shift(value: 9223372036854775808_u64, count: 63_u64) "
      "print(\"${next}/${edge}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  size_t logical_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u)
    if (fixture.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        fixture.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_LOGICAL_SHIFT_RIGHT)
      logical_count += 1u;
  CHECK(logical_count == 1u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0 &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.func internal @w_seed_logical_shift_right_integer") ==
            1u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.call @w_seed_logical_shift_right_integer") == 1u &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.icmp \"uge\" %count, %width : i64") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.lshr %normalized, %count : i64") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "\"llvm.intr.trap\"() : () -> ()") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.unreachable") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "_shift_count_mod = llvm.and ") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_checked_shift_right"));

  static const uint8_t unreachable_source[] =
      "entry { print(\"No logical shift right\") }\n";
  CHECK(lower_hir(unreachable_source, sizeof(unreachable_source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_logical_shift_right_integer"));
  return true;
}

static bool test_fixed_integer_shift_policy_mlir_matrix(void) {
  typedef struct {
    const char *spelling;
    const char *high_bit_literal;
    bool is_signed;
    uint16_t bit_width;
  } integer_case;
  static const integer_case INTEGERS[] = {
      {"i8", "-64_i8", true, 8u},
      {"u8", "128_u8", false, 8u},
      {"i16", "-16384_i16", true, 16u},
      {"u16", "32768_u16", false, 16u},
      {"i32", "-1073741824_i32", true, 32u},
      {"u32", "2147483648_u32", false, 32u},
      {"i64", "-4611686018427387904_i64", true, 64u},
      {"u64", "9223372036854775808_u64", false, 64u},
  };
  typedef struct {
    const char *name;
    const char *member;
    w_seed_hir0_binary_operator operation;
  } shift_case;
  static const shift_case SHIFTS[] = {
      {"masked_left", "maskedShiftLeft",
       W_SEED_HIR0_BINARY_MASKED_SHIFT_LEFT},
      {"masked_right", "maskedShiftRight",
       W_SEED_HIR0_BINARY_MASKED_SHIFT_RIGHT},
      {"logical_right", "logicalShiftRight",
       W_SEED_HIR0_BINARY_LOGICAL_SHIFT_RIGHT},
  };
  static char source[TEST_SOURCE];
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;

  for (size_t integer_index = 0u;
       integer_index < sizeof(INTEGERS) / sizeof(INTEGERS[0]);
       integer_index += 1u) {
    const integer_case *integer = &INTEGERS[integer_index];
    size_t source_length = 0u;
    for (size_t shift_index = 0u;
         shift_index < sizeof(SHIFTS) / sizeof(SHIFTS[0]); shift_index += 1u)
      CHECK(append_mlir_test_source(
          source, sizeof(source), &source_length,
          "fn %s(value: %s, count: UInt): %s { return %s.%s(value, count) }\n",
          SHIFTS[shift_index].name, integer->spelling, integer->spelling,
          integer->spelling, SHIFTS[shift_index].member));
    CHECK(append_mlir_test_source(source, sizeof(source), &source_length,
                                  "fn main() {\n"));
    for (size_t shift_index = 0u;
         shift_index < sizeof(SHIFTS) / sizeof(SHIFTS[0]); shift_index += 1u)
      for (uint16_t boundary = 0u; boundary < 4u; boundary += 1u) {
        const uint16_t count =
            boundary == 0u
                ? 0u
                : (boundary == 1u
                       ? (uint16_t)(integer->bit_width - 1u)
                       : (boundary == 2u ? integer->bit_width
                                         : (uint16_t)(integer->bit_width + 1u)));
        CHECK(append_mlir_test_source(
            source, sizeof(source), &source_length,
            "let %s_%u = %s(value: %s, count: %u_u64)\n",
            SHIFTS[shift_index].name, (unsigned)boundary,
            SHIFTS[shift_index].name, integer->high_bit_literal,
            (unsigned)count));
      }
    CHECK(append_mlir_test_source(source, sizeof(source), &source_length,
                                  "print(\""));
    bool first_interpolation = true;
    for (size_t shift_index = 0u;
         shift_index < sizeof(SHIFTS) / sizeof(SHIFTS[0]); shift_index += 1u)
      for (uint16_t boundary = 0u; boundary < 4u; boundary += 1u) {
        const char *format =
            first_interpolation ? "${%s_%u}" : "/${%s_%u}";
        CHECK(append_mlir_test_source(source, sizeof(source), &source_length,
                                      format, SHIFTS[shift_index].name,
                                      (unsigned)boundary));
        first_interpolation = false;
      }
    CHECK(append_mlir_test_source(source, sizeof(source), &source_length,
                                  "\")\n}\nentry(main)\n"));
    CHECK(lower_hir((const uint8_t *)source, source_length));

    size_t operation_counts[sizeof(SHIFTS) / sizeof(SHIFTS[0])] = {0u};
    uint32_t shift_value_indices[sizeof(SHIFTS) / sizeof(SHIFTS[0])] = {
        W_SEED_HIR0_NONE, W_SEED_HIR0_NONE, W_SEED_HIR0_NONE};
    uint32_t forged_shift = W_SEED_HIR0_NONE;
    uint32_t u64_type = W_SEED_HIR0_NONE;
    for (size_t type_index = 0u;
         type_index < fixture.hir_program.type_count; type_index += 1u) {
      const w_seed_hir0_type *type = &fixture.hir_program.types[type_index];
      if (type->kind == W_SEED_HIR0_TYPE_U64 &&
          !type->integer_is_signed && type->integer_bit_width == 64u)
        u64_type = (uint32_t)type_index;
    }
    for (size_t value_index = 0u;
         value_index < fixture.hir_program.value_count; value_index += 1u) {
      const w_seed_hir0_value *value = &fixture.hir_program.values[value_index];
      size_t shift_index = SIZE_MAX;
      for (size_t candidate = 0u;
           candidate < sizeof(SHIFTS) / sizeof(SHIFTS[0]); candidate += 1u)
        if (value->binary_operator == SHIFTS[candidate].operation) {
          shift_index = candidate;
          break;
        }
      if (shift_index == SIZE_MAX) continue;
      CHECK(value->type_index < fixture.hir_program.type_count &&
            fixture.hir_program.types[value->type_index].integer_is_signed ==
                integer->is_signed &&
            fixture.hir_program.types[value->type_index].integer_bit_width ==
                integer->bit_width);
      operation_counts[shift_index] += 1u;
      shift_value_indices[shift_index] = (uint32_t)value_index;
      if (shift_index == 0u && integer_index == 0u)
        forged_shift = (uint32_t)value_index;
    }
    CHECK(operation_counts[0] == 1u && operation_counts[1] == 1u &&
          operation_counts[2] == 1u);
    CHECK(measure_current(&counts, &measured));
    CHECK(emit_current(artifact, sizeof(artifact), &emitted));
    const size_t artifact_bytes = emitted.written.mlir_bytes;
    CHECK(counts.mlir_bytes == artifact_bytes &&
          memcmp(measured.mlir_sha256, emitted.mlir_sha256,
                 sizeof(measured.mlir_sha256)) == 0 &&
          count_bytes(artifact, artifact_bytes,
                      "llvm.func internal @w_seed_logical_shift_right_integer") ==
              1u &&
          count_bytes(artifact, artifact_bytes,
                      "llvm.call @w_seed_logical_shift_right_integer") == 1u &&
          !contains_bytes(artifact, artifact_bytes,
                          "@w_seed_masked_shift_left_u64") &&
          !contains_bytes(artifact, artifact_bytes,
                          "@w_seed_masked_shift_right_u64") &&
          contains_bytes(artifact, artifact_bytes,
                         integer->is_signed
                             ? "_logical_signed = llvm.mlir.constant(true) : i1"
                             : "_logical_signed = llvm.mlir.constant(false) : i1") &&
          contains_bytes(artifact, artifact_bytes, "llvm.icmp \"uge\"") &&
          contains_bytes(artifact, artifact_bytes,
                         "llvm.lshr %normalized, %count : i64") &&
          contains_bytes(artifact, artifact_bytes,
                         "\"llvm.intr.trap\"() : () -> ()") &&
          contains_bytes(artifact, artifact_bytes, "llvm.unreachable"));

    char carrier_type[12];
    const int type_length = snprintf(carrier_type, sizeof(carrier_type),
                                     ": i%u", (unsigned)integer->bit_width);
    CHECK(type_length > 0 && (size_t)type_length < sizeof(carrier_type));
    char left_shift_line[80];
    char right_shift_line[80];
    const char *shift_result_suffix =
        integer->bit_width < 64u ? "_shift_raw" : "";
    const int left_shift_length = snprintf(
        left_shift_line, sizeof(left_shift_line), "%cv%u%s = llvm.shl",
        '%', shift_value_indices[0], shift_result_suffix);
    const int right_shift_length = snprintf(
        right_shift_line, sizeof(right_shift_line),
        "%cv%u%s = llvm.%s", '%', shift_value_indices[1],
        shift_result_suffix, integer->is_signed ? "ashr" : "lshr");
    CHECK(left_shift_length > 0 &&
          (size_t)left_shift_length < sizeof(left_shift_line) &&
          right_shift_length > 0 &&
          (size_t)right_shift_length < sizeof(right_shift_line));
    CHECK(count_mlir_lines_with_fragment_and_type(
              artifact, artifact_bytes, left_shift_line, carrier_type) == 1u &&
          count_mlir_lines_with_fragment_and_type(
              artifact, artifact_bytes, right_shift_line, carrier_type) == 1u);
    if (integer->bit_width < 64u)
      CHECK(contains_bytes(artifact, artifact_bytes,
                           "_shift_count = llvm.trunc") &&
            contains_bytes(artifact, artifact_bytes,
                           integer->bit_width == 8u
                               ? "to i8"
                               : (integer->bit_width == 16u ? "to i16"
                                                            : "to i32")));
    char mask_needle[64];
    const int mask_length = snprintf(
        mask_needle, sizeof(mask_needle),
        "_shift_mask = llvm.mlir.constant(%u : i64)",
        (unsigned)integer->bit_width - 1u);
    CHECK(mask_length > 0 && (size_t)mask_length < sizeof(mask_needle) &&
          contains_bytes(artifact, artifact_bytes, mask_needle));
    for (uint16_t boundary = 0u; boundary < 4u; boundary += 1u) {
      const unsigned count =
          boundary == 0u
              ? 0u
              : (boundary == 1u
                     ? (unsigned)integer->bit_width - 1u
                     : (boundary == 2u ? (unsigned)integer->bit_width
                                       : (unsigned)integer->bit_width + 1u));
      char count_needle[64];
      const int count_length = snprintf(count_needle, sizeof(count_needle),
                                        "llvm.mlir.constant(%u : i64)",
                                        count);
      CHECK(count_length > 0 && (size_t)count_length < sizeof(count_needle) &&
            contains_bytes(artifact, artifact_bytes, count_needle));
    }

    const size_t guard =
        find_bytes(artifact, artifact_bytes,
                   "llvm.icmp \"uge\" %count, %width : i64", 0u);
    const size_t fault =
        guard == SIZE_MAX
            ? SIZE_MAX
            : find_bytes(artifact, artifact_bytes,
                         "^logical_shift_right_fault:", guard);
    const size_t trap =
        fault == SIZE_MAX
            ? SIZE_MAX
            : find_bytes(artifact, artifact_bytes,
                         "\"llvm.intr.trap\"() : () -> ()", fault);
    const size_t unreachable =
        trap == SIZE_MAX
            ? SIZE_MAX
            : find_bytes(artifact, artifact_bytes, "llvm.unreachable", trap);
    const size_t apply =
        guard == SIZE_MAX
            ? SIZE_MAX
            : find_bytes(artifact, artifact_bytes,
                         "^logical_shift_right_apply:", guard);
    const size_t logical_shift =
        apply == SIZE_MAX
            ? SIZE_MAX
            : find_bytes(artifact, artifact_bytes,
                         "llvm.lshr %normalized, %count : i64", apply);
    CHECK(guard < fault && fault < trap && trap < unreachable &&
          unreachable < apply && apply < logical_shift);

    if (integer_index == 0u) {
      CHECK(forged_shift != W_SEED_HIR0_NONE && u64_type != W_SEED_HIR0_NONE);
      const w_seed_mlir0_input input = mlir_input();
      uint8_t forged_output[W_SEED_MLIR0_MAX_BYTES];
      (void)memset(forged_output, 0xa5u, sizeof(forged_output));
      w_seed_mlir0_result forged_result;
      (void)memset(&forged_result, 0x5au, sizeof(forged_result));
      const w_seed_mlir0_result forged_snapshot = forged_result;
      const uint32_t saved_type = fixture.hir_values[forged_shift].type_index;
      fixture.hir_values[forged_shift].type_index = u64_type;
      CHECK(w_seed_mlir0_emit(
                &input, &TARGET,
                &(w_seed_mlir0_output){forged_output, sizeof(forged_output)},
                &forged_result) == W_SEED_MLIR0_INVALID_HIR);
      for (size_t byte = 0u; byte < sizeof(forged_output); byte += 1u)
        CHECK(forged_output[byte] == 0xa5u);
      CHECK(memcmp(&forged_result, &forged_snapshot,
                   sizeof(forged_result)) == 0);
      fixture.hir_values[forged_shift].type_index = saved_type;
      CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
    }
  }
  return true;
}

static bool test_u64_rotated_left_artifact(void) {
  static const uint8_t source[] =
      "fn rotate(value: UInt, count: UInt): UInt { "
      "return u64.rotatedLeft(value, count) }\n"
      "entry { let width = rotate(value: 7_u64, count: 64_u64) "
      "let next = rotate(value: 1_u64, count: 65_u64) "
      "print(\"${width}/${next}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  size_t rotated_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u)
    if (fixture.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        fixture.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_ROTATED_LEFT)
      rotated_count += 1u;
  CHECK(rotated_count == 1u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0 &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_rotated_left_u64") &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.intr.fshl") == 1u &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "_rotate_mask = llvm.mlir.constant(63 : i64)") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "_rotate_mod = llvm.and ") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "\"llvm.intr.trap\"() : () -> ()"));

  static const uint8_t unreachable_source[] =
      "entry { print(\"No rotated left\") }\n";
  CHECK(lower_hir(unreachable_source, sizeof(unreachable_source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_rotated_left_u64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.intr.fshl"));
  return true;
}

static bool test_u64_rotated_right_artifact(void) {
  static const uint8_t source[] =
      "fn rotate(value: UInt, count: UInt): UInt { "
      "return u64.rotatedRight(value, count) }\n"
      "entry { let width = rotate(value: 7_u64, count: 64_u64) "
      "let next = rotate(value: 3_u64, count: 65_u64) "
      "print(\"${width}/${next}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  size_t rotated_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u)
    if (fixture.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        fixture.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_ROTATED_RIGHT)
      rotated_count += 1u;
  CHECK(rotated_count == 1u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0 &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_rotated_right_u64") &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.intr.fshr") == 1u &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "_rotate_mask = llvm.mlir.constant(63 : i64)") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "_rotate_mod = llvm.and ") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "\"llvm.intr.trap\"() : () -> ()"));

  static const uint8_t unreachable_source[] =
      "entry { print(\"No rotated right\") }\n";
  CHECK(lower_hir(unreachable_source, sizeof(unreachable_source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_rotated_right_u64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.intr.fshr"));
  return true;
}

static bool test_u64_count_ones_artifact(void) {
  static const uint8_t source[] =
      "fn count(value: UInt): UInt { return u64.countOnes(value) }\n"
      "entry { let zero = count(value: 0_u64) "
      "let pattern = count(value: 0xf0f0f0f00f0f0f0f_u64) "
      "print(\"${zero}/${pattern}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  size_t count_ones_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u)
    if (fixture.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_UNARY_U64 &&
        fixture.hir_program.values[index].unary_operator ==
            W_SEED_HIR0_UNARY_COUNT_ONES)
      count_ones_count += 1u;
  CHECK(count_ones_count == 1u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0 &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.intr.ctpop") == 1u &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "\"llvm.intr.trap\"() : () -> ()"));

  static const uint8_t unreachable_source[] =
      "entry { print(\"No population count\") }\n";
  CHECK(lower_hir(unreachable_source, sizeof(unreachable_source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.intr.ctpop"));
  return true;
}

static bool test_u64_count_zeros_artifact(void) {
  static const uint8_t source[] =
      "fn count(value: UInt): UInt { return u64.countZeros(value) }\n"
      "entry { let zero = count(value: 0_u64) "
      "let pattern = count(value: 0xf0f0f0f00f0f0f0f_u64) "
      "print(\"${zero}/${pattern}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  size_t count_zeros_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u)
    if (fixture.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_UNARY_U64 &&
        fixture.hir_program.values[index].unary_operator ==
            W_SEED_HIR0_UNARY_COUNT_ZEROS)
      count_zeros_count += 1u;
  CHECK(count_zeros_count == 1u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0 &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.intr.ctpop") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "_bit_width = llvm.mlir.constant(64 : i64)") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "_bit_ones : i64") == 1u &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "\"llvm.intr.trap\"() : () -> ()"));

  static const uint8_t unreachable_source[] =
      "entry { print(\"No zero count\") }\n";
  CHECK(lower_hir(unreachable_source, sizeof(unreachable_source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.intr.ctpop") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "_bit_width"));
  return true;
}

static bool test_u64_count_leading_zeros_artifact(void) {
  static const uint8_t source[] =
      "fn count(value: UInt): UInt { return u64.countLeadingZeros(value) }\n"
      "entry { let zero = count(value: 0_u64) "
      "let pattern = count(value: 0xf0_u64) "
      "print(\"${zero}/${pattern}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  size_t count_leading_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u)
    if (fixture.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_UNARY_U64 &&
        fixture.hir_program.values[index].unary_operator ==
            W_SEED_HIR0_UNARY_COUNT_LEADING_ZEROS)
      count_leading_count += 1u;
  CHECK(count_leading_count == 1u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0 &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.intr.ctlz") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "is_zero_poison = false") == 1u &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "\"llvm.intr.trap\"() : () -> ()"));

  static const uint8_t unreachable_source[] =
      "entry { print(\"No leading zeros\") }\n";
  CHECK(lower_hir(unreachable_source, sizeof(unreachable_source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.intr.ctlz") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "is_zero_poison"));
  return true;
}

static bool test_u64_count_trailing_zeros_artifact(void) {
  static const uint8_t source[] =
      "fn count(value: UInt): UInt { return u64.countTrailingZeros(value) }\n"
      "entry { let zero = count(value: 0_u64) "
      "let pattern = count(value: 0xf000_u64) "
      "print(\"${zero}/${pattern}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  size_t count_trailing_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u)
    if (fixture.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_UNARY_U64 &&
        fixture.hir_program.values[index].unary_operator ==
            W_SEED_HIR0_UNARY_COUNT_TRAILING_ZEROS)
      count_trailing_count += 1u;
  CHECK(count_trailing_count == 1u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0 &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.intr.cttz") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "is_zero_poison = false") == 1u &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "\"llvm.intr.trap\"() : () -> ()"));

  static const uint8_t unreachable_source[] =
      "entry { print(\"No trailing zeros\") }\n";
  CHECK(lower_hir(unreachable_source, sizeof(unreachable_source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.intr.cttz") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "is_zero_poison"));
  return true;
}

static bool test_u64_reversed_bits_artifact(void) {
  static const uint8_t source[] =
      "fn reverse(value: UInt): UInt { return u64.reversedBits(value) }\n"
      "entry { let zero = reverse(value: 0_u64) "
      "let pattern = reverse(value: 0x0123456789abcdef_u64) "
      "print(\"${zero}/${pattern}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  size_t reversed_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u)
    if (fixture.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_UNARY_U64 &&
        fixture.hir_program.values[index].unary_operator ==
            W_SEED_HIR0_UNARY_REVERSED_BITS)
      reversed_count += 1u;
  CHECK(reversed_count == 1u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0 &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.intr.bitreverse") == 1u &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "\"llvm.intr.trap\"() : () -> ()"));

  static const uint8_t unreachable_source[] =
      "entry { print(\"No reversed bits\") }\n";
  CHECK(lower_hir(unreachable_source, sizeof(unreachable_source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.intr.bitreverse"));
  return true;
}

static bool test_u64_reversed_bytes_artifact(void) {
  static const uint8_t source[] =
      "fn reverse(value: UInt): UInt { return u64.reversedBytes(value) }\n"
      "entry { let zero = reverse(value: 0_u64) "
      "let pattern = reverse(value: 0x0123456789abcdef_u64) "
      "print(\"${zero}/${pattern}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  size_t reversed_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u)
    if (fixture.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_UNARY_U64 &&
        fixture.hir_program.values[index].unary_operator ==
            W_SEED_HIR0_UNARY_REVERSED_BYTES)
      reversed_count += 1u;
  CHECK(reversed_count == 1u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0 &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.intr.bswap") == 1u &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "\"llvm.intr.trap\"() : () -> ()"));

  static const uint8_t unreachable_source[] =
      "entry { print(\"No reversed bytes\") }\n";
  CHECK(lower_hir(unreachable_source, sizeof(unreachable_source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "llvm.intr.bswap"));
  return true;
}

static bool test_fixed_integer_bit_primitive_width_artifact(void) {
  static const struct {
    const char *type;
    const char *pattern;
    const char *zero;
    const char *negative;
    const char *rotate_count;
    uint16_t bit_width;
    bool is_signed;
  } INTEGERS[] = {
      {"i8", "0x52_i8", "0_i8", "~1_i8", "9_u64", 8u, true},
      {"u8", "0x96_u8", "0_u8", "0x96_u8", "9_u64", 8u, false},
      {"i16", "0x1234_i16", "0_i16", "~1_i16", "17_u64", 16u, true},
      {"u16", "0x89ab_u16", "0_u16", "0x89ab_u16", "17_u64", 16u,
       false},
      {"i32", "0x12345678_i32", "0_i32", "~1_i32", "33_u64", 32u,
       true},
      {"u32", "0x89abcdef_u32", "0_u32", "0x89abcdef_u32", "33_u64",
       32u, false},
      {"i64", "0x0123456789abcd6e_i64", "0_i64", "~1_i64", "65_u64",
       64u, true},
      {"u64", "0xfedcba9876543210_u64", "0_u64",
       "0xfedcba9876543210_u64", "65_u64", 64u, false},
  };
  static const char *const OPERATIONS[] = {
      "countOnes",      "countZeros",      "countLeadingZeros",
      "countTrailingZeros", "reversedBits", "reversedBytes",
      "rotatedLeft",    "rotatedRight",
  };
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  for (size_t integer = 0u;
       integer < sizeof(INTEGERS) / sizeof(INTEGERS[0]); integer += 1u) {
    char source[TEST_SOURCE];
    size_t source_length = 0u;
    CHECK(append_mlir_test_source(source, sizeof(source), &source_length,
                                 "fn bitMatrix() { "));
    size_t binding = 0u;
    for (size_t operation = 0u;
         operation < sizeof(OPERATIONS) / sizeof(OPERATIONS[0]);
         operation += 1u) {
      const char *argument = INTEGERS[integer].pattern;
      if (operation == 2u || operation == 3u)
        argument = INTEGERS[integer].zero;
      else if ((operation == 4u || operation == 5u) &&
               INTEGERS[integer].is_signed)
        argument = INTEGERS[integer].negative;
      const bool rotation = operation == 6u || operation == 7u;
      if (rotation) {
        CHECK(append_mlir_test_source(
            source, sizeof(source), &source_length,
            "let bit%u = %s.%s(%s, %s) ", (unsigned)binding,
            INTEGERS[integer].type, OPERATIONS[operation], argument,
            INTEGERS[integer].rotate_count));
      } else {
        CHECK(append_mlir_test_source(
            source, sizeof(source), &source_length, "let bit%u = %s.%s(%s) ",
            (unsigned)binding, INTEGERS[integer].type,
            OPERATIONS[operation], argument));
      }
      binding += 1u;
    }
    CHECK(binding == 8u &&
        append_mlir_test_source(source, sizeof(source), &source_length,
                                "print(\""));
    for (size_t index = 0u; index < binding; index += 1u)
      CHECK(append_mlir_test_source(source, sizeof(source), &source_length,
                                   "${bit%u}%s", (unsigned)index,
                                   index + 1u == binding ? "" : "/"));
    CHECK(append_mlir_test_source(source, sizeof(source), &source_length,
                                 "\") }\nentry(bitMatrix)\n"));

    w_seed_mlir0_counts counts;
    w_seed_mlir0_result measured;
    w_seed_mlir0_result emitted;
    CHECK(lower_hir((const uint8_t *)source, source_length));
    size_t unary_count = 0u;
    size_t rotated_left_count = 0u;
    size_t rotated_right_count = 0u;
    for (size_t index = 0u; index < fixture.hir_program.value_count;
         index += 1u) {
      const w_seed_hir0_value *value = &fixture.hir_program.values[index];
      if ((value->kind == W_SEED_HIR0_VALUE_UNARY_I64 ||
           value->kind == W_SEED_HIR0_VALUE_UNARY_U64) &&
          value->unary_operator >= W_SEED_HIR0_UNARY_COUNT_ONES &&
          value->unary_operator <= W_SEED_HIR0_UNARY_REVERSED_BYTES)
        unary_count += 1u;
      if ((value->kind == W_SEED_HIR0_VALUE_BINARY_I64 ||
           value->kind == W_SEED_HIR0_VALUE_BINARY_U64) &&
          value->binary_operator == W_SEED_HIR0_BINARY_ROTATED_LEFT)
        rotated_left_count += 1u;
      if ((value->kind == W_SEED_HIR0_VALUE_BINARY_I64 ||
           value->kind == W_SEED_HIR0_VALUE_BINARY_U64) &&
          value->binary_operator == W_SEED_HIR0_BINARY_ROTATED_RIGHT)
        rotated_right_count += 1u;
    }
    CHECK(unary_count == 6u && rotated_left_count == 1u &&
          rotated_right_count == 1u);
    CHECK(measure_current(&counts, &measured));
    CHECK(emit_current(artifact, sizeof(artifact), &emitted));
    const size_t mlir_bytes = emitted.written.mlir_bytes;
    CHECK(counts.mlir_bytes == mlir_bytes &&
          memcmp(measured.mlir_sha256, emitted.mlir_sha256,
                 sizeof(measured.mlir_sha256)) == 0 &&
          !contains_bytes(artifact, mlir_bytes,
                          "@w_seed_rotated_left_u64") &&
          !contains_bytes(artifact, mlir_bytes,
                          "@w_seed_rotated_right_u64") &&
          count_bytes(artifact, mlir_bytes, "llvm.intr.fshl") == 1u &&
          count_bytes(artifact, mlir_bytes, "llvm.intr.fshr") == 1u &&
          count_bytes(artifact, mlir_bytes, "llvm.intr.ctpop") == 2u &&
          count_bytes(artifact, mlir_bytes, "llvm.intr.ctlz") == 1u &&
          count_bytes(artifact, mlir_bytes, "llvm.intr.cttz") == 1u &&
          count_bytes(artifact, mlir_bytes, "llvm.intr.bitreverse") == 1u &&
          count_bytes(artifact, mlir_bytes, "llvm.intr.bswap") ==
              (INTEGERS[integer].bit_width == 8u ? 0u : 1u) &&
          count_bytes(artifact, mlir_bytes, "is_zero_poison = false") == 2u &&
          !contains_bytes(artifact, mlir_bytes,
                          "\"llvm.intr.trap\"() : () -> ()"));

    char type_suffix[40];
    char zero_poison_suffix[96];
    char width_constant[80];
    char rotate_mask[80];
    const int type_suffix_length = snprintf(
        type_suffix, sizeof(type_suffix), ") : (i%u) -> i%u",
        (unsigned)INTEGERS[integer].bit_width,
        (unsigned)INTEGERS[integer].bit_width);
    const int zero_poison_suffix_length = snprintf(
        zero_poison_suffix, sizeof(zero_poison_suffix),
        ") <{is_zero_poison = false}> : (i%u) -> i%u",
        (unsigned)INTEGERS[integer].bit_width,
        (unsigned)INTEGERS[integer].bit_width);
    const int width_constant_length = snprintf(
        width_constant, sizeof(width_constant),
        "_bit_width = llvm.mlir.constant(%u : i%u)",
        (unsigned)INTEGERS[integer].bit_width,
        (unsigned)INTEGERS[integer].bit_width);
    const int rotate_mask_length = snprintf(
        rotate_mask, sizeof(rotate_mask),
        "_rotate_mask = llvm.mlir.constant(%u : i64)",
        (unsigned)INTEGERS[integer].bit_width - 1u);
    CHECK(type_suffix_length > 0 &&
          (size_t)type_suffix_length < sizeof(type_suffix) &&
          zero_poison_suffix_length > 0 &&
          (size_t)zero_poison_suffix_length < sizeof(zero_poison_suffix) &&
          width_constant_length > 0 &&
          (size_t)width_constant_length < sizeof(width_constant) &&
          rotate_mask_length > 0 &&
          (size_t)rotate_mask_length < sizeof(rotate_mask));
    const size_t width = INTEGERS[integer].bit_width;
    CHECK(count_mlir_lines_with_fragment_and_type(
              artifact, emitted.written.mlir_bytes, "llvm.intr.ctpop",
              type_suffix) == 2u &&
          count_mlir_lines_with_fragment_and_type(
              artifact, emitted.written.mlir_bytes, "llvm.intr.ctlz",
              zero_poison_suffix) == 1u &&
          count_mlir_lines_with_fragment_and_type(
              artifact, emitted.written.mlir_bytes, "llvm.intr.cttz",
              zero_poison_suffix) == 1u &&
          count_mlir_lines_with_fragment_and_type(
              artifact, emitted.written.mlir_bytes, "llvm.intr.bitreverse",
              type_suffix) == 1u &&
          count_mlir_lines_with_fragment_and_type(
              artifact, emitted.written.mlir_bytes, "llvm.intr.bswap",
              type_suffix) == (width == 8u ? 0u : 1u));
    char rotation_suffix[64];
    const int rotation_suffix_length = snprintf(
        rotation_suffix, sizeof(rotation_suffix),
        ") : (i%u, i%u, i%u) -> i%u", (unsigned)width,
        (unsigned)width, (unsigned)width, (unsigned)width);
    CHECK(rotation_suffix_length > 0 &&
          (size_t)rotation_suffix_length < sizeof(rotation_suffix) &&
          count_mlir_lines_with_fragment_and_type(
              artifact, emitted.written.mlir_bytes, "llvm.intr.fshl",
              rotation_suffix) == 1u &&
          count_mlir_lines_with_fragment_and_type(
              artifact, emitted.written.mlir_bytes, "llvm.intr.fshr",
              rotation_suffix) == 1u &&
          count_bytes(artifact, emitted.written.mlir_bytes, width_constant) ==
              1u &&
          count_bytes(artifact, emitted.written.mlir_bytes, rotate_mask) ==
              2u);
    CHECK(count_bytes(artifact, mlir_bytes, "_bit_input = llvm.trunc ") ==
              (width < 64u ? 6u : 0u) &&
          count_bytes(artifact, mlir_bytes, "_rotate_input = llvm.trunc ") ==
              (width < 64u ? 2u : 0u) &&
          count_bytes(artifact, mlir_bytes, "llvm.zext %v") ==
              (width == 64u ? 0u : (INTEGERS[integer].is_signed ? 4u : 8u)) &&
          count_bytes(artifact, mlir_bytes, "llvm.sext %v") ==
              (width < 64u && INTEGERS[integer].is_signed ? 6u : 0u) &&
          count_bytes(artifact, mlir_bytes, "_bit_raw = llvm.or ") ==
              (width == 8u ? 1u : 0u));
  }
  return true;
}

static bool test_unsigned_unary_bit_not_artifact(void) {
  static const uint8_t dynamic_source[] =
      "fn main() { print(\"UInt not ${~0_u64}\") }\n"
      "entry(main)\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(dynamic_source, sizeof(dynamic_source) - 1u));
  size_t unary_u64_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u)
    if (fixture.hir_program.values[index].kind ==
        W_SEED_HIR0_VALUE_UNARY_U64)
      unary_u64_count += 1u;
  CHECK(unary_u64_count == 1u);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "_bit_not_mask = llvm.mlir.constant(-1 : i64)") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes, "llvm.xor ") == 1u &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.call @w_seed_append_u64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_checked_subtract_i64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_checked_negate_i64"));

  static const uint8_t local_source[] =
      "fn invert(value: UInt): UInt { return ~value }\n"
      "entry { let inverted = invert(value: 0_u64) "
      "print(\"UInt not ${inverted}\") }\n";
  CHECK(lower_hir(local_source, sizeof(local_source) - 1u));
  unary_u64_count = 0u;
  for (size_t index = 0u; index < fixture.hir_program.value_count;
       index += 1u)
    if (fixture.hir_program.values[index].kind ==
        W_SEED_HIR0_VALUE_UNARY_U64)
      unary_u64_count += 1u;
  CHECK(unary_u64_count == 1u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_local_calls && !selection.has_cfg);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "_bit_not_mask = llvm.mlir.constant(-1 : i64)") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes, "llvm.xor ") == 1u &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.call @w_fn_0") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.call @w_seed_append_u64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_checked_subtract_i64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_checked_negate_i64"));

  static const uint8_t cfg_source[] =
      "fn choose(flag: Bool, value: UInt): UInt { let inverted = ~value "
      "if flag { return inverted } else { return value } }\n"
      "fn main() { let result = choose(flag: true, value: 0_u64) "
      "print(\"UInt ${result}\") }\n"
      "entry(main)\n";
  CHECK(lower_hir(cfg_source, sizeof(cfg_source) - 1u));
  CHECK(w_seed_native_subset0_select_program(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_UNSUPPORTED);
  const w_seed_mlir0_input input = mlir_input();
  w_seed_mlir0_counts rejected_counts = {0x11u};
  w_seed_mlir0_result rejected_measure;
  (void)memset(&rejected_measure, 0x22u, sizeof(rejected_measure));
  const w_seed_mlir0_result rejected_measure_snapshot = rejected_measure;
  CHECK(w_seed_mlir0_measure(&input, &TARGET, &rejected_counts,
                             &rejected_measure) == W_SEED_MLIR0_UNSUPPORTED);
  CHECK(rejected_counts.mlir_bytes == 0x11u &&
        memcmp(&rejected_measure, &rejected_measure_snapshot,
               sizeof(rejected_measure)) == 0);
  (void)memset(artifact, 0xa5u, sizeof(artifact));
  w_seed_mlir0_result rejected_emit;
  (void)memset(&rejected_emit, 0x33u, sizeof(rejected_emit));
  const w_seed_mlir0_result rejected_emit_snapshot = rejected_emit;
  CHECK(w_seed_mlir0_emit(
            &input, &TARGET, &(w_seed_mlir0_output){artifact, sizeof(artifact)},
            &rejected_emit) == W_SEED_MLIR0_UNSUPPORTED);
  for (size_t index = 0u; index < sizeof(artifact); index += 1u)
    CHECK(artifact[index] == 0xa5u);
  CHECK(memcmp(&rejected_emit, &rejected_emit_snapshot,
               sizeof(rejected_emit)) == 0);
  return true;
}

static bool test_scalar_if_value_diamond(void) {
  static const uint8_t source[] =
      "fn serve(isOpen: Bool, openCount: i64, closedCount: i64): i64 { "
      "return if isOpen { openCount } else { closedCount } }\n"
      "fn flag(isOpen: Bool): Bool { return if isOpen { true } else { false } }\n"
      "fn main() { "
      "let served = serve(isOpen: true, openCount: 1, closedCount: 2) "
      "let flagged = flag(isOpen: true) "
      "print(\"scalar ${served} ${flagged}\") }\n"
      "entry(main)\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 3u && program->block_count == 9u &&
        program->block_argument_count == 2u &&
        program->functions[0].block_count == 4u &&
        program->functions[1].first_block == 4u &&
        program->functions[1].block_count == 4u &&
        program->functions[2].first_block == 8u &&
        program->functions[2].block_count == 1u);
  CHECK(program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[0].logical_operator ==
            W_SEED_HIR0_LOGICAL_NONE &&
        program->terminators[0].result_type == W_SEED_HIR0_TYPE_I64 &&
        program->blocks[3].block_argument_count == 1u &&
        program->block_arguments[0].type_index == W_SEED_HIR0_TYPE_I64 &&
        program->terminators[4].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[4].logical_operator ==
            W_SEED_HIR0_LOGICAL_NONE &&
        program->terminators[4].result_type == W_SEED_HIR0_TYPE_BOOL &&
        program->blocks[7].block_argument_count == 1u &&
        program->block_arguments[1].type_index == W_SEED_HIR0_TYPE_BOOL);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0);
  CHECK(contains_bytes(
      artifact, emitted.written.mlir_bytes,
      "llvm.func internal @w_fn_0(%buffer: !llvm.ptr, %cursor_address: !llvm.ptr, %p0: i1, %p1: i64, %p2: i64) -> i64"));
  CHECK(contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.cond_br %p0, ^w_fn_0_b_1, ^w_fn_0_b_2") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "^w_fn_0_b_3(%arg0: i64):") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.br ^w_fn_0_b_3(%p1 : i64)") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.br ^w_fn_0_b_3(%p2 : i64)") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.return %arg0 : i64"));
  CHECK(contains_bytes(
      artifact, emitted.written.mlir_bytes,
      "llvm.func internal @w_fn_1(%buffer: !llvm.ptr, %cursor_address: !llvm.ptr, %p0: i1) -> i1"));
  CHECK(contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.cond_br %p0, ^w_fn_1_b_5, ^w_fn_1_b_6") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "^w_fn_1_b_7(%arg1: i1):") &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.br ^w_fn_1_b_7(") == 2u &&
        count_bytes(artifact, emitted.written.mlir_bytes, ": i1)") >= 2u &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.return %arg1 : i1"));
  const size_t serve = find_bytes(
      artifact, emitted.written.mlir_bytes, "llvm.func internal @w_fn_0(", 0u);
  const size_t flag = find_bytes(
      artifact, emitted.written.mlir_bytes, "llvm.func internal @w_fn_1(",
      serve == SIZE_MAX ? 0u : serve + 1u);
  const size_t main = find_bytes(artifact, emitted.written.mlir_bytes,
                                 "llvm.func internal @w_fn_2(",
                                 flag == SIZE_MAX ? 0u : flag + 1u);
  CHECK(serve != SIZE_MAX && flag != SIZE_MAX && main != SIZE_MAX);
  CHECK(find_bytes(artifact + serve, flag - serve, "llvm.select", 0u) ==
            SIZE_MAX &&
        find_bytes(artifact + flag,
                   main - flag, "llvm.select", 0u) ==
            SIZE_MAX);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.maximum_stdout_bytes == 34u);
  const w_seed_mlir0_input windows_input = mlir_input();
  w_seed_mlir0_counts windows_counts;
  w_seed_mlir0_result windows_result;
  CHECK(w_seed_mlir0_measure(&windows_input, &WINDOWS_TARGET,
                             &windows_counts, &windows_result) ==
        W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_emit(
            &windows_input, &WINDOWS_TARGET,
            &(w_seed_mlir0_output){artifact, sizeof(artifact)},
            &windows_result) == W_SEED_MLIR0_OK);
  CHECK(contains_bytes(artifact, windows_result.written.mlir_bytes,
                       "@w_seed_mlir0_buffer() : !llvm.array<35 x i8>") &&
        contains_bytes(artifact, windows_result.written.mlir_bytes,
                       "llvm.store %output_terminator_zero, %output_terminator_address"));
  return true;
}

static bool test_nested_scalar_if_value_diamond(void) {
  static const uint8_t source[] =
      "fn choose(outer: Bool, inner: Bool, open: i64, middle: i64, "
      "closed: i64): i64 { return if outer { if inner { open } else { "
      "middle } } else { closed } }\n"
      "fn main() { let first = choose(outer: true, inner: true, open: 1, "
      "middle: 2, closed: 3) let second = choose(outer: true, inner: false, "
      "open: 1, middle: 2, closed: 3) let third = choose(outer: false, "
      "inner: false, open: 1, middle: 2, closed: 3) "
      "print(\"${first},${second},${third}\") }\n"
      "entry(main)\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 2u && program->block_count == 8u &&
        program->block_argument_count == 2u &&
        program->functions[0].first_block == 0u &&
        program->functions[0].block_count == 7u &&
        program->functions[1].first_block == 7u &&
        program->functions[1].block_count == 1u);
  CHECK(program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[0].target_block == 1u &&
        program->terminators[0].else_block == 5u &&
        program->terminators[0].result_type == W_SEED_HIR0_TYPE_I64 &&
        program->terminators[1].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[1].target_block == 2u &&
        program->terminators[1].else_block == 3u &&
        program->terminators[1].result_type == W_SEED_HIR0_TYPE_I64);
  CHECK(program->blocks[4].block_argument_count == 1u &&
        program->blocks[4].first_block_argument == 0u &&
        program->blocks[6].block_argument_count == 1u &&
        program->blocks[6].first_block_argument == 1u &&
        program->block_arguments[0].owner_block == 4u &&
        program->block_arguments[0].type_index == W_SEED_HIR0_TYPE_I64 &&
        program->block_arguments[1].owner_block == 6u &&
        program->block_arguments[1].type_index == W_SEED_HIR0_TYPE_I64);
  CHECK(program->terminators[2].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[2].target_block == 4u &&
        program->terminators[3].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[3].target_block == 4u &&
        program->terminators[4].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[4].target_block == 6u &&
        program->terminators[5].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[5].target_block == 6u);
  CHECK(edge_value_at(program, 2u) < program->value_count &&
        edge_value_at(program, 3u) < program->value_count &&
        edge_value_at(program, 4u) < program->value_count &&
        edge_value_at(program, 5u) < program->value_count &&
        program->values[edge_value_at(program, 2u)].type_index ==
            W_SEED_HIR0_TYPE_I64 &&
        program->values[edge_value_at(program, 3u)].type_index ==
            W_SEED_HIR0_TYPE_I64 &&
        program->values[edge_value_at(program, 4u)].type_index ==
            W_SEED_HIR0_TYPE_I64 &&
        program->values[edge_value_at(program, 5u)].type_index ==
            W_SEED_HIR0_TYPE_I64);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0);

  const size_t choose = find_bytes(
      artifact, emitted.written.mlir_bytes, "llvm.func internal @w_fn_0(",
      0u);
  const size_t main = find_bytes(
      artifact, emitted.written.mlir_bytes, "llvm.func internal @w_fn_1(",
      choose == SIZE_MAX ? 0u : choose + 1u);
  CHECK(choose != SIZE_MAX && main != SIZE_MAX && choose < main);
  const size_t choose_bytes = main - choose;
  CHECK(contains_bytes(
            artifact + choose, choose_bytes,
            "llvm.func internal @w_fn_0(%buffer: !llvm.ptr, %cursor_address: !llvm.ptr, %p0: i1, %p1: i1, %p2: i64, %p3: i64, %p4: i64) -> i64") &&
        count_bytes(artifact + choose, choose_bytes,
                    "llvm.cond_br") == 2u &&
        contains_bytes(artifact + choose, choose_bytes,
                       "llvm.cond_br %p0, ^w_fn_0_b_1, ^w_fn_0_b_5") &&
        contains_bytes(artifact + choose, choose_bytes,
                       "llvm.cond_br %p1, ^w_fn_0_b_2, ^w_fn_0_b_3") &&
        contains_bytes(artifact + choose, choose_bytes,
                       "^w_fn_0_b_4(%arg0: i64):") &&
        contains_bytes(artifact + choose, choose_bytes,
                       "^w_fn_0_b_6(%arg1: i64):") &&
        contains_bytes(artifact + choose, choose_bytes,
                       "llvm.br ^w_fn_0_b_4(%p2 : i64)") &&
        contains_bytes(artifact + choose, choose_bytes,
                       "llvm.br ^w_fn_0_b_4(%p3 : i64)") &&
        contains_bytes(artifact + choose, choose_bytes,
                       "llvm.br ^w_fn_0_b_6(%arg0 : i64)") &&
        contains_bytes(artifact + choose, choose_bytes,
                       "llvm.br ^w_fn_0_b_6(%p4 : i64)") &&
        contains_bytes(artifact + choose, choose_bytes,
                       "llvm.return %arg1 : i64") &&
        find_bytes(artifact + choose, choose_bytes, "llvm.select", 0u) ==
            SIZE_MAX);
  CHECK(count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.call @w_fn_0") == 3u &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "@w_seed_append_i64") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes, "1,2,3") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "\\31\\2c\\32\\2c\\33\\0a"));

  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(program, &fixture.hir_result,
                                             &selection) ==
            W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_cfg && selection.has_local_calls &&
        selection.has_interpolation && selection.has_bool &&
        selection.maximum_stdout_bytes == 63u);
  const w_seed_mlir0_input windows_input = mlir_input();
  w_seed_mlir0_counts windows_counts;
  w_seed_mlir0_result windows_result;
  CHECK(w_seed_mlir0_measure(&windows_input, &WINDOWS_TARGET,
                             &windows_counts, &windows_result) ==
        W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_emit(
            &windows_input, &WINDOWS_TARGET,
            &(w_seed_mlir0_output){artifact, sizeof(artifact)},
            &windows_result) == W_SEED_MLIR0_OK);
  CHECK(contains_bytes(artifact, windows_result.written.mlir_bytes,
                       "@w_seed_mlir0_buffer() : !llvm.array<64 x i8>") &&
        contains_bytes(artifact, windows_result.written.mlir_bytes,
                       "llvm.getelementptr %buffer[%length]") &&
        contains_bytes(artifact, windows_result.written.mlir_bytes,
                       "llvm.store %output_terminator_zero, %output_terminator_address"));
  return true;
}

static bool test_if_diamond_cfg(void) {
  static const uint8_t source[] =
      "fn serve(isOpen: Bool) {\n"
      "  if isOpen { print(\"Kitchen open\") } else { "
      "print(\"Kitchen closed\") }\n"
      "  print(\"After service\")\n"
      "}\n"
      "fn main() { serve(isOpen: true) serve(isOpen: false) }\n"
      "entry(main)\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(fixture.hir_program.functions[0].block_count == 4u &&
        fixture.hir_program.terminators[0].kind ==
            W_SEED_HIR0_TERMINATOR_BRANCH);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes);
  CHECK(contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.func internal @w_fn_0(%buffer: !llvm.ptr, "
                       "%cursor_address: !llvm.ptr, %p0: i1)"));
  CHECK(contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.cond_br %p0, ^w_fn_0_b_1, ^w_fn_0_b_2"));
  CHECK(count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.br ^w_fn_0_b_3\n") == 2u);
  CHECK(contains_bytes(artifact, emitted.written.mlir_bytes,
                       "\\4b\\69\\74\\63\\68\\65\\6e\\20\\6f\\70\\65\\6e\\0a") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "\\4b\\69\\74\\63\\68\\65\\6e\\20\\63\\6c\\6f\\73\\65\\64\\0a") &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "\\41\\66\\74\\65\\72\\20\\73\\65\\72\\76\\69\\63\\65\\0a") == 1u);
  CHECK(count_bytes(artifact, emitted.written.mlir_bytes,
                     "llvm.call @w_fn_0") == 2u);

  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_cfg && selection.maximum_stdout_bytes == 58u);
  const w_seed_mlir0_input windows_input = mlir_input();
  w_seed_mlir0_counts windows_counts;
  w_seed_mlir0_result windows_result;
  CHECK(w_seed_mlir0_measure(&windows_input, &WINDOWS_TARGET,
                             &windows_counts, &windows_result) ==
        W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_emit(
            &windows_input, &WINDOWS_TARGET,
            &(w_seed_mlir0_output){artifact, sizeof(artifact)},
            &windows_result) == W_SEED_MLIR0_OK);
  CHECK(contains_bytes(artifact, windows_result.written.mlir_bytes,
                       "@w_seed_mlir0_buffer() : !llvm.array<59 x i8>") &&
        contains_bytes(artifact, windows_result.written.mlir_bytes,
                       "llvm.getelementptr %buffer[%length]") &&
        contains_bytes(artifact, windows_result.written.mlir_bytes,
                       "llvm.store %output_terminator_zero, %output_terminator_address"));

  const w_seed_hir0_terminator saved_branch = fixture.hir_terminators[0];
  fixture.hir_terminators[0].target_block = 4u;
  uint8_t rejected_output[W_SEED_MLIR0_MAX_BYTES];
  (void)memset(rejected_output, 0xa1u, sizeof(rejected_output));
  const w_seed_mlir0_result rejected_snapshot = emitted;
  CHECK(w_seed_mlir0_emit(
            &((w_seed_mlir0_input){&fixture.hir_program, &fixture.hir_result,
                                   W_SEED_MLIR0_ARTIFACT_EXECUTABLE}),
            &TARGET, &(w_seed_mlir0_output){rejected_output,
                                            sizeof(rejected_output)},
            &emitted) == W_SEED_MLIR0_INVALID_HIR);
  for (size_t index = 0u; index < sizeof(rejected_output); index += 1u)
    CHECK(rejected_output[index] == 0xa1u);
  CHECK(memcmp(&emitted, &rejected_snapshot, sizeof(emitted)) == 0);
  fixture.hir_terminators[0] = saved_branch;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));

  static const uint8_t no_else[] =
      "fn main() { if true { print(\"Open\") } print(\"After\") }\n"
      "entry(main)\n";
  CHECK(lower_hir(no_else, sizeof(no_else) - 1u));
  CHECK(fixture.hir_program.block_count == 4u &&
        fixture.hir_program.blocks[2].instruction_count == 0u &&
        fixture.hir_program.terminators[0].kind ==
            W_SEED_HIR0_TERMINATOR_BRANCH);
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.cond_br %v") &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.br ^w_fn_0_b_3\n") == 2u &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "\\4f\\70\\65\\6e\\0a") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "\\41\\66\\74\\65\\72\\0a"));
  return true;
}

static bool test_logical_and_diamond(void) {
  static const uint8_t source[] =
      "fn rhs(): Bool { return true }\n"
      "fn allowed(left: Bool): Bool { return left && rhs() }\n"
      "fn main() { let result = allowed(left: true) "
      "print(\"logical ${result}\") }\n"
      "entry(main)\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(fixture.hir_program.function_count == 3u &&
        fixture.hir_program.block_count == 6u &&
        fixture.hir_program.call_count == 3u &&
        fixture.hir_program.block_argument_count == 1u);
  const w_seed_hir0_program *program = &fixture.hir_program;
  const uint32_t function_index = 1u;
  const w_seed_hir0_terminator *branch = &program->terminators[1];
  CHECK(branch->kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        branch->logical_operator == W_SEED_HIR0_LOGICAL_AND &&
        branch->target_block < program->block_count &&
        branch->else_block < program->block_count);
  const w_seed_hir0_block *rhs_block_record =
      &program->blocks[branch->target_block];
  const w_seed_hir0_block *skip_block_record =
      &program->blocks[branch->else_block];
  CHECK(rhs_block_record->owner_function == function_index &&
        skip_block_record->owner_function == function_index &&
        rhs_block_record->terminator_index < program->terminator_count &&
        skip_block_record->terminator_index < program->terminator_count);
  const w_seed_hir0_terminator *rhs_jump =
      &program->terminators[rhs_block_record->terminator_index];
  const w_seed_hir0_terminator *skip_jump =
      &program->terminators[skip_block_record->terminator_index];
  CHECK(rhs_jump->kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        skip_jump->kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        rhs_jump->target_block == skip_jump->target_block &&
        rhs_jump->target_block < program->block_count &&
        edge_value_for(program, rhs_jump) < program->value_count &&
        edge_value_for(program, skip_jump) < program->value_count);
  const uint32_t join_block_index = rhs_jump->target_block;
  const w_seed_hir0_block *join_block_record =
      &program->blocks[join_block_index];
  const uint32_t join_argument_index = join_block_record->first_block_argument;
  CHECK(join_block_record->block_argument_count == 1u &&
        join_argument_index < program->block_argument_count &&
        program->block_arguments[join_argument_index].owner_block ==
            join_block_index &&
        program->block_arguments[join_argument_index].type_index ==
            W_SEED_HIR0_TYPE_BOOL);
  const w_seed_hir0_value *rhs_incoming =
      &program->values[edge_value_for(program, rhs_jump)];
  const w_seed_hir0_value *skip_incoming =
      &program->values[edge_value_for(program, skip_jump)];
  CHECK(rhs_incoming->kind == W_SEED_HIR0_VALUE_CALL_RESULT &&
        rhs_incoming->call_index < program->call_count &&
        skip_incoming->kind == W_SEED_HIR0_VALUE_CONST_BOOL &&
        !skip_incoming->bool_value);
  const w_seed_hir0_call *rhs_call_record =
      &program->calls[rhs_incoming->call_index];
  CHECK(rhs_call_record->callee_identity < program->identity_count &&
        program->identities[rhs_call_record->callee_identity].kind ==
            W_SEED_HIR0_IDENTITY_FUNCTION &&
        rhs_call_record->owner_block == branch->target_block);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes);
  const size_t artifact_length = emitted.written.mlir_bytes;
  char branch_text[160];
  char rhs_label[64];
  char skip_label[64];
  char join_label[80];
  char rhs_call_text[64];
  char rhs_jump_text[96];
  char skip_jump_text[96];
  const int branch_length =
      snprintf(branch_text, sizeof(branch_text),
               "llvm.cond_br %%p0, ^w_fn_%u_b_%u, ^w_fn_%u_b_%u",
               function_index, branch->target_block, function_index,
               branch->else_block);
  const int rhs_label_length =
      snprintf(rhs_label, sizeof(rhs_label), "^w_fn_%u_b_%u:", function_index,
               branch->target_block);
  const int skip_label_length =
      snprintf(skip_label, sizeof(skip_label), "^w_fn_%u_b_%u:", function_index,
               branch->else_block);
  const int join_label_length =
      snprintf(join_label, sizeof(join_label), "^w_fn_%u_b_%u(%%arg%u: i1):",
               function_index, join_block_index, join_argument_index);
  const int rhs_call_length = snprintf(
      rhs_call_text, sizeof(rhs_call_text), "llvm.call @w_fn_%u",
      program->identities[rhs_call_record->callee_identity].target_index);
  const int rhs_jump_length = snprintf(
      rhs_jump_text, sizeof(rhs_jump_text), "llvm.br ^w_fn_%u_b_%u(%%call%u : i1)",
      function_index, join_block_index, rhs_incoming->call_index);
  const int skip_jump_length = snprintf(
      skip_jump_text, sizeof(skip_jump_text), "llvm.br ^w_fn_%u_b_%u(%%v%u : i1)",
      function_index, join_block_index, edge_value_for(program, skip_jump));
  CHECK(branch_length > 0 && (size_t)branch_length < sizeof(branch_text) &&
        rhs_label_length > 0 && (size_t)rhs_label_length < sizeof(rhs_label) &&
        skip_label_length > 0 && (size_t)skip_label_length < sizeof(skip_label) &&
        join_label_length > 0 && (size_t)join_label_length < sizeof(join_label) &&
        rhs_call_length > 0 && (size_t)rhs_call_length < sizeof(rhs_call_text) &&
        rhs_jump_length > 0 && (size_t)rhs_jump_length < sizeof(rhs_jump_text) &&
        skip_jump_length > 0 && (size_t)skip_jump_length < sizeof(skip_jump_text));
  CHECK(contains_bytes(artifact, artifact_length, branch_text));
  CHECK(contains_bytes(artifact, artifact_length, join_label));
  CHECK(count_bytes(artifact, artifact_length, "llvm.br ^w_fn_1_b_4(") == 2u);
  const size_t rhs_block =
      find_bytes(artifact, artifact_length, rhs_label, 0u);
  const size_t skip_block =
      find_bytes(artifact, artifact_length, skip_label, rhs_block);
  const size_t join_block =
      find_bytes(artifact, artifact_length, join_label, 0u);
  const size_t rhs_call =
      find_bytes(artifact, artifact_length, rhs_call_text, rhs_block);
  CHECK(rhs_block != SIZE_MAX && skip_block != SIZE_MAX &&
        join_block != SIZE_MAX && rhs_call != SIZE_MAX &&
        rhs_call > rhs_block && rhs_call < skip_block &&
        find_bytes(artifact, artifact_length, rhs_call_text, skip_block) ==
            SIZE_MAX &&
        find_bytes(artifact, artifact_length, rhs_call_text, join_block) ==
            SIZE_MAX && contains_bytes(artifact, artifact_length, rhs_jump_text) &&
        contains_bytes(artifact, artifact_length, skip_jump_text));
  return true;
}

static bool test_logical_unary_not(void) {
  static const uint8_t source[] =
      "fn negate(flag: Bool): Bool { return !flag }\n"
      "fn main() { let negated = negate(flag: true) "
      "print(\"Unary: ${negated}\") }\n"
      "entry(main)\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  const w_seed_hir0_program *program = &fixture.hir_program;
  uint32_t unary_index = W_SEED_HIR0_NONE;
  for (size_t index = 0u; index < program->value_count; index += 1u)
    if (program->values[index].kind == W_SEED_HIR0_VALUE_UNARY_BOOL) {
      unary_index = (uint32_t)index;
      break;
    }
  CHECK(unary_index != W_SEED_HIR0_NONE);
  const w_seed_hir0_value *unary = &program->values[unary_index];
  CHECK(unary->type_index < program->type_count &&
        program->types[unary->type_index].kind == W_SEED_HIR0_TYPE_BOOL &&
        unary->unary_operator == W_SEED_HIR0_UNARY_NOT &&
        unary->left_value < program->value_count &&
        unary->right_value == W_SEED_HIR0_NONE);
  uint32_t local_call_index = W_SEED_HIR0_NONE;
  for (size_t index = 0u; index < program->call_count; index += 1u) {
    const w_seed_hir0_call *call = &program->calls[index];
    if (call->callee_identity < program->identity_count &&
        program->identities[call->callee_identity].kind ==
            W_SEED_HIR0_IDENTITY_FUNCTION) {
      local_call_index = (uint32_t)index;
      break;
    }
  }
  CHECK(local_call_index != W_SEED_HIR0_NONE &&
        program->calls[local_call_index].argument_count == 1u &&
        program->calls[local_call_index].first_argument <
            program->argument_count &&
        program->arguments[program->calls[local_call_index].first_argument]
                .value_index < program->value_count &&
        program->values[program->arguments[program->calls[local_call_index]
                                                .first_argument]
                            .value_index]
                .kind == W_SEED_HIR0_VALUE_CONST_BOOL &&
        program->values[program->arguments[program->calls[local_call_index]
                                                .first_argument]
                            .value_index]
                .bool_value);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes);
  char xor_prefix[48];
  char call_prefix[64];
  const int xor_length =
      snprintf(xor_prefix, sizeof(xor_prefix), "%%v%u = llvm.xor ",
               unary_index);
  const int call_length = snprintf(
      call_prefix, sizeof(call_prefix), "llvm.call @w_fn_%u",
      program->identities[program->calls[local_call_index].callee_identity]
          .target_index);
  CHECK(xor_length > 0 && (size_t)xor_length < sizeof(xor_prefix) &&
        call_length > 0 && (size_t)call_length < sizeof(call_prefix) &&
        contains_bytes(artifact, emitted.written.mlir_bytes, xor_prefix) &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.mlir.constant(true) : i1") &&
        count_bytes(artifact, emitted.written.mlir_bytes, call_prefix) == 1u);
  return true;
}

static bool test_logical_or_diamond(void) {
  static const uint8_t source[] =
      "fn rhs(): Bool { return true }\n"
      "fn either(left: Bool): Bool { return left || rhs() }\n"
      "fn main() { let result = either(left: false) "
      "print(\"or ${result}\") }\n"
      "entry(main)\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  const w_seed_hir0_program *program = &fixture.hir_program;
  const uint32_t function_index = 1u;
  CHECK(function_index < program->function_count);
  const w_seed_hir0_function *function = &program->functions[function_index];
  CHECK(function->first_block < program->block_count &&
        function->block_count <= program->block_count - function->first_block);
  const w_seed_hir0_terminator *branch = NULL;
  for (size_t ordinal = 0u; ordinal < function->block_count; ordinal += 1u) {
    const size_t block_index = (size_t)function->first_block + ordinal;
    const w_seed_hir0_block *block = &program->blocks[block_index];
    if (block->terminator_index < program->terminator_count &&
        program->terminators[block->terminator_index].kind ==
            W_SEED_HIR0_TERMINATOR_BRANCH) {
      branch = &program->terminators[block->terminator_index];
      break;
    }
  }
  CHECK(branch != NULL && branch->logical_operator == W_SEED_HIR0_LOGICAL_OR &&
        branch->target_block < program->block_count &&
        branch->else_block < program->block_count);
  const w_seed_hir0_block *skip_block_record =
      &program->blocks[branch->target_block];
  const w_seed_hir0_block *rhs_block_record =
      &program->blocks[branch->else_block];
  CHECK(skip_block_record->terminator_index < program->terminator_count &&
        rhs_block_record->terminator_index < program->terminator_count);
  const w_seed_hir0_terminator *skip_jump =
      &program->terminators[skip_block_record->terminator_index];
  const w_seed_hir0_terminator *rhs_jump =
      &program->terminators[rhs_block_record->terminator_index];
  CHECK(skip_jump->kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        rhs_jump->kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        skip_jump->target_block == rhs_jump->target_block &&
        skip_jump->target_block < program->block_count &&
        edge_value_for(program, skip_jump) < program->value_count &&
        edge_value_for(program, rhs_jump) < program->value_count);
  const w_seed_hir0_block *join_block_record =
      &program->blocks[skip_jump->target_block];
  const uint32_t join_argument_index = join_block_record->first_block_argument;
  CHECK(join_block_record->block_argument_count == 1u &&
        join_argument_index < program->block_argument_count &&
        program->block_arguments[join_argument_index].owner_block ==
            skip_jump->target_block &&
        program->block_arguments[join_argument_index].type_index ==
            W_SEED_HIR0_TYPE_BOOL);
  const w_seed_hir0_value *skip_incoming =
      &program->values[edge_value_for(program, skip_jump)];
  const w_seed_hir0_value *rhs_incoming =
      &program->values[edge_value_for(program, rhs_jump)];
  CHECK(skip_incoming->kind == W_SEED_HIR0_VALUE_CONST_BOOL &&
        skip_incoming->bool_value &&
        rhs_incoming->kind == W_SEED_HIR0_VALUE_CALL_RESULT &&
        rhs_incoming->call_index < program->call_count);
  const w_seed_hir0_call *rhs_call_record =
      &program->calls[rhs_incoming->call_index];
  CHECK(rhs_call_record->owner_block == branch->else_block &&
        rhs_call_record->callee_identity < program->identity_count &&
        program->identities[rhs_call_record->callee_identity].kind ==
            W_SEED_HIR0_IDENTITY_FUNCTION);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes);
  char branch_text[160];
  char rhs_label[64];
  char skip_label[64];
  char join_label[80];
  char call_text[64];
  char skip_jump_text[96];
  char rhs_jump_text[96];
  const int branch_length = snprintf(
      branch_text, sizeof(branch_text),
      "llvm.cond_br %%p0, ^w_fn_%u_b_%u, ^w_fn_%u_b_%u", function_index,
      branch->target_block, function_index, branch->else_block);
  const int rhs_label_length =
      snprintf(rhs_label, sizeof(rhs_label), "^w_fn_%u_b_%u:", function_index,
               branch->else_block);
  const int skip_label_length =
      snprintf(skip_label, sizeof(skip_label), "^w_fn_%u_b_%u:", function_index,
               branch->target_block);
  const int join_label_length =
      snprintf(join_label, sizeof(join_label), "^w_fn_%u_b_%u(%%arg%u: i1):",
               function_index, skip_jump->target_block, join_argument_index);
  const int call_length = snprintf(
      call_text, sizeof(call_text), "llvm.call @w_fn_%u",
      program->identities[rhs_call_record->callee_identity].target_index);
  const int skip_jump_length = snprintf(
      skip_jump_text, sizeof(skip_jump_text), "llvm.br ^w_fn_%u_b_%u(%%v%u : i1)",
      function_index, skip_jump->target_block, edge_value_for(program, skip_jump));
  const int rhs_jump_length = snprintf(
      rhs_jump_text, sizeof(rhs_jump_text), "llvm.br ^w_fn_%u_b_%u(%%call%u : i1)",
      function_index, rhs_jump->target_block, rhs_incoming->call_index);
  CHECK(branch_length > 0 && (size_t)branch_length < sizeof(branch_text) &&
        rhs_label_length > 0 && (size_t)rhs_label_length < sizeof(rhs_label) &&
        skip_label_length > 0 && (size_t)skip_label_length < sizeof(skip_label) &&
        join_label_length > 0 && (size_t)join_label_length < sizeof(join_label) &&
        call_length > 0 && (size_t)call_length < sizeof(call_text) &&
        skip_jump_length > 0 && (size_t)skip_jump_length < sizeof(skip_jump_text) &&
        rhs_jump_length > 0 && (size_t)rhs_jump_length < sizeof(rhs_jump_text));
  CHECK(contains_bytes(artifact, emitted.written.mlir_bytes, branch_text) &&
        contains_bytes(artifact, emitted.written.mlir_bytes, join_label) &&
        contains_bytes(artifact, emitted.written.mlir_bytes, skip_jump_text) &&
        contains_bytes(artifact, emitted.written.mlir_bytes, rhs_jump_text));
  const size_t skip_block =
      find_bytes(artifact, emitted.written.mlir_bytes, skip_label, 0u);
  const size_t rhs_block =
      find_bytes(artifact, emitted.written.mlir_bytes, rhs_label, 0u);
  const size_t join_block =
      find_bytes(artifact, emitted.written.mlir_bytes, join_label, 0u);
  const size_t rhs_call =
      find_bytes(artifact, emitted.written.mlir_bytes, call_text, rhs_block);
  CHECK(rhs_block != SIZE_MAX && skip_block != SIZE_MAX &&
        join_block != SIZE_MAX && rhs_call != SIZE_MAX &&
        skip_block < rhs_block &&
        rhs_call > rhs_block && rhs_call < join_block &&
        find_bytes(artifact, emitted.written.mlir_bytes, call_text, join_block) ==
            SIZE_MAX);
  return true;
}

static bool test_logical_nested_diamond(void) {
  static const uint8_t source[] =
      "fn rhs(flag: Bool): Bool { return flag }\n"
      "fn nested(left: Bool): Bool { return left && (false || rhs(flag: true)) }\n"
      "fn main() { let result = nested(left: true) "
      "print(\"nested ${result}\") }\n"
      "entry(main)\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  const w_seed_hir0_program *program = &fixture.hir_program;
  const uint32_t function_index = 1u;
  CHECK(function_index < program->function_count);
  const w_seed_hir0_function *function = &program->functions[function_index];
  CHECK(function->first_block < program->block_count &&
        function->block_count <= program->block_count - function->first_block);
  const w_seed_hir0_terminator *branches[2] = {NULL, NULL};
  size_t branch_count = 0u;
  for (size_t ordinal = 0u; ordinal < function->block_count; ordinal += 1u) {
    const size_t block_index = (size_t)function->first_block + ordinal;
    const w_seed_hir0_block *block = &program->blocks[block_index];
    if (block->terminator_index < program->terminator_count &&
        program->terminators[block->terminator_index].kind ==
            W_SEED_HIR0_TERMINATOR_BRANCH) {
      if (branch_count < 2u)
        branches[branch_count] = &program->terminators[block->terminator_index];
      branch_count += 1u;
    }
  }
  CHECK(branch_count == 2u && branches[0] != NULL && branches[1] != NULL &&
        branches[0]->logical_operator == W_SEED_HIR0_LOGICAL_AND &&
        branches[1]->logical_operator == W_SEED_HIR0_LOGICAL_OR &&
        branches[0]->target_block == branches[1]->owner_block &&
        branches[0]->target_block != branches[0]->else_block);
  const w_seed_hir0_terminator *outer_branch = branches[0];
  const w_seed_hir0_terminator *inner_branch = branches[1];
  const w_seed_hir0_block *inner_skip_block =
      &program->blocks[inner_branch->target_block];
  const w_seed_hir0_block *inner_rhs_block =
      &program->blocks[inner_branch->else_block];
  const w_seed_hir0_block *outer_skip_block =
      &program->blocks[outer_branch->else_block];
  CHECK(inner_skip_block->terminator_index < program->terminator_count &&
        inner_rhs_block->terminator_index < program->terminator_count &&
        outer_skip_block->terminator_index < program->terminator_count);
  const w_seed_hir0_terminator *inner_skip_jump =
      &program->terminators[inner_skip_block->terminator_index];
  const w_seed_hir0_terminator *inner_rhs_jump =
      &program->terminators[inner_rhs_block->terminator_index];
  const w_seed_hir0_terminator *outer_skip_jump =
      &program->terminators[outer_skip_block->terminator_index];
  CHECK(inner_skip_jump->kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        inner_rhs_jump->kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        outer_skip_jump->kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        inner_skip_jump->target_block == inner_rhs_jump->target_block &&
        inner_skip_jump->target_block < program->block_count &&
        outer_skip_jump->target_block < program->block_count &&
        edge_value_for(program, inner_skip_jump) < program->value_count &&
        edge_value_for(program, inner_rhs_jump) < program->value_count &&
        edge_value_for(program, outer_skip_jump) < program->value_count);
  const uint32_t inner_join_index = inner_skip_jump->target_block;
  const uint32_t outer_join_index = outer_skip_jump->target_block;
  const w_seed_hir0_block *inner_join = &program->blocks[inner_join_index];
  const w_seed_hir0_block *outer_join = &program->blocks[outer_join_index];
  CHECK(inner_join->block_argument_count == 1u &&
        outer_join->block_argument_count == 1u &&
        inner_join->first_block_argument < program->block_argument_count &&
        outer_join->first_block_argument < program->block_argument_count &&
        program->block_arguments[inner_join->first_block_argument].owner_block ==
            inner_join_index &&
        program->block_arguments[outer_join->first_block_argument].owner_block ==
            outer_join_index);
  CHECK(inner_join->terminator_index < program->terminator_count);
  const w_seed_hir0_terminator *outer_rhs_jump =
      &program->terminators[inner_join->terminator_index];
  CHECK(outer_rhs_jump->kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        outer_rhs_jump->target_block == outer_join_index &&
        edge_value_for(program, outer_rhs_jump) < program->value_count);
  const w_seed_hir0_value *inner_skip_incoming =
      &program->values[edge_value_for(program, inner_skip_jump)];
  const w_seed_hir0_value *inner_rhs_incoming =
      &program->values[edge_value_for(program, inner_rhs_jump)];
  const w_seed_hir0_value *outer_skip_incoming =
      &program->values[edge_value_for(program, outer_skip_jump)];
  const w_seed_hir0_value *outer_rhs_incoming =
      &program->values[edge_value_for(program, outer_rhs_jump)];
  CHECK(inner_skip_incoming->kind == W_SEED_HIR0_VALUE_CONST_BOOL &&
        inner_skip_incoming->bool_value &&
        inner_rhs_incoming->kind == W_SEED_HIR0_VALUE_CALL_RESULT &&
        inner_rhs_incoming->call_index < program->call_count &&
        outer_skip_incoming->kind == W_SEED_HIR0_VALUE_CONST_BOOL &&
        !outer_skip_incoming->bool_value &&
        outer_rhs_incoming->kind == W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        outer_rhs_incoming->block_argument_index ==
            inner_join->first_block_argument);
  const w_seed_hir0_call *rhs_call_record =
      &program->calls[inner_rhs_incoming->call_index];
  CHECK(rhs_call_record->owner_block == inner_branch->else_block &&
        rhs_call_record->argument_count == 1u &&
        rhs_call_record->first_argument < program->argument_count &&
        program->arguments[rhs_call_record->first_argument].value_index <
            program->value_count &&
        program->values[program->arguments[rhs_call_record->first_argument]
                            .value_index]
                .kind == W_SEED_HIR0_VALUE_CONST_BOOL &&
        program->values[program->arguments[rhs_call_record->first_argument]
                            .value_index]
                .bool_value &&
        rhs_call_record->callee_identity < program->identity_count &&
        program->identities[rhs_call_record->callee_identity].kind ==
            W_SEED_HIR0_IDENTITY_FUNCTION);
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes);
  char inner_rhs_label[64];
  char inner_skip_label[64];
  char inner_join_label[80];
  char outer_rhs_label[64];
  char outer_skip_label[64];
  char outer_join_label[80];
  char call_text[64];
  char inner_skip_jump_text[96];
  char inner_rhs_jump_text[96];
  char outer_skip_jump_text[96];
  char outer_rhs_jump_text[96];
  int length = snprintf(inner_rhs_label, sizeof(inner_rhs_label),
                        "^w_fn_%u_b_%u:", function_index,
                        inner_branch->else_block);
  CHECK(length > 0 && (size_t)length < sizeof(inner_rhs_label));
  length = snprintf(inner_skip_label, sizeof(inner_skip_label),
                    "^w_fn_%u_b_%u:", function_index,
                    inner_branch->target_block);
  CHECK(length > 0 && (size_t)length < sizeof(inner_skip_label));
  length = snprintf(inner_join_label, sizeof(inner_join_label),
                    "^w_fn_%u_b_%u(%%arg%u: i1):", function_index,
                    inner_join_index, inner_join->first_block_argument);
  CHECK(length > 0 && (size_t)length < sizeof(inner_join_label));
  length = snprintf(outer_rhs_label, sizeof(outer_rhs_label),
                    "^w_fn_%u_b_%u:", function_index,
                    outer_branch->target_block);
  CHECK(length > 0 && (size_t)length < sizeof(outer_rhs_label));
  length = snprintf(outer_skip_label, sizeof(outer_skip_label),
                    "^w_fn_%u_b_%u:", function_index,
                    outer_branch->else_block);
  CHECK(length > 0 && (size_t)length < sizeof(outer_skip_label));
  length = snprintf(outer_join_label, sizeof(outer_join_label),
                    "^w_fn_%u_b_%u(%%arg%u: i1):", function_index,
                    outer_join_index, outer_join->first_block_argument);
  CHECK(length > 0 && (size_t)length < sizeof(outer_join_label));
  length = snprintf(
      call_text, sizeof(call_text), "llvm.call @w_fn_%u",
      program->identities[rhs_call_record->callee_identity].target_index);
  CHECK(length > 0 && (size_t)length < sizeof(call_text));
  length = snprintf(inner_skip_jump_text, sizeof(inner_skip_jump_text),
                    "llvm.br ^w_fn_%u_b_%u(%%v%u : i1)", function_index,
                    inner_join_index, edge_value_for(program, inner_skip_jump));
  CHECK(length > 0 && (size_t)length < sizeof(inner_skip_jump_text));
  length = snprintf(inner_rhs_jump_text, sizeof(inner_rhs_jump_text),
                    "llvm.br ^w_fn_%u_b_%u(%%call%u : i1)", function_index,
                    inner_join_index, inner_rhs_incoming->call_index);
  CHECK(length > 0 && (size_t)length < sizeof(inner_rhs_jump_text));
  length = snprintf(outer_skip_jump_text, sizeof(outer_skip_jump_text),
                    "llvm.br ^w_fn_%u_b_%u(%%v%u : i1)", function_index,
                    outer_join_index, edge_value_for(program, outer_skip_jump));
  CHECK(length > 0 && (size_t)length < sizeof(outer_skip_jump_text));
  length = snprintf(outer_rhs_jump_text, sizeof(outer_rhs_jump_text),
                    "llvm.br ^w_fn_%u_b_%u(%%arg%u : i1)", function_index,
                    outer_join_index, inner_join->first_block_argument);
  CHECK(length > 0 && (size_t)length < sizeof(outer_rhs_jump_text));
  const size_t artifact_length = emitted.written.mlir_bytes;
  CHECK(contains_bytes(artifact, artifact_length, inner_skip_jump_text) &&
        contains_bytes(artifact, artifact_length, inner_rhs_jump_text) &&
        contains_bytes(artifact, artifact_length, outer_skip_jump_text) &&
        contains_bytes(artifact, artifact_length, outer_rhs_jump_text) &&
        count_bytes(artifact, artifact_length, call_text) == 1u);
  const size_t outer_rhs_position =
      find_bytes(artifact, artifact_length, outer_rhs_label, 0u);
  const size_t inner_skip_position =
      find_bytes(artifact, artifact_length, inner_skip_label, outer_rhs_position);
  const size_t inner_rhs_position =
      find_bytes(artifact, artifact_length, inner_rhs_label, inner_skip_position);
  const size_t inner_join_position =
      find_bytes(artifact, artifact_length, inner_join_label, inner_rhs_position);
  const size_t outer_skip_position =
      find_bytes(artifact, artifact_length, outer_skip_label, inner_join_position);
  const size_t outer_join_position =
      find_bytes(artifact, artifact_length, outer_join_label, outer_skip_position);
  const size_t call_position =
      find_bytes(artifact, artifact_length, call_text, inner_rhs_position);
  CHECK(outer_rhs_position != SIZE_MAX && inner_skip_position != SIZE_MAX &&
        inner_rhs_position != SIZE_MAX && inner_join_position != SIZE_MAX &&
        outer_skip_position != SIZE_MAX && outer_join_position != SIZE_MAX &&
        call_position != SIZE_MAX && outer_rhs_position < inner_skip_position &&
        inner_skip_position < inner_rhs_position &&
        inner_rhs_position < call_position && call_position < inner_join_position &&
        inner_join_position < outer_skip_position &&
        outer_skip_position < outer_join_position &&
        find_bytes(artifact, artifact_length, call_text, inner_join_position) ==
            SIZE_MAX);
  return true;
}

static bool expect_logical_mlir_invalid(void) {
  const w_seed_mlir0_input input = mlir_input();
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  (void)memset(output, 0x6au, sizeof(output));
  w_seed_mlir0_result result;
  (void)memset(&result, 0x7bu, sizeof(result));
  const w_seed_mlir0_result snapshot = result;
  CHECK(w_seed_mlir0_emit(&input, &TARGET,
                          &(w_seed_mlir0_output){output, sizeof(output)},
                          &result) == W_SEED_MLIR0_INVALID_HIR);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0x6au);
  CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
  return true;
}

static bool test_logical_mlir_adversarial(void) {
  static const uint8_t source[] =
      "fn rhs(flag: Bool): Bool { return !flag }\n"
      "fn allowed(left: Bool): Bool { return left && rhs(flag: false) }\n"
      "fn main() { print(\"invalid\") }\n"
      "entry(main)\n";
  CHECK(lower_hir(source, sizeof(source) - 1u));
  w_seed_hir0_program *program = &fixture.hir_program;
  uint32_t unary_index = W_SEED_HIR0_NONE;
  uint32_t read_index = W_SEED_HIR0_NONE;
  uint32_t logical_jump_index = W_SEED_HIR0_NONE;
  for (size_t index = 0u; index < program->value_count; index += 1u) {
    if (program->values[index].kind == W_SEED_HIR0_VALUE_UNARY_BOOL &&
        unary_index == W_SEED_HIR0_NONE)
      unary_index = (uint32_t)index;
    if (program->values[index].kind ==
            W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        read_index == W_SEED_HIR0_NONE)
      read_index = (uint32_t)index;
  }
  for (size_t index = 0u; index < program->terminator_count; index += 1u)
    if (program->terminators[index].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        edge_value_at(program, index) != W_SEED_HIR0_NONE) {
      logical_jump_index = (uint32_t)index;
      break;
    }
  CHECK(unary_index != W_SEED_HIR0_NONE && read_index != W_SEED_HIR0_NONE &&
        logical_jump_index != W_SEED_HIR0_NONE &&
        program->block_argument_count > 0u);
  const w_seed_hir0_value saved_unary = program->values[unary_index];
  fixture.hir_values[unary_index].unary_operator =
      (w_seed_hir0_unary_operator)1;
  CHECK(expect_logical_mlir_invalid());
  fixture.hir_values[unary_index] = saved_unary;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_block_argument saved_argument =
      program->block_arguments[0];
  fixture.hir_block_arguments[0].type_index = W_SEED_HIR0_TYPE_I64;
  CHECK(expect_logical_mlir_invalid());
  fixture.hir_block_arguments[0] = saved_argument;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_value saved_read = program->values[read_index];
  fixture.hir_values[read_index].block_argument_index = W_SEED_HIR0_NONE;
  CHECK(expect_logical_mlir_invalid());
  fixture.hir_values[read_index] = saved_read;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_terminator saved_jump =
      program->terminators[logical_jump_index];
  const uint32_t saved_jump_edge_value =
      FIXTURE_EDGE_VALUE_SLOT(logical_jump_index);
  FIXTURE_EDGE_VALUE_SLOT(logical_jump_index) =
      W_SEED_HIR0_NONE;
  CHECK(expect_logical_mlir_invalid());
  fixture.hir_terminators[logical_jump_index] = saved_jump;
  FIXTURE_EDGE_VALUE_SLOT(logical_jump_index) = saved_jump_edge_value;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_checked_arithmetic_adversarial(void) {
  static const uint8_t source[] =
      "fn adjust(value: i64): i64 { return value + 1 }\n"
      "fn main() { let result = adjust(value: 5) "
      "print(\"${result}\") }\nentry(main)\n";
  CHECK(lower_hir(source, sizeof(source) - 1u));
  w_seed_hir0_program *program = &fixture.hir_program;
  uint32_t add_index = W_SEED_HIR0_NONE;
  for (size_t index = 0u; index < program->value_count; index += 1u)
    if (program->values[index].kind == W_SEED_HIR0_VALUE_BINARY_I64 &&
        program->values[index].binary_operator == W_SEED_HIR0_BINARY_ADD) {
      add_index = (uint32_t)index;
      break;
    }
  CHECK(add_index != W_SEED_HIR0_NONE);

  const w_seed_hir0_value saved = program->values[add_index];
  fixture.hir_values[add_index].binary_operator = W_SEED_HIR0_BINARY_DIVIDE;
  CHECK(expect_logical_mlir_invalid());
  fixture.hir_values[add_index].binary_operator = W_SEED_HIR0_BINARY_REMAINDER;
  CHECK(expect_logical_mlir_invalid());
  fixture.hir_values[add_index] = saved;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[add_index].left_value = W_SEED_HIR0_NONE;
  CHECK(expect_logical_mlir_invalid());
  fixture.hir_values[add_index] = saved;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[add_index].type_index = W_SEED_HIR0_TYPE_BOOL;
  CHECK(expect_logical_mlir_invalid());
  fixture.hir_values[add_index] = saved;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[add_index].owner_kind = W_SEED_HIR0_VALUE_OWNER_ARGUMENT;
  CHECK(expect_logical_mlir_invalid());
  fixture.hir_values[add_index] = saved;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[add_index].owner_ordinal = 1u;
  CHECK(expect_logical_mlir_invalid());
  fixture.hir_values[add_index] = saved;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_signed_bitwise_artifact(void) {
  static const uint8_t source[] =
      "fn bits(left: i64, right: i64): i64 { "
      "return left | right ^ left & right }\n"
      "fn main() { let result = bits(left: 10, right: 12) "
      "print(\"${result}\") }\nentry(main)\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        count_bytes(artifact, emitted.written.mlir_bytes, "llvm.and ") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes, "llvm.xor ") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes, "llvm.or ") == 1u &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.call @w_fn_0") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "w_seed_checked_bit"));
  return true;
}

static bool test_integer_bitwise_width_artifact(void) {
  static const struct {
    const char *type;
    const char *zero;
    bool is_signed;
    uint16_t bit_width;
  } INTEGERS[] = {
      {"i8", "0_i8", true, 8u},
      {"u8", "0_u8", false, 8u},
      {"i16", "0_i16", true, 16u},
      {"u16", "0_u16", false, 16u},
      {"i32", "0_i32", true, 32u},
      {"u32", "0_u32", false, 32u},
      {"i64", "0_i64", true, 64u},
      {"u64", "0_u64", false, 64u},
      {"Int", "0_i64", true, 64u},
      {"UInt", "0_u64", false, 64u},
  };
  for (size_t index = 0u; index < sizeof(INTEGERS) / sizeof(INTEGERS[0]);
       index += 1u) {
    char source[TEST_SOURCE];
    size_t source_length = 0u;
    char function_source[512];
    const int function_length = snprintf(
        function_source, sizeof(function_source),
        "fn bitwise(left: %s, right: %s): %s { "
        "return left & right | left ^ right }\n",
        INTEGERS[index].type, INTEGERS[index].type,
        INTEGERS[index].type);
    CHECK(function_length > 0 &&
          (size_t)function_length < sizeof(function_source) &&
          append_text(source, sizeof(source), &source_length,
                      function_source));
    char entry_source[160];
    const int entry_length = snprintf(
        entry_source, sizeof(entry_source),
        "entry { let result = bitwise(left: %s, right: %s) "
        "print(\"ok\") }\n",
        INTEGERS[index].zero, INTEGERS[index].zero);
    CHECK(entry_length > 0 && (size_t)entry_length < sizeof(entry_source) &&
          append_text(source, sizeof(source), &source_length, entry_source));

    uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
    w_seed_mlir0_counts counts;
    w_seed_mlir0_result measured;
    w_seed_mlir0_result emitted;
    CHECK(lower_hir((const uint8_t *)source, source_length));
    CHECK(measure_current(&counts, &measured));
    CHECK(emit_current(artifact, sizeof(artifact), &emitted));
    CHECK(counts.mlir_bytes == emitted.written.mlir_bytes);
    CHECK(count_bytes(artifact, emitted.written.mlir_bytes, "llvm.and ") ==
              1u &&
          count_bytes(artifact, emitted.written.mlir_bytes, "llvm.or ") ==
              1u &&
          count_bytes(artifact, emitted.written.mlir_bytes, "llvm.xor ") ==
              1u);
    CHECK(!contains_bytes(artifact, emitted.written.mlir_bytes,
                         "w_seed_checked_bit"));
    if (INTEGERS[index].bit_width == 64u) {
      CHECK(!contains_bytes(artifact, emitted.written.mlir_bytes,
                           "_bitwise_"));
    } else {
      CHECK(count_bytes(artifact, emitted.written.mlir_bytes,
                        "_bitwise_left = llvm.trunc ") == 3u &&
            count_bytes(artifact, emitted.written.mlir_bytes,
                        "_bitwise_right = llvm.trunc ") == 3u);
      CHECK(count_bytes(artifact, emitted.written.mlir_bytes,
                        "_bitwise_raw = llvm.and ") == 1u &&
            count_bytes(artifact, emitted.written.mlir_bytes,
                        "_bitwise_raw = llvm.or ") == 1u &&
            count_bytes(artifact, emitted.written.mlir_bytes,
                        "_bitwise_raw = llvm.xor ") == 1u);
      CHECK(count_bytes(artifact, emitted.written.mlir_bytes,
                        INTEGERS[index].is_signed ? "llvm.sext "
                                                  : "llvm.zext ") == 3u &&
            count_bytes(artifact, emitted.written.mlir_bytes,
                        INTEGERS[index].is_signed ? "llvm.zext "
                                                  : "llvm.sext ") == 0u);
    }
  }
  return true;
}

static bool test_signed_bit_not_artifact(void) {
  static const uint8_t source[] =
      "fn invert(value: i64): i64 { return ~value }\n"
      "fn main() { let result = invert(value: 10) "
      "print(\"${result}\") }\nentry(main)\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "_bit_not_mask = llvm.mlir.constant(-1 : i64)") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes, "llvm.xor ") == 1u &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.call @w_fn_0") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "w_seed_checked_subtract_i64"));
  return true;
}

static bool test_checked_shift_artifact(void) {
  static const uint8_t source[] =
      "fn shift_i8(value: i8, count: UInt): i8 { return value >> count }\n"
      "fn shift_u8(value: u8, count: UInt): u8 { return value << count }\n"
      "fn shift_i16(value: i16, count: UInt): i16 { return value << count }\n"
      "fn shift_u16(value: u16, count: UInt): u16 { return value >> count }\n"
      "fn shift_i32(value: i32, count: UInt): i32 { return value >> count }\n"
      "fn shift_u32(value: u32, count: UInt): u32 { return value << count }\n"
      "fn shift_i64(value: i64, count: UInt): i64 { return value << count }\n"
      "fn shift_u64(value: u64, count: UInt): u64 { return value >> count }\n"
      "fn shift_int(value: Int, count: UInt): Int { return value >> count }\n"
      "fn shift_uint(value: UInt, count: UInt): UInt { return value << count }\n"
      "entry { let a = shift_i8(value: -64_i8, count: 2_u64) "
      "let b = shift_u8(value: 64_u8, count: 1_u64) "
      "let c = shift_i16(value: -16384_i16, count: 1_u64) "
      "let d = shift_u16(value: 32768_u16, count: 2_u64) "
      "let e = shift_i32(value: -1073741824_i32, count: 2_u64) "
      "let f = shift_u32(value: 1073741824_u32, count: 1_u64) "
      "let g = shift_i64(value: -4611686018427387904_i64, count: 1_u64) "
      "let h = shift_u64(value: 9223372036854775808_u64, count: 2_u64) "
      "let i = shift_int(value: -4611686018427387904_i64, count: 2_u64) "
      "let j = shift_uint(value: 4611686018427387904_u64, count: 1_u64) "
      "print(\"${a}/${b}/${c}/${d}/${e}/${f}/${g}/${h}/${i}/${j}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.func internal @w_seed_checked_shift_left(") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.func internal @w_seed_checked_shift_right(") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.call @w_seed_checked_shift_left(") == 5u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.call @w_seed_checked_shift_right(") == 5u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.icmp \"uge\" %count, %width : i64") == 2u &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.ashr %left, %count : i64") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.lshr %left, %count : i64") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.shl %left, %count : i64") &&
        /* Each narrow width appears twice in shift calls and once in the
           checked negation used to materialize its signed negative witness. */
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "_checked_width = llvm.mlir.constant(8 : i64)") == 3u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "_checked_width = llvm.mlir.constant(16 : i64)") == 3u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "_checked_width = llvm.mlir.constant(32 : i64)") == 3u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "_checked_width = llvm.mlir.constant(64 : i64)") == 4u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "_checked_signed = llvm.mlir.constant(true)") == 5u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "_checked_signed = llvm.mlir.constant(false)") == 5u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "_checked_left = llvm.sext") == 3u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "_checked_left = llvm.zext") == 3u);

  static const uint8_t no_shift_source[] =
      "entry { print(\"No checked shift\") }\n";
  CHECK(lower_hir(no_shift_source, sizeof(no_shift_source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_checked_shift_left") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes,
                        "@w_seed_checked_shift_right"));
  return true;
}

static bool test_checked_power_artifact(void) {
  static const uint8_t source[] =
      "fn signed(base: Int, exponent: UInt): Int { return base ** exponent }\n"
      "fn unsigned(base: UInt, exponent: UInt): UInt { return base ** exponent }\n"
      "entry { let a = signed(base: -3, exponent: 3_u64) "
      "let b = unsigned(base: 2_u64, exponent: 10_u64) "
      "let c = signed(base: 0, exponent: 0_u64) "
      "print(\"${a}/${b}/${c}\") }\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.func internal @w_seed_checked_power_i64") == 1u &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.func internal @w_seed_checked_power_u64") == 1u &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.intr.smul.with.overflow") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.intr.umul.with.overflow") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.lshr %remaining, %one : i64") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.call @w_seed_checked_power_i64") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.call @w_seed_checked_power_u64"));
  return true;
}

static bool expect_sequence_unsupported(void) {
  const w_seed_mlir0_input input = mlir_input();
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  (void)memset(output, 0x91u, sizeof(output));
  w_seed_mlir0_counts counts;
  (void)memset(&counts, 0x72u, sizeof(counts));
  const w_seed_mlir0_counts counts_snapshot = counts;
  w_seed_mlir0_result result;
  (void)memset(&result, 0x82u, sizeof(result));
  const w_seed_mlir0_result result_snapshot = result;
  CHECK(w_seed_mlir0_measure(&input, &TARGET, &counts, &result) ==
        W_SEED_MLIR0_UNSUPPORTED);
  CHECK(memcmp(&counts, &counts_snapshot, sizeof(counts)) == 0);
  CHECK(memcmp(&result, &result_snapshot, sizeof(result)) == 0);
  CHECK(w_seed_mlir0_emit(&input, &TARGET,
                         &(w_seed_mlir0_output){output, sizeof(output)},
                         &result) == W_SEED_MLIR0_UNSUPPORTED);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0x91u);
  CHECK(memcmp(&result, &result_snapshot, sizeof(result)) == 0);
  return true;
}

static bool test_linear_sequence(void) {
  static const uint8_t mixed[] =
      "fn serve() {\n"
      "  let message = \"Table 42 remains open\"\n"
      "  print(message)\n"
      "  print(\"Kitchen is ready\")\n"
      "}\nentry(serve)\n";
  static const uint8_t direct[] =
      "fn serve() {\n"
      "  print(\"Table 42 remains open\")\n"
      "  print(\"Kitchen is ready\")\n"
      "}\nentry(serve)\n";
  uint8_t mixed_artifact[W_SEED_MLIR0_MAX_BYTES];
  uint8_t direct_artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_counts mixed_counts;
  w_seed_mlir0_counts direct_counts;
  w_seed_mlir0_result mixed_result;
  w_seed_mlir0_result direct_result;
  CHECK(lower_hir(mixed, sizeof(mixed) - 1u));
  CHECK(fixture.hir_program.instruction_count == 3u &&
        fixture.hir_program.binding_count == 1u &&
        fixture.hir_program.call_count == 2u &&
        fixture.hir_program.values[0].kind == W_SEED_HIR0_VALUE_CONST_STRING &&
        fixture.hir_program.values[1].kind == W_SEED_HIR0_VALUE_BINDING_READ &&
        fixture.hir_program.values[2].kind == W_SEED_HIR0_VALUE_CONST_STRING);
  CHECK(measure_current(&mixed_counts, &mixed_result));
  CHECK(mixed_counts.mlir_bytes < W_SEED_MLIR0_MAX_BYTES);
  CHECK(emit_current(mixed_artifact, sizeof(mixed_artifact), &mixed_result));
  CHECK(contains_bytes(
      mixed_artifact, mixed_counts.mlir_bytes,
      "\\54\\61\\62\\6c\\65\\20\\34\\32\\20\\72\\65\\6d\\61\\69\\6e\\73\\20\\6f\\70\\65\\6e\\0a"
      "\\4b\\69\\74\\63\\68\\65\\6e\\20\\69\\73\\20\\72\\65\\61\\64\\79\\0a"));
  CHECK(contains_bytes(mixed_artifact, mixed_counts.mlir_bytes,
                       "!llvm.array<39 x i8>"));

  uint8_t short_sequence_output[W_SEED_MLIR0_MAX_BYTES];
  (void)memset(short_sequence_output, 0x6eu, sizeof(short_sequence_output));
  w_seed_mlir0_result short_sequence_result;
  (void)memset(&short_sequence_result, 0x7fu,
               sizeof(short_sequence_result));
  const w_seed_mlir0_result short_sequence_snapshot = short_sequence_result;
  CHECK(w_seed_mlir0_emit(
            &(w_seed_mlir0_input){&fixture.hir_program, &fixture.hir_result,
                                  W_SEED_MLIR0_ARTIFACT_EXECUTABLE},
            &TARGET,
            &(w_seed_mlir0_output){short_sequence_output,
                                   mixed_counts.mlir_bytes - 1u},
            &short_sequence_result) == W_SEED_MLIR0_CAPACITY);
  for (size_t index = 0u; index < sizeof(short_sequence_output); index += 1u)
    CHECK(short_sequence_output[index] == 0x6eu);
  CHECK(memcmp(&short_sequence_result, &short_sequence_snapshot,
               sizeof(short_sequence_result)) == 0);

  CHECK(lower_hir(direct, sizeof(direct) - 1u));
  CHECK(fixture.hir_program.instruction_count == 2u &&
        fixture.hir_program.binding_count == 0u &&
        fixture.hir_program.call_count == 2u);
  CHECK(measure_current(&direct_counts, &direct_result));
  CHECK(emit_current(direct_artifact, sizeof(direct_artifact),
                     &direct_result));
  CHECK(mixed_counts.mlir_bytes == direct_counts.mlir_bytes &&
        memcmp(mixed_artifact, direct_artifact, mixed_counts.mlir_bytes) == 0 &&
        memcmp(mixed_result.mlir_sha256, direct_result.mlir_sha256,
               sizeof(mixed_result.mlir_sha256)) == 0);

  static const uint8_t repeated[] =
      "fn main() {\n"
      "  let message = \"x\"\n"
      "  print(message)\n"
      "  print(message)\n"
      "}\nentry(main)\n";
  CHECK(lower_hir(repeated, sizeof(repeated) - 1u));
  CHECK(fixture.hir_program.instruction_count == 3u &&
        fixture.hir_program.binding_count == 1u &&
        fixture.hir_program.call_count == 2u);
  w_seed_mlir0_counts repeated_counts;
  w_seed_mlir0_result repeated_result;
  CHECK(measure_current(&repeated_counts, &repeated_result));
  CHECK(repeated_result.required.mlir_bytes == repeated_counts.mlir_bytes);
  (void)memset(mixed_artifact, 0x5au, sizeof(mixed_artifact));
  CHECK(emit_current(mixed_artifact, sizeof(mixed_artifact), &repeated_result));
  CHECK(contains_bytes(mixed_artifact, repeated_counts.mlir_bytes,
                       "\\78\\0a\\78\\0a") &&
        contains_bytes(mixed_artifact, repeated_counts.mlir_bytes,
                       "!llvm.array<4 x i8>"));

  char source[TEST_SOURCE];
  CHECK(build_binding_call_source(
      source, sizeof(source), W_SEED_NATIVE_SUBSET0_MAX_PAYLOAD - 1u, 16u));
  CHECK(lower_hir((const uint8_t *)source, strlen(source)));
  CHECK(fixture.hir_program.instruction_count == 17u &&
        fixture.hir_program.binding_count == 1u &&
        fixture.hir_program.call_count == 16u);
  w_seed_mlir0_counts stdout_limit_counts;
  w_seed_mlir0_result stdout_limit_measured;
  CHECK(measure_current(&stdout_limit_counts, &stdout_limit_measured));
  CHECK(stdout_limit_counts.mlir_bytes < W_SEED_MLIR0_MAX_BYTES);
  uint8_t stdout_limit_artifact[W_SEED_MLIR0_MAX_BYTES + 1u];
  (void)memset(stdout_limit_artifact, 0x4cu, sizeof(stdout_limit_artifact));
  w_seed_mlir0_result stdout_limit_short;
  (void)memset(&stdout_limit_short, 0x5du, sizeof(stdout_limit_short));
  const w_seed_mlir0_result stdout_limit_short_snapshot = stdout_limit_short;
  const w_seed_mlir0_input stdout_limit_input = mlir_input();
  CHECK(w_seed_mlir0_emit(
            &stdout_limit_input, &TARGET,
            &(w_seed_mlir0_output){stdout_limit_artifact,
                                   stdout_limit_counts.mlir_bytes - 1u},
            &stdout_limit_short) == W_SEED_MLIR0_CAPACITY);
  for (size_t index = 0u; index < sizeof(stdout_limit_artifact); index += 1u)
    CHECK(stdout_limit_artifact[index] == 0x4cu);
  CHECK(memcmp(&stdout_limit_short, &stdout_limit_short_snapshot,
               sizeof(stdout_limit_short)) == 0);
  w_seed_mlir0_result stdout_limit_emitted;
  (void)memset(stdout_limit_artifact, 0x6eu, sizeof(stdout_limit_artifact));
  CHECK(w_seed_mlir0_emit(
            &stdout_limit_input, &TARGET,
            &(w_seed_mlir0_output){stdout_limit_artifact,
                                   stdout_limit_counts.mlir_bytes},
            &stdout_limit_emitted) == W_SEED_MLIR0_OK);
  CHECK(stdout_limit_emitted.status == W_SEED_MLIR0_OK &&
        stdout_limit_emitted.required.mlir_bytes ==
            stdout_limit_counts.mlir_bytes &&
        stdout_limit_emitted.written.mlir_bytes ==
            stdout_limit_counts.mlir_bytes &&
        memcmp(stdout_limit_emitted.mlir_sha256,
               stdout_limit_measured.mlir_sha256,
               sizeof(stdout_limit_emitted.mlir_sha256)) == 0 &&
        stdout_limit_artifact[stdout_limit_counts.mlir_bytes] == 0x6eu);
  CHECK(contains_bytes(stdout_limit_artifact, stdout_limit_counts.mlir_bytes,
                       "!llvm.array<4096 x i8>"));

  CHECK(build_binding_interpolation_source(
      source, sizeof(source), W_SEED_NATIVE_SUBSET0_MAX_PAYLOAD - 1u, 16u));
  CHECK(lower_hir((const uint8_t *)source, strlen(source)));
  CHECK(measure_current(&stdout_limit_counts, &stdout_limit_measured));
  CHECK(emit_current(stdout_limit_artifact, sizeof(stdout_limit_artifact),
                     &stdout_limit_emitted));
  CHECK(contains_bytes(stdout_limit_artifact, stdout_limit_counts.mlir_bytes,
                       "!llvm.array<4096 x i8>"));

  CHECK(build_binding_interpolation_source(
      source, sizeof(source), W_SEED_NATIVE_SUBSET0_MAX_PAYLOAD, 17u));
  CHECK(lower_hir((const uint8_t *)source, strlen(source)));
  CHECK(expect_sequence_unsupported());

  CHECK(build_call_source(source, sizeof(source),
                          W_SEED_NATIVE_SUBSET0_MAX_CALLS));
  CHECK(lower_hir((const uint8_t *)source, strlen(source)));
  CHECK(fixture.hir_program.instruction_count == 32u &&
        fixture.hir_program.call_count == 32u &&
        fixture.hir_program.binding_count == 0u);
  w_seed_mlir0_counts limit_counts;
  w_seed_mlir0_result limit_result;
  CHECK(measure_current(&limit_counts, &limit_result));
  CHECK(limit_result.required.mlir_bytes < W_SEED_MLIR0_MAX_BYTES);
  CHECK(emit_current(mixed_artifact, sizeof(mixed_artifact), &limit_result));
  CHECK(contains_bytes(mixed_artifact, limit_counts.mlir_bytes,
                       "!llvm.array<64 x i8>"));

  CHECK(build_call_source(source, sizeof(source),
                          W_SEED_NATIVE_SUBSET0_MAX_CALLS + 1u));
  CHECK(lower_hir((const uint8_t *)source, strlen(source)));
  CHECK(fixture.hir_program.instruction_count == 33u &&
        fixture.hir_program.call_count == 33u);
  CHECK(expect_sequence_unsupported());

  CHECK(build_binding_call_source(
      source, sizeof(source), W_SEED_NATIVE_SUBSET0_MAX_PAYLOAD, 17u));
  CHECK(lower_hir((const uint8_t *)source, strlen(source)));
  CHECK(fixture.hir_program.instruction_count == 18u &&
        fixture.hir_program.binding_count == 1u &&
        fixture.hir_program.call_count == 17u);
  CHECK(expect_sequence_unsupported());

  static const uint8_t unused[] =
      "fn main() {\n"
      "  let unused = \"x\"\n"
      "  print(\"y\")\n"
      "}\nentry(main)\n";
  CHECK(lower_hir(unused, sizeof(unused) - 1u));
  CHECK(fixture.hir_program.binding_count == 1u &&
        fixture.hir_program.call_count == 1u);
  CHECK(expect_sequence_unsupported());
  return true;
}

static bool test_capacity_and_all_or_nothing(void) {
  static const uint8_t source[] =
      "fn main() { print(\"Hello, world!\") }\nentry(main)\n";
  CHECK(lower_hir(source, sizeof(source) - 1u));
  const w_seed_mlir0_input input = mlir_input();
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  CHECK(measure_current(&counts, &measured));
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_result result;
  (void)memset(output, 0x1cu, sizeof(output));
  CHECK(w_seed_mlir0_emit(
            &input, &TARGET,
            &(w_seed_mlir0_output){output, counts.mlir_bytes}, &result) ==
        W_SEED_MLIR0_OK);
  CHECK(result.required.mlir_bytes == counts.mlir_bytes &&
        result.written.mlir_bytes == counts.mlir_bytes &&
        output[counts.mlir_bytes] == 0x1cu);
  (void)memset(output, 0xc3u, sizeof(output));
  (void)memset(&result, 0x2bu, sizeof(result));
  const w_seed_mlir0_result snapshot = result;
  CHECK(!emit_current(output, counts.mlir_bytes - 1u, &result));
  CHECK(w_seed_mlir0_emit(&input, &TARGET,
                          &(w_seed_mlir0_output){output,
                                                 counts.mlir_bytes - 1u},
                          &result) == W_SEED_MLIR0_CAPACITY);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0xc3u);
  CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
  CHECK(w_seed_mlir0_emit(&input, &TARGET, NULL, &result) ==
        W_SEED_MLIR0_CAPACITY);
  CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
  CHECK(w_seed_mlir0_emit(&input, &TARGET,
                         &(w_seed_mlir0_output){NULL, 0u}, &result) ==
        W_SEED_MLIR0_CAPACITY);
  CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
  CHECK(w_seed_mlir0_measure(&input, &TARGET, NULL, &result) ==
        W_SEED_MLIR0_INVALID_HIR);
  CHECK(w_seed_mlir0_measure(&input, &TARGET, &counts, NULL) ==
        W_SEED_MLIR0_INVALID_HIR);
  return true;
}

typedef struct {
  void *address;
  size_t bytes;
} alias_case;

static bool test_aliases(void) {
  static const uint8_t source[] =
      "fn serve() { let message = \"Table 42 remains open\" "
      "print(message) }\nentry(serve)\n";
  CHECK(lower_hir(source, sizeof(source) - 1u));
  const w_seed_mlir0_input input = mlir_input();
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  uint8_t output_snapshot[W_SEED_MLIR0_MAX_BYTES];
  uint8_t input_range_snapshot[TEST_RECEIPT];
  w_seed_mlir0_result result;
  (void)memset(&result, 0x4au, sizeof(result));
  const w_seed_mlir0_result result_snapshot = result;
  const alias_case ranges[] = {
      {(void *)&input, sizeof(input)},
      {(void *)&fixture.hir_program, sizeof(fixture.hir_program)},
      {(void *)&fixture.hir_result, sizeof(fixture.hir_result)},
      {(void *)fixture.hir_modules, sizeof(fixture.hir_modules)},
      {(void *)fixture.hir_identities, sizeof(fixture.hir_identities)},
      {(void *)fixture.hir_types, sizeof(fixture.hir_types)},
      {(void *)fixture.hir_enums, sizeof(fixture.hir_enums)},
      {(void *)fixture.hir_enum_cases, sizeof(fixture.hir_enum_cases)},
      {(void *)fixture.hir_functions, sizeof(fixture.hir_functions)},
      {(void *)fixture.hir_parameters, sizeof(fixture.hir_parameters)},
      {(void *)fixture.hir_blocks, sizeof(fixture.hir_blocks)},
      {(void *)fixture.hir_block_arguments,
       sizeof(fixture.hir_block_arguments)},
      {(void *)fixture.hir_edge_arguments,
       sizeof(fixture.hir_edge_arguments)},
      {(void *)fixture.hir_switch_edges,
       sizeof(fixture.hir_switch_edges)},
      {(void *)fixture.hir_instructions, sizeof(fixture.hir_instructions)},
      {(void *)fixture.hir_bindings, sizeof(fixture.hir_bindings)},
      {(void *)fixture.hir_calls, sizeof(fixture.hir_calls)},
      {(void *)fixture.hir_host_parameters,
       sizeof(fixture.hir_host_parameters)},
      {(void *)fixture.hir_arguments, sizeof(fixture.hir_arguments)},
      {(void *)fixture.hir_requirements, sizeof(fixture.hir_requirements)},
      {(void *)fixture.hir_values, sizeof(fixture.hir_values)},
      {(void *)fixture.hir_terminators, sizeof(fixture.hir_terminators)},
      {(void *)fixture.hir_entries, sizeof(fixture.hir_entries)},
      {(void *)fixture.hir_text, sizeof(fixture.hir_text)},
      {(void *)fixture.hir_value_bytes, sizeof(fixture.hir_value_bytes)},
      {(void *)fixture.hir_receipt, sizeof(fixture.hir_receipt)},
  };
  for (size_t index = 0u; index < sizeof(ranges) / sizeof(ranges[0]);
       index += 1u) {
    CHECK(ranges[index].bytes <= sizeof(input_range_snapshot));
    (void)memcpy(input_range_snapshot, ranges[index].address,
                 ranges[index].bytes);
    (void)memset(output, 0x7eu, sizeof(output));
    (void)memcpy(output_snapshot, output, sizeof(output_snapshot));
    CHECK(w_seed_mlir0_emit(
              &input, &TARGET,
              &(w_seed_mlir0_output){(uint8_t *)ranges[index].address,
                                     ranges[index].bytes},
              &result) == W_SEED_MLIR0_ALIAS);
    CHECK(memcmp(ranges[index].address, input_range_snapshot,
                 ranges[index].bytes) == 0);
    CHECK(memcmp(output, output_snapshot, sizeof(output)) == 0);
    CHECK(memcmp(&result, &result_snapshot, sizeof(result)) == 0);
  }

  union {
    w_seed_mlir0_counts counts;
    w_seed_mlir0_result result;
  } records;
  (void)memset(&records, 0x63u, sizeof(records));
  CHECK(w_seed_mlir0_measure(
            &input, &TARGET, (w_seed_mlir0_counts *)(void *)&records,
            (w_seed_mlir0_result *)(void *)&records) == W_SEED_MLIR0_ALIAS);
  for (size_t index = 0u; index < sizeof(records); index += 1u)
    CHECK(((const uint8_t *)(const void *)&records)[index] == 0x63u);
  CHECK(w_seed_mlir0_measure(
            &input, &TARGET, (w_seed_mlir0_counts *)(void *)&fixture.hir_text,
            &result) == W_SEED_MLIR0_ALIAS);

  struct {
    uint8_t prefix[8];
    w_seed_mlir0_target target;
    uint8_t suffix[W_SEED_MLIR0_MAX_BYTES];
  } target_range;
  (void)memset(&target_range, 0x2du, sizeof(target_range));
  target_range.target = TARGET;
  CHECK(w_seed_mlir0_emit(
            &input, &target_range.target,
            &(w_seed_mlir0_output){target_range.prefix,
                                   sizeof(target_range.prefix) +
                                       sizeof(target_range.target)},
            &result) == W_SEED_MLIR0_ALIAS);

  (void)memset(output, 0x7eu, sizeof(output));
  CHECK(w_seed_mlir0_emit(
            &input, &TARGET,
            &(w_seed_mlir0_output){(uint8_t *)&result, sizeof(result)},
            &result) == W_SEED_MLIR0_ALIAS);
  CHECK(memcmp(&result, &result_snapshot, sizeof(result)) == 0);
  return true;
}

static bool test_invalid_hir_and_target(void) {
  static const uint8_t source[] =
      "fn main() { print(\"Hello, world!\") }\nentry(main)\n";
  CHECK(lower_hir(source, sizeof(source) - 1u));
  const w_seed_mlir0_input input = mlir_input();
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_result result;
  (void)memset(output, 0x91u, sizeof(output));
  (void)memset(&result, 0x82u, sizeof(result));
  const w_seed_mlir0_result snapshot = result;
  w_seed_hir0_result forged_result = fixture.hir_result;
  forged_result.semantic_digest[0] ^= 1u;
  const w_seed_mlir0_input forged_input = {
      &fixture.hir_program, &forged_result,
      W_SEED_MLIR0_ARTIFACT_EXECUTABLE};
  CHECK(w_seed_mlir0_emit(
            &forged_input, &TARGET,
            &(w_seed_mlir0_output){output, sizeof(output)}, &result) ==
        W_SEED_MLIR0_INVALID_HIR);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0x91u);
  CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);

  static const uint8_t binding[] =
      "fn serve() { let message = \"Hello\" print(message) }\n"
      "entry(serve)\n";
  CHECK(lower_hir(binding, sizeof(binding) - 1u));
  CHECK(fixture.hir_program.binding_count == 1u);
  CHECK(fixture.hir_instructions[0].result_type <
        fixture.hir_program.type_count);
  CHECK(fixture.hir_types[fixture.hir_instructions[0].result_type].kind ==
        W_SEED_HIR0_TYPE_UNIT);
  const w_seed_hir0_instruction saved_instruction = fixture.hir_instructions[0];
  fixture.hir_instructions[0].result_type =
      (uint32_t)fixture.hir_program.type_count;
  const w_seed_mlir0_input forged_instruction_input = mlir_input();
  (void)memset(output, 0x91u, sizeof(output));
  CHECK(w_seed_mlir0_measure(&forged_instruction_input, &TARGET,
                             &(w_seed_mlir0_counts){0u}, &result) ==
        W_SEED_MLIR0_INVALID_HIR);
  CHECK(w_seed_mlir0_emit(
            &forged_instruction_input, &TARGET,
            &(w_seed_mlir0_output){output, sizeof(output)}, &result) ==
        W_SEED_MLIR0_INVALID_HIR);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0x91u);
  fixture.hir_instructions[0] = saved_instruction;

  const w_seed_hir0_entry saved_entry = fixture.hir_entries[0];
  fixture.hir_entries[0].target_function = W_SEED_HIR0_NONE;
  CHECK(w_seed_mlir0_measure(
            &input, &TARGET, &(w_seed_mlir0_counts){0u}, &result) ==
        W_SEED_MLIR0_INVALID_HIR);
  fixture.hir_entries[0] = saved_entry;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));

  const w_seed_mlir0_target unsupported = {
      W_SEED_MLIR0_TARGET_UNSUPPORTED};
  CHECK(!w_seed_mlir0_target_is_supported(&unsupported));
  CHECK(w_seed_mlir0_measure(&input, &unsupported,
                             &(w_seed_mlir0_counts){0u}, &result) ==
        W_SEED_MLIR0_UNSUPPORTED);
  CHECK(w_seed_mlir0_emit(
            &input, &unsupported,
            &(w_seed_mlir0_output){output, sizeof(output)}, &result) ==
        W_SEED_MLIR0_UNSUPPORTED);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0x91u);
  CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
  return true;
}

static bool test_valid_hir_outside_subset(void) {
  static const uint8_t source[] =
      "fn main() { noop() }\nentry(main)\n";
  CHECK(lower_hir(source, sizeof(source) - 1u));
  const w_seed_mlir0_input input = mlir_input();
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_result result;
  (void)memset(output, 0x33u, sizeof(output));
  (void)memset(&result, 0x44u, sizeof(result));
  const w_seed_mlir0_result snapshot = result;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  CHECK(w_seed_mlir0_measure(&input, &TARGET, &(w_seed_mlir0_counts){0u},
                             &result) == W_SEED_MLIR0_UNSUPPORTED);
  CHECK(w_seed_mlir0_emit(
            &input, &TARGET,
            &(w_seed_mlir0_output){output, sizeof(output)}, &result) ==
        W_SEED_MLIR0_UNSUPPORTED);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0x33u);
  CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
  return true;
}

static bool test_typed_propagation_mlir(void) {
  static const uint8_t source[] =
      "enum Failure: Error { denied }\n"
      "fn leaf(): i64 throws Failure { throw .denied }\n"
      "fn relay(): i64 throws Failure { return try leaf() }\n"
      "entry { }\n";
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(fixture.hir_program.function_count == 3u &&
        fixture.hir_program.call_count == 1u &&
        fixture.hir_program.terminator_count == 5u);

  w_seed_native_subset0_typed_propagation selection;
  CHECK(w_seed_native_subset0_select_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(w_seed_native_subset0_verify_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &selection));
  const w_seed_native_subset0_typed_propagation saved_selection = selection;
  selection.error_case_index = 1u;
  CHECK(!w_seed_native_subset0_verify_typed_propagation(
      &fixture.hir_program, &fixture.hir_result, &selection));
  selection = saved_selection;

  const w_seed_mlir0_input ordinary = mlir_input();
  w_seed_mlir0_counts ordinary_counts = {0u};
  w_seed_mlir0_result ordinary_result;
  (void)memset(&ordinary_result, 0xa1, sizeof(ordinary_result));
  const w_seed_mlir0_result ordinary_snapshot = ordinary_result;
  CHECK(w_seed_mlir0_measure(&ordinary, &TARGET, &ordinary_counts,
                             &ordinary_result) == W_SEED_MLIR0_UNSUPPORTED);
  CHECK(memcmp(&ordinary_result, &ordinary_snapshot,
               sizeof(ordinary_result)) == 0);

  w_seed_mlir0_typed_propagation_counts counts;
  w_seed_mlir0_typed_propagation_result measured;
  CHECK(w_seed_mlir0_measure_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &TARGET, &counts,
            &measured) == W_SEED_MLIR0_OK);
  CHECK(counts.mlir_bytes > 0u &&
        counts.mlir_bytes < W_SEED_MLIR0_MAX_BYTES &&
        counts.function_count == 2u && counts.invoke_count == 1u &&
        counts.cleanup_count == 0u &&
        counts.carrier_field_count ==
            W_SEED_MLIR0_TYPED_PROPAGATION_CARRIER_FIELDS &&
        measured.required.mlir_bytes == counts.mlir_bytes &&
        measured.written.mlir_bytes == 0u);

  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_typed_propagation_result emitted;
  CHECK(w_seed_mlir0_emit_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &TARGET,
            &(w_seed_mlir0_typed_propagation_output){artifact, sizeof(artifact)},
            &emitted) == W_SEED_MLIR0_OK);
  CHECK(emitted.required.mlir_bytes == counts.mlir_bytes &&
        emitted.written.mlir_bytes == counts.mlir_bytes &&
        w_seed_mlir0_verify_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &TARGET, artifact,
            emitted.written.mlir_bytes, &emitted));
  CHECK(contains_bytes(
            artifact, emitted.written.mlir_bytes,
            "// " W_SEED_MLIR0_TYPED_PROPAGATION_SCHEMA_VERSION "\n") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "!llvm.struct<(i1, i64)>") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.call @w_seed_typed_leaf()") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.extractvalue %relay_call[0]") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.extractvalue %relay_call[1]") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.cond_br %relay_outcome") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "^w_seed_typed_relay_b_2(%relay_payload : i64)") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "^w_seed_typed_relay_b_3(%relay_outcome : i1)") &&
        count_bytes(artifact, emitted.written.mlir_bytes, "llvm.return") ==
            3u &&
        !contains_bytes(artifact, emitted.written.mlir_bytes, "llvm.invoke") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes, "unwind") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes, "llvm.alloca") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes, "@main") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes, "ExitProcess"));

  uint8_t windows_artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_typed_propagation_result windows_result;
  CHECK(w_seed_mlir0_emit_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &WINDOWS_TARGET,
            &(w_seed_mlir0_typed_propagation_output){windows_artifact,
                                                     sizeof(windows_artifact)},
            &windows_result) == W_SEED_MLIR0_OK);
  CHECK(windows_result.written.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(windows_artifact, artifact, emitted.written.mlir_bytes) == 0 &&
        memcmp(windows_result.mlir_sha256, emitted.mlir_sha256,
               sizeof(emitted.mlir_sha256)) == 0);

  const uint8_t saved_byte = artifact[emitted.written.mlir_bytes - 1u];
  artifact[emitted.written.mlir_bytes - 1u] =
      saved_byte == (uint8_t)'\n' ? (uint8_t)' ' : (uint8_t)'\n';
  CHECK(!w_seed_mlir0_verify_typed_propagation(
      &fixture.hir_program, &fixture.hir_result, &TARGET, artifact,
      emitted.written.mlir_bytes, &emitted));
  artifact[emitted.written.mlir_bytes - 1u] = saved_byte;

  w_seed_mlir0_typed_propagation_result alias_result;
  (void)memset(&alias_result, 0x53, sizeof(alias_result));
  const w_seed_mlir0_typed_propagation_result alias_result_snapshot =
      alias_result;
  CHECK(w_seed_mlir0_measure_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &TARGET,
            (w_seed_mlir0_typed_propagation_counts *)(void *)&alias_result,
            &alias_result) == W_SEED_MLIR0_ALIAS);
  CHECK(memcmp(&alias_result, &alias_result_snapshot,
               sizeof(alias_result)) == 0);

  uint8_t hir_text_snapshot[sizeof(fixture.hir_text)];
  (void)memcpy(hir_text_snapshot, fixture.hir_text, sizeof(hir_text_snapshot));
  CHECK(w_seed_mlir0_measure_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &TARGET,
            (w_seed_mlir0_typed_propagation_counts *)(void *)fixture.hir_text,
            &alias_result) == W_SEED_MLIR0_ALIAS);
  CHECK(memcmp(fixture.hir_text, hir_text_snapshot,
               sizeof(hir_text_snapshot)) == 0);

  uint8_t short_artifact[W_SEED_MLIR0_MAX_BYTES];
  uint8_t short_artifact_snapshot[W_SEED_MLIR0_MAX_BYTES];
  (void)memset(short_artifact, 0x55, sizeof(short_artifact));
  (void)memcpy(short_artifact_snapshot, short_artifact,
               sizeof(short_artifact_snapshot));
  const w_seed_mlir0_typed_propagation_result short_result_snapshot =
      emitted;
  CHECK(w_seed_mlir0_emit_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &TARGET,
            &(w_seed_mlir0_typed_propagation_output){
                short_artifact, emitted.written.mlir_bytes - 1u},
            &emitted) == W_SEED_MLIR0_CAPACITY);
  CHECK(memcmp(short_artifact, short_artifact_snapshot,
               sizeof(short_artifact)) == 0 &&
        memcmp(&emitted, &short_result_snapshot, sizeof(emitted)) == 0);
  CHECK(w_seed_mlir0_emit_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &TARGET, NULL,
            &emitted) == W_SEED_MLIR0_INVALID_HIR);
  CHECK(memcmp(&emitted, &short_result_snapshot, sizeof(emitted)) == 0);
  CHECK(w_seed_mlir0_emit_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &TARGET,
            &(w_seed_mlir0_typed_propagation_output){NULL, 0u}, &emitted) ==
        W_SEED_MLIR0_CAPACITY);
  CHECK(memcmp(&emitted, &short_result_snapshot, sizeof(emitted)) == 0);

  CHECK(w_seed_mlir0_emit_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &TARGET,
            &(w_seed_mlir0_typed_propagation_output){
                fixture.hir_text, sizeof(fixture.hir_text)},
            &emitted) == W_SEED_MLIR0_ALIAS);
  CHECK(memcmp(fixture.hir_text, hir_text_snapshot,
               sizeof(fixture.hir_text)) == 0 &&
        memcmp(&emitted, &short_result_snapshot, sizeof(emitted)) == 0);
  const w_seed_hir0_program program_snapshot = fixture.hir_program;
  CHECK(w_seed_mlir0_emit_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &TARGET,
            &(w_seed_mlir0_typed_propagation_output){artifact, sizeof(artifact)},
            (w_seed_mlir0_typed_propagation_result *)(void *)&fixture.hir_program) ==
        W_SEED_MLIR0_ALIAS);
  CHECK(memcmp(&fixture.hir_program, &program_snapshot,
               sizeof(fixture.hir_program)) == 0 &&
        memcmp(&emitted, &short_result_snapshot, sizeof(emitted)) == 0);
  CHECK(w_seed_mlir0_emit_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &TARGET,
            &(w_seed_mlir0_typed_propagation_output){
                (uint8_t *)(void *)&emitted, sizeof(emitted)},
            &emitted) == W_SEED_MLIR0_ALIAS);
  CHECK(memcmp(&emitted, &short_result_snapshot, sizeof(emitted)) == 0);

  struct {
    w_seed_mlir0_target target;
    uint8_t suffix[32];
  } target_alias;
  target_alias.target = TARGET;
  (void)memset(target_alias.suffix, 0x57, sizeof(target_alias.suffix));
  CHECK(w_seed_mlir0_emit_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &target_alias.target,
            &(w_seed_mlir0_typed_propagation_output){
                (uint8_t *)(void *)&target_alias.target,
                sizeof(target_alias.target)},
            &emitted) == W_SEED_MLIR0_ALIAS);
  CHECK(memcmp(&emitted, &short_result_snapshot, sizeof(emitted)) == 0 &&
        target_alias.target.kind == TARGET.kind &&
        target_alias.suffix[0] == 0x57u);

  w_seed_mlir0_typed_propagation_result forged_result = emitted;
  forged_result.required.carrier_field_count ^= 1u;
  CHECK(!w_seed_mlir0_verify_typed_propagation(
      &fixture.hir_program, &fixture.hir_result, &TARGET, artifact,
      emitted.written.mlir_bytes, &forged_result));
  forged_result = emitted;
  forged_result.mlir_sha256[0] ^= 1u;
  CHECK(!w_seed_mlir0_verify_typed_propagation(
      &fixture.hir_program, &fixture.hir_result, &TARGET, artifact,
      emitted.written.mlir_bytes, &forged_result));
  w_seed_hir0_result forged_hir_result = fixture.hir_result;
  forged_hir_result.semantic_digest[0] ^= 1u;
  w_seed_mlir0_typed_propagation_counts forged_counts;
  w_seed_mlir0_typed_propagation_result forged_api_result;
  (void)memset(&forged_counts, 0x58, sizeof(forged_counts));
  (void)memset(&forged_api_result, 0x59, sizeof(forged_api_result));
  const w_seed_mlir0_typed_propagation_counts forged_counts_snapshot =
      forged_counts;
  const w_seed_mlir0_typed_propagation_result forged_api_snapshot =
      forged_api_result;
  CHECK(w_seed_mlir0_measure_typed_propagation(
            &fixture.hir_program, &forged_hir_result, &TARGET, &forged_counts,
            &forged_api_result) == W_SEED_MLIR0_INVALID_HIR);
  CHECK(memcmp(&forged_counts, &forged_counts_snapshot,
               sizeof(forged_counts)) == 0 &&
        memcmp(&forged_api_result, &forged_api_snapshot,
               sizeof(forged_api_result)) == 0);

  static const uint8_t two_cases[] =
      "enum Failure: Error { denied other }\n"
      "fn leaf(): i64 throws Failure { throw .denied }\n"
      "fn relay(): i64 throws Failure { return try leaf() }\n"
      "entry { }\n";
  CHECK(lower_hir(two_cases, sizeof(two_cases) - 1u));
  w_seed_mlir0_typed_propagation_counts rejected_counts;
  w_seed_mlir0_typed_propagation_result rejected_result;
  (void)memset(&rejected_counts, 0x41, sizeof(rejected_counts));
  (void)memset(&rejected_result, 0x42, sizeof(rejected_result));
  const w_seed_mlir0_typed_propagation_counts rejected_counts_snapshot =
      rejected_counts;
  const w_seed_mlir0_typed_propagation_result rejected_result_snapshot =
      rejected_result;
  CHECK(w_seed_mlir0_measure_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &TARGET,
            &rejected_counts, &rejected_result) == W_SEED_MLIR0_UNSUPPORTED);
  CHECK(memcmp(&rejected_counts, &rejected_counts_snapshot,
               sizeof(rejected_counts)) == 0 &&
        memcmp(&rejected_result, &rejected_result_snapshot,
               sizeof(rejected_result)) == 0);
  return true;
}

static int64_t integer_exactly_signed_min(uint16_t width) {
  return width == 64u ? INT64_MIN : -(INT64_C(1) << (width - 1u));
}

static int64_t integer_exactly_signed_max(uint16_t width) {
  return width == 64u ? INT64_MAX :
                        (INT64_C(1) << (width - 1u)) - 1;
}

static uint64_t integer_exactly_unsigned_max(uint16_t width) {
  return width == 64u ? UINT64_MAX : (UINT64_C(1) << width) - 1u;
}

static bool integer_exactly_oracle_fits(bool source_signed,
                                        int64_t signed_source,
                                        uint64_t unsigned_source,
                                        bool destination_signed,
                                        uint16_t destination_width) {
  if (destination_signed) {
    const int64_t minimum =
        integer_exactly_signed_min(destination_width);
    const int64_t maximum =
        integer_exactly_signed_max(destination_width);
    if (source_signed)
      return signed_source >= minimum && signed_source <= maximum;
    return unsigned_source <= (uint64_t)maximum;
  }
  if (source_signed) {
    if (signed_source < 0) return false;
    return destination_width == 64u ||
           (uint64_t)signed_source <=
               integer_exactly_unsigned_max(destination_width);
  }
  return destination_width == 64u ||
         unsigned_source <= integer_exactly_unsigned_max(destination_width);
}

static uint32_t integer_exactly_expected_predicates(bool source_signed,
                                                    uint16_t source_width,
                                                    bool destination_signed,
                                                    uint16_t destination_width) {
  if (source_signed && destination_signed)
    return source_width > destination_width ? 2u : 0u;
  if (source_signed) {
    return 1u + (source_width > (uint16_t)(destination_width + 1u) ? 1u : 0u);
  }
  if (destination_signed)
    return source_width >= destination_width ? 1u : 0u;
  return source_width > destination_width ? 1u : 0u;
}

static bool test_integer_exactly_mlir(void) {
  static const char *const integer_types[] = {
      "i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64", "Int",
      "UInt"};
  static const bool integer_signed[] = {
      true, false, true, false, true, false, true, false, true, false};
  static const uint16_t integer_widths[] = {
      8u, 8u, 16u, 16u, 32u, 32u, 64u, 64u, 64u, 64u};
  static uint8_t linux_artifact[W_SEED_MLIR0_MAX_BYTES];
  static uint8_t windows_artifact[W_SEED_MLIR0_MAX_BYTES];

  CHECK(integer_exactly_oracle_fits(true, 127, 0u, true, 8u) &&
        !integer_exactly_oracle_fits(true, 128, 0u, true, 8u) &&
        integer_exactly_oracle_fits(true, -128, 0u, true, 8u) &&
        !integer_exactly_oracle_fits(true, -129, 0u, true, 8u) &&
        integer_exactly_oracle_fits(false, 0, 127u, true, 8u) &&
        !integer_exactly_oracle_fits(false, 0, 128u, true, 8u) &&
        integer_exactly_oracle_fits(true, 255, 0u, false, 8u) &&
        !integer_exactly_oracle_fits(true, -1, 0u, false, 8u) &&
        !integer_exactly_oracle_fits(true, 256, 0u, false, 8u) &&
        integer_exactly_oracle_fits(false, 0, (UINT64_C(1) << 63) - 1u,
                                    true, 64u) &&
        !integer_exactly_oracle_fits(false, 0, UINT64_C(1) << 63, true,
                                     64u) &&
        !integer_exactly_oracle_fits(true, -1, 0u, false, 64u));

  for (size_t source = 0u;
       source < sizeof(integer_types) / sizeof(integer_types[0]);
       source += 1u) {
    for (size_t destination = 0u;
         destination < sizeof(integer_types) / sizeof(integer_types[0]);
         destination += 1u) {
      char source_text[320];
      const int source_length = snprintf(
          source_text, sizeof(source_text),
          "fn convert(value: %s): %s throws NumericConversionError { "
          "return try %s(exactly: value) }\nentry { }\n",
          integer_types[source], integer_types[destination],
          integer_types[destination]);
      CHECK(source_length > 0 && (size_t)source_length < sizeof(source_text));
      CHECK(lower_hir((const uint8_t *)source_text, (size_t)source_length));

      w_seed_native_subset0_integer_exactly selection;
      CHECK(w_seed_native_subset0_select_integer_exactly(
                &fixture.hir_program, &fixture.hir_result, &selection) ==
            W_SEED_NATIVE_SUBSET0_OK);
      CHECK(w_seed_native_subset0_verify_integer_exactly(
                &fixture.hir_program, &fixture.hir_result, &selection) &&
            selection.source_is_signed == integer_signed[source] &&
            selection.source_bit_width == integer_widths[source] &&
            selection.destination_is_signed == integer_signed[destination] &&
            selection.destination_bit_width == integer_widths[destination] &&
            fixture.hir_program.call_count == 0u &&
            selection.conversion->kind ==
                W_SEED_HIR0_TERMINATOR_INTEGER_EXACTLY &&
            selection.error_throw->kind == W_SEED_HIR0_TERMINATOR_THROW);
      if (source == 0u && destination == 0u) {
        w_seed_native_subset0_program ordinary_selection;
        w_seed_mlir0_counts ordinary_counts = {0u};
        w_seed_mlir0_result ordinary_result;
        (void)memset(&ordinary_result, 0x6du, sizeof(ordinary_result));
        const w_seed_mlir0_result ordinary_snapshot = ordinary_result;
        const w_seed_mlir0_input ordinary = mlir_input();
        CHECK(w_seed_native_subset0_select_program(
                  &fixture.hir_program, &fixture.hir_result,
                  &ordinary_selection) == W_SEED_NATIVE_SUBSET0_UNSUPPORTED &&
              w_seed_mlir0_measure(&ordinary, &TARGET, &ordinary_counts,
                                   &ordinary_result) ==
                  W_SEED_MLIR0_UNSUPPORTED &&
              memcmp(&ordinary_result, &ordinary_snapshot,
                     sizeof(ordinary_result)) == 0);
      }

      const uint32_t expected_predicate_count = integer_exactly_expected_predicates(
          integer_signed[source], integer_widths[source],
          integer_signed[destination], integer_widths[destination]);
      w_seed_mlir0_integer_exactly_counts measured_counts;
      w_seed_mlir0_integer_exactly_result measured_result;
      CHECK(w_seed_mlir0_measure_integer_exactly(
                &fixture.hir_program, &fixture.hir_result, &TARGET,
                &measured_counts, &measured_result) == W_SEED_MLIR0_OK);
      CHECK(measured_counts.mlir_bytes > 0u &&
            measured_counts.mlir_bytes < W_SEED_MLIR0_MAX_BYTES &&
            measured_counts.source_bit_width == integer_widths[source] &&
            measured_counts.destination_bit_width ==
                integer_widths[destination] &&
            measured_counts.source_is_signed == integer_signed[source] &&
            measured_counts.destination_is_signed ==
                integer_signed[destination] &&
            measured_counts.representability_predicate_count ==
                expected_predicate_count &&
            measured_counts.typed_branch_count == 1u &&
            measured_counts.carrier_field_count ==
                W_SEED_MLIR0_INTEGER_EXACTLY_CARRIER_FIELDS &&
            measured_result.required.mlir_bytes == measured_counts.mlir_bytes &&
            measured_result.written.mlir_bytes == 0u);

      w_seed_mlir0_integer_exactly_result emitted_result;
      CHECK(w_seed_mlir0_emit_integer_exactly(
                &fixture.hir_program, &fixture.hir_result, &TARGET,
                &(w_seed_mlir0_integer_exactly_output){linux_artifact,
                                                        sizeof(linux_artifact)},
                &emitted_result) == W_SEED_MLIR0_OK);
      CHECK(emitted_result.required.mlir_bytes == measured_counts.mlir_bytes &&
            emitted_result.written.mlir_bytes == measured_counts.mlir_bytes &&
            w_seed_mlir0_verify_integer_exactly(
                &fixture.hir_program, &fixture.hir_result, &TARGET,
                linux_artifact, emitted_result.written.mlir_bytes,
                &emitted_result));
      CHECK(contains_bytes(linux_artifact, emitted_result.written.mlir_bytes,
                           "// " W_SEED_MLIR0_INTEGER_EXACTLY_SCHEMA_VERSION
                           "\n") &&
            contains_bytes(linux_artifact, emitted_result.written.mlir_bytes,
                           W_SEED_MLIR0_TARGET_TRIPLE_LINUX) &&
            contains_bytes(linux_artifact, emitted_result.written.mlir_bytes,
                           "NumericConversionError.outOfRange") &&
            contains_bytes(linux_artifact, emitted_result.written.mlir_bytes,
                           "llvm.cond_br %exact_fits, ^exact_success, "
                           "^exact_out_of_range") &&
            contains_bytes(linux_artifact, emitted_result.written.mlir_bytes,
                           "llvm.insertvalue %exact_success_status") &&
            contains_bytes(linux_artifact, emitted_result.written.mlir_bytes,
                           "llvm.insertvalue %exact_error_status") &&
            count_bytes(linux_artifact, emitted_result.written.mlir_bytes,
                        "llvm.icmp") == expected_predicate_count &&
            count_bytes(linux_artifact, emitted_result.written.mlir_bytes,
                        "llvm.return") == 2u &&
            !contains_bytes(linux_artifact, emitted_result.written.mlir_bytes,
                            "@main") &&
            !contains_bytes(linux_artifact, emitted_result.written.mlir_bytes,
                            "llvm.alloca") &&
            !contains_bytes(linux_artifact, emitted_result.written.mlir_bytes,
                            "llvm.invoke") &&
            !contains_bytes(linux_artifact, emitted_result.written.mlir_bytes,
                            "ExitProcess"));

      const size_t branch_position = find_bytes(
          linux_artifact, emitted_result.written.mlir_bytes,
          "llvm.cond_br %exact_fits", 0u);
      const size_t trunc_position = find_bytes(
          linux_artifact, emitted_result.written.mlir_bytes, "llvm.trunc", 0u);
      const size_t sext_position = find_bytes(
          linux_artifact, emitted_result.written.mlir_bytes, "llvm.sext", 0u);
      const size_t zext_position = find_bytes(
          linux_artifact, emitted_result.written.mlir_bytes, "llvm.zext", 0u);
      CHECK(branch_position != SIZE_MAX &&
            (trunc_position == SIZE_MAX || trunc_position > branch_position) &&
            (sext_position == SIZE_MAX || sext_position > branch_position) &&
            (zext_position == SIZE_MAX || zext_position > branch_position));

      if (source == 2u && destination == 0u) {
        CHECK(contains_bytes(linux_artifact, emitted_result.written.mlir_bytes,
                             "llvm.mlir.constant(-128 : i16) : i16") &&
              contains_bytes(linux_artifact, emitted_result.written.mlir_bytes,
                             "llvm.mlir.constant(127 : i16) : i16") &&
              integer_exactly_oracle_fits(true, 127, 0u, true, 8u) &&
              !integer_exactly_oracle_fits(true, 128, 0u, true, 8u));
      } else if (source == 3u && destination == 0u) {
        CHECK(contains_bytes(linux_artifact, emitted_result.written.mlir_bytes,
                             "llvm.mlir.constant(127 : i16) : i16") &&
              contains_bytes(linux_artifact, emitted_result.written.mlir_bytes,
                             "llvm.icmp \"ule\" %source, %exact_bound_0 : i16") &&
              integer_exactly_oracle_fits(false, 0, 127u, true, 8u) &&
              !integer_exactly_oracle_fits(false, 0, 128u, true, 8u));
      } else if (source == 2u && destination == 1u) {
        CHECK(contains_bytes(linux_artifact, emitted_result.written.mlir_bytes,
                             "llvm.mlir.constant(0 : i16) : i16") &&
              contains_bytes(linux_artifact, emitted_result.written.mlir_bytes,
                             "llvm.mlir.constant(255 : i16) : i16") &&
              integer_exactly_oracle_fits(true, 255, 0u, false, 8u) &&
              !integer_exactly_oracle_fits(true, -1, 0u, false, 8u) &&
              !integer_exactly_oracle_fits(true, 256, 0u, false, 8u));
      } else if (source == 7u && destination == 6u) {
        CHECK(contains_bytes(linux_artifact, emitted_result.written.mlir_bytes,
                             "llvm.mlir.constant(9223372036854775807 : i64) : i64") &&
              integer_exactly_oracle_fits(false, 0,
                                          (UINT64_C(1) << 63) - 1u, true,
                                          64u) &&
              !integer_exactly_oracle_fits(false, 0, UINT64_C(1) << 63, true,
                                           64u));
      }

      w_seed_mlir0_integer_exactly_result windows_result;
      CHECK(w_seed_mlir0_emit_integer_exactly(
                &fixture.hir_program, &fixture.hir_result, &WINDOWS_TARGET,
                &(w_seed_mlir0_integer_exactly_output){
                    windows_artifact, sizeof(windows_artifact)},
                &windows_result) == W_SEED_MLIR0_OK);
      CHECK(w_seed_mlir0_verify_integer_exactly(
                &fixture.hir_program, &fixture.hir_result, &WINDOWS_TARGET,
                windows_artifact, windows_result.written.mlir_bytes,
                &windows_result) &&
            contains_bytes(windows_artifact, windows_result.written.mlir_bytes,
                           W_SEED_MLIR0_TARGET_TRIPLE_WINDOWS) &&
            windows_result.written.source_bit_width ==
                emitted_result.written.source_bit_width &&
            windows_result.written.destination_bit_width ==
                emitted_result.written.destination_bit_width &&
            windows_result.written.representability_predicate_count ==
                emitted_result.written.representability_predicate_count);
      w_seed_native_subset0_integer_exactly forged_selection = selection;
      forged_selection.source_is_signed = !forged_selection.source_is_signed;
      CHECK(!w_seed_native_subset0_verify_integer_exactly(
          &fixture.hir_program, &fixture.hir_result, &forged_selection));
    }
  }

  CHECK(lower_hir(
      (const uint8_t *)"fn convert(value: i16): i8 throws NumericConversionError { "
                       "return try i8(exactly: value) }\nentry { }\n",
      sizeof("fn convert(value: i16): i8 throws NumericConversionError { "
             "return try i8(exactly: value) }\nentry { }\n") -
          1u));
  uint8_t short_artifact[8];
  (void)memset(short_artifact, 0x6bu, sizeof(short_artifact));
  w_seed_mlir0_integer_exactly_result rejected_result;
  (void)memset(&rejected_result, 0x6cu, sizeof(rejected_result));
  const w_seed_mlir0_integer_exactly_result rejected_snapshot = rejected_result;
  CHECK(w_seed_mlir0_emit_integer_exactly(
            &fixture.hir_program, &fixture.hir_result, &TARGET,
            &(w_seed_mlir0_integer_exactly_output){short_artifact,
                                                   sizeof(short_artifact)},
            &rejected_result) == W_SEED_MLIR0_CAPACITY &&
        memcmp(&rejected_result, &rejected_snapshot,
               sizeof(rejected_result)) == 0);
  for (size_t index = 0u; index < sizeof(short_artifact); index += 1u)
    CHECK(short_artifact[index] == 0x6bu);
  const w_seed_hir0_program program_snapshot = fixture.hir_program;
  uint8_t hir_text_snapshot[sizeof(fixture.hir_text)];
  (void)memcpy(hir_text_snapshot, fixture.hir_text,
               sizeof(hir_text_snapshot));
  CHECK(w_seed_mlir0_emit_integer_exactly(
            &fixture.hir_program, &fixture.hir_result, &TARGET,
            &(w_seed_mlir0_integer_exactly_output){fixture.hir_text,
                                                   sizeof(fixture.hir_text)},
            &rejected_result) == W_SEED_MLIR0_ALIAS &&
        memcmp(&fixture.hir_program, &program_snapshot,
               sizeof(fixture.hir_program)) == 0 &&
        memcmp(fixture.hir_text, hir_text_snapshot,
               sizeof(fixture.hir_text)) == 0 &&
        memcmp(&rejected_result, &rejected_snapshot,
               sizeof(rejected_result)) == 0);
  w_seed_mlir0_integer_exactly_result forged_result;
  CHECK(w_seed_mlir0_emit_integer_exactly(
            &fixture.hir_program, &fixture.hir_result, &TARGET,
            &(w_seed_mlir0_integer_exactly_output){linux_artifact,
                                                    sizeof(linux_artifact)},
            &forged_result) == W_SEED_MLIR0_OK);
  forged_result.required.representability_predicate_count ^= 1u;
  CHECK(!w_seed_mlir0_verify_integer_exactly(
      &fixture.hir_program, &fixture.hir_result, &TARGET, linux_artifact,
      forged_result.written.mlir_bytes, &forged_result));
  w_seed_mlir0_target unsupported = {W_SEED_MLIR0_TARGET_UNSUPPORTED};
  CHECK(w_seed_mlir0_measure_integer_exactly(
            &fixture.hir_program, &fixture.hir_result, &unsupported,
            &(w_seed_mlir0_integer_exactly_counts){0}, &forged_result) ==
        W_SEED_MLIR0_UNSUPPORTED);
  return true;
}

static bool test_float_to_integer_rounding_mlir(void) {
  static const char *const source_types[] = {"f32", "f64"};
  static const uint16_t source_widths[] = {32u, 64u};
  static const char *const destination_types[] = {
      "i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64", "Int",
      "UInt"};
  static const bool destination_signed[] = {
      true, false, true, false, true, false, true, false, true, false};
  static const uint16_t destination_widths[] = {
      8u, 8u, 16u, 16u, 32u, 32u, 64u, 64u, 64u, 64u};
  static const char *const mode_names[] = {
      "nearestEven", "nearestAwayFromZero", "towardZero", "towardPositive",
      "towardNegative"};
  static const char *const intrinsic_names[] = {
      "llvm.intr.roundeven", "llvm.intr.round", "llvm.intr.trunc",
      "llvm.intr.ceil", "llvm.intr.floor"};
  static const w_seed_hir0_rounding_mode modes[] = {
      W_SEED_HIR0_ROUNDING_MODE_NEAREST_EVEN,
      W_SEED_HIR0_ROUNDING_MODE_NEAREST_AWAY_FROM_ZERO,
      W_SEED_HIR0_ROUNDING_MODE_TOWARD_ZERO,
      W_SEED_HIR0_ROUNDING_MODE_TOWARD_POSITIVE,
      W_SEED_HIR0_ROUNDING_MODE_TOWARD_NEGATIVE};
  static const char *const signed_lower_bits[2][4] = {
      {"0xc3000000", "0xc7000000", "0xcf000000", "0xdf000000"},
      {"0xc060000000000000", "0xc0e0000000000000",
       "0xc1e0000000000000", "0xc3e0000000000000"}};
  static const char *const signed_upper_bits[2][4] = {
      {"0x43000000", "0x47000000", "0x4f000000", "0x5f000000"},
      {"0x4060000000000000", "0x40e0000000000000",
       "0x41e0000000000000", "0x43e0000000000000"}};
  static const char *const unsigned_upper_bits[2][4] = {
      {"0x43800000", "0x47800000", "0x4f800000", "0x5f800000"},
      {"0x4070000000000000", "0x40f0000000000000",
       "0x41f0000000000000", "0x43f0000000000000"}};
  static uint8_t linux_artifact[W_SEED_MLIR0_MAX_BYTES];
  static uint8_t windows_artifact[W_SEED_MLIR0_MAX_BYTES];

  for (size_t source = 0u; source < 2u; source += 1u) {
    for (size_t destination = 0u; destination < 10u; destination += 1u) {
      for (size_t mode = 0u; mode < 5u; mode += 1u) {
        char source_text[384];
        const int source_length = snprintf(
            source_text, sizeof(source_text),
            "fn convert(value: %s): %s throws NumericConversionError { "
            "return try %s(rounding: value, mode: .%s) }\nentry { }\n",
            source_types[source], destination_types[destination],
            destination_types[destination], mode_names[mode]);
        CHECK(source_length > 0 &&
              (size_t)source_length < sizeof(source_text));
        CHECK(lower_hir((const uint8_t *)source_text,
                        (size_t)source_length));

        w_seed_native_subset0_float_to_integer_rounding selection;
        CHECK(w_seed_native_subset0_select_float_to_integer_rounding(
                  &fixture.hir_program, &fixture.hir_result, &selection) ==
              W_SEED_NATIVE_SUBSET0_OK);
        CHECK(w_seed_native_subset0_verify_float_to_integer_rounding(
                  &fixture.hir_program, &fixture.hir_result, &selection) &&
              selection.source_bit_width == source_widths[source] &&
              selection.destination_bit_width ==
                  destination_widths[destination] &&
              selection.destination_is_signed ==
                  destination_signed[destination] &&
              selection.rounding_mode == modes[mode] &&
              selection.conversion->target_block ==
                  selection.normal_block_index &&
              selection.conversion->else_block ==
                  selection.non_finite_block_index &&
              selection.conversion->third_block ==
                  selection.out_of_range_block_index);

        w_seed_mlir0_float_to_integer_rounding_counts measured_counts;
        w_seed_mlir0_float_to_integer_rounding_result measured_result;
        const w_seed_mlir0_status measure_status =
            w_seed_mlir0_measure_float_to_integer_rounding(
                &fixture.hir_program, &fixture.hir_result, &TARGET,
                &measured_counts, &measured_result);
        if (measure_status != W_SEED_MLIR0_OK)
          (void)fprintf(stderr,
                        "rounding measure status=%d source=%lu destination=%lu mode=%lu\n",
                        (int)measure_status, (unsigned long)source,
                        (unsigned long)destination, (unsigned long)mode);
        CHECK(measure_status == W_SEED_MLIR0_OK);
        CHECK(measured_counts.mlir_bytes > 0u &&
              measured_counts.source_bit_width == source_widths[source] &&
              measured_counts.destination_bit_width ==
                  destination_widths[destination] &&
              measured_counts.range_predicate_count == 2u &&
              measured_counts.typed_branch_count == 2u &&
              measured_counts.outcome_count == 3u &&
              measured_counts.carrier_field_count == 2u &&
              measured_counts.outcome_bit_width == 2u &&
              measured_counts.destination_is_signed ==
                  destination_signed[destination] &&
              measured_counts.rounding_mode == modes[mode] &&
              measured_result.written.mlir_bytes == 0u);

        w_seed_mlir0_float_to_integer_rounding_result emitted_result;
        CHECK(w_seed_mlir0_emit_float_to_integer_rounding(
                  &fixture.hir_program, &fixture.hir_result, &TARGET,
                  &(w_seed_mlir0_float_to_integer_rounding_output){
                      linux_artifact, sizeof(linux_artifact)},
                  &emitted_result) == W_SEED_MLIR0_OK);
        CHECK(w_seed_mlir0_verify_float_to_integer_rounding(
            &fixture.hir_program, &fixture.hir_result, &TARGET,
            linux_artifact, emitted_result.written.mlir_bytes,
            &emitted_result));
        CHECK(contains_bytes(
                  linux_artifact, emitted_result.written.mlir_bytes,
                  "// " W_SEED_MLIR0_FLOAT_TO_INTEGER_ROUNDING_SCHEMA_VERSION
                  "\n") &&
              contains_bytes(linux_artifact,
                             emitted_result.written.mlir_bytes,
                             W_SEED_MLIR0_TARGET_TRIPLE_LINUX) &&
              contains_bytes(linux_artifact,
                             emitted_result.written.mlir_bytes,
                             "\"llvm.intr.is.fpclass\"") &&
              contains_bytes(linux_artifact,
                             emitted_result.written.mlir_bytes,
                             "bit = 519 : i32") &&
              contains_bytes(linux_artifact,
                             emitted_result.written.mlir_bytes,
                             intrinsic_names[mode]) &&
              count_bytes(linux_artifact, emitted_result.written.mlir_bytes,
                          "llvm.fcmp") == 2u &&
              count_bytes(linux_artifact, emitted_result.written.mlir_bytes,
                          "llvm.cond_br") == 2u &&
              count_bytes(linux_artifact, emitted_result.written.mlir_bytes,
                          "llvm.return") == 3u &&
              count_bytes(linux_artifact, emitted_result.written.mlir_bytes,
                          destination_signed[destination] ? "llvm.fptosi"
                                                          : "llvm.fptoui") ==
                  1u &&
              !contains_bytes(linux_artifact,
                              emitted_result.written.mlir_bytes,
                              "fastmath") &&
              !contains_bytes(linux_artifact,
                              emitted_result.written.mlir_bytes,
                              "llvm.alloca") &&
              !contains_bytes(linux_artifact,
                              emitted_result.written.mlir_bytes, "@main"));
        const size_t width_index =
            destination_widths[destination] == 8u
                ? 0u
                : destination_widths[destination] == 16u
                      ? 1u
                      : destination_widths[destination] == 32u ? 2u : 3u;
        char lower_needle[96];
        char upper_needle[96];
        const int lower_length = snprintf(
            lower_needle, sizeof(lower_needle),
            "%%round_lower = llvm.mlir.constant(%s : f%u)",
            destination_signed[destination]
                ? signed_lower_bits[source][width_index]
                : source_widths[source] == 32u ? "0x00000000"
                                               : "0x0000000000000000",
            (unsigned)source_widths[source]);
        const int upper_length = snprintf(
            upper_needle, sizeof(upper_needle),
            "%%round_upper = llvm.mlir.constant(%s : f%u)",
            destination_signed[destination]
                ? signed_upper_bits[source][width_index]
                : unsigned_upper_bits[source][width_index],
            (unsigned)source_widths[source]);
        CHECK(lower_length > 0 &&
              (size_t)lower_length < sizeof(lower_needle) &&
              upper_length > 0 &&
              (size_t)upper_length < sizeof(upper_needle) &&
              contains_bytes(linux_artifact,
                             emitted_result.written.mlir_bytes,
                             lower_needle) &&
              contains_bytes(linux_artifact,
                             emitted_result.written.mlir_bytes,
                             upper_needle));
        const size_t classification = find_bytes(
            linux_artifact, emitted_result.written.mlir_bytes,
            "\"llvm.intr.is.fpclass\"", 0u);
        const size_t rounding = find_bytes(
            linux_artifact, emitted_result.written.mlir_bytes,
            intrinsic_names[mode], 0u);
        const size_t lower_compare = find_bytes(
            linux_artifact, emitted_result.written.mlir_bytes,
            "llvm.fcmp \"oge\"", 0u);
        const size_t upper_compare = find_bytes(
            linux_artifact, emitted_result.written.mlir_bytes,
            "llvm.fcmp \"olt\"", 0u);
        const size_t range_branch = find_bytes(
            linux_artifact, emitted_result.written.mlir_bytes,
            "llvm.cond_br %round_fits", 0u);
        const size_t conversion = find_bytes(
            linux_artifact, emitted_result.written.mlir_bytes,
            destination_signed[destination] ? "llvm.fptosi" : "llvm.fptoui",
            0u);
        CHECK(classification != SIZE_MAX && rounding > classification &&
              lower_compare > rounding && upper_compare > lower_compare &&
              range_branch > upper_compare && conversion > range_branch);

        w_seed_mlir0_float_to_integer_rounding_result windows_result;
        CHECK(w_seed_mlir0_emit_float_to_integer_rounding(
                  &fixture.hir_program, &fixture.hir_result, &WINDOWS_TARGET,
                  &(w_seed_mlir0_float_to_integer_rounding_output){
                      windows_artifact, sizeof(windows_artifact)},
                  &windows_result) == W_SEED_MLIR0_OK);
        CHECK(w_seed_mlir0_verify_float_to_integer_rounding(
                  &fixture.hir_program, &fixture.hir_result, &WINDOWS_TARGET,
                  windows_artifact, windows_result.written.mlir_bytes,
                  &windows_result) &&
              contains_bytes(windows_artifact,
                             windows_result.written.mlir_bytes,
                             W_SEED_MLIR0_TARGET_TRIPLE_WINDOWS));

        w_seed_native_subset0_float_to_integer_rounding forged = selection;
        forged.rounding_mode = W_SEED_HIR0_ROUNDING_MODE_NONE;
        CHECK(!w_seed_native_subset0_verify_float_to_integer_rounding(
            &fixture.hir_program, &fixture.hir_result, &forged));
      }
    }
  }

  static const uint8_t transaction_source[] =
      "fn convert(value: f32): i8 throws NumericConversionError { "
      "return try i8(rounding: value, mode: .nearestEven) }\nentry { }\n";
  CHECK(lower_hir(transaction_source, sizeof(transaction_source) - 1u));
  uint8_t short_artifact[8];
  (void)memset(short_artifact, 0x6bu, sizeof(short_artifact));
  w_seed_mlir0_float_to_integer_rounding_result rejected_result;
  (void)memset(&rejected_result, 0x6cu, sizeof(rejected_result));
  const w_seed_mlir0_float_to_integer_rounding_result rejected_snapshot =
      rejected_result;
  CHECK(w_seed_mlir0_emit_float_to_integer_rounding(
            &fixture.hir_program, &fixture.hir_result, &TARGET,
            &(w_seed_mlir0_float_to_integer_rounding_output){
                short_artifact, sizeof(short_artifact)},
            &rejected_result) == W_SEED_MLIR0_CAPACITY &&
        memcmp(&rejected_result, &rejected_snapshot,
               sizeof(rejected_result)) == 0);
  for (size_t index = 0u; index < sizeof(short_artifact); index += 1u)
    CHECK(short_artifact[index] == 0x6bu);

  const w_seed_hir0_program program_snapshot = fixture.hir_program;
  uint8_t hir_text_snapshot[sizeof(fixture.hir_text)];
  (void)memcpy(hir_text_snapshot, fixture.hir_text,
               sizeof(hir_text_snapshot));
  CHECK(w_seed_mlir0_emit_float_to_integer_rounding(
            &fixture.hir_program, &fixture.hir_result, &TARGET,
            &(w_seed_mlir0_float_to_integer_rounding_output){
                fixture.hir_text, sizeof(fixture.hir_text)},
            &rejected_result) == W_SEED_MLIR0_ALIAS &&
        memcmp(&fixture.hir_program, &program_snapshot,
               sizeof(fixture.hir_program)) == 0 &&
        memcmp(fixture.hir_text, hir_text_snapshot,
               sizeof(fixture.hir_text)) == 0 &&
        memcmp(&rejected_result, &rejected_snapshot,
               sizeof(rejected_result)) == 0);

  w_seed_mlir0_float_to_integer_rounding_result forged_result;
  CHECK(w_seed_mlir0_emit_float_to_integer_rounding(
            &fixture.hir_program, &fixture.hir_result, &TARGET,
            &(w_seed_mlir0_float_to_integer_rounding_output){
                linux_artifact, sizeof(linux_artifact)},
            &forged_result) == W_SEED_MLIR0_OK);
  forged_result.required.outcome_count ^= 1u;
  CHECK(!w_seed_mlir0_verify_float_to_integer_rounding(
      &fixture.hir_program, &fixture.hir_result, &TARGET, linux_artifact,
      forged_result.written.mlir_bytes, &forged_result));
  const w_seed_mlir0_target unsupported = {W_SEED_MLIR0_TARGET_UNSUPPORTED};
  CHECK(w_seed_mlir0_measure_float_to_integer_rounding(
            &fixture.hir_program, &fixture.hir_result, &unsupported,
            &(w_seed_mlir0_float_to_integer_rounding_counts){0},
            &forged_result) == W_SEED_MLIR0_UNSUPPORTED);
  return true;
}

static bool test_typed_cleanup_mlir(void) {
  static const uint8_t source[] =
      "enum Failure: Error { denied }\n"
      "fn clean() { }\n"
      "fn leaf(): i64 throws Failure { throw .denied }\n"
      "fn relay(): i64 throws Failure { defer { clean() } return try leaf() }\n"
      "entry { }\n";
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(fixture.hir_program.function_count == 4u &&
        fixture.hir_program.cleanup_count == 1u &&
        fixture.hir_program.call_count == 3u &&
        fixture.hir_program.instruction_count == 2u);

  w_seed_native_subset0_typed_propagation selection;
  CHECK(w_seed_native_subset0_select_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.cleanup == &fixture.hir_program.cleanups[0] &&
        selection.cleanup_function == &fixture.hir_program.functions[0] &&
        selection.normal_cleanup_call == &fixture.hir_program.calls[1] &&
        selection.error_cleanup_call == &fixture.hir_program.calls[2] &&
        selection.cleanup_function_index == 0u &&
        w_seed_native_subset0_verify_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &selection));

  w_seed_native_subset0_selection scalar_selection;
  w_seed_native_subset0_sequence sequence_selection;
  w_seed_native_subset0_program program_selection;
  w_seed_native_subset0_process process_selection;
  w_seed_cooperative_selection0 cooperative_selection;
  CHECK(w_seed_native_subset0_select(
            &fixture.hir_program, &fixture.hir_result, &scalar_selection) ==
            W_SEED_NATIVE_SUBSET0_UNSUPPORTED &&
        w_seed_native_subset0_select_sequence(
            &fixture.hir_program, &fixture.hir_result, &sequence_selection) ==
            W_SEED_NATIVE_SUBSET0_UNSUPPORTED &&
        w_seed_native_subset0_select_program(
            &fixture.hir_program, &fixture.hir_result, &program_selection) ==
            W_SEED_NATIVE_SUBSET0_UNSUPPORTED &&
        w_seed_native_subset0_select_process(
            &fixture.hir_program, &fixture.hir_result, &process_selection) ==
            W_SEED_NATIVE_SUBSET0_UNSUPPORTED &&
        w_seed_native_subset0_select_cooperative(
            &fixture.hir_program, &fixture.hir_result,
            &cooperative_selection) == W_SEED_NATIVE_SUBSET0_UNSUPPORTED);

  w_seed_mlir0_counts ordinary_counts = {0u};
  w_seed_mlir0_result ordinary_result;
  (void)memset(&ordinary_result, 0xa7, sizeof(ordinary_result));
  const w_seed_mlir0_result ordinary_snapshot = ordinary_result;
  const w_seed_mlir0_input ordinary = mlir_input();
  CHECK(w_seed_mlir0_measure(&ordinary, &TARGET, &ordinary_counts,
                             &ordinary_result) == W_SEED_MLIR0_UNSUPPORTED);
  CHECK(memcmp(&ordinary_result, &ordinary_snapshot,
               sizeof(ordinary_result)) == 0);

  w_seed_mlir0_typed_propagation_counts counts;
  w_seed_mlir0_typed_propagation_result measured;
  CHECK(w_seed_mlir0_measure_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &TARGET, &counts,
            &measured) == W_SEED_MLIR0_OK);
  CHECK(counts.mlir_bytes > 0u &&
        counts.mlir_bytes < W_SEED_MLIR0_MAX_BYTES &&
        counts.function_count == 3u && counts.invoke_count == 1u &&
        counts.cleanup_count == 1u &&
        counts.carrier_field_count ==
            W_SEED_MLIR0_TYPED_PROPAGATION_CARRIER_FIELDS &&
        measured.required.mlir_bytes == counts.mlir_bytes &&
        measured.written.mlir_bytes == 0u);

  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_typed_propagation_result emitted;
  CHECK(w_seed_mlir0_emit_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &TARGET,
            &(w_seed_mlir0_typed_propagation_output){artifact, sizeof(artifact)},
            &emitted) == W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_verify_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &TARGET, artifact,
            emitted.written.mlir_bytes, &emitted) &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "// " W_SEED_MLIR0_TYPED_CLEANUP_SCHEMA_VERSION "\n") &&
        contains_bytes(artifact, emitted.written.mlir_bytes,
                       "llvm.func internal @w_seed_typed_clean()") &&
        count_bytes(artifact, emitted.written.mlir_bytes,
                    "llvm.call @w_seed_typed_clean() : () -> ()") == 2u &&
        count_bytes(artifact, emitted.written.mlir_bytes, "llvm.return") == 4u &&
        !contains_bytes(artifact, emitted.written.mlir_bytes, "llvm.invoke") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes, "unwind") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes, "llvm.alloca") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes, "@main") &&
        !contains_bytes(artifact, emitted.written.mlir_bytes, "ExitProcess"));
  const size_t normal_block = find_bytes(
      artifact, emitted.written.mlir_bytes,
      "^w_seed_typed_relay_b_3(%relay_normal: i64):", 0u);
  const size_t error_block = find_bytes(
      artifact, emitted.written.mlir_bytes,
      "^w_seed_typed_relay_b_4(%relay_error: i1):", normal_block);
  const size_t normal_cleanup = find_bytes(
      artifact, emitted.written.mlir_bytes,
      "llvm.call @w_seed_typed_clean() : () -> ()", normal_block);
  const size_t normal_return =
      find_bytes(artifact, emitted.written.mlir_bytes,
                 "llvm.return %relay_normal_result", normal_cleanup);
  const size_t error_cleanup = find_bytes(
      artifact, emitted.written.mlir_bytes,
      "llvm.call @w_seed_typed_clean() : () -> ()", error_block);
  const size_t error_return =
      find_bytes(artifact, emitted.written.mlir_bytes,
                 "llvm.return %relay_error_result", error_cleanup);
  CHECK(normal_block != SIZE_MAX && error_block != SIZE_MAX &&
        normal_cleanup > normal_block && normal_return > normal_cleanup &&
        normal_return < error_block && error_cleanup > error_block &&
        error_return > error_cleanup);

  uint8_t windows_artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_typed_propagation_result windows_result;
  CHECK(w_seed_mlir0_emit_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &WINDOWS_TARGET,
            &(w_seed_mlir0_typed_propagation_output){windows_artifact,
                                                     sizeof(windows_artifact)},
            &windows_result) == W_SEED_MLIR0_OK);
  CHECK(windows_result.written.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(windows_artifact, artifact, emitted.written.mlir_bytes) == 0 &&
        memcmp(windows_result.mlir_sha256, emitted.mlir_sha256,
               sizeof(emitted.mlir_sha256)) == 0);

  uint8_t cleanup_snapshot[sizeof(fixture.hir_cleanups)];
  (void)memcpy(cleanup_snapshot, fixture.hir_cleanups,
               sizeof(cleanup_snapshot));
  const w_seed_mlir0_typed_propagation_result result_snapshot = emitted;
  CHECK(w_seed_mlir0_emit_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &TARGET,
            &(w_seed_mlir0_typed_propagation_output){
                (uint8_t *)(void *)fixture.hir_cleanups,
                sizeof(fixture.hir_cleanups)},
            &emitted) == W_SEED_MLIR0_ALIAS);
  CHECK(memcmp(fixture.hir_cleanups, cleanup_snapshot,
               sizeof(cleanup_snapshot)) == 0 &&
        memcmp(&emitted, &result_snapshot, sizeof(emitted)) == 0);
  return true;
}

/* Compiler-lifecycle probe used by the repository gate to feed the exact
 * emitted artifact to the pinned MLIR parser and translator.  This is not a
 * public W command or executable ABI. */
static bool emit_typed_propagation_probe(void) {
  static const uint8_t source[] =
      "enum Failure: Error { denied }\n"
      "fn leaf(): i64 throws Failure { throw .denied }\n"
      "fn relay(): i64 throws Failure { return try leaf() }\n"
      "entry { }\n";
  static uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  CHECK(lower_hir(source, sizeof(source) - 1u));
  w_seed_mlir0_typed_propagation_result result;
  CHECK(w_seed_mlir0_emit_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &TARGET,
            &(w_seed_mlir0_typed_propagation_output){artifact,
                                                     sizeof(artifact)},
            &result) == W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_verify_typed_propagation(
      &fixture.hir_program, &fixture.hir_result, &TARGET, artifact,
      result.written.mlir_bytes, &result));
  return fwrite(artifact, 1u, result.written.mlir_bytes, stdout) ==
             result.written.mlir_bytes &&
         fflush(stdout) == 0;
}

static bool emit_typed_cleanup_probe(void) {
  static const uint8_t source[] =
      "enum Failure: Error { denied }\n"
      "fn clean() { }\n"
      "fn leaf(): i64 throws Failure { throw .denied }\n"
      "fn relay(): i64 throws Failure { defer { clean() } return try leaf() }\n"
      "entry { }\n";
  static uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  CHECK(lower_hir(source, sizeof(source) - 1u));
  w_seed_mlir0_typed_propagation_result result;
  CHECK(w_seed_mlir0_emit_typed_propagation(
            &fixture.hir_program, &fixture.hir_result, &TARGET,
            &(w_seed_mlir0_typed_propagation_output){artifact,
                                                     sizeof(artifact)},
            &result) == W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_verify_typed_propagation(
      &fixture.hir_program, &fixture.hir_result, &TARGET, artifact,
      result.written.mlir_bytes, &result));
  return fwrite(artifact, 1u, result.written.mlir_bytes, stdout) ==
             result.written.mlir_bytes &&
         fflush(stdout) == 0;
}

static bool emit_integer_exactly_probe(void) {
  static const uint8_t source[] =
      "fn convert(value: u64): i64 throws NumericConversionError { "
      "return try i64(exactly: value) }\n"
      "entry { }\n";
  static uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  CHECK(lower_hir(source, sizeof(source) - 1u));
  w_seed_mlir0_integer_exactly_result result;
  CHECK(w_seed_mlir0_emit_integer_exactly(
            &fixture.hir_program, &fixture.hir_result, &TARGET,
            &(w_seed_mlir0_integer_exactly_output){artifact,
                                                   sizeof(artifact)},
            &result) == W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_verify_integer_exactly(
      &fixture.hir_program, &fixture.hir_result, &TARGET, artifact,
      result.written.mlir_bytes, &result));
  return fwrite(artifact, 1u, result.written.mlir_bytes, stdout) ==
             result.written.mlir_bytes &&
         fflush(stdout) == 0;
}

static bool emit_float_to_integer_rounding_probe(const char *source_type,
                                                 const char *destination_type,
                                                 const char *mode) {
  if (source_type == NULL || destination_type == NULL || mode == NULL)
    return false;
  char source[384];
  const int source_length = snprintf(
      source, sizeof(source),
      "fn convert(value: %s): %s throws NumericConversionError { "
      "return try %s(rounding: value, mode: .%s) }\nentry { }\n",
      source_type, destination_type, destination_type, mode);
  if (source_length <= 0 || (size_t)source_length >= sizeof(source))
    return false;
  static uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  CHECK(lower_hir((const uint8_t *)source, (size_t)source_length));
  w_seed_mlir0_float_to_integer_rounding_result result;
  CHECK(w_seed_mlir0_emit_float_to_integer_rounding(
            &fixture.hir_program, &fixture.hir_result, &TARGET,
            &(w_seed_mlir0_float_to_integer_rounding_output){
                artifact, sizeof(artifact)},
            &result) == W_SEED_MLIR0_OK);
  CHECK(w_seed_mlir0_verify_float_to_integer_rounding(
      &fixture.hir_program, &fixture.hir_result, &TARGET, artifact,
      result.written.mlir_bytes, &result));
  return fwrite(artifact, 1u, result.written.mlir_bytes, stdout) ==
             result.written.mlir_bytes &&
         fflush(stdout) == 0;
}

static bool emit_process_float_rounding_probe(bool windows) {
  static const uint8_t source[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode throws NumericConversionError { "
      "let rounded = try i8(rounding: 2.5_f64, mode: .nearestEven) "
      "return .success }\nentry(run)\n";
  static uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  process_input_float_rounding_mode = true;
  const bool lowered = lower_process_input_hir(source, sizeof(source) - 1u);
  process_input_float_rounding_mode = false;
  CHECK(lowered);
  const w_seed_mlir0_input input = {
      &fixture.hir_program, &fixture.hir_result,
      W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE};
  w_seed_mlir0_result result;
  CHECK(w_seed_mlir0_emit(
            &input, windows ? &WINDOWS_TARGET : &TARGET,
            &(w_seed_mlir0_output){artifact, sizeof(artifact)}, &result) ==
        W_SEED_MLIR0_OK);
  return fwrite(artifact, 1u, result.written.mlir_bytes, stdout) ==
             result.written.mlir_bytes &&
         fflush(stdout) == 0;
}

static bool test_signed_comparison_artifacts(void) {
  static const char *const operators[] = {"==", "!=", "<", "<=", ">", ">="};
  static const char *const predicates[] = {"eq", "ne", "slt", "sle", "sgt", "sge"};
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  for (size_t operation = 0u; operation < 6u; operation += 1u) {
    char source[1024];
    const int length = snprintf(source, sizeof(source),
        "fn fits(left: i64, right: i64): Bool { return left %s right }\n"
        "fn main() { let fits = fits(left: 0 - 9223372036854775807 - 1, "
        "right: 9223372036854775807) print(\"Fits: ${fits}\") }\nentry(main)\n",
        operators[operation]);
    CHECK(length > 0 && (size_t)length < sizeof(source));
    CHECK(lower_hir((const uint8_t *)source, (size_t)length));
    char expected[80];
    const int expected_length = snprintf(expected, sizeof(expected),
        "llvm.icmp \"%s\" %%p0, %%p1 : i64", predicates[operation]);
    CHECK(expected_length > 0 && (size_t)expected_length < sizeof(expected));
    const w_seed_mlir0_input input = mlir_input();
    for (size_t host = 0u; host < 2u; host += 1u) {
      w_seed_mlir0_result result;
      const w_seed_mlir0_status comparison_status = w_seed_mlir0_emit(
          &input, host == 0u ? &TARGET : &WINDOWS_TARGET,
          &(w_seed_mlir0_output){artifact, sizeof(artifact)}, &result);
      CHECK(comparison_status == W_SEED_MLIR0_OK);
      CHECK(contains_bytes(artifact, result.written.mlir_bytes, expected));
      CHECK(contains_bytes(artifact, result.written.mlir_bytes, "llvm.return %v"));
      CHECK(contains_bytes(artifact, result.written.mlir_bytes, " : i1"));
      CHECK(contains_bytes(artifact, result.written.mlir_bytes,
                           "llvm.call @w_seed_checked_subtract_i64"));
      CHECK(!contains_bytes(artifact, result.written.mlir_bytes, "llvm.sub %v"));
    }
  }
  static const uint8_t linear[] =
      "fn main() { let guests = 3 let seats = 4 let fits = guests <= seats "
      "print(\"Fits: ${fits}; equal: ${3 == 3}\") }\nentry(main)\n";
  CHECK(lower_hir(linear, sizeof(linear) - 1u));
  w_seed_mlir0_result result;
  CHECK(emit_current(artifact, sizeof(artifact), &result));
  CHECK(contains_bytes(artifact, result.written.mlir_bytes, "llvm.icmp \"sle\" %v0, %v1"));
  CHECK(contains_bytes(artifact, result.written.mlir_bytes, "llvm.icmp \"eq\""));
  const w_seed_mlir0_input input = mlir_input();
  w_seed_mlir0_result rejected;
  (void)memset(&rejected, 0x71, sizeof(rejected));
  const w_seed_mlir0_result snapshot = rejected;
  (void)memset(artifact, 0x72, sizeof(artifact));
  CHECK(w_seed_mlir0_emit(&input, &TARGET,
            &(w_seed_mlir0_output){artifact, result.written.mlir_bytes - 1u},
            &rejected) == W_SEED_MLIR0_CAPACITY);
  for (size_t index = 0u; index < sizeof(artifact); index += 1u)
    CHECK(artifact[index] == 0x72u);
  CHECK(memcmp(&rejected, &snapshot, sizeof(rejected)) == 0);
  return true;
}

static bool test_straight_line_mutation_is_ssa(void) {
  static const uint8_t source[] =
      "entry {\n"
      "  var seats = 5\n"
      "  seats = seats + 1\n"
      "  print(\"Open ${seats}\")\n"
      "}\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(fixture.hir_program.binding_count == 2u &&
        fixture.hir_program.bindings[0].is_mutable &&
        fixture.hir_program.bindings[1].source_binding == 0u &&
        fixture.hir_program.bindings[1].previous_version == 0u);
  w_seed_mlir0_result result;
  CHECK(emit_current(artifact, sizeof(artifact), &result));
  const size_t function_start =
      find_bytes(artifact, result.written.mlir_bytes,
                 "llvm.func internal @w_fn_0", 0u);
  const size_t main_start =
      find_bytes(artifact, result.written.mlir_bytes, "llvm.func @main()",
                 function_start);
  CHECK(function_start != SIZE_MAX && main_start != SIZE_MAX &&
        contains_bytes(artifact, result.written.mlir_bytes,
                       "%v3 = llvm.call @w_seed_checked_add_i64(%v0, %v2, "
                       "%v3_checked_width)") &&
        count_bytes(artifact + function_start, main_start - function_start,
                    "llvm.alloca") == 0u &&
        count_bytes(artifact, result.written.mlir_bytes, "llvm.alloca") == 2u);
  return true;
}

static bool test_conditional_mutation_merge_is_ssa(void) {
  static const uint8_t source[] =
      "fn nextSeats(isOpen: Bool): i64 {\n"
      "  var seats = 5\n"
      "  let selected = if isOpen { seats + 1 } else { seats - 1 }\n"
      "  seats = selected\n"
      "  return seats\n"
      "}\n"
      "entry {\n"
      "  let open = nextSeats(isOpen: true)\n"
      "  let closed = nextSeats(isOpen: false)\n"
      "  print(\"Open ${open}; closed ${closed}\")\n"
      "}\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(fixture.hir_program.binding_count == 5u &&
        fixture.hir_program.block_count == 5u &&
        fixture.hir_program.block_argument_count == 1u);
  w_seed_mlir0_result result;
  CHECK(emit_current(artifact, sizeof(artifact), &result));
  const size_t function_start =
      find_bytes(artifact, result.written.mlir_bytes,
                 "llvm.func internal @w_fn_0", 0u);
  const size_t main_start =
      find_bytes(artifact, result.written.mlir_bytes, "llvm.func @main()",
                 function_start);
  CHECK(function_start != SIZE_MAX && main_start != SIZE_MAX &&
        contains_bytes(artifact + function_start, main_start - function_start,
                       "llvm.cond_br %p0, ^w_fn_0_b_1, ^w_fn_0_b_2") &&
        contains_bytes(artifact + function_start, main_start - function_start,
                       "^w_fn_0_b_3(%arg0: i64):") &&
        contains_bytes(artifact + function_start, main_start - function_start,
                       "llvm.return %") &&
        count_bytes(artifact + function_start, main_start - function_start,
                    "llvm.alloca") == 0u);
  return true;
}

static bool test_bool_mutation_is_ssa(void) {
  static const uint8_t source[] =
      "fn availability(requested: Bool): Bool {\n"
      "  var open = false\n"
      "  open = requested\n"
      "  return open\n"
      "}\n"
      "entry {\n"
      "  let open = availability(requested: true)\n"
      "  let closed = availability(requested: false)\n"
      "  print(\"Open ${open}; closed ${closed}\")\n"
      "}\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(fixture.hir_program.binding_count == 4u &&
        fixture.hir_program.bindings[0].type_index == W_SEED_HIR0_TYPE_BOOL &&
        fixture.hir_program.bindings[1].type_index == W_SEED_HIR0_TYPE_BOOL);
  w_seed_mlir0_result result;
  CHECK(emit_current(artifact, sizeof(artifact), &result));
  const size_t function_start =
      find_bytes(artifact, result.written.mlir_bytes,
                 "llvm.func internal @w_fn_0", 0u);
  const size_t entry_start =
      find_bytes(artifact, result.written.mlir_bytes,
                 "llvm.func internal @w_fn_1", function_start);
  CHECK(function_start != SIZE_MAX && entry_start != SIZE_MAX &&
        contains_bytes(artifact + function_start, entry_start - function_start,
                       "%p0: i1) -> i1") &&
        contains_bytes(artifact + function_start, entry_start - function_start,
                       "llvm.return %p0 : i1") &&
        count_bytes(artifact + function_start, entry_start - function_start,
                    "llvm.alloca") == 0u);
  return true;
}

static bool test_branch_local_mutation_merge_is_ssa(void) {
  static const uint8_t source[] =
      "fn nextSeats(isOpen: Bool): i64 {\n"
      "  var seats = 5\n"
      "  if isOpen { seats = seats + 1 }\n"
      "  else { seats = seats - 1 }\n"
      "  return seats\n"
      "}\n"
      "entry {\n"
      "  let open = nextSeats(isOpen: true)\n"
      "  let closed = nextSeats(isOpen: false)\n"
      "  print(\"Open ${open}; closed ${closed}\")\n"
      "}\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(fixture.hir_program.binding_count == 4u &&
        fixture.hir_program.block_count == 5u &&
        fixture.hir_program.block_argument_count == 1u &&
        fixture.hir_program.bindings[1].owner_block == 3u &&
        fixture.hir_program.bindings[1].source_binding == 0u &&
        fixture.hir_program.bindings[1].previous_version == 0u);
  w_seed_mlir0_result result;
  CHECK(emit_current(artifact, sizeof(artifact), &result));
  const size_t function_start =
      find_bytes(artifact, result.written.mlir_bytes,
                 "llvm.func internal @w_fn_0", 0u);
  const size_t entry_start =
      find_bytes(artifact, result.written.mlir_bytes,
                 "llvm.func internal @w_fn_1", function_start);
  CHECK(function_start != SIZE_MAX && entry_start != SIZE_MAX &&
        contains_bytes(artifact + function_start, entry_start - function_start,
                       "llvm.cond_br %p0, ^w_fn_0_b_1, ^w_fn_0_b_2") &&
        contains_bytes(artifact + function_start, entry_start - function_start,
                       "^w_fn_0_b_3(%arg0: i64):") &&
        contains_bytes(artifact + function_start, entry_start - function_start,
                       "llvm.return %arg0 : i64") &&
        count_bytes(artifact + function_start, entry_start - function_start,
                    "llvm.alloca") == 0u);
  return true;
}

static bool test_multi_branch_mutation_merge_is_ssa(void) {
  static const uint8_t source[] =
      "fn nextState(isOpen: Bool): i64 {\n"
      "  var seats = 5\n"
      "  var tables = 2\n"
      "  if isOpen {\n"
      "    tables = tables + 10\n"
      "    seats = seats + 1\n"
      "  } else {\n"
      "    seats = seats - 1\n"
      "    tables = tables - 10\n"
      "  }\n"
      "  return seats + tables\n"
      "}\n"
      "entry {\n"
      "  let open = nextState(isOpen: true)\n"
      "  let closed = nextState(isOpen: false)\n"
      "  print(\"Open ${open}; closed ${closed}\")\n"
      "}\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(fixture.hir_program.binding_count == 6u &&
        fixture.hir_program.block_count == 5u &&
        fixture.hir_program.block_argument_count == 2u &&
        fixture.hir_program.blocks[3].block_argument_count == 2u &&
        fixture.hir_program.terminators[0].result_type == 0u);
  CHECK(fixture.hir_program.terminators[1].edge_argument_count == 2u &&
        fixture.hir_program.terminators[2].edge_argument_count == 2u &&
        fixture.hir_program.edge_arguments[0].ordinal == 0u &&
        fixture.hir_program.edge_arguments[1].ordinal == 1u &&
        fixture.hir_program.edge_arguments[2].ordinal == 0u &&
        fixture.hir_program.edge_arguments[3].ordinal == 1u);
  w_seed_mlir0_result result;
  CHECK(emit_current(artifact, sizeof(artifact), &result));
  const size_t function_start =
      find_bytes(artifact, result.written.mlir_bytes,
                 "llvm.func internal @w_fn_0", 0u);
  const size_t entry_start =
      find_bytes(artifact, result.written.mlir_bytes,
                 "llvm.func internal @w_fn_1", function_start);
  CHECK(function_start != SIZE_MAX && entry_start > function_start);
  const size_t function_bytes = entry_start - function_start;
  CHECK(contains_bytes(artifact + function_start, function_bytes,
                       "llvm.cond_br %p0, ^w_fn_0_b_1, ^w_fn_0_b_2") &&
        contains_bytes(artifact + function_start, function_bytes,
                       "^w_fn_0_b_3(%arg0: i64, %arg1: i64):") &&
        count_bytes(artifact + function_start, function_bytes,
                    "llvm.br ^w_fn_0_b_3(%v") == 2u &&
        contains_bytes(artifact + function_start, function_bytes,
                       "llvm.return %v") &&
        count_bytes(artifact + function_start, function_bytes,
                    "llvm.alloca") == 0u);
  return true;
}

static bool test_natural_loop_preserves_structured_mlir(void) {
  static const uint8_t source[] =
      "fn countTo(limit: i64): i64 {\n"
      "  var count = 0\n"
      "  while count < limit { count = count + 1 }\n"
      "  return count\n"
      "}\n"
      "entry {\n"
      "  let count = countTo(limit: 3)\n"
      "  print(\"Count ${count}\")\n"
      "}\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(fixture.hir_program.function_count == 2u &&
        fixture.hir_program.functions[0].block_count == 4u &&
        fixture.hir_program.block_argument_count == 1u &&
        fixture.hir_program.edge_argument_count == 2u);
  w_seed_mlir0_result result;
  CHECK(emit_current(artifact, sizeof(artifact), &result));
  const size_t function_start =
      find_bytes(artifact, result.written.mlir_bytes,
                 "llvm.func internal @w_fn_0", 0u);
  const size_t entry_start =
      find_bytes(artifact, result.written.mlir_bytes,
                 "llvm.func internal @w_fn_1", function_start);
  CHECK(function_start != SIZE_MAX && entry_start > function_start);
  const size_t function_bytes = entry_start - function_start;
  CHECK(contains_bytes(artifact + function_start, function_bytes,
                       " = scf.while (") &&
        contains_bytes(artifact + function_start, function_bytes,
                       "scf.condition(") &&
        contains_bytes(artifact + function_start, function_bytes,
                       "scf.yield ") &&
        contains_bytes(artifact + function_start, function_bytes,
                       "llvm.return %loop0 : i64") &&
        !contains_bytes(artifact + function_start, function_bytes,
                        "llvm.br ^w_fn_0_b_1") &&
        count_bytes(artifact + function_start, function_bytes,
                    "llvm.alloca") == 0u);
  return true;
}

static bool test_natural_loop_multi_carrier_projection_mlir(void) {
  static const uint8_t source[] =
      "fn accumulate(limit: i64): i64 {\n"
      "  var count = 0\n"
      "  var total = 10\n"
      "  while count < limit {\n"
      "    total = total + 2\n"
      "    count = count + 1\n"
      "  }\n"
      "  return count + total\n"
      "}\n"
      "entry {\n"
      "  let total = accumulate(limit: 3)\n"
      "  print(\"Total ${total}\")\n"
      "}\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(fixture.hir_program.functions[0].block_count == 4u &&
        fixture.hir_program.blocks[1].block_argument_count == 2u &&
        fixture.hir_program.blocks[0].instruction_count == 2u &&
        fixture.hir_program.blocks[2].instruction_count == 2u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.natural_loop_functions[0]);
  w_seed_mlir0_result result;
  CHECK(emit_current(artifact, sizeof(artifact), &result));
  const size_t function_start =
      find_bytes(artifact, result.written.mlir_bytes,
                 "llvm.func internal @w_fn_0", 0u);
  const size_t entry_start =
      find_bytes(artifact, result.written.mlir_bytes,
                 "llvm.func internal @w_fn_1", function_start);
  CHECK(function_start != SIZE_MAX && entry_start > function_start);
  const size_t function_bytes = entry_start - function_start;
  CHECK(contains_bytes(artifact + function_start, function_bytes,
                       "%loop0_0, %loop0_1 = scf.while ("));
  CHECK(contains_bytes(artifact + function_start, function_bytes,
                       ") : (i64, i64) -> (i64, i64) {"));
  CHECK(contains_bytes(artifact + function_start, function_bytes,
                       "scf.condition("));
  CHECK(contains_bytes(artifact + function_start, function_bytes,
                       "scf.yield "));
  CHECK(contains_bytes(artifact + function_start, function_bytes,
                       "@w_seed_checked_add_i64(%loop0_0, %loop0_1, %v"));
  CHECK(contains_bytes(artifact + function_start, function_bytes,
                       "llvm.return %v"));
  CHECK(!contains_bytes(artifact + function_start, function_bytes,
                        "llvm.br ^w_fn_0_b_1"));
  CHECK(count_bytes(artifact + function_start, function_bytes,
                    "llvm.alloca") == 0u);
  return true;
}

static bool test_conditional_exit_loop_uses_typed_cfg_mlir(void) {
  static const uint8_t source[] =
      "fn scan(limit: i64): i64 {\n"
      "  var index = 0\n"
      "  var total = 0\n"
      "  while index < limit {\n"
      "    index = index + 1\n"
      "    if index == 2 { continue }\n"
      "    if index == 5 { break }\n"
      "    total = total + index\n"
      "  }\n"
      "  return total\n"
      "}\n"
      "entry {\n"
      "  let result = scan(limit: 9)\n"
      "  print(\"${result}\")\n"
      "}\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(fixture.hir_program.function_count == 2u);
  const w_seed_hir0_function *function = &fixture.hir_program.functions[0];
  CHECK(function->block_count >= 5u &&
        function->return_type < fixture.hir_program.type_count &&
        fixture.hir_program.types[function->return_type].kind ==
            W_SEED_HIR0_TYPE_I64);

  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.verified_i64_loop_cfg_functions[0] &&
        !selection.natural_loop_functions[0] &&
        !selection.post_test_loop_functions[0] && selection.has_cfg);

  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0);
  const size_t function_start =
      find_bytes(artifact, emitted.written.mlir_bytes,
                 "llvm.func internal @w_fn_0", 0u);
  const size_t entry_start =
      find_bytes(artifact, emitted.written.mlir_bytes,
                 "llvm.func internal @w_fn_1", function_start);
  CHECK(function_start != SIZE_MAX && entry_start > function_start);
  const size_t function_bytes = entry_start - function_start;
  const uint8_t *function_artifact = artifact + function_start;
  size_t branch_count = 0u;
  size_t jump_count = 0u;
  size_t two_i64_argument_tuples = 0u;
  size_t two_i64_jump_count = 0u;
  bool saw_typed_jump_arguments = false;
  bool saw_back_edge = false;
  bool saw_i64_return = false;
  const size_t block_end = (size_t)function->first_block + function->block_count;
  CHECK(block_end <= fixture.hir_program.block_count);
  for (size_t block_index = function->first_block; block_index < block_end;
       block_index += 1u) {
    const w_seed_hir0_block *block = &fixture.hir_program.blocks[block_index];
    CHECK(block->owner_function == 0u &&
          block->terminator_index < fixture.hir_program.terminator_count);
    if (block->block_argument_count == 2u) {
      two_i64_argument_tuples += 1u;
      CHECK(block->first_block_argument != W_SEED_HIR0_NONE &&
            block->first_block_argument + 1u <
                fixture.hir_program.block_argument_count);
      char expected_arguments[128];
      const int expected_arguments_length = snprintf(
          expected_arguments, sizeof(expected_arguments),
          "^w_fn_0_b_%llu(%%arg%u: i64, %%arg%u: i64):",
          (unsigned long long)block_index,
          block->first_block_argument, block->first_block_argument + 1u);
      CHECK(expected_arguments_length > 0 &&
            (size_t)expected_arguments_length < sizeof(expected_arguments) &&
            contains_bytes(function_artifact, function_bytes,
                           expected_arguments));
      for (size_t argument = 0u; argument < 2u; argument += 1u) {
        const w_seed_hir0_block_argument *item =
            &fixture.hir_program.block_arguments[
                (size_t)block->first_block_argument + argument];
        CHECK(item->owner_block == block_index &&
              item->type_index < fixture.hir_program.type_count &&
              fixture.hir_program.types[item->type_index].kind ==
                  W_SEED_HIR0_TYPE_I64);
      }
    }
    const w_seed_hir0_terminator *terminator =
        &fixture.hir_program.terminators[block->terminator_index];
    if (terminator->kind == W_SEED_HIR0_TERMINATOR_BRANCH) {
      char expected_branch[192];
      CHECK(terminator->value_index < fixture.hir_program.value_count);
      const w_seed_hir0_value *condition =
          &fixture.hir_program.values[terminator->value_index];
      CHECK(condition->type_index < fixture.hir_program.type_count &&
            fixture.hir_program.types[condition->type_index].kind ==
                W_SEED_HIR0_TYPE_BOOL);
      const int expected_length = snprintf(
          expected_branch, sizeof(expected_branch),
          "llvm.cond_br %%v%u, ^w_fn_0_b_%u, ^w_fn_0_b_%u",
          terminator->value_index, terminator->target_block,
          terminator->else_block);
      CHECK(expected_length > 0 &&
            (size_t)expected_length < sizeof(expected_branch) &&
            contains_bytes(function_artifact, function_bytes,
                           expected_branch));
      branch_count += 1u;
    } else if (terminator->kind == W_SEED_HIR0_TERMINATOR_JUMP) {
      CHECK(terminator->target_block >= function->first_block &&
            terminator->target_block < block_end);
      const w_seed_hir0_block *target =
          &fixture.hir_program.blocks[terminator->target_block];
      CHECK(terminator->edge_argument_count == target->block_argument_count);
      if (terminator->edge_argument_count != 0u) {
        char expected_target[96];
        const int target_length = snprintf(
            expected_target, sizeof(expected_target),
            "llvm.br ^w_fn_0_b_%u(", terminator->target_block);
        CHECK(target_length > 0 &&
              (size_t)target_length < sizeof(expected_target) &&
              contains_bytes(function_artifact, function_bytes,
                             expected_target));
        size_t type_suffix_bytes = 0u;
        char type_suffix[96] = " :";
        for (size_t argument = 0u;
             argument < terminator->edge_argument_count; argument += 1u) {
          const w_seed_hir0_edge_argument *edge =
              &fixture.hir_program.edge_arguments[
                  (size_t)terminator->first_edge_argument + argument];
          CHECK(edge->type_index < fixture.hir_program.type_count &&
                fixture.hir_program.types[edge->type_index].kind ==
                    W_SEED_HIR0_TYPE_I64);
          const int suffix_length = snprintf(
              type_suffix + type_suffix_bytes,
              sizeof(type_suffix) - type_suffix_bytes,
              "%s i64", argument == 0u ? "" : ",");
          CHECK(suffix_length > 0 &&
                (size_t)suffix_length <
                    sizeof(type_suffix) - type_suffix_bytes);
          type_suffix_bytes += (size_t)suffix_length;
        }
        CHECK(type_suffix_bytes + 2u < sizeof(type_suffix));
        type_suffix[type_suffix_bytes] = ')';
        type_suffix[type_suffix_bytes + 1u] = '\0';
        CHECK(contains_bytes(function_artifact, function_bytes,
                             type_suffix));
        if (terminator->edge_argument_count == 2u) {
          saw_typed_jump_arguments = true;
          two_i64_jump_count += 1u;
        }
      }
      if (terminator->target_block < block_index) saw_back_edge = true;
      jump_count += 1u;
    } else if (terminator->kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE) {
      CHECK(terminator->value_index < fixture.hir_program.value_count &&
            contains_bytes(function_artifact, function_bytes,
                           "llvm.return ") &&
            contains_bytes(function_artifact, function_bytes,
                           " : i64\n"));
      saw_i64_return = true;
    }
  }
  CHECK(two_i64_argument_tuples == 2u && branch_count >= 2u &&
        jump_count >= 2u && saw_typed_jump_arguments && saw_back_edge &&
        saw_i64_return);
  CHECK(count_bytes(function_artifact, function_bytes, "llvm.cond_br ") ==
            branch_count &&
        count_bytes(function_artifact, function_bytes, "llvm.br ^w_fn_0_b_") ==
            jump_count &&
        count_bytes(function_artifact, function_bytes,
                    " : i64, i64)\n") == two_i64_jump_count &&
        !contains_bytes(function_artifact, function_bytes, "scf.while") &&
        !contains_bytes(function_artifact, function_bytes, "scf.yield") &&
        !contains_bytes(function_artifact, function_bytes, "llvm.alloca"));

  uint32_t bool_type_index = W_SEED_HIR0_NONE;
  uint32_t edge_to_corrupt = W_SEED_HIR0_NONE;
  for (size_t type = 0u; type < fixture.hir_program.type_count; type += 1u)
    if (fixture.hir_program.types[type].kind == W_SEED_HIR0_TYPE_BOOL) {
      bool_type_index = (uint32_t)type;
      break;
    }
  for (size_t block_index = function->first_block; block_index < block_end;
       block_index += 1u) {
    const w_seed_hir0_terminator *terminator =
        &fixture.hir_program.terminators[
            fixture.hir_program.blocks[block_index].terminator_index];
    if (terminator->kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        terminator->edge_argument_count != 0u) {
      edge_to_corrupt = terminator->first_edge_argument;
      break;
    }
  }
  CHECK(bool_type_index != W_SEED_HIR0_NONE &&
        edge_to_corrupt != W_SEED_HIR0_NONE &&
        edge_to_corrupt < fixture.hir_program.edge_argument_count);
  const w_seed_hir0_edge_argument saved_edge =
      fixture.hir_program.edge_arguments[edge_to_corrupt];
  CHECK(saved_edge.type_index != bool_type_index);
  fixture.hir_edge_arguments[edge_to_corrupt].type_index = bool_type_index;

  const w_seed_mlir0_input invalid_input = mlir_input();
  w_seed_mlir0_counts invalid_counts = {0x37u};
  const w_seed_mlir0_counts invalid_counts_snapshot = invalid_counts;
  w_seed_mlir0_result invalid_measure_result;
  (void)memset(&invalid_measure_result, 0x4au,
               sizeof(invalid_measure_result));
  const w_seed_mlir0_result invalid_measure_snapshot =
      invalid_measure_result;
  CHECK(w_seed_mlir0_measure(&invalid_input, &TARGET, &invalid_counts,
                             &invalid_measure_result) ==
        W_SEED_MLIR0_INVALID_HIR);
  CHECK(memcmp(&invalid_counts, &invalid_counts_snapshot,
               sizeof(invalid_counts)) == 0 &&
        memcmp(&invalid_measure_result, &invalid_measure_snapshot,
               sizeof(invalid_measure_result)) == 0);

  uint8_t rejected_output[W_SEED_MLIR0_MAX_BYTES];
  (void)memset(rejected_output, 0xa5u, sizeof(rejected_output));
  w_seed_mlir0_result invalid_emit_result;
  (void)memset(&invalid_emit_result, 0x5bu, sizeof(invalid_emit_result));
  const w_seed_mlir0_result invalid_emit_snapshot = invalid_emit_result;
  CHECK(w_seed_mlir0_emit(
            &invalid_input, &TARGET,
            &(w_seed_mlir0_output){rejected_output, sizeof(rejected_output)},
            &invalid_emit_result) == W_SEED_MLIR0_INVALID_HIR);
  for (size_t byte = 0u; byte < sizeof(rejected_output); byte += 1u)
    CHECK(rejected_output[byte] == 0xa5u);
  CHECK(memcmp(&invalid_emit_result, &invalid_emit_snapshot,
               sizeof(invalid_emit_result)) == 0);
  fixture.hir_edge_arguments[edge_to_corrupt] = saved_edge;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool nested_hir_is_rejected_without_publication(void) {
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  const w_seed_mlir0_input input = mlir_input();
  w_seed_mlir0_counts counts = {0x3bu};
  w_seed_mlir0_result result;
  (void)memset(&result, 0x4du, sizeof(result));
  const w_seed_mlir0_counts counts_snapshot = counts;
  const w_seed_mlir0_result result_snapshot = result;
  CHECK(w_seed_mlir0_measure(&input, &TARGET, &counts, &result) ==
        W_SEED_MLIR0_INVALID_HIR);
  CHECK(memcmp(&counts, &counts_snapshot, sizeof(counts)) == 0 &&
        memcmp(&result, &result_snapshot, sizeof(result)) == 0);
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  (void)memset(output, 0xa7u, sizeof(output));
  CHECK(w_seed_mlir0_emit(
            &input, &TARGET,
            &(w_seed_mlir0_output){output, sizeof(output)}, &result) ==
        W_SEED_MLIR0_INVALID_HIR);
  for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
    CHECK(output[byte] == 0xa7u);
  CHECK(memcmp(&result, &result_snapshot, sizeof(result)) == 0);
  return true;
}

static bool test_nested_labeled_while_uses_verified_cfg_mlir(void) {
  /* Mirrors fixtures/nested-labeled-while.w: exit 0; stdout "0,1,3\n". */
  static const uint8_t source[] =
      "fn walk(limit: i64): i64 {\n"
      "  var outer = 0\n"
      "  var inner = 0\n"
      "  var total = 0\n"
      "  outerLoop: while outer < limit {\n"
      "    outer = outer + 1\n"
      "    inner = 0\n"
      "    while inner < limit {\n"
      "      inner = inner + 1\n"
      "      if inner == 2 { continue }\n"
      "      if inner == 3 { break }\n"
      "      if outer == 4 { continue outerLoop }\n"
      "      if outer == 5 { break outerLoop }\n"
      "      total = total + 1\n"
      "    }\n"
      "  }\n"
      "  return total\n"
      "}\n"
      "\n"
      "entry {\n"
      "  let zero = walk(limit: 0)\n"
      "  let one = walk(limit: 1)\n"
      "  let six = walk(limit: 6)\n"
      "  print(\"${zero},${one},${six}\")\n"
      "}\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(fixture.hir_program.function_count == 2u);
  const w_seed_hir0_function *function = &fixture.hir_program.functions[0];
  CHECK(function->block_count >= 2u &&
        fixture.hir_program.types[function->return_type].kind ==
            W_SEED_HIR0_TYPE_I64);

  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.verified_i64_loop_cfg_functions[0] &&
        !selection.natural_loop_functions[0] &&
        selection.has_cfg && !selection.has_enum_switch);
  CHECK(w_seed_product_closure0_cross_check_functions(
      &fixture.hir_program, &fixture.hir_result, (const bool[]){true, true},
      fixture.hir_program.function_count));

  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  w_seed_mlir0_result emitted;
  CHECK(measure_current(&counts, &measured));
  CHECK(emit_current(artifact, sizeof(artifact), &emitted));
  CHECK(counts.mlir_bytes == emitted.written.mlir_bytes &&
        memcmp(measured.mlir_sha256, emitted.mlir_sha256,
               sizeof(measured.mlir_sha256)) == 0);
  const size_t function_start =
      find_bytes(artifact, emitted.written.mlir_bytes,
                 "llvm.func internal @w_fn_0", 0u);
  const size_t entry_start =
      find_bytes(artifact, emitted.written.mlir_bytes,
                 "llvm.func internal @w_fn_1", function_start);
  CHECK(function_start != SIZE_MAX && entry_start > function_start);
  const size_t function_bytes = entry_start - function_start;
  size_t block_arguments = 0u;
  size_t branches = 0u;
  size_t jumps = 0u;
  size_t backedges = 0u;
  const size_t block_end = (size_t)function->first_block + function->block_count;
  CHECK(block_end <= fixture.hir_program.block_count);
  for (size_t block_index = function->first_block; block_index < block_end;
       block_index += 1u) {
    const w_seed_hir0_block *block = &fixture.hir_program.blocks[block_index];
    CHECK(block->owner_function == 0u &&
          block->terminator_index < fixture.hir_program.terminator_count);
    block_arguments += block->block_argument_count;
    const w_seed_hir0_terminator *term =
        &fixture.hir_program.terminators[block->terminator_index];
    if (term->kind == W_SEED_HIR0_TERMINATOR_BRANCH) {
      char expected[192];
      const int length = snprintf(
          expected, sizeof(expected),
          "llvm.cond_br %%v%u, ^w_fn_0_b_%u, ^w_fn_0_b_%u", term->value_index,
          term->target_block, term->else_block);
      CHECK(length > 0 && (size_t)length < sizeof(expected) &&
            contains_bytes(artifact + function_start, function_bytes,
                           expected));
      branches += 1u;
    } else if (term->kind == W_SEED_HIR0_TERMINATOR_JUMP) {
      char expected[96];
      const int length = snprintf(expected, sizeof(expected),
                                  "llvm.br ^w_fn_0_b_%u", term->target_block);
      CHECK(length > 0 && (size_t)length < sizeof(expected) &&
            contains_bytes(artifact + function_start, function_bytes,
                           expected));
      if (term->target_block < block_index) backedges += 1u;
      jumps += 1u;
    }
  }
  CHECK(block_arguments >= 4u && branches >= 4u && jumps >= 4u &&
        backedges >= 2u &&
        count_bytes(artifact + function_start, function_bytes,
                    "llvm.cond_br ") == branches &&
        count_bytes(artifact + function_start, function_bytes,
                    "llvm.br ^w_fn_0_b_") == jumps &&
        contains_bytes(artifact + function_start, function_bytes,
                       "llvm.return ") &&
        !contains_bytes(artifact + function_start, function_bytes,
                        "scf.while") &&
        !contains_bytes(artifact + function_start, function_bytes,
                        "llvm.alloca"));

  /* Every downstream consumer re-verifies the HIR graph and exact carrier
   * records; these mutations cannot be normalized into a source-shape case. */
  uint32_t branch_term_index = W_SEED_HIR0_NONE;
  uint32_t jump_term_index = W_SEED_HIR0_NONE;
  uint32_t argument_index = W_SEED_HIR0_NONE;
  uint32_t edge_index = W_SEED_HIR0_NONE;
  uint32_t bool_type_index = W_SEED_HIR0_NONE;
  for (size_t type = 0u; type < fixture.hir_program.type_count; type += 1u)
    if (fixture.hir_program.types[type].kind == W_SEED_HIR0_TYPE_BOOL)
      bool_type_index = (uint32_t)type;
  for (size_t block_index = function->first_block; block_index < block_end;
       block_index += 1u) {
    const w_seed_hir0_block *block = &fixture.hir_program.blocks[block_index];
    if (argument_index == W_SEED_HIR0_NONE &&
        block->block_argument_count >= 2u)
      argument_index = block->first_block_argument;
    const w_seed_hir0_terminator *term =
        &fixture.hir_program.terminators[block->terminator_index];
    if (term->kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        branch_term_index == W_SEED_HIR0_NONE)
      branch_term_index = block->terminator_index;
    if (term->kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        term->edge_argument_count != 0u) {
      jump_term_index = block->terminator_index;
      edge_index = term->first_edge_argument;
    }
  }
  CHECK(branch_term_index != W_SEED_HIR0_NONE &&
        jump_term_index != W_SEED_HIR0_NONE &&
        argument_index != W_SEED_HIR0_NONE &&
        edge_index != W_SEED_HIR0_NONE && bool_type_index != W_SEED_HIR0_NONE);

  w_seed_hir0_terminator saved_term =
      fixture.hir_terminators[branch_term_index];
  fixture.hir_terminators[branch_term_index].target_block =
      W_SEED_HIR0_NONE;
  CHECK(nested_hir_is_rejected_without_publication());
  fixture.hir_terminators[branch_term_index] = saved_term;

  w_seed_hir0_edge_argument saved_edge =
      fixture.hir_edge_arguments[edge_index];
  fixture.hir_edge_arguments[edge_index].value_index = W_SEED_HIR0_NONE;
  CHECK(nested_hir_is_rejected_without_publication());
  fixture.hir_edge_arguments[edge_index] = saved_edge;

  w_seed_hir0_block_argument saved_argument =
      fixture.hir_block_arguments[argument_index];
  fixture.hir_block_arguments[argument_index].ordinal = UINT32_MAX;
  CHECK(nested_hir_is_rejected_without_publication());
  fixture.hir_block_arguments[argument_index] = saved_argument;

  fixture.hir_block_arguments[argument_index].type_index = bool_type_index;
  CHECK(nested_hir_is_rejected_without_publication());
  fixture.hir_block_arguments[argument_index] = saved_argument;

  w_seed_hir0_block saved_block =
      fixture.hir_blocks[fixture.hir_terminators[jump_term_index].owner_block];
  const uint32_t owner_block = fixture.hir_terminators[jump_term_index].owner_block;
  fixture.hir_blocks[owner_block].owner_function = 1u;
  CHECK(nested_hir_is_rejected_without_publication());
  fixture.hir_blocks[owner_block] = saved_block;

  fixture.hir_result.semantic_digest[0] ^= 1u;
  CHECK(nested_hir_is_rejected_without_publication());
  fixture.hir_result.semantic_digest[0] ^= 1u;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));

  /* Capacity and output/input alias failures preserve both buffers. */
  uint8_t limited[W_SEED_MLIR0_MAX_BYTES];
  (void)memset(limited, 0xc9u, sizeof(limited));
  w_seed_mlir0_result failed_result;
  (void)memset(&failed_result, 0x6du, sizeof(failed_result));
  const w_seed_mlir0_result failed_result_snapshot = failed_result;
  CHECK(w_seed_mlir0_emit(
            &(w_seed_mlir0_input){&fixture.hir_program, &fixture.hir_result,
                                  W_SEED_MLIR0_ARTIFACT_EXECUTABLE},
            &TARGET,
            &(w_seed_mlir0_output){limited, counts.mlir_bytes - 1u},
            &failed_result) == W_SEED_MLIR0_CAPACITY);
  for (size_t byte = 0u; byte < sizeof(limited); byte += 1u)
    CHECK(limited[byte] == 0xc9u);
  CHECK(memcmp(&failed_result, &failed_result_snapshot,
               sizeof(failed_result)) == 0);

  uint8_t blocks_snapshot[sizeof(fixture.hir_blocks)];
  (void)memcpy(blocks_snapshot, fixture.hir_blocks, sizeof(blocks_snapshot));
  CHECK(w_seed_mlir0_emit(
            &(w_seed_mlir0_input){&fixture.hir_program, &fixture.hir_result,
                                  W_SEED_MLIR0_ARTIFACT_EXECUTABLE},
            &TARGET,
            &(w_seed_mlir0_output){(uint8_t *)(void *)fixture.hir_blocks,
                                   sizeof(fixture.hir_blocks)},
            &failed_result) == W_SEED_MLIR0_ALIAS);
  CHECK(memcmp(blocks_snapshot, fixture.hir_blocks, sizeof(blocks_snapshot)) ==
            0 &&
        memcmp(&failed_result, &failed_result_snapshot,
               sizeof(failed_result)) == 0);
  return true;
}

static bool test_natural_loop_post_loop_continuation_mlir(void) {
  static const uint8_t source[] =
      "fn settle(limit: i64): i64 {\n"
      "  var served = 0\n"
      "  var total = 0\n"
      "  while served < limit {\n"
      "    total = total + 2\n"
      "    served = served + 1\n"
      "  }\n"
      "  total = total + served\n"
      "  return total\n"
      "}\n"
      "entry {\n"
      "  let result = settle(limit: 3)\n"
      "  print(\"Final ${result}\")\n"
      "}\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(fixture.hir_program.function_count == 2u &&
        fixture.hir_program.functions[0].block_count == 4u &&
        fixture.hir_program.blocks[1].block_argument_count == 2u &&
        fixture.hir_program.blocks[3].instruction_count == 1u &&
        fixture.hir_program.bindings[4].previous_version == 2u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.natural_loop_functions[0]);
  w_seed_mlir0_result result;
  CHECK(emit_current(artifact, sizeof(artifact), &result));
  const size_t function_start =
      find_bytes(artifact, result.written.mlir_bytes,
                 "llvm.func internal @w_fn_0", 0u);
  CHECK(function_start != SIZE_MAX);
  const size_t function_end =
      find_bytes(artifact, result.written.mlir_bytes,
                 "\n  }\n", function_start);
  CHECK(function_end > function_start);
  const size_t function_bytes = function_end + 5u - function_start;
  const uint8_t *function_artifact = artifact + function_start;
  const size_t loop =
      find_bytes(function_artifact, function_bytes, " = scf.while (", 0u);
  const size_t continuation = find_bytes(
      function_artifact, function_bytes,
      " = llvm.call @w_seed_checked_add_i64(%loop0_1, %loop0_0, %v", loop);
  const size_t returned =
      find_bytes(function_artifact, function_bytes, "llvm.return %v", continuation);
  CHECK(loop != SIZE_MAX && continuation > loop && returned > continuation &&
        contains_bytes(function_artifact, function_bytes,
                       "%loop0_0, %loop0_1 = scf.while (") &&
        count_bytes(function_artifact, function_bytes, "llvm.alloca") == 0u);
  return true;
}

static bool test_post_test_repeat_structured_mlir(void) {
  static const uint8_t source[] =
      "fn receiptDigits(value: i64): i64 {\n"
      "  var remaining = value\n"
      "  var digits = 0\n"
      "  repeat {\n"
      "    digits = digits + 1\n"
      "    remaining = remaining / 10\n"
      "  } while remaining > 0\n"
      "  return digits\n"
      "}\n"
      "entry {\n"
      "  let zero = receiptDigits(value: 0)\n"
      "  let cosmic = receiptDigits(value: 42424)\n"
      "  print(\"Receipt digits ${zero}/${cosmic}\")\n"
      "}\n";
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  CHECK(lower_hir(source, sizeof(source) - 1u));
  CHECK(fixture.hir_program.function_count == 2u &&
        fixture.hir_program.functions[0].block_count == 5u &&
        fixture.hir_program.blocks[1].block_argument_count == 2u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(!selection.natural_loop_functions[0] &&
        selection.post_test_loop_functions[0]);
  w_seed_mlir0_result result;
  CHECK(emit_current(artifact, sizeof(artifact), &result));
  const size_t function_start =
      find_bytes(artifact, result.written.mlir_bytes,
                 "llvm.func internal @w_fn_0", 0u);
  const size_t entry_start =
      find_bytes(artifact, result.written.mlir_bytes,
                 "llvm.func internal @w_fn_1", function_start);
  CHECK(function_start != SIZE_MAX && entry_start > function_start);
  const size_t function_bytes = entry_start - function_start;
  const uint8_t *function_artifact = artifact + function_start;
  CHECK(contains_bytes(function_artifact, function_bytes,
                       " = scf.while (") &&
        contains_bytes(function_artifact, function_bytes,
                       "scf.condition(%post_test_before0)") &&
        contains_bytes(function_artifact, function_bytes, "scf.yield ") &&
        contains_bytes(function_artifact, function_bytes, "llvm.sdiv") &&
        contains_bytes(function_artifact, function_bytes,
                       "llvm.icmp \"sgt\"") &&
        contains_bytes(function_artifact, function_bytes,
                       "llvm.return %loop0_1 : i64") &&
        !contains_bytes(function_artifact, function_bytes,
                        "llvm.br ^w_fn_0_b_") &&
        count_bytes(function_artifact, function_bytes, "llvm.alloca") == 0u);
  return true;
}

int main(int argc, char **argv) {
  if (argc == 2 && argv[1] != NULL &&
      strcmp(argv[1], "--emit-typed-propagation") == 0) {
#if defined(_WIN32)
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 3;
#endif
    return emit_typed_propagation_probe() ? 0 : 1;
  }
  if (argc == 2 && argv[1] != NULL &&
      strcmp(argv[1], "--emit-typed-cleanup") == 0) {
#if defined(_WIN32)
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 3;
#endif
    return emit_typed_cleanup_probe() ? 0 : 1;
  }
  if (argc == 2 && argv[1] != NULL &&
      strcmp(argv[1], "--emit-integer-exactly") == 0) {
#if defined(_WIN32)
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 3;
#endif
    return emit_integer_exactly_probe() ? 0 : 1;
  }
  if (argc == 5 && argv[1] != NULL && argv[2] != NULL && argv[3] != NULL &&
      argv[4] != NULL &&
      strcmp(argv[1], "--emit-float-to-integer-rounding") == 0) {
#if defined(_WIN32)
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 3;
#endif
    return emit_float_to_integer_rounding_probe(argv[2], argv[3], argv[4])
               ? 0
               : 1;
  }
  if (argc == 2 && argv[1] != NULL &&
      strcmp(argv[1], "--emit-process-float-rounding") == 0) {
#if defined(_WIN32)
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 3;
#endif
    return emit_process_float_rounding_probe(false) ? 0 : 1;
  }
  if (argc == 2 && argv[1] != NULL &&
      strcmp(argv[1], "--emit-process-float-rounding-windows") == 0) {
#if defined(_WIN32)
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 3;
#endif
    return emit_process_float_rounding_probe(true) ? 0 : 1;
  }
  if (argc != 1) return 2;
  if (!test_reachable_panic_mlir()) return 1;
  if (!test_process_panic_mlir()) return 1;
  if (!test_process_checked_integer_helper_fault_mlir()) return 1;
  if (!test_process_float_rounding_native_subset()) return 1;
  if (!test_process_float_rounding_join_mlir()) return 1;
  if (!test_process_hir_is_closed_to_mlir()) return 1;
  if (!test_process_arguments_count_comparison_mlir()) return 1;
  if (!test_process_arguments_value_lane_mlir()) return 1;
  if (!test_process_arguments_count_ordered_mlir()) return 1;
  if (!test_enum_switch_mlir()) return 1;
  if (!test_enum_if_value_join_mlir()) return 1;
  if (!test_enum_subset_switch_mlir()) return 1;
  if (!test_signed_comparison_artifacts()) return 1;
  if (!test_straight_line_mutation_is_ssa()) return 1;
  if (!test_conditional_mutation_merge_is_ssa()) return 1;
  if (!test_bool_mutation_is_ssa()) return 1;
  if (!test_branch_local_mutation_merge_is_ssa()) return 1;
  if (!test_multi_branch_mutation_merge_is_ssa()) return 1;
  if (!test_natural_loop_preserves_structured_mlir()) return 1;
  if (!test_natural_loop_multi_carrier_projection_mlir()) return 1;
  if (!test_conditional_exit_loop_uses_typed_cfg_mlir()) return 1;
  if (!test_nested_labeled_while_uses_verified_cfg_mlir()) return 1;
  if (!test_natural_loop_post_loop_continuation_mlir()) return 1;
  if (!test_post_test_repeat_structured_mlir()) return 1;
  if (!test_direct_products()) return 1;
  if (!test_strict_float_mlir()) return 1;
  if (!test_numeric_widen_mlir()) return 1;
  if (!test_float_bits_mlir()) return 1;
  if (!test_windows_target_runtime_surface()) return 1;
  if (!test_restaurant_and_nul()) return 1;
  if (!test_typed_interpolation_artifact()) return 1;
  if (!test_fixed_integer_family_artifact()) return 1;
  if (!test_direct_unit_call()) return 1;
  if (!test_checked_runtime_arithmetic()) return 1;
  if (!test_checked_integer_width_matrix()) return 1;
  if (!test_checked_helper_reachability()) return 1;
  if (!test_scalar_return_call_result()) return 1;
  if (!test_unsigned_scalar_return_call_result()) return 1;
  if (!test_unsigned_binary_u64_slice()) return 1;
  if (!test_u64_wrapping_add_artifact()) return 1;
  if (!test_u64_saturating_add_artifact()) return 1;
  if (!test_u64_overflowing_products_artifact()) return 1;
  if (!test_u64_saturating_subtract_artifact()) return 1;
  if (!test_u64_saturating_multiply_artifact()) return 1;
  if (!test_u64_saturating_policy_artifact()) return 1;
  if (!test_u64_wrapping_subtract_artifact()) return 1;
  if (!test_u64_wrapping_multiply_artifact()) return 1;
  if (!test_u64_wrapping_negate_artifact()) return 1;
  if (!test_u64_wrapping_power_artifact()) return 1;
  if (!test_u64_overflowing_power_artifact()) return 1;
  if (!test_u64_wrapping_shift_left_artifact()) return 1;
  if (!test_u64_masked_shift_left_artifact()) return 1;
  if (!test_u64_masked_shift_right_artifact()) return 1;
  if (!test_u64_logical_shift_right_artifact()) return 1;
  if (!test_fixed_integer_shift_policy_mlir_matrix()) return 1;
  if (!test_u64_rotated_left_artifact()) return 1;
  if (!test_u64_rotated_right_artifact()) return 1;
  if (!test_u64_count_ones_artifact()) return 1;
  if (!test_u64_count_zeros_artifact()) return 1;
  if (!test_u64_count_leading_zeros_artifact()) return 1;
  if (!test_u64_count_trailing_zeros_artifact()) return 1;
  if (!test_u64_reversed_bits_artifact()) return 1;
  if (!test_u64_reversed_bytes_artifact()) return 1;
  if (!test_fixed_integer_bit_primitive_width_artifact()) return 1;
  if (!test_unsigned_unary_bit_not_artifact()) return 1;
  if (!test_scalar_if_value_diamond()) return 1;
  if (!test_nested_scalar_if_value_diamond()) return 1;
  if (!test_if_diamond_cfg()) return 1;
  if (!test_logical_and_diamond()) return 1;
  if (!test_logical_unary_not()) return 1;
  if (!test_logical_or_diamond()) return 1;
  if (!test_logical_nested_diamond()) return 1;
  if (!test_logical_mlir_adversarial()) return 1;
  if (!test_checked_arithmetic_adversarial()) return 1;
  if (!test_signed_bitwise_artifact()) return 1;
  if (!test_integer_bitwise_width_artifact()) return 1;
  if (!test_signed_bit_not_artifact()) return 1;
  if (!test_checked_shift_artifact()) return 1;
  if (!test_checked_power_artifact()) return 1;
  if (!test_interpolation_semantic_barriers()) return 1;
  if (!test_checked_division_remainder_lowering()) return 1;
  if (!test_linear_sequence()) return 1;
  if (!test_capacity_and_all_or_nothing()) return 1;
  if (!test_aliases()) return 1;
  if (!test_invalid_hir_and_target()) return 1;
  if (!test_integer_exactly_mlir()) return 1;
  if (!test_float_to_integer_rounding_mlir()) return 1;
  if (!test_typed_propagation_mlir()) return 1;
  if (!test_typed_cleanup_mlir()) return 1;
  if (!test_valid_hir_outside_subset()) return 1;
  (void)puts("seed MLIR0: verified HIR0 native subset and LLVM dialect barriers passed");
  return 0;
}
