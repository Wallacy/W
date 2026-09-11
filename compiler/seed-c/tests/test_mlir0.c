#include "w_seed_mlir0.h"

#include "../src/w_seed_native_subset0.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
  TEST_TYPES = 16,
  TEST_FUNCTIONS = 8,
  TEST_PARAMETERS = 8,
  TEST_ENTRIES = 4,
  TEST_STATEMENTS = 64,
  TEST_EXPRESSIONS = 128,
  TEST_ARGUMENTS = 64,
  TEST_SYMBOLS = 64,
  TEST_FACTS = 16,
  TEST_DIAGNOSTICS = 8,
  TEST_RECEIPT = 65536,
  TEST_HIR_IDENTITIES = 32,
  TEST_HIR_RECORDS = 64,
  TEST_HIR_TEXT = 4096,
  TEST_HIR_VALUES = 4096,
  TEST_HIR_RECEIPT = 256,
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
  w_seed_frontend_type_declaration type_declarations[TEST_STRUCTS];
  w_seed_frontend_alias aliases[TEST_STRUCTS];
  w_seed_frontend_type types[TEST_TYPES];
  w_seed_frontend_function functions[TEST_FUNCTIONS];
  w_seed_frontend_parameter parameters[TEST_PARAMETERS];
  w_seed_frontend_entry entries[TEST_ENTRIES];
  w_seed_frontend_statement statements[TEST_STATEMENTS];
  w_seed_frontend_expression expressions[TEST_EXPRESSIONS];
  w_seed_frontend_argument arguments[TEST_ARGUMENTS];
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
  w_seed_hir0_function hir_functions[TEST_HIR_RECORDS];
  w_seed_hir0_parameter hir_parameters[TEST_HIR_RECORDS];
  w_seed_hir0_block hir_blocks[TEST_HIR_RECORDS];
  w_seed_hir0_block_argument hir_block_arguments[TEST_HIR_RECORDS];
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
  w_seed_hir0_program hir_program;
} mlir_fixture;

static mlir_fixture fixture;

static const w_seed_mlir0_target TARGET = {
    W_SEED_MLIR0_TARGET_X86_64_UNKNOWN_LINUX_GNU};
static const w_seed_mlir0_target WINDOWS_TARGET = {
    W_SEED_MLIR0_TARGET_X86_64_PC_WINDOWS_MSVC};

static bool process_frontend_mode;
static void configure_process_external(void);
static bool resolve_process_import(void);
static bool contains_bytes(const uint8_t *bytes, size_t length,
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
    if (!resolve_process_import()) return false;
  }
  return w_seed_frontend_run(&fixture.input, &fixture.output,
                             &fixture.frontend_result) == W_SEED_FRONTEND_OK;
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
      .functions = fixture.hir_functions,
      .function_capacity = TEST_HIR_RECORDS,
      .parameters = fixture.hir_parameters,
      .parameter_capacity = TEST_HIR_RECORDS,
      .blocks = fixture.hir_blocks,
      .block_capacity = TEST_HIR_RECORDS,
      .block_arguments = fixture.hir_block_arguments,
      .block_argument_capacity = TEST_HIR_RECORDS,
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
  const w_seed_hir0_input input = {
      &fixture.input, &fixture.output, &fixture.frontend_result};
  w_seed_hir0_counts counts;
  w_seed_hir0_result result;
  CHECK(w_seed_hir0_measure(&input, &counts, &result) == W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_OK);
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
      .functions = fixture.hir_functions,
      .function_capacity = TEST_HIR_RECORDS,
      .parameters = fixture.hir_parameters,
      .parameter_capacity = TEST_HIR_RECORDS,
      .blocks = fixture.hir_blocks,
      .block_capacity = TEST_HIR_RECORDS,
      .block_arguments = fixture.hir_block_arguments,
      .block_argument_capacity = TEST_HIR_RECORDS,
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
  const w_seed_hir0_input input = {
      &fixture.input, &fixture.output, &fixture.frontend_result};
  w_seed_hir0_counts counts;
  w_seed_hir0_result result;
  CHECK(w_seed_hir0_measure(&input, &counts, &result) == W_SEED_HIR0_OK);
  CHECK(counts.external_modules == 1u && counts.external_symbols == 4u &&
        counts.types == 7u);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output,
                                        &fixture.hir_result,
                                        &fixture.hir_program));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
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

