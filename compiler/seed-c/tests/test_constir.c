#include "w_seed_constir.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      (void)fprintf(stderr, "check failed: %s:%d: %s\n", __FILE__, __LINE__, \
                    #condition);                                               \
      return false;                                                             \
    }                                                                           \
  } while (0)

enum {
  SOURCE_BYTES = 32768,
  LEXER_FRAMES = 1024,
  TOKENS = 8192,
  CST_NODES = 32768,
  PARSE_FRAMES = 4096,
  ISSUES = 1024,
  ARRAY = 4096,
  TYPES = 16384,
  FUNCTIONS = 256,
  PARAMETERS = 4096,
  STATEMENTS = 8192,
  EXPRESSIONS = 32768,
  ARGUMENTS = 8192,
  SWITCH_ARMS = 8192,
  MEMBERSHIP = 32768,
  CONST_BYTES = 65536,
  FRONTEND_RECEIPT = 2 * 1024 * 1024,
  CONSTIR_FUNCTIONS = 256,
  CONSTIR_PARAMETERS = 4096,
  CONSTIR_NODES = 65536,
  CONSTIR_ARGUMENTS = 16384,
  CONSTIR_SWITCH = 16384,
  CONSTIR_MEMBERSHIP = 65536,
  CONSTIR_STATEMENTS = 8192,
  CONSTIR_LOCALS = 1024,
  CONSTIR_DIAGNOSTICS = 256,
  CONSTIR_RECEIPT = 8 * 1024 * 1024,
};

typedef struct {
  char source_bytes[SOURCE_BYTES];
  w_seed_source source;
  w_seed_lexer_frame lexer_frames[LEXER_FRAMES];
  w_seed_parse_token tokens[TOKENS];
  w_seed_cst_node cst_nodes[CST_NODES];
  w_seed_parse_frame parse_frames[PARSE_FRAMES];
  w_seed_parse_issue issues[ISSUES];
  w_seed_parser parser;
  w_seed_parse_result parse;
  w_seed_frontend_document document;
  w_seed_frontend_input frontend_input;
  w_seed_frontend_output frontend_output;
  w_seed_frontend_result frontend_result;
  w_seed_frontend_module modules[ARRAY];
  w_seed_frontend_import imports[ARRAY];
  w_seed_frontend_import_item import_items[ARRAY];
  w_seed_frontend_struct structs[ARRAY];
  w_seed_frontend_enum enums[ARRAY];
  w_seed_frontend_enum_case enum_cases[ARRAY];
  w_seed_frontend_enum_case_parameter enum_case_parameters[ARRAY];
  w_seed_frontend_enum_subset_member enum_subset_members[ARRAY];
  w_seed_frontend_field fields[ARRAY];
  w_seed_frontend_type_declaration declarations[ARRAY];
  w_seed_frontend_alias aliases[ARRAY];
  w_seed_frontend_type types[TYPES];
  w_seed_frontend_function functions[FUNCTIONS];
  w_seed_frontend_parameter parameters[PARAMETERS];
  w_seed_frontend_entry entries[ARRAY];
  w_seed_frontend_statement statements[STATEMENTS];
  w_seed_frontend_expression expressions[EXPRESSIONS];
  w_seed_frontend_argument arguments[ARGUMENTS];
  w_seed_frontend_switch_arm switch_arms[SWITCH_ARMS];
  w_seed_frontend_enum_membership_case membership[MEMBERSHIP];
  w_seed_frontend_symbol symbols[ARRAY];
  w_seed_frontend_fact facts[ARRAY];
  w_seed_frontend_diagnostic diagnostics[ARRAY];
  w_seed_frontend_external_parameter external_parameters[ARRAY];
  w_seed_frontend_external_symbol external_symbols[ARRAY];
  w_seed_frontend_external_module external_modules[ARRAY];
  uint8_t const_bytes[CONST_BYTES];
  uint8_t frontend_receipt[FRONTEND_RECEIPT];
  w_seed_constir_output constir_output;
  w_seed_constir_result constir_result;
  w_seed_constir_function constir_functions[CONSTIR_FUNCTIONS];
  w_seed_constir_parameter constir_parameters[CONSTIR_PARAMETERS];
  w_seed_constir_node constir_nodes[CONSTIR_NODES];
  w_seed_constir_call_argument constir_arguments[CONSTIR_ARGUMENTS];
  w_seed_constir_switch_arm constir_switch[CONSTIR_SWITCH];
  w_seed_constir_membership_case constir_membership[CONSTIR_MEMBERSHIP];
  w_seed_constir_statement constir_statements[CONSTIR_STATEMENTS];
  w_seed_constir_local constir_locals[CONSTIR_LOCALS];
  w_seed_constir_diagnostic constir_diagnostics[CONSTIR_DIAGNOSTICS];
  uint8_t constir_receipt[CONSTIR_RECEIPT];
} fixture;

static fixture first_fixture;
static fixture second_fixture;

static void fixture_init_output(fixture *value) {
  value->frontend_input = (w_seed_frontend_input){&value->document, 1u, NULL, 0u};
  value->frontend_output = (w_seed_frontend_output){
      .modules = value->modules,
      .module_capacity = ARRAY,
      .imports = value->imports,
      .import_capacity = ARRAY,
      .import_items = value->import_items,
      .import_item_capacity = ARRAY,
      .structs = value->structs,
      .struct_capacity = ARRAY,
      .enums = value->enums,
      .enum_capacity = ARRAY,
      .enum_cases = value->enum_cases,
      .enum_case_capacity = ARRAY,
      .enum_case_parameters = value->enum_case_parameters,
      .enum_case_parameter_capacity = ARRAY,
      .enum_subset_members = value->enum_subset_members,
      .enum_subset_member_capacity = ARRAY,
      .fields = value->fields,
      .field_capacity = ARRAY,
      .type_declarations = value->declarations,
      .type_declaration_capacity = ARRAY,
      .aliases = value->aliases,
      .alias_capacity = ARRAY,
      .types = value->types,
      .type_capacity = TYPES,
      .functions = value->functions,
      .function_capacity = FUNCTIONS,
      .parameters = value->parameters,
      .parameter_capacity = PARAMETERS,
      .arguments = value->arguments,
      .argument_capacity = ARGUMENTS,
      .switch_arms = value->switch_arms,
      .switch_arm_capacity = SWITCH_ARMS,
      .enum_membership_cases = value->membership,
      .enum_membership_case_capacity = MEMBERSHIP,
      .entries = value->entries,
      .entry_capacity = ARRAY,
      .statements = value->statements,
      .statement_capacity = STATEMENTS,
      .expressions = value->expressions,
      .expression_capacity = EXPRESSIONS,
      .const_bytes = value->const_bytes,
      .const_bytes_capacity = CONST_BYTES,
      .symbols = value->symbols,
      .symbol_capacity = ARRAY,
      .facts = value->facts,
      .fact_capacity = ARRAY,
      .diagnostics = value->diagnostics,
      .diagnostic_capacity = ARRAY,
      .receipt = value->frontend_receipt,
      .receipt_capacity = FRONTEND_RECEIPT,
  };
  value->constir_output = (w_seed_constir_output){
      .functions = value->constir_functions,
      .function_capacity = CONSTIR_FUNCTIONS,
      .parameters = value->constir_parameters,
      .parameter_capacity = CONSTIR_PARAMETERS,
      .nodes = value->constir_nodes,
      .node_capacity = CONSTIR_NODES,
      .call_arguments = value->constir_arguments,
      .call_argument_capacity = CONSTIR_ARGUMENTS,
      .switch_arms = value->constir_switch,
      .switch_arm_capacity = CONSTIR_SWITCH,
      .membership_cases = value->constir_membership,
      .membership_case_capacity = CONSTIR_MEMBERSHIP,
      .statements = value->constir_statements,
      .statement_capacity = CONSTIR_STATEMENTS,
      .locals = value->constir_locals,
      .local_capacity = CONSTIR_LOCALS,
      .diagnostics = value->constir_diagnostics,
      .diagnostic_capacity = CONSTIR_DIAGNOSTICS,
      .receipt = value->constir_receipt,
      .receipt_capacity = CONSTIR_RECEIPT,
  };
}

