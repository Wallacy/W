#include "w_seed_hlo0.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "hlo0 check failed: %s (%s:%d)\n", #condition,  \
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
  TEST_HLO_RECEIPT = 4096,
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
  w_seed_hlo0_plan hlo_plan;
  uint8_t hlo_receipt[TEST_HLO_RECEIPT];
  w_seed_hlo0_output hlo_output;
  w_seed_hlo0_result hlo_result;
} hlo_fixture;

static hlo_fixture fixture;

static const char CANONICAL_SOURCE[] =
    "fn main() { print(\"Hello, world!\") }\nentry(main)\n";

static bool fixture_parse(const char *text) {
  if (text == NULL) return false;
  const size_t source_length = strlen(text);
  if (source_length >= sizeof(fixture.source_bytes)) return false;
  (void)memset(&fixture, 0, sizeof(fixture));
  fixture.source_length = source_length;
  (void)memcpy(fixture.source_bytes, text, source_length);
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
      .logical_source_id = (w_seed_frontend_text){"hlo0-test", 9u},
      .module_id = (w_seed_frontend_text){"hlo0-test", 9u},
      .local_module_name = (w_seed_frontend_text){"hlo0-test", 9u},
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
  return true;
}

static void configure_host(void) {
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
}

static bool fixture_frontend(const char *source) {
  CHECK(fixture_parse(source));
  configure_host();
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output,
                            &fixture.frontend_result) == W_SEED_FRONTEND_OK);
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
  const w_seed_hir0_input input = {
      &fixture.input, &fixture.output, &fixture.frontend_result};
  w_seed_hir0_counts counts;
  w_seed_hir0_result result;
  CHECK(w_seed_hir0_measure(&input, &counts, &result) == W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_OK);
  CHECK(fixture.hir_result.required.modules == counts.modules);
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output,
                                        &fixture.hir_result,
                                        &fixture.hir_program));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static w_seed_hlo0_input hlo_input(void) {
  return (w_seed_hlo0_input){&fixture.hir_program, &fixture.hir_result};
}

static void prepare_hlo_output(uint8_t value) {
  (void)memset(&fixture.hlo_plan, value, sizeof(fixture.hlo_plan));
  (void)memset(fixture.hlo_receipt, value, sizeof(fixture.hlo_receipt));
  fixture.hlo_output = (w_seed_hlo0_output){
      .plans = &fixture.hlo_plan,
      .plan_capacity = 1u,
      .receipt = fixture.hlo_receipt,
      .receipt_capacity = sizeof(fixture.hlo_receipt)};
}