static bool contains_bytes(const uint8_t *bytes, size_t length,
                           const char *needle) {
  if (bytes == NULL || needle == NULL) return false;
  const size_t needle_length = strlen(needle);
  if (needle_length == 0u || needle_length > length) return false;
  for (size_t offset = 0u; offset + needle_length <= length; offset += 1u)
    if (memcmp(bytes + offset, needle, needle_length) == 0) return true;
  return false;
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
  CHECK(contains_bytes(first, counts.mlir_bytes, "@mainCRTStartup"));
  CHECK(contains_bytes(first, counts.mlir_bytes, "@w_seed_mlir0_buffer"));
  CHECK(contains_bytes(first, counts.mlir_bytes, "llvm.mlir.zero"));
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
  CHECK(contains_bytes(second, dynamic_counts.mlir_bytes,
                       "@w_seed_mlir0_buffer"));
  CHECK(contains_bytes(second, dynamic_counts.mlir_bytes, "llvm.mlir.zero"));
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
                       "llvm.call @w_seed_checked_multiply_i64(%v0, %v1) : "
                       "(i64, i64) -> i64"));
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
                       "llvm.call @w_seed_checked_multiply_i64(%v0, %v1) : "
                       "(i64, i64) -> i64"));
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
                        "llvm.func internal @w_seed_checked_divide_i64") &&
        contains_bytes(runtime_division_artifact,
                        runtime_division_result.written.mlir_bytes,
                        "llvm.call @w_seed_checked_divide_i64"));

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
                        "llvm.func internal @w_seed_checked_remainder_i64") &&
        contains_bytes(runtime_remainder_artifact,
                        runtime_remainder_result.written.mlir_bytes,
                        "llvm.call @w_seed_checked_remainder_i64"));

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
                       "llvm.call @w_seed_checked_multiply_i64(%v4, %v5) : "
                       "(i64, i64) -> i64"));
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
                      "llvm.cond_br %overflow, ^checked_overflow, ^checked_ok") ==
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

