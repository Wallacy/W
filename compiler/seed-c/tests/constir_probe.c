#include "w_seed_constir.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

enum {
  SOURCE_CAPACITY = 16 * 1024 * 1024,
  LEXER_FRAMES = 4096,
  TOKENS = 8192,
  NODES = 262144,
  PARSE_FRAMES = 16384,
  ISSUES = 4096,
  FRONTEND_MODULES = 128,
  FRONTEND_ENUMS = 4096,
  FRONTEND_ENUM_CASES = 65536,
  FRONTEND_TYPES = 65536,
  FRONTEND_FUNCTIONS = 8192,
  FRONTEND_PARAMETERS = 65536,
  FRONTEND_STATEMENTS = 65536,
  FRONTEND_EXPRESSIONS = 262144,
  FRONTEND_ARGUMENTS = 65536,
  FRONTEND_TYPED_CONST_EXPRESSIONS = 65536,
  FRONTEND_GENERIC_PARAMETERS = 65536,
  FRONTEND_GENERIC_APPLICATIONS = 65536,
  FRONTEND_GENERIC_ARGUMENTS = 65536,
  FRONTEND_CONST_VALUES = 262144,
  FRONTEND_CONST_ELEMENTS = 262144,
  FRONTEND_CONST_BYTES = 8 * 1024 * 1024,
  FRONTEND_SWITCH_ARMS = 65536,
  FRONTEND_MEMBERSHIP = 262144,
  FRONTEND_ARRAY = 262144,
  FRONTEND_CONST_DECLARATIONS = 4096,
  FRONTEND_RECEIPT = 16 * 1024 * 1024,
  CONSTIR_FUNCTIONS = 8192,
  CONSTIR_PARAMETERS = 65536,
  CONSTIR_NODES = 524288,
  CONSTIR_CALL_ARGUMENTS = 262144,
  CONSTIR_SWITCH_ARMS = 262144,
  CONSTIR_MEMBERSHIP = 524288,
  CONSTIR_STATEMENTS = 131072,
  CONSTIR_LOCALS = 16384,
  CONSTIR_DIAGNOSTICS = 8192,
  CONSTIR_RECEIPT = 32 * 1024 * 1024,
};

static uint8_t source_bytes[SOURCE_CAPACITY];
static w_seed_lexer_frame lexer_frames[LEXER_FRAMES];
static w_seed_parse_token tokens[TOKENS];
static w_seed_cst_node nodes[NODES];
static w_seed_parse_frame parse_frames[PARSE_FRAMES];
static w_seed_parse_issue issues[ISSUES];
static w_seed_frontend_module modules[FRONTEND_MODULES];
static w_seed_frontend_enum enums[FRONTEND_ENUMS];
static w_seed_frontend_enum_case enum_cases[FRONTEND_ENUM_CASES];
static w_seed_frontend_enum_case_parameter enum_case_parameters[FRONTEND_ARRAY];
static w_seed_frontend_enum_subset_member enum_subset_members[FRONTEND_ARRAY];
static w_seed_frontend_type types[FRONTEND_TYPES];
static w_seed_frontend_function functions[FRONTEND_FUNCTIONS];
static w_seed_frontend_parameter parameters[FRONTEND_PARAMETERS];
static w_seed_frontend_statement statements[FRONTEND_STATEMENTS];
static w_seed_frontend_expression expressions[FRONTEND_EXPRESSIONS];
static w_seed_frontend_argument arguments[FRONTEND_ARGUMENTS];
static w_seed_frontend_typed_const_expression
    typed_const_expressions[FRONTEND_TYPED_CONST_EXPRESSIONS];
static w_seed_frontend_generic_parameter
    generic_parameters[FRONTEND_GENERIC_PARAMETERS];
static w_seed_frontend_generic_application
    generic_applications[FRONTEND_GENERIC_APPLICATIONS];
static w_seed_frontend_generic_argument
    generic_arguments[FRONTEND_GENERIC_ARGUMENTS];