static bool fixture_parse(fixture *value, const char *text) {
  const size_t length = strlen(text);
  CHECK(length < sizeof(value->source_bytes));
  (void)memcpy(value->source_bytes, text, length);
  const w_seed_byte_view bytes = {(const uint8_t *)value->source_bytes, length};
  w_seed_source_error source_error;
  CHECK(w_seed_source_init(bytes, &value->source, &source_error));
  w_seed_lex_error lex_error;
  CHECK(w_seed_parser_init(
      &value->source, (w_seed_span){0, length}, (w_seed_foreign_limits){65536u, 256u},
      value->lexer_frames, LEXER_FRAMES, value->tokens, TOKENS,
      value->cst_nodes, CST_NODES, value->parse_frames, PARSE_FRAMES,
      value->issues, ISSUES, &value->parser, &lex_error));
  CHECK(w_seed_parser_parse(&value->parser, &value->parse));
  value->document = (w_seed_frontend_document){
      {"test", 4}, {"test", 4}, &value->source, value->cst_nodes,
      value->parse.node_count, value->parse};
  fixture_init_output(value);
  CHECK(w_seed_frontend_run(&value->frontend_input, &value->frontend_output,
                            &value->frontend_result) == W_SEED_FRONTEND_OK ||
        value->frontend_result.status == W_SEED_FRONTEND_UNSUPPORTED ||
        value->frontend_result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  return true;
}

static bool fixture_lower(fixture *value, const char *text) {
  CHECK(fixture_parse(value, text));
  const w_seed_constir_input input = {
      &value->frontend_input, &value->frontend_output, &value->frontend_result};
  CHECK(w_seed_constir_run(&input, &value->constir_output,
                           &value->constir_result) == W_SEED_CONSTIR_OK);
  return true;
}

static w_seed_constir_program fixture_program(const fixture *value) {
  return (w_seed_constir_program){
      .functions = value->constir_functions,
      .function_count = value->constir_result.written.functions,
      .parameters = value->constir_parameters,
      .parameter_count = value->constir_result.written.parameters,
      .nodes = value->constir_nodes,
      .node_count = value->constir_result.written.nodes,
      .call_arguments = value->constir_arguments,
      .call_argument_count = value->constir_result.written.call_arguments,
      .switch_arms = value->constir_switch,
      .switch_arm_count = value->constir_result.written.switch_arms,
      .membership_cases = value->constir_membership,
      .membership_case_count = value->constir_result.written.membership_cases,
      .frontend_output = &value->frontend_output,
      .frontend_result = &value->frontend_result,
      .statements = value->constir_statements,
      .statement_count = value->constir_result.written.statements,
      .locals = value->constir_locals,
      .local_count = value->constir_result.written.locals};
}

static bool make_repeated_string_source(char *destination, size_t capacity,
                                        const char *prefix, size_t count,
                                        const char *suffix) {
  if (destination == NULL || prefix == NULL || suffix == NULL) return false;
  const size_t prefix_length = strlen(prefix);
  const size_t suffix_length = strlen(suffix);
  if (prefix_length > capacity || count > capacity - prefix_length ||
      suffix_length > capacity - prefix_length - count ||
      prefix_length + count + suffix_length + 1u > capacity)
    return false;
  (void)memcpy(destination, prefix, prefix_length);
  (void)memset(destination + prefix_length, 'x', count);
  (void)memcpy(destination + prefix_length + count, suffix, suffix_length);
  destination[prefix_length + count + suffix_length] = '\0';
  return true;
}

static bool evaluate_enum(const fixture *value, uint32_t from, uint32_t to,
                          w_seed_constir_value *result,
                          w_seed_constir_eval_result *eval_result,
                          size_t steps, size_t call_depth, size_t result_bytes) {
  CHECK(value->constir_result.written.parameters == 2u);
  w_seed_constir_value arguments[2];
  CHECK(w_seed_constir_value_enum(
      value->constir_parameters[0].type_index,
      value->constir_parameters[0].enum_base_index, from, &arguments[0]));
  CHECK(w_seed_constir_value_enum(
      value->constir_parameters[1].type_index,
      value->constir_parameters[1].enum_base_index, to, &arguments[1]));
  w_seed_constir_eval_frame frames[32];
  w_seed_constir_eval_workspace workspace = {frames, 32u};
  const w_seed_constir_quota quota = {steps, 0u, call_depth, result_bytes};
  const w_seed_constir_program program = fixture_program(value);
  CHECK(w_seed_constir_evaluate(&program, 0u, arguments, 2u,
                                quota, &workspace, result, eval_result) ==
        W_SEED_CONSTIR_OK);
  return true;
}

static bool test_can_move_and_digest(void) {
  static const char source[] =
      "enum ServiceStage { accepted reserving preparing serving completed cancelled }\n"
      "export const fn canMove(from current: ServiceStage, to next: ServiceStage): Bool { "
      "return switch current { case .accepted: next in (.reserving, .cancelled) "
      "case .reserving: next in (.preparing, .cancelled) "
      "case .preparing: next in (.serving, .cancelled) "
      "case .serving: next in (.completed, .cancelled) "
      "case .completed: false case .cancelled: false } }\n";
  CHECK(fixture_lower(&first_fixture, source));
  CHECK(first_fixture.constir_result.written.functions == 1u);
  CHECK(first_fixture.constir_result.written.nodes != 0u);
  CHECK(first_fixture.constir_result.written.switch_arms == 6u);
  CHECK(first_fixture.constir_result.written.membership_cases == 8u);
  const bool expected[6][6] = {
      {false, true, false, false, false, true},
      {false, false, true, false, false, true},
      {false, false, false, true, false, true},
      {false, false, false, false, true, true},
      {false, false, false, false, false, false},
      {false, false, false, false, false, false},
  };
  for (uint32_t from = 0; from < 6u; from += 1) {
    for (uint32_t to = 0; to < 6u; to += 1) {
      w_seed_constir_value value;
      w_seed_constir_eval_result result;
      CHECK(evaluate_enum(&first_fixture, from, to, &value, &result, 1000u,
                          16u, SIZE_MAX));
      CHECK(result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE);
      CHECK(value.kind == W_SEED_CONSTIR_VALUE_BOOL &&
            value.bool_value == expected[from][to]);
    }
  }
  CHECK(fixture_lower(&second_fixture, source));
  CHECK(first_fixture.constir_result.written.receipt_bytes ==
        second_fixture.constir_result.written.receipt_bytes);
  CHECK(memcmp(first_fixture.constir_receipt, second_fixture.constir_receipt,
               first_fixture.constir_result.written.receipt_bytes) == 0);
  CHECK(memcmp(first_fixture.constir_functions[0].body_digest,
               second_fixture.constir_functions[0].body_digest, 32u) == 0);
  const w_seed_constir_program program = fixture_program(&first_fixture);
  w_seed_constir_value wrong_arguments[2];
  CHECK(w_seed_constir_value_enum(
      first_fixture.constir_parameters[0].type_index,
      first_fixture.constir_parameters[0].enum_base_index, 99u,
      &wrong_arguments[0]));
  CHECK(w_seed_constir_value_enum(
      first_fixture.constir_parameters[1].type_index,
      first_fixture.constir_parameters[1].enum_base_index, 0u,
      &wrong_arguments[1]));
  w_seed_constir_value invalid_value;
  w_seed_constir_eval_result invalid_result;
  w_seed_constir_eval_frame invalid_frames[4];
  w_seed_constir_eval_workspace invalid_workspace = {invalid_frames, 4u};
  CHECK(w_seed_constir_evaluate(
            &program, 0u, wrong_arguments, 2u,
            (w_seed_constir_quota){100u, 0u, 8u, SIZE_MAX},
            &invalid_workspace, &invalid_value, &invalid_result) ==
        W_SEED_CONSTIR_INVALID &&
        invalid_result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        invalid_result.consumed_steps == 0u);
  CHECK(w_seed_constir_evaluate(
            &program, 0u, wrong_arguments, 1u,
            (w_seed_constir_quota){100u, 0u, 8u, SIZE_MAX},
            &invalid_workspace, &invalid_value, &invalid_result) ==
        W_SEED_CONSTIR_INVALID &&
        invalid_result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE);
  CHECK(w_seed_constir_value_bool(first_fixture.constir_parameters[0].type_index,
                                  true, &wrong_arguments[0]));
  CHECK(w_seed_constir_evaluate(
            &program, 0u, wrong_arguments, 2u,
            (w_seed_constir_quota){100u, 0u, 8u, SIZE_MAX},
            &invalid_workspace, &invalid_value, &invalid_result) ==
        W_SEED_CONSTIR_INVALID &&
        invalid_result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE);
  return true;
}

static bool test_static_list_stage_path(void) {
  static const char source[] =
      "enum ServiceStage { accepted reserving preparing serving completed cancelled }\n"
      "const fn canMove(from current: ServiceStage, to next: ServiceStage): Bool { "
      "return switch current { case .accepted: next in (.reserving, .cancelled) "
      "case .reserving: next in (.preparing, .cancelled) "
      "case .preparing: next in (.serving, .cancelled) "
      "case .serving: next in (.completed, .cancelled) "
      "case .completed: false case .cancelled: false } }\n"
      "const fn isValidStagePath(stages: StaticList<ServiceStage>): Bool { "
      "guard stages.count > 0 else return false "
      "for index in 1..<stages.count { if !canMove(from: stages[index - 1], "
      "to: stages[index]) { return false } } return true }\n"
      "const fn firstStage(stages: StaticList<ServiceStage>, index: usize): Bool {\n"
      "return stages[index] == stages[index]\n}\n";
  fixture *value = &first_fixture;
  CHECK(fixture_lower(value, source));
  CHECK(value->constir_result.written.functions == 3u &&
        value->constir_result.written.parameters == 5u &&
        value->constir_result.written.statements != 0u &&
        value->constir_result.written.locals == 1u);
  const w_seed_constir_program program = fixture_program(value);
  const w_seed_constir_parameter *list_parameter = &value->constir_parameters[2];
  const w_seed_constir_parameter *bounds_list_parameter =
      &value->constir_parameters[3];
  const w_seed_constir_parameter *index_parameter =
      &value->constir_parameters[4];
  CHECK(list_parameter->type_kind == W_SEED_FRONTEND_TYPE_STATIC_LIST &&
        list_parameter->type_index < value->frontend_result.written.types);
  const w_seed_frontend_type *list_type =
      &value->types[list_parameter->type_index];
  CHECK(list_type->kind == W_SEED_FRONTEND_TYPE_STATIC_LIST &&
        list_type->element_type != W_SEED_FRONTEND_NONE &&
        list_type->element_type < value->frontend_result.written.types);
  const uint32_t element_type_index = list_type->element_type;
  const uint32_t bounds_element_type_index =
      value->types[bounds_list_parameter->type_index].element_type;
  const uint32_t enum_base = value->constir_parameters[0].enum_base_index;
  static const uint32_t paths[][6] = {
      {0u, 0u, 0u, 0u, 0u, 0u}, {0u, 0u, 0u, 0u, 0u, 0u},
      {0u, 1u, 2u, 3u, 4u, 0u}, {0u, 1u, 2u, 0u, 0u, 0u},
      {0u, 5u, 0u, 0u, 0u, 0u}, {1u, 5u, 0u, 0u, 0u, 0u},
      {2u, 5u, 0u, 0u, 0u, 0u}, {3u, 5u, 0u, 0u, 0u, 0u},
      {0u, 2u, 0u, 0u, 0u, 0u}, {1u, 0u, 0u, 0u, 0u, 0u},
      {4u, 5u, 0u, 0u, 0u, 0u}, {0u, 0u, 0u, 0u, 0u, 0u}};
  static const size_t lengths[] = {0u, 1u, 5u, 3u, 2u, 2u,
                                   2u, 2u, 2u, 2u, 2u, 2u};
  static const bool expected[] = {false, true, true, true, true, true,
                                  true,  true, false, false, false, false};
  for (size_t path = 0u; path < sizeof(lengths) / sizeof(lengths[0]);
       path += 1u) {
    w_seed_constir_value elements[6];
    for (size_t index = 0u; index < lengths[path]; index += 1u)
      CHECK(w_seed_constir_value_enum(element_type_index, enum_base,
                                      paths[path][index], &elements[index]));
    w_seed_constir_value list;
    CHECK(w_seed_constir_value_static_list(
        list_parameter->type_index, element_type_index,
        lengths[path] == 0u ? NULL : elements, lengths[path], &list));
    w_seed_constir_value result_value;
    w_seed_constir_eval_result result;
    w_seed_constir_eval_frame frames[16];
    w_seed_constir_eval_workspace workspace = {frames, 16u};
    CHECK(w_seed_constir_evaluate(
              &program, 1u, &list, 1u,
              (w_seed_constir_quota){10000u, 0u, 32u, SIZE_MAX}, &workspace,
              &result_value, &result) == W_SEED_CONSTIR_OK &&
          result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
          result.consumed_heap_bytes == 0u &&
          result_value.kind == W_SEED_CONSTIR_VALUE_BOOL &&
          result_value.bool_value == expected[path]);
  }

  /* Invalid arity, scalar, wrong enum base, and a non-empty NULL list must
   * reject before execution and leave no result value. */
  w_seed_constir_value result_value;
  w_seed_constir_eval_result result;
  w_seed_constir_eval_frame frames[4];
  w_seed_constir_eval_workspace workspace = {frames, 4u};
  CHECK(w_seed_constir_evaluate(
            &program, 1u, NULL, 0u,
            (w_seed_constir_quota){100u, 0u, 32u, SIZE_MAX}, &workspace,
            &result_value, &result) == W_SEED_CONSTIR_INVALID &&
        result_value.kind == W_SEED_CONSTIR_VALUE_INVALID &&
        result.consumed_steps == 0u);
  w_seed_constir_value scalar;
  CHECK(w_seed_constir_value_bool(list_parameter->type_index, true, &scalar));
  CHECK(w_seed_constir_evaluate(
            &program, 1u, &scalar, 1u,
            (w_seed_constir_quota){100u, 0u, 32u, SIZE_MAX}, &workspace,
            &result_value, &result) == W_SEED_CONSTIR_INVALID &&
        result_value.kind == W_SEED_CONSTIR_VALUE_INVALID &&
        result.consumed_steps == 0u);
  w_seed_constir_value wrong_element;
  CHECK(w_seed_constir_value_enum(element_type_index, enum_base + 1u, 0u,
                                  &wrong_element));
  w_seed_constir_value wrong_base_list;
  CHECK(w_seed_constir_value_static_list(
      list_parameter->type_index, element_type_index, &wrong_element, 1u,
      &wrong_base_list));
  CHECK(w_seed_constir_evaluate(
            &program, 1u, &wrong_base_list, 1u,
            (w_seed_constir_quota){100u, 0u, 32u, SIZE_MAX}, &workspace,
            &result_value, &result) == W_SEED_CONSTIR_INVALID &&
        result_value.kind == W_SEED_CONSTIR_VALUE_INVALID &&
        result.consumed_steps == 0u);
  w_seed_constir_value null_nonempty;
  CHECK(w_seed_constir_value_static_list(
      list_parameter->type_index, element_type_index, &wrong_element, 1u,
      &null_nonempty));
  null_nonempty.elements = NULL;
  CHECK(w_seed_constir_evaluate(
            &program, 1u, &null_nonempty, 1u,
            (w_seed_constir_quota){100u, 0u, 32u, SIZE_MAX}, &workspace,
            &result_value, &result) == W_SEED_CONSTIR_INVALID &&
        result_value.kind == W_SEED_CONSTIR_VALUE_INVALID &&
        result.consumed_steps == 0u);

  /* An empty list is valid input, but indexing it is a deterministic bounds
   * fault.  A small step quota must also produce W-CONST-0003 repeatedly. */
  w_seed_constir_value empty;
  CHECK(w_seed_constir_value_static_list(
      bounds_list_parameter->type_index, bounds_element_type_index, NULL, 0u,
      &empty));
  uint8_t zero_index[W_SEED_CONSTIR_INTEGER_BYTES] = {0u};
  w_seed_constir_value bounds_index;
  CHECK(w_seed_constir_value_integer(
      index_parameter->type_index, index_parameter->type_kind,
      index_parameter->type_is_signed, index_parameter->type_bit_width,
      zero_index, &bounds_index));
  w_seed_constir_value bounds_arguments[2] = {empty, bounds_index};
  const w_seed_constir_status bounds_status = w_seed_constir_evaluate(
      &program, 2u, bounds_arguments, 2u,
      (w_seed_constir_quota){100u, 0u, 32u, SIZE_MAX}, &workspace,
      &result_value, &result);
  CHECK(bounds_status == W_SEED_CONSTIR_OK &&
        result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0006 &&
        result_value.kind == W_SEED_CONSTIR_VALUE_INVALID);
  enum { large_count = 256u };
  w_seed_constir_value elements[large_count];
  for (uint32_t index = 0u; index < large_count; index += 1u)
    CHECK(w_seed_constir_value_enum(element_type_index, enum_base, index % 6u,
                                    &elements[index]));
  w_seed_constir_value large_list;
  CHECK(w_seed_constir_value_static_list(
      list_parameter->type_index, element_type_index, elements, large_count,
      &large_list));
  /* The caller-owned validation scan has a deterministic D1 ceiling that is
   * independent of the execution step quota. */
  w_seed_constir_value over_limit = large_list;
  over_limit.element_count = W_SEED_CONSTIR_MAX_STATIC_LIST_ELEMENTS + 1u;
  CHECK(w_seed_constir_evaluate(
            &program, 1u, &over_limit, 1u,
            (w_seed_constir_quota){100u, 0u, 32u, SIZE_MAX}, &workspace,
            &result_value, &result) == W_SEED_CONSTIR_INVALID &&
        result_value.kind == W_SEED_CONSTIR_VALUE_INVALID &&
        result.consumed_steps == 0u);
  w_seed_constir_eval_result quota_first;
  w_seed_constir_eval_result quota_second;
  CHECK(w_seed_constir_evaluate(
            &program, 1u, &large_list, 1u,
            (w_seed_constir_quota){10u, 0u, 32u, SIZE_MAX}, &workspace,
            &result_value, &quota_first) == W_SEED_CONSTIR_OK &&
        quota_first.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003 &&
        result_value.kind == W_SEED_CONSTIR_VALUE_INVALID);
  CHECK(w_seed_constir_evaluate(
            &program, 1u, &large_list, 1u,
            (w_seed_constir_quota){10u, 0u, 32u, SIZE_MAX}, &workspace,
            &result_value, &quota_second) == W_SEED_CONSTIR_OK &&
        quota_second.diagnostic == quota_first.diagnostic &&
        quota_second.consumed_steps == quota_first.consumed_steps &&
        quota_second.consumed_heap_bytes == 0u);
  CHECK(w_seed_constir_evaluate(
            &program, 1u, &large_list, 1u,
            (w_seed_constir_quota){10000u, 0u, 32u, SIZE_MAX}, NULL,
            &result_value, &result) == W_SEED_CONSTIR_INVALID &&
        result_value.kind == W_SEED_CONSTIR_VALUE_INVALID &&
        result.consumed_steps == 0u);

  /* The statement tree is mutable caller-owned IR.  Wrong child counts,
   * cycles, and out-of-range local ordinals must fail before any step. */
  w_seed_constir_function *path_function = &value->constir_functions[1];
  uint32_t for_statement = W_SEED_CONSTIR_NONE;
  for (uint32_t offset = 0u; offset < path_function->statement_count;
       offset += 1u) {
    if (value->constir_statements[path_function->first_statement + offset].kind ==
        W_SEED_CONSTIR_STATEMENT_FOR_RANGE) {
      for_statement = path_function->first_statement + offset;
      break;
    }
  }
  CHECK(for_statement != W_SEED_CONSTIR_NONE);
  w_seed_constir_statement saved_statement = value->constir_statements[for_statement];
  value->constir_statements[for_statement].child_count += 1u;
  CHECK(w_seed_constir_evaluate(
            &program, 1u, &large_list, 1u,
            (w_seed_constir_quota){100u, 0u, 32u, SIZE_MAX}, &workspace,
            &result_value, &result) == W_SEED_CONSTIR_INVALID &&
        result.consumed_steps == 0u);
  value->constir_statements[for_statement] = saved_statement;
  value->constir_statements[for_statement].local_ordinal = 1u;
  CHECK(w_seed_constir_evaluate(
            &program, 1u, &large_list, 1u,
            (w_seed_constir_quota){100u, 0u, 32u, SIZE_MAX}, &workspace,
            &result_value, &result) == W_SEED_CONSTIR_INVALID &&
        result.consumed_steps == 0u);
  value->constir_statements[for_statement] = saved_statement;
  uint32_t bool_node = W_SEED_CONSTIR_NONE;
  for (uint32_t offset = 0u; offset < path_function->node_count;
       offset += 1u) {
    const w_seed_constir_node *node =
        &value->constir_nodes[path_function->first_node + offset];
    if (node->type_kind == W_SEED_FRONTEND_TYPE_BOOL) {
      bool_node = path_function->first_node + offset;
      break;
    }
  }
  CHECK(bool_node != W_SEED_CONSTIR_NONE);
  value->constir_statements[for_statement].lower_node = bool_node;
  CHECK(w_seed_constir_evaluate(
            &program, 1u, &large_list, 1u,
            (w_seed_constir_quota){100u, 0u, 32u, SIZE_MAX}, &workspace,
            &result_value, &result) == W_SEED_CONSTIR_INVALID &&
        result.consumed_steps == 0u);
  value->constir_statements[for_statement] = saved_statement;
  w_seed_constir_local *path_local =
      &value->constir_locals[path_function->first_local];
  w_seed_constir_local saved_local = *path_local;
  path_local->ordinal = 1u;
  CHECK(w_seed_constir_evaluate(
            &program, 1u, &large_list, 1u,
            (w_seed_constir_quota){100u, 0u, 32u, SIZE_MAX}, &workspace,
            &result_value, &result) == W_SEED_CONSTIR_INVALID &&
        result.consumed_steps == 0u);
  *path_local = saved_local;
  path_local->type_bit_width += 1u;
  CHECK(w_seed_constir_evaluate(
            &program, 1u, &large_list, 1u,
            (w_seed_constir_quota){100u, 0u, 32u, SIZE_MAX}, &workspace,
            &result_value, &result) == W_SEED_CONSTIR_INVALID &&
        result.consumed_steps == 0u);
  *path_local = saved_local;
  path_local->owner_function = 0u;
  CHECK(w_seed_constir_evaluate(
            &program, 1u, &large_list, 1u,
            (w_seed_constir_quota){100u, 0u, 32u, SIZE_MAX}, &workspace,
            &result_value, &result) == W_SEED_CONSTIR_INVALID &&
        result.consumed_steps == 0u);
  *path_local = saved_local;
  uint32_t return_statement = W_SEED_CONSTIR_NONE;
  for (uint32_t offset = 0u; offset < path_function->statement_count;
       offset += 1u) {
    const w_seed_constir_statement *statement =
        &value->constir_statements[path_function->first_statement + offset];
    if (statement->kind == W_SEED_CONSTIR_STATEMENT_RETURN) {
      return_statement = path_function->first_statement + offset;
      break;
    }
  }
  CHECK(return_statement != W_SEED_CONSTIR_NONE);
  w_seed_constir_statement saved_return_statement =
      value->constir_statements[return_statement];
  value->constir_statements[return_statement].condition_node = bool_node;
  CHECK(w_seed_constir_evaluate(
            &program, 1u, &large_list, 1u,
            (w_seed_constir_quota){100u, 0u, 32u, SIZE_MAX}, &workspace,
            &result_value, &result) == W_SEED_CONSTIR_INVALID &&
        result.consumed_steps == 0u);
  value->constir_statements[return_statement] = saved_return_statement;
  value->constir_statements[for_statement].next_sibling = for_statement;
  CHECK(w_seed_constir_evaluate(
            &program, 1u, &large_list, 1u,
            (w_seed_constir_quota){100u, 0u, 32u, SIZE_MAX}, &workspace,
            &result_value, &result) == W_SEED_CONSTIR_INVALID &&
        result.consumed_steps == 0u);
  value->constir_statements[for_statement] = saved_statement;
  return true;
}

static bool test_diagnostics_and_quotas(void) {
  static const char arithmetic[] =
      "const fn overflow(value: u8): u8 { return value + 1_u8 }\n"
      "const fn divide(): u8 { return 1_u8 / 0_u8 }\n"
      "const fn short(): Bool { return false && (1_u8 / 0_u8 == 0_u8) }\n"
      "const fn scalar(): Bool { return true }\n";
  fixture *value = &first_fixture;
  CHECK(fixture_lower(value, arithmetic));
  CHECK(value->constir_result.written.functions == 4u);
  w_seed_constir_program program = fixture_program(value);
  uint8_t one[W_SEED_CONSTIR_INTEGER_BYTES] = {1u};
  w_seed_constir_value argument;
  CHECK(w_seed_constir_value_integer(value->constir_parameters[0].type_index,
                                     W_SEED_FRONTEND_TYPE_INTEGER, false, 8u,
                                     one, &argument));
  w_seed_constir_value result_value;
  w_seed_constir_eval_result result;
  w_seed_constir_eval_frame frames[8];
  w_seed_constir_eval_workspace workspace = {frames, 8u};
  argument.integer_value[0] = 255u;
  const w_seed_constir_status overflow_status = w_seed_constir_evaluate(&program, 0u, &argument, 1u,
                                (w_seed_constir_quota){100u, 0u, 8u, SIZE_MAX},
                                &workspace, &result_value, &result);
  CHECK(overflow_status == W_SEED_CONSTIR_OK);
  CHECK(result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0006 &&
        result_value.kind == W_SEED_CONSTIR_VALUE_INVALID);
  CHECK(w_seed_constir_evaluate(&program, 1u, NULL, 0u,
                                (w_seed_constir_quota){100u, 0u, 8u, SIZE_MAX},
                                &workspace, &result_value, &result) ==
        W_SEED_CONSTIR_OK);
  CHECK(result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0006 &&
        result_value.kind == W_SEED_CONSTIR_VALUE_INVALID);
  CHECK(w_seed_constir_evaluate(
      &program, 2u, NULL, 0u,
      (w_seed_constir_quota){100u, 0u, 8u, SIZE_MAX}, &workspace,
      &result_value, &result) == W_SEED_CONSTIR_OK);
  CHECK(result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        !result_value.bool_value);
  CHECK(w_seed_constir_evaluate(&program, 3u, NULL, 0u,
                                (w_seed_constir_quota){100u, 0u, 8u, 0u},
                                &workspace, &result_value, &result) ==
        W_SEED_CONSTIR_OK);
  CHECK(result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003 &&
        result.quota_limit == 0u && result.consumed_heap_bytes == 0u &&
        result.consumed_result_bytes == 19u &&
        result_value.kind == W_SEED_CONSTIR_VALUE_INVALID);
  CHECK(w_seed_constir_evaluate(&program, 3u, NULL, 0u,
                                (w_seed_constir_quota){100u, 0u, 8u, SIZE_MAX},
                                &workspace, &result_value, &result) ==
        W_SEED_CONSTIR_OK && result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE);
  return true;
}

static bool test_labels_relations_and_parentheses(void) {
  static const char labels[] =
      "enum Stage { accepted reserving }\n"
      "const fn canMove(from current: Stage, to next: Stage): Bool { "
      "return current == next }\n"
      "const fn relay(at index: Stage, to other: Stage): Bool { "
      "return canMove(from: index, to: other) }\n";
  fixture *value = &first_fixture;
  CHECK(fixture_lower(value, labels));
  CHECK(value->constir_result.written.functions == 2u);
  CHECK(value->frontend_output.parameters[0].name.length == 7u &&
        memcmp(value->frontend_output.parameters[0].name.data, "current", 7u) == 0 &&
        value->frontend_output.parameters[0].label.length == 4u &&
        memcmp(value->frontend_output.parameters[0].label.data, "from", 4u) == 0);
  CHECK(value->frontend_output.parameters[1].name.length == 4u &&
        memcmp(value->frontend_output.parameters[1].name.data, "next", 4u) == 0 &&
        value->frontend_output.parameters[1].label.length == 2u &&
        memcmp(value->frontend_output.parameters[1].label.data, "to", 2u) == 0);
  CHECK(value->frontend_output.parameters[2].name.length == 5u &&
        memcmp(value->frontend_output.parameters[2].name.data, "index", 5u) == 0 &&
        value->frontend_output.parameters[2].label.length == 2u &&
        memcmp(value->frontend_output.parameters[2].label.data, "at", 2u) == 0);
  CHECK(value->constir_result.written.call_arguments == 2u);
  const w_seed_constir_program program = fixture_program(value);
  w_seed_constir_value arguments[2];
  CHECK(w_seed_constir_value_enum(value->constir_parameters[2].type_index,
                                  value->constir_parameters[2].enum_base_index,
                                  0u, &arguments[0]));
  CHECK(w_seed_constir_value_enum(value->constir_parameters[3].type_index,
                                  value->constir_parameters[3].enum_base_index,
                                  0u, &arguments[1]));
  w_seed_constir_eval_frame frames[4];
  w_seed_constir_eval_workspace workspace = {frames, 4u};
  w_seed_constir_value output;
  w_seed_constir_eval_result evaluation;
  CHECK(w_seed_constir_evaluate(
            &program, 1u, arguments, 2u,
            (w_seed_constir_quota){100u, 0u, 2u, SIZE_MAX}, &workspace, &output,
            &evaluation) == W_SEED_CONSTIR_OK &&
        evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        output.kind == W_SEED_CONSTIR_VALUE_BOOL && output.bool_value);

  static const char relations[] =
      "const fn less(value: u8): Bool { return value < 2_u8 }\n"
      "const fn lessEqual(value: u8): Bool { return value <= 2_u8 }\n"
      "const fn greater(value: u8): Bool { return value > 2_u8 }\n"
      "const fn greaterEqual(value: u8): Bool { return value >= 2_u8 }\n";
  CHECK(fixture_lower(value, relations));
  CHECK(value->constir_result.written.functions == 4u);
  uint8_t one[W_SEED_CONSTIR_INTEGER_BYTES] = {1u};
  w_seed_constir_value one_value;
  CHECK(w_seed_constir_value_integer(
      value->constir_parameters[0].type_index, W_SEED_FRONTEND_TYPE_INTEGER,
      false, 8u, one, &one_value));
  for (uint32_t function_index = 0; function_index < 4u; function_index += 1u) {
    CHECK(w_seed_constir_evaluate(
              &(w_seed_constir_program){
                  value->constir_functions, value->constir_result.written.functions,
                  value->constir_parameters, value->constir_result.written.parameters,
                  value->constir_nodes, value->constir_result.written.nodes,
                  value->constir_arguments, value->constir_result.written.call_arguments,
                  value->constir_switch, value->constir_result.written.switch_arms,
                  value->constir_membership,
                  value->constir_result.written.membership_cases,
                  &value->frontend_output, &value->frontend_result, NULL, 0u,
                  NULL, 0u},
              function_index, &one_value, 1u,
              (w_seed_constir_quota){100u, 0u, 1u, SIZE_MAX}, &workspace, &output,
              &evaluation) == W_SEED_CONSTIR_OK &&
          evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
          output.kind == W_SEED_CONSTIR_VALUE_BOOL &&
          output.bool_value == (function_index == 0u || function_index == 1u));
  }

  static const char mixed_width[] =
      "const fn mixedLess(left: i8, right: i16): Bool { return left < right }\n"
      "const fn mixedEqual(left: u8, right: u16): Bool { return left == right }\n"
      "const fn mixedAdd(left: u8, right: u16): u16 { return left + right }\n";
  CHECK(fixture_lower(value, mixed_width));
  CHECK(value->constir_result.written.functions == 3u &&
        value->constir_result.written.parameters == 6u);
  uint8_t minus_one[W_SEED_CONSTIR_INTEGER_BYTES];
  (void)memset(minus_one, 0xff, sizeof(minus_one));
  uint8_t one_twenty_eight[W_SEED_CONSTIR_INTEGER_BYTES] = {0x80u};
  w_seed_constir_value mixed_arguments[2];
  CHECK(w_seed_constir_value_integer(
      value->constir_parameters[0].type_index, W_SEED_FRONTEND_TYPE_INTEGER,
      true, 8u, minus_one, &mixed_arguments[0]));
  CHECK(w_seed_constir_value_integer(
      value->constir_parameters[1].type_index, W_SEED_FRONTEND_TYPE_INTEGER,
      true, 16u, one_twenty_eight, &mixed_arguments[1]));
  CHECK(w_seed_constir_evaluate(
            &(w_seed_constir_program){
                value->constir_functions, value->constir_result.written.functions,
                value->constir_parameters, value->constir_result.written.parameters,
                value->constir_nodes, value->constir_result.written.nodes,
                value->constir_arguments, value->constir_result.written.call_arguments,
                value->constir_switch, value->constir_result.written.switch_arms,
                value->constir_membership,
                value->constir_result.written.membership_cases,
                &value->frontend_output, &value->frontend_result, NULL, 0u,
                NULL, 0u},
            0u, mixed_arguments, 2u,
            (w_seed_constir_quota){100u, 0u, 1u, SIZE_MAX}, &workspace, &output,
            &evaluation) == W_SEED_CONSTIR_OK &&
        evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        output.kind == W_SEED_CONSTIR_VALUE_BOOL && output.bool_value);
  uint8_t two_fifty_five[W_SEED_CONSTIR_INTEGER_BYTES] = {0xffu};
  uint8_t two_fifty_six[W_SEED_CONSTIR_INTEGER_BYTES] = {0x00u, 0x01u};
  CHECK(w_seed_constir_value_integer(
      value->constir_parameters[2].type_index, W_SEED_FRONTEND_TYPE_INTEGER,
      false, 8u, two_fifty_five, &mixed_arguments[0]));
  CHECK(w_seed_constir_value_integer(
      value->constir_parameters[3].type_index, W_SEED_FRONTEND_TYPE_INTEGER,
      false, 16u, two_fifty_five, &mixed_arguments[1]));
  CHECK(w_seed_constir_evaluate(
            &(w_seed_constir_program){
                value->constir_functions, value->constir_result.written.functions,
                value->constir_parameters, value->constir_result.written.parameters,
                value->constir_nodes, value->constir_result.written.nodes,
                value->constir_arguments, value->constir_result.written.call_arguments,
                value->constir_switch, value->constir_result.written.switch_arms,
                value->constir_membership,
                value->constir_result.written.membership_cases,
                &value->frontend_output, &value->frontend_result, NULL, 0u,
                NULL, 0u},
            1u, mixed_arguments, 2u,
            (w_seed_constir_quota){100u, 0u, 1u, SIZE_MAX}, &workspace, &output,
            &evaluation) == W_SEED_CONSTIR_OK &&
        evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        output.kind == W_SEED_CONSTIR_VALUE_BOOL && output.bool_value);
  CHECK(w_seed_constir_value_integer(
      value->constir_parameters[3].type_index, W_SEED_FRONTEND_TYPE_INTEGER,
      false, 16u, two_fifty_six, &mixed_arguments[1]));
  CHECK(w_seed_constir_evaluate(
            &(w_seed_constir_program){
                value->constir_functions, value->constir_result.written.functions,
                value->constir_parameters, value->constir_result.written.parameters,
                value->constir_nodes, value->constir_result.written.nodes,
                value->constir_arguments, value->constir_result.written.call_arguments,
                value->constir_switch, value->constir_result.written.switch_arms,
                value->constir_membership,
                value->constir_result.written.membership_cases,
                &value->frontend_output, &value->frontend_result, NULL, 0u,
                NULL, 0u},
            1u, mixed_arguments, 2u,
            (w_seed_constir_quota){100u, 0u, 1u, SIZE_MAX}, &workspace, &output,
            &evaluation) == W_SEED_CONSTIR_OK &&
        evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        output.kind == W_SEED_CONSTIR_VALUE_BOOL && !output.bool_value);
  CHECK(w_seed_constir_value_integer(
      value->constir_parameters[4].type_index, W_SEED_FRONTEND_TYPE_INTEGER,
      false, 8u, two_fifty_five, &mixed_arguments[0]));
  CHECK(w_seed_constir_value_integer(
      value->constir_parameters[5].type_index, W_SEED_FRONTEND_TYPE_INTEGER,
      false, 16u, (uint8_t[W_SEED_CONSTIR_INTEGER_BYTES]){1u},
      &mixed_arguments[1]));
  CHECK(w_seed_constir_evaluate(
            &(w_seed_constir_program){
                value->constir_functions, value->constir_result.written.functions,
                value->constir_parameters, value->constir_result.written.parameters,
                value->constir_nodes, value->constir_result.written.nodes,
                value->constir_arguments, value->constir_result.written.call_arguments,
                value->constir_switch, value->constir_result.written.switch_arms,
                value->constir_membership,
                value->constir_result.written.membership_cases,
                &value->frontend_output, &value->frontend_result, NULL, 0u,
                NULL, 0u},
            2u, mixed_arguments, 2u,
            (w_seed_constir_quota){100u, 0u, 1u, SIZE_MAX}, &workspace, &output,
            &evaluation) == W_SEED_CONSTIR_OK &&
        evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        output.kind == W_SEED_CONSTIR_VALUE_INTEGER &&
        output.integer_value[0] == 0u && output.integer_value[1] == 1u);

  static const char direct[] =
      "const fn direct(value: u8): u8 { return value }\n"
      "const fn wrapped(value: u8): u8 { return (value) }\n";
  CHECK(fixture_lower(&second_fixture, direct));
  CHECK(second_fixture.constir_result.written.functions == 2u);
  CHECK(second_fixture.constir_functions[0].node_count ==
            second_fixture.constir_functions[1].node_count &&
        memcmp(second_fixture.constir_functions[0].body_digest,
               second_fixture.constir_functions[1].body_digest, 32u) == 0 &&
        second_fixture.constir_nodes[second_fixture.constir_functions[1].root_node].kind ==
            W_SEED_CONSTIR_NODE_PARAMETER);
  uint8_t three[W_SEED_CONSTIR_INTEGER_BYTES] = {3u};
  w_seed_constir_value three_value;
  CHECK(w_seed_constir_value_integer(
      second_fixture.constir_parameters[0].type_index,
      W_SEED_FRONTEND_TYPE_INTEGER, false, 8u, three, &three_value));
  w_seed_constir_value direct_value;
  w_seed_constir_value wrapped_value;
  w_seed_constir_eval_result direct_result;
  w_seed_constir_eval_result wrapped_result;
  const w_seed_constir_program direct_program = fixture_program(&second_fixture);
  CHECK(w_seed_constir_evaluate(
            &direct_program, 0u, &three_value, 1u,
            (w_seed_constir_quota){32u, 0u, 1u, SIZE_MAX}, &workspace,
            &direct_value, &direct_result) == W_SEED_CONSTIR_OK &&
        w_seed_constir_evaluate(
            &direct_program, 1u, &three_value, 1u,
            (w_seed_constir_quota){32u, 0u, 1u, SIZE_MAX}, &workspace,
            &wrapped_value, &wrapped_result) == W_SEED_CONSTIR_OK &&
        direct_result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        wrapped_result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        memcmp(&direct_value, &wrapped_value, sizeof(direct_value)) == 0);
  static const char renamed[] =
      "const fn first(value: u8): u8 { return value }\n"
      "const fn second(other: u8): u8 { return other }\n";
  CHECK(fixture_lower(&second_fixture, renamed));
  CHECK(memcmp(second_fixture.constir_functions[0].body_digest,
               second_fixture.constir_functions[1].body_digest, 32u) == 0);
  return true;
}

static bool test_typed_literal_projection(void) {
  static const char decimal_source[] =
      "const fn literal(): u8 { return 1_u8 }\n";
  static const char hexadecimal_source[] =
      "const fn literal(): u8 { return 0x0_1_u8 }\n";
  fixture *decimal = &first_fixture;
  fixture *hexadecimal = &second_fixture;
  CHECK(fixture_lower(decimal, decimal_source));
  CHECK(fixture_lower(hexadecimal, hexadecimal_source));
  CHECK(decimal->constir_functions[0].lowerable &&
        hexadecimal->constir_functions[0].lowerable);
  CHECK(memcmp(decimal->constir_functions[0].body_digest,
               hexadecimal->constir_functions[0].body_digest, 32u) == 0);
  CHECK(decimal->constir_nodes[0].kind == W_SEED_CONSTIR_NODE_INTEGER &&
        hexadecimal->constir_nodes[0].kind == W_SEED_CONSTIR_NODE_INTEGER &&
        memcmp(decimal->constir_nodes[0].integer_value,
               hexadecimal->constir_nodes[0].integer_value,
               W_SEED_CONSTIR_INTEGER_BYTES) == 0);
  w_seed_constir_value decimal_value;
  w_seed_constir_value hexadecimal_value;
  w_seed_constir_eval_result decimal_result;
  w_seed_constir_eval_result hexadecimal_result;
  w_seed_constir_eval_frame frames[2];
  w_seed_constir_eval_workspace workspace = {frames, 2u};
  CHECK(w_seed_constir_evaluate(
            &(w_seed_constir_program){
                decimal->constir_functions, decimal->constir_result.written.functions,
                decimal->constir_parameters, decimal->constir_result.written.parameters,
                decimal->constir_nodes, decimal->constir_result.written.nodes,
                decimal->constir_arguments, decimal->constir_result.written.call_arguments,
                decimal->constir_switch, decimal->constir_result.written.switch_arms,
                decimal->constir_membership, decimal->constir_result.written.membership_cases,
                &decimal->frontend_output, &decimal->frontend_result, NULL, 0u,
                NULL, 0u},
            0u, NULL, 0u, (w_seed_constir_quota){32u, 0u, 2u, SIZE_MAX},
            &workspace, &decimal_value, &decimal_result) == W_SEED_CONSTIR_OK &&
        decimal_result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        decimal_value.kind == W_SEED_CONSTIR_VALUE_INTEGER &&
        decimal_value.integer_value[0] == 1u);
  CHECK(w_seed_constir_evaluate(
            &(w_seed_constir_program){
                hexadecimal->constir_functions, hexadecimal->constir_result.written.functions,
                hexadecimal->constir_parameters, hexadecimal->constir_result.written.parameters,
                hexadecimal->constir_nodes, hexadecimal->constir_result.written.nodes,
                hexadecimal->constir_arguments, hexadecimal->constir_result.written.call_arguments,
                hexadecimal->constir_switch, hexadecimal->constir_result.written.switch_arms,
                hexadecimal->constir_membership, hexadecimal->constir_result.written.membership_cases,
                &hexadecimal->frontend_output, &hexadecimal->frontend_result,
                NULL, 0u, NULL, 0u},
            0u, NULL, 0u, (w_seed_constir_quota){32u, 0u, 2u, SIZE_MAX},
            &workspace, &hexadecimal_value, &hexadecimal_result) == W_SEED_CONSTIR_OK &&
        hexadecimal_result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        hexadecimal_value.kind == W_SEED_CONSTIR_VALUE_INTEGER &&
        hexadecimal_value.integer_value[0] == 1u);
  return true;
}

static bool test_string_literals_and_comparisons(void) {
  static const char source[] =
      "const fn equals(value: String): Bool { return value == \"a\" }\n"
      "const fn differs(value: String): Bool { return value != \"b\" }\n"
      "const fn empty(value: String): Bool { return value == \"\" }\n";
  fixture *value = &first_fixture;
  CHECK(fixture_lower(value, source));
  CHECK(value->constir_result.written.functions == 3u &&
        value->constir_result.written.parameters == 3u);
  const w_seed_constir_program program = fixture_program(value);
  uint32_t one_node = W_SEED_CONSTIR_NONE;
  uint32_t other_node = W_SEED_CONSTIR_NONE;
  uint32_t empty_node = W_SEED_CONSTIR_NONE;
  for (size_t index = 0u; index < value->constir_result.written.nodes;
       index += 1u) {
    const w_seed_constir_node *node = &value->constir_nodes[index];
    if (node->kind != W_SEED_CONSTIR_NODE_STRING) continue;
    if (node->const_byte_count == 0u)
      empty_node = (uint32_t)index;
    else if (one_node == W_SEED_CONSTIR_NONE)
      one_node = (uint32_t)index;
    else
      other_node = (uint32_t)index;
  }
  CHECK(one_node != W_SEED_CONSTIR_NONE &&
        other_node != W_SEED_CONSTIR_NONE &&
        empty_node != W_SEED_CONSTIR_NONE &&
        value->constir_nodes[one_node].const_byte_count == 1u &&
        value->constir_nodes[other_node].const_byte_count == 1u &&
        value->const_bytes[value->constir_nodes[one_node].const_byte_offset] ==
            'a' &&
        value->const_bytes[value->constir_nodes[other_node].const_byte_offset] ==
            'b');

  w_seed_constir_value argument;
  w_seed_constir_value result_value;
  w_seed_constir_eval_result evaluation;
  w_seed_constir_eval_frame frames[8];
  w_seed_constir_eval_workspace workspace = {frames, 8u};
  const w_seed_constir_node *one = &value->constir_nodes[one_node];
  CHECK(w_seed_constir_value_string(
      value->constir_parameters[0].type_index,
      value->const_bytes + one->const_byte_offset, one->const_byte_count,
      &argument));
  CHECK(w_seed_constir_evaluate(
            &program, 0u, &argument, 1u,
            (w_seed_constir_quota){100u, 0u, 8u, SIZE_MAX}, &workspace,
            &result_value, &evaluation) == W_SEED_CONSTIR_OK &&
        evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        evaluation.consumed_heap_bytes == 0u &&
        result_value.kind == W_SEED_CONSTIR_VALUE_BOOL &&
        result_value.bool_value);
  CHECK(w_seed_constir_evaluate(
            &program, 1u, &argument, 1u,
            (w_seed_constir_quota){100u, 0u, 8u, SIZE_MAX}, &workspace,
            &result_value, &evaluation) == W_SEED_CONSTIR_OK &&
        evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        result_value.kind == W_SEED_CONSTIR_VALUE_BOOL &&
        result_value.bool_value);
  CHECK(w_seed_constir_value_string(value->constir_parameters[2].type_index,
                                    NULL, 0u, &argument));
  CHECK(w_seed_constir_evaluate(
            &program, 2u, &argument, 1u,
            (w_seed_constir_quota){100u, 0u, 8u, SIZE_MAX}, &workspace,
            &result_value, &evaluation) == W_SEED_CONSTIR_OK &&
        evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        result_value.kind == W_SEED_CONSTIR_VALUE_BOOL &&
        result_value.bool_value);

  static const char digest_source[] =
      "const fn equal(value: String): Bool { return value == \"a\" }\n";
  static const char digest_spaced_source[] =
      "// trivia is outside the body preimage\n"
      "const fn equal(value: String): Bool {\n  return value == \"a\"\n}\n";
  static const char digest_other_source[] =
      "const fn equal(value: String): Bool { return value == \"b\" }\n";
  CHECK(fixture_lower(&first_fixture, digest_source));
  uint8_t digest[32];
  (void)memcpy(digest, first_fixture.constir_functions[0].body_digest,
               sizeof(digest));
  CHECK(fixture_lower(&second_fixture, digest_spaced_source));
  CHECK(memcmp(digest, second_fixture.constir_functions[0].body_digest,
               sizeof(digest)) == 0);
  CHECK(fixture_lower(&second_fixture, digest_other_source));
  CHECK(memcmp(digest, second_fixture.constir_functions[0].body_digest,
               sizeof(digest)) != 0);

  static char max_source[SOURCE_BYTES];
  CHECK(make_repeated_string_source(
      max_source, sizeof(max_source),
      "const fn maximum(value: String): Bool { return value == \"",
      W_SEED_CONSTIR_MAX_STRING_BYTES, "\" }\n"));
  CHECK(fixture_lower(value, max_source));
  CHECK(value->constir_result.written.functions == 1u &&
        value->constir_functions[0].lowerable);
  uint32_t max_node = W_SEED_CONSTIR_NONE;
  for (size_t index = 0u; index < value->constir_result.written.nodes;
       index += 1u) {
    if (value->constir_nodes[index].kind == W_SEED_CONSTIR_NODE_STRING) {
      max_node = (uint32_t)index;
      break;
    }
  }
  CHECK(max_node != W_SEED_CONSTIR_NONE &&
        value->constir_nodes[max_node].const_byte_count ==
            W_SEED_CONSTIR_MAX_STRING_BYTES);
  CHECK(w_seed_constir_value_string(
      value->constir_parameters[0].type_index,
      value->const_bytes + value->constir_nodes[max_node].const_byte_offset,
      W_SEED_CONSTIR_MAX_STRING_BYTES, &argument));
  const w_seed_constir_program max_program = fixture_program(value);
  CHECK(w_seed_constir_evaluate(
            &max_program, 0u, &argument, 1u,
            (w_seed_constir_quota){100u, 0u, 8u, SIZE_MAX}, &workspace,
            &result_value, &evaluation) == W_SEED_CONSTIR_OK &&
        result_value.kind == W_SEED_CONSTIR_VALUE_BOOL &&
        result_value.bool_value && evaluation.consumed_heap_bytes == 0u);

  static char over_limit_source[SOURCE_BYTES];
  CHECK(make_repeated_string_source(
      over_limit_source, sizeof(over_limit_source),
      "const fn over(value: String): Bool { return value == \"",
      W_SEED_CONSTIR_MAX_STRING_BYTES + 1u, "\" }\n"));
  CHECK(fixture_lower(value, over_limit_source));
  CHECK(value->constir_result.written.functions == 1u &&
        !value->constir_functions[0].lowerable &&
        memcmp(value->constir_functions[0].body_digest,
               (uint8_t[32]){0}, 32u) == 0);

  /* All malformed String node relations fail the canonical program
   * preflight, before a single evaluation step. */
  CHECK(fixture_lower(value, source));
  const w_seed_constir_program valid_program = fixture_program(value);
  uint32_t malformed_node = W_SEED_CONSTIR_NONE;
  for (size_t index = 0u; index < value->constir_result.written.nodes;
       index += 1u) {
    if (value->constir_nodes[index].kind == W_SEED_CONSTIR_NODE_STRING) {
      malformed_node = (uint32_t)index;
      break;
    }
  }
  CHECK(malformed_node != W_SEED_CONSTIR_NONE);
  CHECK(w_seed_constir_value_string(
      value->constir_parameters[0].type_index,
      value->const_bytes + value->constir_nodes[malformed_node].const_byte_offset,
      value->constir_nodes[malformed_node].const_byte_count, &argument));
  w_seed_constir_node saved_node = value->constir_nodes[malformed_node];
  value->constir_nodes[malformed_node].const_byte_offset = CONST_BYTES - 1u;
  CHECK(!w_seed_constir_validate_program(&valid_program));
  CHECK(w_seed_constir_evaluate(
            &valid_program, 0u, &argument, 1u,
            (w_seed_constir_quota){100u, 0u, 8u, SIZE_MAX}, &workspace,
            &result_value, &evaluation) == W_SEED_CONSTIR_INVALID &&
        evaluation.consumed_steps == 0u);
  value->constir_nodes[malformed_node] = saved_node;
  value->constir_nodes[malformed_node].const_byte_count =
      W_SEED_CONSTIR_MAX_STRING_BYTES + 1u;
  CHECK(!w_seed_constir_validate_program(&valid_program));
  value->constir_nodes[malformed_node] = saved_node;
  value->constir_nodes[malformed_node].type_kind = W_SEED_FRONTEND_TYPE_BOOL;
  CHECK(!w_seed_constir_validate_program(&valid_program));
  value->constir_nodes[malformed_node] = saved_node;
  const uint8_t caller_owned_byte = 'a';
  CHECK(w_seed_constir_value_string(value->constir_parameters[0].type_index,
                                    &caller_owned_byte, 1u, &argument));
  CHECK(w_seed_constir_evaluate(
            &valid_program, 0u, &argument, 1u,
            (w_seed_constir_quota){100u, 0u, 8u, SIZE_MAX}, &workspace,
            &result_value, &evaluation) == W_SEED_CONSTIR_OK &&
        evaluation.consumed_steps != 0u && result_value.bool_value &&
        evaluation.consumed_heap_bytes == 0u);

  w_seed_constir_value malformed_value = argument;
  malformed_value.string_bytes = NULL;
  CHECK(w_seed_constir_evaluate(
            &valid_program, 0u, &malformed_value, 1u,
            (w_seed_constir_quota){100u, 0u, 8u, SIZE_MAX}, &workspace,
            &result_value, &evaluation) == W_SEED_CONSTIR_INVALID &&
        evaluation.consumed_steps == 0u);
  malformed_value = argument;
  malformed_value.string_count = W_SEED_CONSTIR_MAX_STRING_BYTES + 1u;
  CHECK(w_seed_constir_evaluate(
            &valid_program, 0u, &malformed_value, 1u,
            (w_seed_constir_quota){100u, 0u, 8u, SIZE_MAX}, &workspace,
            &result_value, &evaluation) == W_SEED_CONSTIR_INVALID &&
        evaluation.consumed_steps == 0u);
  malformed_value = argument;
  malformed_value.type_kind = W_SEED_FRONTEND_TYPE_BOOL;
  CHECK(w_seed_constir_evaluate(
            &valid_program, 0u, &malformed_value, 1u,
            (w_seed_constir_quota){100u, 0u, 8u, SIZE_MAX}, &workspace,
            &result_value, &evaluation) == W_SEED_CONSTIR_INVALID &&
        evaluation.consumed_steps == 0u);
  malformed_value = argument;
  malformed_value.type_index = W_SEED_CONSTIR_NONE;
  CHECK(w_seed_constir_evaluate(
            &valid_program, 0u, &malformed_value, 1u,
            (w_seed_constir_quota){100u, 0u, 8u, SIZE_MAX}, &workspace,
            &result_value, &evaluation) == W_SEED_CONSTIR_INVALID &&
        evaluation.consumed_steps == 0u);
  return true;
}

static bool test_recursion_and_invalid_inputs(void) {
  static const char source[] = "const fn recurse(): Bool { return recurse() }\n";
  fixture *value = &first_fixture;
  CHECK(fixture_lower(value, source));
  CHECK(value->constir_result.written.functions == 1u);
  const w_seed_constir_program program = fixture_program(value);
  /* Downstream callers use the same canonical preflight as the evaluator;
   * recursive call graphs remain valid until the runtime quota is reached. */
  CHECK(w_seed_constir_validate_program(&program));
  const w_seed_constir_invocation invocation = {0u, NULL, 0u};
  CHECK(w_seed_constir_validate_invocations(&program, &invocation, 1u));
  CHECK(w_seed_constir_validate_invocations_in_validated_program(
      &program, &invocation, 1u));
  const w_seed_constir_invocation malformed_invocation = {1u, NULL, 0u};
  CHECK(!w_seed_constir_validate_invocations_in_validated_program(
      &program, &malformed_invocation, 1u));
  w_seed_constir_value result_value;
  w_seed_constir_eval_result result;
  w_seed_constir_eval_frame frames[8];
  w_seed_constir_eval_workspace workspace = {frames, 8u};
  CHECK(w_seed_constir_evaluate(&program, 0u, NULL, 0u,
                                (w_seed_constir_quota){100u, 0u, 3u, SIZE_MAX},
                                &workspace, &result_value, &result) ==
        W_SEED_CONSTIR_OK);
  CHECK(result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003 &&
        result.quota_limit == 3u);
  CHECK(w_seed_constir_evaluate(&program, 0u, NULL, 0u,
                                (w_seed_constir_quota){0u, 0u, 3u, SIZE_MAX},
                                &workspace, &result_value, &result) ==
        W_SEED_CONSTIR_OK &&
        result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003 &&
        result.consumed_steps == 0u && result.quota_limit == 0u &&
        result.consumed_heap_bytes == 0u);
  return true;
}

static bool test_empty_program_validation(void) {
  const w_seed_constir_program empty = {0};
  CHECK(w_seed_constir_validate_program(&empty));
  CHECK(w_seed_constir_validate_invocations_in_validated_program(
      &empty, NULL, 0u));

  const w_seed_constir_value argument = {0};
  const w_seed_constir_invocation nonempty_invocation = {0u, &argument, 1u};
  CHECK(!w_seed_constir_validate_invocations_in_validated_program(
      &empty, &nonempty_invocation, 1u));

  w_seed_constir_program orphan = empty;
  orphan.node_count = 1u;
  CHECK(!w_seed_constir_validate_program(&orphan));
  return true;
}

static bool test_depth_and_caller_owned_validation(void) {
  static const char leaf_source[] =
      "const fn leaf(): Bool { return true }\n";
  fixture *value = &first_fixture;
  CHECK(fixture_lower(value, leaf_source));
  const w_seed_constir_program leaf_program = fixture_program(value);
  w_seed_constir_value output;
  w_seed_constir_eval_result evaluation;
  w_seed_constir_eval_frame frames[2];
  w_seed_constir_eval_workspace workspace = {frames, 2u};
  CHECK(w_seed_constir_evaluate(
            &leaf_program, 0u, NULL, 0u,
            (w_seed_constir_quota){32u, 0u, 0u, SIZE_MAX}, &workspace, &output,
            &evaluation) == W_SEED_CONSTIR_OK &&
        evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003 &&
        evaluation.consumed_steps == 0u && evaluation.consumed_call_depth == 1u &&
        output.kind == W_SEED_CONSTIR_VALUE_INVALID);
  CHECK(w_seed_constir_evaluate(
            &leaf_program, 0u, NULL, 0u,
            (w_seed_constir_quota){32u, 0u, 1u, SIZE_MAX}, &workspace, &output,
            &evaluation) == W_SEED_CONSTIR_OK &&
        evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        output.kind == W_SEED_CONSTIR_VALUE_BOOL);
  CHECK(w_seed_constir_evaluate(
            &leaf_program, 0u, NULL, 0u,
            (w_seed_constir_quota){32u, 0u,
                                   W_SEED_CONSTIR_MAX_CALL_DEPTH + 1u, SIZE_MAX},
            &workspace, &output, &evaluation) == W_SEED_CONSTIR_INVALID &&
        evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE);

  static const char nested_source[] =
      "const fn leaf(): Bool { return true }\n"
      "const fn nested(): Bool { return leaf() }\n";
  CHECK(fixture_lower(value, nested_source));
  const w_seed_constir_program nested_program = fixture_program(value);
  CHECK(w_seed_constir_evaluate(
            &nested_program, 1u, NULL, 0u,
            (w_seed_constir_quota){32u, 0u, 1u, SIZE_MAX}, &workspace, &output,
            &evaluation) == W_SEED_CONSTIR_OK &&
        evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003 &&
        evaluation.quota_limit == 1u && output.kind == W_SEED_CONSTIR_VALUE_INVALID);
  CHECK(w_seed_constir_evaluate(
            &nested_program, 1u, NULL, 0u,
            (w_seed_constir_quota){32u, 0u, 2u, SIZE_MAX}, &workspace, &output,
            &evaluation) == W_SEED_CONSTIR_OK &&
        evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        output.kind == W_SEED_CONSTIR_VALUE_BOOL);
  CHECK(w_seed_constir_evaluate(
            &nested_program, 1u, NULL, 0u,
            (w_seed_constir_quota){32u, 0u, 2u, SIZE_MAX}, NULL, &output,
            &evaluation) == W_SEED_CONSTIR_INVALID &&
        evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        output.kind == W_SEED_CONSTIR_VALUE_INVALID);
  workspace.frame_capacity = 0u;
  CHECK(w_seed_constir_evaluate(
            &nested_program, 1u, NULL, 0u,
            (w_seed_constir_quota){32u, 0u, 2u, SIZE_MAX}, &workspace, &output,
            &evaluation) == W_SEED_CONSTIR_INVALID &&
        evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE);
  workspace.frame_capacity = 2u;

  static const char scalar_source[] =
      "const fn scalar(): Bool { return true }\n";
  CHECK(fixture_lower(value, scalar_source));
  w_seed_constir_function function_copy = value->constir_functions[0];
  const w_seed_constir_node original = value->constir_nodes[function_copy.root_node];
  w_seed_constir_program scalar_program = fixture_program(value);
  scalar_program.frontend_output = NULL;
  scalar_program.frontend_result = NULL;
  value->constir_nodes[function_copy.root_node].type_kind =
      W_SEED_FRONTEND_TYPE_RANGE;
  CHECK(w_seed_constir_evaluate(
            &scalar_program, 0u, NULL, 0u,
            (w_seed_constir_quota){32u, 0u, 1u, SIZE_MAX}, &workspace, &output,
            &evaluation) == W_SEED_CONSTIR_INVALID &&
        evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        evaluation.consumed_steps == 0u &&
        output.kind == W_SEED_CONSTIR_VALUE_INVALID);
  value->constir_nodes[function_copy.root_node].type_kind = original.type_kind;
  const uint32_t first = function_copy.first_node;
  const size_t chain_count = (size_t)W_SEED_CONSTIR_MAX_EVAL_DEPTH + 1u;
  for (size_t offset = 0; offset < chain_count; offset += 1u) {
    w_seed_constir_node *node = &value->constir_nodes[(size_t)first + offset];
    (void)memset(node, 0, sizeof(*node));
    node->owner_function = function_copy.frontend_function;
    node->frontend_expression = W_SEED_CONSTIR_NONE;
    node->type_index = original.type_index;
    node->type_kind = W_SEED_FRONTEND_TYPE_BOOL;
    node->source_span = (w_seed_span){0u, 1u};
    node->left = W_SEED_CONSTIR_NONE;
    node->right = W_SEED_CONSTIR_NONE;
    node->parameter_ordinal = W_SEED_CONSTIR_NONE;
    node->call_target_function = W_SEED_CONSTIR_NONE;
    node->first_call_argument = W_SEED_CONSTIR_NONE;
    node->first_switch_arm = W_SEED_CONSTIR_NONE;
    node->first_membership_case = W_SEED_CONSTIR_NONE;
    node->normalized_operator = W_SEED_CONSTIR_OPERATOR_INVALID;
    node->kind = W_SEED_CONSTIR_NODE_BOOL;
    if (offset != 0u) {
      node->kind = W_SEED_CONSTIR_NODE_UNARY;
      node->normalized_operator = W_SEED_CONSTIR_OPERATOR_NOT;
      node->left = first + (uint32_t)(offset - 1u);
    }
  }
  function_copy.node_count = (uint32_t)chain_count;
  function_copy.root_node = first + (uint32_t)(chain_count - 1u);
  const w_seed_constir_program deep_program = {
      &function_copy, 1u, value->constir_parameters, 0u, value->constir_nodes,
      first + chain_count, NULL, 0u, NULL, 0u, NULL, 0u,
      &value->frontend_output, &value->frontend_result, NULL, 0u, NULL, 0u};
  CHECK(!w_seed_constir_validate_program(&deep_program));
  CHECK(w_seed_constir_evaluate(
            &deep_program, 0u, NULL, 0u,
            (w_seed_constir_quota){SIZE_MAX, 0u, SIZE_MAX, SIZE_MAX},
            &workspace, &output, &evaluation) == W_SEED_CONSTIR_INVALID &&
        evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        evaluation.consumed_steps == 0u);

  function_copy.node_count = 1u;
  function_copy.root_node = first;
  value->constir_nodes[first].kind = W_SEED_CONSTIR_NODE_UNARY;
  value->constir_nodes[first].normalized_operator = W_SEED_CONSTIR_OPERATOR_NOT;
  value->constir_nodes[first].left = first;
  CHECK(w_seed_constir_evaluate(
            &(w_seed_constir_program){
                .functions = &function_copy,
                .function_count = 1u,
                .parameters = value->constir_parameters,
                .parameter_count = 0u,
                .nodes = value->constir_nodes,
                .node_count = first + 1u,
                .frontend_output = &value->frontend_output,
                .frontend_result = &value->frontend_result},
            0u, NULL, 0u,
            (w_seed_constir_quota){32u, 0u, 1u, SIZE_MAX}, &workspace, &output,
            &evaluation) == W_SEED_CONSTIR_INVALID &&
        evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        evaluation.consumed_steps == 0u);

  w_seed_constir_function malformed_function = {
      .frontend_function = 0u,
      .lowerable = true,
      .body_span = {0u, 1u},
      .first_parameter = 0u,
      .parameter_count = 0u,
      .first_node = 0u,
      .node_count = 3u,
      .root_node = 2u,
      .diagnostic_index = W_SEED_CONSTIR_NONE};
  w_seed_constir_node malformed_nodes[3];
  (void)memset(malformed_nodes, 0, sizeof(malformed_nodes));
  malformed_nodes[0] = (w_seed_constir_node){
      .kind = W_SEED_CONSTIR_NODE_INTEGER,
      .owner_function = 0u,
      .type_index = 1u,
      .type_kind = W_SEED_FRONTEND_TYPE_INTEGER,
      .type_bit_width = 8u,
      .source_span = {0u, 1u},
      .left = W_SEED_CONSTIR_NONE,
      .right = W_SEED_CONSTIR_NONE,
      .parameter_ordinal = W_SEED_CONSTIR_NONE,
      .call_target_function = W_SEED_CONSTIR_NONE,
      .first_call_argument = W_SEED_CONSTIR_NONE,
      .first_switch_arm = W_SEED_CONSTIR_NONE,
      .first_membership_case = W_SEED_CONSTIR_NONE,
      .normalized_operator = W_SEED_CONSTIR_OPERATOR_INVALID};
  malformed_nodes[1] = malformed_nodes[0];
  malformed_nodes[1].type_index = 2u;
  malformed_nodes[1].type_is_signed = true;
  malformed_nodes[1].type_bit_width = 16u;
  malformed_nodes[2] = malformed_nodes[0];
  malformed_nodes[2].type_index = 3u;
  malformed_nodes[2].kind = W_SEED_CONSTIR_NODE_BINARY;
  malformed_nodes[2].normalized_operator = W_SEED_CONSTIR_OPERATOR_ADD;
  malformed_nodes[2].left = 0u;
  malformed_nodes[2].right = 1u;
  const w_seed_constir_program malformed_program = {
      .functions = &malformed_function,
      .function_count = 1u,
      .nodes = malformed_nodes,
      .node_count = 3u};
  CHECK(!w_seed_constir_validate_program(&malformed_program));
  CHECK(w_seed_constir_evaluate(
            &malformed_program, 0u, NULL, 0u,
            (w_seed_constir_quota){32u, 0u, 1u, SIZE_MAX}, &workspace, &output,
            &evaluation) == W_SEED_CONSTIR_INVALID &&
        evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        evaluation.consumed_steps == 0u);
  malformed_nodes[2].kind = W_SEED_CONSTIR_NODE_UNARY;
  malformed_nodes[2].normalized_operator = W_SEED_CONSTIR_OPERATOR_NEGATE;
  malformed_nodes[2].right = W_SEED_CONSTIR_NONE;
  malformed_nodes[2].type_index = 2u;
  malformed_nodes[2].type_is_signed = true;
  malformed_nodes[2].type_bit_width = 16u;
  CHECK(w_seed_constir_evaluate(
            &malformed_program, 0u, NULL, 0u,
            (w_seed_constir_quota){32u, 0u, 1u, SIZE_MAX}, &workspace, &output,
            &evaluation) == W_SEED_CONSTIR_INVALID &&
        evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE);
  w_seed_constir_parameter malformed_parameter = {
      .owner_function = 0u,
      .ordinal = 0u,
      .type_index = 1u,
      .type_kind = W_SEED_FRONTEND_TYPE_INTEGER,
      .type_bit_width = 8u,
      .source_span = {0u, 1u}};
  malformed_function.parameter_count = 1u;
  malformed_nodes[0].kind = W_SEED_CONSTIR_NODE_PARAMETER;
  malformed_nodes[0].type_index = 2u;
  malformed_nodes[0].type_is_signed = true;
  malformed_nodes[0].type_bit_width = 16u;
  malformed_nodes[0].parameter_ordinal = 0u;
  malformed_function.node_count = 1u;
  malformed_function.root_node = 0u;
  const w_seed_constir_program malformed_parameter_program = {
      .functions = &malformed_function,
      .function_count = 1u,
      .parameters = &malformed_parameter,
      .parameter_count = 1u,
      .nodes = malformed_nodes,
      .node_count = 1u};
  CHECK(w_seed_constir_evaluate(
            &malformed_parameter_program, 0u, NULL, 0u,
            (w_seed_constir_quota){32u, 0u, 1u, SIZE_MAX}, &workspace, &output,
            &evaluation) == W_SEED_CONSTIR_INVALID &&
        evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        evaluation.consumed_steps == 0u);

  w_seed_constir_function call_functions[2];
  (void)memset(call_functions, 0, sizeof(call_functions));
  call_functions[0] = malformed_function;
  call_functions[0].node_count = 1u;
  call_functions[0].root_node = 0u;
  call_functions[1] = malformed_function;
  call_functions[1].frontend_function = 1u;
  call_functions[1].first_node = 1u;
  call_functions[1].root_node = 1u;
  call_functions[1].node_count = 1u;
  w_seed_constir_node call_nodes[2];
  (void)memset(call_nodes, 0, sizeof(call_nodes));
  call_nodes[0] = malformed_nodes[0];
  call_nodes[0].kind = W_SEED_CONSTIR_NODE_CALL;
  call_nodes[0].type_index = 2u;
  call_nodes[0].type_kind = W_SEED_FRONTEND_TYPE_INTEGER;
  call_nodes[0].type_bit_width = 16u;
  call_nodes[0].call_target_function = 1u;
  call_nodes[0].first_call_argument = W_SEED_CONSTIR_NONE;
  call_nodes[0].call_argument_count = 0u;
  call_nodes[1] = malformed_nodes[0];
  call_nodes[1].owner_function = 1u;
  call_nodes[1].type_index = 4u;
  call_nodes[1].type_kind = W_SEED_FRONTEND_TYPE_BOOL;
  const w_seed_constir_program malformed_call_program = {
      .functions = call_functions,
      .function_count = 2u,
      .nodes = call_nodes,
      .node_count = 2u};
  CHECK(w_seed_constir_evaluate(
            &malformed_call_program, 0u, NULL, 0u,
            (w_seed_constir_quota){32u, 0u, 2u, SIZE_MAX}, &workspace, &output,
            &evaluation) == W_SEED_CONSTIR_INVALID &&
        evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        evaluation.consumed_steps == 0u);
  return true;
}

static bool test_direct_call_and_external_barrier(void) {
  static const char local_source[] =
      "const fn caller(): u8 { return helper(2_u8) }\n"
      "const fn helper(value: u8): u8 { return value + 1_u8 }\n";
  fixture *value = &second_fixture;
  CHECK(fixture_lower(value, local_source));
  CHECK(value->constir_result.written.functions == 2u &&
        value->constir_result.written.call_arguments == 1u);
  const w_seed_constir_program program = fixture_program(value);
  w_seed_constir_value result_value;
  w_seed_constir_eval_result result;
  w_seed_constir_eval_frame frames[8];
  w_seed_constir_eval_workspace workspace = {frames, 8u};
  CHECK(w_seed_constir_evaluate(
      &program, 0u, NULL, 0u,
      (w_seed_constir_quota){100u, 0u, 8u, SIZE_MAX}, &workspace,
      &result_value, &result) == W_SEED_CONSTIR_OK &&
        result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        result_value.kind == W_SEED_CONSTIR_VALUE_INTEGER &&
        result_value.integer_value[0] == 3u);

  static const char external_source[] =
      "import { externalFn } from external\n"
      "const fn caller(): Bool { return externalFn() }\n";
  CHECK(fixture_parse(value, external_source));
  value->external_symbols[0] = (w_seed_frontend_external_symbol){
      {"externalFn", 10}, W_SEED_FRONTEND_EXTERNAL_VALUE, true, NULL, 0u,
      {"Bool", 4}, true};
  value->external_modules[0] =
      (w_seed_frontend_external_module){{"external", 8},
                                        value->external_symbols, 1u};
  value->frontend_input.external_modules = value->external_modules;
  value->frontend_input.external_module_count = 1u;
  CHECK(w_seed_frontend_run(&value->frontend_input, &value->frontend_output,
                            &value->frontend_result) == W_SEED_FRONTEND_OK);
  const w_seed_constir_input external_input = {
      &value->frontend_input, &value->frontend_output, &value->frontend_result};
  CHECK(w_seed_constir_run(&external_input, &value->constir_output,
                           &value->constir_result) == W_SEED_CONSTIR_OK);
  CHECK(value->constir_result.written.functions == 1u &&
        value->constir_result.written.nodes == 0u &&
        value->constir_result.written.diagnostics == 1u &&
        value->constir_diagnostics[0].code ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0001);
  return true;
}

static bool test_capacity_and_barrier(void) {
  static const char source[] = "const fn scalar(): Bool { return true }\n";
  fixture *value = &first_fixture;
  CHECK(fixture_parse(value, source));
  const w_seed_constir_input input = {
      &value->frontend_input, &value->frontend_output, &value->frontend_result};
  w_seed_constir_counts counts;
  w_seed_constir_result result;
  CHECK(w_seed_constir_measure(&input, &counts, &result) == W_SEED_CONSTIR_OK);
  CHECK(counts.functions == 1u && counts.parameters == 0u && counts.nodes != 0u &&
        counts.receipt_bytes != 0u);
  (void)memset(value->constir_functions, 0xa5, sizeof(value->constir_functions));
  (void)memset(value->constir_nodes, 0xa5, sizeof(value->constir_nodes));
  (void)memset(value->constir_receipt, 0xa5, sizeof(value->constir_receipt));
  value->constir_output.function_capacity = 0u;
  value->constir_output.functions = NULL;
  CHECK(w_seed_constir_run(&input, &value->constir_output, &result) ==
        W_SEED_CONSTIR_CAPACITY);
  CHECK(((const uint8_t *)value->constir_nodes)[0] == 0xa5u &&
        value->constir_receipt[0] == 0xa5u);

  static const char complete_source[] =
      "enum Stage { accepted reserving preparing serving completed cancelled }\n"
      "const fn helper(value: u8): u8 { return value + 1_u8 }\n"
      "const fn caller(value: u8): u8 { return helper(value) }\n"
      "const fn move(from current: Stage, to next: Stage): Bool { return switch current { "
      "case .accepted: next in (.reserving, .cancelled) "
      "case .reserving: next in (.preparing, .cancelled) "
      "case .preparing: next in (.serving, .cancelled) "
      "case .serving: next in (.completed, .cancelled) "
      "case .completed: false case .cancelled: false } }\n"
      "const fn structured(stages: StaticList<Stage>): Bool {\n"
      "for index in 0..<stages.count {\n"
      "return true\n"
      "}\n"
      "return false\n"
      "}\n"
      "const fn bad(value: u8): u8 { let local = value return local }\n";
  CHECK(fixture_parse(value, complete_source));
  const w_seed_constir_input complete_input = {
      &value->frontend_input, &value->frontend_output, &value->frontend_result};
  w_seed_constir_counts complete_counts;
  CHECK(w_seed_constir_measure(&complete_input, &complete_counts, &result) ==
        W_SEED_CONSTIR_OK);
  CHECK(complete_counts.functions != 0u && complete_counts.parameters != 0u &&
        complete_counts.nodes != 0u && complete_counts.call_arguments != 0u &&
        complete_counts.switch_arms != 0u &&
        complete_counts.membership_cases != 0u &&
        complete_counts.statements != 0u && complete_counts.locals != 0u &&
        complete_counts.diagnostics != 0u && complete_counts.receipt_bytes != 0u);

  static const char list_result_source[] =
      "enum Stage { accepted reserving preparing serving completed cancelled }\n"
      "const fn identity(stages: StaticList<Stage>): StaticList<Stage> {\n"
      "return stages\n"
      "}\n";
  CHECK(fixture_lower(value, list_result_source));
  CHECK(value->constir_result.written.functions == 1u &&
        !value->constir_functions[0].lowerable &&
        value->constir_result.written.diagnostics == 1u &&
        value->constir_diagnostics[0].code ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0001);
  CHECK(fixture_parse(value, complete_source));

  (void)memset(value->constir_functions, 0xa5, sizeof(value->constir_functions));
  (void)memset(value->constir_parameters, 0xa5, sizeof(value->constir_parameters));
  (void)memset(value->constir_nodes, 0xa5, sizeof(value->constir_nodes));
  (void)memset(value->constir_arguments, 0xa5, sizeof(value->constir_arguments));
  (void)memset(value->constir_switch, 0xa5, sizeof(value->constir_switch));
  (void)memset(value->constir_membership, 0xa5, sizeof(value->constir_membership));
  (void)memset(value->constir_statements, 0xa5, sizeof(value->constir_statements));
  (void)memset(value->constir_locals, 0xa5, sizeof(value->constir_locals));
  (void)memset(value->constir_diagnostics, 0xa5, sizeof(value->constir_diagnostics));
  (void)memset(value->constir_receipt, 0xa5, sizeof(value->constir_receipt));
#define CHECK_CONSTIR_SENTINELS()                                               \
  CHECK(((const uint8_t *)value->constir_functions)[0] == 0xa5u &&             \
        ((const uint8_t *)value->constir_parameters)[0] == 0xa5u &&             \
        ((const uint8_t *)value->constir_nodes)[0] == 0xa5u &&                  \
        ((const uint8_t *)value->constir_arguments)[0] == 0xa5u &&              \
        ((const uint8_t *)value->constir_switch)[0] == 0xa5u &&                 \
        ((const uint8_t *)value->constir_membership)[0] == 0xa5u &&             \
        ((const uint8_t *)value->constir_statements)[0] == 0xa5u &&             \
        ((const uint8_t *)value->constir_locals)[0] == 0xa5u &&                 \
        ((const uint8_t *)value->constir_diagnostics)[0] == 0xa5u &&            \
        value->constir_receipt[0] == 0xa5u)

  fixture_init_output(value);
  value->constir_output.functions = NULL;
  value->constir_output.function_capacity = 0u;
  CHECK(w_seed_constir_run(&complete_input, &value->constir_output, &result) ==
        W_SEED_CONSTIR_CAPACITY);
  CHECK_CONSTIR_SENTINELS();
  fixture_init_output(value);
  value->constir_output.parameters = NULL;
  value->constir_output.parameter_capacity = 0u;
  CHECK(w_seed_constir_run(&complete_input, &value->constir_output, &result) ==
        W_SEED_CONSTIR_CAPACITY);
  CHECK_CONSTIR_SENTINELS();
  fixture_init_output(value);
  value->constir_output.nodes = NULL;
  value->constir_output.node_capacity = 0u;
  CHECK(w_seed_constir_run(&complete_input, &value->constir_output, &result) ==
        W_SEED_CONSTIR_CAPACITY);
  CHECK_CONSTIR_SENTINELS();
  fixture_init_output(value);
  value->constir_output.statements = NULL;
  value->constir_output.statement_capacity = 0u;
  CHECK(w_seed_constir_run(&complete_input, &value->constir_output, &result) ==
        W_SEED_CONSTIR_CAPACITY);
  CHECK_CONSTIR_SENTINELS();
  fixture_init_output(value);
  value->constir_output.locals = NULL;
  value->constir_output.local_capacity = 0u;
  CHECK(w_seed_constir_run(&complete_input, &value->constir_output, &result) ==
        W_SEED_CONSTIR_CAPACITY);
  CHECK_CONSTIR_SENTINELS();
  fixture_init_output(value);
  value->constir_output.call_arguments = NULL;
  value->constir_output.call_argument_capacity = 0u;
  CHECK(w_seed_constir_run(&complete_input, &value->constir_output, &result) ==
        W_SEED_CONSTIR_CAPACITY);
  CHECK_CONSTIR_SENTINELS();
  fixture_init_output(value);
  value->constir_output.switch_arms = NULL;
  value->constir_output.switch_arm_capacity = 0u;
  CHECK(w_seed_constir_run(&complete_input, &value->constir_output, &result) ==
        W_SEED_CONSTIR_CAPACITY);
  CHECK_CONSTIR_SENTINELS();
  fixture_init_output(value);
  value->constir_output.membership_cases = NULL;
  value->constir_output.membership_case_capacity = 0u;
  CHECK(w_seed_constir_run(&complete_input, &value->constir_output, &result) ==
        W_SEED_CONSTIR_CAPACITY);
  CHECK_CONSTIR_SENTINELS();
  fixture_init_output(value);
  value->constir_output.diagnostics = NULL;
  value->constir_output.diagnostic_capacity = 0u;
  CHECK(w_seed_constir_run(&complete_input, &value->constir_output, &result) ==
        W_SEED_CONSTIR_CAPACITY);
  CHECK_CONSTIR_SENTINELS();
  fixture_init_output(value);
  value->constir_output.receipt = NULL;
  value->constir_output.receipt_capacity = 0u;
  CHECK(w_seed_constir_run(&complete_input, &value->constir_output, &result) ==
        W_SEED_CONSTIR_CAPACITY);
  CHECK_CONSTIR_SENTINELS();
#undef CHECK_CONSTIR_SENTINELS
  return true;
}

int main(void) {
  CHECK(test_can_move_and_digest());
  CHECK(test_static_list_stage_path());
  CHECK(test_diagnostics_and_quotas());
  CHECK(test_labels_relations_and_parentheses());
  CHECK(test_typed_literal_projection());
  CHECK(test_string_literals_and_comparisons());
  CHECK(test_recursion_and_invalid_inputs());
  CHECK(test_empty_program_validation());
  CHECK(test_depth_and_caller_owned_validation());
  CHECK(test_direct_call_and_external_barrier());
  CHECK(test_capacity_and_barrier());
  (void)puts("constir tests passed");
  return 0;
}
