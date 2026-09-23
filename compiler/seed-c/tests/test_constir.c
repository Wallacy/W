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
  TUPLE_COMPONENTS = TOKENS * 2,
  TUPLE_ELEMENTS = TOKENS,
  SWITCH_ARMS = 8192,
  MEMBERSHIP = 32768,
  CONST_BYTES = 65536,
  DIAGNOSTIC_FACTS = ARRAY * 5,
  DIAGNOSTIC_ITEMS = ARRAY * 4,
  DIAGNOSTIC_LABELS = ARRAY * 2,
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
  w_seed_frontend_resolved_import resolved_imports[ARRAY];
  w_seed_module_origin module_origins[ARRAY];
  w_seed_frontend_module modules[ARRAY];
  w_seed_frontend_import imports[ARRAY];
  w_seed_frontend_import_item import_items[ARRAY];
  w_seed_frontend_struct structs[ARRAY];
  w_seed_frontend_generic_parameter generic_parameters[ARRAY];
  w_seed_frontend_generic_application generic_applications[ARRAY];
  w_seed_frontend_generic_argument generic_arguments[ARRAY];
  w_seed_frontend_typed_const_expression typed_const_expressions[ARRAY];
  w_seed_frontend_const_value const_values[ARRAY];
  w_seed_frontend_const_element const_elements[ARRAY];
  w_seed_frontend_enum enums[ARRAY];
  w_seed_frontend_enum_case enum_cases[ARRAY];
  w_seed_frontend_enum_case_parameter enum_case_parameters[ARRAY];
  w_seed_frontend_enum_subset_member enum_subset_members[ARRAY];
  w_seed_frontend_field fields[ARRAY];
  w_seed_frontend_type_declaration declarations[ARRAY];
  w_seed_frontend_alias aliases[ARRAY];
  w_seed_frontend_const_declaration const_declarations[ARRAY];
  w_seed_frontend_type types[TYPES];
  w_seed_frontend_function functions[FUNCTIONS];
  w_seed_frontend_parameter parameters[PARAMETERS];
  w_seed_frontend_entry entries[ARRAY];
  w_seed_frontend_statement statements[STATEMENTS];
  w_seed_frontend_expression expressions[EXPRESSIONS];
  w_seed_frontend_argument arguments[ARGUMENTS];
  w_seed_frontend_tuple_component tuple_components[TUPLE_COMPONENTS];
  w_seed_frontend_tuple_element tuple_elements[TUPLE_ELEMENTS];
  w_seed_frontend_switch_arm switch_arms[SWITCH_ARMS];
  w_seed_frontend_enum_membership_case membership[MEMBERSHIP];
  w_seed_frontend_symbol symbols[ARRAY];
  w_seed_frontend_fact facts[ARRAY];
  w_seed_frontend_diagnostic diagnostics[ARRAY];
  w_seed_frontend_diagnostic_fact diagnostic_facts[DIAGNOSTIC_FACTS];
  w_seed_frontend_diagnostic_item diagnostic_items[DIAGNOSTIC_ITEMS];
  w_seed_frontend_diagnostic_label diagnostic_labels[DIAGNOSTIC_LABELS];
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

static bool first_byte_equals(const void *object, unsigned char expected) {
  unsigned char actual;
  (void)memcpy(&actual, object, sizeof(actual));
  return actual == expected;
}

static void fixture_init_output(fixture *value) {
  value->frontend_input = (w_seed_frontend_input){
      .documents = &value->document,
      .document_count = 1u,
      .external_modules = NULL,
      .external_module_count = 0u,
  };
  value->frontend_output = (w_seed_frontend_output){
      .modules = value->modules,
      .module_capacity = ARRAY,
      .imports = value->imports,
      .import_capacity = ARRAY,
      .import_items = value->import_items,
      .import_item_capacity = ARRAY,
      .structs = value->structs,
      .struct_capacity = ARRAY,
      .generic_parameters = value->generic_parameters,
      .generic_parameter_capacity = ARRAY,
      .generic_applications = value->generic_applications,
      .generic_application_capacity = ARRAY,
      .generic_arguments = value->generic_arguments,
      .generic_argument_capacity = ARRAY,
      .typed_const_expressions = value->typed_const_expressions,
      .typed_const_expression_capacity = ARRAY,
      .const_values = value->const_values,
      .const_value_capacity = ARRAY,
      .const_elements = value->const_elements,
      .const_element_capacity = ARRAY,
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
      .const_declarations = value->const_declarations,
      .const_declaration_capacity = ARRAY,
      .types = value->types,
      .type_capacity = TYPES,
      .functions = value->functions,
      .function_capacity = FUNCTIONS,
      .parameters = value->parameters,
      .parameter_capacity = PARAMETERS,
      .arguments = value->arguments,
      .argument_capacity = ARGUMENTS,
      .tuple_components = value->tuple_components,
      .tuple_component_capacity = TUPLE_COMPONENTS,
      .tuple_elements = value->tuple_elements,
      .tuple_element_capacity = TUPLE_ELEMENTS,
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
      .diagnostic_facts = value->diagnostic_facts,
      .diagnostic_fact_capacity = DIAGNOSTIC_FACTS,
      .diagnostic_items = value->diagnostic_items,
      .diagnostic_item_capacity = DIAGNOSTIC_ITEMS,
      .diagnostic_labels = value->diagnostic_labels,
      .diagnostic_label_capacity = DIAGNOSTIC_LABELS,
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
      .logical_source_id = {"test", 4},
      .module_id = {"test", 4},
      .local_module_name = {"test", 4},
      .source = &value->source,
      .nodes = value->cst_nodes,
      .node_count = value->parse.node_count,
      .parse = value->parse,
  };
  fixture_init_output(value);
  CHECK(w_seed_frontend_run(&value->frontend_input, &value->frontend_output,
                            &value->frontend_result) == W_SEED_FRONTEND_OK ||
        value->frontend_result.status == W_SEED_FRONTEND_UNSUPPORTED ||
        value->frontend_result.status == W_SEED_FRONTEND_DIAGNOSTICS);
  return true;
}

