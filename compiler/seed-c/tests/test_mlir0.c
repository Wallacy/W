#include "w_seed_mlir0.h"

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
  TEST_STATEMENTS = 32,
  TEST_EXPRESSIONS = 64,
  TEST_ARGUMENTS = 32,
  TEST_SYMBOLS = 32,
  TEST_FACTS = 16,
  TEST_DIAGNOSTICS = 8,
  TEST_RECEIPT = 65536,
  TEST_HIR_IDENTITIES = 32,
  TEST_HIR_RECORDS = 32,
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
  w_seed_hir0_instruction hir_instructions[TEST_HIR_RECORDS];
  w_seed_hir0_binding hir_bindings[TEST_HIR_RECORDS];
  w_seed_hir0_call hir_calls[TEST_HIR_RECORDS];
  w_seed_hir0_host_parameter hir_host_parameters[TEST_HIR_RECORDS];
  w_seed_hir0_argument hir_arguments[TEST_HIR_RECORDS];
  w_seed_hir0_requirement hir_requirements[TEST_HIR_RECORDS];
  w_seed_hir0_value hir_values[TEST_HIR_RECORDS];
  w_seed_hir0_terminator hir_terminators[TEST_HIR_RECORDS];
  w_seed_hir0_entry hir_entries[TEST_HIR_RECORDS];
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
  return w_seed_frontend_run(&fixture.input, &fixture.output,
                             &fixture.frontend_result) == W_SEED_FRONTEND_OK;
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
      .terminators = fixture.hir_terminators,
      .terminator_capacity = TEST_HIR_RECORDS,
      .entries = fixture.hir_entries,
      .entry_capacity = TEST_HIR_RECORDS,
      .text_bytes = fixture.hir_text,
      .text_byte_capacity = sizeof(fixture.hir_text),
      .value_bytes = fixture.hir_value_bytes,
      .value_byte_capacity = sizeof(fixture.hir_value_bytes),
      .receipt = fixture.hir_receipt,
      .receipt_capacity = sizeof(fixture.hir_receipt)};
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

static w_seed_mlir0_input mlir_input(void) {
  return (w_seed_mlir0_input){&fixture.hir_program, &fixture.hir_result};
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
  CHECK(contains_bytes(output, counts.mlir_bytes, "!llvm.array<1 x i8>"));
  CHECK(output[counts.mlir_bytes] == 0x5au);
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
  const w_seed_mlir0_input forged_input = {&fixture.hir_program,
                                           &forged_result};
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

int main(void) {
  if (!test_direct_products()) return 1;
  if (!test_restaurant_and_nul()) return 1;
  if (!test_capacity_and_all_or_nothing()) return 1;
  if (!test_aliases()) return 1;
  if (!test_invalid_hir_and_target()) return 1;
  if (!test_valid_hir_outside_subset()) return 1;
  (void)puts("seed MLIR0: verified HIR0 native subset and LLVM dialect barriers passed");
  return 0;
}
