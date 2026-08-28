#include "w_seed_hlo0.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
  GATE_SOURCE = 4096,
  GATE_LEXER_FRAMES = 256,
  GATE_TOKENS = 2048,
  GATE_NODES = 4096,
  GATE_PARSE_FRAMES = 2048,
  GATE_ISSUES = 128,
  GATE_MODULES = 8,
  GATE_IMPORTS = 16,
  GATE_IMPORT_ITEMS = 16,
  GATE_STRUCTS = 8,
  GATE_FIELDS = 16,
  GATE_TYPES = 32,
  GATE_FUNCTIONS = 8,
  GATE_PARAMETERS = 16,
  GATE_ENTRIES = 8,
  GATE_STATEMENTS = 64,
  GATE_EXPRESSIONS = 128,
  GATE_ARGUMENTS = 64,
  GATE_SYMBOLS = 64,
  GATE_FACTS = 64,
  GATE_DIAGNOSTICS = 32,
  GATE_RECEIPT = 65536,
  GATE_HIR_IDENTITIES = 32,
  GATE_HIR_RECORDS = 32,
  GATE_HIR_TEXT = 4096,
  GATE_HIR_VALUES = 4096,
  GATE_HIR_RECEIPT = 256,
};

typedef struct {
  uint8_t source_bytes[GATE_SOURCE];
  size_t source_length;
  w_seed_source source;
  w_seed_lexer_frame lexer_frames[GATE_LEXER_FRAMES];
  w_seed_parse_token tokens[GATE_TOKENS];
  w_seed_cst_node nodes[GATE_NODES];
  w_seed_parse_frame parse_frames[GATE_PARSE_FRAMES];
  w_seed_parse_issue issues[GATE_ISSUES];
  w_seed_parser parser;
  w_seed_parse_result parse;
  w_seed_frontend_document document;
  w_seed_frontend_input input;
  w_seed_frontend_module modules[GATE_MODULES];
  w_seed_frontend_import imports[GATE_IMPORTS];
  w_seed_frontend_import_item import_items[GATE_IMPORT_ITEMS];
  w_seed_frontend_struct structs[GATE_STRUCTS];
  w_seed_frontend_field fields[GATE_FIELDS];
  w_seed_frontend_type_declaration type_declarations[GATE_STRUCTS];
  w_seed_frontend_alias aliases[GATE_STRUCTS];
  w_seed_frontend_type types[GATE_TYPES];
  w_seed_frontend_function functions[GATE_FUNCTIONS];
  w_seed_frontend_parameter parameters[GATE_PARAMETERS];
  w_seed_frontend_entry entries[GATE_ENTRIES];
  w_seed_frontend_statement statements[GATE_STATEMENTS];
  w_seed_frontend_expression expressions[GATE_EXPRESSIONS];
  w_seed_frontend_argument arguments[GATE_ARGUMENTS];
  w_seed_frontend_symbol symbols[GATE_SYMBOLS];
  w_seed_frontend_fact facts[GATE_FACTS];
  w_seed_frontend_diagnostic diagnostics[GATE_DIAGNOSTICS];
  w_seed_frontend_diagnostic_fact diagnostic_facts[GATE_DIAGNOSTICS * 5];
  w_seed_frontend_diagnostic_item diagnostic_items[GATE_DIAGNOSTICS * 4];
  w_seed_frontend_diagnostic_label diagnostic_labels[GATE_DIAGNOSTICS * 2];
  w_seed_frontend_host_requirement host_requirements[2];
  w_seed_frontend_external_parameter host_parameters[2];
  w_seed_frontend_host_prelude_symbol host_symbols[2];
  w_seed_frontend_host_prelude host_scope;
  uint8_t const_bytes[GATE_SOURCE];
  uint8_t frontend_receipt[GATE_RECEIPT];
  w_seed_frontend_output output;
  w_seed_frontend_result frontend_result;
  w_seed_hir0_module hir_modules[GATE_HIR_RECORDS];
  w_seed_hir0_identity hir_identities[GATE_HIR_IDENTITIES];
  w_seed_hir0_type hir_types[GATE_HIR_RECORDS];
  w_seed_hir0_function hir_functions[GATE_HIR_RECORDS];
  w_seed_hir0_parameter hir_parameters[GATE_HIR_RECORDS];
  w_seed_hir0_block hir_blocks[GATE_HIR_RECORDS];
  w_seed_hir0_instruction hir_instructions[GATE_HIR_RECORDS];
  w_seed_hir0_call hir_calls[GATE_HIR_RECORDS];
  w_seed_hir0_host_parameter hir_host_parameters[GATE_HIR_RECORDS];
  w_seed_hir0_argument hir_arguments[GATE_HIR_RECORDS];
  w_seed_hir0_requirement hir_requirements[GATE_HIR_RECORDS];
  w_seed_hir0_value hir_values[GATE_HIR_RECORDS];
  w_seed_hir0_terminator hir_terminators[GATE_HIR_RECORDS];
  w_seed_hir0_entry hir_entries[GATE_HIR_RECORDS];
  uint8_t hir_text[GATE_HIR_TEXT];
  uint8_t hir_value_bytes[GATE_HIR_VALUES];
  uint8_t hir_receipt[GATE_HIR_RECEIPT];
  w_seed_hir0_output hir_output;
  w_seed_hir0_result hir_result;
  w_seed_hir0_program hir_program;
} gate_fixture;

