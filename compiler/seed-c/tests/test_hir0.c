#include "w_seed_hir0.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Test-only seam for resealing an in-memory HIR mutation. This keeps the
 * production digest helpers private while distinguishing lifecycle rejection
 * from the ordinary unchanged-digest mutation checks below. The included
 * translation unit supplies the public HIR symbols, so the archive object is
 * not extracted a second time at link. */
#include "../src/w_seed_hir0.c"

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "hir0 check failed: %s (%s:%d)\n", #condition, \
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
  TEST_ENUMS = 8,
  TEST_ENUM_CASES = 32,
  TEST_ENUM_CASE_PARAMETERS = 16,
  TEST_SWITCH_ARMS = 32,
  TEST_TYPES = 16,
  TEST_FUNCTIONS = 8,
  TEST_PARAMETERS = 8,
  TEST_ENTRIES = 4,
  TEST_STATEMENTS = 256,
  TEST_EXPRESSIONS = 256,
  TEST_ARGUMENTS = 32,
  TEST_INTERPOLATION_SEGMENTS = 16,
  TEST_SYMBOLS = 32,
  TEST_FACTS = 16,
  TEST_DIAGNOSTICS = 8,
  TEST_RECEIPT = 65536,
  TEST_HIR_IDENTITIES = 32,
  TEST_HIR_RECORDS = 512,
  TEST_HIR_TEXT = 4096,
  TEST_HIR_VALUES = 4096,
  TEST_HIR_RECEIPT = 320,
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
  w_seed_frontend_enum enums[TEST_ENUMS];
  w_seed_frontend_enum_case enum_cases[TEST_ENUM_CASES];
  w_seed_frontend_enum_case_parameter
      enum_case_parameters[TEST_ENUM_CASE_PARAMETERS];
  w_seed_frontend_type_declaration type_declarations[TEST_STRUCTS];
  w_seed_frontend_alias aliases[TEST_STRUCTS];
  w_seed_frontend_type types[TEST_TYPES];
  w_seed_frontend_function functions[TEST_FUNCTIONS];
  w_seed_frontend_parameter parameters[TEST_PARAMETERS];
  w_seed_frontend_entry entries[TEST_ENTRIES];
  w_seed_frontend_statement statements[TEST_STATEMENTS];
  w_seed_frontend_expression expressions[TEST_EXPRESSIONS];
  w_seed_frontend_argument arguments[TEST_ARGUMENTS];
  w_seed_frontend_switch_arm switch_arms[TEST_SWITCH_ARMS];
  w_seed_frontend_interpolation_segment
      interpolation_segments[TEST_INTERPOLATION_SEGMENTS];
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
  w_seed_frontend_result result;
  w_seed_hir0_module hir_modules[TEST_HIR_RECORDS];
  w_seed_hir0_identity hir_identities[TEST_HIR_IDENTITIES];
  w_seed_hir0_type hir_types[TEST_HIR_RECORDS];
  w_seed_hir0_enum hir_enums[TEST_HIR_RECORDS];
  w_seed_hir0_enum_case hir_enum_cases[TEST_HIR_RECORDS];
  w_seed_hir0_enum_case_parameter
      hir_enum_case_parameters[TEST_HIR_RECORDS];
  w_seed_hir0_function hir_functions[TEST_HIR_RECORDS];
  w_seed_hir0_parameter hir_parameters[TEST_HIR_RECORDS];
  w_seed_hir0_block hir_blocks[TEST_HIR_RECORDS];
  w_seed_hir0_block_argument hir_block_arguments[TEST_HIR_RECORDS];
  w_seed_hir0_edge_argument hir_edge_arguments[TEST_HIR_RECORDS];
  w_seed_hir0_switch_edge hir_switch_edges[TEST_HIR_RECORDS];
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
  uint8_t hir_text[TEST_HIR_TEXT];
  uint8_t hir_value_bytes[TEST_HIR_VALUES];
  uint8_t hir_receipt[TEST_HIR_RECEIPT];
  w_seed_hir0_output hir_output;
  w_seed_hir0_result hir_result;
  w_seed_hir0_counts hir_counts;
  w_seed_hir0_program hir_program;
} hir_fixture;

static hir_fixture fixture;

static w_seed_hir0_input hir_input(void);
static void fill_hir_output(uint8_t value);
static bool hir_output_is_byte(uint8_t value);

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

static const char CANONICAL_SOURCE[] =
    "fn main() { print(message: \"Hello, world!\", suffix: \"!\") }\n"
    "entry(main)\n";

static const char COMMENTED_SOURCE[] =
    "// harmless comment\n"
    "fn main() {   print(message: \"Hello, world!\", suffix: \"!\")   }\n"
    "\nentry(main)\n";

static bool fixture_parse(const char *text) {
  fixture.source_length = strlen(text);
  CHECK(fixture.source_length < sizeof(fixture.source_bytes));
  (void)memset(&fixture, 0, sizeof(fixture));
  fixture.source_length = strlen(text);
  (void)memcpy(fixture.source_bytes, text, fixture.source_length);
  w_seed_source_error source_error;
  CHECK(w_seed_source_init(
      (w_seed_byte_view){fixture.source_bytes, fixture.source_length},
      &fixture.source, &source_error));
  w_seed_lex_error lex_error;
  CHECK(w_seed_parser_init(
      &fixture.source, (w_seed_span){0u, fixture.source_length},
      (w_seed_foreign_limits){65536u, 256u}, fixture.lexer_frames,
      TEST_LEXER_FRAMES, fixture.tokens, TEST_TOKENS, fixture.nodes,
      TEST_NODES, fixture.parse_frames, TEST_PARSE_FRAMES, fixture.issues,
      TEST_ISSUES, &fixture.parser, &lex_error));
  CHECK(w_seed_parser_parse(&fixture.parser, &fixture.parse));
  fixture.document = (w_seed_frontend_document){
      .logical_source_id = (w_seed_frontend_text){"hir0-test", 9u},
      .module_id = (w_seed_frontend_text){"hir0-test", 9u},
      .local_module_name = (w_seed_frontend_text){"hir0-test", 9u},
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
      .enum_capacity = TEST_ENUMS,
      .enum_cases = fixture.enum_cases,
      .enum_case_capacity = TEST_ENUM_CASES,
      .enum_case_parameters = fixture.enum_case_parameters,
      .enum_case_parameter_capacity = TEST_ENUM_CASE_PARAMETERS,
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
      .switch_arms = fixture.switch_arms,
      .switch_arm_capacity = TEST_SWITCH_ARMS,
      .interpolation_segments = fixture.interpolation_segments,
      .interpolation_segment_capacity = TEST_INTERPOLATION_SEGMENTS,
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
      .receipt = fixture.frontend_receipt,
      .receipt_capacity = sizeof(fixture.frontend_receipt),
      .const_bytes = fixture.const_bytes,
      .const_bytes_capacity = sizeof(fixture.const_bytes)};
  return true;
}

static void configure_host(void) {
  fixture.host_requirements[0] = (w_seed_frontend_host_requirement){
      .name = (w_seed_frontend_text){"Console", 7u}};
  fixture.host_parameters[0] = (w_seed_frontend_external_parameter){
      .name = (w_seed_frontend_text){"message", 7u},
      .type = (w_seed_frontend_text){"String", 6u},
      .label_kind = W_SEED_FRONTEND_LABEL_REQUIRED};
  fixture.host_parameters[1] = (w_seed_frontend_external_parameter){
      .name = (w_seed_frontend_text){"suffix", 6u},
      .type = (w_seed_frontend_text){"String", 6u},
      .label_kind = W_SEED_FRONTEND_LABEL_REQUIRED};
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
      .parameter_count = 2u,
      .return_type = (w_seed_frontend_text){"()", 2u},
      .is_const = false,
      .requirements = fixture.host_requirements,
      .requirement_count = 1u};
  fixture.host_scope = (w_seed_frontend_host_prelude){
      .profile = (w_seed_frontend_text){"native-process@1", 16u},
      .symbols = fixture.host_symbols,
      .symbol_count = 2u};
  fixture.input.host_scope = &fixture.host_scope;
}

static bool fixture_frontend(const char *source) {
  CHECK(fixture_parse(source));
  configure_host();
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output,
                            &fixture.result) == W_SEED_FRONTEND_OK);
  return true;
}

static bool test_frontend_inferred_call_interpolation(void) {
  static const char SOURCE[] =
      "enum Course { starter }\n"
      "fn localCall(label: Course): i64 { return 11 }\n"
      "entry {\n"
      "  let x = localCall(label: .starter)\n"
      "  print(message: \"Courses ${x}\", suffix: \"\")\n"
      "}\n";
  CHECK(fixture_parse(SOURCE));
  configure_host();
  w_seed_frontend_counts measured;
  w_seed_frontend_result measure_result;
  CHECK(w_seed_frontend_measure(&fixture.input, &measured, &measure_result) ==
        W_SEED_FRONTEND_OK);
  CHECK(measured.facts == 0u && measure_result.required.facts == 0u &&
        measured.interpolation_segments == 2u);
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output,
                            &fixture.result) == W_SEED_FRONTEND_OK);
  CHECK(fixture.result.status == W_SEED_FRONTEND_OK &&
        fixture.result.written.facts == 0u &&
        fixture.result.written.interpolation_segments ==
            measured.interpolation_segments &&
        fixture.result.written.receipt_bytes == measured.receipt_bytes &&
        fixture.result.required.receipt_bytes == measured.receipt_bytes &&
        fixture.result.receipt_bytes == measured.receipt_bytes);
  CHECK(fixture.result.written.expressions == measured.expressions &&
        fixture.result.written.statements == measured.statements &&
        fixture.result.written.arguments == measured.arguments);

  static const char *const REJECTED[] = {
      "enum Course { starter }\n"
      "fn localCall(label: Course): i64 { return 11 }\n"
      "entry { let x = localCall\n"
      "print(message: \"Courses ${x}\", suffix: \"\") }\n",
      "enum Course { starter }\n"
      "fn localCall(label: Course): i64 { return 11 }\n"
      "entry { let x = localCall.member\n"
      "print(message: \"Courses ${x}\", suffix: \"\") }\n",
      "enum Course { starter }\n"
      "fn localCall(label: Course): i64 { return 11 }\n"
      "entry { let x = localCall(wrong: .starter)\n"
      "print(message: \"Courses ${x}\", suffix: \"\") }\n"};
  for (size_t index = 0u; index < sizeof(REJECTED) / sizeof(REJECTED[0]);
       index += 1u) {
    CHECK(fixture_parse(REJECTED[index]));
    configure_host();
    CHECK(w_seed_frontend_run(&fixture.input, &fixture.output,
                              &fixture.result) != W_SEED_FRONTEND_OK);
  }
  return true;
}

static void configure_process_input_host(void) {
  configure_host();
  fixture.host_parameters[0].label_kind =
      W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY;
  fixture.host_symbols[1].parameter_count = 1u;
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
  static const w_seed_frontend_text empty = {NULL, 0u};
  configure_process_external();
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
  fixture.external_modules[0].symbol_count = 6u;
  (void)empty;
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

static bool fixture_process_frontend(const char *source) {
  CHECK(fixture_parse(source));
  configure_host();
  configure_process_external();
  CHECK(resolve_process_import());
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output,
                            &fixture.result) == W_SEED_FRONTEND_OK);
  return true;
}

static bool fixture_process_input0_frontend(const char *source) {
  CHECK(fixture_parse(source));
  configure_process_input_host();
  configure_process_input_external();
  CHECK(resolve_process_import());
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output,
                            &fixture.result) == W_SEED_FRONTEND_OK);
  return true;
}

static void setup_hir_output(void) {
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
      .external_symbol_capacity = 8u};
}

static bool lower(const char *source) {
  CHECK(fixture_frontend(source));
  setup_hir_output();
  w_seed_hir0_input input = {&fixture.input, &fixture.output, &fixture.result};
  w_seed_hir0_counts measured;
  w_seed_hir0_result measure_result;
  CHECK(w_seed_hir0_measure(&input, &measured, &measure_result) ==
        W_SEED_HIR0_OK);
  CHECK(measure_result.required.modules == measured.modules);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_OK);
  CHECK(fixture.hir_result.required.modules == measured.modules);
  CHECK(fixture.hir_result.written.receipt_bytes == measured.receipt_bytes);
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output,
                                        &fixture.hir_result,
                                        &fixture.hir_program));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_counts = measured;
  return true;
}

static bool lower_process(const char *source) {
  CHECK(fixture_process_frontend(source));
  setup_hir_output();
  const w_seed_hir0_input input = {&fixture.input, &fixture.output,
                                   &fixture.result};
  w_seed_hir0_counts measured;
  w_seed_hir0_result measure_result;
  CHECK(w_seed_hir0_measure(&input, &measured, &measure_result) ==
        W_SEED_HIR0_OK);
  CHECK(measured.external_modules == 1u && measured.external_symbols == 4u &&
        measured.types == 7u);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output,
                                        &fixture.hir_result,
                                        &fixture.hir_program));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_counts = measured;
  return true;
}

static bool lower_process_input0(const char *source) {
  CHECK(fixture_process_input0_frontend(source));
  setup_hir_output();
  const w_seed_hir0_input input = {&fixture.input, &fixture.output,
                                   &fixture.result};
  w_seed_hir0_counts measured;
  w_seed_hir0_result measure_result;
  CHECK(w_seed_hir0_measure(&input, &measured, &measure_result) ==
        W_SEED_HIR0_OK);
  CHECK(measured.external_modules == 1u && measured.external_symbols == 6u &&
        measured.types == 7u && measured.functions == 1u &&
        measured.parameters == 2u && measured.blocks == 3u &&
        measured.instructions == 2u && measured.calls == 2u &&
        measured.arguments == 2u && measured.requirements == 1u &&
        measured.values == 7u && measured.terminators == 3u &&
        measured.entries == 1u && measured.value_bytes == 15u);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output,
                                        &fixture.hir_result,
                                        &fixture.hir_program));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_counts = measured;
  return true;
}

typedef enum {
  PROCESS_INPUT0_BAD_SOURCE_MEMBER,
  PROCESS_INPUT0_BAD_SOURCE_FAILURE_VALUE,
  PROCESS_INPUT0_BAD_SOURCE_FAILURE_LABEL,
  PROCESS_INPUT0_BAD_SOURCE_HANDLER_SIGNATURE,
  PROCESS_INPUT0_BAD_CATALOG_MEMBER,
  PROCESS_INPUT0_BAD_CATALOG_FAILURE_SIGNATURE,
} process_input0_bad_case;

static void reseal_hir_fixture(void);

static bool expect_process_input0_rejected(const char *source,
                                           process_input0_bad_case bad) {
  CHECK(fixture_parse(source));
  configure_process_input_host();
  configure_process_input_external();
  if (bad == PROCESS_INPUT0_BAD_CATALOG_MEMBER)
    fixture.external_symbols[4].name = (w_seed_frontend_text){"empty", 5u};
  if (bad == PROCESS_INPUT0_BAD_CATALOG_FAILURE_SIGNATURE) {
    fixture.external_parameters[0].type =
        (w_seed_frontend_text){"Bool", 4u};
  }
  CHECK(resolve_process_import());
  const w_seed_frontend_status frontend_status =
      w_seed_frontend_run(&fixture.input, &fixture.output, &fixture.result);
  if (frontend_status != W_SEED_FRONTEND_OK) return true;
  setup_hir_output();
  const w_seed_hir0_input input = {&fixture.input, &fixture.output,
                                   &fixture.result};
  w_seed_hir0_counts counts;
  w_seed_hir0_result result;
  CHECK(w_seed_hir0_measure(&input, &counts, &result) != W_SEED_HIR0_OK);
  return true;
}