static bool check_canonical_plan(void) {
  static const uint8_t digest[32] = {
      0xd9, 0x01, 0x4c, 0x46, 0x24, 0x84, 0x4a, 0xa5,
      0xba, 0xc3, 0x14, 0x77, 0x3d, 0x6b, 0x68, 0x9a,
      0xd4, 0x67, 0xfa, 0x4e, 0x1d, 0x1a, 0x50, 0xa1,
      0xb8, 0xa9, 0x9d, 0x5a, 0x95, 0xf7, 0x2f, 0xf5};
  static const char expected_receipt[] =
      "schema=w-seed-hlo0-1\n"
      "profile=native-process@1\n"
      "slot=.default\n"
      "entry=main\n"
      "handler=main\n"
      "callee=print\n"
      "requirement=Console\n"
      "effects=sync,no-throws,no-unsafe,no-borrows\n"
      "signature=zero-params,unit\n"
      "payload=13:48656c6c6f2c20776f726c6421\n"
      "newline=add-lf\n"
      "stdout=14\n"
      "sha256=d9014c4624844aa5bac314773d6b689ad467fa4e1d1a50a1b8a99d5a95f72ff5\n"
      "exit=success\n";
  CHECK(strcmp(fixture.hlo_plan.schema, W_SEED_HLO0_SCHEMA_VERSION) == 0);
  CHECK(strcmp(fixture.hlo_plan.profile, "native-process@1") == 0);
  CHECK(strcmp(fixture.hlo_plan.slot, ".default") == 0);
  CHECK(strcmp(fixture.hlo_plan.entry_target, "main") == 0);
  CHECK(strcmp(fixture.hlo_plan.handler, "main") == 0);
  CHECK(strcmp(fixture.hlo_plan.callee, "print") == 0);
  CHECK(strcmp(fixture.hlo_plan.requirement, "Console") == 0);
  CHECK(!fixture.hlo_plan.is_async && !fixture.hlo_plan.is_throws &&
        !fixture.hlo_plan.is_unsafe && !fixture.hlo_plan.has_borrow_clause &&
        fixture.hlo_plan.zero_parameters && fixture.hlo_plan.unit_return);
  CHECK(fixture.hlo_plan.newline_policy == W_SEED_HLO0_NEWLINE_ADD_LF);
  CHECK(fixture.hlo_plan.payload_bytes == 13u &&
        memcmp(fixture.hlo_plan.payload, "Hello, world!", 13u) == 0);
  CHECK(fixture.hlo_plan.stdout_bytes == 14u && fixture.hlo_plan.exit_success);
  CHECK(memcmp(fixture.hlo_plan.stdout_sha256, digest, sizeof(digest)) == 0);
  CHECK(fixture.hlo_result.written.plans == 1u &&
        fixture.hlo_result.written.payload_bytes == 13u &&
        fixture.hlo_result.written.receipt_bytes == sizeof(expected_receipt) - 1u);
  CHECK(fixture.hlo_result.written.receipt_bytes ==
        fixture.hlo_result.required.receipt_bytes);
  CHECK(memcmp(fixture.hlo_receipt, expected_receipt,
               sizeof(expected_receipt) - 1u) == 0);
  CHECK(w_seed_hlo0_verify_plan(&fixture.hlo_plan));
  return true;
}

static bool check_shared_verifier_rejects_forgery(void) {
  const w_seed_hlo0_plan canonical = fixture.hlo_plan;
  w_seed_hlo0_plan plan = canonical;
  plan.profile[0] = 'X';
  CHECK(!w_seed_hlo0_verify_plan(&plan));
  plan = canonical;
  plan.profile[sizeof("native-process@1")] = 'X';
  CHECK(!w_seed_hlo0_verify_plan(&plan));
  plan = canonical;
  plan.is_async = true;
  CHECK(!w_seed_hlo0_verify_plan(&plan));
  plan = canonical;
  plan.zero_parameters = false;
  CHECK(!w_seed_hlo0_verify_plan(&plan));
  plan = canonical;
  plan.payload[0] ^= 1u;
  CHECK(!w_seed_hlo0_verify_plan(&plan));
  plan = canonical;
  plan.payload[plan.payload_bytes] = 1u;
  CHECK(!w_seed_hlo0_verify_plan(&plan));
  plan = canonical;
  plan.payload_bytes -= 1u;
  CHECK(!w_seed_hlo0_verify_plan(&plan));
  plan = canonical;
  plan.stdout_bytes -= 1u;
  CHECK(!w_seed_hlo0_verify_plan(&plan));
  plan = canonical;
  plan.stdout_sha256[0] ^= 1u;
  CHECK(!w_seed_hlo0_verify_plan(&plan));
  plan = canonical;
  plan.exit_success = false;
  CHECK(!w_seed_hlo0_verify_plan(&plan));
  return true;
}

static bool expect_status_current(w_seed_hlo0_status expected,
                                  w_seed_hlo0_input input) {
  (void)memset(&fixture.hlo_result, 0x5au, sizeof(fixture.hlo_result));
  const w_seed_hlo0_plan plan_before = fixture.hlo_plan;
  uint8_t receipt_before[sizeof(fixture.hlo_receipt)];
  (void)memcpy(receipt_before, fixture.hlo_receipt, sizeof(receipt_before));
  const w_seed_hlo0_result result_before = fixture.hlo_result;
  CHECK(w_seed_hlo0_run(&input, &fixture.hlo_output, &fixture.hlo_result) ==
        expected);
  CHECK(memcmp(&fixture.hlo_plan, &plan_before, sizeof(plan_before)) == 0);
  CHECK(memcmp(fixture.hlo_receipt, receipt_before, sizeof(receipt_before)) ==
        0);
  CHECK(memcmp(&fixture.hlo_result, &result_before, sizeof(result_before)) ==
        0);
  return true;
}

