#include "w_seed_hlo0.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      (void)fprintf(stderr, "hlo0 check failed: %s (%s:%d)\n", #condition, \
                    __FILE__, __LINE__);                                       \
      return false;                                                             \
    }                                                                           \
  } while (0)

enum {
  TEST_SOURCE = 4096,
  TEST_LEXER_FRAMES = 256,
  TEST_TOKENS = 2048,
  TEST_NODES = 4096,
  TEST_PARSE_FRAMES = 2048,
  TEST_ISSUES = 128,
  TEST_MODULES = 8,
  TEST_IMPORTS = 16,
  TEST_IMPORT_ITEMS = 16,
  TEST_STRUCTS = 8,
  TEST_FIELDS = 16,
  TEST_TYPES = 32,
  TEST_FUNCTIONS = 8,
  TEST_PARAMETERS = 16,
  TEST_ENTRIES = 8,
  TEST_STATEMENTS = 64,
  TEST_EXPRESSIONS = 128,
  TEST_ARGUMENTS = 64,
  TEST_SYMBOLS = 64,
  TEST_FACTS = 64,
  TEST_DIAGNOSTICS = 32,
  TEST_RECEIPT = 65536,
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
  w_seed_frontend_external_parameter external_parameters[2];
  w_seed_frontend_external_symbol external_symbols[2];
  w_seed_frontend_external_module external_modules[2];
  w_seed_frontend_resolved_import resolved_imports[2];
  uint8_t const_bytes[TEST_SOURCE];
  uint8_t frontend_receipt[TEST_RECEIPT];
  w_seed_frontend_output output;
  w_seed_frontend_result result;
  w_seed_hlo0_plan plan;
  uint8_t hlo_receipt[4096];
  w_seed_hlo0_output hlo_output;
  w_seed_hlo0_result hlo_result;
} hlo_fixture;

static hlo_fixture fixture;

static const char CANONICAL_SOURCE[] =
    "fn main() { print(\"Hello, world!\") }\nentry(main)\n";