static bool test_checked_helper_reachability(void) {
  static const uint8_t source[] =
      "fn deadArithmetic(value: i64): i64 { return value + 1 }\n"
      "fn deadDivision(value: i64): i64 { return value / 2 }\n"
      "fn deadRemainder(value: i64): i64 { return value % 2 }\n"
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
  CHECK(program->terminators[2].incoming_value < program->value_count &&
        program->terminators[3].incoming_value < program->value_count &&
        program->terminators[4].incoming_value < program->value_count &&
        program->terminators[5].incoming_value < program->value_count &&
        program->values[program->terminators[2].incoming_value].type_index ==
            W_SEED_HIR0_TYPE_I64 &&
        program->values[program->terminators[3].incoming_value].type_index ==
            W_SEED_HIR0_TYPE_I64 &&
        program->values[program->terminators[4].incoming_value].type_index ==
            W_SEED_HIR0_TYPE_I64 &&
        program->values[program->terminators[5].incoming_value].type_index ==
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
        rhs_jump->incoming_value < program->value_count &&
        skip_jump->incoming_value < program->value_count);
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
      &program->values[rhs_jump->incoming_value];
  const w_seed_hir0_value *skip_incoming =
      &program->values[skip_jump->incoming_value];
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
      function_index, join_block_index, skip_jump->incoming_value);
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
        skip_jump->incoming_value < program->value_count &&
        rhs_jump->incoming_value < program->value_count);
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
      &program->values[skip_jump->incoming_value];
  const w_seed_hir0_value *rhs_incoming =
      &program->values[rhs_jump->incoming_value];
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
      function_index, skip_jump->target_block, skip_jump->incoming_value);
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
        inner_skip_jump->incoming_value < program->value_count &&
        inner_rhs_jump->incoming_value < program->value_count &&
        outer_skip_jump->incoming_value < program->value_count);
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
        outer_rhs_jump->incoming_value < program->value_count);
  const w_seed_hir0_value *inner_skip_incoming =
      &program->values[inner_skip_jump->incoming_value];
  const w_seed_hir0_value *inner_rhs_incoming =
      &program->values[inner_rhs_jump->incoming_value];
  const w_seed_hir0_value *outer_skip_incoming =
      &program->values[outer_skip_jump->incoming_value];
  const w_seed_hir0_value *outer_rhs_incoming =
      &program->values[outer_rhs_jump->incoming_value];
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
                    inner_join_index, inner_skip_jump->incoming_value);
  CHECK(length > 0 && (size_t)length < sizeof(inner_skip_jump_text));
  length = snprintf(inner_rhs_jump_text, sizeof(inner_rhs_jump_text),
                    "llvm.br ^w_fn_%u_b_%u(%%call%u : i1)", function_index,
                    inner_join_index, inner_rhs_incoming->call_index);
  CHECK(length > 0 && (size_t)length < sizeof(inner_rhs_jump_text));
  length = snprintf(outer_skip_jump_text, sizeof(outer_skip_jump_text),
                    "llvm.br ^w_fn_%u_b_%u(%%v%u : i1)", function_index,
                    outer_join_index, outer_skip_jump->incoming_value);
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
        program->terminators[index].incoming_value != W_SEED_HIR0_NONE) {
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
  fixture.hir_terminators[logical_jump_index].incoming_value =
      W_SEED_HIR0_NONE;
  CHECK(expect_logical_mlir_invalid());
  fixture.hir_terminators[logical_jump_index] = saved_jump;
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
      {(void *)fixture.hir_functions, sizeof(fixture.hir_functions)},
      {(void *)fixture.hir_parameters, sizeof(fixture.hir_parameters)},
      {(void *)fixture.hir_blocks, sizeof(fixture.hir_blocks)},
      {(void *)fixture.hir_block_arguments,
       sizeof(fixture.hir_block_arguments)},
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
    (void)memset(output, 0x7eu, sizeof(output));
    (void)memcpy(output_snapshot, output, sizeof(output_snapshot));
    CHECK(w_seed_mlir0_emit(
              &input, &TARGET,
              &(w_seed_mlir0_output){(uint8_t *)ranges[index].address,
                                     ranges[index].bytes},
              &result) == W_SEED_MLIR0_ALIAS);
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
      CHECK(w_seed_mlir0_emit(&input, host == 0u ? &TARGET : &WINDOWS_TARGET,
                &(w_seed_mlir0_output){artifact, sizeof(artifact)}, &result) ==
            W_SEED_MLIR0_OK);
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

int main(void) {
  if (!test_process_hir_is_closed_to_mlir()) return 1;
  if (!test_signed_comparison_artifacts()) return 1;
  if (!test_direct_products()) return 1;
  if (!test_windows_target_runtime_surface()) return 1;
  if (!test_restaurant_and_nul()) return 1;
  if (!test_typed_interpolation_artifact()) return 1;
  if (!test_direct_unit_call()) return 1;
  if (!test_checked_runtime_arithmetic()) return 1;
  if (!test_checked_helper_reachability()) return 1;
  if (!test_scalar_return_call_result()) return 1;
  if (!test_scalar_if_value_diamond()) return 1;
  if (!test_nested_scalar_if_value_diamond()) return 1;
  if (!test_if_diamond_cfg()) return 1;
  if (!test_logical_and_diamond()) return 1;
  if (!test_logical_unary_not()) return 1;
  if (!test_logical_or_diamond()) return 1;
  if (!test_logical_nested_diamond()) return 1;
  if (!test_logical_mlir_adversarial()) return 1;
  if (!test_checked_arithmetic_adversarial()) return 1;
  if (!test_interpolation_semantic_barriers()) return 1;
  if (!test_linear_sequence()) return 1;
  if (!test_capacity_and_all_or_nothing()) return 1;
  if (!test_aliases()) return 1;
  if (!test_invalid_hir_and_target()) return 1;
  if (!test_valid_hir_outside_subset()) return 1;
  (void)puts("seed MLIR0: verified HIR0 native subset and LLVM dialect barriers passed");
  return 0;
}