static bool expect_status(w_seed_hlo0_status expected,
                          w_seed_hlo0_input input) {
  prepare_hlo_output(0xa5u);
  return expect_status_current(expected, input);
}

static void discard_frontend(void) {
  (void)memset(fixture.source_bytes, 0, sizeof(fixture.source_bytes));
  (void)memset(&fixture.source, 0, sizeof(fixture.source));
  (void)memset(&fixture.document, 0, sizeof(fixture.document));
  (void)memset(&fixture.input, 0, sizeof(fixture.input));
  (void)memset(fixture.modules, 0, sizeof(fixture.modules));
  (void)memset(fixture.functions, 0, sizeof(fixture.functions));
  (void)memset(fixture.entries, 0, sizeof(fixture.entries));
  (void)memset(fixture.expressions, 0, sizeof(fixture.expressions));
  (void)memset(fixture.arguments, 0, sizeof(fixture.arguments));
  (void)memset(fixture.const_bytes, 0, sizeof(fixture.const_bytes));
  (void)memset(fixture.frontend_receipt, 0, sizeof(fixture.frontend_receipt));
  (void)memset(&fixture.output, 0, sizeof(fixture.output));
  (void)memset(&fixture.frontend_result, 0, sizeof(fixture.frontend_result));
  (void)memset(&fixture.host_scope, 0, sizeof(fixture.host_scope));
}

static bool test_frontend_to_verified_hir_to_hlo(void) {
  CHECK(lower(CANONICAL_SOURCE));
  const w_seed_hlo0_input input = hlo_input();
  w_seed_hlo0_counts counts;
  w_seed_hlo0_result measure_result;
  CHECK(w_seed_hlo0_measure(&input, &counts, &measure_result) ==
        W_SEED_HLO0_OK);
  CHECK(counts.plans == 1u && counts.payload_bytes == 13u &&
        counts.receipt_bytes != 0u);
  discard_frontend();
  prepare_hlo_output(0xa5u);
  CHECK(w_seed_hlo0_run(&input, &fixture.hlo_output, &fixture.hlo_result) ==
        W_SEED_HLO0_OK);
  CHECK(fixture.hlo_result.status == W_SEED_HLO0_OK);
  CHECK(check_canonical_plan());
  CHECK(check_shared_verifier_rejects_forgery());
  const w_seed_hlo0_plan plan_before = fixture.hlo_plan;
  uint8_t receipt_before[sizeof(fixture.hlo_receipt)];
  (void)memcpy(receipt_before, fixture.hlo_receipt, sizeof(receipt_before));
  CHECK(w_seed_hlo0_run(&input, &fixture.hlo_output, &fixture.hlo_result) ==
        W_SEED_HLO0_OK);
  CHECK(memcmp(&fixture.hlo_plan, &plan_before, sizeof(plan_before)) == 0 &&
        memcmp(fixture.hlo_receipt, receipt_before, sizeof(receipt_before)) ==
            0);
  return true;
}

