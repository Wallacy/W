/* Test-only source-to-RUN0 harness. fopen below acquires bounded fixtures for
 * this gate. It is not a safe source-acquisition implementation. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "w_cli_io.h"
#include "w_seed_frontend.h"
#include "w_seed_hir0.h"
#include "w_seed_hlo0.h"
#include "w_seed_parser.h"
#include "w_seed_run0.h"
#include "w_seed_source.h"

enum {
  RUN_SOURCE_LIMIT = 4096,
  RUN_SOURCE_READ_CAPACITY = RUN_SOURCE_LIMIT + 1,
  RUN_LEXER_FRAMES = 256,
  RUN_TOKENS = 2048,
  RUN_NODES = 4096,
  RUN_PARSE_FRAMES = 2048,
  RUN_ISSUES = 128,
  RUN_MODULES = 8,
  RUN_IMPORTS = 8,
  RUN_IMPORT_ITEMS = 16,
  RUN_STRUCTS = 8,
  RUN_FIELDS = 16,
  RUN_TYPE_DECLARATIONS = 8,
  RUN_ALIASES = 8,
  RUN_TYPES = 32,
  RUN_FUNCTIONS = 16,
  RUN_PARAMETERS = 32,
  RUN_ENTRIES = 8,
  RUN_STATEMENTS = 128,
  RUN_EXPRESSIONS = 256,
  RUN_ARGUMENTS = 128,
  RUN_SYMBOLS = 128,
  RUN_FACTS = 128,
  RUN_DIAGNOSTICS = 64,
  RUN_DIAGNOSTIC_FACTS = 256,
  RUN_DIAGNOSTIC_ITEMS = 256,
  RUN_DIAGNOSTIC_LABELS = 128,
  RUN_FRONTEND_RECEIPT = 65536,
  RUN_HIR_IDENTITIES = 16,
  RUN_HIR_RECORDS = 64,
  RUN_HIR_TEXT = 4096,
  RUN_HIR_VALUES = 4096,
  RUN_HIR_RECEIPT = 256,
  RUN_HLO_RECEIPT = 4096,
};

typedef struct {
  uint8_t source_bytes[RUN_SOURCE_READ_CAPACITY];
  w_seed_source source;
  w_seed_lexer_frame lexer_frames[RUN_LEXER_FRAMES];
  w_seed_parse_token tokens[RUN_TOKENS];
  w_seed_cst_node nodes[RUN_NODES];
  w_seed_parse_frame parse_frames[RUN_PARSE_FRAMES];
  w_seed_parse_issue issues[RUN_ISSUES];
  w_seed_parser parser;
  w_seed_parse_result parse;
  w_seed_frontend_document document;
  w_seed_frontend_input frontend_input;
  w_seed_frontend_output frontend_output;
  w_seed_frontend_result frontend_result;
  w_seed_frontend_module modules[RUN_MODULES];
  w_seed_frontend_import imports[RUN_IMPORTS];
  w_seed_frontend_import_item import_items[RUN_IMPORT_ITEMS];
  w_seed_frontend_struct structs[RUN_STRUCTS];
  w_seed_frontend_field fields[RUN_FIELDS];
  w_seed_frontend_type_declaration type_declarations[RUN_TYPE_DECLARATIONS];
  w_seed_frontend_alias aliases[RUN_ALIASES];
  w_seed_frontend_type types[RUN_TYPES];
  w_seed_frontend_function functions[RUN_FUNCTIONS];
  w_seed_frontend_parameter parameters[RUN_PARAMETERS];
  w_seed_frontend_entry entries[RUN_ENTRIES];
  w_seed_frontend_statement statements[RUN_STATEMENTS];
  w_seed_frontend_expression expressions[RUN_EXPRESSIONS];
  w_seed_frontend_argument arguments[RUN_ARGUMENTS];
  w_seed_frontend_symbol symbols[RUN_SYMBOLS];
  w_seed_frontend_fact facts[RUN_FACTS];
  w_seed_frontend_diagnostic diagnostics[RUN_DIAGNOSTICS];
  w_seed_frontend_diagnostic_fact diagnostic_facts[RUN_DIAGNOSTIC_FACTS];
  w_seed_frontend_diagnostic_item diagnostic_items[RUN_DIAGNOSTIC_ITEMS];
  w_seed_frontend_diagnostic_label diagnostic_labels[RUN_DIAGNOSTIC_LABELS];
  uint8_t const_bytes[RUN_SOURCE_LIMIT];
  uint8_t frontend_receipt[RUN_FRONTEND_RECEIPT];
  w_seed_frontend_host_requirement host_requirements[1];
  w_seed_frontend_external_parameter host_parameters[1];
  w_seed_frontend_host_prelude_symbol host_symbols[2];
  w_seed_frontend_host_prelude host_scope;
  w_seed_hir0_module hir_modules[RUN_HIR_RECORDS];
  w_seed_hir0_identity hir_identities[RUN_HIR_IDENTITIES];
  w_seed_hir0_type hir_types[RUN_HIR_RECORDS];
  w_seed_hir0_function hir_functions[RUN_HIR_RECORDS];
  w_seed_hir0_parameter hir_parameters[RUN_HIR_RECORDS];
  w_seed_hir0_block hir_blocks[RUN_HIR_RECORDS];
  w_seed_hir0_instruction hir_instructions[RUN_HIR_RECORDS];
  w_seed_hir0_binding hir_bindings[RUN_HIR_RECORDS];
  w_seed_hir0_call hir_calls[RUN_HIR_RECORDS];
  w_seed_hir0_host_parameter hir_host_parameters[RUN_HIR_RECORDS];
  w_seed_hir0_argument hir_arguments[RUN_HIR_RECORDS];
  w_seed_hir0_requirement hir_requirements[RUN_HIR_RECORDS];
  w_seed_hir0_value hir_values[RUN_HIR_RECORDS];
  w_seed_hir0_terminator hir_terminators[RUN_HIR_RECORDS];
  w_seed_hir0_entry hir_entries[RUN_HIR_RECORDS];
  uint8_t hir_text[RUN_HIR_TEXT];
  uint8_t hir_value_bytes[RUN_HIR_VALUES];
  uint8_t hir_receipt[RUN_HIR_RECEIPT];
  w_seed_hir0_output hir_output;
  w_seed_hir0_result hir_result;
  w_seed_hir0_program hir_program;
  w_seed_hlo0_plan hlo_plan;
  uint8_t hlo_receipt[RUN_HLO_RECEIPT];
} run_storage;

typedef struct {
  FILE *stream;
  const w_seed_cli_io_ops *ops;
} gate_stdout_context;

static run_storage storage;

static bool write_error(const char *message) {
  if (message == NULL ||
      fprintf(stderr, "w_seed_run0_gate: %s\n", message) < 0)
    return false;
  return fflush(stderr) == 0;
}

static int source_failure(const char *message) {
  return write_error(message) ? 2 : 3;
}

static int internal_failure(const char *message) {
  (void)write_error(message);
  return 3;
}

static bool read_source(const char *path, size_t *length) {
  if (path == NULL || length == NULL) return false;
  FILE *file = fopen(path, "rb");
  if (file == NULL) return false;
  const size_t read = fread(storage.source_bytes, 1u,
                            sizeof(storage.source_bytes), file);
  const bool read_ok = ferror(file) == 0;
  const bool close_ok = fclose(file) == 0;
  if (!read_ok || !close_ok) return false;
  if (read > (size_t)RUN_SOURCE_LIMIT) return false;
  *length = read;
  return true;
}

static void configure_host(void) {
  storage.host_requirements[0] = (w_seed_frontend_host_requirement){
      .name = (w_seed_frontend_text){"Console", 7u}};
  storage.host_parameters[0] = (w_seed_frontend_external_parameter){
      .name = (w_seed_frontend_text){"message", 7u},
      .type = (w_seed_frontend_text){"String", 6u},
      .label_kind = W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY};
  storage.host_symbols[0] = (w_seed_frontend_host_prelude_symbol){
      .name = (w_seed_frontend_text){"noop", 4u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"()", 2u},
      .is_const = false,
      .requirements = NULL,
      .requirement_count = 0u};
  storage.host_symbols[1] = (w_seed_frontend_host_prelude_symbol){
      .name = (w_seed_frontend_text){"print", 5u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .parameters = storage.host_parameters,
      .parameter_count = 1u,
      .return_type = (w_seed_frontend_text){"()", 2u},
      .is_const = false,
      .requirements = storage.host_requirements,
      .requirement_count = 1u};
  storage.host_scope = (w_seed_frontend_host_prelude){
      .profile = (w_seed_frontend_text){"native-process@1", 16u},
      .symbols = storage.host_symbols,
      .symbol_count = 2u};
  storage.frontend_input.host_scope = &storage.host_scope;
}

static bool parse_source(size_t source_length) {
  w_seed_source_error source_error;
  if (!w_seed_source_init(
          (w_seed_byte_view){storage.source_bytes, source_length},
          &storage.source, &source_error))
    return false;
  w_seed_lex_error lex_error;
  if (!w_seed_parser_init(
          &storage.source, (w_seed_span){0u, source_length},
          (w_seed_foreign_limits){65536u, 256u}, storage.lexer_frames,
          RUN_LEXER_FRAMES, storage.tokens, RUN_TOKENS, storage.nodes,
          RUN_NODES, storage.parse_frames, RUN_PARSE_FRAMES, storage.issues,
          RUN_ISSUES, &storage.parser, &lex_error))
    return false;
  if (!w_seed_parser_parse(&storage.parser, &storage.parse)) return false;
  return storage.parse.status == W_SEED_PARSE_COMPLETE &&
         storage.parse.issue_count == 0u;
}

static void configure_frontend(void) {
  storage.document = (w_seed_frontend_document){
      .logical_source_id = (w_seed_frontend_text){"run0", 4u},
      .module_id = (w_seed_frontend_text){"run0", 4u},
      .local_module_name = (w_seed_frontend_text){"run0", 4u},
      .source = &storage.source,
      .nodes = storage.nodes,
      .node_count = storage.parse.node_count,
      .parse = storage.parse};
  storage.frontend_input = (w_seed_frontend_input){
      .documents = &storage.document,
      .document_count = 1u,
      .external_modules = NULL,
      .external_module_count = 0u,
      .host_scope = NULL,
      .import_resolution_complete = false,
      .resolved_imports = NULL,
      .resolved_import_count = 0u};
  configure_host();
}

static void configure_frontend_output(void) {
  storage.frontend_output = (w_seed_frontend_output){
      .modules = storage.modules,
      .module_capacity = RUN_MODULES,
      .imports = storage.imports,
      .import_capacity = RUN_IMPORTS,
      .import_items = storage.import_items,
      .import_item_capacity = RUN_IMPORT_ITEMS,
      .structs = storage.structs,
      .struct_capacity = RUN_STRUCTS,
      .fields = storage.fields,
      .field_capacity = RUN_FIELDS,
      .type_declarations = storage.type_declarations,
      .type_declaration_capacity = RUN_TYPE_DECLARATIONS,
      .aliases = storage.aliases,
      .alias_capacity = RUN_ALIASES,
      .types = storage.types,
      .type_capacity = RUN_TYPES,
      .functions = storage.functions,
      .function_capacity = RUN_FUNCTIONS,
      .parameters = storage.parameters,
      .parameter_capacity = RUN_PARAMETERS,
      .entries = storage.entries,
      .entry_capacity = RUN_ENTRIES,
      .statements = storage.statements,
      .statement_capacity = RUN_STATEMENTS,
      .expressions = storage.expressions,
      .expression_capacity = RUN_EXPRESSIONS,
      .arguments = storage.arguments,
      .argument_capacity = RUN_ARGUMENTS,
      .symbols = storage.symbols,
      .symbol_capacity = RUN_SYMBOLS,
      .facts = storage.facts,
      .fact_capacity = RUN_FACTS,
      .diagnostics = storage.diagnostics,
      .diagnostic_capacity = RUN_DIAGNOSTICS,
      .diagnostic_facts = storage.diagnostic_facts,
      .diagnostic_fact_capacity = RUN_DIAGNOSTIC_FACTS,
      .diagnostic_items = storage.diagnostic_items,
      .diagnostic_item_capacity = RUN_DIAGNOSTIC_ITEMS,
      .diagnostic_labels = storage.diagnostic_labels,
      .diagnostic_label_capacity = RUN_DIAGNOSTIC_LABELS,
      .const_bytes = storage.const_bytes,
      .const_bytes_capacity = sizeof(storage.const_bytes),
      .receipt = storage.frontend_receipt,
      .receipt_capacity = sizeof(storage.frontend_receipt)};
}

static void configure_hir_output(void) {
  storage.hir_output = (w_seed_hir0_output){
      .modules = storage.hir_modules,
      .module_capacity = RUN_HIR_RECORDS,
      .identities = storage.hir_identities,
      .identity_capacity = RUN_HIR_IDENTITIES,
      .types = storage.hir_types,
      .type_capacity = RUN_HIR_RECORDS,
      .functions = storage.hir_functions,
      .function_capacity = RUN_HIR_RECORDS,
      .parameters = storage.hir_parameters,
      .parameter_capacity = RUN_HIR_RECORDS,
      .blocks = storage.hir_blocks,
      .block_capacity = RUN_HIR_RECORDS,
      .instructions = storage.hir_instructions,
      .instruction_capacity = RUN_HIR_RECORDS,
      .bindings = storage.hir_bindings,
      .binding_capacity = RUN_HIR_RECORDS,
      .calls = storage.hir_calls,
      .call_capacity = RUN_HIR_RECORDS,
      .host_parameters = storage.hir_host_parameters,
      .host_parameter_capacity = RUN_HIR_RECORDS,
      .arguments = storage.hir_arguments,
      .argument_capacity = RUN_HIR_RECORDS,
      .requirements = storage.hir_requirements,
      .requirement_capacity = RUN_HIR_RECORDS,
      .values = storage.hir_values,
      .value_capacity = RUN_HIR_RECORDS,
      .terminators = storage.hir_terminators,
      .terminator_capacity = RUN_HIR_RECORDS,
      .entries = storage.hir_entries,
      .entry_capacity = RUN_HIR_RECORDS,
      .text_bytes = storage.hir_text,
      .text_byte_capacity = sizeof(storage.hir_text),
      .value_bytes = storage.hir_value_bytes,
      .value_byte_capacity = sizeof(storage.hir_value_bytes),
      .receipt = storage.hir_receipt,
      .receipt_capacity = sizeof(storage.hir_receipt)};
}

static w_seed_run0_sink_result gate_stdout_sink(void *context,
                                                const uint8_t *bytes,
                                                size_t byte_count) {
  gate_stdout_context *output = (gate_stdout_context *)context;
  if (output == NULL || output->stream == NULL || output->ops == NULL)
    return (w_seed_run0_sink_result){0u,
                                     W_SEED_RUN0_FLUSH_NOT_ATTEMPTED};
  const w_seed_cli_write_result write_result =
      w_seed_cli_write_bytes_with_ops(output->stream, bytes, byte_count,
                                      output->ops);
  w_seed_run0_flush_status flush_status = W_SEED_RUN0_FLUSH_NOT_ATTEMPTED;
  if (write_result.flush_status == W_SEED_CLI_FLUSH_SUCCEEDED)
    flush_status = W_SEED_RUN0_FLUSH_SUCCEEDED;
  else if (write_result.flush_status == W_SEED_CLI_FLUSH_FAILED)
    flush_status = W_SEED_RUN0_FLUSH_FAILED;
  return (w_seed_run0_sink_result){write_result.accepted_bytes, flush_status};
}

static size_t gate_short_write(void *context, const void *bytes, size_t size,
                               size_t count, FILE *stream) {
  (void)context;
  const size_t accepted_limit = 5u;
  const size_t accepted = count < accepted_limit ? count : accepted_limit;
  return fwrite(bytes, size, accepted, stream);
}

static int gate_flush_failure(void *context, FILE *stream) {
  (void)context;
  (void)fflush(stream);
  return -1;
}

static w_seed_run0_sink_result gate_reject_sink(void *context,
                                                const uint8_t *bytes,
                                                size_t byte_count) {
  (void)context;
  (void)bytes;
  (void)byte_count;
  return (w_seed_run0_sink_result){0u,
                                   W_SEED_RUN0_FLUSH_NOT_ATTEMPTED};
}

static bool fault_is(const char *fault, const char *expected) {
  return fault != NULL && expected != NULL && strcmp(fault, expected) == 0;
}

static int run_gate(const char *path) {
  (void)memset(&storage, 0, sizeof(storage));
  size_t source_length = 0u;
  if (!read_source(path, &source_length))
    return source_failure(
        "source is missing, unreadable, empty, or over 4096 bytes");
  if (source_length == 0u)
    return source_failure(
        "source is missing, unreadable, empty, or over 4096 bytes");
  if (!parse_source(source_length))
    return source_failure("source was rejected by UTF-8 or parser validation");
  configure_frontend();
  configure_frontend_output();
  if (w_seed_frontend_run(&storage.frontend_input, &storage.frontend_output,
                          &storage.frontend_result) != W_SEED_FRONTEND_OK)
    return source_failure("source was rejected by frontend validation");

  configure_hir_output();
  const w_seed_hir0_input hir_input = {
      &storage.frontend_input, &storage.frontend_output,
      &storage.frontend_result};
  const w_seed_hir0_status hir_status = w_seed_hir0_run(
      &hir_input, &storage.hir_output, &storage.hir_result);
  if (hir_status == W_SEED_HIR0_FRONTEND ||
      hir_status == W_SEED_HIR0_UNSUPPORTED ||
      hir_status == W_SEED_HIR0_CAPACITY)
    return source_failure("source was rejected by HIR0 validation");
  if (hir_status != W_SEED_HIR0_OK)
    return internal_failure("HIR0 pipeline failed after frontend validation");
  if (!w_seed_hir0_program_from_output(&storage.hir_output,
                                        &storage.hir_result,
                                        &storage.hir_program) ||
      !w_seed_hir0_verify(&storage.hir_program, &storage.hir_result))
    return internal_failure("HIR0 verification failed after lowering");

  const w_seed_hlo0_input hlo_input = {
      &storage.hir_program, &storage.hir_result};
  w_seed_hlo0_output hlo_output = {
      &storage.hlo_plan, 1u, storage.hlo_receipt, sizeof(storage.hlo_receipt)};
  w_seed_hlo0_result hlo_result;
  const w_seed_hlo0_status hlo_status =
      w_seed_hlo0_run(&hlo_input, &hlo_output, &hlo_result);
  if (hlo_status == W_SEED_HLO0_UNSUPPORTED)
    return source_failure("source is unsupported by the HLO0 seed subset");
  if (hlo_status != W_SEED_HLO0_OK)
    return internal_failure("HLO0 pipeline failed after HIR0 verification");
  const char *fault = getenv("W_SEED_RUN0_GATE_FAULT");
  if (fault_is(fault, "hlo-forgery")) storage.hlo_plan.stdout_sha256[0] ^= 1u;
  if (!w_seed_hlo0_verify_plan(&storage.hlo_plan))
    return internal_failure("HLO0 shared verification failed after lowering");

  if (fault_is(fault, "run0-invalid")) storage.hlo_plan.payload[0] ^= 1u;
  w_seed_run0_result run_result;
  w_seed_run0_result *run_result_output = &run_result;
  if (fault_is(fault, "run0-alias"))
    run_result_output = (w_seed_run0_result *)(void *)&storage.hlo_plan;
  const w_seed_run0_sink sink = fault_is(fault, "sink-reject")
                                    ? gate_reject_sink
                                    : gate_stdout_sink;
  w_seed_cli_io_ops stdout_ops = w_seed_cli_stdio_ops;
  if (fault_is(fault, "sink-short-write"))
    stdout_ops.write_bytes = gate_short_write;
  if (fault_is(fault, "sink-flush-failure"))
    stdout_ops.flush = gate_flush_failure;
  gate_stdout_context stdout_context = {stdout, &stdout_ops};
  const w_seed_run0_status run_status =
      w_seed_run0_execute(&storage.hlo_plan, sink, &stdout_context,
                          run_result_output);
  if (run_status == W_SEED_RUN0_OK) return 0;
  if (run_status == W_SEED_RUN0_IO)
    return internal_failure("output write or flush failed");
  return internal_failure("RUN0 rejected an internally verified HLO0 plan");
}

int main(int argc, char **argv) {
  static const char usage[] = "usage: w_seed_run0_gate <path/file.w>\n";
  if (!w_seed_cli_prepare_binary(stdout, &w_seed_cli_stdio_ops)) return 3;
  if (argc != 2 || argv == NULL || argv[1] == NULL || argv[1][0] == '\0' ||
      argv[1][0] == '-') {
    return w_seed_cli_write_text(stderr, usage, &w_seed_cli_stdio_ops) ? 2 : 3;
  }
  return run_gate(argv[1]);
}