static w_seed_frontend_const_value const_values[FRONTEND_CONST_VALUES];
static w_seed_frontend_const_element const_elements[FRONTEND_CONST_ELEMENTS];
static uint8_t frontend_const_bytes[FRONTEND_CONST_BYTES];
static w_seed_frontend_switch_arm switch_arms[FRONTEND_SWITCH_ARMS];
static w_seed_frontend_enum_membership_case membership[FRONTEND_MEMBERSHIP];
static w_seed_frontend_import imports[FRONTEND_ARRAY];
static w_seed_frontend_import_item import_items[FRONTEND_ARRAY];
static w_seed_frontend_struct structs[FRONTEND_ARRAY];
static w_seed_frontend_field fields[FRONTEND_ARRAY];
static w_seed_frontend_type_declaration declarations[FRONTEND_ARRAY];
static w_seed_frontend_alias aliases[FRONTEND_ARRAY];
static w_seed_frontend_const_declaration const_declarations[
    FRONTEND_CONST_DECLARATIONS];
static w_seed_frontend_entry entries[FRONTEND_ARRAY];
static w_seed_frontend_symbol symbols[FRONTEND_ARRAY];
static w_seed_frontend_fact facts[FRONTEND_ARRAY];
static w_seed_frontend_diagnostic frontend_diagnostics[FRONTEND_ARRAY];
static uint8_t frontend_receipt[FRONTEND_RECEIPT];
static w_seed_constir_function constir_functions[CONSTIR_FUNCTIONS];
static w_seed_constir_parameter constir_parameters[CONSTIR_PARAMETERS];
static w_seed_constir_node constir_nodes[CONSTIR_NODES];
static w_seed_constir_call_argument constir_call_arguments[CONSTIR_CALL_ARGUMENTS];
static w_seed_constir_switch_arm constir_switch_arms[CONSTIR_SWITCH_ARMS];
static w_seed_constir_membership_case constir_membership[CONSTIR_MEMBERSHIP];
static w_seed_constir_statement constir_statements[CONSTIR_STATEMENTS];
static w_seed_constir_local constir_locals[CONSTIR_LOCALS];
static w_seed_constir_diagnostic constir_diagnostics[CONSTIR_DIAGNOSTICS];
static uint8_t constir_receipt[CONSTIR_RECEIPT];
static w_seed_constir_eval_frame eval_frames[64];

static const char *status_name(w_seed_constir_status status) {
  switch (status) {
    case W_SEED_CONSTIR_OK:
      return "ok";
    case W_SEED_CONSTIR_CAPACITY:
      return "capacity";
    case W_SEED_CONSTIR_INVALID:
      return "invalid";
  }
  return "unknown";
}