static bool test_process_input0_hir(void) {
  static const char SOURCE[] =
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
      "\n"
      "entry(run)\n";
  static const char BAD_MEMBER[] =
      "import {\n"
      "  Arguments as ProcessArguments,\n"
      "  Context as ProcessContext,\n"
      "  ExitCode as ProcessExitCode,\n"
      "} from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode {\n"
      "  if args.empty {\n"
      "    print(\"missing\")\n"
      "    return .failure(2)\n"
      "  } else {\n"
      "    print(\"received\")\n"
      "    return .success\n"
      "  }\n"
      "}\n"
      "entry(run)\n";
  static const char BAD_FAILURE_VALUE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { if args.isEmpty { print(\"missing\") "
      "return .failure(3) } else { print(\"received\") return .success } }\n"
      "entry(run)\n";
  static const char BAD_FAILURE_LABEL[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { if args.isEmpty { print(\"missing\") "
      "return .failure(code: 2) } else { print(\"received\") "
      "return .success } }\n"
      "entry(run)\n";
  static const char BAD_HANDLER_SIGNATURE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessExitCode): "
      "ProcessExitCode { if args.isEmpty { print(\"missing\") "
      "return .failure(2) } else { print(\"received\") return .success } }\n"
      "entry(run)\n";
  CHECK(lower_process_input0(SOURCE));
  CHECK(fixture.result.written.modules == 1u &&
        fixture.result.written.types == 5u &&
        fixture.result.written.symbols == 5u &&
        fixture.result.written.functions == 1u &&
        fixture.result.written.parameters == 2u &&
        fixture.result.written.statements == 5u &&
        fixture.result.written.expressions == 12u &&
        fixture.result.written.arguments == 3u);
  CHECK(fixture.input.external_modules[0].symbol_count == 6u &&
        fixture.result.written.imports == 1u &&
        fixture.result.written.import_items == 3u);
  CHECK(fixture.output.expressions[1].kind == W_SEED_FRONTEND_EXPR_MEMBER &&
        fixture.output.expressions[1].resolved_external_module_index == 0u &&
        fixture.output.expressions[1].resolved_external_symbol_index == 4u &&
        text_equal(fixture.output.expressions[1].member_name,
                   (w_seed_frontend_text){"isEmpty", 7u}) &&
        fixture.output.expressions[5].kind == W_SEED_FRONTEND_EXPR_ENUM_CASE &&
        fixture.output.expressions[5].resolved_external_module_index == 0u &&
        fixture.output.expressions[5].resolved_external_symbol_index == 5u &&
        fixture.output.expressions[7].kind == W_SEED_FRONTEND_EXPR_CALL &&
        fixture.output.expressions[7].resolved_external_module_index == 0u &&
        fixture.output.expressions[7].resolved_external_symbol_index == 5u &&
        fixture.output.expressions[6].kind == W_SEED_FRONTEND_EXPR_INTEGER &&
        text_equal(fixture.output.expressions[6].spelling,
                   (w_seed_frontend_text){"2", 1u}));
  CHECK(fixture.hir_counts.external_modules == 1u &&
        fixture.hir_counts.external_symbols == 6u &&
        fixture.hir_counts.blocks == 3u && fixture.hir_counts.instructions == 2u &&
        fixture.hir_counts.calls == 2u && fixture.hir_counts.arguments == 2u &&
        fixture.hir_counts.requirements == 1u &&
        fixture.hir_counts.values == 7u && fixture.hir_counts.terminators == 3u);
  CHECK(fixture.hir_program.external_modules[0].symbol_count == 6u &&
        fixture.hir_program.external_symbol_count == 6u &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE &&
        fixture.hir_program.entries[0].adapter_kind ==
            W_SEED_HIR0_ENTRY_ADAPTER_NATIVE_PROCESS &&
        fixture.hir_program.entries[0].cleanup_obligation ==
            W_SEED_HIR0_ENTRY_CLEANUP_RELEASE_HANDLER_OWNERS &&
        fixture.hir_program.terminators[0].kind ==
            W_SEED_HIR0_TERMINATOR_BRANCH &&
        fixture.hir_program.terminators[0].value_index == 3u &&
        fixture.hir_program.terminators[1].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        fixture.hir_program.terminators[1].value_index == 5u &&
        fixture.hir_program.terminators[2].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        fixture.hir_program.terminators[2].value_index == 6u &&
        fixture.hir_program.values[2].kind ==
            W_SEED_HIR0_VALUE_PARAMETER_READ &&
        fixture.hir_program.values[3].kind ==
            W_SEED_HIR0_VALUE_EXTERNAL_MEMBER &&
        fixture.hir_program.values[3].left_value == 2u &&
        fixture.hir_program.values[3].external_symbol_index == 4u &&
        fixture.hir_program.values[4].kind == W_SEED_HIR0_VALUE_CONST_I64 &&
        fixture.hir_program.values[4].integer_value == 2 &&
        fixture.hir_program.values[5].kind ==
            W_SEED_HIR0_VALUE_EXTERNAL_ENUM_CASE &&
        fixture.hir_program.values[5].left_value == 4u &&
        fixture.hir_program.values[5].external_symbol_index == 5u &&
        fixture.hir_program.values[6].kind ==
            W_SEED_HIR0_VALUE_EXTERNAL_ENUM_CASE &&
        fixture.hir_program.values[6].left_value == W_SEED_HIR0_NONE &&
        fixture.hir_program.values[6].external_symbol_index == 3u &&
        hir_text_is(&fixture.hir_program, fixture.hir_program.values[3].member_name,
                    "isEmpty") &&
        hir_text_is(&fixture.hir_program, fixture.hir_program.values[5].member_name,
                    "failure") &&
        hir_text_is(&fixture.hir_program, fixture.hir_program.values[6].member_name,
                    "success"));
  CHECK(expect_process_input0_rejected(
      BAD_MEMBER, PROCESS_INPUT0_BAD_SOURCE_MEMBER));
  CHECK(expect_process_input0_rejected(
      BAD_FAILURE_VALUE, PROCESS_INPUT0_BAD_SOURCE_FAILURE_VALUE));
  CHECK(expect_process_input0_rejected(
      BAD_FAILURE_LABEL, PROCESS_INPUT0_BAD_SOURCE_FAILURE_LABEL));
  CHECK(expect_process_input0_rejected(
      BAD_HANDLER_SIGNATURE, PROCESS_INPUT0_BAD_SOURCE_HANDLER_SIGNATURE));
  CHECK(expect_process_input0_rejected(SOURCE, PROCESS_INPUT0_BAD_CATALOG_MEMBER));
  CHECK(expect_process_input0_rejected(
      SOURCE, PROCESS_INPUT0_BAD_CATALOG_FAILURE_SIGNATURE));

  /* The public MLIR adapter emits these verified HIR literals. Resealing a
   * different byte sequence must not preserve the public witness identity. */
  CHECK(lower_process_input0(SOURCE));
  const uint8_t saved_literal_byte = fixture.hir_value_bytes[0];
  fixture.hir_value_bytes[0] = 'x';
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_value_bytes[0] = saved_literal_byte;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static void reseal_hir_fixture(void) {
  uint8_t semantic_digest[32];
  uint8_t provenance_digest[32];
  digest_program(&fixture.hir_program, &fixture.hir_counts, semantic_digest);
  digest_provenance(&fixture.hir_program, &fixture.hir_counts,
                    provenance_digest);
  (void)memcpy(fixture.hir_result.semantic_digest, semantic_digest,
               sizeof(semantic_digest));
  (void)memcpy(fixture.hir_result.provenance_digest, provenance_digest,
               sizeof(provenance_digest));
  write_receipt_unchecked(fixture.hir_receipt, &fixture.hir_counts,
                          semantic_digest, provenance_digest);
}

static bool test_process_hir(void) {
  static const char canonical[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return .success }\n"
      "entry(run)\n";
  static const char variant[] =
      "import { Arguments as ProcessArgs, Context as ProcessCtx, "
      "ExitCode as ProcessStatus } from std.process\n"
      "async fn run(args: ProcessArgs, ctx: ProcessCtx): ProcessStatus { "
      "return .success }\n"
      "entry(run)\n";
  CHECK(lower_process(canonical));
  CHECK(fixture.hir_counts.external_modules == 1u &&
        fixture.hir_counts.external_symbols == 4u &&
        fixture.hir_counts.types == 7u && fixture.hir_counts.functions == 1u &&
        fixture.hir_counts.parameters == 2u && fixture.hir_counts.values == 1u);
  CHECK(fixture.hir_program.external_modules[0].module_index == 0u &&
        fixture.hir_program.external_modules[0].first_symbol == 0u &&
        fixture.hir_program.external_modules[0].symbol_count == 4u &&
        fixture.hir_program.external_symbol_count == 4u);
  CHECK(fixture.hir_program.types[4].kind == W_SEED_HIR0_TYPE_NOMINAL &&
        fixture.hir_program.types[4].external_module_index == 0u &&
        fixture.hir_program.types[4].external_symbol_index == 0u &&
        fixture.hir_program.types[5].external_symbol_index == 1u &&
        fixture.hir_program.types[6].external_symbol_index == 2u);
  CHECK(fixture.hir_program.types[0].lifecycle ==
            W_SEED_HIR0_LIFECYCLE_VALUE_COPY &&
        fixture.hir_program.types[0].release_contract ==
            W_SEED_HIR0_RELEASE_CONTRACT_NONE &&
        fixture.hir_program.types[1].lifecycle ==
            W_SEED_HIR0_LIFECYCLE_UNKNOWN &&
        fixture.hir_program.types[1].release_contract ==
            W_SEED_HIR0_RELEASE_CONTRACT_UNKNOWN &&
        fixture.hir_program.types[2].lifecycle ==
            W_SEED_HIR0_LIFECYCLE_VALUE_COPY &&
        fixture.hir_program.types[2].release_contract ==
            W_SEED_HIR0_RELEASE_CONTRACT_NONE &&
        fixture.hir_program.types[3].lifecycle ==
            W_SEED_HIR0_LIFECYCLE_VALUE_COPY &&
        fixture.hir_program.types[3].release_contract ==
            W_SEED_HIR0_RELEASE_CONTRACT_NONE &&
        fixture.hir_program.types[4].lifecycle ==
            W_SEED_HIR0_LIFECYCLE_ENTRY_ROOT_OWNER &&
        fixture.hir_program.types[4].release_contract ==
            W_SEED_HIR0_RELEASE_CONTRACT_PROCESS_V1_WRAPPER_RELEASE &&
        fixture.hir_program.types[5].lifecycle ==
            W_SEED_HIR0_LIFECYCLE_ENTRY_ROOT_OWNER &&
        fixture.hir_program.types[5].release_contract ==
            W_SEED_HIR0_RELEASE_CONTRACT_PROCESS_V1_WRAPPER_RELEASE &&
        fixture.hir_program.types[6].lifecycle ==
            W_SEED_HIR0_LIFECYCLE_VALUE_COPY &&
        fixture.hir_program.types[6].release_contract ==
            W_SEED_HIR0_RELEASE_CONTRACT_NONE);
  CHECK(fixture.hir_program.functions[0].is_async &&
        fixture.hir_program.functions[0].return_type == 6u &&
        fixture.hir_program.functions[0].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE &&
        fixture.hir_program.parameters[0].type_index == 4u &&
        fixture.hir_program.parameters[1].type_index == 5u &&
        fixture.hir_program.entries[0].adapter_kind ==
            W_SEED_HIR0_ENTRY_ADAPTER_NATIVE_PROCESS &&
        fixture.hir_program.entries[0].cleanup_obligation ==
            W_SEED_HIR0_ENTRY_CLEANUP_RELEASE_HANDLER_OWNERS &&
        fixture.hir_program.entries[0].first_cleanup_owner_parameter == 0u &&
        fixture.hir_program.entries[0].cleanup_owner_parameter_count == 2u);
  CHECK(fixture.hir_program.values[0].kind ==
            W_SEED_HIR0_VALUE_EXTERNAL_ENUM_CASE &&
        fixture.hir_program.values[0].type_index == 6u &&
        fixture.hir_program.values[0].external_module_index == 0u &&
        fixture.hir_program.values[0].external_symbol_index == 3u &&
        fixture.hir_program.values[0].member_name.count == 7u &&
        memcmp(fixture.hir_text + fixture.hir_program.values[0].member_name.offset,
               "success", 7u) == 0);

  uint8_t semantic[32];
  uint8_t provenance[32];
  (void)memcpy(semantic, fixture.hir_result.semantic_digest, sizeof(semantic));
  (void)memcpy(provenance, fixture.hir_result.provenance_digest,
               sizeof(provenance));
  /* The HIR owns canonical external names and does not retain resolver
   * pointers or source aliases. */
  fixture.external_symbols[0].name = (w_seed_frontend_text){NULL, 9u};
  fixture.external_modules[0].module_id = (w_seed_frontend_text){NULL, 11u};
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));

  CHECK(lower_process(variant));
  CHECK(memcmp(semantic, fixture.hir_result.semantic_digest,
               sizeof(semantic)) == 0);
  CHECK(memcmp(provenance, fixture.hir_result.provenance_digest,
               sizeof(provenance)) != 0);

  /* HIR-consumer mutations are rejected without reparsing aliases. */
  w_seed_hir0_type saved_type = fixture.hir_types[4];
  fixture.hir_types[4].external_symbol_index = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_types[4] = saved_type;
  w_seed_hir0_external_symbol saved_symbol = fixture.hir_external_symbols[0];
  fixture.hir_external_symbols[0] = fixture.hir_external_symbols[1];
  fixture.hir_external_symbols[1] = saved_symbol;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  saved_symbol = fixture.hir_external_symbols[0];
  fixture.hir_external_symbols[0] = fixture.hir_external_symbols[1];
  fixture.hir_external_symbols[1] = saved_symbol;
  w_seed_hir0_entry saved_entry = fixture.hir_entries[0];
  fixture.hir_entries[0].adapter_kind =
      W_SEED_HIR0_ENTRY_ADAPTER_DEFAULT_UNIT;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_entries[0] = saved_entry;
  w_seed_hir0_value saved_value = fixture.hir_values[0];
  fixture.hir_values[0].external_symbol_index = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_values[0] = saved_value;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool test_direct_entry_facts(void) {
  static const char PURE_SOURCE[] =
      "async fn quote(count: i64): i64 { return count + 1 }\n"
      "entry { }\n";
  CHECK(lower(PURE_SOURCE));
  CHECK(fixture.hir_program.function_count == 2u &&
        fixture.hir_program.functions[0].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE);

  static const char EMPTY_ASYNC_SOURCE[] =
      "async fn empty() { }\n"
      "entry { }\n";
  CHECK(lower(EMPTY_ASYNC_SOURCE));
  CHECK(fixture.hir_program.function_count == 2u &&
        fixture.hir_program.functions[0].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE);

  /* A zero-count frontend family may omit its caller-owned storage. */
  CHECK(fixture_parse(EMPTY_ASYNC_SOURCE));
  configure_host();
  fixture.output.statements = NULL;
  fixture.output.statement_capacity = 0u;
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output,
                            &fixture.result) == W_SEED_FRONTEND_OK);
  CHECK(fixture.result.written.statements == 0u);
  setup_hir_output();
  const w_seed_hir0_input empty_input = hir_input();
  w_seed_hir0_counts empty_counts;
  w_seed_hir0_result empty_measure;
  CHECK(w_seed_hir0_measure(&empty_input, &empty_counts, &empty_measure) ==
        W_SEED_HIR0_OK);
  CHECK(empty_counts.blocks == 2u && empty_counts.terminators == 2u);
  CHECK(w_seed_hir0_run(&empty_input, &fixture.hir_output,
                        &fixture.hir_result) == W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output,
                                        &fixture.hir_result,
                                        &fixture.hir_program));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  CHECK(fixture.hir_program.functions[0].direct_entry ==
        W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE);

  static const char HELPER_SOURCE[] =
      "fn baseTotal(count: i64): i64 { return count + 1 }\n"
      "fn orderTotal(count: i64): i64 { "
      "let subtotal = baseTotal(count: count) return subtotal }\n"
      "async fn quote(count: i64): i64 { "
      "let total = orderTotal(count: count) return total }\n"
      "entry { }\n";
  CHECK(lower(HELPER_SOURCE));
  CHECK(fixture.hir_program.function_count == 4u &&
        fixture.hir_program.functions[0].suspension ==
            W_SEED_HIR0_SUSPENSION_NEVER &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT &&
        fixture.hir_program.functions[1].suspension ==
            W_SEED_HIR0_SUSPENSION_NEVER &&
        fixture.hir_program.functions[1].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT &&
        fixture.hir_program.functions[2].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[2].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE);

  static const char REVERSED_HELPER_SOURCE[] =
      "async fn reverseQuote(count: i64): i64 { "
      "let total = reverseTotal(count: count) return total }\n"
      "fn reverseTotal(count: i64): i64 { "
      "let subtotal = reverseBase(count: count) return subtotal }\n"
      "fn reverseBase(count: i64): i64 { return count + 1 }\n"
      "entry { }\n";
  CHECK(lower(REVERSED_HELPER_SOURCE));
  CHECK(fixture.hir_program.function_count == 4u &&
        fixture.hir_program.functions[0].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE &&
        fixture.hir_program.functions[1].suspension ==
            W_SEED_HIR0_SUSPENSION_NEVER &&
        fixture.hir_program.functions[1].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT &&
        fixture.hir_program.functions[2].suspension ==
            W_SEED_HIR0_SUSPENSION_NEVER &&
        fixture.hir_program.functions[2].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT);

  static const char POISONED_REVERSED_SOURCE[] =
      "fn unrelated(value: Bool): Bool { return value }\n"
      "async fn reverseQuote(count: i64): i64 { "
      "let total = reverseTotal(count: count) return total }\n"
      "fn reverseTotal(count: i64): i64 { "
      "let subtotal = reverseBase(count: count) return subtotal }\n"
      "fn reverseBase(count: i64): i64 { "
      "print(message: \"poison\", suffix: \"\") return count }\n"
      "fn cycleA(value: Bool): Bool { "
      "let next = cycleB(value: value) return next }\n"
      "fn cycleB(value: Bool): Bool { "
      "print(message: \"cycle\", suffix: \"\") "
      "let next = cycleA(value: value) return next }\n"
      "entry { }\n";
  CHECK(lower(POISONED_REVERSED_SOURCE));
  CHECK(fixture.hir_program.function_count == 7u &&
        fixture.hir_program.functions[0].suspension ==
            W_SEED_HIR0_SUSPENSION_NEVER &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT &&
        fixture.hir_program.functions[1].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[1].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT &&
        fixture.hir_program.functions[2].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[2].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT &&
        fixture.hir_program.functions[3].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[4].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[5].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY);

  static const char HOST_SOURCE[] =
      "async fn announce() { "
      "print(message: \"order\", suffix: \"\") }\n"
      "entry { }\n";
  CHECK(lower(HOST_SOURCE));
  CHECK(fixture.hir_program.function_count == 2u &&
        fixture.hir_program.functions[0].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT);

  static const char STRING_SOURCE[] =
      "async fn retain(value: String) { let echoed = value }\n"
      "entry { }\n";
  CHECK(lower(STRING_SOURCE));
  CHECK(fixture.hir_program.function_count == 2u &&
        fixture.hir_program.functions[0].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT);

  static const char ASYNC_BRANCH_SOURCE[] =
      "async fn maybe(): Bool { return true }\n"
      "async fn choose() { "
      "if false { let selected = maybe() } "
      "else { let fallback = true } }\n"
      "entry { }\n";
  CHECK(lower(ASYNC_BRANCH_SOURCE));
  CHECK(fixture.hir_program.function_count == 3u &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE &&
        fixture.hir_program.functions[1].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[1].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT);

  static const char RECURSIVE_SOURCE[] =
      "fn left(value: Bool): Bool { let next = right(value: value) return next }\n"
      "fn right(value: Bool): Bool { let next = left(value: value) return next }\n"
      "async fn choose(value: Bool): Bool { "
      "let next = left(value: value) return next }\n"
      "entry { }\n";
  CHECK(lower(RECURSIVE_SOURCE));
  CHECK(fixture.hir_program.function_count == 4u &&
        fixture.hir_program.functions[0].suspension ==
            W_SEED_HIR0_SUSPENSION_NEVER &&
        fixture.hir_program.functions[1].suspension ==
            W_SEED_HIR0_SUSPENSION_NEVER &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT &&
        fixture.hir_program.functions[1].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT &&
        fixture.hir_program.functions[2].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE);

  const w_seed_hir0_function saved = fixture.hir_functions[2];
  fixture.hir_functions[2].suspension = W_SEED_HIR0_SUSPENSION_NEVER;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_functions[2] = saved;
  fixture.hir_functions[2].direct_entry =
      W_SEED_HIR0_DIRECT_ENTRY_ABSENT;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_functions[2] = saved;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool check_direct_entry_effect_barrier(
    const char *source, w_seed_frontend_status expected_status) {
  CHECK(fixture_parse(source));
  configure_host();
  const w_seed_frontend_status status =
      w_seed_frontend_run(&fixture.input, &fixture.output, &fixture.result);
  CHECK(status == expected_status);
  for (size_t fact = 0u; fact < fixture.result.written.facts; fact += 1u)
    CHECK(fixture.facts[fact].kind !=
              W_SEED_FRONTEND_FACT_UNRESOLVED_IMPORTED_SYMBOL &&
          fixture.facts[fact].kind !=
              W_SEED_FRONTEND_FACT_UNRESOLVED_LOCAL_SYMBOL);

  setup_hir_output();
  fill_hir_output(0xa5u);
  const w_seed_hir0_input input = hir_input();
  (void)memset(&fixture.hir_counts, 0x6au, sizeof(fixture.hir_counts));
  const w_seed_hir0_counts counts_before = fixture.hir_counts;
  (void)memset(&fixture.hir_result, 0x5au, sizeof(fixture.hir_result));
  const w_seed_hir0_result result_before = fixture.hir_result;
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) ==
        W_SEED_HIR0_FRONTEND);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_FRONTEND);
  CHECK(hir_output_is_byte(0xa5u) &&
        memcmp(&fixture.hir_counts, &counts_before, sizeof(counts_before)) ==
            0 &&
        memcmp(&fixture.hir_result, &result_before, sizeof(result_before)) ==
            0);
  return true;
}

static bool test_direct_entry_effect_barrier(void) {
  static const char UNSUPPORTED_EFFECT_SOURCE[] =
      "async fn value(): i64 { return 1 }\n"
      "async fn wait(): i64 { return await value() }\n"
      "entry { }\n";
  CHECK(check_direct_entry_effect_barrier(UNSUPPORTED_EFFECT_SOURCE,
                                          W_SEED_FRONTEND_UNSUPPORTED));

  static const char DEFER_EFFECT_SOURCE[] =
      "async fn cleanup() { }\n"
      "async fn deferWork() { defer async { await cleanup() } }\n"
      "entry { }\n";
  CHECK(check_direct_entry_effect_barrier(DEFER_EFFECT_SOURCE,
                                          W_SEED_FRONTEND_BARRIER));
  return true;
}

static bool test_canonical_and_copy_boundary(void) {
  CHECK(lower(CANONICAL_SOURCE));
  CHECK(fixture.hir_counts.modules == 1u);
  CHECK(fixture.hir_counts.identities == 5u);
  CHECK(fixture.hir_counts.types == 4u);
  CHECK(fixture.hir_counts.functions == 1u);
  CHECK(fixture.hir_counts.parameters == 0u);
  CHECK(fixture.hir_counts.blocks == 1u);
  CHECK(fixture.hir_counts.instructions == 1u);
  CHECK(fixture.hir_counts.bindings == 0u);
  CHECK(fixture.hir_counts.calls == 1u);
  CHECK(fixture.hir_counts.host_parameters == 2u);
  CHECK(fixture.hir_counts.arguments == 2u);
  CHECK(fixture.hir_counts.requirements == 1u);
  CHECK(fixture.hir_counts.values == 2u);
  CHECK(fixture.hir_counts.terminators == 1u);
  CHECK(fixture.hir_counts.entries == 1u);
  CHECK(fixture.hir_program.identities[3].profile.count == 16u);
  CHECK(memcmp(fixture.hir_text + fixture.hir_program.identities[3].profile.offset,
               "native-process@1", 16u) == 0);
  CHECK(fixture.hir_program.identities[4].first_parameter == 0u &&
        fixture.hir_program.identities[4].parameter_count == 2u &&
        fixture.hir_program.host_parameters[0].owner_identity == 4u &&
        fixture.hir_program.host_parameters[1].owner_identity == 4u &&
        fixture.hir_program.host_parameters[0].ordinal == 0u &&
        fixture.hir_program.host_parameters[1].ordinal == 1u &&
        fixture.hir_program.host_parameters[0].type_index == 1u &&
        fixture.hir_program.host_parameters[1].type_index == 1u);
  CHECK(fixture.hir_program.host_parameters[0].label.count == 7u &&
        fixture.hir_program.host_parameters[1].label.count == 6u &&
        memcmp(fixture.hir_text + fixture.hir_program.host_parameters[0].label.offset,
               "message", 7u) == 0 &&
        memcmp(fixture.hir_text + fixture.hir_program.host_parameters[1].label.offset,
               "suffix", 6u) == 0);
  CHECK(fixture.hir_program.entries[0].slot.count == 8u);
  CHECK(memcmp(fixture.hir_text + fixture.hir_program.entries[0].slot.offset,
               ".default", 8u) == 0);
  CHECK(fixture.hir_program.host_parameters[0].ordinal == 0u &&
        fixture.hir_program.host_parameters[1].ordinal == 1u &&
        fixture.hir_program.host_parameters[0].label_kind ==
            W_SEED_HIR0_LABEL_REQUIRED &&
        fixture.hir_program.host_parameters[1].label_kind ==
            W_SEED_HIR0_LABEL_REQUIRED);
  CHECK(fixture.hir_program.arguments[0].label_kind ==
            W_SEED_HIR0_LABEL_REQUIRED &&
        fixture.hir_program.arguments[1].label_kind ==
            W_SEED_HIR0_LABEL_REQUIRED &&
        fixture.hir_program.arguments[0].label.count == 7u &&
        fixture.hir_program.arguments[1].label.count == 6u);
  CHECK(fixture.hir_program.arguments[0].owner_call == 0u &&
        fixture.hir_program.arguments[1].owner_call == 0u &&
        fixture.hir_program.arguments[0].ordinal == 0u &&
        fixture.hir_program.arguments[1].ordinal == 1u &&
        fixture.hir_program.arguments[0].value_index == 0u &&
        fixture.hir_program.arguments[1].value_index == 1u);
  CHECK(fixture.hir_program.calls[0].callee_identity == 4u);
  CHECK(fixture.hir_program.values[0].byte_count == 13u);
  CHECK(memcmp(fixture.hir_value_bytes + fixture.hir_program.values[0].byte_offset,
               "Hello, world!", 13u) == 0);
  CHECK(fixture.hir_program.values[1].byte_count == 1u &&
        memcmp(fixture.hir_value_bytes + fixture.hir_program.values[1].byte_offset,
               "!", 1u) == 0);
  (void)memset(&fixture.document, 0xa5, sizeof(fixture.document));
  (void)memset(&fixture.input, 0xa5, sizeof(fixture.input));
  (void)memset(&fixture.output, 0xa5, sizeof(fixture.output));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool test_semantic_and_provenance_digests(void) {
  uint8_t semantic[32];
  uint8_t provenance[32];
  CHECK(lower(CANONICAL_SOURCE));
  (void)memcpy(semantic, fixture.hir_result.semantic_digest, sizeof(semantic));
  (void)memcpy(provenance, fixture.hir_result.provenance_digest,
               sizeof(provenance));
  CHECK(lower(COMMENTED_SOURCE));
  CHECK(memcmp(semantic, fixture.hir_result.semantic_digest, sizeof(semantic)) ==
        0);
  CHECK(memcmp(provenance, fixture.hir_result.provenance_digest,
               sizeof(provenance)) != 0);
  return true;
}

static bool test_function_parameter_records(void) {
  static const char SOURCE[] =
      "fn main(value: String) { print(message: \"Hello, world!\", suffix: \"!\") }\n"
      "entry(main)\n";
  CHECK(lower(SOURCE));
  CHECK(fixture.hir_counts.parameters == 1u);
  const w_seed_hir0_parameter *parameter = &fixture.hir_program.parameters[0];
  CHECK(parameter->owner_function == 0u && parameter->ordinal == 0u &&
        parameter->type_index == W_SEED_HIR0_TYPE_STRING &&
        parameter->label_kind == W_SEED_HIR0_LABEL_REQUIRED &&
        parameter->name.count == 5u && parameter->label.count == 5u);
  CHECK(memcmp(fixture.hir_text + parameter->name.offset, "value", 5u) == 0);
  CHECK(memcmp(fixture.hir_text + parameter->label.offset, "value", 5u) == 0);
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool test_lowering_is_not_hello_hardcoded(void) {
  static const char OTHER_SOURCE[] =
      "fn main() { print(message: \"north\", suffix: \"!\") }\nentry(main)\n";
  CHECK(lower(OTHER_SOURCE));
  CHECK(fixture.hir_program.values[0].byte_count == 5u);
  CHECK(memcmp(fixture.hir_value_bytes + fixture.hir_program.values[0].byte_offset,
               "north", 5u) == 0);
  return true;
}

static bool test_local_binding_lowering(void) {
  static const char SOURCE[] =
      "fn main() { let message = \"Table 42 remains open\" "
      "print(message: message, suffix: \"!\") }\nentry(main)\n";
  CHECK(lower(SOURCE));
  CHECK(fixture.hir_counts.bindings == 1u);
  CHECK(fixture.hir_counts.instructions == 2u);
  CHECK(fixture.hir_counts.calls == 1u);
  CHECK(fixture.hir_counts.arguments == 2u && fixture.hir_counts.values == 3u);
  CHECK(fixture.hir_program.instructions[0].kind ==
            W_SEED_HIR0_INSTRUCTION_BINDING &&
        fixture.hir_program.instructions[0].call_index == W_SEED_HIR0_NONE &&
        fixture.hir_program.instructions[0].binding_index == 0u &&
        fixture.hir_program.instructions[1].kind ==
            W_SEED_HIR0_INSTRUCTION_CALL &&
        fixture.hir_program.instructions[1].call_index == 0u &&
        fixture.hir_program.instructions[1].binding_index == W_SEED_HIR0_NONE);
  const w_seed_hir0_binding *binding = &fixture.hir_program.bindings[0];
  CHECK(binding->owner_instruction == 0u && binding->owner_block == 0u &&
        binding->ordinal == 0u && binding->type_index == W_SEED_HIR0_TYPE_STRING &&
        !binding->is_mutable && binding->name.count == 7u &&
        memcmp(fixture.hir_text + binding->name.offset, "message", 7u) == 0 &&
        binding->initializer_value == 0u);
  const w_seed_hir0_value *initializer = &fixture.hir_program.values[0];
  CHECK(initializer->kind == W_SEED_HIR0_VALUE_CONST_STRING &&
        initializer->owner_kind == W_SEED_HIR0_VALUE_OWNER_BINDING &&
        initializer->owner_index == 0u && initializer->owner_ordinal == 0u &&
        initializer->byte_offset == 0u &&
        initializer->byte_count == strlen("Table 42 remains open"));
  CHECK(memcmp(fixture.hir_value_bytes + initializer->byte_offset,
               "Table 42 remains open", initializer->byte_count) == 0);
  CHECK(fixture.hir_program.calls[0].owner_instruction == 1u &&
        fixture.hir_program.values[1].kind == W_SEED_HIR0_VALUE_BINDING_READ &&
        fixture.hir_program.values[1].binding_index == 0u &&
        fixture.hir_program.values[1].byte_offset == 0u &&
        fixture.hir_program.values[1].byte_count == 0u &&
        fixture.hir_program.values[2].kind == W_SEED_HIR0_VALUE_CONST_STRING &&
        fixture.hir_program.values[2].binding_index == W_SEED_HIR0_NONE &&
        fixture.hir_program.values[2].byte_offset == initializer->byte_count &&
        fixture.hir_program.values[2].byte_count == 1u);
  (void)memset(&fixture.document, 0, sizeof(fixture.document));
  (void)memset(&fixture.input, 0, sizeof(fixture.input));
  (void)memset(&fixture.output, 0, sizeof(fixture.output));
  (void)memset(&fixture.result, 0, sizeof(fixture.result));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool test_straight_line_mutation_ssa(void) {
  static const char SOURCE[] =
      "entry {\n"
      "  var seats = 5\n"
      "  seats = seats + 1\n"
      "  print(message: \"Open ${seats}\", suffix: \"!\")\n"
      "}\n";
  CHECK(lower(SOURCE));
  CHECK(fixture.hir_program.binding_count == 2u &&
        fixture.hir_program.instruction_count == 3u &&
        fixture.hir_program.call_count == 1u);
  const w_seed_hir0_binding declaration = fixture.hir_bindings[0];
  const w_seed_hir0_binding update = fixture.hir_bindings[1];
  CHECK(declaration.is_mutable && declaration.source_binding == 0u &&
        declaration.previous_version == W_SEED_HIR0_NONE &&
        declaration.next_version == 1u &&
        declaration.type_index == W_SEED_HIR0_TYPE_I64 &&
        declaration.initializer_value < fixture.hir_program.value_count &&
        update.is_mutable && update.source_binding == 0u &&
        update.previous_version == 0u &&
        update.next_version == W_SEED_HIR0_NONE &&
        update.type_index == declaration.type_index &&
        update.initializer_value < fixture.hir_program.value_count &&
        update.owner_instruction == 1u &&
        declaration.name.count == update.name.count &&
        memcmp(fixture.hir_text + declaration.name.offset,
               fixture.hir_text + update.name.offset,
               declaration.name.count) == 0);
  CHECK(fixture.hir_values[declaration.initializer_value].kind ==
            W_SEED_HIR0_VALUE_CONST_I64 &&
        fixture.hir_values[declaration.initializer_value].integer_value == 5);
  const w_seed_hir0_value *replacement =
      &fixture.hir_values[update.initializer_value];
  CHECK(replacement->kind == W_SEED_HIR0_VALUE_BINARY_I64 &&
        fixture.hir_values[replacement->left_value].kind ==
            W_SEED_HIR0_VALUE_BINDING_READ &&
        fixture.hir_values[replacement->left_value].binding_index == 0u &&
        fixture.hir_values[replacement->right_value].kind ==
            W_SEED_HIR0_VALUE_CONST_I64 &&
        fixture.hir_values[replacement->right_value].integer_value == 1);
  bool saw_latest_read = false;
  for (size_t index = 0u; index < fixture.hir_program.value_count; index += 1u)
    if (fixture.hir_values[index].kind == W_SEED_HIR0_VALUE_BINDING_READ &&
        fixture.hir_values[index].binding_index == 1u)
      saw_latest_read = true;
  CHECK(saw_latest_read);

  fixture.hir_bindings[1].source_binding = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_bindings[1] = update;
  reseal_hir_fixture();
  fixture.hir_bindings[0].next_version = W_SEED_HIR0_NONE;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_bindings[0] = declaration;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool test_interleaved_mutation_versions(void) {
  static const char SOURCE[] =
      "entry {\n"
      "  var seats = 5\n"
      "  var tables = 2\n"
      "  seats = seats + 1\n"
      "  tables = tables + 3\n"
      "  seats = seats + tables\n"
      "  print(message: \"Capacity ${seats}\", suffix: \"!\")\n"
      "}\n";
  CHECK(lower(SOURCE));
  CHECK(fixture.hir_program.binding_count == 5u &&
        fixture.hir_program.instruction_count == 6u &&
        fixture.hir_program.call_count == 1u);

  const w_seed_hir0_binding *seats0 = &fixture.hir_bindings[0];
  const w_seed_hir0_binding *tables0 = &fixture.hir_bindings[1];
  const w_seed_hir0_binding *seats1 = &fixture.hir_bindings[2];
  const w_seed_hir0_binding *tables1 = &fixture.hir_bindings[3];
  const w_seed_hir0_binding *seats2 = &fixture.hir_bindings[4];
  CHECK(seats0->source_binding == 0u &&
        seats0->previous_version == W_SEED_HIR0_NONE &&
        seats0->next_version == 2u && tables0->source_binding == 1u &&
        tables0->previous_version == W_SEED_HIR0_NONE &&
        tables0->next_version == 3u && seats1->source_binding == 0u &&
        seats1->previous_version == 0u && seats1->next_version == 4u &&
        tables1->source_binding == 1u && tables1->previous_version == 1u &&
        tables1->next_version == W_SEED_HIR0_NONE &&
        seats2->source_binding == 0u && seats2->previous_version == 2u &&
        seats2->next_version == W_SEED_HIR0_NONE);

  const w_seed_hir0_value *seats_update1 =
      &fixture.hir_values[seats1->initializer_value];
  const w_seed_hir0_value *tables_update1 =
      &fixture.hir_values[tables1->initializer_value];
  const w_seed_hir0_value *seats_update2 =
      &fixture.hir_values[seats2->initializer_value];
  CHECK(seats_update1->kind == W_SEED_HIR0_VALUE_BINARY_I64 &&
        fixture.hir_values[seats_update1->left_value].binding_index == 0u &&
        tables_update1->kind == W_SEED_HIR0_VALUE_BINARY_I64 &&
        fixture.hir_values[tables_update1->left_value].binding_index == 1u &&
        seats_update2->kind == W_SEED_HIR0_VALUE_BINARY_I64 &&
        fixture.hir_values[seats_update2->left_value].binding_index == 2u &&
        fixture.hir_values[seats_update2->right_value].binding_index == 3u);

  bool saw_final_seats = false;
  for (size_t index = 0u; index < fixture.hir_program.value_count; index += 1u)
    if (fixture.hir_values[index].kind == W_SEED_HIR0_VALUE_BINDING_READ &&
        fixture.hir_values[index].binding_index == 4u)
      saw_final_seats = true;
  CHECK(saw_final_seats &&
        w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool test_conditional_mutation_merge(void) {
  static const char SOURCE[] =
      "fn nextSeats(isOpen: Bool): i64 {\n"
      "  var seats = 5\n"
      "  let selected = if isOpen { seats + 1 } else { seats - 1 }\n"
      "  seats = selected\n"
      "  return seats\n"
      "}\n"
      "entry(nextSeats)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 1u && program->binding_count == 3u &&
        program->block_count == 4u && program->block_argument_count == 1u);
  const w_seed_hir0_binding *declaration = &program->bindings[0];
  const w_seed_hir0_binding *selected = &program->bindings[1];
  const w_seed_hir0_binding *merge = &program->bindings[2];
  CHECK(declaration->source_binding == 0u && declaration->next_version == 2u &&
        selected->source_binding == 1u &&
        selected->previous_version == W_SEED_HIR0_NONE &&
        selected->next_version == W_SEED_HIR0_NONE &&
        merge->source_binding == 0u && merge->previous_version == 0u &&
        merge->next_version == W_SEED_HIR0_NONE &&
        merge->owner_block == 3u &&
        merge->initializer_value < program->value_count);
  const w_seed_hir0_value *selected_value =
      &program->values[selected->initializer_value];
  const w_seed_hir0_value *merged = &program->values[merge->initializer_value];
  CHECK(selected_value->kind == W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        selected_value->block_argument_index == 0u &&
        merged->kind == W_SEED_HIR0_VALUE_BINDING_READ &&
        merged->binding_index == 1u &&
        program->blocks[3].block_argument_count == 1u &&
        program->block_arguments[0].owner_block == 3u &&
        program->block_arguments[0].type_index == W_SEED_HIR0_TYPE_I64 &&
        edge_value_at(program, 1u) < program->value_count &&
        edge_value_at(program, 2u) < program->value_count);
  const w_seed_hir0_value *then_value =
      &program->values[edge_value_at(program, 1u)];
  const w_seed_hir0_value *else_value =
      &program->values[edge_value_at(program, 2u)];
  CHECK(then_value->kind == W_SEED_HIR0_VALUE_BINARY_I64 &&
        else_value->kind == W_SEED_HIR0_VALUE_BINARY_I64 &&
        program->values[then_value->left_value].binding_index == 0u &&
        program->values[else_value->left_value].binding_index == 0u);
  bool saw_merged_read = false;
  for (size_t index = 0u; index < program->value_count; index += 1u)
    if (program->values[index].kind == W_SEED_HIR0_VALUE_BINDING_READ &&
        program->values[index].binding_index == 2u)
      saw_merged_read = true;
  CHECK(saw_merged_read && w_seed_hir0_verify(program, &fixture.hir_result));
  const uint32_t then_read_index = then_value->left_value;
  const w_seed_hir0_value saved_then_read =
      fixture.hir_values[then_read_index];
  fixture.hir_values[then_read_index].binding_index = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[then_read_index] = saved_then_read;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_bool_mutation_ssa(void) {
  static const char SOURCE[] =
      "fn availability(requested: Bool): Bool {\n"
      "  var open = false\n"
      "  open = requested\n"
      "  return open\n"
      "}\n"
      "entry(availability)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 1u && program->binding_count == 2u &&
        program->bindings[0].type_index == W_SEED_HIR0_TYPE_BOOL &&
        program->bindings[1].type_index == W_SEED_HIR0_TYPE_BOOL &&
        program->bindings[0].next_version == 1u &&
        program->bindings[1].source_binding == 0u &&
        program->bindings[1].previous_version == 0u);
  const w_seed_hir0_value *replacement =
      &program->values[program->bindings[1].initializer_value];
  CHECK(replacement->kind == W_SEED_HIR0_VALUE_PARAMETER_READ &&
        replacement->type_index == W_SEED_HIR0_TYPE_BOOL);
  const w_seed_hir0_terminator *terminator = &program->terminators[0];
  CHECK(terminator->kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        terminator->value_index < program->value_count &&
        program->values[terminator->value_index].kind ==
            W_SEED_HIR0_VALUE_BINDING_READ &&
        program->values[terminator->value_index].binding_index == 1u &&
        w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_branch_local_mutation_merge(void) {
  static const char SOURCE[] =
      "fn nextSeats(isOpen: Bool): i64 {\n"
      "  var seats = 5\n"
      "  if isOpen {\n"
      "    seats = seats + 1\n"
      "  } else {\n"
      "    seats = seats - 1\n"
      "  }\n"
      "  return seats\n"
      "}\n"
      "entry(nextSeats)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 1u && program->block_count == 4u &&
        program->block_argument_count == 1u &&
        program->binding_count == 2u);
  const w_seed_hir0_binding *declaration = &program->bindings[0];
  const w_seed_hir0_binding *merge = &program->bindings[1];
  CHECK(declaration->owner_block == 0u && declaration->next_version == 1u &&
        merge->owner_block == 3u && merge->source_binding == 0u &&
        merge->previous_version == 0u &&
        merge->next_version == W_SEED_HIR0_NONE &&
        program->blocks[3].block_argument_count == 1u &&
        program->block_arguments[0].owner_block == 3u &&
        program->block_arguments[0].type_index == W_SEED_HIR0_TYPE_I64);
  const w_seed_hir0_value *initializer =
      &program->values[merge->initializer_value];
  CHECK(initializer->kind == W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        initializer->block_argument_index == 0u &&
        initializer->owner_kind == W_SEED_HIR0_VALUE_OWNER_BINDING &&
        initializer->owner_index == 1u);
  CHECK(program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[0].result_type == 0u &&
        edge_value_at(program, 1u) != W_SEED_HIR0_NONE &&
        edge_value_at(program, 2u) != W_SEED_HIR0_NONE &&
        program->terminators[1].target_block == 3u &&
        program->terminators[2].target_block == 3u);
  const w_seed_hir0_terminator *return_term = &program->terminators[3];
  CHECK(return_term->kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        return_term->value_index < program->value_count &&
        program->values[return_term->value_index].kind ==
            W_SEED_HIR0_VALUE_BINDING_READ &&
        program->values[return_term->value_index].binding_index == 1u &&
        w_seed_hir0_verify(program, &fixture.hir_result));
  const w_seed_hir0_terminator saved_branch = fixture.hir_terminators[0];
  fixture.hir_terminators[0].result_type = W_SEED_HIR0_TYPE_I64;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[0] = saved_branch;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  const w_seed_hir0_terminator saved_then = fixture.hir_terminators[1];
  const uint32_t saved_then_edge_value = FIXTURE_EDGE_VALUE_SLOT(1u);
  FIXTURE_EDGE_VALUE_SLOT(1u) = W_SEED_HIR0_NONE;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[1] = saved_then;
  FIXTURE_EDGE_VALUE_SLOT(1u) = saved_then_edge_value;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_multi_branch_mutation_merge(void) {
  static const char SOURCE[] =
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
      "entry(nextState)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 1u && program->block_count == 4u &&
        program->block_argument_count == 2u && program->binding_count == 4u &&
        program->instruction_count == 4u);
  const w_seed_hir0_binding *seats = &program->bindings[0];
  const w_seed_hir0_binding *tables = &program->bindings[1];
  const w_seed_hir0_binding *seats_merge = &program->bindings[2];
  const w_seed_hir0_binding *tables_merge = &program->bindings[3];
  CHECK(seats->source_binding == 0u && seats->next_version == 2u &&
        tables->source_binding == 1u && tables->next_version == 3u &&
        seats_merge->source_binding == 0u && seats_merge->previous_version == 0u &&
        seats_merge->next_version == W_SEED_HIR0_NONE &&
        tables_merge->source_binding == 1u &&
        tables_merge->previous_version == 1u &&
        tables_merge->next_version == W_SEED_HIR0_NONE);
  CHECK(seats_merge->owner_block == 3u && tables_merge->owner_block == 3u &&
        seats_merge->type_index == W_SEED_HIR0_TYPE_I64 &&
        tables_merge->type_index == W_SEED_HIR0_TYPE_I64 &&
        seats_merge->name.count == 5u && tables_merge->name.count == 6u &&
        memcmp(fixture.hir_text + seats_merge->name.offset, "seats", 5u) == 0 &&
        memcmp(fixture.hir_text + tables_merge->name.offset, "tables", 6u) == 0);
  CHECK(program->blocks[3].first_block_argument == 0u &&
        program->blocks[3].block_argument_count == 2u &&
        program->block_arguments[0].owner_block == 3u &&
        program->block_arguments[0].ordinal == 0u &&
        program->block_arguments[0].type_index == W_SEED_HIR0_TYPE_I64 &&
        program->block_arguments[1].owner_block == 3u &&
        program->block_arguments[1].ordinal == 1u &&
        program->block_arguments[1].type_index == W_SEED_HIR0_TYPE_I64);
  CHECK(program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[0].result_type == 0u &&
        program->terminators[1].target_block == 3u &&
        program->terminators[2].target_block == 3u &&
        program->terminators[1].edge_argument_count == 2u &&
        program->terminators[2].edge_argument_count == 2u &&
        program->terminators[1].first_edge_argument == 0u &&
        program->terminators[2].first_edge_argument == 2u);
  for (size_t predecessor = 1u; predecessor <= 2u; predecessor += 1u) {
    const w_seed_hir0_terminator *terminator =
        &program->terminators[predecessor];
    for (size_t ordinal = 0u; ordinal < 2u; ordinal += 1u) {
      const w_seed_hir0_edge_argument *edge =
          &program->edge_arguments[(size_t)terminator->first_edge_argument +
                                   ordinal];
      CHECK(edge->owner_terminator == predecessor &&
            edge->owner_block == predecessor && edge->ordinal == ordinal &&
            edge->type_index == W_SEED_HIR0_TYPE_I64 &&
            edge->value_index < program->value_count);
    }
  }
  CHECK(program->values[program->edge_arguments[0].value_index].type_index ==
            W_SEED_HIR0_TYPE_I64 &&
        program->values[program->edge_arguments[2].value_index].type_index ==
            W_SEED_HIR0_TYPE_I64 &&
        w_seed_hir0_verify(program, &fixture.hir_result));
  const w_seed_hir0_edge_argument saved_edge = fixture.hir_edge_arguments[1];
  fixture.hir_edge_arguments[1].ordinal = 0u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_edge_arguments[1] = saved_edge;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_edge_arguments[1].type_index = W_SEED_HIR0_TYPE_BOOL;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_edge_arguments[1] = saved_edge;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  const w_seed_hir0_terminator saved_then = fixture.hir_terminators[1];
  fixture.hir_terminators[1].edge_argument_count = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[1] = saved_then;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_while_mutation_ssa(void) {
  static const char SOURCE[] =
      "fn countTo(limit: i64): i64 {\n"
      "  var count = 0\n"
      "  while count < limit { count = count + 1 }\n"
      "  return count\n"
      "}\n"
      "entry(countTo)\n";
  CHECK(lower(SOURCE));
  w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 1u && program->block_count == 4u &&
        program->block_argument_count == 1u &&
        program->edge_argument_count == 2u &&
        program->binding_count == 2u && program->instruction_count == 2u &&
        program->value_count == 10u && program->terminator_count == 4u);
  CHECK(program->blocks[0].instruction_count == 1u &&
        program->blocks[1].instruction_count == 0u &&
        program->blocks[1].first_block_argument == 0u &&
        program->blocks[1].block_argument_count == 1u &&
        program->blocks[2].instruction_count == 1u &&
        program->blocks[3].instruction_count == 0u &&
        program->block_arguments[0].owner_block == 1u &&
        program->block_arguments[0].ordinal == 0u &&
        program->block_arguments[0].type_index == W_SEED_HIR0_TYPE_I64);
  CHECK(program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[0].target_block == 1u &&
        program->terminators[0].edge_argument_count == 1u &&
        program->terminators[1].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[1].target_block == 2u &&
        program->terminators[1].else_block == 3u &&
        program->terminators[2].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[2].target_block == 1u &&
        program->terminators[2].edge_argument_count == 1u &&
        program->terminators[3].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_VALUE);
  const w_seed_hir0_binding declaration = fixture.hir_bindings[0];
  const w_seed_hir0_binding update = fixture.hir_bindings[1];
  CHECK(declaration.owner_block == 0u && declaration.is_mutable &&
        declaration.source_binding == 0u && declaration.next_version == 1u &&
        update.owner_block == 2u && update.is_mutable &&
        update.source_binding == 0u && update.previous_version == 0u &&
        update.next_version == W_SEED_HIR0_NONE &&
        program->values[update.initializer_value].kind ==
            W_SEED_HIR0_VALUE_BINARY_I64 &&
        program->values[program->values[update.initializer_value].left_value]
                .kind == W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        program->values[program->values[update.initializer_value].left_value]
                .block_argument_index == 0u);
  CHECK(program->values[program->terminators[1].value_index].type_index ==
            W_SEED_HIR0_TYPE_BOOL &&
        program->values[program->terminators[3].value_index].kind ==
            W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        program->values[program->terminators[3].value_index]
                .block_argument_index == 0u &&
        program->values[edge_value_at(program, 0u)].binding_index == 0u &&
        program->values[edge_value_at(program, 2u)].binding_index == 1u &&
        w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_block_argument saved_argument =
      fixture.hir_block_arguments[0];
  fixture.hir_block_arguments[0].type_index = W_SEED_HIR0_TYPE_BOOL;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_block_arguments[0] = saved_argument;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_terminator saved_preheader = fixture.hir_terminators[0];
  fixture.hir_terminators[0].edge_argument_count = 0u;
  fixture.hir_terminators[0].first_edge_argument = W_SEED_HIR0_NONE;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[0] = saved_preheader;
  reseal_hir_fixture();

  const w_seed_hir0_edge_argument saved_back_edge =
      fixture.hir_edge_arguments[1];
  fixture.hir_edge_arguments[1].value_index =
      fixture.hir_edge_arguments[0].value_index;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_edge_arguments[1] = saved_back_edge;
  reseal_hir_fixture();

  fixture.hir_block_arguments[0].owner_block = 2u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_block_arguments[0] = saved_argument;
  reseal_hir_fixture();

  const w_seed_hir0_terminator saved_body = fixture.hir_terminators[2];
  fixture.hir_terminators[2].target_block = 3u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[2] = saved_body;
  reseal_hir_fixture();

  const w_seed_hir0_terminator saved_exit = fixture.hir_terminators[3];
  fixture.hir_terminators[3] = saved_body;
  fixture.hir_terminators[3].owner_block = 3u;
  fixture.hir_terminators[3].target_block = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[3] = saved_exit;
  reseal_hir_fixture();

  fixture.hir_bindings[1].previous_version = W_SEED_HIR0_NONE;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_bindings[1] = update;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool expect_branch_mutation_unsupported(const char *source) {
  CHECK(fixture_frontend(source));
  setup_hir_output();
  const w_seed_hir0_input input = hir_input();
  w_seed_hir0_counts counts;
  w_seed_hir0_result result;
  CHECK(w_seed_hir0_measure(&input, &counts, &result) ==
        W_SEED_HIR0_UNSUPPORTED);
  return true;
}

static bool test_while_mutation_barriers(void) {
  CHECK(expect_branch_mutation_unsupported(
      "fn countTo(limit: i64): i64 {\n"
      "  var count = 0\n"
      "  while limit > 0 { count = count + 1 }\n"
      "  return count\n"
      "}\nentry(countTo)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn countTo(limit: i64): i64 {\n"
      "  var count = 0\n"
      "  while count < limit { count = limit }\n"
      "  return count\n"
      "}\nentry(countTo)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn countTo(limit: i64): i64 {\n"
      "  var count = 0\n"
      "  while count < limit { count = count + 1 count = count + 1 }\n"
      "  return count\n"
      "}\nentry(countTo)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn adjust(value: i64): i64 { return value + 1 }\n"
      "fn countTo(limit: i64): i64 {\n"
      "  var count = 0\n"
      "  while count < limit { count = adjust(value: count) }\n"
      "  return count\n"
      "}\nentry(countTo)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn countTo(limit: i64): i64 {\n"
      "  var count = 0\n"
      "  while count < limit { while count < limit { count = count + 1 } }\n"
      "  return count\n"
      "}\nentry(countTo)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn flip(limit: i64): Bool {\n"
      "  var open = false\n"
      "  while limit > 0 { open = !open }\n"
      "  return open\n"
      "}\nentry(flip)\n"));
  return true;
}

static bool test_branch_local_mutation_barriers(void) {
  CHECK(expect_branch_mutation_unsupported(
      "fn choose(flag: Bool): i64 {\n"
      "  var value = 1\n"
      "  if flag { value = 2 }\n"
      "  return value\n"
      "}\nentry(choose)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn choose(flag: Bool): i64 {\n"
      "  var left = 1\n"
      "  var right = 2\n"
      "  if flag { left = 3 } else { right = 4 }\n"
      "  return left\n"
      "}\nentry(choose)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn choose(flag: Bool): i64 {\n"
      "  var value = 1\n"
      "  if flag { value = 2 let extra = 3 } else { value = 3 }\n"
      "  return value\n"
      "}\nentry(choose)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn choose(flag: Bool): i64 {\n"
      "  var value = 1\n"
      "  if flag { value = 2 value = 3 } else { value = 4 }\n"
      "  return value\n"
      "}\nentry(choose)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn choose(flag: Bool): i64 {\n"
      "  var left = 1\n"
      "  var right = 2\n"
      "  if flag { left = 3 right = left + 1 } else { left = 4 right = 5 }\n"
      "  return left + right\n"
      "}\nentry(choose)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn choose(flag: Bool): i64 {\n"
      "  var left = 1\n"
      "  var right = 2\n"
      "  if flag { left = right + 1 right = 3 } else { left = 4 right = 5 }\n"
      "  return left + right\n"
      "}\nentry(choose)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn adjust(): i64 { return 4 }\n"
      "fn choose(flag: Bool): i64 {\n"
      "  var value = 1\n"
      "  if flag { value = adjust() } else { value = 2 }\n"
      "  return value\n"
      "}\nentry(choose)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn choose(flag: Bool): i64 {\n"
      "  var value = 1\n"
      "  if flag { if true { value = 2 } else { value = 3 } }\n"
      "  else { value = 4 }\n"
      "  return value\n"
      "}\nentry(choose)\n"));
  return true;
}

static bool test_bindings_across_functions(void) {
  static const char SOURCE[] =
      "fn first() { let first = true }\n"
      "fn second() { let second = false }\n"
      "entry(second)\n";
  CHECK(lower(SOURCE));
  CHECK(fixture.hir_program.function_count == 2u &&
        fixture.hir_program.binding_count == 2u &&
        fixture.hir_program.instruction_count == 2u &&
        fixture.hir_program.value_count == 2u);
  CHECK(fixture.hir_program.bindings[0].owner_block == 0u &&
        fixture.hir_program.bindings[0].initializer_value == 0u &&
        fixture.hir_program.bindings[1].owner_block == 1u &&
        fixture.hir_program.bindings[1].initializer_value == 1u);
  CHECK(fixture.hir_program.values[0].kind == W_SEED_HIR0_VALUE_CONST_BOOL &&
        fixture.hir_program.values[0].bool_value &&
        fixture.hir_program.values[1].kind == W_SEED_HIR0_VALUE_CONST_BOOL &&
        !fixture.hir_program.values[1].bool_value);
  return true;
}

static bool test_local_binding_verify_mutations(void) {
  static const char SOURCE[] =
      "fn main() { let message = \"Table 42 remains open\" "
      "print(message: message, suffix: \"!\") }\nentry(main)\n";
  CHECK(lower(SOURCE));
  w_seed_hir0_program *program = &fixture.hir_program;
  w_seed_hir0_binding saved_binding = fixture.hir_bindings[0];
  w_seed_hir0_instruction saved_instruction0 = fixture.hir_instructions[0];
  w_seed_hir0_instruction saved_instruction1 = fixture.hir_instructions[1];
  w_seed_hir0_value saved_value1 = fixture.hir_values[1];
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_bindings[0].owner_instruction = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_bindings[0] = saved_binding;
  fixture.hir_bindings[0].type_index = W_SEED_HIR0_TYPE_UNIT;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_bindings[0] = saved_binding;
  fixture.hir_bindings[0].is_mutable = true;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_bindings[0] = saved_binding;
  fixture.hir_instructions[1].binding_index = 0u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_instructions[1] = saved_instruction1;
  fixture.hir_values[1].binding_index = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[1] = saved_value1;
  fixture.hir_bindings[0].initializer_value = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_bindings[0] = saved_binding;
  fixture.hir_program.bindings =
      (const w_seed_hir0_binding *)(const void *)fixture.hir_instructions;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_program.bindings = fixture.hir_bindings;
  fixture.hir_instructions[0] = saved_instruction0;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_local_enum_hir(void) {
  static const char SOURCE[] =
      "enum Stage { cold ready done }\n"
      "enum OtherStage { cold ready }\n"
      "fn choose(): Stage { return .ready }\n"
      "entry { }\n";
  CHECK(lower(SOURCE));
  CHECK(fixture.result.written.enums == 2u &&
        fixture.result.written.enum_cases == 5u &&
        fixture.result.written.enum_case_parameters == 0u &&
        fixture.result.written.switch_arms == 0u &&
        fixture.result.written.types == 4u &&
        fixture.result.written.functions == 2u &&
        fixture.result.written.entries == 1u &&
        fixture.result.written.expressions == 1u &&
        fixture.result.written.statements == 1u &&
        fixture.result.written.symbols == 11u);

  const w_seed_frontend_enum *stage = &fixture.output.enums[0];
  const w_seed_frontend_enum *other = &fixture.output.enums[1];
  CHECK(stage->module_index == 0u && stage->type_index == 0u &&
        stage->first_case == 0u && stage->case_count == 3u &&
        text_is(stage->name, "Stage"));
  CHECK(other->module_index == 0u && other->type_index == 1u &&
        other->first_case == 3u && other->case_count == 2u &&
        text_is(other->name, "OtherStage"));
  CHECK(fixture.output.types[0].kind == W_SEED_FRONTEND_TYPE_ENUM &&
        fixture.output.types[0].enum_base_index == 0u &&
        text_is(fixture.output.types[0].spelling, "Stage") &&
        fixture.output.types[1].kind == W_SEED_FRONTEND_TYPE_ENUM &&
        fixture.output.types[1].enum_base_index == 1u &&
        text_is(fixture.output.types[1].spelling, "OtherStage") &&
        fixture.output.types[2].kind == W_SEED_FRONTEND_TYPE_ENUM &&
        fixture.output.types[2].enum_base_index == 0u &&
        text_is(fixture.output.types[2].spelling, "Stage"));

  static const char *const STAGE_CASES[] = {"cold", "ready", "done"};
  static const char *const OTHER_CASES[] = {"cold", "ready"};
  for (size_t ordinal = 0u; ordinal < 3u; ordinal += 1u) {
    const w_seed_frontend_enum_case *value =
        &fixture.output.enum_cases[ordinal];
    CHECK(value->module_index == 0u && value->owner_enum == 0u &&
          value->first_payload == 0u &&
          value->payload_count == 0u &&
          text_is(value->name, STAGE_CASES[ordinal]));
  }
  for (size_t ordinal = 0u; ordinal < 2u; ordinal += 1u) {
    const w_seed_frontend_enum_case *value =
        &fixture.output.enum_cases[3u + ordinal];
    CHECK(value->module_index == 0u && value->owner_enum == 1u &&
          value->first_payload == 0u &&
          value->payload_count == 0u &&
          text_is(value->name, OTHER_CASES[ordinal]));
  }
  const w_seed_frontend_expression *frontend_value =
      &fixture.output.expressions[0];
  CHECK(frontend_value->kind == W_SEED_FRONTEND_EXPR_ENUM_CASE);
  CHECK(frontend_value->supported);
  CHECK(frontend_value->enum_index == 0u);
  CHECK(frontend_value->enum_case_index == 1u);
  CHECK(frontend_value->inferred_type < fixture.result.written.types);
  CHECK(fixture.output.types[frontend_value->inferred_type].kind ==
        W_SEED_FRONTEND_TYPE_ENUM);
  CHECK(fixture.output.types[frontend_value->inferred_type].enum_base_index ==
        0u);
  CHECK(fixture.output.functions[0].return_type < fixture.result.written.types);
  CHECK(fixture.output.types[fixture.output.functions[0].return_type].kind ==
        W_SEED_FRONTEND_TYPE_ENUM);
  CHECK(fixture.output.types[fixture.output.functions[0].return_type]
            .enum_base_index == 0u);

  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(fixture.hir_counts.enums == 2u &&
        fixture.hir_counts.enum_cases == 5u && fixture.hir_counts.types == 6u &&
        fixture.hir_counts.functions == 2u && fixture.hir_counts.entries == 1u &&
        fixture.hir_counts.values == 1u && fixture.hir_counts.blocks == 2u &&
        fixture.hir_counts.terminators == 2u && program->enum_count == 2u &&
        program->enum_case_count == 5u);
  CHECK(program->enums[0].module_index == 0u &&
        program->enums[0].type_index == 4u &&
        program->enums[0].first_case == 0u &&
        program->enums[0].case_count == 3u &&
        hir_text_is(program, program->enums[0].name, "Stage") &&
        program->enums[1].module_index == 0u &&
        program->enums[1].type_index == 5u &&
        program->enums[1].first_case == 3u &&
        program->enums[1].case_count == 2u &&
        hir_text_is(program, program->enums[1].name, "OtherStage"));
  CHECK(program->types[4].kind == W_SEED_HIR0_TYPE_ENUM &&
        program->types[4].owner_module == 0u &&
        program->types[4].enum_index == 0u &&
        program->types[4].lifecycle == W_SEED_HIR0_LIFECYCLE_VALUE_COPY &&
        program->types[4].release_contract ==
            W_SEED_HIR0_RELEASE_CONTRACT_NONE &&
        hir_text_equal(program, program->types[4].name, program->enums[0].name) &&
        program->types[5].kind == W_SEED_HIR0_TYPE_ENUM &&
        program->types[5].owner_module == 0u &&
        program->types[5].enum_index == 1u &&
        program->types[5].lifecycle == W_SEED_HIR0_LIFECYCLE_VALUE_COPY &&
        program->types[5].release_contract ==
            W_SEED_HIR0_RELEASE_CONTRACT_NONE &&
        hir_text_equal(program, program->types[5].name, program->enums[1].name));
  for (size_t ordinal = 0u; ordinal < 3u; ordinal += 1u) {
    const w_seed_hir0_enum_case *value = &program->enum_cases[ordinal];
    CHECK(value->owner_enum == 0u && value->ordinal == ordinal &&
          value->tag == ordinal && value->payload_count == 0u &&
          hir_text_is(program, value->name, STAGE_CASES[ordinal]));
  }
  for (size_t ordinal = 0u; ordinal < 2u; ordinal += 1u) {
    const w_seed_hir0_enum_case *value = &program->enum_cases[3u + ordinal];
    CHECK(value->owner_enum == 1u && value->ordinal == ordinal &&
          value->tag == ordinal && value->payload_count == 0u &&
          hir_text_is(program, value->name, OTHER_CASES[ordinal]));
  }
  CHECK(program->functions[0].return_type == 4u &&
        program->identities[1].return_type == 4u &&
        program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        program->terminators[0].value_index == 0u &&
        program->terminators[0].result_type == 4u);
  const w_seed_hir0_value saved_value = program->values[0];
  CHECK(saved_value.kind == W_SEED_HIR0_VALUE_ENUM_CASE &&
        saved_value.owner_kind == W_SEED_HIR0_VALUE_OWNER_TERMINATOR &&
        saved_value.owner_index == 0u && saved_value.owner_ordinal == 0u &&
        saved_value.type_index == 4u && saved_value.enum_index == 0u &&
        saved_value.enum_case_index == 1u &&
        saved_value.external_module_index == W_SEED_HIR0_NONE &&
        saved_value.external_symbol_index == W_SEED_HIR0_NONE &&
        saved_value.member_name.count == 0u);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_enum saved_enum0 = fixture.hir_enums[0];
  fixture.hir_enums[0].type_index = 5u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enums[0] = saved_enum0;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_enums[0].first_case = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enums[0] = saved_enum0;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_enum_case saved_case1 = fixture.hir_enum_cases[1];
  fixture.hir_enum_cases[1].owner_enum = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_cases[1] = saved_case1;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_enum_cases[1].ordinal = 2u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_cases[1] = saved_case1;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_enum_cases[1].tag = 9u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_cases[1] = saved_case1;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_enum_cases[1].payload_count = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_cases[1] = saved_case1;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_enum_cases[1].name = fixture.hir_enum_cases[0].name;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_cases[1] = saved_case1;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[0].enum_index = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[0] = saved_value;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[0].enum_case_index = 3u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[0] = saved_value;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[0].type_index = 5u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[0] = saved_value;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[0].kind = W_SEED_HIR0_VALUE_CONST_BOOL;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[0] = saved_value;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_input input = hir_input();
  const uint8_t sentinel = 0xa5u;
  w_seed_hir0_result rejected;
  (void)memset(&rejected, 0x42, sizeof(rejected));
  const w_seed_hir0_result rejected_before = rejected;

  setup_hir_output();
  fill_hir_output(sentinel);
  fixture.hir_output.enum_capacity = fixture.hir_counts.enums - 1u;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(sentinel) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  setup_hir_output();
  fill_hir_output(sentinel);
  fixture.hir_output.enum_capacity = 0u;
  fixture.hir_output.enums = NULL;
  rejected = rejected_before;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(sentinel) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  setup_hir_output();
  fill_hir_output(sentinel);
  fixture.hir_output.enum_case_capacity = fixture.hir_counts.enum_cases - 1u;
  rejected = rejected_before;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(sentinel) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  setup_hir_output();
  fill_hir_output(sentinel);
  fixture.hir_output.enum_case_capacity = 0u;
  fixture.hir_output.enum_cases = NULL;
  rejected = rejected_before;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(sentinel) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  setup_hir_output();
  fill_hir_output(sentinel);
  w_seed_hir0_output alias = fixture.hir_output;
  alias.enum_cases = (w_seed_hir0_enum_case *)(void *)alias.enums;
  rejected = rejected_before;
  CHECK(w_seed_hir0_run(&input, &alias, &rejected) == W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(sentinel) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  return true;
}

static bool test_local_enum_payload_declarations_hir(void) {
  static const char SOURCE[] =
      "enum Course { starter main(price: i64) shared(i64, i64) }\n"
      "entry { }\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(fixture.result.written.enums == 1u &&
        fixture.result.written.enum_cases == 3u &&
        fixture.result.written.enum_case_parameters == 3u &&
        fixture.hir_counts.enums == 1u && fixture.hir_counts.enum_cases == 3u &&
        fixture.hir_counts.enum_case_parameters == 3u &&
        program->enum_count == 1u && program->enum_case_count == 3u &&
        program->enum_case_parameter_count == 3u);

  CHECK(program->enum_cases[0].first_payload == 0u &&
        program->enum_cases[0].payload_count == 0u &&
        program->enum_cases[1].first_payload == 0u &&
        program->enum_cases[1].payload_count == 1u &&
        program->enum_cases[2].first_payload == 1u &&
        program->enum_cases[2].payload_count == 2u);
  CHECK(program->enum_case_parameters[0].owner_case == 1u &&
        program->enum_case_parameters[0].ordinal == 0u &&
        program->enum_case_parameters[0].type_index == 2u &&
        program->enum_case_parameters[0].has_label &&
        hir_text_is(program, program->enum_case_parameters[0].label, "price"));
  for (size_t ordinal = 0u; ordinal < 2u; ordinal += 1u) {
    const w_seed_hir0_enum_case_parameter *payload =
        &program->enum_case_parameters[1u + ordinal];
    CHECK(payload->owner_case == 2u && payload->ordinal == ordinal &&
          payload->type_index == 2u && !payload->has_label &&
          payload->label.count == 0u);
  }
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_enum_case saved_case = fixture.hir_enum_cases[1];
  fixture.hir_enum_cases[1].first_payload = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_cases[1] = saved_case;

  const w_seed_hir0_enum_case_parameter saved_payload =
      fixture.hir_enum_case_parameters[0];
  fixture.hir_enum_case_parameters[0].owner_case = 2u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_case_parameters[0] = saved_payload;
  fixture.hir_enum_case_parameters[0].ordinal = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_case_parameters[0] = saved_payload;
  fixture.hir_enum_case_parameters[0].type_index = 3u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_case_parameters[0] = saved_payload;
  fixture.hir_enum_case_parameters[0].has_label = false;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_case_parameters[0] = saved_payload;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_input input = hir_input();
  const uint8_t sentinel = 0x6bu;
  w_seed_hir0_result rejected;
  (void)memset(&rejected, 0x42, sizeof(rejected));
  const w_seed_hir0_result rejected_before = rejected;
  setup_hir_output();
  fill_hir_output(sentinel);
  fixture.hir_output.enum_case_parameter_capacity =
      fixture.hir_counts.enum_case_parameters - 1u;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(sentinel) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  setup_hir_output();
  fill_hir_output(sentinel);
  w_seed_hir0_output alias = fixture.hir_output;
  alias.enum_case_parameters =
      (w_seed_hir0_enum_case_parameter *)(void *)alias.enum_cases;
  rejected = rejected_before;
  CHECK(w_seed_hir0_run(&input, &alias, &rejected) == W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(sentinel) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  CHECK(fixture_frontend(
      "enum Message { text(value: String) }\nentry { }\n"));
  setup_hir_output();
  const w_seed_hir0_input unsupported = hir_input();
  w_seed_hir0_counts unsupported_counts;
  w_seed_hir0_result unsupported_result;
  CHECK(w_seed_hir0_measure(&unsupported, &unsupported_counts,
                            &unsupported_result) == W_SEED_HIR0_UNSUPPORTED);
  return true;
}

static w_seed_hir0_input hir_input(void) {
  return (w_seed_hir0_input){&fixture.input, &fixture.output, &fixture.result};
}

static void fill_hir_output(uint8_t value) {
  (void)memset(fixture.hir_modules, value, sizeof(fixture.hir_modules));
  (void)memset(fixture.hir_identities, value, sizeof(fixture.hir_identities));
  (void)memset(fixture.hir_types, value, sizeof(fixture.hir_types));
  (void)memset(fixture.hir_enums, value, sizeof(fixture.hir_enums));
  (void)memset(fixture.hir_enum_cases, value, sizeof(fixture.hir_enum_cases));
  (void)memset(fixture.hir_enum_case_parameters, value,
               sizeof(fixture.hir_enum_case_parameters));
  (void)memset(fixture.hir_functions, value, sizeof(fixture.hir_functions));
  (void)memset(fixture.hir_parameters, value, sizeof(fixture.hir_parameters));
  (void)memset(fixture.hir_blocks, value, sizeof(fixture.hir_blocks));
  (void)memset(fixture.hir_block_arguments, value,
               sizeof(fixture.hir_block_arguments));
  (void)memset(fixture.hir_edge_arguments, value,
               sizeof(fixture.hir_edge_arguments));
  (void)memset(fixture.hir_switch_edges, value,
               sizeof(fixture.hir_switch_edges));
  (void)memset(fixture.hir_instructions, value,
               sizeof(fixture.hir_instructions));
  (void)memset(fixture.hir_bindings, value, sizeof(fixture.hir_bindings));
  (void)memset(fixture.hir_calls, value, sizeof(fixture.hir_calls));
  (void)memset(fixture.hir_host_parameters, value,
               sizeof(fixture.hir_host_parameters));
  (void)memset(fixture.hir_arguments, value, sizeof(fixture.hir_arguments));
  (void)memset(fixture.hir_requirements, value,
               sizeof(fixture.hir_requirements));
  (void)memset(fixture.hir_values, value, sizeof(fixture.hir_values));
  (void)memset(fixture.hir_interpolation_segments, value,
               sizeof(fixture.hir_interpolation_segments));
  (void)memset(fixture.hir_terminators, value,
               sizeof(fixture.hir_terminators));
  (void)memset(fixture.hir_entries, value, sizeof(fixture.hir_entries));
  (void)memset(fixture.hir_external_modules, value,
               sizeof(fixture.hir_external_modules));
  (void)memset(fixture.hir_external_symbols, value,
               sizeof(fixture.hir_external_symbols));
  (void)memset(fixture.hir_text, value, sizeof(fixture.hir_text));
  (void)memset(fixture.hir_value_bytes, value,
               sizeof(fixture.hir_value_bytes));
  (void)memset(fixture.hir_receipt, value, sizeof(fixture.hir_receipt));
}

static bool hir_output_is_byte(uint8_t value) {
#define CHECK_BYTES(field)                                                     \
  CHECK(memchr(fixture.field, (int)value, sizeof(fixture.field)) != NULL ||    \
        sizeof(fixture.field) == 0u)
  /* A capacity rejection must preserve a uniform sentinel in every region. */
#undef CHECK_BYTES
  const uint8_t *regions[] = {
      (const uint8_t *)fixture.hir_modules,
      (const uint8_t *)fixture.hir_identities,
      (const uint8_t *)fixture.hir_types,
      (const uint8_t *)fixture.hir_enums,
      (const uint8_t *)fixture.hir_enum_cases,
      (const uint8_t *)fixture.hir_enum_case_parameters,
      (const uint8_t *)fixture.hir_functions,
      (const uint8_t *)fixture.hir_parameters,
      (const uint8_t *)fixture.hir_blocks,
      (const uint8_t *)fixture.hir_block_arguments,
      (const uint8_t *)fixture.hir_edge_arguments,
      (const uint8_t *)fixture.hir_switch_edges,
      (const uint8_t *)fixture.hir_instructions,
      (const uint8_t *)fixture.hir_bindings,
      (const uint8_t *)fixture.hir_calls,
      (const uint8_t *)fixture.hir_host_parameters,
      (const uint8_t *)fixture.hir_arguments,
      (const uint8_t *)fixture.hir_requirements,
      (const uint8_t *)fixture.hir_values,
      (const uint8_t *)fixture.hir_interpolation_segments,
      (const uint8_t *)fixture.hir_terminators,
      (const uint8_t *)fixture.hir_entries,
      (const uint8_t *)fixture.hir_external_modules,
      (const uint8_t *)fixture.hir_external_symbols,
      fixture.hir_text,
      fixture.hir_value_bytes,
      fixture.hir_receipt};
  const size_t sizes[] = {
      sizeof(fixture.hir_modules), sizeof(fixture.hir_identities),
      sizeof(fixture.hir_types), sizeof(fixture.hir_enums),
      sizeof(fixture.hir_enum_cases),
      sizeof(fixture.hir_enum_case_parameters),
      sizeof(fixture.hir_functions),
      sizeof(fixture.hir_parameters), sizeof(fixture.hir_blocks),
      sizeof(fixture.hir_block_arguments),
      sizeof(fixture.hir_edge_arguments),
      sizeof(fixture.hir_switch_edges),
      sizeof(fixture.hir_instructions), sizeof(fixture.hir_bindings),
      sizeof(fixture.hir_calls),
      sizeof(fixture.hir_host_parameters), sizeof(fixture.hir_arguments),
      sizeof(fixture.hir_requirements), sizeof(fixture.hir_values),
      sizeof(fixture.hir_interpolation_segments),
      sizeof(fixture.hir_terminators), sizeof(fixture.hir_entries),
      sizeof(fixture.hir_external_modules),
      sizeof(fixture.hir_external_symbols),
      sizeof(fixture.hir_text), sizeof(fixture.hir_value_bytes),
      sizeof(fixture.hir_receipt)};
  for (size_t region = 0u; region < sizeof(sizes) / sizeof(sizes[0]);
       region += 1u) {
    for (size_t byte = 0u; byte < sizes[region]; byte += 1u)
      if (regions[region][byte] != value) return false;
  }
  return true;
}

typedef enum {
  PROCESS_BAD_EXTERNAL_MODULE_COUNT,
  PROCESS_BAD_EXTERNAL_SYMBOL_COUNT,
  PROCESS_BAD_EXTERNAL_MODULE_ID,
  PROCESS_BAD_EXTERNAL_SYMBOL_NAME,
  PROCESS_BAD_EXTERNAL_SYMBOL_KIND,
  PROCESS_BAD_EXTERNAL_SYMBOL_EXPORT,
  PROCESS_BAD_EXTERNAL_SYMBOL_CONST,
  PROCESS_BAD_EXTERNAL_SYMBOL_RECEIVER,
  PROCESS_BAD_EXTERNAL_SYMBOL_RETURN,
  PROCESS_BAD_EXTERNAL_IMPORT_ITEM,
  PROCESS_BAD_EXTERNAL_TYPE_IDENTITY,
  PROCESS_BAD_EXTERNAL_CASE_IDENTITY,
  PROCESS_BAD_EXTERNAL_CASE_MEMBER,
  PROCESS_BAD_EXTERNAL_RETURN_EXPRESSION_MAX,
  PROCESS_BAD_EXTERNAL_RETURN_EXPRESSION_COUNT,
  PROCESS_BAD_PROCESS_PROFILE,
  PROCESS_BAD_HANDLER_ARITY,
  PROCESS_BAD_HANDLER_ORDER,
  PROCESS_BAD_HANDLER_ASYNC,
  PROCESS_BAD_HANDLER_RETURN,
  PROCESS_BAD_HANDLER_CONST,
  PROCESS_BAD_HANDLER_THROWS,
  PROCESS_BAD_HANDLER_UNSAFE,
  PROCESS_BAD_HANDLER_BORROW,
  PROCESS_BAD_HANDLER_ANONYMOUS,
} process_bad_frontend_case;

static w_seed_frontend_expression *process_return_expression(void) {
  const w_seed_frontend_function *function = &fixture.functions[0];
  const w_seed_frontend_statement *statement =
      &fixture.statements[function->first_statement];
  return &fixture.expressions[statement->expression_index];
}

static bool expect_process_frontend_rejected(process_bad_frontend_case bad) {
  static const char SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return .success }\n"
      "entry(run)\n";
  CHECK(fixture_process_frontend(SOURCE));
  switch (bad) {
    case PROCESS_BAD_EXTERNAL_MODULE_COUNT:
      fixture.input.external_module_count = 2u;
      break;
    case PROCESS_BAD_EXTERNAL_SYMBOL_COUNT:
      fixture.external_modules[0].symbol_count = 5u;
      break;
    case PROCESS_BAD_EXTERNAL_MODULE_ID:
      fixture.external_modules[0].module_id =
          (w_seed_frontend_text){"std.other", 9u};
      break;
    case PROCESS_BAD_EXTERNAL_SYMBOL_NAME:
      fixture.external_symbols[0].name =
          (w_seed_frontend_text){"Wrong", 5u};
      break;
    case PROCESS_BAD_EXTERNAL_SYMBOL_KIND:
      fixture.external_symbols[0].kind = W_SEED_FRONTEND_EXTERNAL_VALUE;
      break;
    case PROCESS_BAD_EXTERNAL_SYMBOL_EXPORT:
      fixture.external_symbols[2].exported = false;
      break;
    case PROCESS_BAD_EXTERNAL_SYMBOL_CONST:
      fixture.external_symbols[3].is_const = false;
      break;
    case PROCESS_BAD_EXTERNAL_SYMBOL_RECEIVER:
      fixture.external_symbols[3].receiver_type =
          (w_seed_frontend_text){"Context", 7u};
      break;
    case PROCESS_BAD_EXTERNAL_SYMBOL_RETURN:
      fixture.external_symbols[3].return_type =
          (w_seed_frontend_text){"Context", 7u};
      break;
    case PROCESS_BAD_EXTERNAL_IMPORT_ITEM:
      fixture.import_items[0].name = (w_seed_frontend_text){"Wrong", 5u};
      break;
    case PROCESS_BAD_EXTERNAL_TYPE_IDENTITY:
      fixture.types[0].external_symbol_index = 1u;
      break;
    case PROCESS_BAD_EXTERNAL_CASE_IDENTITY:
      process_return_expression()->resolved_external_symbol_index = 2u;
      break;
    case PROCESS_BAD_EXTERNAL_CASE_MEMBER:
      process_return_expression()->member_name =
          (w_seed_frontend_text){"failure", 7u};
      break;
    case PROCESS_BAD_EXTERNAL_RETURN_EXPRESSION_MAX:
      fixture.statements[0].expression_index = UINT32_MAX;
      break;
    case PROCESS_BAD_EXTERNAL_RETURN_EXPRESSION_COUNT:
      fixture.statements[0].expression_index =
          (uint32_t)fixture.result.written.expressions;
      break;
    case PROCESS_BAD_PROCESS_PROFILE:
      fixture.host_scope.profile = (w_seed_frontend_text){"bogus", 5u};
      break;
    case PROCESS_BAD_HANDLER_ARITY:
      fixture.functions[0].parameter_count = 1u;
      break;
    case PROCESS_BAD_HANDLER_ORDER:
      fixture.parameters[0].type_index = 5u;
      break;
    case PROCESS_BAD_HANDLER_ASYNC:
      fixture.functions[0].is_async = false;
      break;
    case PROCESS_BAD_HANDLER_RETURN:
      fixture.functions[0].return_type = 4u;
      break;
    case PROCESS_BAD_HANDLER_CONST:
      fixture.functions[0].is_const = true;
      break;
    case PROCESS_BAD_HANDLER_THROWS:
      fixture.functions[0].is_throws = true;
      break;
    case PROCESS_BAD_HANDLER_UNSAFE:
      fixture.functions[0].is_unsafe = true;
      break;
    case PROCESS_BAD_HANDLER_BORROW:
      fixture.functions[0].has_borrow_clause = true;
      break;
    case PROCESS_BAD_HANDLER_ANONYMOUS:
      fixture.functions[0].is_anonymous_entry = true;
      fixture.entries[0].is_body = true;
      break;
  }
  setup_hir_output();
  fill_hir_output(0xa5u);
  const w_seed_hir0_input input = hir_input();
  const w_seed_hir0_result result_before = fixture.hir_result;
  const w_seed_hir0_status status =
      w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result);
  if (status == W_SEED_HIR0_OK)
    (void)fprintf(stderr, "process bad case unexpectedly accepted: %d\n",
                  (int)bad);
  CHECK(status != W_SEED_HIR0_OK);
  CHECK(hir_output_is_byte(0xa5u));
  CHECK(memcmp(&fixture.hir_result, &result_before, sizeof(result_before)) ==
        0);
  return true;
}

static bool test_process_hir_adversarial(void) {
  static const char SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return .success }\n"
      "entry(run)\n";
  for (int bad = PROCESS_BAD_EXTERNAL_MODULE_COUNT;
       bad <= PROCESS_BAD_HANDLER_ANONYMOUS; bad += 1)
    CHECK(expect_process_frontend_rejected((process_bad_frontend_case)bad));

  CHECK(lower_process(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  const w_seed_hir0_result result_before = fixture.hir_result;
  const w_seed_hir0_external_module saved_module =
      fixture.hir_external_modules[0];
  const w_seed_hir0_external_symbol saved_symbol =
      fixture.hir_external_symbols[3];
  const w_seed_hir0_type saved_type = fixture.hir_types[4];
  const w_seed_hir0_value saved_value = fixture.hir_values[0];
  const w_seed_hir0_function saved_function = fixture.hir_functions[0];
  const w_seed_hir0_parameter saved_parameter = fixture.hir_parameters[0];
  const w_seed_hir0_entry saved_entry = fixture.hir_entries[0];
  const w_seed_hir0_terminator saved_terminator = fixture.hir_terminators[0];

  /* Lifecycle and cleanup facts are not trusted merely because the canonical
   * names, profile, and success case remain intact. */
  fixture.hir_types[4].lifecycle = W_SEED_HIR0_LIFECYCLE_VALUE_COPY;
  fixture.hir_functions[0].direct_entry =
      W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_types[4] = saved_type;
  fixture.hir_functions[0] = saved_function;
  fixture.hir_types[4].release_contract =
      W_SEED_HIR0_RELEASE_CONTRACT_UNKNOWN;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_types[4] = saved_type;
  fixture.hir_entries[0].cleanup_obligation =
      W_SEED_HIR0_ENTRY_CLEANUP_NONE;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_entries[0] = saved_entry;
  fixture.hir_entries[0].first_cleanup_owner_parameter = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_entries[0] = saved_entry;
  fixture.hir_entries[0].cleanup_owner_parameter_count = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_entries[0] = saved_entry;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  /* Reseal each mutant with the production-private digest helpers. This
   * makes the following failures exercise lifecycle verification itself,
   * rather than only the unchanged-digest barrier. */
  fixture.hir_types[4].release_contract = W_SEED_HIR0_RELEASE_CONTRACT_NONE;
  fixture.hir_functions[0].direct_entry =
      W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_types[4] = saved_type;
  fixture.hir_functions[0] = saved_function;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  w_seed_hir0_type saved_string_type = fixture.hir_types[1];
  fixture.hir_types[1].lifecycle = W_SEED_HIR0_LIFECYCLE_VALUE_COPY;
  fixture.hir_types[1].release_contract = W_SEED_HIR0_RELEASE_CONTRACT_NONE;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_types[1] = saved_string_type;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_entries[0].first_cleanup_owner_parameter = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_entries[0] = saved_entry;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_entries[0].cleanup_owner_parameter_count = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_entries[0] = saved_entry;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_external_modules[0].module_id.count = 10u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_external_modules[0] = saved_module;
  fixture.hir_external_symbols[3].kind = W_SEED_HIR0_EXTERNAL_TYPE;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_external_symbols[3] = saved_symbol;
  fixture.hir_external_symbols[3].exported = false;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_external_symbols[3] = saved_symbol;
  fixture.hir_external_symbols[3].is_const = false;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_external_symbols[3] = saved_symbol;
  fixture.hir_external_symbols[3].receiver_type =
      fixture.hir_external_symbols[1].name;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_external_symbols[3] = saved_symbol;
  fixture.hir_external_symbols[3].return_type =
      fixture.hir_external_symbols[1].name;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_external_symbols[3] = saved_symbol;
  fixture.hir_external_symbols[3].parameter_count = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_external_symbols[3] = saved_symbol;
  fixture.hir_external_modules[0].first_symbol = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_external_modules[0] = saved_module;

  fixture.hir_types[4].external_module_index = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_types[4] = saved_type;
  fixture.hir_types[4].external_symbol_index = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_types[4] = saved_type;
  fixture.hir_values[0].kind = W_SEED_HIR0_VALUE_CONST_STRING;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[0] = saved_value;
  fixture.hir_values[0].member_name.count = 6u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[0] = saved_value;
  fixture.hir_values[0].external_symbol_index = 2u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[0] = saved_value;

  fixture.hir_functions[0].is_async = false;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_functions[0] = saved_function;
  fixture.hir_functions[0].is_const = true;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_functions[0] = saved_function;
  fixture.hir_functions[0].is_throws = true;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_functions[0] = saved_function;
  fixture.hir_functions[0].is_unsafe = true;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_functions[0] = saved_function;
  fixture.hir_functions[0].has_borrow_clause = true;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_functions[0] = saved_function;
  fixture.hir_functions[0].return_type = 4u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_functions[0] = saved_function;
  fixture.hir_parameters[0].type_index = 5u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_parameters[0] = saved_parameter;
  fixture.hir_parameters[0].ordinal = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_parameters[0] = saved_parameter;
  fixture.hir_entries[0].adapter_kind =
      W_SEED_HIR0_ENTRY_ADAPTER_DEFAULT_UNIT;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_entries[0] = saved_entry;
  fixture.hir_terminators[0].result_type = 4u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[0] = saved_terminator;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const uint8_t saved_receipt_byte = fixture.hir_receipt[0];
  fixture.hir_receipt[0] ^= 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_receipt[0] = saved_receipt_byte;
  const uint8_t saved_digest_byte = fixture.hir_result.semantic_digest[0];
  fixture.hir_result.semantic_digest[0] ^= 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_result.semantic_digest[0] = saved_digest_byte;
  const size_t saved_external_symbols =
      fixture.hir_result.required.external_symbols;
  fixture.hir_result.required.external_symbols = 3u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_result.required.external_symbols = saved_external_symbols;
  CHECK(memcmp(&fixture.hir_result, &result_before, sizeof(result_before)) ==
        0);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  /* Caller-owned external output arrays are transactional and non-aliasing. */
  CHECK(lower_process(SOURCE));
  const w_seed_hir0_input input = hir_input();
  setup_hir_output();
  fill_hir_output(0xa5u);
  fixture.hir_output.external_module_capacity = 0u;
  const w_seed_hir0_result capacity_result_before = fixture.hir_result;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(0xa5u));
  CHECK(memcmp(&fixture.hir_result, &capacity_result_before,
               sizeof(capacity_result_before)) == 0);
  setup_hir_output();
  fill_hir_output(0xa5u);
  fixture.hir_output.external_symbol_capacity = 3u;
  const w_seed_hir0_result symbol_capacity_result_before = fixture.hir_result;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(0xa5u));
  CHECK(memcmp(&fixture.hir_result, &symbol_capacity_result_before,
               sizeof(symbol_capacity_result_before)) == 0);
  setup_hir_output();
  fill_hir_output(0xa5u);
  w_seed_hir0_output alias = fixture.hir_output;
  alias.external_modules =
      (w_seed_hir0_external_module *)(void *)fixture.hir_external_symbols;
  CHECK(w_seed_hir0_run(&input, &alias, &fixture.hir_result) ==
        W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u));
  setup_hir_output();
  fill_hir_output(0xa5u);
  alias = fixture.hir_output;
  alias.external_symbols =
      (w_seed_hir0_external_symbol *)(void *)fixture.hir_external_modules;
  CHECK(w_seed_hir0_run(&input, &alias, &fixture.hir_result) ==
        W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u));

  /* Every nested import text slice is input-owned and must stay outside the
   * caller-owned HIR text buffer. */
  CHECK(lower_process(SOURCE));
  const w_seed_hir0_input import_input = hir_input();
  uint8_t path_before[11];
  (void)memcpy(path_before, fixture.imports[0].path.data,
               sizeof(path_before));
  setup_hir_output();
  fill_hir_output(0xa5u);
  alias = fixture.hir_output;
  alias.text_bytes = (uint8_t *)(void *)fixture.imports[0].path.data;
  alias.text_byte_capacity = fixture.hir_counts.text_bytes;
  const w_seed_hir0_result path_result_before = fixture.hir_result;
  CHECK(w_seed_hir0_run(&import_input, &alias, &fixture.hir_result) ==
        W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u) &&
        memcmp(path_before, fixture.imports[0].path.data,
               sizeof(path_before)) == 0 &&
        memcmp(&fixture.hir_result, &path_result_before,
               sizeof(path_result_before)) == 0);

  uint8_t local_name_before[sizeof("ProcessArguments") - 1u];
  (void)memcpy(local_name_before, fixture.import_items[0].local_name.data,
               sizeof(local_name_before));
  setup_hir_output();
  fill_hir_output(0xa5u);
  alias = fixture.hir_output;
  alias.text_bytes =
      (uint8_t *)(void *)fixture.import_items[0].local_name.data;
  alias.text_byte_capacity = fixture.hir_counts.text_bytes;
  const w_seed_hir0_result local_name_result_before = fixture.hir_result;
  CHECK(w_seed_hir0_run(&import_input, &alias, &fixture.hir_result) ==
        W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u) &&
        memcmp(local_name_before, fixture.import_items[0].local_name.data,
               sizeof(local_name_before)) == 0 &&
        memcmp(&fixture.hir_result, &local_name_result_before,
               sizeof(local_name_result_before)) == 0);

  static const char MODULE_ALIAS[] = "module";
  fixture.imports[0].alias =
      (w_seed_frontend_text){MODULE_ALIAS, sizeof(MODULE_ALIAS) - 1u};
  setup_hir_output();
  fill_hir_output(0xa5u);
  alias = fixture.hir_output;
  alias.text_bytes = (uint8_t *)(void *)fixture.imports[0].alias.data;
  alias.text_byte_capacity = fixture.hir_counts.text_bytes;
  const w_seed_hir0_result alias_result_before = fixture.hir_result;
  CHECK(w_seed_hir0_run(&import_input, &alias, &fixture.hir_result) ==
        W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u) &&
        memcmp(MODULE_ALIAS, fixture.imports[0].alias.data,
               sizeof(MODULE_ALIAS) - 1u) == 0 &&
        memcmp(&fixture.hir_result, &alias_result_before,
               sizeof(alias_result_before)) == 0);
  return true;
}

static bool test_capacity_and_alias_barriers(void) {
  CHECK(lower(CANONICAL_SOURCE));
  const w_seed_hir0_input input = hir_input();
  w_seed_hir0_counts counts;
  w_seed_hir0_result measured;
  CHECK(w_seed_hir0_measure(&input, &counts, &measured) == W_SEED_HIR0_OK);
  CHECK(counts.modules == fixture.hir_counts.modules &&
        counts.receipt_bytes == fixture.hir_counts.receipt_bytes);
  w_seed_hir0_result invalid_result;
  CHECK(w_seed_hir0_measure(&input, NULL, &invalid_result) ==
        W_SEED_HIR0_INVALID);
  CHECK(w_seed_hir0_measure(&input, &counts, NULL) == W_SEED_HIR0_INVALID);
  CHECK(w_seed_hir0_run(&input, NULL, &fixture.hir_result) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, NULL) ==
        W_SEED_HIR0_INVALID);
  typedef struct {
    size_t *capacity;
    size_t required;
  } capacity_case;
  capacity_case cases[] = {
      {&fixture.hir_output.module_capacity, 1u},
      {&fixture.hir_output.identity_capacity, 5u},
      {&fixture.hir_output.type_capacity, 4u},
      {&fixture.hir_output.function_capacity, 1u},
      {&fixture.hir_output.parameter_capacity, 0u},
      {&fixture.hir_output.block_capacity, 1u},
      {&fixture.hir_output.instruction_capacity, 1u},
      {&fixture.hir_output.call_capacity, 1u},
      {&fixture.hir_output.host_parameter_capacity, 1u},
      {&fixture.hir_output.argument_capacity, 1u},
      {&fixture.hir_output.requirement_capacity, 1u},
      {&fixture.hir_output.value_capacity, 1u},
      {&fixture.hir_output.terminator_capacity, 1u},
      {&fixture.hir_output.entry_capacity, 1u},
      {&fixture.hir_output.text_byte_capacity, 0u},
      {&fixture.hir_output.value_byte_capacity, 0u},
      {&fixture.hir_output.receipt_capacity, 0u}};
  for (size_t index = 0u; index < sizeof(cases) / sizeof(cases[0]);
       index += 1u) {
    if (index == 14u) cases[index].required = fixture.hir_counts.text_bytes;
    if (index == 15u) cases[index].required = fixture.hir_counts.value_bytes;
    if (index == 16u) cases[index].required = fixture.hir_counts.receipt_bytes;
    if (cases[index].required == 0u) continue;
    setup_hir_output();
    fill_hir_output(0xa5u);
    const w_seed_hir0_result result_before = fixture.hir_result;
    *cases[index].capacity = cases[index].required - 1u;
    CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
          W_SEED_HIR0_CAPACITY);
    CHECK(hir_output_is_byte(0xa5u));
    CHECK(memcmp(&fixture.hir_result, &result_before,
                 sizeof(result_before)) == 0);
  }
  setup_hir_output();
  w_seed_hir0_output alias = fixture.hir_output;
  alias.identities = (w_seed_hir0_identity *)alias.modules;
  fill_hir_output(0xa5u);
  CHECK(w_seed_hir0_run(&input, &alias, &fixture.hir_result) ==
        W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u));
  setup_hir_output();
  alias = fixture.hir_output;
  alias.modules = (w_seed_hir0_module *)alias.text_bytes;
  fill_hir_output(0xa5u);
  CHECK(w_seed_hir0_run(&input, &alias, &fixture.hir_result) ==
        W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u));
  setup_hir_output();
  const w_seed_frontend_module frontend_module_before = fixture.modules[0];
  alias = fixture.hir_output;
  alias.modules = (w_seed_hir0_module *)fixture.modules;
  CHECK(w_seed_hir0_run(&input, &alias, &fixture.hir_result) ==
        W_SEED_HIR0_INVALID);
  CHECK(memcmp(&fixture.modules[0], &frontend_module_before,
               sizeof(frontend_module_before)) == 0);
  setup_hir_output();
  fill_hir_output(0xa5u);
  CHECK(w_seed_hir0_run(
            &input, &fixture.hir_output,
            (w_seed_hir0_result *)(void *)fixture.hir_modules) ==
        W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u));
  setup_hir_output();
  fill_hir_output(0xa5u);
  const w_seed_frontend_result frontend_result_before = fixture.result;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output,
                        (w_seed_hir0_result *)(void *)&fixture.result) ==
        W_SEED_HIR0_INVALID);
  CHECK(memcmp(&fixture.result, &frontend_result_before,
               sizeof(frontend_result_before)) == 0 &&
        hir_output_is_byte(0xa5u));
  setup_hir_output();
  fill_hir_output(0xa5u);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output,
                        (w_seed_hir0_result *)(void *)fixture.hir_text) ==
        W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u));
  union {
    w_seed_hir0_counts counts;
    w_seed_hir0_result result;
  } measure_alias;
  (void)memset(&measure_alias, 0xa5, sizeof(measure_alias));
  CHECK(w_seed_hir0_measure(&input,
                            (w_seed_hir0_counts *)(void *)&measure_alias,
                            (w_seed_hir0_result *)(void *)&measure_alias) ==
        W_SEED_HIR0_INVALID);
  const uint8_t *measure_alias_bytes = (const uint8_t *)(const void *)&measure_alias;
  for (size_t byte = 0u; byte < sizeof(measure_alias); byte += 1u)
    CHECK(measure_alias_bytes[byte] == 0xa5u);
  setup_hir_output();
  const w_seed_hir0_program bridge_before = fixture.hir_program;
  const w_seed_hir0_output output_before = fixture.hir_output;
  const w_seed_hir0_result bridge_result_before = fixture.hir_result;
  CHECK(!w_seed_hir0_program_from_output(
      &fixture.hir_output, &fixture.hir_result,
      (w_seed_hir0_program *)(void *)&fixture.hir_output));
  CHECK(memcmp(&fixture.hir_program, &bridge_before, sizeof(bridge_before)) ==
            0 &&
        memcmp(&fixture.hir_output, &output_before, sizeof(output_before)) ==
            0 &&
        memcmp(&fixture.hir_result, &bridge_result_before,
               sizeof(bridge_result_before)) ==
            0);
  const w_seed_hir0_module module_before = fixture.hir_modules[0];
  CHECK(!w_seed_hir0_program_from_output(
      &fixture.hir_output, &fixture.hir_result,
      (w_seed_hir0_program *)(void *)fixture.hir_modules));
  CHECK(memcmp(&fixture.hir_modules[0], &module_before, sizeof(module_before)) ==
        0);
  const w_seed_hir0_result result_destination_before = fixture.hir_result;
  CHECK(!w_seed_hir0_program_from_output(
      &fixture.hir_output, &fixture.hir_result,
      (w_seed_hir0_program *)(void *)&fixture.hir_result));
  CHECK(memcmp(&fixture.hir_result, &result_destination_before,
               sizeof(result_destination_before)) == 0);
  uint8_t text_before[sizeof(fixture.hir_text)];
  (void)memcpy(text_before, fixture.hir_text, sizeof(text_before));
  CHECK(!w_seed_hir0_program_from_output(
      &fixture.hir_output, &fixture.hir_result,
      (w_seed_hir0_program *)(void *)fixture.hir_text));
  CHECK(memcmp(fixture.hir_text, text_before, sizeof(text_before)) == 0);
  w_seed_hir0_counts counts_before = counts;
  w_seed_hir0_result result_before = fixture.hir_result;
  fixture.input.host_scope = NULL;
  CHECK(w_seed_hir0_measure(&input, &counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  CHECK(memcmp(&counts, &counts_before, sizeof(counts)) == 0 &&
        memcmp(&fixture.hir_result, &result_before, sizeof(result_before)) ==
            0);
  fixture.input.host_scope = &fixture.host_scope;
  return true;
}

static bool test_verify_mutations(void) {
  CHECK(lower(CANONICAL_SOURCE));
  w_seed_hir0_program *program = &fixture.hir_program;
  w_seed_hir0_result *result = &fixture.hir_result;
  CHECK(w_seed_hir0_verify(program, result));
  w_seed_hir0_program saved_program = *program;
  w_seed_hir0_result saved_result = *result;
  const w_seed_hir0_module saved_module = fixture.hir_modules[0];
  const w_seed_hir0_identity saved_identity = fixture.hir_identities[4];
  const w_seed_hir0_function saved_function = fixture.hir_functions[0];
  const w_seed_hir0_block saved_block = fixture.hir_blocks[0];
  const w_seed_hir0_instruction saved_instruction = fixture.hir_instructions[0];
  const w_seed_hir0_call saved_call = fixture.hir_calls[0];
  const w_seed_hir0_argument saved_argument = fixture.hir_arguments[0];
  const w_seed_hir0_argument saved_argument1 = fixture.hir_arguments[1];
  const w_seed_hir0_value saved_value = fixture.hir_values[0];
  const w_seed_hir0_terminator saved_terminator = fixture.hir_terminators[0];
  const w_seed_hir0_entry saved_entry = fixture.hir_entries[0];
  const w_seed_hir0_requirement saved_requirement = fixture.hir_requirements[0];
  const w_seed_hir0_host_parameter saved_host_parameter0 =
      fixture.hir_host_parameters[0];
  const w_seed_hir0_host_parameter saved_host_parameter1 =
      fixture.hir_host_parameters[1];
#define RESTORE_RECORDS()                                                       \
  do {                                                                         \
    fixture.hir_modules[0] = saved_module;                                     \
    fixture.hir_identities[4] = saved_identity;                               \
    fixture.hir_functions[0] = saved_function;                                 \
    fixture.hir_blocks[0] = saved_block;                                       \
    fixture.hir_instructions[0] = saved_instruction;                           \
    fixture.hir_calls[0] = saved_call;                                         \
    fixture.hir_arguments[0] = saved_argument;                                 \
    fixture.hir_arguments[1] = saved_argument1;                                \
    fixture.hir_values[0] = saved_value;                                       \
    fixture.hir_terminators[0] = saved_terminator;                             \
    fixture.hir_entries[0] = saved_entry;                                      \
    fixture.hir_requirements[0] = saved_requirement;                           \
    fixture.hir_host_parameters[0] = saved_host_parameter0;                     \
    fixture.hir_host_parameters[1] = saved_host_parameter1;                     \
  } while (0)

  result->schema[0] = 'X';
  CHECK(!w_seed_hir0_verify(program, result));
  *result = saved_result;
  program->module_capacity = 0u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  program->text_byte_count -= 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_text[0] = 'X';
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_text[0] = '(';
  fixture.hir_identities[4].name.count = UINT32_MAX;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_identities[4].name.offset = UINT32_MAX;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_identities[4].profile.offset = UINT32_MAX;
  fixture.hir_identities[4].profile.count = 0u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_value_bytes[0] = 'X';
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_value_bytes[0] = 'H';
  fixture.hir_receipt[0] ^= 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_receipt[0] ^= 1u;
  result->semantic_digest[0] ^= 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *result = saved_result;
  result->provenance_digest[0] ^= 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *result = saved_result;

  fixture.hir_modules[0].module_index = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_functions[0].identity_index = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  program->parameters = NULL;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_blocks[0].first_instruction = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_blocks[0].ordinal = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_instructions[0].owner_block = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_calls[0].callee_identity = 3u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_calls[0].first_argument = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_host_parameters[0].label = (w_seed_hir0_text){0u, 0u};
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_arguments[0].label = fixture.hir_host_parameters[1].label;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_arguments[0].ordinal = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_arguments[0].type_index = 0u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_values[0].owner_index = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_terminators[0].kind =
      (w_seed_hir0_terminator_kind)(W_SEED_HIR0_TERMINATOR_RETURN_UNIT + 1);
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_entries[0].target_function = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  const uint32_t profile_offset = program->identities[4].profile.offset;
  fixture.hir_text[profile_offset] = 'X';
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_text[profile_offset] = 'n';
  fixture.hir_identities[4].parameter_count = 0u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_identities[4].first_parameter = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_identities[4].parameter_count = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_identities[4].first_requirement = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_calls[0].requirement_count = 0u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_requirements[0].owner_index = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_calls[0].argument_count = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_arguments[1].value_index = 0u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  w_seed_hir0_program bridge_before = *program;
  w_seed_hir0_result bad_result = *result;
  bad_result.status = W_SEED_HIR0_INVALID;
  CHECK(!w_seed_hir0_program_from_output(&fixture.hir_output, &bad_result,
                                         program));
  CHECK(program->modules == bridge_before.modules &&
        program->module_count == bridge_before.module_count &&
        program->module_capacity == bridge_before.module_capacity &&
        program->receipt == bridge_before.receipt &&
        program->receipt_count == bridge_before.receipt_count);
  w_seed_hir0_output truncated_output = fixture.hir_output;
  truncated_output.module_capacity = 0u;
  CHECK(!w_seed_hir0_program_from_output(&truncated_output, result, program));
  CHECK(program->modules == bridge_before.modules &&
        program->module_capacity == bridge_before.module_capacity);
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output, result,
                                        program));
  program->module_capacity = 0u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  CHECK(w_seed_hir0_verify(program, result));