static bool test_hlo_all_or_nothing(void) {
  CHECK(lower(CANONICAL_SOURCE));
  const w_seed_hlo0_input input = hlo_input();
  w_seed_hlo0_counts counts;
  w_seed_hlo0_result measure_result;
  CHECK(w_seed_hlo0_measure(&input, &counts, &measure_result) ==
        W_SEED_HLO0_OK);
  prepare_hlo_output(0xa5u);
  fixture.hlo_output.plan_capacity = 0u;
  CHECK(expect_status_current(W_SEED_HLO0_CAPACITY, input));
  prepare_hlo_output(0xa5u);
  fixture.hlo_output.receipt_capacity = counts.receipt_bytes - 1u;
  CHECK(expect_status_current(W_SEED_HLO0_CAPACITY, input));
  prepare_hlo_output(0xa5u);
  fixture.hlo_output.receipt = (uint8_t *)&fixture.hlo_plan;
  fixture.hlo_output.receipt_capacity = sizeof(fixture.hlo_plan);
  CHECK(expect_status_current(W_SEED_HLO0_INVALID, input));
  w_seed_hlo0_counts counts_before = {0x11u, 0x22u, 0x33u};
  w_seed_hlo0_result result_before;
  (void)memset(&result_before, 0x7bu, sizeof(result_before));
  w_seed_hlo0_input bad_input = {NULL, &fixture.hir_result};
  CHECK(w_seed_hlo0_measure(&bad_input, &counts_before, &result_before) ==
        W_SEED_HLO0_INVALID);
  CHECK(counts_before.plans == 0x11u && counts_before.payload_bytes == 0x22u &&
        counts_before.receipt_bytes == 0x33u);
  w_seed_hlo0_result result_snapshot = result_before;
  CHECK(memcmp(&result_before, &result_snapshot, sizeof(result_before)) == 0);
  union {
    w_seed_hlo0_counts counts;
    w_seed_hlo0_result result;
  } measure_alias;
  (void)memset(&measure_alias, 0xa5, sizeof(measure_alias));
  const uint8_t *measure_alias_bytes =
      (const uint8_t *)(const void *)&measure_alias;
  CHECK(w_seed_hlo0_measure(
            &input, (w_seed_hlo0_counts *)(void *)&measure_alias,
            (w_seed_hlo0_result *)(void *)&measure_alias) ==
        W_SEED_HLO0_INVALID);
  for (size_t byte = 0u; byte < sizeof(measure_alias); byte += 1u)
    CHECK(measure_alias_bytes[byte] == 0xa5u);
  return true;
}