static bool lower_hir(gate_fixture *fixture) {
  if (fixture == NULL) return false;
  fixture->hir_output = (w_seed_hir0_output){
      .modules = fixture->hir_modules,
      .module_capacity = GATE_HIR_RECORDS,
      .identities = fixture->hir_identities,
      .identity_capacity = GATE_HIR_IDENTITIES,
      .types = fixture->hir_types,
      .type_capacity = GATE_HIR_RECORDS,
      .functions = fixture->hir_functions,
      .function_capacity = GATE_HIR_RECORDS,
      .parameters = fixture->hir_parameters,
      .parameter_capacity = GATE_HIR_RECORDS,
      .blocks = fixture->hir_blocks,
      .block_capacity = GATE_HIR_RECORDS,
      .instructions = fixture->hir_instructions,
      .instruction_capacity = GATE_HIR_RECORDS,
      .calls = fixture->hir_calls,
      .call_capacity = GATE_HIR_RECORDS,
      .host_parameters = fixture->hir_host_parameters,
      .host_parameter_capacity = GATE_HIR_RECORDS,
      .arguments = fixture->hir_arguments,
      .argument_capacity = GATE_HIR_RECORDS,
      .requirements = fixture->hir_requirements,
      .requirement_capacity = GATE_HIR_RECORDS,
      .values = fixture->hir_values,
      .value_capacity = GATE_HIR_RECORDS,
      .terminators = fixture->hir_terminators,
      .terminator_capacity = GATE_HIR_RECORDS,
      .entries = fixture->hir_entries,
      .entry_capacity = GATE_HIR_RECORDS,
      .text_bytes = fixture->hir_text,
      .text_byte_capacity = sizeof(fixture->hir_text),
      .value_bytes = fixture->hir_value_bytes,
      .value_byte_capacity = sizeof(fixture->hir_value_bytes),
      .receipt = fixture->hir_receipt,
      .receipt_capacity = sizeof(fixture->hir_receipt)};
  const w_seed_hir0_input input = {
      &fixture->input, &fixture->output, &fixture->frontend_result};
  w_seed_hir0_counts counts;
  w_seed_hir0_result result;
  return w_seed_hir0_measure(&input, &counts, &result) == W_SEED_HIR0_OK &&
         w_seed_hir0_run(&input, &fixture->hir_output, &fixture->hir_result) ==
             W_SEED_HIR0_OK &&
         w_seed_hir0_program_from_output(&fixture->hir_output,
                                          &fixture->hir_result,
                                          &fixture->hir_program) &&
         w_seed_hir0_verify(&fixture->hir_program, &fixture->hir_result);
}