#undef RESTORE_RECORDS
  return true;
}

static bool test_closed_frontend_barriers(void) {
  CHECK(lower(CANONICAL_SOURCE));
  const w_seed_hir0_input input = hir_input();
  const w_seed_frontend_result saved_result = fixture.result;
  typedef struct {
    size_t *required;
    size_t *written;
  } family_case;
  family_case unsupported[] = {
      {&fixture.result.required.imports, &fixture.result.written.imports},
      {&fixture.result.required.import_items,
       &fixture.result.written.import_items},
      {&fixture.result.required.structs, &fixture.result.written.structs},
      {&fixture.result.required.fields, &fixture.result.written.fields},
      {&fixture.result.required.type_declarations,
       &fixture.result.written.type_declarations},
      {&fixture.result.required.aliases, &fixture.result.written.aliases},
      {&fixture.result.required.facts, &fixture.result.written.facts},
      {&fixture.result.required.diagnostics,
       &fixture.result.written.diagnostics},
      {&fixture.result.required.diagnostic_facts,
       &fixture.result.written.diagnostic_facts},
      {&fixture.result.required.diagnostic_items,
       &fixture.result.written.diagnostic_items},
      {&fixture.result.required.diagnostic_labels,
       &fixture.result.written.diagnostic_labels},
      {&fixture.result.required.switch_arms,
       &fixture.result.written.switch_arms},
      {&fixture.result.required.enum_subset_members,
       &fixture.result.written.enum_subset_members},
      {&fixture.result.required.enum_membership_cases,
       &fixture.result.written.enum_membership_cases},
      {&fixture.result.required.generic_parameters,
       &fixture.result.written.generic_parameters},
      {&fixture.result.required.generic_applications,
       &fixture.result.written.generic_applications},
      {&fixture.result.required.generic_arguments,
       &fixture.result.written.generic_arguments},
      {&fixture.result.required.typed_const_expressions,
       &fixture.result.written.typed_const_expressions},
      {&fixture.result.required.const_values,
       &fixture.result.written.const_values},
      {&fixture.result.required.const_elements,
       &fixture.result.written.const_elements},
      {&fixture.result.required.const_declarations,
       &fixture.result.written.const_declarations},
  };
  for (size_t index = 0u; index < sizeof(unsupported) / sizeof(unsupported[0]);
       index += 1u) {
    *unsupported[index].required = 1u;
    *unsupported[index].written = 1u;
    CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) ==
          W_SEED_HIR0_UNSUPPORTED);
    fixture.result = saved_result;
  }
  const w_seed_frontend_symbol saved_symbol = fixture.symbols[1];
  fixture.symbols[1].name = (w_seed_frontend_text){"forged", 6u};
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  fixture.symbols[1] = saved_symbol;
  const w_seed_frontend_module saved_module = fixture.modules[0];
  fixture.modules[0].first_function = 1u;
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  fixture.modules[0] = saved_module;
  const w_seed_frontend_function saved_function = fixture.functions[0];
  fixture.functions[0].first_statement = 1u;
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  fixture.functions[0] = saved_function;
  const size_t call_expression_index = fixture.statements[0].expression_index;
  const w_seed_frontend_expression saved_call =
      fixture.expressions[call_expression_index];
  fixture.expressions[call_expression_index].first_argument = 1u;
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  fixture.expressions[call_expression_index] = saved_call;
  const w_seed_frontend_expression saved_callee = fixture.expressions[0];
  fixture.expressions[call_expression_index].left = 1u;
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  fixture.expressions[call_expression_index] = saved_call;
  const w_seed_frontend_argument saved_frontend_argument = fixture.arguments[0];
  fixture.arguments[0].expression_index = 0u;
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  fixture.arguments[0] = saved_frontend_argument;
  const w_seed_frontend_expression saved_first_value = fixture.expressions[1];
  fixture.expressions[1].const_byte_offset = 1u;
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  fixture.expressions[1] = saved_first_value;
  const w_seed_frontend_expression saved_second_value = fixture.expressions[2];
  fixture.expressions[2].const_byte_offset = 12u;
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  fixture.expressions[2] = saved_second_value;
  const w_seed_frontend_statement saved_statement = fixture.statements[0];
  fixture.statements[0].expression_index = 2u;
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  fixture.statements[0] = saved_statement;
  fixture.expressions[0] = saved_callee;
  const size_t saved_document_count = fixture.input.document_count;
  fixture.input.document_count = 2u;
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  fixture.input.document_count = saved_document_count;
  const w_seed_frontend_result saved_entry_result = fixture.result;
  fixture.result.required.entries = 2u;
  fixture.result.written.entries = 2u;
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  fixture.result = saved_entry_result;
  return true;
}