static bool fixture_resolve_external_imports(fixture *value) {
  if (value == NULL || value->frontend_input.document_count != 1u)
    return false;
  w_seed_module_scan_result scan_result;
  if (w_seed_module_scan(&value->source, value->cst_nodes,
                         value->parse.node_count, &value->parse,
                         value->module_origins, ARRAY, &scan_result) !=
      W_SEED_MODULE_SCAN_OK) {
    return false;
  }
  for (size_t index = 0u; index < scan_result.written; index += 1u) {
    const w_seed_span path = value->module_origins[index].module_path_span;
    const w_seed_frontend_text path_text = {
        value->source_bytes + path.start_byte,
        path.end_byte - path.start_byte};
    size_t target = SIZE_MAX;
    for (size_t module = 0u;
         module < value->frontend_input.external_module_count; module += 1u) {
      const w_seed_frontend_text candidate =
          value->external_modules[module].module_id;
      if (candidate.length == path_text.length &&
          memcmp(candidate.data, path_text.data, path_text.length) == 0) {
        target = module;
        break;
      }
    }
    if (target == SIZE_MAX) return false;
    value->resolved_imports[index] = (w_seed_frontend_resolved_import){
        .source_document_index = 0u,
        .direct_import_ordinal =
            value->module_origins[index].direct_import_ordinal,
        .import_declaration_span = value->module_origins[index].declaration_span,
        .target_kind = W_SEED_FRONTEND_RESOLVED_IMPORT_EXTERNAL_MODULE,
        .target_index = (uint32_t)target};
  }
  value->frontend_input.import_resolution_complete = true;
  value->frontend_input.resolved_imports = value->resolved_imports;
  value->frontend_input.resolved_import_count = scan_result.written;
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

static bool fixture_lower_with_unary_operator(fixture *value, const char *text,
                                              const char *operator_text) {
  CHECK(value != NULL && text != NULL && operator_text != NULL);
  CHECK(fixture_parse(value, text));
  size_t unary_count = 0u;
  for (size_t index = 0u;
       index < value->frontend_result.written.expressions; index += 1u) {
    w_seed_frontend_expression *expression =
        &value->expressions[index];
    if (expression->kind != W_SEED_FRONTEND_EXPR_UNARY) continue;
    expression->operator_text =
        (w_seed_frontend_text){operator_text, strlen(operator_text)};
    unary_count += 1u;
  }
  CHECK(unary_count != 0u);
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

static bool fixture_constir_valid(const fixture *value) {
  const w_seed_constir_program program = fixture_program(value);
  return w_seed_constir_validate_program(&program);
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

static void complement_case(uint16_t width, bool signed_value, size_t case_index,
                            uint8_t input[W_SEED_CONSTIR_INTEGER_BYTES],
                            uint8_t expected[W_SEED_CONSTIR_INTEGER_BYTES]) {
  (void)memset(input, 0, W_SEED_CONSTIR_INTEGER_BYTES);
  const size_t bytes = (size_t)width / 8u;
  if (case_index == 1u) {
    for (size_t index = 0u; index < bytes; index += 1u)
      input[index] = 0xffu;
    if (signed_value) {
      for (size_t index = bytes; index < W_SEED_CONSTIR_INTEGER_BYTES;
           index += 1u)
        input[index] = 0xffu;
    }
  } else if (case_index == 2u) {
    input[bytes - 1u] = 0x80u;
    if (signed_value) {
      for (size_t index = bytes; index < W_SEED_CONSTIR_INTEGER_BYTES;
           index += 1u)
        input[index] = 0xffu;
    }
  }
  for (size_t index = 0u; index < W_SEED_CONSTIR_INTEGER_BYTES; index += 1u)
    expected[index] = (uint8_t)~input[index];
  if (!signed_value) {
    for (size_t index = bytes; index < W_SEED_CONSTIR_INTEGER_BYTES;
         index += 1u)
      expected[index] = 0u;
  } else if ((expected[bytes - 1u] & 0x80u) != 0u) {
    for (size_t index = bytes; index < W_SEED_CONSTIR_INTEGER_BYTES;
         index += 1u)
      expected[index] = 0xffu;
  }
}

static bool test_integer_bitwise_complement(void) {
  static const char source[] =
      "const fn complementU8(value: u8): u8 { return ~value }\n"
      "const fn complementU64(value: u64): u64 { return ~value }\n"
      "const fn complementI8(value: i8): i8 { return ~value }\n"
      "const fn complementI64(value: i64): i64 { return ~value }\n";
  static const char negation_source[] =
      "const fn complementU8(value: u8): u8 { return -value }\n"
      "const fn complementU64(value: u64): u64 { return -value }\n"
      "const fn complementI8(value: i8): i8 { return -value }\n"
      "const fn complementI64(value: i64): i64 { return -value }\n";
  static const uint16_t widths[] = {8u, 64u, 8u, 64u};
  static const bool signedness[] = {false, false, true, true};
  CHECK(fixture_lower(&first_fixture, source));
  CHECK(fixture_lower(&second_fixture, negation_source));
  CHECK(first_fixture.constir_result.written.functions == 4u &&
        first_fixture.constir_result.written.parameters == 4u &&
        second_fixture.constir_result.written.functions == 4u);
  CHECK(memcmp(first_fixture.constir_functions[0].body_digest,
               second_fixture.constir_functions[0].body_digest, 32u) != 0);

  const w_seed_constir_program program = fixture_program(&first_fixture);
  for (uint32_t function_index = 0u; function_index < 4u;
       function_index += 1u) {
    const w_seed_constir_function *function =
        &first_fixture.constir_functions[function_index];
    CHECK(function->lowerable && function->root_node != W_SEED_CONSTIR_NONE);
    const w_seed_constir_node *root =
        &first_fixture.constir_nodes[function->root_node];
    CHECK(root->kind == W_SEED_CONSTIR_NODE_UNARY &&
          root->normalized_operator == W_SEED_CONSTIR_OPERATOR_BIT_NOT &&
          root->type_kind == W_SEED_FRONTEND_TYPE_INTEGER &&
          root->type_is_signed == signedness[function_index] &&
          root->type_bit_width == widths[function_index]);
    for (size_t case_index = 0u; case_index < 3u; case_index += 1u) {
      uint8_t input_bytes[W_SEED_CONSTIR_INTEGER_BYTES];
      uint8_t expected_bytes[W_SEED_CONSTIR_INTEGER_BYTES];
      complement_case(widths[function_index], signedness[function_index],
                      case_index, input_bytes, expected_bytes);
      w_seed_constir_value argument;
      CHECK(w_seed_constir_value_integer(
          first_fixture.constir_parameters[function_index].type_index,
          W_SEED_FRONTEND_TYPE_INTEGER, signedness[function_index],
          widths[function_index], input_bytes, &argument));
      w_seed_constir_value output;
      w_seed_constir_eval_result evaluation;
      w_seed_constir_eval_frame frames[2];
      w_seed_constir_eval_workspace workspace = {frames, 2u};
      CHECK(w_seed_constir_evaluate(
                &program, function_index, &argument, 1u,
                (w_seed_constir_quota){16u, 0u, 1u, SIZE_MAX}, &workspace,
                &output, &evaluation) == W_SEED_CONSTIR_OK &&
            evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
            output.kind == W_SEED_CONSTIR_VALUE_INTEGER &&
            output.type_kind == W_SEED_FRONTEND_TYPE_INTEGER &&
            output.type_is_signed == signedness[function_index] &&
            output.type_bit_width == widths[function_index] &&
            memcmp(output.integer_value, expected_bytes,
                   W_SEED_CONSTIR_INTEGER_BYTES) == 0);
    }
  }

  /* A valid complement node must not accept a binary operator forgery. */
  const uint32_t root_index = first_fixture.constir_functions[0].root_node;
  w_seed_constir_node saved_root = first_fixture.constir_nodes[root_index];
  first_fixture.constir_nodes[root_index].normalized_operator =
      W_SEED_CONSTIR_OPERATOR_POWER;
  CHECK(!w_seed_constir_validate_program(&program));
  w_seed_constir_value argument;
  uint8_t zero_bytes[W_SEED_CONSTIR_INTEGER_BYTES] = {0u};
  CHECK(w_seed_constir_value_integer(
      first_fixture.constir_parameters[0].type_index,
      W_SEED_FRONTEND_TYPE_INTEGER, false, 8u, zero_bytes, &argument));
  w_seed_constir_value output;
  w_seed_constir_eval_result evaluation;
  w_seed_constir_eval_frame frames[2];
  w_seed_constir_eval_workspace workspace = {frames, 2u};
  CHECK(w_seed_constir_evaluate(
            &program, 0u, &argument, 1u,
            (w_seed_constir_quota){16u, 0u, 1u, SIZE_MAX}, &workspace, &output,
            &evaluation) == W_SEED_CONSTIR_INVALID &&
        evaluation.consumed_steps == 0u);
  first_fixture.constir_nodes[root_index] = saved_root;

  /* The same node shape rejects a non-integer result type forgery. */
  first_fixture.constir_nodes[root_index].type_kind =
      W_SEED_FRONTEND_TYPE_BOOL;
  first_fixture.constir_nodes[root_index].type_is_signed = false;
  first_fixture.constir_nodes[root_index].type_bit_width = 0u;
  first_fixture.constir_nodes[root_index].enum_base_index =
      W_SEED_CONSTIR_NONE;
  CHECK(!w_seed_constir_validate_program(&program));
  first_fixture.constir_nodes[root_index] = saved_root;

  static const char bool_source[] =
      "const fn boolComplement(value: Bool): Bool { return !value }\n";
  CHECK(fixture_lower_with_unary_operator(&second_fixture, bool_source, "~"));
  CHECK(second_fixture.constir_result.written.functions == 1u &&
        !second_fixture.constir_functions[0].lowerable &&
        second_fixture.constir_result.written.nodes == 0u);
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
      "const fn mixedAdd(left: u8, right: u16): u16 { return left + right }\n"
      "const fn mixedUnsignedSigned(value: u8): i16 { return value }\n";
  CHECK(fixture_lower(value, mixed_width));
  CHECK(value->constir_result.written.functions == 4u &&
        value->constir_result.written.parameters == 7u);
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
  CHECK(w_seed_constir_value_integer(
      value->constir_parameters[6].type_index, W_SEED_FRONTEND_TYPE_INTEGER,
      false, 8u, two_fifty_five, &mixed_arguments[0]));
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
            3u, mixed_arguments, 1u,
            (w_seed_constir_quota){100u, 0u, 1u, SIZE_MAX}, &workspace, &output,
            &evaluation) == W_SEED_CONSTIR_OK &&
        evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        output.kind == W_SEED_CONSTIR_VALUE_INTEGER &&
        output.type_is_signed && output.type_bit_width == 16u &&
        output.integer_value[0] == 0xffu && output.integer_value[1] == 0u);

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
      "const fn helper(_ value: u8): u8 { return value + 1_u8 }\n";
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
      {"Bool", 4}, true, {NULL, 0u}};
  value->external_modules[0] =
      (w_seed_frontend_external_module){{"external", 8},
                                        value->external_symbols, 1u};
  value->frontend_input.external_modules = value->external_modules;
  value->frontend_input.external_module_count = 1u;
  CHECK(fixture_resolve_external_imports(value));
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
      "const fn helper(_ value: u8): u8 { return value + 1_u8 }\n"
      "const fn caller(_ value: u8): u8 { return helper(value) }\n"
      "const fn move(from current: Stage, to next: Stage): Bool { return switch current { "
      "case .accepted: next in (.reserving, .cancelled) "
      "case .reserving: next in (.preparing, .cancelled) "
      "case .preparing: next in (.serving, .cancelled) "
      "case .serving: next in (.completed, .cancelled) "
      "case .completed: false case .cancelled: false } }\n"
      "const fn structured(_ stages: StaticList<Stage>): Bool {\n"
      "for index in 0..<stages.count {\n"
      "return true\n"
      "}\n"
      "return false\n"
      "}\n"
      "const fn bad(_ value: u8): u8 { let local = value return local }\n";
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
  CHECK(first_byte_equals(value->constir_functions, 0xa5u) &&                  \
        first_byte_equals(value->constir_parameters, 0xa5u) &&                 \
        first_byte_equals(value->constir_nodes, 0xa5u) &&                      \
        first_byte_equals(value->constir_arguments, 0xa5u) &&                  \
        first_byte_equals(value->constir_switch, 0xa5u) &&                     \
        first_byte_equals(value->constir_membership, 0xa5u) &&                 \
        first_byte_equals(value->constir_statements, 0xa5u) &&                 \
        first_byte_equals(value->constir_locals, 0xa5u) &&                     \
        first_byte_equals(value->constir_diagnostics, 0xa5u) &&                \
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

static bool test_typed_const_expression_synthetic(void) {
  static const char source[] =
      "const fn isUltimateAnswer(value: i64): Bool { return value == 42 }\n"
      "struct UltimateAnswer<_ value: i64<(isUltimateAnswer(.member))>> {}\n"
      "struct Use { let immediate: UltimateAnswer<42> let computed: "
      "UltimateAnswer<(6 * 7)> let duplicate: UltimateAnswer<(6 * 7)> }\n";
  CHECK(fixture_lower(&first_fixture, source));
  CHECK(first_fixture.frontend_result.written.typed_const_expressions == 2u &&
        first_fixture.constir_result.written.functions == 3u);
  const w_seed_constir_function *predicate =
      &first_fixture.constir_functions[0];
  const w_seed_constir_function *computed =
      &first_fixture.constir_functions[1];
  const w_seed_constir_function *duplicate =
      &first_fixture.constir_functions[2];
  CHECK(predicate->origin == W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_FUNCTION &&
        predicate->frontend_function == 0u &&
        predicate->typed_const_expression_index == W_SEED_CONSTIR_NONE &&
        computed->origin ==
            W_SEED_CONSTIR_FUNCTION_ORIGIN_TYPED_CONST_EXPRESSION &&
        computed->frontend_function == W_SEED_CONSTIR_NONE &&
        computed->typed_const_expression_index == 0u &&
        computed->parameter_count == 0u && computed->lowerable &&
        duplicate->origin ==
            W_SEED_CONSTIR_FUNCTION_ORIGIN_TYPED_CONST_EXPRESSION &&
        duplicate->typed_const_expression_index == 1u && duplicate->lowerable &&
        memcmp(computed->body_digest, duplicate->body_digest, 32u) == 0);
  const w_seed_constir_node *computed_node =
      &first_fixture.constir_nodes[computed->root_node];
  CHECK(computed_node->type_kind == W_SEED_FRONTEND_TYPE_INTEGER &&
        computed_node->type_bit_width == 64u &&
        computed_node->type_is_signed);
  const size_t receipt_prefix = strlen(W_SEED_CONSTIR_SCHEMA_VERSION);
  const size_t receipt_function_bytes = 94u;
  CHECK(first_fixture.constir_receipt[receipt_prefix] ==
            (uint8_t)W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_FUNCTION &&
        first_fixture.constir_receipt[receipt_prefix + receipt_function_bytes] ==
            (uint8_t)W_SEED_CONSTIR_FUNCTION_ORIGIN_TYPED_CONST_EXPRESSION &&
        first_fixture.constir_receipt[receipt_prefix + receipt_function_bytes * 2u] ==
            (uint8_t)W_SEED_CONSTIR_FUNCTION_ORIGIN_TYPED_CONST_EXPRESSION);
  CHECK(fixture_constir_valid(&first_fixture));

  CHECK(fixture_lower(&second_fixture, source));
  CHECK(second_fixture.constir_result.written.receipt_bytes ==
        first_fixture.constir_result.written.receipt_bytes &&
        memcmp(first_fixture.constir_receipt, second_fixture.constir_receipt,
               first_fixture.constir_result.written.receipt_bytes) == 0 &&
        memcmp(first_fixture.constir_functions[1].body_digest,
               second_fixture.constir_functions[1].body_digest, 32u) == 0);

  CHECK(fixture_lower(&first_fixture, source));
  first_fixture.constir_functions[1].origin =
      W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_FUNCTION;
  CHECK(!fixture_constir_valid(&first_fixture));
  CHECK(fixture_lower(&first_fixture, source));
  first_fixture.constir_functions[2].typed_const_expression_index = 0u;
  CHECK(!fixture_constir_valid(&first_fixture));
  CHECK(fixture_lower(&first_fixture, source));
  first_fixture.constir_nodes[first_fixture.constir_functions[1].root_node]
      .type_bit_width = 32u;
  CHECK(!fixture_constir_valid(&first_fixture));
  CHECK(fixture_lower(&first_fixture, source));
  first_fixture.generic_applications[1].binding_status =
      W_SEED_FRONTEND_GENERIC_BINDING_INVALID;
  CHECK(!fixture_constir_valid(&first_fixture));

  static const char unsupported_call[] =
      "const fn helper(_ value: i64): i64 { return value }\n"
      "struct Box<_ value: i64> {}\n"
      "struct Use { let value: Box<(helper(6))> }\n";
  CHECK(fixture_lower(&first_fixture, unsupported_call));
  CHECK(first_fixture.constir_result.written.functions == 2u &&
        !first_fixture.constir_functions[1].lowerable &&
        fixture_constir_valid(&first_fixture));

  /* A calculated relation retained for an UNSUPPORTED application is audit
   * data only.  ConstIR may keep its non-lowerable synthetic record, but the
   * origin validator must not make that record executable. */
  static const char audit_only[] =
      "struct Pair<_ first: i64, _ second: i64> {}\n"
      "struct Use { let pair: Pair<(6 * 7), (\"42\")> }\n";
  CHECK(fixture_lower(&first_fixture, audit_only));
  CHECK(first_fixture.frontend_result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        first_fixture.frontend_result.written.typed_const_expressions == 1u &&
        first_fixture.generic_applications[0].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_UNSUPPORTED &&
        first_fixture.constir_result.written.functions == 1u &&
        !first_fixture.constir_functions[0].lowerable &&
        fixture_constir_valid(&first_fixture));
  return true;
}

static bool test_module_const_synthetic_d4(void) {
  static const char source[] =
      "export const ultimateAnswer: i64 = 6 * 7\n"
      "const forward: i64 = ultimateAnswer\n"
      "struct Box<_ value: i64> {}\n"
      "struct Use { let named: Box<(forward)> let direct: Box<(ultimateAnswer)> }\n";
  CHECK(fixture_lower(&first_fixture, source));
  CHECK(first_fixture.frontend_result.written.const_declarations == 2u &&
        first_fixture.constir_result.written.functions == 4u &&
        first_fixture.constir_functions[0].origin ==
            W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_CONST_DECLARATION &&
        first_fixture.constir_functions[1].origin ==
            W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_CONST_DECLARATION &&
        first_fixture.constir_functions[2].origin ==
            W_SEED_CONSTIR_FUNCTION_ORIGIN_TYPED_CONST_EXPRESSION &&
        first_fixture.constir_functions[3].origin ==
            W_SEED_CONSTIR_FUNCTION_ORIGIN_TYPED_CONST_EXPRESSION);
  const w_seed_constir_function *forward = &first_fixture.constir_functions[1];
  CHECK(forward->frontend_const_declaration == 1u && forward->lowerable &&
        forward->root_node != W_SEED_CONSTIR_NONE);
  const w_seed_constir_node *forward_node =
      &first_fixture.constir_nodes[forward->root_node];
  CHECK(forward_node->kind == W_SEED_CONSTIR_NODE_CALL &&
        forward_node->call_target_function == W_SEED_CONSTIR_NONE &&
        forward_node->call_target_const_declaration == 0u);
  CHECK(fixture_constir_valid(&first_fixture));
  uint8_t explicit_body_digests[2][32];
  (void)memcpy(explicit_body_digests[0],
               first_fixture.constir_functions[0].body_digest, 32u);
  (void)memcpy(explicit_body_digests[1],
               first_fixture.constir_functions[1].body_digest, 32u);

  static const char changed_source[] =
      "export const ultimateAnswer: i64 = 6 * 8\n"
      "const forward: i64 = ultimateAnswer\n"
      "struct Box<_ value: i64> {}\n"
      "struct Use { let named: Box<(forward)> let direct: Box<(ultimateAnswer)> }\n";
  CHECK(fixture_lower(&second_fixture, changed_source));
  CHECK(memcmp(first_fixture.constir_functions[1].body_digest,
               second_fixture.constir_functions[1].body_digest, 32u) != 0);

  static const char cycle_source[] =
      "const left: i64 = right\n"
      "const right: i64 = left\n"
      "struct Box<_ value: i64> {}\n"
      "struct Use { let value: Box<(left)> }\n";
  CHECK(fixture_lower(&first_fixture, cycle_source));
  CHECK(first_fixture.constir_result.written.functions == 3u &&
        first_fixture.constir_functions[0].lowerable &&
        first_fixture.constir_functions[1].lowerable &&
        fixture_constir_valid(&first_fixture));

  static const char inferred_source[] =
      "export const ultimateAnswer = 6 * 7\n"
      "const forward = ultimateAnswer\n"
      "struct Box<_ value: i64> {}\n"
      "struct Use { let named: Box<(forward)> let direct: Box<(ultimateAnswer)> }\n";
  CHECK(fixture_lower(&first_fixture, inferred_source));
  CHECK(first_fixture.frontend_result.status == W_SEED_FRONTEND_OK &&
        first_fixture.frontend_result.written.const_declarations == 2u &&
        first_fixture.constir_result.written.functions == 4u &&
        !first_fixture.const_declarations[0].has_explicit_type &&
        !first_fixture.const_declarations[1].has_explicit_type &&
        first_fixture.const_declarations[0].declared_type ==
            W_SEED_FRONTEND_NONE &&
        first_fixture.const_declarations[1].declared_type ==
            W_SEED_FRONTEND_NONE && first_fixture.constir_functions[0].lowerable &&
        first_fixture.constir_functions[1].lowerable &&
        memcmp(explicit_body_digests[0],
               first_fixture.constir_functions[0].body_digest, 32u) == 0 &&
        memcmp(explicit_body_digests[1],
               first_fixture.constir_functions[1].body_digest, 32u) == 0);
  CHECK(fixture_constir_valid(&first_fixture));

  static const char nonlowerable_module_consts[] =
      "const callValue = helper(6)\n"
      "const stringValue = \"42\"\n"
      "const listValue = [1, 2]\n"
      "const unresolvedValue = missing\n"
      "const fn helper(_ value: i64): i64 { return value }\n"
      "struct Use {}\n";
  CHECK(fixture_lower(&first_fixture, nonlowerable_module_consts));
  CHECK(first_fixture.frontend_result.written.const_declarations == 4u);
  for (size_t index = 0u; index < 4u; index += 1u)
    CHECK(first_fixture.const_declarations[index].declared_type ==
              W_SEED_FRONTEND_NONE &&
          first_fixture.const_declarations[index].effective_type ==
              W_SEED_FRONTEND_NONE);
  for (size_t index = 0u; index < first_fixture.constir_result.written.functions;
       index += 1u) {
    const w_seed_constir_function *function =
        &first_fixture.constir_functions[index];
    if (function->origin ==
        W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_CONST_DECLARATION) {
      CHECK(function->frontend_const_declaration < 4u &&
            !function->lowerable && function->root_node == W_SEED_CONSTIR_NONE &&
            function->node_count == 0u);
    }
  }
  CHECK(fixture_constir_valid(&first_fixture));
  return true;
}

static bool test_module_const_active_cycle_defense(void) {
  static const char source[] =
      "const left: i64 = right\n"
      "const right: i64 = left\n"
      "struct Box<_ value: i64> {}\n"
      "struct Use { let cycle: Box<(left)> }\n";
  fixture *value = &first_fixture;
  CHECK(fixture_lower(value, source));
  CHECK(value->frontend_result.written.const_declarations == 2u &&
        value->constir_result.written.functions == 3u &&
        fixture_constir_valid(value));
  const w_seed_constir_program program = fixture_program(value);
  w_seed_constir_eval_frame frames[8];
  w_seed_constir_eval_workspace workspace = {frames, 8u};
  w_seed_constir_value output;
  w_seed_constir_eval_result first;
  /* The direct entry is not pre-seeded.  Only its two memoized CALL targets
   * contribute misses before the ACTIVE defense closes the cycle. */
  CHECK(w_seed_constir_evaluate(
            &program, 0u, NULL, 0u,
            (w_seed_constir_quota){100u, 0u, 8u, SIZE_MAX}, &workspace,
            &output, &first) == W_SEED_CONSTIR_OK &&
        first.status == W_SEED_CONSTIR_OK &&
        first.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0002 &&
        output.kind == W_SEED_CONSTIR_VALUE_INVALID &&
        first.consumed_steps == 3u && first.consumed_call_depth == 3u &&
        first.const_cache_hits == 0u && first.const_cache_misses == 2u);
  w_seed_constir_eval_result second;
  CHECK(w_seed_constir_evaluate(
            &program, 0u, NULL, 0u,
            (w_seed_constir_quota){100u, 0u, 8u, SIZE_MAX}, &workspace,
            &output, &second) == W_SEED_CONSTIR_OK &&
        second.status == W_SEED_CONSTIR_OK &&
        second.diagnostic == first.diagnostic &&
        output.kind == W_SEED_CONSTIR_VALUE_INVALID &&
        second.consumed_steps == first.consumed_steps &&
        second.consumed_call_depth == first.consumed_call_depth &&
        second.const_cache_hits == first.const_cache_hits &&
        second.const_cache_misses == first.const_cache_misses);
  return true;
}

static bool constir_u64_is(const w_seed_constir_value *value, uint64_t expected) {
  if (value == NULL || value->kind != W_SEED_CONSTIR_VALUE_INTEGER ||
      value->type_kind != W_SEED_FRONTEND_TYPE_INTEGER ||
      value->type_is_signed || value->type_bit_width != 64u)
    return false;
  for (size_t index = 8u; index < W_SEED_CONSTIR_INTEGER_BYTES; index += 1u)
    if (value->integer_value[index] != 0u) return false;
  for (size_t index = 0u; index < 8u; index += 1u)
    if (value->integer_value[index] != (uint8_t)(expected >> (index * 8u)))
      return false;
  return true;
}

static bool constir_u64_bool_tuple_is(const w_seed_constir_value *value,
                                      uint64_t expected, bool overflowed) {
  if (value == NULL ||
      value->kind != W_SEED_CONSTIR_VALUE_U64_BOOL_TUPLE ||
      value->type_kind != W_SEED_FRONTEND_TYPE_TUPLE ||
      value->type_is_signed || value->type_bit_width != 0u ||
      value->bool_value != overflowed)
    return false;
  for (size_t index = 8u; index < W_SEED_CONSTIR_INTEGER_BYTES; index += 1u)
    if (value->integer_value[index] != 0u) return false;
  for (size_t index = 0u; index < 8u; index += 1u)
    if (value->integer_value[index] != (uint8_t)(expected >> (index * 8u)))
      return false;
  return true;
}

static uint32_t constir_function_for_frontend(const fixture *value,
                                              uint32_t frontend_function) {
  if (value == NULL) return W_SEED_CONSTIR_NONE;
  for (size_t index = 0u; index < value->constir_result.written.functions;
       index += 1u) {
    const w_seed_constir_function *function = &value->constir_functions[index];
    if (function->origin == W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_FUNCTION &&
        function->frontend_function == frontend_function)
      return (uint32_t)index;
  }
  return W_SEED_CONSTIR_NONE;
}

static uint32_t constir_function_for_const(const fixture *value,
                                           uint32_t const_declaration) {
  if (value == NULL) return W_SEED_CONSTIR_NONE;
  for (size_t index = 0u; index < value->constir_result.written.functions;
       index += 1u) {
    const w_seed_constir_function *function = &value->constir_functions[index];
    if (function->origin ==
            W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_CONST_DECLARATION &&
        function->frontend_const_declaration == const_declaration)
      return (uint32_t)index;
  }
  return W_SEED_CONSTIR_NONE;
}

static bool constir_builtin_root_is(const fixture *value, uint32_t function_index,
                                    w_seed_frontend_builtin_operation operation) {
  if (value == NULL || function_index >= value->constir_result.written.functions)
    return false;
  const w_seed_constir_function *function = &value->constir_functions[function_index];
  if (!function->lowerable || function->root_node == W_SEED_CONSTIR_NONE ||
      function->root_node >= value->constir_result.written.nodes)
    return false;
  const w_seed_constir_node *node = &value->constir_nodes[function->root_node];
  return node->kind == W_SEED_CONSTIR_NODE_BUILTIN_U64 &&
         node->builtin_operation == operation &&
         node->type_kind == W_SEED_FRONTEND_TYPE_TUPLE &&
         !node->type_is_signed && node->type_bit_width == 0u;
}

static bool constir_projection_root_is(const fixture *value,
                                       uint32_t function_index,
                                       uint32_t element_index,
                                       uint32_t target_function,
                                       uint32_t target_const) {
  if (value == NULL || function_index >= value->constir_result.written.functions)
    return false;
  const w_seed_constir_function *function = &value->constir_functions[function_index];
  if (!function->lowerable || function->root_node == W_SEED_CONSTIR_NONE ||
      function->root_node >= value->constir_result.written.nodes)
    return false;
  const w_seed_constir_node *projection =
      &value->constir_nodes[function->root_node];
  if (projection->kind != W_SEED_CONSTIR_NODE_TUPLE_ELEMENT ||
      projection->tuple_element_index != element_index ||
      projection->left == W_SEED_CONSTIR_NONE ||
      projection->left >= value->constir_result.written.nodes)
    return false;
  const w_seed_constir_node *call = &value->constir_nodes[projection->left];
  return call->kind == W_SEED_CONSTIR_NODE_CALL &&
         call->call_target_function == target_function &&
         call->call_target_const_declaration == target_const;
}

static bool constir_const_repeat_is(const fixture *value,
                                    uint32_t function_index,
                                    uint32_t target_const) {
  if (value == NULL || function_index >= value->constir_result.written.functions)
    return false;
  const w_seed_constir_function *function = &value->constir_functions[function_index];
  if (!function->lowerable || function->root_node == W_SEED_CONSTIR_NONE ||
      function->root_node >= value->constir_result.written.nodes)
    return false;
  const w_seed_constir_node *binary = &value->constir_nodes[function->root_node];
  if (binary->kind != W_SEED_CONSTIR_NODE_BINARY ||
      binary->normalized_operator != W_SEED_CONSTIR_OPERATOR_EQUAL ||
      binary->left == W_SEED_CONSTIR_NONE || binary->right == W_SEED_CONSTIR_NONE ||
      binary->left >= value->constir_result.written.nodes ||
      binary->right >= value->constir_result.written.nodes)
    return false;
  const w_seed_constir_node *left = &value->constir_nodes[binary->left];
  const w_seed_constir_node *right = &value->constir_nodes[binary->right];
  return left->kind == W_SEED_CONSTIR_NODE_TUPLE_ELEMENT &&
         right->kind == W_SEED_CONSTIR_NODE_TUPLE_ELEMENT &&
         left->tuple_element_index == 0u && right->tuple_element_index == 0u &&
         left->left != W_SEED_CONSTIR_NONE &&
         right->left != W_SEED_CONSTIR_NONE &&
         left->left < value->constir_result.written.nodes &&
         right->left < value->constir_result.written.nodes &&
         value->constir_nodes[left->left].kind == W_SEED_CONSTIR_NODE_CALL &&
         value->constir_nodes[right->left].kind == W_SEED_CONSTIR_NODE_CALL &&
         value->constir_nodes[left->left].call_target_const_declaration ==
             target_const &&
         value->constir_nodes[right->left].call_target_const_declaration ==
             target_const;
}

static bool test_u64_policy_constir(void) {
  static const char source[] =
      "const fn satAdd(): u64 { return u64.saturatingAdd(18446744073709551615_u64, 1_u64) }\n"
      "const fn satSubtract(): u64 { return u64.saturatingSubtract(0_u64, 1_u64) }\n"
      "const fn satMultiply(): u64 { return u64.saturatingMultiply(18446744073709551615_u64, 2_u64) }\n"
      "const fn satNegate(): u64 { return u64.saturatingNegate(18446744073709551615_u64) }\n"
      "const fn satPower(): u64 { return u64.saturatingPower(2_u64, 64_u64) }\n"
      "const fn satZeroPowerZero(): u64 { return u64.saturatingPower(0_u64, 0_u64) }\n"
      "const fn ovAddValue(): u64 { return u64.overflowingAdd(18446744073709551615_u64, 1_u64).0 }\n"
      "const fn ovAddFlag(): Bool { return u64.overflowingAdd(18446744073709551615_u64, 1_u64).1 }\n"
      "const fn ovSubtractValue(): u64 { return u64.overflowingSubtract(0_u64, 1_u64).0 }\n"
      "const fn ovSubtractFlag(): Bool { return u64.overflowingSubtract(0_u64, 1_u64).1 }\n"
      "const fn ovMultiplyValue(): u64 { return u64.overflowingMultiply(18446744073709551615_u64, 2_u64).0 }\n"
      "const fn ovMultiplyFlag(): Bool { return u64.overflowingMultiply(18446744073709551615_u64, 2_u64).1 }\n"
      "const fn ovNegateValue(): u64 { return u64.overflowingNegate(1_u64).0 }\n"
      "const fn ovNegateFlag(): Bool { return u64.overflowingNegate(1_u64).1 }\n"
      "const fn ovPowerValue(): u64 { return u64.overflowingPower(2_u64, 64_u64).0 }\n"
      "const fn ovPowerFlag(): Bool { return u64.overflowingPower(2_u64, 64_u64).1 }\n"
      "const fn ovZeroPowerZeroValue(): u64 { return u64.overflowingPower(0_u64, 0_u64).0 }\n"
      "const fn ovZeroPowerZeroFlag(): Bool { return u64.overflowingPower(0_u64, 0_u64).1 }\n";
  fixture *value = &first_fixture;
  CHECK(fixture_lower(value, source));
  CHECK(value->constir_result.written.functions == 18u &&
        value->constir_result.written.nodes != 0u &&
        value->constir_result.written.receipt_bytes != 0u);
  CHECK(fixture_constir_valid(value));
  size_t builtin_count = 0u;
  size_t projection_count = 0u;
  size_t projection_zero_count = 0u;
  size_t projection_one_count = 0u;
  uint32_t builtin_node_index = W_SEED_CONSTIR_NONE;
  uint32_t projection_node_index = W_SEED_CONSTIR_NONE;
  bool saw_policy[10] = {false};
  for (size_t index = 0u; index < value->constir_result.written.nodes;
       index += 1u) {
    const w_seed_constir_node *node = &value->constir_nodes[index];
    if (node->kind == W_SEED_CONSTIR_NODE_BUILTIN_U64) {
      bool tuple = false;
      bool unary = false;
      size_t policy_index = 0u;
      switch (node->builtin_operation) {
        case W_SEED_FRONTEND_BUILTIN_U64_SATURATING_ADD:
          policy_index = 0u;
          break;
        case W_SEED_FRONTEND_BUILTIN_U64_SATURATING_SUBTRACT:
          policy_index = 1u;
          break;
        case W_SEED_FRONTEND_BUILTIN_U64_SATURATING_MULTIPLY:
          policy_index = 2u;
          break;
        case W_SEED_FRONTEND_BUILTIN_U64_SATURATING_NEGATE:
          policy_index = 3u;
          unary = true;
          break;
        case W_SEED_FRONTEND_BUILTIN_U64_SATURATING_POWER:
          policy_index = 4u;
          break;
        case W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_ADD:
          policy_index = 5u;
          tuple = true;
          break;
        case W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_SUBTRACT:
          policy_index = 6u;
          tuple = true;
          break;
        case W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_MULTIPLY:
          policy_index = 7u;
          tuple = true;
          break;
        case W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_NEGATE:
          policy_index = 8u;
          tuple = true;
          unary = true;
          break;
        case W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_POWER:
          policy_index = 9u;
          tuple = true;
          break;
        default:
          CHECK(false);
      }
      saw_policy[policy_index] = true;
      builtin_count += 1u;
      if (builtin_node_index == W_SEED_CONSTIR_NONE)
        builtin_node_index = (uint32_t)index;
      CHECK(node->left != W_SEED_CONSTIR_NONE &&
            (unary ? node->right == W_SEED_CONSTIR_NONE
                   : node->right != W_SEED_CONSTIR_NONE));
      if (tuple)
        CHECK(node->type_kind == W_SEED_FRONTEND_TYPE_TUPLE &&
              !node->type_is_signed && node->type_bit_width == 0u);
      else
        CHECK(node->type_kind == W_SEED_FRONTEND_TYPE_INTEGER &&
              !node->type_is_signed && node->type_bit_width == 64u);
    } else if (node->kind == W_SEED_CONSTIR_NODE_TUPLE_ELEMENT) {
      projection_count += 1u;
      if (projection_node_index == W_SEED_CONSTIR_NONE)
        projection_node_index = (uint32_t)index;
      CHECK(node->left != W_SEED_CONSTIR_NONE &&
            node->tuple_element_index <= 1u);
      if (node->tuple_element_index == 0u) {
        projection_zero_count += 1u;
        CHECK(node->type_kind == W_SEED_FRONTEND_TYPE_INTEGER &&
              !node->type_is_signed && node->type_bit_width == 64u);
      } else {
        projection_one_count += 1u;
        CHECK(node->type_kind == W_SEED_FRONTEND_TYPE_BOOL &&
              !node->type_is_signed && node->type_bit_width == 0u);
      }
    }
  }
  CHECK(builtin_count == 18u && projection_count == 12u &&
        projection_zero_count == 6u && projection_one_count == 6u &&
        builtin_node_index != W_SEED_CONSTIR_NONE &&
        projection_node_index != W_SEED_CONSTIR_NONE);
  for (size_t index = 0u; index < sizeof(saw_policy) / sizeof(saw_policy[0]);
       index += 1u)
    CHECK(saw_policy[index]);
  const w_seed_constir_program program = fixture_program(value);
  w_seed_constir_eval_frame frames[8];
  w_seed_constir_eval_workspace workspace = {frames, 8u};
  const uint64_t scalar_expected[] = {
      UINT64_MAX, 0u, UINT64_MAX, 0u, UINT64_MAX, 1u};
  for (size_t function_index = 0u; function_index < 6u; function_index += 1u) {
    w_seed_constir_value output;
    w_seed_constir_eval_result result;
    CHECK(w_seed_constir_evaluate(
              &program, (uint32_t)function_index, NULL, 0u,
              (w_seed_constir_quota){1000u, 0u, 8u, SIZE_MAX}, &workspace,
              &output, &result) == W_SEED_CONSTIR_OK &&
          result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
          result.consumed_heap_bytes == 0u &&
          constir_u64_is(&output, scalar_expected[function_index]));
  }
  w_seed_constir_value output;
  w_seed_constir_eval_result result;
  const uint64_t overflowing_values[] = {
      0u, 0u, UINT64_MAX, UINT64_MAX, UINT64_MAX - 1u, UINT64_MAX - 1u,
      UINT64_MAX, UINT64_MAX, 0u, 0u, 1u, 1u};
  const bool overflowing_flags[] = {true, true, true, true, true, true,
                                    true, true, true, true, false, false};
  for (size_t function_index = 6u; function_index < 18u;
       function_index += 1u) {
    CHECK(w_seed_constir_evaluate(
              &program, (uint32_t)function_index, NULL, 0u,
              (w_seed_constir_quota){1000u, 0u, 8u, SIZE_MAX}, &workspace,
              &output, &result) == W_SEED_CONSTIR_OK &&
          result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
          ((function_index - 6u) % 2u == 0u
               ? constir_u64_is(&output, overflowing_values[function_index - 6u])
               : (output.kind == W_SEED_CONSTIR_VALUE_BOOL &&
                  output.bool_value == overflowing_flags[function_index - 6u])));
  }
  w_seed_constir_node saved_builtin = value->constir_nodes[builtin_node_index];
  value->constir_nodes[builtin_node_index].builtin_operation =
      W_SEED_FRONTEND_BUILTIN_NONE;
  CHECK(!fixture_constir_valid(value));
  CHECK(w_seed_constir_evaluate(
            &program, 6u, NULL, 0u,
            (w_seed_constir_quota){1000u, 0u, 8u, SIZE_MAX}, &workspace,
            &output, &result) == W_SEED_CONSTIR_INVALID &&
        result.consumed_steps == 0u);
  value->constir_nodes[builtin_node_index] = saved_builtin;
  CHECK(fixture_constir_valid(value));
  saved_builtin = value->constir_nodes[builtin_node_index];
  value->constir_nodes[builtin_node_index].type_kind =
      W_SEED_FRONTEND_TYPE_BOOL;
  value->constir_nodes[builtin_node_index].type_bit_width = 0u;
  CHECK(!fixture_constir_valid(value));
  value->constir_nodes[builtin_node_index] = saved_builtin;
  CHECK(fixture_constir_valid(value));

  w_seed_constir_node saved_projection = value->constir_nodes[projection_node_index];
  value->constir_nodes[projection_node_index].tuple_element_index = 2u;
  CHECK(!fixture_constir_valid(value));
  value->constir_nodes[projection_node_index] = saved_projection;
  CHECK(fixture_constir_valid(value));
  saved_projection = value->constir_nodes[projection_node_index];
  value->constir_nodes[projection_node_index].type_kind =
      W_SEED_FRONTEND_TYPE_BOOL;
  value->constir_nodes[projection_node_index].type_bit_width = 0u;
  CHECK(!fixture_constir_valid(value));
  value->constir_nodes[projection_node_index] = saved_projection;
  CHECK(fixture_constir_valid(value));
  saved_projection = value->constir_nodes[projection_node_index];
  value->constir_nodes[projection_node_index].owner_function = UINT32_MAX - 1u;
  CHECK(!fixture_constir_valid(value));
  value->constir_nodes[projection_node_index] = saved_projection;
  CHECK(fixture_constir_valid(value));
  saved_projection = value->constir_nodes[projection_node_index];
  value->constir_nodes[projection_node_index].left = builtin_node_index;
  CHECK(!fixture_constir_valid(value));
  value->constir_nodes[projection_node_index] = saved_projection;
  CHECK(fixture_constir_valid(value));

  static const char boundary_source[] =
      "const fn satExactPower(): u64 { return u64.saturatingPower(2_u64, 63_u64) }\n"
      "const fn satMaximumSquare(): u64 { return u64.saturatingPower(18446744073709551615_u64, 2_u64) }\n"
      "const fn ovExactPowerValue(): u64 { return u64.overflowingPower(2_u64, 63_u64).0 }\n"
      "const fn ovExactPowerFlag(): Bool { return u64.overflowingPower(2_u64, 63_u64).1 }\n"
      "const fn ovMaximumSquareValue(): u64 { return u64.overflowingPower(18446744073709551615_u64, 2_u64).0 }\n"
      "const fn ovMaximumSquareFlag(): Bool { return u64.overflowingPower(18446744073709551615_u64, 2_u64).1 }\n"
      "const fn satNegateZero(): u64 { return u64.saturatingNegate(0_u64) }\n"
      "const fn ovNegateZeroValue(): u64 { return u64.overflowingNegate(0_u64).0 }\n"
      "const fn ovNegateZeroFlag(): Bool { return u64.overflowingNegate(0_u64).1 }\n"
      "const fn ovNegateMaximumValue(): u64 { return u64.overflowingNegate(18446744073709551615_u64).0 }\n"
      "const fn ovNegateMaximumFlag(): Bool { return u64.overflowingNegate(18446744073709551615_u64).1 }\n";
  CHECK(fixture_lower(&second_fixture, boundary_source));
  CHECK(second_fixture.constir_result.written.functions == 11u &&
        fixture_constir_valid(&second_fixture));
  const w_seed_constir_program boundary_program = fixture_program(&second_fixture);
  w_seed_constir_eval_frame boundary_frames[4];
  w_seed_constir_eval_workspace boundary_workspace = {boundary_frames, 4u};
  const uint64_t boundary_values[] = {
      UINT64_C(0x8000000000000000), UINT64_MAX, UINT64_C(0x8000000000000000),
      1u, 0u, 0u, 1u};
  const size_t boundary_value_indices[] = {0u, 1u, 2u, SIZE_MAX, 3u,
                                           SIZE_MAX, 4u, 5u, SIZE_MAX, 6u,
                                           SIZE_MAX};
  const bool boundary_flags[] = {false, true};
  for (size_t function_index = 0u; function_index < 11u;
       function_index += 1u) {
    w_seed_constir_value boundary_output;
    w_seed_constir_eval_result boundary_result;
    CHECK(w_seed_constir_evaluate(
              &boundary_program, (uint32_t)function_index, NULL, 0u,
              (w_seed_constir_quota){SIZE_MAX, 0u, 8u, SIZE_MAX},
              &boundary_workspace, &boundary_output, &boundary_result) ==
              W_SEED_CONSTIR_OK &&
          boundary_result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE);
    if (function_index == 3u || function_index == 5u || function_index == 8u ||
        function_index == 10u)
      CHECK(boundary_output.kind == W_SEED_CONSTIR_VALUE_BOOL &&
            boundary_output.bool_value ==
                ((function_index == 3u || function_index == 8u)
                     ? boundary_flags[0]
                     : boundary_flags[1]));
    else
      CHECK(boundary_value_indices[function_index] != SIZE_MAX &&
            constir_u64_is(&boundary_output,
                           boundary_values[boundary_value_indices[function_index]]));
  }
  w_seed_constir_value boundary_output;
  w_seed_constir_eval_result power_result;
  CHECK(w_seed_constir_evaluate(
            &boundary_program, 0u, NULL, 0u,
            (w_seed_constir_quota){SIZE_MAX, 0u, 8u, SIZE_MAX},
            &boundary_workspace, &boundary_output, &power_result) ==
            W_SEED_CONSTIR_OK &&
        power_result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        power_result.consumed_steps > 0u && power_result.consumed_steps < 63u);
  const size_t power_steps = power_result.consumed_steps;
  w_seed_constir_eval_result power_quota_first;
  CHECK(w_seed_constir_evaluate(
            &boundary_program, 0u, NULL, 0u,
            (w_seed_constir_quota){power_steps - 1u, 0u, 8u, SIZE_MAX},
            &boundary_workspace, &boundary_output, &power_quota_first) ==
            W_SEED_CONSTIR_OK &&
        power_quota_first.diagnostic ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003 &&
        power_quota_first.consumed_steps == power_steps - 1u &&
        boundary_output.kind == W_SEED_CONSTIR_VALUE_INVALID);
  w_seed_constir_eval_result power_quota_second;
  CHECK(w_seed_constir_evaluate(
            &boundary_program, 0u, NULL, 0u,
            (w_seed_constir_quota){power_steps - 1u, 0u, 8u, SIZE_MAX},
            &boundary_workspace, &boundary_output, &power_quota_second) ==
            W_SEED_CONSTIR_OK &&
        power_quota_second.diagnostic == power_quota_first.diagnostic &&
        power_quota_second.consumed_steps == power_quota_first.consumed_steps &&
        boundary_output.kind == W_SEED_CONSTIR_VALUE_INVALID);
  w_seed_constir_eval_result power_exact;
  CHECK(w_seed_constir_evaluate(
            &boundary_program, 0u, NULL, 0u,
            (w_seed_constir_quota){power_steps, 0u, 8u, SIZE_MAX},
            &boundary_workspace, &boundary_output, &power_exact) ==
            W_SEED_CONSTIR_OK &&
        power_exact.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        constir_u64_is(&boundary_output, UINT64_C(0x8000000000000000)));
  CHECK(power_exact.consumed_result_bytes > 0u);
  const size_t result_bytes = power_exact.consumed_result_bytes;
  w_seed_constir_eval_result result_quota;
  CHECK(w_seed_constir_evaluate(
            &boundary_program, 0u, NULL, 0u,
            (w_seed_constir_quota){SIZE_MAX, 0u, 8u, result_bytes - 1u},
            &boundary_workspace, &boundary_output, &result_quota) ==
            W_SEED_CONSTIR_OK &&
        result_quota.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003 &&
        result_quota.consumed_result_bytes == result_bytes &&
        boundary_output.kind == W_SEED_CONSTIR_VALUE_INVALID);
  CHECK(w_seed_constir_evaluate(
            &boundary_program, 0u, NULL, 0u,
            (w_seed_constir_quota){SIZE_MAX, 0u, 8u, result_bytes},
            &boundary_workspace, &boundary_output, &result_quota) ==
            W_SEED_CONSTIR_OK &&
        result_quota.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        result_quota.consumed_result_bytes == result_bytes);

  static const char policy_digest_source[] =
      "const fn policy(): u64 { return u64.saturatingAdd(18446744073709551615_u64, 1_u64) }\n";
  static const char changed_policy_source[] =
      "const fn policy(): u64 { return u64.saturatingSubtract(18446744073709551615_u64, 1_u64) }\n";
  static const char changed_projection_source[] =
      "const fn policy(): Bool { return u64.overflowingAdd(18446744073709551615_u64, 1_u64).1 }\n";
  CHECK(fixture_lower(&first_fixture, policy_digest_source));
  CHECK(fixture_lower(&second_fixture, changed_policy_source));
  CHECK(memcmp(first_fixture.constir_functions[0].body_digest,
               second_fixture.constir_functions[0].body_digest, 32u) != 0 &&
        first_fixture.constir_result.written.receipt_bytes != 0u &&
        second_fixture.constir_result.written.receipt_bytes != 0u &&
        (first_fixture.constir_result.written.receipt_bytes !=
             second_fixture.constir_result.written.receipt_bytes ||
         memcmp(first_fixture.constir_receipt, second_fixture.constir_receipt,
                first_fixture.constir_result.written.receipt_bytes) != 0));
  CHECK(fixture_lower(&second_fixture, changed_projection_source));
  CHECK(memcmp(first_fixture.constir_functions[0].body_digest,
               second_fixture.constir_functions[0].body_digest, 32u) != 0);

  static const char *const rejected_sources[] = {
      "const fn bad(): u64 { return UInt.saturatingAdd(1_u64, 2_u64) }\n",
      "const fn bad(): u64 { return u64.saturatingAdd(left: 1_u64, 2_u64) }\n",
      "const fn bad(): u64 { return u64.saturatingAdd(1_u64) }\n",
      "const fn bad(): u64 { return u64.saturatingAdd(true, 2_u64) }\n",
      "const fn bad(): u64 { return u64.overflowingAdd(1_u64, 2_u64).2 }\n",
      "const fn bad(): u64 { return u64.overflowingNegate(1_u64, 2_u64).0 }\n",
      "const fn bad(): u64 { return u64.saturatingPower(1_u64, true) }\n",
  };
  for (size_t index = 0u;
       index < sizeof(rejected_sources) / sizeof(rejected_sources[0]);
       index += 1u) {
    CHECK(fixture_lower(&second_fixture, rejected_sources[index]));
    CHECK((second_fixture.frontend_result.status == W_SEED_FRONTEND_UNSUPPORTED ||
           second_fixture.frontend_result.status == W_SEED_FRONTEND_DIAGNOSTICS) &&
          second_fixture.constir_result.written.functions == 1u &&
          !second_fixture.constir_functions[0].lowerable &&
          second_fixture.constir_result.written.nodes == 0u &&
          second_fixture.constir_result.written.diagnostics == 1u &&
          second_fixture.constir_diagnostics[0].code ==
              W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0001 &&
          fixture_constir_valid(&second_fixture));
  }

  CHECK(fixture_parse(&second_fixture, policy_digest_source));
  const w_seed_constir_input capacity_input = {
      &second_fixture.frontend_input, &second_fixture.frontend_output,
      &second_fixture.frontend_result};
  (void)memset(second_fixture.constir_functions, 0xa5,
               sizeof(second_fixture.constir_functions));
  (void)memset(second_fixture.constir_nodes, 0xa5,
               sizeof(second_fixture.constir_nodes));
  (void)memset(second_fixture.constir_receipt, 0xa5,
               sizeof(second_fixture.constir_receipt));
#define CHECK_POLICY_SENTINELS()                                               \
  CHECK(first_byte_equals(second_fixture.constir_functions, 0xa5u) &&          \
        first_byte_equals(second_fixture.constir_nodes, 0xa5u) &&              \
        second_fixture.constir_receipt[0] == 0xa5u)
  w_seed_constir_result capacity_result;
  fixture_init_output(&second_fixture);
  second_fixture.constir_output.functions = NULL;
  second_fixture.constir_output.function_capacity = 0u;
  CHECK(w_seed_constir_run(&capacity_input, &second_fixture.constir_output,
                           &capacity_result) == W_SEED_CONSTIR_CAPACITY);
  CHECK_POLICY_SENTINELS();
  fixture_init_output(&second_fixture);
  second_fixture.constir_output.nodes = NULL;
  second_fixture.constir_output.node_capacity = 0u;
  CHECK(w_seed_constir_run(&capacity_input, &second_fixture.constir_output,
                           &capacity_result) == W_SEED_CONSTIR_CAPACITY);
  CHECK_POLICY_SENTINELS();
  fixture_init_output(&second_fixture);
  second_fixture.constir_output.receipt = NULL;
  second_fixture.constir_output.receipt_capacity = 0u;
  CHECK(w_seed_constir_run(&capacity_input, &second_fixture.constir_output,
                           &capacity_result) == W_SEED_CONSTIR_CAPACITY);
  CHECK_POLICY_SENTINELS();
#undef CHECK_POLICY_SENTINELS
  return true;
}

static bool test_u64_bool_tuple_product_boundary_constir(void) {
  static const char source[] =
      "export const product: (u64, Bool) = "
      "u64.overflowingAdd(18446744073709551615_u64, 1_u64)\n"
      "const fn direct(): (u64, Bool) { return "
      "u64.overflowingAdd(18446744073709551615_u64, 1_u64) }\n"
      "const fn spaced(): (u64 , Bool) { return "
      "u64.overflowingAdd(1_u64, 2_u64) }\n"
      "const fn forwarded(): (u64, Bool) { return direct() }\n"
      "const fn directValue(): u64 { return direct().0 }\n"
      "const fn directFlag(): Bool { return direct().1 }\n"
      "const fn moduleValue(): u64 { return product.0 }\n"
      "const fn moduleFlag(): Bool { return product.1 }\n"
      "const fn moduleRepeat(): Bool { return product.0 == product.0 }\n";
  fixture *value = &first_fixture;
  CHECK(fixture_lower(value, source));
  CHECK(value->frontend_result.status == W_SEED_FRONTEND_OK &&
        value->frontend_result.written.const_declarations == 1u &&
        value->frontend_result.written.functions == 8u);
  CHECK(value->const_declarations[0].exported &&
        value->const_declarations[0].has_explicit_type &&
        value->const_declarations[0].effective_type != W_SEED_FRONTEND_NONE &&
        value->types[value->const_declarations[0].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_TUPLE);
  CHECK(value->constir_result.written.functions == 9u &&
        fixture_constir_valid(value));

  const uint32_t const_ir = constir_function_for_const(value, 0u);
  const uint32_t direct_ir = constir_function_for_frontend(value, 0u);
  const uint32_t spaced_ir = constir_function_for_frontend(value, 1u);
  const uint32_t forwarded_ir = constir_function_for_frontend(value, 2u);
  const uint32_t direct_value_ir = constir_function_for_frontend(value, 3u);
  const uint32_t direct_flag_ir = constir_function_for_frontend(value, 4u);
  const uint32_t module_value_ir = constir_function_for_frontend(value, 5u);
  const uint32_t module_flag_ir = constir_function_for_frontend(value, 6u);
  const uint32_t module_repeat_ir = constir_function_for_frontend(value, 7u);
  CHECK(const_ir != W_SEED_CONSTIR_NONE && direct_ir != W_SEED_CONSTIR_NONE &&
        spaced_ir != W_SEED_CONSTIR_NONE && forwarded_ir != W_SEED_CONSTIR_NONE &&
        direct_value_ir != W_SEED_CONSTIR_NONE &&
        direct_flag_ir != W_SEED_CONSTIR_NONE &&
        module_value_ir != W_SEED_CONSTIR_NONE &&
        module_flag_ir != W_SEED_CONSTIR_NONE &&
        module_repeat_ir != W_SEED_CONSTIR_NONE);
  CHECK(constir_builtin_root_is(
            value, const_ir, W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_ADD) &&
        constir_builtin_root_is(
            value, direct_ir, W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_ADD) &&
        constir_builtin_root_is(
            value, spaced_ir, W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_ADD) &&
        value->constir_functions[forwarded_ir].lowerable &&
        value->constir_functions[forwarded_ir].root_node !=
            W_SEED_CONSTIR_NONE &&
        value->constir_nodes[value->constir_functions[forwarded_ir].root_node]
                .kind == W_SEED_CONSTIR_NODE_CALL &&
        value->constir_nodes[value->constir_functions[forwarded_ir].root_node]
                .call_target_function == 0u &&
        value->constir_nodes[value->constir_functions[forwarded_ir].root_node]
                .call_target_const_declaration == W_SEED_CONSTIR_NONE &&
        constir_projection_root_is(value, direct_value_ir, 0u, 0u,
                                   W_SEED_CONSTIR_NONE) &&
        constir_projection_root_is(value, direct_flag_ir, 1u, 0u,
                                   W_SEED_CONSTIR_NONE) &&
        constir_projection_root_is(value, module_value_ir, 0u,
                                   W_SEED_FRONTEND_NONE, 0u) &&
        constir_projection_root_is(value, module_flag_ir, 1u,
                                   W_SEED_FRONTEND_NONE, 0u) &&
        constir_const_repeat_is(value, module_repeat_ir, 0u));

  size_t builtin_count = 0u;
  size_t projection_count = 0u;
  for (size_t index = 0u; index < value->constir_result.written.nodes;
       index += 1u) {
    const w_seed_constir_node *node = &value->constir_nodes[index];
    if (node->kind == W_SEED_CONSTIR_NODE_BUILTIN_U64) {
      CHECK(node->builtin_operation ==
                W_SEED_FRONTEND_BUILTIN_U64_OVERFLOWING_ADD &&
            node->type_kind == W_SEED_FRONTEND_TYPE_TUPLE &&
            !node->type_is_signed && node->type_bit_width == 0u);
      builtin_count += 1u;
    } else if (node->kind == W_SEED_CONSTIR_NODE_TUPLE_ELEMENT) {
      CHECK(node->tuple_element_index <= 1u &&
            node->left != W_SEED_CONSTIR_NONE);
      projection_count += 1u;
    }
  }
  CHECK(builtin_count == 3u && projection_count == 6u);

  const w_seed_constir_program program = fixture_program(value);
  w_seed_constir_node saved_node =
      value->constir_nodes[value->constir_functions[direct_ir].root_node];
  const uint32_t direct_root = value->constir_functions[direct_ir].root_node;
  value->constir_nodes[direct_root].builtin_operation =
      W_SEED_FRONTEND_BUILTIN_NONE;
  CHECK(!fixture_constir_valid(value));
  static w_seed_constir_eval_frame invalid_frames[8];
  w_seed_constir_eval_workspace invalid_workspace = {invalid_frames, 8u};
  w_seed_constir_value invalid_output;
  w_seed_constir_eval_result invalid_result;
  CHECK(w_seed_constir_evaluate(
            &program, direct_ir, NULL, 0u,
            (w_seed_constir_quota){SIZE_MAX, 0u, 8u, SIZE_MAX},
            &invalid_workspace, &invalid_output, &invalid_result) ==
            W_SEED_CONSTIR_INVALID &&
        invalid_result.consumed_steps == 0u);
  value->constir_nodes[direct_root] = saved_node;
  CHECK(fixture_constir_valid(value));

  const uint32_t direct_projection_root =
      value->constir_functions[direct_value_ir].root_node;
  saved_node = value->constir_nodes[direct_projection_root];
  value->constir_nodes[direct_projection_root].tuple_element_index = 2u;
  CHECK(!fixture_constir_valid(value));
  value->constir_nodes[direct_projection_root] = saved_node;
  CHECK(fixture_constir_valid(value));
  saved_node = value->constir_nodes[direct_projection_root];
  value->constir_nodes[direct_projection_root].type_kind =
      W_SEED_FRONTEND_TYPE_BOOL;
  value->constir_nodes[direct_projection_root].type_bit_width = 0u;
  CHECK(!fixture_constir_valid(value));
  value->constir_nodes[direct_projection_root] = saved_node;
  CHECK(fixture_constir_valid(value));
  saved_node = value->constir_nodes[direct_root];
  value->constir_nodes[direct_root].type_kind = W_SEED_FRONTEND_TYPE_BOOL;
  value->constir_nodes[direct_root].type_bit_width = 0u;
  CHECK(!fixture_constir_valid(value));
  value->constir_nodes[direct_root] = saved_node;
  CHECK(fixture_constir_valid(value));

  static w_seed_constir_eval_frame frames[16];
  w_seed_constir_eval_workspace workspace = {frames, 16u};
  w_seed_constir_value output;
  w_seed_constir_eval_result result;
  CHECK(w_seed_constir_evaluate(
            &program, direct_ir, NULL, 0u,
            (w_seed_constir_quota){SIZE_MAX, 0u, 16u, SIZE_MAX}, &workspace,
            &output, &result) == W_SEED_CONSTIR_OK &&
        result.status == W_SEED_CONSTIR_OK &&
        constir_u64_bool_tuple_is(&output, 0u, true) &&
        result.consumed_steps > 0u && result.consumed_result_bytes > 0u);
  const size_t tuple_steps = result.consumed_steps;
  const size_t tuple_result_bytes = result.consumed_result_bytes;
  CHECK(w_seed_constir_evaluate(
            &program, direct_ir, NULL, 0u,
            (w_seed_constir_quota){tuple_steps - 1u, 0u, 16u, SIZE_MAX},
            &workspace, &output, &result) == W_SEED_CONSTIR_OK &&
        result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003 &&
        output.kind == W_SEED_CONSTIR_VALUE_INVALID);
  CHECK(w_seed_constir_evaluate(
            &program, direct_ir, NULL, 0u,
            (w_seed_constir_quota){SIZE_MAX, 0u, 16u,
                                   tuple_result_bytes - 1u},
            &workspace, &output, &result) == W_SEED_CONSTIR_OK &&
        result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003 &&
        result.consumed_result_bytes == tuple_result_bytes &&
        output.kind == W_SEED_CONSTIR_VALUE_INVALID);
  CHECK(w_seed_constir_evaluate(
            &program, direct_ir, NULL, 0u,
            (w_seed_constir_quota){SIZE_MAX, 0u, 16u, tuple_result_bytes},
            &workspace, &output, &result) == W_SEED_CONSTIR_OK &&
        result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        constir_u64_bool_tuple_is(&output, 0u, true));
  CHECK(w_seed_constir_evaluate(
            &program, spaced_ir, NULL, 0u,
            (w_seed_constir_quota){SIZE_MAX, 0u, 16u, SIZE_MAX}, &workspace,
            &output, &result) == W_SEED_CONSTIR_OK &&
        constir_u64_bool_tuple_is(&output, 3u, false));
  CHECK(w_seed_constir_evaluate(
            &program, forwarded_ir, NULL, 0u,
            (w_seed_constir_quota){SIZE_MAX, 0u, 16u, SIZE_MAX}, &workspace,
            &output, &result) == W_SEED_CONSTIR_OK &&
        constir_u64_bool_tuple_is(&output, 0u, true));
  CHECK(w_seed_constir_evaluate(
            &program, direct_value_ir, NULL, 0u,
            (w_seed_constir_quota){SIZE_MAX, 0u, 16u, SIZE_MAX}, &workspace,
            &output, &result) == W_SEED_CONSTIR_OK &&
        constir_u64_is(&output, 0u) && result.const_cache_hits == 0u &&
        result.const_cache_misses == 0u);
  CHECK(w_seed_constir_evaluate(
            &program, direct_flag_ir, NULL, 0u,
            (w_seed_constir_quota){SIZE_MAX, 0u, 16u, SIZE_MAX}, &workspace,
            &output, &result) == W_SEED_CONSTIR_OK &&
        output.kind == W_SEED_CONSTIR_VALUE_BOOL && output.bool_value);

  w_seed_constir_eval_result module_first;
  w_seed_constir_eval_result module_second;
  CHECK(w_seed_constir_evaluate(
            &program, module_value_ir, NULL, 0u,
            (w_seed_constir_quota){SIZE_MAX, 0u, 16u, SIZE_MAX}, &workspace,
            &output, &module_first) == W_SEED_CONSTIR_OK &&
        constir_u64_is(&output, 0u) && module_first.const_cache_hits == 0u &&
        module_first.const_cache_misses == 1u);
  CHECK(w_seed_constir_evaluate(
            &program, module_value_ir, NULL, 0u,
            (w_seed_constir_quota){SIZE_MAX, 0u, 16u, SIZE_MAX}, &workspace,
            &output, &module_second) == W_SEED_CONSTIR_OK &&
        constir_u64_is(&output, 0u) &&
        module_second.const_cache_hits == module_first.const_cache_hits &&
        module_second.const_cache_misses == module_first.const_cache_misses);
  CHECK(w_seed_constir_evaluate(
            &program, module_flag_ir, NULL, 0u,
            (w_seed_constir_quota){SIZE_MAX, 0u, 16u, SIZE_MAX}, &workspace,
            &output, &result) == W_SEED_CONSTIR_OK &&
        output.kind == W_SEED_CONSTIR_VALUE_BOOL && output.bool_value &&
        result.const_cache_hits == 0u && result.const_cache_misses == 1u);
  CHECK(w_seed_constir_evaluate(
            &program, module_repeat_ir, NULL, 0u,
            (w_seed_constir_quota){SIZE_MAX, 0u, 16u, SIZE_MAX}, &workspace,
            &output, &result) == W_SEED_CONSTIR_OK &&
        output.kind == W_SEED_CONSTIR_VALUE_BOOL && output.bool_value &&
        result.const_cache_hits == 1u && result.const_cache_misses == 1u);

  static const char tuple_cycle_source[] =
      "const left: (u64, Bool) = right\n"
      "const right: (u64, Bool) = left\n"
      "const fn read(): u64 { return left.0 }\n";
  CHECK(fixture_lower(&second_fixture, tuple_cycle_source));
  CHECK(second_fixture.frontend_result.status == W_SEED_FRONTEND_OK &&
        second_fixture.frontend_result.written.const_declarations == 2u &&
        second_fixture.constir_result.written.functions == 3u &&
        fixture_constir_valid(&second_fixture));
  const uint32_t cycle_read_ir =
      constir_function_for_frontend(&second_fixture, 0u);
  CHECK(cycle_read_ir != W_SEED_CONSTIR_NONE &&
        constir_projection_root_is(&second_fixture, cycle_read_ir, 0u,
                                   W_SEED_CONSTIR_NONE, 0u));
  const w_seed_constir_program cycle_program = fixture_program(&second_fixture);
  static w_seed_constir_eval_frame cycle_frames[8];
  w_seed_constir_eval_workspace cycle_workspace = {cycle_frames, 8u};
  w_seed_constir_value cycle_output;
  w_seed_constir_eval_result cycle_result;
  CHECK(w_seed_constir_evaluate(
            &cycle_program, cycle_read_ir, NULL, 0u,
            (w_seed_constir_quota){SIZE_MAX, 0u, 8u, SIZE_MAX},
            &cycle_workspace, &cycle_output, &cycle_result) ==
            W_SEED_CONSTIR_OK &&
        cycle_result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0002 &&
        cycle_output.kind == W_SEED_CONSTIR_VALUE_INVALID &&
        cycle_result.const_cache_hits == 0u &&
        cycle_result.const_cache_misses == 2u);

  static const char *const rejected_sources[] = {
      "const fn reversed(): (Bool, u64) { return "
      "u64.overflowingAdd(1_u64, 2_u64) }\n",
      "const fn widened(): (u64, u64) { return "
      "u64.overflowingAdd(1_u64, 2_u64) }\n",
      "const fn malformed(): (u64, Bool, Bool) { return "
      "u64.overflowingAdd(1_u64, 2_u64) }\n",
      "export const reversedProduct: (Bool, u64) = "
      "u64.overflowingAdd(1_u64, 2_u64)\n",
      "const fn tupleParameter(pair: (u64, Bool)): u64 { return pair.0 }\n",
      "const inferredProduct = u64.overflowingAdd(1_u64, 2_u64)\n",
      "const fn invalidProjection(): u64 { return "
      "u64.overflowingAdd(1_u64, 2_u64).2 }\n",
      "const missingOperand: (u64, Bool) = "
      "u64.overflowingAdd(1_u64)\n",
      "const wrongOperand: (u64, Bool) = "
      "u64.overflowingAdd(1_u64, true)\n",
  };
  for (size_t index = 0u;
       index < sizeof(rejected_sources) / sizeof(rejected_sources[0]);
       index += 1u) {
    CHECK(fixture_lower(&second_fixture, rejected_sources[index]));
    for (size_t function_index = 0u;
         function_index < second_fixture.constir_result.written.functions;
         function_index += 1u) {
      const w_seed_constir_function *function =
          &second_fixture.constir_functions[function_index];
      CHECK(!function->lowerable && function->root_node == W_SEED_CONSTIR_NONE &&
            function->node_count == 0u);
    }
    CHECK(second_fixture.constir_result.written.nodes == 0u &&
          second_fixture.constir_result.written.diagnostics == 1u &&
          second_fixture.constir_diagnostics[0].code ==
              W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0001 &&
          fixture_constir_valid(&second_fixture));
    if (index != 4u)
      CHECK(second_fixture.frontend_result.status != W_SEED_FRONTEND_OK);
  }
  return true;
}

int main(void) {
  if (!test_can_move_and_digest()) return 1;
  if (!test_static_list_stage_path()) return 1;
  if (!test_diagnostics_and_quotas()) return 1;
  if (!test_integer_bitwise_complement()) return 1;
  if (!test_labels_relations_and_parentheses()) return 1;
  if (!test_typed_literal_projection()) return 1;
  if (!test_string_literals_and_comparisons()) return 1;
  if (!test_recursion_and_invalid_inputs()) return 1;
  if (!test_empty_program_validation()) return 1;
  if (!test_depth_and_caller_owned_validation()) return 1;
  if (!test_direct_call_and_external_barrier()) return 1;
  if (!test_capacity_and_barrier()) return 1;
  if (!test_typed_const_expression_synthetic()) return 1;
  if (!test_module_const_synthetic_d4()) return 1;
  if (!test_module_const_active_cycle_defense()) return 1;
  if (!test_u64_policy_constir()) return 1;
  if (!test_u64_bool_tuple_product_boundary_constir()) return 1;
  (void)puts("constir tests passed");
  return 0;
}