static bool fixture_parse(const char *text) {
  fixture.source_length = strlen(text);
  if (fixture.source_length >= sizeof(fixture.source_bytes)) return false;
  (void)memset(&fixture, 0, sizeof(fixture));
  fixture.source_length = strlen(text);
  (void)memcpy(fixture.source_bytes, text, fixture.source_length);
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

static bool configure_external_print_import(void) {
  w_seed_module_origin origins[2];
  w_seed_module_scan_result scan_result;
  fixture.external_parameters[0] = (w_seed_frontend_external_parameter){
      .name = (w_seed_frontend_text){"message", 7u},
      .type = (w_seed_frontend_text){"String", 6u},
      .label_kind = W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY};
  fixture.external_symbols[0] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"print", 5u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .exported = true,
      .parameters = fixture.external_parameters,
      .parameter_count = 1u,
      .return_type = (w_seed_frontend_text){"()", 2u},
      .is_const = false};
  fixture.external_modules[0] = (w_seed_frontend_external_module){
      .module_id = (w_seed_frontend_text){"extdep", 6u},
      .symbols = fixture.external_symbols,
      .symbol_count = 1u};
  fixture.input.external_modules = fixture.external_modules;
  fixture.input.external_module_count = 1u;
  if (w_seed_module_scan(
          &fixture.source, fixture.nodes, fixture.parse.node_count,
          &fixture.parse, origins, sizeof(origins) / sizeof(origins[0]),
          &scan_result) != W_SEED_MODULE_SCAN_OK ||
      scan_result.written != 1u)
    return false;
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

static bool fixture_frontend(const char *source) {
  if (!fixture_parse(source)) return false;
  configure_host();
  return w_seed_frontend_run(&fixture.input, &fixture.output,
                             &fixture.result) == W_SEED_FRONTEND_OK;
}

static void prepare_hlo_output(uint8_t value) {
  (void)memset(&fixture.plan, value, sizeof(fixture.plan));
  (void)memset(fixture.hlo_receipt, value, sizeof(fixture.hlo_receipt));
  fixture.hlo_output = (w_seed_hlo0_output){
      .plans = &fixture.plan,
      .plan_capacity = 1u,
      .receipt = fixture.hlo_receipt,
      .receipt_capacity = sizeof(fixture.hlo_receipt)};
}

static w_seed_hlo0_input hlo_input(void) {
  return (w_seed_hlo0_input){
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .host_scope = &fixture.host_scope,
      .profile_identity = fixture.host_scope.profile};
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
  CHECK(strcmp(fixture.plan.schema, W_SEED_HLO0_SCHEMA_VERSION) == 0);
  CHECK(strcmp(fixture.plan.profile, "native-process@1") == 0);
  CHECK(strcmp(fixture.plan.slot, ".default") == 0);
  CHECK(strcmp(fixture.plan.entry_target, "main") == 0);
  CHECK(strcmp(fixture.plan.handler, "main") == 0);
  CHECK(strcmp(fixture.plan.callee, "print") == 0);
  CHECK(strcmp(fixture.plan.requirement, "Console") == 0);
  CHECK(!fixture.plan.is_async && !fixture.plan.is_throws &&
        !fixture.plan.is_unsafe && !fixture.plan.has_borrow_clause &&
        fixture.plan.zero_parameters && fixture.plan.unit_return);
  CHECK(fixture.plan.newline_policy == W_SEED_HLO0_NEWLINE_ADD_LF);
  CHECK(fixture.plan.payload_bytes == 13u &&
        memcmp(fixture.plan.payload, "Hello, world!", 13u) == 0);
  CHECK(fixture.plan.stdout_bytes == 14u && fixture.plan.exit_success);
  CHECK(memcmp(fixture.plan.stdout_sha256, digest, sizeof(digest)) == 0);
  CHECK(fixture.hlo_result.written.plans == 1u &&
        fixture.hlo_result.written.payload_bytes == 13u &&
        fixture.hlo_result.written.receipt_bytes == sizeof(expected_receipt) - 1u);
  CHECK(fixture.hlo_result.written.receipt_bytes ==
        fixture.hlo_result.required.receipt_bytes);
  CHECK(memcmp(fixture.hlo_receipt, expected_receipt,
               sizeof(expected_receipt) - 1u) == 0);
  return true;
}

static bool test_canonical_measure_run(void) {
  CHECK(fixture_frontend(CANONICAL_SOURCE));
  size_t call_count = 0u;
  for (size_t index = 0u; index < fixture.result.written.expressions;
       index += 1u) {
    if (fixture.expressions[index].kind == W_SEED_FRONTEND_EXPR_CALL) {
      CHECK(fixture.expressions[index].resolved_host_symbol_index == 1u);
      CHECK((size_t)fixture.expressions[index].left <
            fixture.result.written.expressions);
      CHECK(fixture.expressions[fixture.expressions[index].left]
                .resolved_host_symbol_index == 1u);
      call_count += 1u;
    }
  }
  CHECK(call_count == 1u);
  const w_seed_hlo0_input input = hlo_input();
  w_seed_hlo0_counts measured;
  w_seed_hlo0_result measure_result;
  CHECK(w_seed_hlo0_measure(&input, &measured, &measure_result) ==
        W_SEED_HLO0_OK);
  CHECK(measured.plans == 1u && measured.payload_bytes == 13u &&
        measured.receipt_bytes != 0u);
  prepare_hlo_output(0xa5u);
  CHECK(w_seed_hlo0_run(&input, &fixture.hlo_output, &fixture.hlo_result) ==
        W_SEED_HLO0_OK);
  CHECK(fixture.hlo_result.status == W_SEED_HLO0_OK &&
        fixture.hlo_result.required.plans == measured.plans &&
        fixture.hlo_result.required.payload_bytes == measured.payload_bytes &&
        fixture.hlo_result.required.receipt_bytes == measured.receipt_bytes);
  CHECK(check_canonical_plan());
  w_seed_hlo0_plan plan_copy = fixture.plan;
  uint8_t receipt_copy[sizeof(fixture.hlo_receipt)];
  (void)memcpy(receipt_copy, fixture.hlo_receipt, sizeof(receipt_copy));
  CHECK(w_seed_hlo0_run(&input, &fixture.hlo_output, &fixture.hlo_result) ==
        W_SEED_HLO0_OK);
  CHECK(memcmp(&fixture.plan, &plan_copy, sizeof(plan_copy)) == 0 &&
        memcmp(fixture.hlo_receipt, receipt_copy, sizeof(receipt_copy)) == 0);
  return true;
}

static bool expect_rejection_with_capacity(w_seed_hlo0_status expected,
                                           w_seed_hlo0_input input,
                                           size_t plan_capacity,
                                           size_t receipt_capacity) {
  prepare_hlo_output(0xa5u);
  fixture.hlo_output.plan_capacity = plan_capacity;
  fixture.hlo_output.receipt_capacity = receipt_capacity;
  w_seed_hlo0_plan plan_copy = fixture.plan;
  uint8_t receipt_copy[sizeof(fixture.hlo_receipt)];
  (void)memcpy(receipt_copy, fixture.hlo_receipt, sizeof(receipt_copy));
  CHECK(w_seed_hlo0_run(&input, &fixture.hlo_output, &fixture.hlo_result) ==
        expected);
  CHECK(memcmp(&fixture.plan, &plan_copy, sizeof(plan_copy)) == 0 &&
        memcmp(fixture.hlo_receipt, receipt_copy, sizeof(receipt_copy)) == 0);
  return true;
}

static bool expect_rejection(w_seed_hlo0_status expected,
                             w_seed_hlo0_input input) {
  return expect_rejection_with_capacity(expected, input, 1u,
                                         sizeof(fixture.hlo_receipt));
}

static bool expect_rejection_current_output(w_seed_hlo0_status expected,
                                             w_seed_hlo0_input input) {
  w_seed_hlo0_plan plan_copy = fixture.plan;
  uint8_t receipt_copy[sizeof(fixture.hlo_receipt)];
  (void)memcpy(receipt_copy, fixture.hlo_receipt, sizeof(receipt_copy));
  CHECK(w_seed_hlo0_run(&input, &fixture.hlo_output, &fixture.hlo_result) ==
        expected);
  CHECK(memcmp(&fixture.plan, &plan_copy, sizeof(plan_copy)) == 0 &&
        memcmp(fixture.hlo_receipt, receipt_copy, sizeof(receipt_copy)) == 0);
  return true;
}

static bool test_capacity_and_result_barriers(void) {
  CHECK(fixture_frontend(CANONICAL_SOURCE));
  w_seed_hlo0_input input = hlo_input();
  w_seed_hlo0_counts counts;
  w_seed_hlo0_result result;
  CHECK(w_seed_hlo0_measure(&input, &counts, &result) == W_SEED_HLO0_OK);
  CHECK(expect_rejection_with_capacity(W_SEED_HLO0_CAPACITY, input, 0u,
                                       sizeof(fixture.hlo_receipt)));
  CHECK(expect_rejection_with_capacity(W_SEED_HLO0_CAPACITY, input, 1u,
                                       counts.receipt_bytes - 1u));
  fixture.result.status = W_SEED_FRONTEND_UNSUPPORTED;
  CHECK(expect_rejection(W_SEED_HLO0_FRONTEND, input));
  fixture.result.status = W_SEED_FRONTEND_OK;
  fixture.result.written.expressions -= 1u;
  CHECK(expect_rejection(W_SEED_HLO0_INVALID, input));
  fixture.result.written.expressions += 1u;
  fixture.result.schema_version = (w_seed_frontend_text){"w-seed-frontend-9", 18u};
  CHECK(expect_rejection(W_SEED_HLO0_INVALID, input));
  return true;
}

static bool test_host_and_graph_mutations(void) {
  static const char *const canonical = CANONICAL_SOURCE;
  CHECK(fixture_frontend(canonical));
  w_seed_hlo0_input input = hlo_input();
  fixture.host_scope.profile = (w_seed_frontend_text){"other@1", 7u};
  CHECK(expect_rejection(W_SEED_HLO0_INVALID, input));
  configure_host();
  fixture.host_parameters[0].type = (w_seed_frontend_text){"Bytes", 5u};
  CHECK(expect_rejection(W_SEED_HLO0_UNSUPPORTED, input));
  configure_host();
  fixture.host_requirements[0].name = (w_seed_frontend_text){"File", 4u};
  CHECK(expect_rejection(W_SEED_HLO0_UNSUPPORTED, input));
  configure_host();
  fixture.functions[0].is_throws = true;
  CHECK(expect_rejection(W_SEED_HLO0_UNSUPPORTED, input));
  fixture.functions[0].is_throws = false;
  fixture.functions[0].return_type = W_SEED_FRONTEND_NONE;
  CHECK(expect_rejection(W_SEED_HLO0_UNSUPPORTED, input));
  fixture.functions[0].return_type = 0u;
  fixture.entries[0].target = (w_seed_frontend_text){"other", 5u};
  CHECK(expect_rejection(W_SEED_HLO0_UNSUPPORTED, input));
  fixture.entries[0].target = (w_seed_frontend_text){"main", 4u};
  fixture.statements[0].owner_function = W_SEED_FRONTEND_NONE;
  CHECK(expect_rejection(W_SEED_HLO0_INVALID, input));
  fixture.statements[0].owner_function = 0u;
  fixture.statements[0].expression_index = 1u;
  CHECK(expect_rejection(W_SEED_HLO0_UNSUPPORTED, input));
  fixture.statements[0].expression_index = 2u;
  fixture.expressions[2].left = W_SEED_FRONTEND_NONE;
  CHECK(expect_rejection(W_SEED_HLO0_UNSUPPORTED, input));
  fixture.expressions[2].left = 0u;
  fixture.expressions[2].resolved_host_symbol_index = W_SEED_FRONTEND_NONE;
  CHECK(expect_rejection(W_SEED_HLO0_INVALID, input));
  fixture.expressions[2].resolved_host_symbol_index = 1u;
  fixture.expressions[2].resolved_callee_kind =
      W_SEED_FRONTEND_CALLEE_EXTERNAL_MODULE_SYMBOL;
  CHECK(expect_rejection(W_SEED_HLO0_INVALID, input));
  fixture.expressions[2].resolved_callee_kind =
      W_SEED_FRONTEND_CALLEE_HOST_PRELUDE_SYMBOL;
  fixture.arguments[0].expression_index = 0u;
  CHECK(expect_rejection(W_SEED_HLO0_UNSUPPORTED, input));
  fixture.arguments[0].expression_index = 1u;
  fixture.const_bytes[0] = (uint8_t)'X';
  CHECK(expect_rejection(W_SEED_HLO0_UNSUPPORTED, input));
  fixture.const_bytes[0] = (uint8_t)'H';
  fixture.expressions[1].const_byte_offset =
      (uint32_t)fixture.result.written.const_bytes;
  CHECK(expect_rejection(W_SEED_HLO0_INVALID, input));
  fixture.expressions[1].const_byte_offset = 0u;
  fixture.expressions[1].span.end_byte = fixture.source_length + 1u;
  CHECK(expect_rejection(W_SEED_HLO0_INVALID, input));
  return true;
}

static bool test_source_forgery_and_unsupported_forms(void) {
  w_seed_hlo0_input input;
  CHECK(fixture_frontend(
      "// Hello, world!\n"
      "fn main() { print(\"Other\") }\nentry(main)\n"));
  input = hlo_input();
  CHECK(expect_rejection(W_SEED_HLO0_UNSUPPORTED, input));
  CHECK(fixture_parse(
      "fn main() { print(\"Hello, ${name}\") }\nentry(main)\n"));
  configure_host();
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output,
                            &fixture.result) != W_SEED_FRONTEND_OK);
  input = hlo_input();
  CHECK(expect_rejection(W_SEED_HLO0_FRONTEND, input));
  CHECK(fixture_parse(
      "fn main() { print(\"Hello, \\n\") }\nentry(main)\n"));
  configure_host();
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output,
                            &fixture.result) != W_SEED_FRONTEND_OK);
  input = hlo_input();
  CHECK(fixture.result.status != W_SEED_FRONTEND_OK);
  CHECK(expect_rejection(W_SEED_HLO0_FRONTEND, input));
  CHECK(fixture_frontend(
      "fn other() { print(\"Hello, world!\") }\n"
      "fn main() { print(\"Hello, world!\") }\nentry(main)\n"));
  input = hlo_input();
  CHECK(expect_rejection(W_SEED_HLO0_UNSUPPORTED, input));
  CHECK(fixture_frontend(
      "async fn main() { print(\"Hello, world!\") }\nentry(main)\n"));
  input = hlo_input();
  CHECK(expect_rejection(W_SEED_HLO0_UNSUPPORTED, input));
  return true;
}