static bool test_typed_interpolation_value_tree(void) {
  static const char SOURCE[] =
      "fn main() { print(message: \"The answer is ${6 * 7}\", suffix: \"!\") }\n"
      "entry(main)\n";
  CHECK(fixture_frontend(SOURCE));
  CHECK(fixture.result.written.interpolation_segments == 2u);
  setup_hir_output();
  const w_seed_hir0_input input = hir_input();
  w_seed_hir0_counts counts;
  w_seed_hir0_result result;
  (void)memset(&counts, 0xa4, sizeof(counts));
  (void)memset(&result, 0xa4, sizeof(result));
  const w_seed_hir0_counts rejected_counts = counts;
  const w_seed_hir0_result rejected_measure = result;
  const uint32_t saved_owner =
      fixture.interpolation_segments[0].owner_expression;
  fixture.interpolation_segments[0].owner_expression = W_SEED_FRONTEND_NONE;
  CHECK(w_seed_hir0_measure(&input, &counts, &result) ==
        W_SEED_HIR0_UNSUPPORTED);
  CHECK(memcmp(&counts, &rejected_counts, sizeof(counts)) == 0);
  CHECK(memcmp(&result, &rejected_measure, sizeof(result)) == 0);
  fixture.interpolation_segments[0].owner_expression = saved_owner;

  CHECK(w_seed_hir0_measure(&input, &counts, &result) == W_SEED_HIR0_OK);
  CHECK(counts.types == 4u && counts.values == 5u &&
        counts.interpolation_segments == 2u && counts.value_bytes == 15u);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &result) ==
        W_SEED_HIR0_OK);
  w_seed_hir0_program program;
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output, &result,
                                        &program));
  CHECK(w_seed_hir0_verify(&program, &result));
  CHECK(program.arguments[0].value_index == 3u &&
        program.arguments[1].value_index == 4u);
  CHECK(program.values[0].kind == W_SEED_HIR0_VALUE_CONST_I64 &&
        program.values[0].integer_value == 6 &&
        program.values[1].kind == W_SEED_HIR0_VALUE_CONST_I64 &&
        program.values[1].integer_value == 7);
  CHECK(program.values[2].kind == W_SEED_HIR0_VALUE_BINARY_I64 &&
        program.values[2].binary_operator == W_SEED_HIR0_BINARY_MULTIPLY &&
        program.values[2].left_value == 0u &&
        program.values[2].right_value == 1u);
  CHECK(program.values[3].kind ==
            W_SEED_HIR0_VALUE_INTERPOLATED_STRING &&
        program.values[3].first_interpolation_segment == 0u &&
        program.values[3].interpolation_segment_count == 2u);
  CHECK(program.interpolation_segments[0].kind ==
            W_SEED_HIR0_INTERPOLATION_TEXT &&
        program.interpolation_segments[0].byte_count == 14u &&
        program.interpolation_segments[1].kind ==
            W_SEED_HIR0_INTERPOLATION_VALUE &&
        program.interpolation_segments[1].value_index == 2u);
  CHECK(memcmp(program.value_bytes, "The answer is !", 15u) == 0);

  const w_seed_hir0_value saved_binary = fixture.hir_values[2];
  const w_seed_hir0_value saved_interpolation = fixture.hir_values[3];
  const w_seed_hir0_interpolation_segment saved_segment =
      fixture.hir_interpolation_segments[1];
  fixture.hir_values[2].left_value = 1u;
  CHECK(!w_seed_hir0_verify(&program, &result));
  fixture.hir_values[2] = saved_binary;
  fixture.hir_values[3].interpolation_segment_count = 1u;
  CHECK(!w_seed_hir0_verify(&program, &result));
  fixture.hir_values[3] = saved_interpolation;
  fixture.hir_interpolation_segments[1].owner_value = 2u;
  CHECK(!w_seed_hir0_verify(&program, &result));
  fixture.hir_interpolation_segments[1] = saved_segment;
  CHECK(w_seed_hir0_verify(&program, &result));

  setup_hir_output();
  fill_hir_output(0xa5u);
  w_seed_hir0_result rejected_result;
  (void)memset(&rejected_result, 0xa5, sizeof(rejected_result));
  const w_seed_hir0_result rejected_before = rejected_result;
  fixture.hir_output.interpolation_segment_capacity = 1u;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected_result) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(0xa5u));
  CHECK(memcmp(&rejected_result, &rejected_before,
               sizeof(rejected_result)) == 0);

  setup_hir_output();
  fill_hir_output(0xa5u);
  w_seed_hir0_output aliased = fixture.hir_output;
  aliased.interpolation_segments =
      (w_seed_hir0_interpolation_segment *)(void *)fixture.hir_values;
  CHECK(w_seed_hir0_run(&input, &aliased, &rejected_result) ==
        W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u));
  CHECK(memcmp(&rejected_result, &rejected_before,
               sizeof(rejected_result)) == 0);
  return true;
}

