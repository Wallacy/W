#include "w_seed_hir0.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
  w_seed_frontend_result result;
  w_seed_hir0_module hir_modules[TEST_HIR_RECORDS];
  w_seed_hir0_identity hir_identities[TEST_HIR_IDENTITIES];
  w_seed_hir0_type hir_types[TEST_HIR_RECORDS];
  w_seed_hir0_function hir_functions[TEST_HIR_RECORDS];
  w_seed_hir0_parameter hir_parameters[TEST_HIR_RECORDS];
  w_seed_hir0_block hir_blocks[TEST_HIR_RECORDS];
  w_seed_hir0_instruction hir_instructions[TEST_HIR_RECORDS];
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
  w_seed_hir0_counts hir_counts;
  w_seed_hir0_program hir_program;
} hir_fixture;

static hir_fixture fixture;

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

static void setup_hir_output(void) {
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

static bool test_canonical_and_copy_boundary(void) {
  CHECK(lower(CANONICAL_SOURCE));
  CHECK(fixture.hir_counts.modules == 1u);
  CHECK(fixture.hir_counts.identities == 5u);
  CHECK(fixture.hir_counts.types == 2u);
  CHECK(fixture.hir_counts.functions == 1u);
  CHECK(fixture.hir_counts.parameters == 0u);
  CHECK(fixture.hir_counts.blocks == 1u);
  CHECK(fixture.hir_counts.instructions == 1u);
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

static w_seed_hir0_input hir_input(void) {
  return (w_seed_hir0_input){&fixture.input, &fixture.output, &fixture.result};
}

static void fill_hir_output(uint8_t value) {
  (void)memset(fixture.hir_modules, value, sizeof(fixture.hir_modules));
  (void)memset(fixture.hir_identities, value, sizeof(fixture.hir_identities));
  (void)memset(fixture.hir_types, value, sizeof(fixture.hir_types));
  (void)memset(fixture.hir_functions, value, sizeof(fixture.hir_functions));
  (void)memset(fixture.hir_parameters, value, sizeof(fixture.hir_parameters));
  (void)memset(fixture.hir_blocks, value, sizeof(fixture.hir_blocks));
  (void)memset(fixture.hir_instructions, value,
               sizeof(fixture.hir_instructions));
  (void)memset(fixture.hir_calls, value, sizeof(fixture.hir_calls));
  (void)memset(fixture.hir_host_parameters, value,
               sizeof(fixture.hir_host_parameters));
  (void)memset(fixture.hir_arguments, value, sizeof(fixture.hir_arguments));
  (void)memset(fixture.hir_requirements, value,
               sizeof(fixture.hir_requirements));
  (void)memset(fixture.hir_values, value, sizeof(fixture.hir_values));
  (void)memset(fixture.hir_terminators, value,
               sizeof(fixture.hir_terminators));
  (void)memset(fixture.hir_entries, value, sizeof(fixture.hir_entries));
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
      (const uint8_t *)fixture.hir_functions,
      (const uint8_t *)fixture.hir_parameters,
      (const uint8_t *)fixture.hir_blocks,
      (const uint8_t *)fixture.hir_instructions,
      (const uint8_t *)fixture.hir_calls,
      (const uint8_t *)fixture.hir_host_parameters,
      (const uint8_t *)fixture.hir_arguments,
      (const uint8_t *)fixture.hir_requirements,
      (const uint8_t *)fixture.hir_values,
      (const uint8_t *)fixture.hir_terminators,
      (const uint8_t *)fixture.hir_entries,
      fixture.hir_text,
      fixture.hir_value_bytes,
      fixture.hir_receipt};
  const size_t sizes[] = {
      sizeof(fixture.hir_modules), sizeof(fixture.hir_identities),
      sizeof(fixture.hir_types), sizeof(fixture.hir_functions),
      sizeof(fixture.hir_parameters), sizeof(fixture.hir_blocks),
      sizeof(fixture.hir_instructions), sizeof(fixture.hir_calls),
      sizeof(fixture.hir_host_parameters), sizeof(fixture.hir_arguments),
      sizeof(fixture.hir_requirements), sizeof(fixture.hir_values),
      sizeof(fixture.hir_terminators), sizeof(fixture.hir_entries),
      sizeof(fixture.hir_text), sizeof(fixture.hir_value_bytes),
      sizeof(fixture.hir_receipt)};
  for (size_t region = 0u; region < sizeof(sizes) / sizeof(sizes[0]);
       region += 1u) {
    for (size_t byte = 0u; byte < sizes[region]; byte += 1u)
      if (regions[region][byte] != value) return false;
  }
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
      {&fixture.hir_output.type_capacity, 2u},
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
  fixture.hir_values[0].owner_argument = W_SEED_HIR0_NONE;
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
      {&fixture.result.required.enums, &fixture.result.written.enums},
      {&fixture.result.required.enum_cases, &fixture.result.written.enum_cases},
      {&fixture.result.required.enum_case_parameters,
       &fixture.result.written.enum_case_parameters},
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

int main(void) {
  if (!test_canonical_and_copy_boundary()) return 1;
  if (!test_semantic_and_provenance_digests()) return 1;
  if (!test_function_parameter_records()) return 1;
  if (!test_lowering_is_not_hello_hardcoded()) return 1;
  if (!test_capacity_and_alias_barriers()) return 1;
  if (!test_verify_mutations()) return 1;
  if (!test_closed_frontend_barriers()) return 1;
  (void)puts("hir0 tests: ok");
  return 0;
}