static bool test_coherent_scope_and_external_unsupported(void) {
  w_seed_hlo0_input input;
  CHECK(fixture_parse(CANONICAL_SOURCE));
  configure_host();
  fixture.host_scope.profile = (w_seed_frontend_text){"other@1", 7u};
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output,
                            &fixture.result) == W_SEED_FRONTEND_OK);
  input = hlo_input();
  CHECK(expect_rejection(W_SEED_HLO0_UNSUPPORTED, input));

  CHECK(fixture_parse(CANONICAL_SOURCE));
  configure_host();
  fixture.host_requirements[0].name = (w_seed_frontend_text){"File", 4u};
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output,
                            &fixture.result) == W_SEED_FRONTEND_OK);
  input = hlo_input();
  CHECK(expect_rejection(W_SEED_HLO0_UNSUPPORTED, input));

  CHECK(fixture_parse(
      "import { print } from extdep\n"
      "fn main() { print(\"Hello, world!\") }\n"
      "entry(main)\n"));
  configure_host();
  CHECK(configure_external_print_import());
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output,
                            &fixture.result) == W_SEED_FRONTEND_OK);
  input = hlo_input();
  CHECK(expect_rejection(W_SEED_HLO0_UNSUPPORTED, input));
  return true;
}

static bool test_aliasing_and_active_scope(void) {
  CHECK(fixture_frontend(CANONICAL_SOURCE));
  w_seed_hlo0_input input = hlo_input();
  prepare_hlo_output(0xa5u);
  fixture.hlo_output.receipt = fixture.frontend_receipt;
  fixture.hlo_output.receipt_capacity = sizeof(fixture.frontend_receipt);
  CHECK(expect_rejection_current_output(W_SEED_HLO0_INVALID, input));
  configure_host();
  input.host_scope = &fixture.host_scope;
  input.profile_identity = (w_seed_frontend_text){"native-process@2", 16u};
  CHECK(expect_rejection(W_SEED_HLO0_INVALID, input));
  return true;
}

int main(void) {
  if (!test_canonical_measure_run()) return 1;
  if (!test_capacity_and_result_barriers()) return 1;
  if (!test_host_and_graph_mutations()) return 1;
  if (!test_source_forgery_and_unsupported_forms()) return 1;
  if (!test_coherent_scope_and_external_unsupported()) return 1;
  if (!test_aliasing_and_active_scope()) return 1;
  (void)puts("seed HLO0: source-backed bounded Hello plan and adversarial cases passed");
  return 0;
}