static bool test_builtin_display_value_tree(void) {
  static const char SOURCE[] =
      "fn main() { let state = \"open\" "
      "print(message: \"${true}/${false}/${state}\", suffix: \"!\") }\n"
      "entry(main)\n";
  CHECK(fixture_frontend(SOURCE));
  setup_hir_output();
  const w_seed_hir0_input input = hir_input();
  w_seed_hir0_counts counts;
  w_seed_hir0_result result;
  CHECK(w_seed_hir0_measure(&input, &counts, &result) == W_SEED_HIR0_OK);
  CHECK(counts.bindings == 1u && counts.values == 6u &&
        counts.interpolation_segments == 5u && counts.value_bytes == 7u);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &result) ==
        W_SEED_HIR0_OK);
  w_seed_hir0_program program;
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output, &result,
                                        &program));
  CHECK(w_seed_hir0_verify(&program, &result));
  CHECK(program.bindings[0].initializer_value == 0u &&
        program.values[0].kind == W_SEED_HIR0_VALUE_CONST_STRING &&
        program.values[0].owner_kind == W_SEED_HIR0_VALUE_OWNER_BINDING &&
        program.values[1].kind == W_SEED_HIR0_VALUE_CONST_BOOL &&
        program.values[1].type_index == 3u && program.values[1].bool_value &&
        program.values[2].kind == W_SEED_HIR0_VALUE_CONST_BOOL &&
        program.values[2].type_index == 3u && !program.values[2].bool_value &&
        program.values[3].kind == W_SEED_HIR0_VALUE_BINDING_READ &&
        program.values[3].type_index == 1u &&
        program.values[3].binding_index == 0u &&
        program.values[4].kind == W_SEED_HIR0_VALUE_INTERPOLATED_STRING &&
        program.values[4].type_index == 1u &&
        program.values[5].kind == W_SEED_HIR0_VALUE_CONST_STRING &&
        program.values[5].type_index == 1u);
  CHECK(program.interpolation_segments[0].kind ==
            W_SEED_HIR0_INTERPOLATION_VALUE &&
        program.interpolation_segments[0].value_index == 1u &&
        program.interpolation_segments[1].kind ==
            W_SEED_HIR0_INTERPOLATION_TEXT &&
        program.interpolation_segments[2].kind ==
            W_SEED_HIR0_INTERPOLATION_VALUE &&
        program.interpolation_segments[2].value_index == 2u &&
        program.interpolation_segments[3].kind ==
            W_SEED_HIR0_INTERPOLATION_TEXT &&
        program.interpolation_segments[4].kind ==
            W_SEED_HIR0_INTERPOLATION_VALUE &&
        program.interpolation_segments[4].value_index == 3u);
  CHECK(memcmp(program.value_bytes, "open//!", 7u) == 0);
  return true;
}