static bool test_hlo_result_alias_barriers(void) {
  CHECK(lower(CANONICAL_SOURCE));
  const w_seed_hlo0_input input = hlo_input();
  prepare_hlo_output(0xa5u);
  const w_seed_hlo0_plan plan_before = fixture.hlo_plan;
  uint8_t receipt_before[sizeof(fixture.hlo_receipt)];
  (void)memcpy(receipt_before, fixture.hlo_receipt, sizeof(receipt_before));
  CHECK(w_seed_hlo0_run(
            &input, &fixture.hlo_output,
            (w_seed_hlo0_result *)(void *)&fixture.hlo_plan) ==
        W_SEED_HLO0_INVALID);
  CHECK(memcmp(&fixture.hlo_plan, &plan_before, sizeof(plan_before)) == 0 &&
        memcmp(fixture.hlo_receipt, receipt_before, sizeof(receipt_before)) ==
            0);

  prepare_hlo_output(0xa5u);
  uint8_t hir_text_before[sizeof(fixture.hir_text)];
  (void)memcpy(hir_text_before, fixture.hir_text, sizeof(hir_text_before));
  CHECK(w_seed_hlo0_run(&input, &fixture.hlo_output,
                        (w_seed_hlo0_result *)(void *)fixture.hir_text) ==
        W_SEED_HLO0_INVALID);
  CHECK(memcmp(fixture.hir_text, hir_text_before, sizeof(hir_text_before)) ==
        0);

  prepare_hlo_output(0xa5u);
  const w_seed_hir0_module hir_module_before = fixture.hir_modules[0];
  fixture.hlo_output.plans = (w_seed_hlo0_plan *)(void *)fixture.hir_modules;
  fixture.hlo_output.plan_capacity = 1u;
  CHECK(w_seed_hlo0_run(&input, &fixture.hlo_output, &fixture.hlo_result) ==
        W_SEED_HLO0_INVALID);
  CHECK(memcmp(&fixture.hir_modules[0], &hir_module_before,
               sizeof(hir_module_before)) == 0);

  prepare_hlo_output(0xa5u);
  const uint8_t hir_text_plan_before[sizeof(fixture.hir_text)];
  (void)memcpy((void *)hir_text_plan_before, fixture.hir_text,
               sizeof(hir_text_plan_before));
  fixture.hlo_output.plans = (w_seed_hlo0_plan *)(void *)fixture.hir_text;
  fixture.hlo_output.plan_capacity = 1u;
  CHECK(w_seed_hlo0_run(&input, &fixture.hlo_output, &fixture.hlo_result) ==
        W_SEED_HLO0_INVALID);
  CHECK(memcmp(fixture.hir_text, hir_text_plan_before,
               sizeof(hir_text_plan_before)) == 0);

  prepare_hlo_output(0xa5u);
  const w_seed_hir0_result hir_result_receipt_before = fixture.hir_result;
  fixture.hlo_output.receipt = (uint8_t *)(void *)&fixture.hir_result;
  fixture.hlo_output.receipt_capacity = sizeof(fixture.hir_result);
  CHECK(w_seed_hlo0_run(&input, &fixture.hlo_output, &fixture.hlo_result) ==
        W_SEED_HLO0_INVALID);
  CHECK(memcmp(&fixture.hir_result, &hir_result_receipt_before,
               sizeof(hir_result_receipt_before)) == 0);

  prepare_hlo_output(0xa5u);
  const w_seed_hlo0_plan hlo_plan_before = fixture.hlo_plan;
  fixture.hlo_output.receipt = (uint8_t *)(void *)fixture.hir_modules;
  fixture.hlo_output.receipt_capacity = sizeof(fixture.hir_modules);
  CHECK(w_seed_hlo0_run(&input, &fixture.hlo_output, &fixture.hlo_result) ==
        W_SEED_HLO0_INVALID);
  CHECK(memcmp(&fixture.hir_modules[0], &hir_module_before,
               sizeof(hir_module_before)) == 0 &&
        memcmp(&fixture.hlo_plan, &hlo_plan_before, sizeof(hlo_plan_before)) ==
            0);

  prepare_hlo_output(0xa5u);
  const w_seed_hir0_result hir_result_before = fixture.hir_result;
  CHECK(w_seed_hlo0_run(&input, &fixture.hlo_output,
                        (w_seed_hlo0_result *)(void *)&fixture.hir_result) ==
        W_SEED_HLO0_INVALID);
  CHECK(memcmp(&fixture.hir_result, &hir_result_before,
               sizeof(hir_result_before)) == 0);

  prepare_hlo_output(0xa5u);
  uint8_t hlo_receipt_result_before[sizeof(fixture.hlo_receipt)];
  (void)memcpy(hlo_receipt_result_before, fixture.hlo_receipt,
               sizeof(hlo_receipt_result_before));
  CHECK(w_seed_hlo0_run(
            &input, &fixture.hlo_output,
            (w_seed_hlo0_result *)(void *)fixture.hlo_receipt) ==
        W_SEED_HLO0_INVALID);
  CHECK(memcmp(fixture.hlo_receipt, hlo_receipt_result_before,
               sizeof(hlo_receipt_result_before)) == 0);

  prepare_hlo_output(0xa5u);
  const w_seed_hir0_module hir_module_result_before = fixture.hir_modules[0];
  CHECK(w_seed_hlo0_run(
            &input, &fixture.hlo_output,
            (w_seed_hlo0_result *)(void *)fixture.hir_modules) ==
        W_SEED_HLO0_INVALID);
  CHECK(memcmp(&fixture.hir_modules[0], &hir_module_result_before,
               sizeof(hir_module_result_before)) == 0);
  return true;
}