static bool read_fixture(gate_fixture *fixture, const char *path) {
  if (fixture == NULL || path == NULL) return false;
  FILE *file = fopen(path, "rb");
  if (file == NULL) return false;
  const size_t read = fread(fixture->source_bytes, 1u,
                            sizeof(fixture->source_bytes), file);
  const int read_error = ferror(file);
  const int extra = fgetc(file);
  (void)fclose(file);
  if (read_error != 0 || extra != EOF || read == 0u ||
      read >= sizeof(fixture->source_bytes))
    return false;
  fixture->source_length = read;
  w_seed_source_error source_error;
  if (!w_seed_source_init(
          (w_seed_byte_view){fixture->source_bytes, fixture->source_length},
          &fixture->source, &source_error))
    return false;
  w_seed_lex_error lex_error;
  if (!w_seed_parser_init(
          &fixture->source, (w_seed_span){0u, fixture->source_length},
          (w_seed_foreign_limits){65536u, 256u}, fixture->lexer_frames,
          GATE_LEXER_FRAMES, fixture->tokens, GATE_TOKENS, fixture->nodes,
          GATE_NODES, fixture->parse_frames, GATE_PARSE_FRAMES, fixture->issues,
          GATE_ISSUES, &fixture->parser, &lex_error) ||
      !w_seed_parser_parse(&fixture->parser, &fixture->parse))
    return false;
  fixture->document = (w_seed_frontend_document){
      .logical_source_id = (w_seed_frontend_text){"hlo0-fixture", 12u},
      .module_id = (w_seed_frontend_text){"hlo0-fixture", 12u},
      .local_module_name = (w_seed_frontend_text){"hlo0-fixture", 12u},
      .source = &fixture->source,
      .nodes = fixture->nodes,
      .node_count = fixture->parse.node_count,
      .parse = fixture->parse};
  fixture->input = (w_seed_frontend_input){
      .documents = &fixture->document,
      .document_count = 1u,
      .external_modules = NULL,
      .external_module_count = 0u,
      .host_scope = NULL,
      .import_resolution_complete = false,
      .resolved_imports = NULL,
      .resolved_import_count = 0u};
  fixture->output = (w_seed_frontend_output){
      .modules = fixture->modules,
      .module_capacity = GATE_MODULES,
      .imports = fixture->imports,
      .import_capacity = GATE_IMPORTS,
      .import_items = fixture->import_items,
      .import_item_capacity = GATE_IMPORT_ITEMS,
      .structs = fixture->structs,
      .struct_capacity = GATE_STRUCTS,
      .fields = fixture->fields,
      .field_capacity = GATE_FIELDS,
      .type_declarations = fixture->type_declarations,
      .type_declaration_capacity = GATE_STRUCTS,
      .aliases = fixture->aliases,
      .alias_capacity = GATE_STRUCTS,
      .types = fixture->types,
      .type_capacity = GATE_TYPES,
      .functions = fixture->functions,
      .function_capacity = GATE_FUNCTIONS,
      .parameters = fixture->parameters,
      .parameter_capacity = GATE_PARAMETERS,
      .entries = fixture->entries,
      .entry_capacity = GATE_ENTRIES,
      .statements = fixture->statements,
      .statement_capacity = GATE_STATEMENTS,
      .expressions = fixture->expressions,
      .expression_capacity = GATE_EXPRESSIONS,
      .arguments = fixture->arguments,
      .argument_capacity = GATE_ARGUMENTS,
      .symbols = fixture->symbols,
      .symbol_capacity = GATE_SYMBOLS,
      .facts = fixture->facts,
      .fact_capacity = GATE_FACTS,
      .diagnostics = fixture->diagnostics,
      .diagnostic_capacity = GATE_DIAGNOSTICS,
      .diagnostic_facts = fixture->diagnostic_facts,
      .diagnostic_fact_capacity = GATE_DIAGNOSTICS * 5u,
      .diagnostic_items = fixture->diagnostic_items,
      .diagnostic_item_capacity = GATE_DIAGNOSTICS * 4u,
      .diagnostic_labels = fixture->diagnostic_labels,
      .diagnostic_label_capacity = GATE_DIAGNOSTICS * 2u,
      .const_bytes = fixture->const_bytes,
      .const_bytes_capacity = sizeof(fixture->const_bytes),
      .receipt = fixture->frontend_receipt,
      .receipt_capacity = sizeof(fixture->frontend_receipt)};
  fixture->host_requirements[0] = (w_seed_frontend_host_requirement){
      .name = (w_seed_frontend_text){"Console", 7u}};
  fixture->host_parameters[0] = (w_seed_frontend_external_parameter){
      .name = (w_seed_frontend_text){"message", 7u},
      .type = (w_seed_frontend_text){"String", 6u},
      .label_kind = W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY};
  fixture->host_symbols[0] = (w_seed_frontend_host_prelude_symbol){
      .name = (w_seed_frontend_text){"noop", 4u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"()", 2u},
      .is_const = false,
      .requirements = NULL,
      .requirement_count = 0u};
  fixture->host_symbols[1] = (w_seed_frontend_host_prelude_symbol){
      .name = (w_seed_frontend_text){"print", 5u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .parameters = fixture->host_parameters,
      .parameter_count = 1u,
      .return_type = (w_seed_frontend_text){"()", 2u},
      .is_const = false,
      .requirements = fixture->host_requirements,
      .requirement_count = 1u};
  fixture->host_scope = (w_seed_frontend_host_prelude){
      .profile = (w_seed_frontend_text){"native-process@1", 16u},
      .symbols = fixture->host_symbols,
      .symbol_count = 2u};
  fixture->input.host_scope = &fixture->host_scope;
  return w_seed_frontend_run(&fixture->input, &fixture->output,
                             &fixture->frontend_result) == W_SEED_FRONTEND_OK &&
         lower_hir(fixture);
}

static void print_hex(const uint8_t *bytes, size_t length) {
  static const char digits[] = "0123456789abcdef";
  for (size_t index = 0u; index < length; index += 1u) {
    (void)putchar(digits[bytes[index] >> 4u]);
    (void)putchar(digits[bytes[index] & 0x0fu]);
  }
}

int main(int argc, char **argv) {
  if (argc != 2 || argv[1] == NULL) return 2;
  gate_fixture fixture;
  (void)memset(&fixture, 0, sizeof(fixture));
  if (!read_fixture(&fixture, argv[1])) {
    (void)fprintf(stderr, "HLO0 gate: fixture/frontend/HIR0 failed\n");
    return 1;
  }
  const w_seed_hlo0_input input = {
      .program = &fixture.hir_program, .hir_result = &fixture.hir_result};
  w_seed_hlo0_counts counts;
  w_seed_hlo0_result result;
  if (w_seed_hlo0_measure(&input, &counts, &result) != W_SEED_HLO0_OK ||
      counts.plans != 1u || counts.payload_bytes != 13u) {
    (void)fprintf(stderr, "HLO0 gate: measure failed\n");
    return 1;
  }
  w_seed_hlo0_plan plan;
  uint8_t receipt[4096];
  w_seed_hlo0_output output = {
      .plans = &plan,
      .plan_capacity = 1u,
      .receipt = receipt,
      .receipt_capacity = sizeof(receipt)};
  if (w_seed_hlo0_run(&input, &output, &result) != W_SEED_HLO0_OK ||
      plan.payload_bytes != 13u || plan.stdout_bytes != 14u ||
      plan.newline_policy != W_SEED_HLO0_NEWLINE_ADD_LF ||
      !plan.exit_success) {
    (void)fprintf(stderr, "HLO0 gate: run failed\n");
    return 1;
  }
  (void)printf("HLO0 gate: profile=%s entry=%s callee=%s requirement=%s payload=%lu stdout=%lu sha256=",
               plan.profile, plan.entry_target, plan.callee, plan.requirement,
               (unsigned long)plan.payload_bytes,
               (unsigned long)plan.stdout_bytes);
  print_hex(plan.stdout_sha256, sizeof(plan.stdout_sha256));
  (void)puts(" verified-HIR plan only; W execution unavailable");
  return 0;
}