static bool test_typed_immutable_binding_values(void) {
  static const char SOURCE[] =
      "fn serve() { let table = 6 * 7 let isOpen = true let state = \"open\" "
      "print(message: \"Table ${table}; open: ${isOpen}; state: ${state}\", "
      "suffix: \"!\") }\nentry(serve)\n";
  CHECK(lower(SOURCE));
  CHECK(fixture.hir_program.binding_count == 3u &&
        fixture.hir_program.instruction_count == 4u &&
        fixture.hir_program.value_count == 10u &&
        fixture.hir_program.interpolation_segment_count == 6u);
  CHECK(fixture.hir_program.bindings[0].type_index == 2u &&
        fixture.hir_program.bindings[0].initializer_value == 2u &&
        fixture.hir_program.values[2].kind ==
            W_SEED_HIR0_VALUE_BINARY_I64 &&
        fixture.hir_program.values[2].owner_kind ==
            W_SEED_HIR0_VALUE_OWNER_BINDING &&
        fixture.hir_program.values[2].owner_index == 0u);
  CHECK(fixture.hir_program.bindings[1].type_index == 3u &&
        fixture.hir_program.bindings[1].initializer_value == 3u &&
        fixture.hir_program.values[3].kind == W_SEED_HIR0_VALUE_CONST_BOOL &&
        fixture.hir_program.values[3].owner_kind ==
            W_SEED_HIR0_VALUE_OWNER_BINDING &&
        fixture.hir_program.values[3].bool_value);
  CHECK(fixture.hir_program.bindings[2].type_index == 1u &&
        fixture.hir_program.bindings[2].initializer_value == 4u &&
        fixture.hir_program.values[4].kind ==
            W_SEED_HIR0_VALUE_CONST_STRING);
  CHECK(fixture.hir_program.values[5].kind ==
            W_SEED_HIR0_VALUE_BINDING_READ &&
        fixture.hir_program.values[5].type_index == 2u &&
        fixture.hir_program.values[5].binding_index == 0u &&
        fixture.hir_program.values[6].kind ==
            W_SEED_HIR0_VALUE_BINDING_READ &&
        fixture.hir_program.values[6].type_index == 3u &&
        fixture.hir_program.values[6].binding_index == 1u &&
        fixture.hir_program.values[7].kind ==
            W_SEED_HIR0_VALUE_BINDING_READ &&
        fixture.hir_program.values[7].type_index == 1u &&
        fixture.hir_program.values[7].binding_index == 2u &&
        fixture.hir_program.values[8].kind ==
            W_SEED_HIR0_VALUE_INTERPOLATED_STRING &&
        fixture.hir_program.values[9].kind ==
            W_SEED_HIR0_VALUE_CONST_STRING);
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));

  const w_seed_hir0_binding saved_binding = fixture.hir_bindings[0];
  fixture.hir_bindings[0].initializer_value = 3u;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_bindings[0] = saved_binding;

  const w_seed_hir0_value saved_initializer = fixture.hir_values[2];
  fixture.hir_values[2].owner_index = 1u;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_values[2] = saved_initializer;

  const w_seed_hir0_value saved_read = fixture.hir_values[5];
  fixture.hir_values[5].type_index = 1u;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_values[5] = saved_read;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool test_local_unit_call_and_parameter_reads(void) {
  static const char SOURCE[] =
      "fn announce(table: i64, isOpen: Bool) { "
      "print(message: \"Table ${table}; open: ${isOpen}\", suffix: \"\") }\n"
      "fn main() { announce(isOpen: true, table: 6 * 7) }\n"
      "entry(main)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 2u && program->parameter_count == 2u &&
        program->call_count == 2u && program->argument_count == 4u &&
        program->value_count == 8u && program->entry_count == 1u);
  CHECK(program->functions[0].first_parameter == 0u &&
        program->functions[0].parameter_count == 2u &&
        program->parameters[0].owner_function == 0u &&
        program->parameters[0].ordinal == 0u &&
        program->parameters[0].type_index == 2u &&
        program->parameters[1].owner_function == 0u &&
        program->parameters[1].ordinal == 1u &&
        program->parameters[1].type_index == 3u);
  CHECK(program->identities[1].kind == W_SEED_HIR0_IDENTITY_FUNCTION &&
        program->identities[1].first_parameter == 0u &&
        program->identities[1].parameter_count == 2u &&
        program->identities[1].return_type == 0u);
  CHECK(program->calls[0].callee_identity >=
            program->module_count + program->function_count +
                program->entry_count &&
        program->calls[1].callee_identity == 1u &&
        program->calls[1].first_requirement == W_SEED_HIR0_NONE &&
        program->calls[1].requirement_count == 0u);
  CHECK(program->values[0].kind == W_SEED_HIR0_VALUE_PARAMETER_READ &&
        program->values[0].parameter_index == 0u &&
        program->values[0].type_index == 2u &&
        program->values[1].kind == W_SEED_HIR0_VALUE_PARAMETER_READ &&
        program->values[1].parameter_index == 1u &&
        program->values[1].type_index == 3u);
  CHECK(program->arguments[2].ordinal == 0u &&
        program->arguments[2].parameter_ordinal == 1u &&
        program->arguments[2].type_index == 3u &&
        program->values[program->arguments[2].value_index].kind ==
            W_SEED_HIR0_VALUE_CONST_BOOL &&
        program->arguments[3].ordinal == 1u &&
        program->arguments[3].parameter_ordinal == 0u &&
        program->arguments[3].type_index == 2u &&
        program->values[program->arguments[3].value_index].kind ==
            W_SEED_HIR0_VALUE_BINARY_I64);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_argument saved_argument = fixture.hir_arguments[2];
  fixture.hir_arguments[2].parameter_ordinal = 0u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_arguments[2] = saved_argument;
  const w_seed_hir0_value saved_parameter = fixture.hir_values[0];
  fixture.hir_values[0].parameter_index = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[0] = saved_parameter;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_scalar_return_and_call_result(void) {
  static const char SOURCE[] =
      "fn tableNumber(): i64 { return 6 * 7 }\n"
      "fn main() { let table = tableNumber() "
      "print(message: \"Table ${table}\", suffix: \"\") }\n"
      "entry(main)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 2u && program->call_count == 2u &&
        program->binding_count == 1u && program->terminator_count == 2u &&
        program->functions[0].return_type == 2u &&
        program->functions[1].return_type == 0u);
  CHECK(program->calls[0].result_type == 2u &&
        program->identities[program->calls[0].callee_identity].target_index ==
            0u &&
        program->instructions[program->calls[0].owner_instruction]
                .result_type == 2u);
  const uint32_t initializer = program->bindings[0].initializer_value;
  CHECK(initializer < program->value_count &&
        program->values[initializer].kind == W_SEED_HIR0_VALUE_CALL_RESULT &&
        program->values[initializer].type_index == 2u &&
        program->values[initializer].call_index == 0u);
  CHECK(program->terminators[0].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        program->terminators[0].result_type == 2u &&
        program->terminators[0].value_index < program->value_count &&
        program->values[program->terminators[0].value_index].kind ==
            W_SEED_HIR0_VALUE_BINARY_I64 &&
        program->terminators[1].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_UNIT);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_value saved_result = fixture.hir_values[initializer];
  fixture.hir_values[initializer].call_index = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[initializer] = saved_result;
  const w_seed_hir0_terminator saved_terminator = fixture.hir_terminators[0];
  fixture.hir_terminators[0].result_type = 3u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[0] = saved_terminator;
  const w_seed_hir0_call saved_call = fixture.hir_calls[0];
  fixture.hir_calls[0].result_type = 3u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_calls[0] = saved_call;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_scalar_if_value_diamond(void) {
  static const char SOURCE[] =
      "fn serve(isOpen: Bool, seats: i64): i64 { "
      "let next = if isOpen { seats + 1 } else { seats - 1 } "
      "return next }\n"
      "fn flag(isOpen: Bool): Bool { return if isOpen { true } else { false } }\n"
      "entry(serve)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 2u && program->block_count == 8u &&
        program->block_arguments == fixture.hir_block_arguments &&
        program->block_argument_count == 2u &&
        program->functions[0].block_count == 4u &&
        program->functions[1].block_count == 4u);
  CHECK(program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[0].logical_operator ==
            W_SEED_HIR0_LOGICAL_NONE &&
        program->terminators[0].result_type == W_SEED_HIR0_TYPE_I64 &&
        program->terminators[0].target_block == 1u &&
        program->terminators[0].else_block == 2u &&
        edge_value_at(program, 1u) != W_SEED_HIR0_NONE &&
        edge_value_at(program, 2u) != W_SEED_HIR0_NONE &&
        program->terminators[3].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_VALUE);
  CHECK(program->blocks[3].block_argument_count == 1u &&
        program->block_arguments[0].owner_block == 3u &&
        program->block_arguments[0].ordinal == 0u &&
        program->block_arguments[0].type_index == W_SEED_HIR0_TYPE_I64 &&
        program->blocks[7].block_argument_count == 1u &&
        program->block_arguments[1].owner_block == 7u &&
        program->block_arguments[1].type_index == W_SEED_HIR0_TYPE_BOOL);
  CHECK(program->terminators[4].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[4].result_type == W_SEED_HIR0_TYPE_BOOL &&
        edge_value_at(program, 5u) != W_SEED_HIR0_NONE &&
        edge_value_at(program, 6u) != W_SEED_HIR0_NONE &&
        program->terminators[7].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_VALUE);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_terminator saved_branch = fixture.hir_terminators[0];
  fixture.hir_terminators[0].result_type = W_SEED_HIR0_TYPE_BOOL;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[0] = saved_branch;
  fixture.hir_terminators[0].logical_operator = W_SEED_HIR0_LOGICAL_AND;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[0] = saved_branch;

  const uint32_t saved_incoming = edge_value_at(program, 1u);
  FIXTURE_EDGE_VALUE_SLOT(1u) = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  FIXTURE_EDGE_VALUE_SLOT(1u) = saved_incoming;
  const w_seed_hir0_value saved_incoming_value =
      fixture.hir_values[saved_incoming];
  fixture.hir_values[saved_incoming].owner_index = 2u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[saved_incoming] = saved_incoming_value;
  fixture.hir_values[saved_incoming].owner_ordinal = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[saved_incoming] = saved_incoming_value;
  fixture.hir_values[saved_incoming].type_index = W_SEED_HIR0_TYPE_BOOL;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[saved_incoming] = saved_incoming_value;

  const uint32_t saved_target = fixture.hir_terminators[1].target_block;
  fixture.hir_terminators[1].target_block = 2u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[1].target_block = saved_target;
  const w_seed_hir0_block saved_join = fixture.hir_blocks[3];
  const w_seed_hir0_block_argument saved_argument = fixture.hir_block_arguments[0];
  fixture.hir_block_arguments[0].type_index = W_SEED_HIR0_TYPE_BOOL;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_block_arguments[0] = saved_argument;
  fixture.hir_blocks[3].first_block_argument =
      (uint32_t)program->block_argument_count;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_blocks[3] = saved_join;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const uint32_t saved_read = program->values[program->bindings[0].initializer_value]
                                  .block_argument_index;
  fixture.hir_values[program->bindings[0].initializer_value].block_argument_index =
      1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[program->bindings[0].initializer_value].block_argument_index =
      saved_read;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_nested_scalar_if_value_diamond(void) {
  static const char SOURCE[] =
      "fn choose(outer: Bool, inner: Bool, open: i64, middle: i64, "
      "closed: i64): i64 { return if outer { if inner { open } else { "
      "middle } } else { closed } }\n"
      "fn main() { let first = choose(outer: true, inner: true, open: 1, "
      "middle: 2, closed: 3) let second = choose(outer: true, inner: false, "
      "open: 1, middle: 2, closed: 3) let third = choose(outer: false, "
      "inner: false, open: 1, middle: 2, closed: 3) print(message: \"${first},${second},${third}\", "
      "suffix: \"\") }\n"
      "entry(main)\n";
  CHECK(lower(SOURCE));
  w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 2u && program->block_count == 8u &&
        program->block_argument_count == 2u &&
        program->functions[0].first_block == 0u &&
        program->functions[0].block_count == 7u &&
        program->functions[1].first_block == 7u &&
        program->functions[1].block_count == 1u);

  const w_seed_hir0_terminator *outer_branch = &program->terminators[0];
  const w_seed_hir0_terminator *inner_branch = &program->terminators[1];
  CHECK(outer_branch->kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        outer_branch->result_type == W_SEED_HIR0_TYPE_I64 &&
        outer_branch->target_block == 1u && outer_branch->else_block == 5u &&
        inner_branch->kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        inner_branch->result_type == W_SEED_HIR0_TYPE_I64 &&
        inner_branch->target_block == 2u && inner_branch->else_block == 3u);
  CHECK(program->blocks[4].block_argument_count == 1u &&
        program->blocks[4].first_block_argument == 0u &&
        program->block_arguments[0].owner_block == 4u &&
        program->block_arguments[0].ordinal == 0u &&
        program->block_arguments[0].type_index == W_SEED_HIR0_TYPE_I64 &&
        program->blocks[6].block_argument_count == 1u &&
        program->blocks[6].first_block_argument == 1u &&
        program->block_arguments[1].owner_block == 6u &&
        program->block_arguments[1].ordinal == 0u &&
        program->block_arguments[1].type_index == W_SEED_HIR0_TYPE_I64);
  CHECK(program->terminators[2].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[3].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[4].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[5].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[2].target_block == 4u &&
        program->terminators[3].target_block == 4u &&
        program->terminators[4].target_block == 6u &&
        program->terminators[5].target_block == 6u &&
        edge_value_at(program, 2u) != W_SEED_HIR0_NONE &&
        edge_value_at(program, 3u) != W_SEED_HIR0_NONE &&
        edge_value_at(program, 4u) != W_SEED_HIR0_NONE &&
        edge_value_at(program, 5u) != W_SEED_HIR0_NONE);
  CHECK(program->values[edge_value_at(program, 2u)].kind ==
            W_SEED_HIR0_VALUE_PARAMETER_READ &&
        program->values[edge_value_at(program, 3u)].kind ==
            W_SEED_HIR0_VALUE_PARAMETER_READ &&
        program->values[edge_value_at(program, 4u)].kind ==
            W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        program->values[edge_value_at(program, 4u)]
                .block_argument_index == 0u &&
        program->values[edge_value_at(program, 5u)].kind ==
            W_SEED_HIR0_VALUE_PARAMETER_READ);
  CHECK(program->terminators[6].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        program->values[program->terminators[6].value_index].kind ==
            W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        program->values[program->terminators[6].value_index]
                .block_argument_index == 1u &&
        program->terminators[7].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_UNIT &&
        w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_block saved_inner_join = fixture.hir_blocks[4];
  fixture.hir_blocks[4].owner_function = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_blocks[4] = saved_inner_join;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_terminator saved_outer_branch = fixture.hir_terminators[0];
  fixture.hir_terminators[0].target_block = 2u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[0] = saved_outer_branch;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_value saved_outer_incoming =
      fixture.hir_values[edge_value_at(program, 4u)];
  const uint32_t outer_incoming_index = edge_value_at(program, 4u);
  fixture.hir_values[outer_incoming_index].block_argument_index = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[outer_incoming_index] = saved_outer_incoming;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const uint32_t inner_incoming_index = edge_value_at(program, 2u);
  const w_seed_hir0_value saved_inner_incoming =
      fixture.hir_values[inner_incoming_index];
  fixture.hir_values[inner_incoming_index].parameter_index = 5u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[inner_incoming_index] = saved_inner_incoming;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  size_t outer_expression = SIZE_MAX;
  size_t outer_span = 0u;
  for (size_t index = 0u; index < fixture.result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *candidate = &fixture.expressions[index];
    if (candidate->kind != W_SEED_FRONTEND_EXPR_IF ||
        candidate->span.end_byte < candidate->span.start_byte)
      continue;
    const size_t span = candidate->span.end_byte - candidate->span.start_byte;
    if (span > outer_span) {
      outer_expression = index;
      outer_span = span;
    }
  }
  CHECK(outer_expression != SIZE_MAX);
  const w_seed_frontend_expression saved_outer_expression =
      fixture.expressions[outer_expression];
  CHECK(saved_outer_expression.left != W_SEED_FRONTEND_NONE &&
        fixture.expressions[saved_outer_expression.left].inferred_type !=
            saved_outer_expression.inferred_type);
  fixture.expressions[outer_expression].inferred_type =
      fixture.expressions[saved_outer_expression.left].inferred_type;
  setup_hir_output();
  fill_hir_output(0xa5u);
  w_seed_hir0_result rejected;
  (void)memset(&rejected, 0x42, sizeof(rejected));
  const w_seed_hir0_result rejected_snapshot = rejected;
  const w_seed_hir0_input input = hir_input();
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_UNSUPPORTED);
  CHECK(hir_output_is_byte(0xa5u));
  CHECK(memcmp(&rejected, &rejected_snapshot, sizeof(rejected)) == 0);
  fixture.expressions[outer_expression] = saved_outer_expression;
  return true;
}