static bool test_hir_result_forgery(void) {
  CHECK(lower(CANONICAL_SOURCE));
  const w_seed_hlo0_input input = hlo_input();
  const w_seed_hir0_result saved = fixture.hir_result;
  fixture.hir_result.semantic_digest[0] ^= 1u;
  CHECK(expect_status(W_SEED_HLO0_INVALID, input));
  fixture.hir_result = saved;
  fixture.hir_result.provenance_digest[0] ^= 1u;
  CHECK(expect_status(W_SEED_HLO0_INVALID, input));
  fixture.hir_result = saved;
  fixture.hir_result.schema[0] = 'X';
  CHECK(expect_status(W_SEED_HLO0_INVALID, input));
  fixture.hir_result = saved;
  fixture.hir_result.status = W_SEED_HIR0_INVALID;
  CHECK(expect_status(W_SEED_HLO0_INVALID, input));
  fixture.hir_result = saved;
  fixture.hir_result.written.values -= 1u;
  CHECK(expect_status(W_SEED_HLO0_INVALID, input));
  fixture.hir_result = saved;
  fixture.hir_receipt[0] ^= 1u;
  CHECK(expect_status(W_SEED_HLO0_INVALID, input));
  fixture.hir_result = saved;
  fixture.hir_receipt[0] ^= 1u;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool test_hir_record_forgery(void) {
  CHECK(lower(CANONICAL_SOURCE));
  const w_seed_hlo0_input input = hlo_input();
  const w_seed_hir0_call saved_call = fixture.hir_calls[0];
  fixture.hir_calls[0].callee_identity = 3u;
  CHECK(expect_status(W_SEED_HLO0_INVALID, input));
  fixture.hir_calls[0] = saved_call;
  const w_seed_hir0_argument saved_argument = fixture.hir_arguments[0];
  fixture.hir_arguments[0].type_index = W_SEED_HIR0_TYPE_UNIT;
  CHECK(expect_status(W_SEED_HLO0_INVALID, input));
  fixture.hir_arguments[0] = saved_argument;
  const w_seed_hir0_function saved_function = fixture.hir_functions[0];
  fixture.hir_functions[0].return_type = W_SEED_HIR0_TYPE_STRING;
  CHECK(expect_status(W_SEED_HLO0_INVALID, input));
  fixture.hir_functions[0] = saved_function;
  const w_seed_hir0_entry saved_entry = fixture.hir_entries[0];
  fixture.hir_entries[0].target_function = W_SEED_HIR0_NONE;
  CHECK(expect_status(W_SEED_HLO0_INVALID, input));
  fixture.hir_entries[0] = saved_entry;
  const w_seed_hir0_requirement saved_requirement = fixture.hir_requirements[0];
  fixture.hir_requirements[0].owner_index = W_SEED_HIR0_NONE;
  CHECK(expect_status(W_SEED_HLO0_INVALID, input));
  fixture.hir_requirements[0] = saved_requirement;
  const uint32_t slot_offset = fixture.hir_entries[0].slot.offset;
  fixture.hir_text[slot_offset] = 'X';
  CHECK(expect_status(W_SEED_HLO0_INVALID, input));
  fixture.hir_text[slot_offset] = '.';
  const uint32_t value_offset = fixture.hir_values[0].byte_offset;
  fixture.hir_value_bytes[value_offset] = 'X';
  CHECK(expect_status(W_SEED_HLO0_INVALID, input));
  fixture.hir_value_bytes[value_offset] = 'H';
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool test_hlo_selection_not_hardcoded(void) {
  static const char OTHER_SOURCE[] =
      "fn main() { print(\"north\") }\nentry(main)\n";
  CHECK(lower(OTHER_SOURCE));
  const w_seed_hlo0_input input = hlo_input();
  w_seed_hlo0_counts counts;
  w_seed_hlo0_result result;
  CHECK(w_seed_hlo0_measure(&input, &counts, &result) ==
        W_SEED_HLO0_UNSUPPORTED);
  CHECK(expect_status(W_SEED_HLO0_UNSUPPORTED, input));
  return true;
}

int main(void) {
  if (!test_frontend_to_verified_hir_to_hlo()) return 1;
  if (!test_hlo_all_or_nothing()) return 1;
  if (!test_hlo_result_alias_barriers()) return 1;
  if (!test_hir_result_forgery()) return 1;
  if (!test_hir_record_forgery()) return 1;
  if (!test_hlo_selection_not_hardcoded()) return 1;
  (void)puts("seed HLO0: verified-HIR Hello plan and adversarial cases passed");
  return 0;
}