int main(void) {
#ifdef _WIN32
  (void)_setmode(_fileno(stdin), _O_BINARY);
#endif
  size_t length = 0;
  while (length < SOURCE_CAPACITY) {
    const size_t room = SOURCE_CAPACITY - length;
    const size_t read = fread(source_bytes + length, 1, room, stdin);
    length += read;
    if (read < room) {
      if (ferror(stdin) != 0) return 2;
      break;
    }
  }
  const w_seed_byte_view bytes = {source_bytes, length};
  w_seed_source source;
  w_seed_source_error source_error;
  if (!w_seed_source_init(bytes, &source, &source_error)) return 2;
  w_seed_parser parser;
  w_seed_lex_error lex_error;
  if (!w_seed_parser_init(
          &source, (w_seed_span){0, length}, (w_seed_foreign_limits){65536u, 256u},
          lexer_frames, LEXER_FRAMES, tokens, TOKENS, nodes, NODES, parse_frames,
          PARSE_FRAMES, issues, ISSUES, &parser, &lex_error)) return 2;
  w_seed_parse_result parse;
  if (!w_seed_parser_parse(&parser, &parse)) return 2;
  const w_seed_frontend_document document = {
      .logical_source_id = {"constir-probe", 14},
      .module_id = {"constir-probe", 14},
      .local_module_name = {"constir-probe", 14},
      .source = &source,
      .nodes = nodes,
      .node_count = parse.node_count,
      .parse = parse,
  };
  const w_seed_frontend_input frontend_input = {
      .documents = &document,
      .document_count = 1u,
      .external_modules = NULL,
      .external_module_count = 0u,
  };
  w_seed_frontend_output frontend_output = {
      .modules = modules,
      .module_capacity = FRONTEND_MODULES,
      .imports = imports,
      .import_capacity = FRONTEND_ARRAY,
      .import_items = import_items,
      .import_item_capacity = FRONTEND_ARRAY,
      .structs = structs,
      .struct_capacity = FRONTEND_ARRAY,
      .enums = enums,
      .enum_capacity = FRONTEND_ENUMS,
      .enum_cases = enum_cases,
      .enum_case_capacity = FRONTEND_ENUM_CASES,
      .enum_case_parameters = enum_case_parameters,
      .enum_case_parameter_capacity = FRONTEND_ARRAY,
      .enum_subset_members = enum_subset_members,
      .enum_subset_member_capacity = FRONTEND_ARRAY,
      .fields = fields,
      .field_capacity = FRONTEND_ARRAY,
      .type_declarations = declarations,
      .type_declaration_capacity = FRONTEND_ARRAY,
      .aliases = aliases,
      .alias_capacity = FRONTEND_ARRAY,
      .const_declarations = const_declarations,
      .const_declaration_capacity = FRONTEND_CONST_DECLARATIONS,
      .types = types,
      .type_capacity = FRONTEND_TYPES,
      .functions = functions,
      .function_capacity = FRONTEND_FUNCTIONS,
      .parameters = parameters,
      .parameter_capacity = FRONTEND_PARAMETERS,
      .arguments = arguments,
      .argument_capacity = FRONTEND_ARGUMENTS,
      .typed_const_expressions = typed_const_expressions,
      .typed_const_expression_capacity = FRONTEND_TYPED_CONST_EXPRESSIONS,
      .generic_parameters = generic_parameters,
      .generic_parameter_capacity = FRONTEND_GENERIC_PARAMETERS,
      .generic_applications = generic_applications,
      .generic_application_capacity = FRONTEND_GENERIC_APPLICATIONS,
      .generic_arguments = generic_arguments,
      .generic_argument_capacity = FRONTEND_GENERIC_ARGUMENTS,
      .const_bytes = frontend_const_bytes,
      .const_bytes_capacity = FRONTEND_CONST_BYTES,
      .const_values = const_values,
      .const_value_capacity = FRONTEND_CONST_VALUES,
      .const_elements = const_elements,
      .const_element_capacity = FRONTEND_CONST_ELEMENTS,
      .switch_arms = switch_arms,
      .switch_arm_capacity = FRONTEND_SWITCH_ARMS,
      .enum_membership_cases = membership,
      .enum_membership_case_capacity = FRONTEND_MEMBERSHIP,
      .entries = entries,
      .entry_capacity = FRONTEND_ARRAY,
      .statements = statements,
      .statement_capacity = FRONTEND_STATEMENTS,
      .expressions = expressions,
      .expression_capacity = FRONTEND_EXPRESSIONS,
      .symbols = symbols,
      .symbol_capacity = FRONTEND_ARRAY,
      .facts = facts,
      .fact_capacity = FRONTEND_ARRAY,
      .diagnostics = frontend_diagnostics,
      .diagnostic_capacity = FRONTEND_ARRAY,
      .receipt = frontend_receipt,
      .receipt_capacity = FRONTEND_RECEIPT,
  };
  w_seed_frontend_result frontend_result;
  const w_seed_frontend_status frontend_status = w_seed_frontend_run(
      &frontend_input, &frontend_output, &frontend_result);
  const w_seed_constir_input constir_input = {
      &frontend_input, &frontend_output, &frontend_result};
  w_seed_constir_counts measured;
  w_seed_constir_result constir_result;
  const w_seed_constir_status measured_status = w_seed_constir_measure(
      &constir_input, &measured, &constir_result);
  w_seed_constir_output constir_output = {
      .functions = constir_functions,
      .function_capacity = CONSTIR_FUNCTIONS,
      .parameters = constir_parameters,
      .parameter_capacity = CONSTIR_PARAMETERS,
      .nodes = constir_nodes,
      .node_capacity = CONSTIR_NODES,
      .call_arguments = constir_call_arguments,
      .call_argument_capacity = CONSTIR_CALL_ARGUMENTS,
      .switch_arms = constir_switch_arms,
      .switch_arm_capacity = CONSTIR_SWITCH_ARMS,
      .membership_cases = constir_membership,
      .membership_case_capacity = CONSTIR_MEMBERSHIP,
      .statements = constir_statements,
      .statement_capacity = CONSTIR_STATEMENTS,
      .locals = constir_locals,
      .local_capacity = CONSTIR_LOCALS,
      .diagnostics = constir_diagnostics,
      .diagnostic_capacity = CONSTIR_DIAGNOSTICS,
      .receipt = constir_receipt,
      .receipt_capacity = CONSTIR_RECEIPT,
  };
  const w_seed_constir_status run_status = w_seed_constir_run(
      &constir_input, &constir_output, &constir_result);
  (void)printf("FRONTEND status=%d parse=%d applications=%" PRIuMAX
               " typed_const_expressions=%" PRIuMAX "\n",
               (int)frontend_status, (int)parse.status,
               (uintmax_t)frontend_result.written.generic_applications,
               (uintmax_t)frontend_result.written.typed_const_expressions);
  (void)printf("CONSTIR status=%s measured=%s functions=%" PRIuMAX
               " parameters=%" PRIuMAX " nodes=%" PRIuMAX
               " calls=%" PRIuMAX " switch=%" PRIuMAX " membership=%" PRIuMAX
               " statements=%" PRIuMAX " locals=%" PRIuMAX
               " diagnostics=%" PRIuMAX " receipt=%" PRIuMAX "\n",
               status_name(run_status), status_name(measured_status),
               (uintmax_t)constir_result.written.functions,
               (uintmax_t)constir_result.written.parameters,
               (uintmax_t)constir_result.written.nodes,
               (uintmax_t)constir_result.written.call_arguments,
               (uintmax_t)constir_result.written.switch_arms,
               (uintmax_t)constir_result.written.membership_cases,
               (uintmax_t)constir_result.written.statements,
               (uintmax_t)constir_result.written.locals,
               (uintmax_t)constir_result.written.diagnostics,
               (uintmax_t)constir_result.written.receipt_bytes);
  uint8_t receipt_digest[32];
  w_seed_sha256_state receipt_state;
  w_seed_sha256_init(&receipt_state);
  if (constir_result.written.receipt_bytes != 0u)
    w_seed_sha256_update(&receipt_state, constir_receipt,
                         constir_result.written.receipt_bytes);
  w_seed_sha256_final(&receipt_state, receipt_digest);
  (void)printf("RECEIPT digest=");
  for (size_t byte = 0; byte < sizeof(receipt_digest); byte += 1)
    (void)printf("%02x", receipt_digest[byte]);
  (void)putchar('\n');
  for (size_t index = 0; index < constir_result.written.functions; index += 1) {
    (void)printf("FUNCTION origin=%d frontend=%" PRIu32
                 " typed=%" PRIu32 " lowerable=%d digest=",
                 (int)constir_functions[index].origin,
                 constir_functions[index].frontend_function,
                 constir_functions[index].typed_const_expression_index,
                 constir_functions[index].lowerable ? 1 : 0);
    for (size_t byte = 0; byte < sizeof(constir_functions[index].body_digest);
         byte += 1) (void)printf("%02x", constir_functions[index].body_digest[byte]);
    (void)printf(" nodes=%" PRIu32 "\n", constir_functions[index].node_count);
  }
  for (size_t index = 0; index < constir_result.written.diagnostics; index += 1)
    (void)printf("DIAG code=%d expr=%" PRIu32 " span=%" PRIuMAX ":%" PRIuMAX "\n",
                 (int)constir_diagnostics[index].code,
                 constir_diagnostics[index].frontend_expression,
                 (uintmax_t)constir_diagnostics[index].source_span.start_byte,
                 (uintmax_t)constir_diagnostics[index].source_span.end_byte);
  if (run_status != W_SEED_CONSTIR_OK || constir_result.written.functions == 0u)
    return 1;
  const w_seed_constir_program program = {
      .functions = constir_functions,
      .function_count = constir_result.written.functions,
      .parameters = constir_parameters,
      .parameter_count = constir_result.written.parameters,
      .nodes = constir_nodes,
      .node_count = constir_result.written.nodes,
      .call_arguments = constir_call_arguments,
      .call_argument_count = constir_result.written.call_arguments,
      .switch_arms = constir_switch_arms,
      .switch_arm_count = constir_result.written.switch_arms,
      .membership_cases = constir_membership,
      .membership_case_count = constir_result.written.membership_cases,
      .frontend_output = &frontend_output,
      .frontend_result = &frontend_result,
      .statements = constir_statements,
      .statement_count = constir_result.written.statements,
      .locals = constir_locals,
      .local_count = constir_result.written.locals};
  const w_seed_constir_function *function = &constir_functions[0];
  for (uint32_t from = 0; from < 6u; from += 1) {
    for (uint32_t to = 0; to < 6u; to += 1) {
      w_seed_constir_value args[2];
      if (!w_seed_constir_value_enum(
              constir_parameters[0].type_index, constir_parameters[0].enum_base_index,
              from, &args[0]) ||
          !w_seed_constir_value_enum(
              constir_parameters[1].type_index, constir_parameters[1].enum_base_index,
              to, &args[1])) return 1;
      w_seed_constir_value value;
      w_seed_constir_eval_result eval_result;
      const w_seed_constir_quota quota = {10000u, 0u, 32u, SIZE_MAX};
      w_seed_constir_eval_workspace workspace = {eval_frames, 64u};
      (void)w_seed_constir_evaluate(&program, 0u, args, 2u, quota, &workspace,
                                    &value, &eval_result);
      (void)printf("EVAL from=%" PRIu32 " to=%" PRIu32 " ok=%d diag=%d steps=%" PRIuMAX
                   "\n",
                   from, to, value.kind == W_SEED_CONSTIR_VALUE_BOOL &&
                               value.bool_value ? 1 : 0,
                   (int)eval_result.diagnostic,
                   (uintmax_t)eval_result.consumed_steps);
    }
  }
  if (constir_result.written.functions > 1u &&
      constir_result.written.parameters > 2u) {
    const w_seed_constir_parameter *list_parameter = &constir_parameters[2];
    const uint32_t element_type_index =
        types[list_parameter->type_index].element_type;
    static const char *const path_names[] = {
        "empty", "singleton", "default", "prefix", "cancel-accepted",
        "cancel-reserving", "cancel-preparing", "cancel-serving", "skipped",
        "reverse", "terminal-out", "duplicate"};
    static const uint32_t path_values[][6] = {
        {0u, 0u, 0u, 0u, 0u, 0u},
        {0u, 0u, 0u, 0u, 0u, 0u},
        {0u, 1u, 2u, 3u, 4u, 0u},
        {0u, 1u, 2u, 0u, 0u, 0u},
        {0u, 5u, 0u, 0u, 0u, 0u},
        {1u, 5u, 0u, 0u, 0u, 0u},
        {2u, 5u, 0u, 0u, 0u, 0u},
        {3u, 5u, 0u, 0u, 0u, 0u},
        {0u, 2u, 0u, 0u, 0u, 0u},
        {1u, 0u, 0u, 0u, 0u, 0u},
        {4u, 5u, 0u, 0u, 0u, 0u},
        {0u, 0u, 0u, 0u, 0u, 0u},
    };
    static const size_t path_lengths[] = {0u, 1u, 5u, 3u, 2u, 2u,
                                          2u, 2u, 2u, 2u, 2u, 2u};
    const size_t path_count = sizeof(path_lengths) / sizeof(path_lengths[0]);
    for (size_t path_index = 0u; path_index < path_count; path_index += 1u) {
      w_seed_constir_value elements[6];
      for (size_t element = 0u; element < path_lengths[path_index];
           element += 1u) {
        if (!w_seed_constir_value_enum(
                element_type_index, constir_parameters[0].enum_base_index,
                path_values[path_index][element], &elements[element]))
          return 1;
      }
      w_seed_constir_value list;
      if (!w_seed_constir_value_static_list(
              list_parameter->type_index, element_type_index,
              path_lengths[path_index] == 0u ? NULL : elements,
              path_lengths[path_index], &list))
        return 1;
      w_seed_constir_value value;
      w_seed_constir_eval_result eval_result;
      const w_seed_constir_quota quota = {10000u, 0u, 32u, SIZE_MAX};
      w_seed_constir_eval_workspace workspace = {eval_frames, 64u};
      const w_seed_constir_status status = w_seed_constir_evaluate(
          &program, 1u, &list, 1u, quota, &workspace, &value, &eval_result);
      (void)printf("PATH case=%s status=%d kind=%d bool=%d diag=%d steps=%" PRIuMAX
                   " heap=%" PRIuMAX "\n",
                   path_names[path_index], (int)status, (int)value.kind,
                   value.kind == W_SEED_CONSTIR_VALUE_BOOL && value.bool_value
                       ? 1
                       : 0,
                   (int)eval_result.diagnostic,
                   (uintmax_t)eval_result.consumed_steps,
                   (uintmax_t)eval_result.consumed_heap_bytes);
    }
  }
  (void)function;
  return 0;
}