static bool test_if_diamond_cfg(void) {
  static const char SOURCE[] =
      "fn serve(isOpen: Bool) { "
      "if isOpen { print(message: \"Kitchen open\", suffix: \"\") } "
      "else { print(message: \"Kitchen closed\", suffix: \"\") } "
      "print(message: \"After service\", suffix: \"\") }\n"
      "fn main() { serve(isOpen: true) serve(isOpen: false) }\n"
      "entry(main)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 2u && program->block_count == 5u &&
        program->terminator_count == 5u && program->call_count == 5u &&
        program->entry_count == 1u);
  CHECK(program->functions[0].first_block == 0u &&
        program->functions[0].block_count == 4u &&
        program->functions[1].first_block == 4u &&
        program->functions[1].block_count == 1u);
  CHECK(program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[0].target_block == 1u &&
        program->terminators[0].else_block == 2u &&
        program->terminators[1].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[1].target_block == 3u &&
        program->terminators[2].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[2].target_block == 3u &&
        program->terminators[3].kind == W_SEED_HIR0_TERMINATOR_RETURN_UNIT &&
        program->terminators[4].kind == W_SEED_HIR0_TERMINATOR_RETURN_UNIT);
  CHECK(program->values[program->terminators[0].value_index].type_index == 3u &&
        program->values[program->terminators[0].value_index].owner_kind ==
            W_SEED_HIR0_VALUE_OWNER_TERMINATOR);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_terminator saved_branch = fixture.hir_terminators[0];
  fixture.hir_terminators[0].target_block = 4u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[0] = saved_branch;
  fixture.hir_terminators[0].else_block = 3u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[0] = saved_branch;
  const w_seed_hir0_terminator saved_then = fixture.hir_terminators[1];
  fixture.hir_terminators[1].target_block = 2u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[1].target_block = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[1] = saved_then;

  const uint32_t branch_value = saved_branch.value_index;
  const w_seed_hir0_value saved_condition = fixture.hir_values[branch_value];
  fixture.hir_values[branch_value].type_index = 2u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[branch_value] = saved_condition;
  fixture.hir_values[branch_value].owner_kind =
      W_SEED_HIR0_VALUE_OWNER_ARGUMENT;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[branch_value] = saved_condition;

  const w_seed_hir0_block saved_entry = fixture.hir_blocks[0];
  fixture.hir_blocks[0].next_block = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_blocks[0] = saved_entry;
  const w_seed_hir0_block saved_then_block = fixture.hir_blocks[1];
  fixture.hir_blocks[1].first_instruction += 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_blocks[1] = saved_then_block;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_if_without_else_cfg(void) {
  static const char SOURCE[] =
      "fn main() { if true { let label = \"Open\" "
      "print(message: label, suffix: \"\") } "
      "print(message: \"After\", suffix: \"\") }\n"
      "entry(main)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 1u && program->block_count == 4u &&
        program->call_count == 2u && program->binding_count == 1u &&
        program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[1].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[2].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->blocks[2].instruction_count == 0u &&
        program->terminators[3].kind == W_SEED_HIR0_TERMINATOR_RETURN_UNIT);
  CHECK(program->blocks[1].instruction_count == 2u &&
        program->bindings[0].owner_block == 1u &&
        program->values[1].kind == W_SEED_HIR0_VALUE_BINDING_READ);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_sequential_if_diamonds(void) {
  static const char SOURCE[] =
      "fn main() { if true { print(message: \"first\", suffix: \"\") } "
      "print(message: \"middle\", suffix: \"\") "
      "if false { print(message: \"second\", suffix: \"\") } "
      "else { print(message: \"third\", suffix: \"\") } "
      "print(message: \"after\", suffix: \"\") }\n"
      "entry(main)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 1u && program->block_count == 7u &&
        program->terminator_count == 7u);
  CHECK(program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[0].target_block == 1u &&
        program->terminators[0].else_block == 2u &&
        program->terminators[1].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[1].target_block == 3u &&
        program->terminators[2].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[2].target_block == 3u &&
        program->terminators[3].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[3].target_block == 4u &&
        program->terminators[3].else_block == 5u &&
        program->terminators[4].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[4].target_block == 6u &&
        program->terminators[5].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[5].target_block == 6u &&
        program->terminators[6].kind == W_SEED_HIR0_TERMINATOR_RETURN_UNIT);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_nested_if_diamonds(void) {
  static const char SOURCE[] =
      "fn serve(isOpen: Bool, isKitchen: Bool) { "
      "if isOpen { print(message: \"outer open\", suffix: \"\") "
      "if isKitchen { print(message: \"inner open\", suffix: \"\") } "
      "else { print(message: \"inner closed\", suffix: \"\") } "
      "print(message: \"outer open after\", suffix: \"\") } "
      "else { print(message: \"outer closed\", suffix: \"\") "
      "if isKitchen { print(message: \"inner open closed\", suffix: \"\") } "
      "else { print(message: \"inner closed closed\", suffix: \"\") } "
      "print(message: \"outer closed after\", suffix: \"\") } "
      "print(message: \"post join\", suffix: \"\") }\n"
      "fn main() { serve(isOpen: true, isKitchen: true) "
      "serve(isOpen: true, isKitchen: false) "
      "serve(isOpen: false, isKitchen: true) "
      "serve(isOpen: false, isKitchen: false) }\n"
      "entry(main)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->functions[0].block_count == 10u &&
        program->functions[1].block_count == 1u &&
        program->block_count == 11u);
  CHECK(program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[0].target_block == 1u &&
        program->terminators[0].else_block == 5u);
  CHECK(program->terminators[1].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[1].target_block == 2u &&
        program->terminators[1].else_block == 3u);
  CHECK(program->terminators[2].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[2].target_block == 4u &&
        program->terminators[3].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[3].target_block == 4u &&
        program->terminators[4].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[4].target_block == 9u);
  CHECK(program->terminators[5].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[5].target_block == 6u &&
        program->terminators[5].else_block == 7u);
  CHECK(program->terminators[6].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[6].target_block == 8u &&
        program->terminators[7].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[7].target_block == 8u &&
        program->terminators[8].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[8].target_block == 9u &&
        program->terminators[9].kind == W_SEED_HIR0_TERMINATOR_RETURN_UNIT);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_terminator saved = fixture.hir_terminators[1];
  fixture.hir_terminators[1].else_block = 4u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[1] = saved;
  fixture.hir_terminators[4].target_block = 5u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[4].target_block = 9u;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_enum_switch_hir(void) {
  static const char SOURCE[] =
      "enum Course { starter main dessert }\n"
      "fn price(course: Course): i64 { return switch course { "
      "case .starter: 10 case .main: 30 case .dessert: 20 } }\n"
      "entry { }\n";
  CHECK(fixture_parse(SOURCE));
  configure_host();
  const w_seed_frontend_status frontend_status =
      w_seed_frontend_run(&fixture.input, &fixture.output, &fixture.result);
  CHECK(frontend_status == W_SEED_FRONTEND_OK);
  setup_hir_output();
  const w_seed_hir0_input input = {&fixture.input, &fixture.output,
                                   &fixture.result};
  w_seed_hir0_counts measured;
  w_seed_hir0_result measure_result;
  CHECK(w_seed_hir0_measure(&input, &measured, &measure_result) ==
        W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output,
                                        &fixture.hir_result,
                                        &fixture.hir_program));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_counts = measured;
  CHECK(fixture.hir_counts.enums == 1u &&
        fixture.hir_counts.enum_cases == 3u &&
        fixture.hir_counts.switch_edges == 3u &&
        fixture.hir_program.switch_edge_count == 3u);
  const w_seed_hir0_program *program = &fixture.hir_program;
  size_t dispatch = SIZE_MAX;
  for (size_t block = 0u; block < program->block_count; block += 1u)
    if (program->terminators[block].kind ==
        W_SEED_HIR0_TERMINATOR_SWITCH_ENUM) {
      CHECK(dispatch == SIZE_MAX);
      dispatch = block;
    }
  CHECK(dispatch != SIZE_MAX);
  const w_seed_hir0_terminator *term = &program->terminators[dispatch];
  CHECK(term->value_index != W_SEED_HIR0_NONE &&
        term->switch_enum_index == 0u && term->first_switch_edge == 0u &&
        term->switch_edge_count == 3u && term->switch_carrier_width == 2u);
  for (size_t ordinal = 0u; ordinal < 3u; ordinal += 1u) {
    const w_seed_hir0_switch_edge *edge =
        &program->switch_edges[ordinal];
    CHECK(edge->owner_terminator == dispatch && edge->ordinal == ordinal &&
          edge->enum_index == 0u && edge->enum_case_index == ordinal &&
          edge->target_block == dispatch + 1u + ordinal &&
          program->terminators[edge->target_block].kind ==
              W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
          program->terminators[edge->target_block].value_index !=
              W_SEED_HIR0_NONE);
  }
  const w_seed_hir0_terminator saved_dispatch =
      fixture.hir_terminators[dispatch];
  const w_seed_hir0_switch_edge saved_edges[3] = {
      fixture.hir_switch_edges[0], fixture.hir_switch_edges[1],
      fixture.hir_switch_edges[2]};
  const w_seed_hir0_terminator saved_arms[3] = {
      fixture.hir_terminators[dispatch + 1u],
      fixture.hir_terminators[dispatch + 2u],
      fixture.hir_terminators[dispatch + 3u]};

  fixture.hir_terminators[dispatch].value_index = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[dispatch] = saved_dispatch;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_terminators[dispatch].switch_carrier_width = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[dispatch] = saved_dispatch;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_terminators[dispatch].switch_edge_count = 2u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[dispatch] = saved_dispatch;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_switch_edges[1].ordinal = 0u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_switch_edges[1] = saved_edges[1];
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_switch_edges[1].enum_index = UINT32_MAX;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_switch_edges[1] = saved_edges[1];
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_switch_edges[1].enum_case_index = 0u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_switch_edges[1] = saved_edges[1];
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_switch_edges[1].target_block = (uint32_t)dispatch;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_switch_edges[1] = saved_edges[1];
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_switch_edges[1].target_block =
      (uint32_t)program->functions[1].first_block;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_switch_edges[1] = saved_edges[1];
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_terminators[dispatch + 2u].value_index =
      W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[dispatch + 2u] = saved_arms[1];
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  setup_hir_output();
  fill_hir_output(0xa5u);
  fixture.hir_output.switch_edge_capacity = measured.switch_edges - 1u;
  w_seed_hir0_result rejected;
  (void)memset(&rejected, 0x42, sizeof(rejected));
  const w_seed_hir0_result rejected_before = rejected;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(0xa5u) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);
  return true;
}

static bool test_enum_switch_local_calls(void) {
  static const char SOURCE[] =
      "enum Course { starter main dessert }\n"
      "fn ten(): i64 { return 10 }\n"
      "fn thirty(value: i64): i64 { return value }\n"
      "fn price(course: Course): i64 { return switch course { "
      "case .dessert: ten() case .starter: thirty(value: 10) "
      "case .main: 30 } }\n"
      "entry { }\n";
  CHECK(fixture_parse(SOURCE));
  configure_host();
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output, &fixture.result) ==
        W_SEED_FRONTEND_OK);
  setup_hir_output();
  const w_seed_hir0_input input = {&fixture.input, &fixture.output,
                                   &fixture.result};
  w_seed_hir0_counts measured;
  w_seed_hir0_result measure_result;
  CHECK(w_seed_hir0_measure(&input, &measured, &measure_result) ==
        W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output,
                                        &fixture.hir_result,
                                        &fixture.hir_program));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_counts = measured;
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(measured.functions == 4u && measured.blocks == 7u &&
        measured.instructions == 2u && measured.calls == 2u &&
        measured.arguments == 1u && measured.switch_edges == 3u);
  const size_t dispatch = program->functions[2].first_block;
  CHECK(program->terminators[dispatch].kind ==
            W_SEED_HIR0_TERMINATOR_SWITCH_ENUM &&
        program->terminators[dispatch].switch_edge_count == 3u);
  const w_seed_hir0_switch_edge *starter = &program->switch_edges[0];
  const w_seed_hir0_switch_edge *main_course = &program->switch_edges[1];
  const w_seed_hir0_switch_edge *dessert = &program->switch_edges[2];
  CHECK(starter->enum_case_index == 0u && starter->target_block == dispatch + 1u &&
        main_course->enum_case_index == 1u &&
        main_course->target_block == dispatch + 2u &&
        dessert->enum_case_index == 2u &&
        dessert->target_block == dispatch + 3u);
  CHECK(program->blocks[starter->target_block].instruction_count == 1u &&
        program->blocks[dessert->target_block].instruction_count == 1u &&
        program->blocks[main_course->target_block].instruction_count == 0u);
  CHECK(program->calls[0].owner_block == starter->target_block &&
        program->calls[0].argument_count == 1u &&
        program->calls[1].owner_block == dessert->target_block &&
        program->calls[1].argument_count == 0u);
  CHECK(program->values[program->arguments[0].value_index].kind ==
            W_SEED_HIR0_VALUE_CONST_I64);
  CHECK(program->values[program->terminators[starter->target_block].value_index]
                .kind == W_SEED_HIR0_VALUE_CALL_RESULT &&
        program->values[program->terminators[dessert->target_block].value_index]
                .kind == W_SEED_HIR0_VALUE_CALL_RESULT);
  return true;
}

static bool test_enum_switch_cfg_composition_barrier(void) {
  static const char SOURCE[] =
      "enum Course { starter main dessert }\n"
      "fn price(course: Course): i64 { "
      "let seed = if true { 1 } else { 2 } "
      "return switch course { case .starter: 10 case .main: 30 "
      "case .dessert: 20 } }\n"
      "entry { }\n";
  CHECK(fixture_parse(SOURCE));
  configure_host();
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output, &fixture.result) ==
        W_SEED_FRONTEND_OK);
  setup_hir_output();
  const w_seed_hir0_input input = {&fixture.input, &fixture.output,
                                   &fixture.result};
  w_seed_hir0_counts measured;
  w_seed_hir0_result measure_result;
  CHECK(w_seed_hir0_measure(&input, &measured, &measure_result) ==
        W_SEED_HIR0_UNSUPPORTED);
  return true;
}

static bool append_source(char *buffer, size_t capacity, size_t *offset,
                          const char *text) {
  if (buffer == NULL || offset == NULL || text == NULL || *offset > capacity)
    return false;
  const size_t count = strlen(text);
  if (count > capacity - *offset) return false;
  (void)memcpy(buffer + *offset, text, count);
  *offset += count;
  buffer[*offset] = '\0';
  return true;
}

static bool make_nested_if_source(char *buffer, size_t capacity, size_t depth) {
  size_t offset = 0u;
  if (!append_source(buffer, capacity, &offset, "fn main() { ")) return false;
  for (size_t level = 0u; level < depth; level += 1u)
    if (!append_source(buffer, capacity, &offset, "if true { ")) return false;
  if (!append_source(buffer, capacity, &offset,
                     "print(message: \"nested\", suffix: \"\") "))
    return false;
  for (size_t level = 0u; level < depth; level += 1u)
    if (!append_source(buffer, capacity, &offset, "} ")) return false;
  return append_source(buffer, capacity, &offset, "}\nentry(main)\n");
}

static bool make_nested_scalar_if_source(char *buffer, size_t capacity,
                                         size_t depth) {
  size_t offset = 0u;
  if (!append_source(buffer, capacity, &offset,
                     "fn choose(flag: Bool, value: i64): i64 { return "))
    return false;
  for (size_t level = 0u; level < depth; level += 1u)
    if (!append_source(buffer, capacity, &offset, "if flag { ")) return false;
  if (!append_source(buffer, capacity, &offset, "value")) return false;
  for (size_t level = 0u; level < depth; level += 1u)
    if (!append_source(buffer, capacity, &offset, " } else { value }"))
      return false;
  return append_source(buffer, capacity, &offset,
                       " }\nentry(choose)\n");
}

static bool test_nested_if_depth_boundary(void) {
  char source[TEST_SOURCE];
  CHECK(make_nested_if_source(source, sizeof(source) - 1u,
                              W_SEED_HIR0_MAX_NESTING));
  CHECK(fixture_frontend(source));
  setup_hir_output();
  const w_seed_hir0_input input = hir_input();
  w_seed_hir0_counts counts;
  w_seed_hir0_result result;
  CHECK(w_seed_hir0_measure(&input, &counts, &result) == W_SEED_HIR0_OK);
  CHECK(counts.blocks == 1u + W_SEED_HIR0_MAX_NESTING * 3u);

  CHECK(make_nested_if_source(source, sizeof(source) - 1u,
                              W_SEED_HIR0_MAX_NESTING + 1u));
  CHECK(fixture_frontend(source));
  setup_hir_output();
  CHECK(w_seed_hir0_measure(&input, &counts, &result) ==
        W_SEED_HIR0_UNSUPPORTED);
  return true;
}

static bool test_nested_scalar_if_depth_boundary(void) {
  char source[TEST_SOURCE];
  CHECK(make_nested_scalar_if_source(source, sizeof(source) - 1u,
                                     W_SEED_HIR0_MAX_NESTING));
  CHECK(fixture_frontend(source));
  setup_hir_output();
  const w_seed_hir0_input input = hir_input();
  w_seed_hir0_counts counts;
  w_seed_hir0_result result;
  CHECK(w_seed_hir0_measure(&input, &counts, &result) == W_SEED_HIR0_OK);
  CHECK(counts.blocks == 1u + W_SEED_HIR0_MAX_NESTING * 3u);

  CHECK(make_nested_scalar_if_source(source, sizeof(source) - 1u,
                                     W_SEED_HIR0_MAX_NESTING + 1u));
  CHECK(fixture_frontend(source));
  setup_hir_output();
  fill_hir_output(0xa5u);
  (void)memset(&result, 0x42, sizeof(result));
  const w_seed_hir0_result snapshot = result;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &result) ==
        W_SEED_HIR0_UNSUPPORTED);
  CHECK(hir_output_is_byte(0xa5u));
  CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
  return true;
}

static bool test_logical_and_diamond_positive(void) {
  static const char SOURCE[] =
      "fn rhs(): Bool { return true }\n"
      "fn allowed(left: Bool): Bool { return left && rhs() }\n"
      "entry(allowed)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 2u && program->block_count == 5u &&
        program->block_argument_count == 1u && program->call_count == 1u &&
        program->value_count == 5u);
  const w_seed_hir0_terminator *branch = &program->terminators[1];
  CHECK(branch->kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        branch->logical_operator == W_SEED_HIR0_LOGICAL_AND &&
        branch->target_block == 2u && branch->else_block == 3u &&
        edge_value_for(program, branch) == W_SEED_HIR0_NONE);
  CHECK(program->blocks[4].block_argument_count == 1u &&
        program->blocks[4].first_block_argument == 0u &&
        program->block_arguments[0].owner_block == 4u &&
        program->block_arguments[0].ordinal == 0u &&
        program->block_arguments[0].type_index == W_SEED_HIR0_TYPE_BOOL);
  CHECK(program->terminators[2].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[2].target_block == 4u &&
        edge_value_at(program, 2u) == 2u &&
        program->values[2].kind == W_SEED_HIR0_VALUE_CALL_RESULT &&
        program->values[2].type_index == W_SEED_HIR0_TYPE_BOOL &&
        program->calls[0].owner_block == 2u);
  CHECK(program->terminators[3].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[3].target_block == 4u &&
        edge_value_at(program, 3u) == 3u &&
        program->values[3].kind == W_SEED_HIR0_VALUE_CONST_BOOL &&
        !program->values[3].bool_value);
  CHECK(program->values[4].kind == W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        program->values[4].block_argument_index == 0u &&
        program->terminators[4].value_index == 4u);
  return true;
}

static bool test_logical_unary_not_positive(void) {
  static const char SOURCE[] =
      "fn allowed(left: Bool): Bool { return !left }\n"
      "entry(allowed)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 1u && program->block_count == 1u &&
        program->block_argument_count == 0u && program->value_count == 2u);
  const w_seed_hir0_terminator *return_term = &program->terminators[0];
  CHECK(return_term->kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        return_term->value_index == 1u);
  CHECK(program->values[0].kind == W_SEED_HIR0_VALUE_PARAMETER_READ &&
        program->values[0].type_index == W_SEED_HIR0_TYPE_BOOL &&
        program->values[0].owner_kind == W_SEED_HIR0_VALUE_OWNER_UNARY &&
        program->values[0].owner_index == 1u &&
        program->values[1].kind == W_SEED_HIR0_VALUE_UNARY_BOOL &&
        program->values[1].unary_operator == W_SEED_HIR0_UNARY_NOT &&
        program->values[1].type_index == W_SEED_HIR0_TYPE_BOOL &&
        program->values[1].left_value == 0u);
  return true;
}

static bool test_i64_unary_negate_positive(void) {
  static const char SOURCE[] =
      "fn negate(value: i64): i64 { return -value }\n"
      "entry(negate)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 1u && program->block_count == 1u &&
        program->value_count == 2u &&
        program->terminators[0].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        program->terminators[0].value_index == 1u);
  CHECK(program->values[0].kind == W_SEED_HIR0_VALUE_PARAMETER_READ &&
        program->values[0].type_index == W_SEED_HIR0_TYPE_I64 &&
        program->values[0].owner_kind == W_SEED_HIR0_VALUE_OWNER_UNARY &&
        program->values[0].owner_index == 1u &&
        program->values[1].kind == W_SEED_HIR0_VALUE_UNARY_I64 &&
        program->values[1].unary_operator == W_SEED_HIR0_UNARY_NEGATE &&
        program->values[1].type_index == W_SEED_HIR0_TYPE_I64 &&
        program->values[1].left_value == 0u);

  const w_seed_hir0_value saved = fixture.hir_values[1];
  fixture.hir_values[1].unary_operator = W_SEED_HIR0_UNARY_NOT;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[1] = saved;
  fixture.hir_values[1].type_index = W_SEED_HIR0_TYPE_BOOL;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[1] = saved;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_direct_i64_unary_interpolation(void) {
  static const char SOURCE[] =
      "entry { print(message: \"Balance ${-7}\", suffix: \"\") }\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  uint32_t literal = W_SEED_HIR0_NONE;
  uint32_t unary = W_SEED_HIR0_NONE;
  uint32_t interpolation = W_SEED_HIR0_NONE;
  for (size_t index = 0u; index < program->value_count; index += 1u) {
    if (program->values[index].kind == W_SEED_HIR0_VALUE_CONST_I64)
      literal = (uint32_t)index;
    else if (program->values[index].kind == W_SEED_HIR0_VALUE_UNARY_I64)
      unary = (uint32_t)index;
    else if (program->values[index].kind ==
             W_SEED_HIR0_VALUE_INTERPOLATED_STRING)
      interpolation = (uint32_t)index;
  }
  CHECK(literal != W_SEED_HIR0_NONE && unary != W_SEED_HIR0_NONE &&
        interpolation != W_SEED_HIR0_NONE &&
        program->values[literal].type_index == 2u &&
        program->values[literal].integer_value == 7 &&
        program->values[unary].type_index == 2u &&
        program->values[unary].unary_operator == W_SEED_HIR0_UNARY_NEGATE &&
        program->values[unary].left_value == literal &&
        program->values[interpolation].first_interpolation_segment == 0u &&
        program->values[interpolation].interpolation_segment_count == 2u &&
        program->interpolation_segments[1].kind ==
            W_SEED_HIR0_INTERPOLATION_VALUE &&
        program->interpolation_segments[1].value_index == unary &&
        w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_logical_or_diamond_positive(void) {
  static const char SOURCE[] =
      "fn rhs(): Bool { return true }\n"
      "fn allowed(left: Bool): Bool { return left || rhs() }\n"
      "entry(allowed)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 2u && program->block_count == 5u &&
        program->block_argument_count == 1u && program->call_count == 1u);
  const w_seed_hir0_terminator *branch = &program->terminators[1];
  const w_seed_hir0_terminator *skip = &program->terminators[2];
  const w_seed_hir0_terminator *rhs = &program->terminators[3];
  CHECK(branch->kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        branch->logical_operator == W_SEED_HIR0_LOGICAL_OR &&
        branch->target_block == 2u && branch->else_block == 3u &&
        skip->kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        skip->target_block == 4u &&
        rhs->kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        rhs->target_block == 4u &&
        edge_value_for(program, skip) != W_SEED_HIR0_NONE &&
        edge_value_for(program, rhs) != W_SEED_HIR0_NONE);
  CHECK(program->values[edge_value_for(program, skip)].kind ==
            W_SEED_HIR0_VALUE_CONST_BOOL &&
        program->values[edge_value_for(program, skip)].bool_value &&
        program->values[edge_value_for(program, rhs)].kind ==
            W_SEED_HIR0_VALUE_CALL_RESULT &&
        program->values[edge_value_for(program, rhs)].type_index ==
            W_SEED_HIR0_TYPE_BOOL &&
        program->calls[0].owner_block == 3u);
  CHECK(program->blocks[4].block_argument_count == 1u &&
        program->block_arguments[0].owner_block == 4u &&
        program->terminators[4].value_index != W_SEED_HIR0_NONE &&
        program->values[program->terminators[4].value_index].kind ==
            W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ);
  return true;
}

static bool test_nested_logical_positive(void) {
  static const char SOURCE[] =
      "fn rhs(): Bool { return true }\n"
      "fn allowed(left: Bool): Bool { return left && (false || rhs()) }\n"
      "entry(allowed)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 2u && program->block_count == 8u &&
        program->block_argument_count == 2u && program->call_count == 1u);
  CHECK(program->terminators[1].logical_operator ==
            W_SEED_HIR0_LOGICAL_AND &&
        program->terminators[1].target_block == 2u &&
        program->terminators[1].else_block == 6u &&
        program->terminators[2].logical_operator == W_SEED_HIR0_LOGICAL_OR &&
        program->terminators[2].target_block == 3u &&
        program->terminators[2].else_block == 4u);
  CHECK(program->terminators[3].target_block == 5u &&
        edge_value_at(program, 3u) != W_SEED_HIR0_NONE &&
        program->values[edge_value_at(program, 3u)].kind ==
            W_SEED_HIR0_VALUE_CONST_BOOL &&
        program->values[edge_value_at(program, 3u)].bool_value &&
        program->terminators[4].target_block == 5u &&
        edge_value_at(program, 4u) != W_SEED_HIR0_NONE &&
        program->values[edge_value_at(program, 4u)].kind ==
            W_SEED_HIR0_VALUE_CALL_RESULT);
  CHECK(program->terminators[5].target_block == 7u &&
        edge_value_at(program, 5u) != W_SEED_HIR0_NONE &&
        program->values[edge_value_at(program, 5u)].kind ==
            W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        program->terminators[6].target_block == 7u &&
        edge_value_at(program, 6u) != W_SEED_HIR0_NONE &&
        !program->values[edge_value_at(program, 6u)].bool_value &&
        program->blocks[5].block_argument_count == 1u &&
        program->blocks[7].block_argument_count == 1u &&
        program->block_arguments[0].owner_block == 5u &&
        program->block_arguments[1].owner_block == 7u);
  return true;
}

static bool test_logical_rhs_call_argument_positive(void) {
  static const char SOURCE[] =
      "fn rhs(flag: Bool): Bool { return flag }\n"
      "fn allowed(): Bool { return false || rhs(flag: true) }\n"
      "entry(allowed)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 2u && program->block_count == 5u &&
        program->block_argument_count == 1u && program->call_count == 1u &&
        program->argument_count == 1u);
  const w_seed_hir0_terminator *branch = &program->terminators[1];
  const w_seed_hir0_terminator *skip = &program->terminators[2];
  const w_seed_hir0_terminator *rhs = &program->terminators[3];
  CHECK(branch->logical_operator == W_SEED_HIR0_LOGICAL_OR &&
        branch->target_block == 2u && branch->else_block == 3u &&
        edge_value_for(program, skip) != W_SEED_HIR0_NONE &&
        program->values[edge_value_for(program, skip)].kind ==
            W_SEED_HIR0_VALUE_CONST_BOOL &&
        program->values[edge_value_for(program, skip)].bool_value &&
        edge_value_for(program, rhs) != W_SEED_HIR0_NONE &&
        program->values[edge_value_for(program, rhs)].kind ==
            W_SEED_HIR0_VALUE_CALL_RESULT &&
        program->calls[0].owner_block == 3u &&
        program->calls[0].argument_count == 1u);
  const w_seed_hir0_argument *argument = &program->arguments[0];
  CHECK(argument->owner_call == 0u && argument->ordinal == 0u &&
        argument->type_index == W_SEED_HIR0_TYPE_BOOL &&
        program->values[argument->value_index].kind ==
            W_SEED_HIR0_VALUE_CONST_BOOL &&
        program->values[argument->value_index].bool_value);
  CHECK(program->blocks[4].block_argument_count == 1u &&
        program->terminators[4].value_index != W_SEED_HIR0_NONE &&
        program->values[program->terminators[4].value_index].kind ==
            W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ);
  return true;
}

static bool test_logical_adversarial_barriers(void) {
  static const char LOGICAL_SOURCE[] =
      "fn rhs(): Bool { return true }\n"
      "fn allowed(left: Bool): Bool { return !((left && rhs()) || rhs()) }\n"
      "entry(allowed)\n";
  CHECK(lower(LOGICAL_SOURCE));
  w_seed_hir0_program *program = &fixture.hir_program;
  w_seed_hir0_result *result = &fixture.hir_result;
  CHECK(program->function_count == 2u && program->block_count == 8u &&
        program->block_argument_count == 2u && program->value_count == 9u);
  const size_t inner_branch = 1u;
  const size_t inner_rhs_jump = 2u;
  const size_t inner_skip_jump = 3u;
  const size_t inner_join = 4u;
  const size_t outer_branch = 4u;
  const size_t outer_join = 7u;
  size_t unary = SIZE_MAX;
  size_t inner_read = SIZE_MAX;
  size_t outer_read = SIZE_MAX;
  for (size_t index = 0u; index < program->value_count; index += 1u) {
    if (program->values[index].kind == W_SEED_HIR0_VALUE_UNARY_BOOL)
      unary = index;
    if (program->values[index].kind ==
        W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ) {
      if (program->values[index].block_argument_index == 0u)
        inner_read = index;
      if (program->values[index].block_argument_index == 1u)
        outer_read = index;
    }
  }
  CHECK(unary != SIZE_MAX && inner_read != SIZE_MAX &&
        outer_read != SIZE_MAX &&
        program->terminators[inner_branch].logical_operator ==
            W_SEED_HIR0_LOGICAL_AND &&
        program->terminators[outer_branch].logical_operator ==
            W_SEED_HIR0_LOGICAL_OR &&
        program->blocks[inner_join].block_argument_count == 1u &&
        program->blocks[outer_join].block_argument_count == 1u);
  const w_seed_hir0_value saved_unary = fixture.hir_values[unary];
  const w_seed_hir0_value saved_inner_read = fixture.hir_values[inner_read];
  const w_seed_hir0_value saved_outer_read = fixture.hir_values[outer_read];
  const w_seed_hir0_value saved_inner_incoming =
      fixture.hir_values[edge_value_at(program, inner_rhs_jump)];
  const w_seed_hir0_value saved_inner_skip =
      fixture.hir_values[edge_value_at(program, inner_skip_jump)];
  const w_seed_hir0_terminator saved_inner_branch =
      fixture.hir_terminators[inner_branch];
  const w_seed_hir0_terminator saved_inner_rhs =
      fixture.hir_terminators[inner_rhs_jump];
  const uint32_t inner_edge_index =
      fixture.hir_terminators[inner_rhs_jump].first_edge_argument;
  const w_seed_hir0_edge_argument saved_inner_edge =
      fixture.hir_edge_arguments[inner_edge_index];
  const w_seed_hir0_block saved_inner_join = fixture.hir_blocks[inner_join];
  const w_seed_hir0_block_argument saved_inner_argument =
      fixture.hir_block_arguments[0];

  fixture.hir_values[unary].unary_operator =
      (w_seed_hir0_unary_operator)(W_SEED_HIR0_UNARY_NOT + 1);
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_values[unary] = saved_unary;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_values[unary].type_index = W_SEED_HIR0_TYPE_I64;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_values[unary] = saved_unary;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_terminators[inner_branch].logical_operator =
      W_SEED_HIR0_LOGICAL_OR;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_terminators[inner_branch] = saved_inner_branch;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_terminators[inner_branch].target_block = (uint32_t)inner_skip_jump;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_terminators[inner_branch] = saved_inner_branch;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_terminators[inner_rhs_jump].target_block = (uint32_t)outer_join;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_terminators[inner_rhs_jump] = saved_inner_rhs;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_edge_arguments[inner_edge_index].owner_terminator =
      (uint32_t)inner_skip_jump;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_edge_arguments[inner_edge_index] = saved_inner_edge;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_edge_arguments[inner_edge_index].owner_block =
      (uint32_t)outer_join;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_edge_arguments[inner_edge_index] = saved_inner_edge;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_edge_arguments[inner_edge_index].ordinal = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_edge_arguments[inner_edge_index] = saved_inner_edge;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_edge_arguments[inner_edge_index].type_index =
      W_SEED_HIR0_TYPE_I64;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_edge_arguments[inner_edge_index] = saved_inner_edge;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_edge_arguments[inner_edge_index].value_index =
      W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_edge_arguments[inner_edge_index] = saved_inner_edge;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_terminators[inner_rhs_jump].first_edge_argument =
      (uint32_t)program->edge_argument_count;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_terminators[inner_rhs_jump] = saved_inner_rhs;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_terminators[inner_rhs_jump].edge_argument_count = 2u;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_terminators[inner_rhs_jump] = saved_inner_rhs;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_terminators[inner_rhs_jump].first_edge_argument =
      W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_terminators[inner_rhs_jump] = saved_inner_rhs;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_terminators[inner_rhs_jump].edge_argument_count = 0u;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_terminators[inner_rhs_jump] = saved_inner_rhs;
  CHECK(w_seed_hir0_verify(program, result));

  const uint32_t inner_incoming_index = edge_value_at(program, inner_rhs_jump);
  FIXTURE_EDGE_VALUE_SLOT(inner_rhs_jump) =
      W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, result));
  FIXTURE_EDGE_VALUE_SLOT(inner_rhs_jump) =
      inner_incoming_index;
  fixture.hir_terminators[inner_rhs_jump] = saved_inner_rhs;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_values[inner_incoming_index].owner_index = (uint32_t)inner_skip_jump;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_values[inner_incoming_index] = saved_inner_incoming;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_values[inner_incoming_index].owner_ordinal = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_values[inner_incoming_index] = saved_inner_incoming;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_values[inner_incoming_index].type_index =
      W_SEED_HIR0_TYPE_I64;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_values[inner_incoming_index] = saved_inner_incoming;
  CHECK(w_seed_hir0_verify(program, result));

  const uint32_t inner_skip_index =
      edge_value_at(program, inner_skip_jump);
  fixture.hir_values[inner_skip_index].bool_value = true;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_values[inner_skip_index] = saved_inner_skip;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_block_arguments[0].owner_block = (uint32_t)outer_join;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_block_arguments[0] = saved_inner_argument;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_block_arguments[0].type_index = W_SEED_HIR0_TYPE_I64;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_block_arguments[0] = saved_inner_argument;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_blocks[inner_join].first_block_argument =
      (uint32_t)program->block_argument_count;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_blocks[inner_join] = saved_inner_join;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_values[outer_read].block_argument_index = 0u;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_values[outer_read] = saved_outer_read;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_values[outer_read].block_argument_index =
      (uint32_t)program->block_argument_count;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_values[outer_read] = saved_outer_read;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_values[outer_read].block_argument_index = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_values[outer_read] = saved_outer_read;
  CHECK(w_seed_hir0_verify(program, result));
  fixture.hir_values[inner_read] = saved_inner_read;

  const w_seed_hir0_input input = hir_input();
  setup_hir_output();
  fill_hir_output(0xa5u);
  w_seed_hir0_result rejected;
  (void)memset(&rejected, 0x42, sizeof(rejected));
  const w_seed_hir0_result rejected_before = rejected;
  fixture.hir_output.block_argument_capacity =
      fixture.hir_counts.block_arguments - 1u;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(0xa5u) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  setup_hir_output();
  fill_hir_output(0xa5u);
  fixture.hir_output.edge_argument_capacity =
      fixture.hir_counts.edge_arguments - 1u;
  (void)memset(&rejected, 0x42, sizeof(rejected));
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(0xa5u) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  setup_hir_output();
  fill_hir_output(0xa5u);
  w_seed_hir0_output alias = fixture.hir_output;
  alias.block_arguments =
      (w_seed_hir0_block_argument *)(void *)alias.blocks;
  (void)memset(&rejected, 0x42, sizeof(rejected));
  CHECK(w_seed_hir0_run(&input, &alias, &rejected) == W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  setup_hir_output();
  fill_hir_output(0xa5u);
  alias = fixture.hir_output;
  alias.edge_arguments =
      (w_seed_hir0_edge_argument *)(void *)alias.blocks;
  (void)memset(&rejected, 0x42, sizeof(rejected));
  CHECK(w_seed_hir0_run(&input, &alias, &rejected) == W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  static const char NORMAL_SOURCE[] =
      "fn main() { if true { print(message: \"yes\", suffix: \"\") } "
      "else { print(message: \"no\", suffix: \"\") } }\n"
      "entry(main)\n";
  CHECK(lower(NORMAL_SOURCE));
  program = &fixture.hir_program;
  result = &fixture.hir_result;
  CHECK(program->block_argument_count == 0u &&
        program->terminators[0].logical_operator ==
            W_SEED_HIR0_LOGICAL_NONE &&
        edge_value_at(program, 1u) == W_SEED_HIR0_NONE);
  const w_seed_hir0_terminator saved_normal_branch =
      fixture.hir_terminators[0];
  const w_seed_hir0_terminator saved_normal_jump = fixture.hir_terminators[1];
  fixture.hir_terminators[0].logical_operator = W_SEED_HIR0_LOGICAL_AND;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_terminators[0] = saved_normal_branch;
  CHECK(w_seed_hir0_verify(program, result));
  fixture.hir_terminators[1].first_edge_argument = 0u;
  fixture.hir_terminators[1].edge_argument_count = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_terminators[1] = saved_normal_jump;
  CHECK(w_seed_hir0_verify(program, result));
  return true;
}

static bool test_signed_comparison_values(void) {
  static const char *const operators[] = {"==", "!=", "<", "<=", ">", ">="};
  static const w_seed_hir0_binary_operator opcodes[] = {
      W_SEED_HIR0_BINARY_EQUAL, W_SEED_HIR0_BINARY_NOT_EQUAL,
      W_SEED_HIR0_BINARY_LESS, W_SEED_HIR0_BINARY_LESS_EQUAL,
      W_SEED_HIR0_BINARY_GREATER, W_SEED_HIR0_BINARY_GREATER_EQUAL};
  for (size_t operation = 0u; operation < 6u; operation += 1u) {
    char source[1024];
    const int written = snprintf(source, sizeof(source),
        "fn fits(left: i64, right: i64): Bool { return left %s right }\n"
        "fn main() { let fits = fits(left: 0 - 9223372036854775807 - 1, "
        "right: 9223372036854775807) "
        "print(message: \"${fits}\", suffix: \"\") }\nentry(main)\n",
        operators[operation]);
    CHECK(written > 0 && (size_t)written < sizeof(source));
    CHECK(lower(source));
    size_t comparison = SIZE_MAX;
    for (size_t index = 0u; index < fixture.hir_program.value_count; index += 1u)
      if (fixture.hir_values[index].kind == W_SEED_HIR0_VALUE_BINARY_I64 &&
          fixture.hir_values[index].binary_operator == opcodes[operation])
        comparison = index;
    CHECK(comparison != SIZE_MAX);
    const w_seed_hir0_value saved = fixture.hir_values[comparison];
    CHECK(saved.type_index == 3u &&
          saved.owner_kind == W_SEED_HIR0_VALUE_OWNER_TERMINATOR &&
          fixture.hir_values[saved.left_value].type_index == 2u &&
          fixture.hir_values[saved.right_value].type_index == 2u &&
          fixture.hir_values[saved.left_value].kind ==
              W_SEED_HIR0_VALUE_PARAMETER_READ &&
          fixture.hir_values[saved.right_value].kind ==
              W_SEED_HIR0_VALUE_PARAMETER_READ);
    /* Original digests remain unchanged: these exercise the combined verifier. */
    for (size_t mutation = 0u; mutation < 7u; mutation += 1u) {
      const w_seed_hir0_value saved_left = fixture.hir_values[saved.left_value];
      switch (mutation) {
        case 0u: fixture.hir_values[comparison].type_index = 2u; break;
        case 1u: fixture.hir_values[saved.left_value].type_index = 3u; break;
        case 2u:
          fixture.hir_values[comparison].binary_operator =
              (w_seed_hir0_binary_operator)UINT32_MAX;
          break;
        case 3u:
          fixture.hir_values[comparison].binary_operator =
              (w_seed_hir0_binary_operator)(W_SEED_HIR0_BINARY_GREATER_EQUAL + 1);
          break;
        case 4u: fixture.hir_values[comparison].left_value = (uint32_t)comparison; break;
        case 5u: fixture.hir_values[saved.left_value].owner_index = 0u; break;
        default: fixture.hir_values[comparison].right_value = saved.left_value; break;
      }
      CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
      fixture.hir_values[comparison] = saved;
      fixture.hir_values[saved.left_value] = saved_left;
      CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
    }
  }

  const w_seed_hir0_input input = hir_input();
  setup_hir_output();
  fill_hir_output(0xa5u);
  fixture.hir_output.value_capacity = 0u;
  w_seed_hir0_result rejected;
  (void)memset(&rejected, 0x42, sizeof(rejected));
  const w_seed_hir0_result snapshot = rejected;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(0xa5u));
  CHECK(memcmp(&rejected, &snapshot, sizeof(rejected)) == 0);
  return true;
}

static bool test_short_entry_hir(void) {
  static const char SOURCE[] =
      "entry { print(message: \"Hello, world!\", suffix: \"!\") }\n";
  static const char COMMENTED[] =
      "// trivia\nentry {   print(message: \"Hello, world!\", suffix: \"!\")   }\n";
  CHECK(lower(SOURCE));
  CHECK(fixture.hir_program.function_count == 1u &&
        fixture.hir_program.entry_count == 1u);
  const w_seed_hir0_function function = fixture.hir_functions[0];
  const w_seed_hir0_entry entry = fixture.hir_entries[0];
  CHECK(function.is_anonymous_entry && function.parameter_count == 0u &&
        function.return_type == 0u && entry.is_body &&
        entry.target_function == 0u && entry.target_identity == 1u);
  CHECK(function.name.count == strlen("<entry.default>") &&
        entry.target_name.count == function.name.count &&
        memcmp(fixture.hir_text + function.name.offset, "<entry.default>",
               function.name.count) == 0 &&
        memcmp(fixture.hir_text + entry.target_name.offset,
               fixture.hir_text + function.name.offset, function.name.count) ==
            0);
  uint8_t semantic[32];
  uint8_t provenance[32];
  (void)memcpy(semantic, fixture.hir_result.semantic_digest, sizeof(semantic));
  (void)memcpy(provenance, fixture.hir_result.provenance_digest,
               sizeof(provenance));

  fixture.hir_functions[0].is_anonymous_entry = false;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_functions[0] = function;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_entries[0].is_body = false;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_entries[0] = entry;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));

  CHECK(lower(COMMENTED));
  CHECK(memcmp(semantic, fixture.hir_result.semantic_digest, sizeof(semantic)) ==
        0);
  CHECK(memcmp(provenance, fixture.hir_result.provenance_digest,
               sizeof(provenance)) != 0);
  return true;
}

static bool test_function_export_facts(void) {
  static const char SOURCE[] =
      "export fn public() { }\n"
      "fn private() { }\n"
      "entry { }\n";
  CHECK(lower(SOURCE));
  CHECK(fixture.hir_program.function_count == 3u &&
        fixture.hir_program.entry_count == 1u &&
        fixture.hir_program.functions[0].exported &&
        !fixture.hir_program.functions[1].exported &&
        !fixture.hir_program.functions[2].exported &&
        !fixture.hir_program.functions[0].is_anonymous_entry &&
        !fixture.hir_program.functions[1].is_anonymous_entry &&
        fixture.hir_program.functions[2].is_anonymous_entry);

  const w_seed_hir0_function saved_public = fixture.hir_functions[0];
  fixture.hir_functions[0].exported = false;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_functions[0] = saved_public;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));

  const w_seed_hir0_function saved_private = fixture.hir_functions[1];
  fixture.hir_functions[1].exported = true;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_functions[1] = saved_private;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));

  const w_seed_hir0_function saved_entry = fixture.hir_functions[2];
  fixture.hir_functions[2].exported = true;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_functions[2] = saved_entry;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

int main(void) {
  if (!test_frontend_inferred_call_interpolation()) return 1;
  if (!test_process_hir()) return 1;
  if (!test_process_input0_hir()) return 1;
  if (!test_process_hir_adversarial()) return 1;
  if (!test_direct_entry_facts()) return 1;
  if (!test_direct_entry_effect_barrier()) return 1;
  if (!test_short_entry_hir()) return 1;
  if (!test_signed_comparison_values()) return 1;
  if (!test_canonical_and_copy_boundary()) return 1;
  if (!test_semantic_and_provenance_digests()) return 1;
  if (!test_function_parameter_records()) return 1;
  if (!test_lowering_is_not_hello_hardcoded()) return 1;
  if (!test_local_binding_lowering()) return 1;
  if (!test_straight_line_mutation_ssa()) return 1;
  if (!test_interleaved_mutation_versions()) return 1;
  if (!test_conditional_mutation_merge()) return 1;
  if (!test_bool_mutation_ssa()) return 1;
  if (!test_branch_local_mutation_merge()) return 1;
  if (!test_multi_branch_mutation_merge()) return 1;
  if (!test_while_mutation_ssa()) return 1;
  if (!test_while_mutation_barriers()) return 1;
  if (!test_branch_local_mutation_barriers()) return 1;
  if (!test_bindings_across_functions()) return 1;
  if (!test_local_binding_verify_mutations()) return 1;
  if (!test_local_enum_hir()) return 1;
  if (!test_local_enum_payload_declarations_hir()) return 1;
  if (!test_enum_switch_hir()) return 1;
  if (!test_enum_switch_local_calls()) return 1;
  if (!test_enum_switch_cfg_composition_barrier()) return 1;
  if (!test_capacity_and_alias_barriers()) return 1;
  if (!test_verify_mutations()) return 1;
  if (!test_closed_frontend_barriers()) return 1;
  if (!test_typed_interpolation_value_tree()) return 1;
  if (!test_builtin_display_value_tree()) return 1;
  if (!test_typed_immutable_binding_values()) return 1;
  if (!test_local_unit_call_and_parameter_reads()) return 1;
  if (!test_scalar_return_and_call_result()) return 1;
  if (!test_scalar_if_value_diamond()) return 1;
  if (!test_nested_scalar_if_value_diamond()) return 1;
  if (!test_if_diamond_cfg()) return 1;
  if (!test_if_without_else_cfg()) return 1;
  if (!test_sequential_if_diamonds()) return 1;
  if (!test_nested_if_diamonds()) return 1;
  if (!test_nested_if_depth_boundary()) return 1;
  if (!test_nested_scalar_if_depth_boundary()) return 1;
  if (!test_logical_and_diamond_positive()) return 1;
  if (!test_logical_unary_not_positive()) return 1;
  if (!test_i64_unary_negate_positive()) return 1;
  if (!test_direct_i64_unary_interpolation()) return 1;
  if (!test_logical_or_diamond_positive()) return 1;
  if (!test_nested_logical_positive()) return 1;
  if (!test_logical_rhs_call_argument_positive()) return 1;
  if (!test_logical_adversarial_barriers()) return 1;
  if (!test_function_export_facts()) return 1;
  (void)puts("hir0 tests: ok");
  return 0;
}
