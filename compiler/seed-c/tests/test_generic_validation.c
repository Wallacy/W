#include "w_seed_generic_validation.h"
#include "w_seed_sha256.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      (void)fprintf(stderr, "generic validation check failed: %s:%d: %s\n", \
                    __FILE__, __LINE__, #condition);                           \
      return false;                                                             \
    }                                                                           \
  } while (0)

enum {
  SOURCE_BYTES = 65536,
  LEXER_FRAMES = 2048,
  TOKENS = 8192,
  CST_NODES = 32768,
  PARSE_FRAMES = 4096,
  ISSUES = 1024,
  MODULES = 8,
  ARRAY = 512,
  TYPES = 4096,
  FUNCTIONS = 128,
  PARAMETERS = 512,
  GENERIC_PARAMETERS = 128,
  GENERIC_APPLICATIONS = 64,
  GENERIC_ARGUMENTS = 512,
  TYPED_CONST_EXPRESSIONS = 512,
  CONST_VALUES = 8192,
  CONST_ELEMENTS = 16384,
  CONST_BYTES = 65536,
  ENUMS = 32,
  ENUM_CASES = 256,
  ENUM_CASE_PARAMETERS = 256,
  ENUM_SUBSET_MEMBERS = 512,
  FIELDS = 256,
  DECLARATIONS = 512,
  STATEMENTS = 8192,
  EXPRESSIONS = 32768,
  ARGUMENTS = 8192,
  SWITCH_ARMS = 8192,
  MEMBERSHIP_CASES = 32768,
  SYMBOLS = 4096,
  FACTS = 4096,
  DIAGNOSTICS = 2048,
  DIAGNOSTIC_FACTS = DIAGNOSTICS * 5,
  DIAGNOSTIC_ITEMS = DIAGNOSTICS * 4,
  DIAGNOSTIC_LABELS = DIAGNOSTICS * 2,
  FRONTEND_RECEIPT = 4 * 1024 * 1024,
  CONSTIR_FUNCTIONS = 512,
  CONSTIR_PARAMETERS = 512,
  CONSTIR_NODES = 65536,
  CONSTIR_ARGUMENTS = 16384,
  CONSTIR_SWITCH_ARMS = 16384,
  CONSTIR_MEMBERSHIP = 65536,
  CONSTIR_STATEMENTS = 16384,
  CONSTIR_LOCALS = 2048,
  CONSTIR_DIAGNOSTICS = 512,
  CONSTIR_RECEIPT = 8 * 1024 * 1024,
  CONVERSION_VALUES = 8192,
  VALIDATION_RECEIPTS = 64,
  EVAL_FRAMES = 64,
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
  w_seed_frontend_module modules[MODULES];
  w_seed_frontend_import imports[ARRAY];
  w_seed_frontend_import_item import_items[ARRAY];
  w_seed_frontend_struct structs[ARRAY];
  w_seed_frontend_field fields[FIELDS];
  w_seed_frontend_enum enums[ENUMS];
  w_seed_frontend_enum_case enum_cases[ENUM_CASES];
  w_seed_frontend_enum_case_parameter enum_case_parameters[ENUM_CASE_PARAMETERS];
  w_seed_frontend_enum_subset_member enum_subset_members[ENUM_SUBSET_MEMBERS];
  w_seed_frontend_type_declaration type_declarations[DECLARATIONS];
  w_seed_frontend_alias aliases[DECLARATIONS];
  w_seed_frontend_const_declaration const_declarations[DECLARATIONS];
  w_seed_frontend_type types[TYPES];
  w_seed_frontend_function functions[FUNCTIONS];
  w_seed_frontend_parameter parameters[PARAMETERS];
  w_seed_frontend_entry entries[ARRAY];
  w_seed_frontend_statement statements[STATEMENTS];
  w_seed_frontend_expression expressions[EXPRESSIONS];
  w_seed_frontend_argument arguments[ARGUMENTS];
  w_seed_frontend_switch_arm switch_arms[SWITCH_ARMS];
  w_seed_frontend_enum_membership_case membership_cases[MEMBERSHIP_CASES];
  w_seed_frontend_symbol symbols[SYMBOLS];
  w_seed_frontend_fact facts[FACTS];
  w_seed_frontend_diagnostic diagnostics[DIAGNOSTICS];
  w_seed_frontend_diagnostic_fact diagnostic_facts[DIAGNOSTIC_FACTS];
  w_seed_frontend_diagnostic_item diagnostic_items[DIAGNOSTIC_ITEMS];
  w_seed_frontend_diagnostic_label diagnostic_labels[DIAGNOSTIC_LABELS];
  w_seed_frontend_generic_parameter generic_parameters[GENERIC_PARAMETERS];
  w_seed_frontend_generic_application generic_applications[GENERIC_APPLICATIONS];
  w_seed_frontend_generic_argument generic_arguments[GENERIC_ARGUMENTS];
  w_seed_frontend_typed_const_expression
      typed_const_expressions[TYPED_CONST_EXPRESSIONS];
  w_seed_frontend_const_value const_values[CONST_VALUES];
  w_seed_frontend_const_element const_elements[CONST_ELEMENTS];
  uint8_t const_bytes[CONST_BYTES];
  uint8_t frontend_receipt[FRONTEND_RECEIPT];
  w_seed_constir_output constir_output;
  w_seed_constir_result constir_result;
  w_seed_constir_function constir_functions[CONSTIR_FUNCTIONS];
  w_seed_constir_parameter constir_parameters[CONSTIR_PARAMETERS];
  w_seed_constir_node constir_nodes[CONSTIR_NODES];
  w_seed_constir_call_argument constir_arguments[CONSTIR_ARGUMENTS];
  w_seed_constir_switch_arm constir_switch_arms[CONSTIR_SWITCH_ARMS];
  w_seed_constir_membership_case constir_membership[CONSTIR_MEMBERSHIP];
  w_seed_constir_statement constir_statements[CONSTIR_STATEMENTS];
  w_seed_constir_local constir_locals[CONSTIR_LOCALS];
  w_seed_constir_diagnostic constir_diagnostics[CONSTIR_DIAGNOSTICS];
  uint8_t constir_receipt[CONSTIR_RECEIPT];
  w_seed_constir_value conversion_values[CONVERSION_VALUES];
  uint8_t evidence_bytes[W_SEED_GENERIC_VALIDATION_MAX_EVIDENCE_BYTES];
  uint8_t nominal_origin_preimage[
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PREIMAGE_BYTES];
  uint8_t nominal_origin_digest[
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_DIGEST_BYTES];
  w_seed_generic_nominal_origin_view nominal_origin_view;
  w_seed_generic_validation_receipt receipts[VALIDATION_RECEIPTS];
  w_seed_constir_eval_frame eval_frames[EVAL_FRAMES];
  uint8_t specialization_preimage[65536];
} fixture;

static fixture value;
static const uint8_t *nominal_test_source_authority = NULL;
static size_t nominal_test_source_authority_length = 0u;

static void fixture_init_outputs(fixture *fixture_value) {
  fixture_value->frontend_input = (w_seed_frontend_input){
      .documents = &fixture_value->document,
      .document_count = 1u,
      .external_modules = NULL,
      .external_module_count = 0u,
  };
  fixture_value->frontend_output = (w_seed_frontend_output){
      .modules = fixture_value->modules,
      .module_capacity = MODULES,
      .imports = fixture_value->imports,
      .import_capacity = ARRAY,
      .import_items = fixture_value->import_items,
      .import_item_capacity = ARRAY,
      .structs = fixture_value->structs,
      .struct_capacity = ARRAY,
      .fields = fixture_value->fields,
      .field_capacity = FIELDS,
      .type_declarations = fixture_value->type_declarations,
      .type_declaration_capacity = DECLARATIONS,
      .aliases = fixture_value->aliases,
      .alias_capacity = DECLARATIONS,
      .const_declarations = fixture_value->const_declarations,
      .const_declaration_capacity = DECLARATIONS,
      .types = fixture_value->types,
      .type_capacity = TYPES,
      .functions = fixture_value->functions,
      .function_capacity = FUNCTIONS,
      .parameters = fixture_value->parameters,
      .parameter_capacity = PARAMETERS,
      .arguments = fixture_value->arguments,
      .argument_capacity = ARGUMENTS,
      .entries = fixture_value->entries,
      .entry_capacity = ARRAY,
      .statements = fixture_value->statements,
      .statement_capacity = STATEMENTS,
      .expressions = fixture_value->expressions,
      .expression_capacity = EXPRESSIONS,
      .switch_arms = fixture_value->switch_arms,
      .switch_arm_capacity = SWITCH_ARMS,
      .enum_membership_cases = fixture_value->membership_cases,
      .enum_membership_case_capacity = MEMBERSHIP_CASES,
      .symbols = fixture_value->symbols,
      .symbol_capacity = SYMBOLS,
      .facts = fixture_value->facts,
      .fact_capacity = FACTS,
      .diagnostics = fixture_value->diagnostics,
      .diagnostic_capacity = DIAGNOSTICS,
      .diagnostic_facts = fixture_value->diagnostic_facts,
      .diagnostic_fact_capacity = DIAGNOSTIC_FACTS,
      .diagnostic_items = fixture_value->diagnostic_items,
      .diagnostic_item_capacity = DIAGNOSTIC_ITEMS,
      .diagnostic_labels = fixture_value->diagnostic_labels,
      .diagnostic_label_capacity = DIAGNOSTIC_LABELS,
      .enums = fixture_value->enums,
      .enum_capacity = ENUMS,
      .enum_cases = fixture_value->enum_cases,
      .enum_case_capacity = ENUM_CASES,
      .enum_case_parameters = fixture_value->enum_case_parameters,
      .enum_case_parameter_capacity = ENUM_CASE_PARAMETERS,
      .enum_subset_members = fixture_value->enum_subset_members,
      .enum_subset_member_capacity = ENUM_SUBSET_MEMBERS,
      .generic_parameters = fixture_value->generic_parameters,
      .generic_parameter_capacity = GENERIC_PARAMETERS,
      .generic_applications = fixture_value->generic_applications,
      .generic_application_capacity = GENERIC_APPLICATIONS,
      .generic_arguments = fixture_value->generic_arguments,
      .generic_argument_capacity = GENERIC_ARGUMENTS,
      .typed_const_expressions = fixture_value->typed_const_expressions,
      .typed_const_expression_capacity = TYPED_CONST_EXPRESSIONS,
      .const_values = fixture_value->const_values,
      .const_value_capacity = CONST_VALUES,
      .const_elements = fixture_value->const_elements,
      .const_element_capacity = CONST_ELEMENTS,
      .const_bytes = fixture_value->const_bytes,
      .const_bytes_capacity = CONST_BYTES,
      .receipt = fixture_value->frontend_receipt,
      .receipt_capacity = FRONTEND_RECEIPT};
  fixture_value->constir_output = (w_seed_constir_output){
      .functions = fixture_value->constir_functions,
      .function_capacity = CONSTIR_FUNCTIONS,
      .parameters = fixture_value->constir_parameters,
      .parameter_capacity = CONSTIR_PARAMETERS,
      .nodes = fixture_value->constir_nodes,
      .node_capacity = CONSTIR_NODES,
      .call_arguments = fixture_value->constir_arguments,
      .call_argument_capacity = CONSTIR_ARGUMENTS,
      .switch_arms = fixture_value->constir_switch_arms,
      .switch_arm_capacity = CONSTIR_SWITCH_ARMS,
      .membership_cases = fixture_value->constir_membership,
      .membership_case_capacity = CONSTIR_MEMBERSHIP,
      .statements = fixture_value->constir_statements,
      .statement_capacity = CONSTIR_STATEMENTS,
      .locals = fixture_value->constir_locals,
      .local_capacity = CONSTIR_LOCALS,
      .diagnostics = fixture_value->constir_diagnostics,
      .diagnostic_capacity = CONSTIR_DIAGNOSTICS,
      .receipt = fixture_value->constir_receipt,
      .receipt_capacity = CONSTIR_RECEIPT};
}

static bool fixture_lower_base(fixture *fixture_value, const char *source_text,
                               const char *module_id,
                               w_seed_constir_status *constir_status_out) {
  const size_t length = strlen(source_text);
  CHECK(length < sizeof(fixture_value->source_bytes));
  (void)memset(fixture_value, 0, sizeof(*fixture_value));
  (void)memcpy(fixture_value->source_bytes, source_text, length);
  const w_seed_byte_view bytes = {
      (const uint8_t *)fixture_value->source_bytes, length};
  w_seed_source_error source_error;
  CHECK(w_seed_source_init(bytes, &fixture_value->source, &source_error));
  w_seed_lex_error lex_error;
  CHECK(w_seed_parser_init(
      &fixture_value->source, (w_seed_span){0u, length},
      (w_seed_foreign_limits){65536u, 256u}, fixture_value->lexer_frames,
      LEXER_FRAMES, fixture_value->tokens, TOKENS, fixture_value->cst_nodes,
      CST_NODES, fixture_value->parse_frames, PARSE_FRAMES,
      fixture_value->issues, ISSUES, &fixture_value->parser, &lex_error));
  CHECK(w_seed_parser_parse(&fixture_value->parser, &fixture_value->parse));
  fixture_value->document = (w_seed_frontend_document){
      .logical_source_id = {module_id, strlen(module_id)},
      .module_id = {module_id, strlen(module_id)},
      .local_module_name = {module_id, strlen(module_id)},
      .source = &fixture_value->source,
      .nodes = fixture_value->cst_nodes,
      .node_count = fixture_value->parse.node_count,
      .parse = fixture_value->parse,
  };
  fixture_init_outputs(fixture_value);
  const w_seed_frontend_status frontend_status = w_seed_frontend_run(
      &fixture_value->frontend_input, &fixture_value->frontend_output,
      &fixture_value->frontend_result);
  CHECK(frontend_status == W_SEED_FRONTEND_OK ||
        frontend_status == W_SEED_FRONTEND_UNSUPPORTED ||
        frontend_status == W_SEED_FRONTEND_DIAGNOSTICS);
  const w_seed_constir_input constir_input = {
      &fixture_value->frontend_input, &fixture_value->frontend_output,
      &fixture_value->frontend_result};
  const w_seed_constir_status constir_status = w_seed_constir_run(
      &constir_input, &fixture_value->constir_output,
      &fixture_value->constir_result);
  if (constir_status_out != NULL) *constir_status_out = constir_status;
  return true;
}

static bool fixture_lower_with_module(fixture *fixture_value,
                                      const char *source_text,
                                      const char *module_id) {
  w_seed_constir_status constir_status = W_SEED_CONSTIR_INVALID;
  CHECK(fixture_lower_base(fixture_value, source_text, module_id,
                           &constir_status));
  CHECK(constir_status == W_SEED_CONSTIR_OK);
  return true;
}

static bool fixture_lower(fixture *fixture_value, const char *source_text) {
  return fixture_lower_with_module(fixture_value, source_text,
                                   "generic_test");
}

static w_seed_constir_program fixture_program(const fixture *fixture_value) {
  return (w_seed_constir_program){
      .functions = fixture_value->constir_functions,
      .function_count = fixture_value->constir_result.written.functions,
      .parameters = fixture_value->constir_parameters,
      .parameter_count = fixture_value->constir_result.written.parameters,
      .nodes = fixture_value->constir_nodes,
      .node_count = fixture_value->constir_result.written.nodes,
      .call_arguments = fixture_value->constir_arguments,
      .call_argument_count = fixture_value->constir_result.written.call_arguments,
      .switch_arms = fixture_value->constir_switch_arms,
      .switch_arm_count = fixture_value->constir_result.written.switch_arms,
      .membership_cases = fixture_value->constir_membership,
      .membership_case_count = fixture_value->constir_result.written.membership_cases,
      .frontend_output = &fixture_value->frontend_output,
      .frontend_result = &fixture_value->frontend_result,
      .statements = fixture_value->constir_statements,
      .statement_count = fixture_value->constir_result.written.statements,
      .locals = fixture_value->constir_locals,
      .local_count = fixture_value->constir_result.written.locals};
}

static bool fixture_prepare_nominal_origin_with_authority(
    fixture *fixture_value, uint32_t application_index,
    const uint8_t *authority, size_t authority_length) {
  CHECK(fixture_value != NULL);
  CHECK(authority != NULL && authority_length != 0u);
  CHECK(application_index <
        fixture_value->frontend_result.written.generic_applications);
  const w_seed_frontend_generic_application *application =
      &fixture_value->generic_applications[application_index];
  if (application->module_index >= fixture_value->frontend_result.written.modules ||
      application->head_struct >= fixture_value->frontend_result.written.structs) {
    fixture_value->nominal_origin_view = (w_seed_generic_nominal_origin_view){0};
    return true;
  }
  const w_seed_frontend_module *module =
      &fixture_value->modules[application->module_index];
  const w_seed_frontend_struct *head =
      &fixture_value->structs[application->head_struct];
  static const char package_name[] = "last-light/restaurant";
  w_seed_frontend_text module_segments[
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_MODULE_SEGMENTS];
  size_t module_segment_count = 0u;
  size_t segment_start = 0u;
  for (size_t index = 0u; index <= module->module_id.length; index += 1u) {
    const bool at_end = index == module->module_id.length;
    const bool at_separator =
        !at_end && ((const uint8_t *)module->module_id.data)[index] ==
                       (uint8_t)'.';
    if (!at_end && !at_separator) continue;
    CHECK(module_segment_count <
          W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_MODULE_SEGMENTS);
    CHECK(index > segment_start);
    module_segments[module_segment_count++] = (w_seed_frontend_text){
        module->module_id.data + segment_start,
        index - segment_start};
    segment_start = index + 1u;
  }
  const w_seed_generic_nominal_origin origin = {
      authority,
      authority_length,
      {package_name, sizeof(package_name) - 1u},
      module_segments,
      module_segment_count,
      W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
      NULL,
      0u,
      head->name};
  w_seed_generic_nominal_origin_result origin_result;
  const w_seed_generic_nominal_origin_state origin_state =
      w_seed_generic_nominal_origin_write(
          &origin, fixture_value->nominal_origin_preimage,
          sizeof(fixture_value->nominal_origin_preimage), &origin_result);
  CHECK(origin_state == W_SEED_GENERIC_NOMINAL_ORIGIN_AVAILABLE);
  CHECK(origin_result.bytes_written == origin_result.bytes_required);
  (void)memcpy(fixture_value->nominal_origin_digest, origin_result.digest,
               sizeof(fixture_value->nominal_origin_digest));
  fixture_value->nominal_origin_view = (w_seed_generic_nominal_origin_view){
      fixture_value->nominal_origin_preimage,
      origin_result.bytes_written,
      fixture_value->nominal_origin_digest,
      application->module_index,
      application->head_struct};
  CHECK(w_seed_generic_nominal_origin_view_valid(
      &fixture_value->nominal_origin_view));
  return true;
}

static bool fixture_prepare_nominal_origin(fixture *fixture_value,
                                           uint32_t application_index) {
  static const uint8_t authority[] =
      "w-authority-fixture-1|registry=w";
  if (nominal_test_source_authority != NULL &&
      nominal_test_source_authority_length != 0u)
    return fixture_prepare_nominal_origin_with_authority(
        fixture_value, application_index, nominal_test_source_authority,
        nominal_test_source_authority_length);
  return fixture_prepare_nominal_origin_with_authority(
      fixture_value, application_index, authority, sizeof(authority) - 1u);
}

static w_seed_generic_validation_state validate_application_at_with_evidence(
    fixture *fixture_value, uint32_t application_index,
    size_t conversion_capacity, uint8_t *evidence_bytes,
    size_t evidence_byte_capacity, w_seed_constir_quota quota,
    w_seed_generic_validation_result *result) {
  CHECK(fixture_prepare_nominal_origin(fixture_value, application_index));
  w_seed_constir_eval_workspace workspace = {
      fixture_value->eval_frames, EVAL_FRAMES};
  const w_seed_constir_program program = fixture_program(fixture_value);
  const w_seed_generic_validation_input input = {
      .frontend_output = &fixture_value->frontend_output,
      .frontend_result = &fixture_value->frontend_result,
      .constir_program = &program,
      .application_index = application_index,
      .quota = quota,
      .eval_workspace = &workspace,
      .conversion_values = fixture_value->conversion_values,
      .conversion_value_capacity = conversion_capacity,
      .evidence_bytes = evidence_bytes,
      .evidence_byte_capacity = evidence_byte_capacity,
      .receipts = fixture_value->receipts,
      .receipt_capacity = VALIDATION_RECEIPTS,
      .nominal_origin = &fixture_value->nominal_origin_view,
      .specialization_preimage = fixture_value->specialization_preimage,
      .specialization_preimage_capacity =
          sizeof(fixture_value->specialization_preimage)};
  return w_seed_generic_validation_run(&input, result);
}

static w_seed_generic_validation_state validate_application_at_with_capacities(
    fixture *fixture_value, uint32_t application_index,
    size_t conversion_capacity, uint8_t *evidence_bytes,
    size_t evidence_byte_capacity, size_t receipt_capacity,
    w_seed_constir_quota quota, w_seed_generic_validation_result *result) {
  CHECK(fixture_prepare_nominal_origin(fixture_value, application_index));
  w_seed_constir_eval_workspace workspace = {
      fixture_value->eval_frames, EVAL_FRAMES};
  const w_seed_constir_program program = fixture_program(fixture_value);
  const w_seed_generic_validation_input input = {
      .frontend_output = &fixture_value->frontend_output,
      .frontend_result = &fixture_value->frontend_result,
      .constir_program = &program,
      .application_index = application_index,
      .quota = quota,
      .eval_workspace = &workspace,
      .conversion_values = fixture_value->conversion_values,
      .conversion_value_capacity = conversion_capacity,
      .evidence_bytes = evidence_bytes,
      .evidence_byte_capacity = evidence_byte_capacity,
      .receipts = fixture_value->receipts,
      .receipt_capacity = receipt_capacity,
      .nominal_origin = &fixture_value->nominal_origin_view,
      .specialization_preimage = fixture_value->specialization_preimage,
      .specialization_preimage_capacity =
          sizeof(fixture_value->specialization_preimage)};
  return w_seed_generic_validation_run(&input, result);
}

static w_seed_generic_validation_state validate_application_at(
    fixture *fixture_value, uint32_t application_index,
    size_t conversion_capacity, w_seed_constir_quota quota,
    w_seed_generic_validation_result *result) {
  return validate_application_at_with_evidence(
      fixture_value, application_index, conversion_capacity,
      fixture_value->evidence_bytes,
      W_SEED_GENERIC_VALIDATION_MAX_EVIDENCE_BYTES, quota, result);
}

static w_seed_generic_validation_state validate_application(
    fixture *fixture_value, size_t conversion_capacity,
    w_seed_constir_quota quota, w_seed_generic_validation_result *result) {
  return validate_application_at(fixture_value, 0u, conversion_capacity, quota,
                                 result);
}

static w_seed_generic_validation_state
validate_application_at_with_specialization_capacity(
    fixture *fixture_value, uint32_t application_index, uint8_t *preimage,
    size_t preimage_capacity, w_seed_generic_validation_result *result) {
  CHECK(fixture_prepare_nominal_origin(fixture_value, application_index));
  w_seed_constir_eval_workspace workspace = {
      fixture_value->eval_frames, EVAL_FRAMES};
  const w_seed_constir_program program = fixture_program(fixture_value);
  const w_seed_generic_validation_input input = {
      .frontend_output = &fixture_value->frontend_output,
      .frontend_result = &fixture_value->frontend_result,
      .constir_program = &program,
      .application_index = application_index,
      .quota = {100000u, 0u, 64u, SIZE_MAX},
      .eval_workspace = &workspace,
      .conversion_values = fixture_value->conversion_values,
      .conversion_value_capacity = CONVERSION_VALUES,
      .evidence_bytes = fixture_value->evidence_bytes,
      .evidence_byte_capacity = W_SEED_GENERIC_VALIDATION_MAX_EVIDENCE_BYTES,
      .receipts = fixture_value->receipts,
      .receipt_capacity = VALIDATION_RECEIPTS,
      .nominal_origin = &fixture_value->nominal_origin_view,
      .specialization_preimage = preimage,
      .specialization_preimage_capacity = preimage_capacity};
  return w_seed_generic_validation_run(&input, result);
}

static w_seed_generic_validation_state validate_application_at_with_origin_view(
    fixture *fixture_value, uint32_t application_index,
    const w_seed_generic_nominal_origin_view *origin_view, uint8_t *preimage,
    size_t preimage_capacity, w_seed_generic_validation_result *result) {
  w_seed_constir_eval_workspace workspace = {
      fixture_value->eval_frames, EVAL_FRAMES};
  const w_seed_constir_program program = fixture_program(fixture_value);
  const w_seed_generic_validation_input input = {
      .frontend_output = &fixture_value->frontend_output,
      .frontend_result = &fixture_value->frontend_result,
      .constir_program = &program,
      .application_index = application_index,
      .quota = {100000u, 0u, 64u, SIZE_MAX},
      .eval_workspace = &workspace,
      .conversion_values = fixture_value->conversion_values,
      .conversion_value_capacity = CONVERSION_VALUES,
      .evidence_bytes = fixture_value->evidence_bytes,
      .evidence_byte_capacity = W_SEED_GENERIC_VALIDATION_MAX_EVIDENCE_BYTES,
      .receipts = fixture_value->receipts,
      .receipt_capacity = VALIDATION_RECEIPTS,
      .nominal_origin = origin_view,
      .specialization_preimage = preimage,
      .specialization_preimage_capacity = preimage_capacity};
  return w_seed_generic_validation_run(&input, result);
}

static bool fingerprint_not_available(const w_seed_generic_validation_result *result) {
  return result != NULL &&
         result->fingerprint_state ==
             W_SEED_GENERIC_VALIDATION_FINGERPRINT_NOT_AVAILABLE &&
         memcmp(result->fingerprint_digest,
                (uint8_t[W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES]){0},
                W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES) == 0 &&
         result->specialization_state ==
             W_SEED_GENERIC_VALIDATION_SPECIALIZATION_NOT_AVAILABLE &&
         result->specialization_bytes_written == 0u &&
         result->specialization_bytes_required == 0u &&
         memcmp(result->specialization_digest,
                (uint8_t[W_SEED_GENERIC_VALIDATION_SPECIALIZATION_DIGEST_BYTES]){0},
                W_SEED_GENERIC_VALIDATION_SPECIALIZATION_DIGEST_BYTES) == 0;
}

static bool fingerprint_unsupported(const w_seed_generic_validation_result *result) {
  return result != NULL &&
         result->fingerprint_state ==
             W_SEED_GENERIC_VALIDATION_FINGERPRINT_UNSUPPORTED &&
         memcmp(result->fingerprint_digest,
                (uint8_t[W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES]){0},
                W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES) == 0;
}

static const char *const STAGE_PREFIX =
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
    "struct StagePath<_ stages: StaticList<ServiceStage>"
    "<(isValidStagePath(.member))>> { orderId: u64 }\n";

static bool fixture_lower_stage(fixture *fixture_value, const char *suffix) {
  const size_t prefix_length = strlen(STAGE_PREFIX);
  const size_t suffix_length = strlen(suffix);
  CHECK(prefix_length + suffix_length + 1u <=
        sizeof(fixture_value->source_bytes));
  char source[SOURCE_BYTES];
  (void)memcpy(source, STAGE_PREFIX, prefix_length);
  (void)memcpy(source + prefix_length, suffix, suffix_length);
  source[prefix_length + suffix_length] = '\0';
  return fixture_lower(fixture_value, source);
}

static void print_digest(const uint8_t digest[32]) {
  for (size_t index = 0u; index < 32u; index += 1u)
    (void)printf("%02x", (unsigned int)digest[index]);
}

static void print_bytes_hex(const uint8_t *bytes, size_t length) {
  if (bytes == NULL) return;
  for (size_t index = 0u; index < length; index += 1u)
    (void)printf("%02x", (unsigned int)bytes[index]);
}

static void print_specialization_projection(
    const fixture *fixture_value,
    const w_seed_generic_validation_result *result) {
  if (fixture_value == NULL || result == NULL) return;
  (void)printf(" specialization_state=%s specialization_written=%llu "
               "specialization_required=%llu specialization_digest=",
               w_seed_generic_validation_specialization_state_name(
                   result->specialization_state),
               (unsigned long long)result->specialization_bytes_written,
               (unsigned long long)result->specialization_bytes_required);
  if (result->specialization_state ==
      W_SEED_GENERIC_VALIDATION_SPECIALIZATION_AVAILABLE) {
    print_digest(result->specialization_digest);
    (void)printf(" specialization_preimage=");
    print_bytes_hex(fixture_value->specialization_preimage,
                    result->specialization_bytes_written);
  } else {
    for (size_t byte = 0u; byte < 32u; byte += 1u) (void)printf("00");
  }
}

static void print_receipt_projection(const fixture *fixture_value,
                                     const w_seed_generic_validation_result *result) {
  if (fixture_value == NULL || result == NULL) return;
  (void)printf(" receipt_kinds=");
  for (size_t index = 0u; index < result->receipts_written; index += 1u)
    (void)putchar(fixture_value->receipts[index].kind ==
                          W_SEED_GENERIC_VALIDATION_RECEIPT_CONST_ARGUMENT
                      ? 'C'
                      : 'P');
  (void)printf(" receipt_steps=");
  for (size_t index = 0u; index < result->receipts_written; index += 1u) {
    if (index != 0u) (void)putchar(',');
    (void)printf("%llu", (unsigned long long)
                            fixture_value->receipts[index]
                                .evaluation.consumed_steps);
  }
  (void)printf(" receipt_args=");
  for (size_t index = 0u; index < result->receipts_written; index += 1u) {
    if (index != 0u) (void)putchar(',');
    (void)printf("%u", fixture_value->receipts[index].generic_argument_index);
  }
  (void)printf(" receipt_typed=");
  for (size_t index = 0u; index < result->receipts_written; index += 1u) {
    if (index != 0u) (void)putchar(',');
    (void)printf("%u", fixture_value->receipts[index]
                              .typed_const_expression_index);
  }
  (void)printf(" receipt_values=");
  for (size_t index = 0u; index < result->receipts_written; index += 1u) {
    if (index != 0u) (void)putchar(',');
    const w_seed_constir_value *eval_value =
        &fixture_value->receipts[index].eval_value;
    if (eval_value->kind == W_SEED_CONSTIR_VALUE_INTEGER)
      (void)printf("i%u", eval_value->integer_value[0]);
    else if (eval_value->kind == W_SEED_CONSTIR_VALUE_BOOL)
      (void)printf("b%d", eval_value->bool_value ? 1 : 0);
    else
      (void)putchar('x');
  }
  (void)printf(" receipt_cache_hits=");
  for (size_t index = 0u; index < result->receipts_written; index += 1u) {
    if (index != 0u) (void)putchar(',');
    (void)printf("%llu", (unsigned long long)fixture_value->receipts[index]
                                      .evaluation.const_cache_hits);
  }
  (void)printf(" receipt_cache_misses=");
  for (size_t index = 0u; index < result->receipts_written; index += 1u) {
    if (index != 0u) (void)putchar(',');
    (void)printf("%llu", (unsigned long long)fixture_value->receipts[index]
                                      .evaluation.const_cache_misses);
  }
}

static bool predicate_body_digest_for_application(
    const fixture *fixture_value,
    const w_seed_frontend_generic_application *application,
    uint8_t digest[32]) {
  if (fixture_value == NULL || application == NULL || digest == NULL ||
      application->head_struct >= fixture_value->frontend_result.written.structs ||
      fixture_value->frontend_output.structs == NULL ||
      fixture_value->frontend_output.generic_parameters == NULL)
    return false;
  const w_seed_frontend_struct *head =
      &fixture_value->structs[application->head_struct];
  if (head->generic_parameter_count == 0u ||
      head->first_generic_parameter >=
          fixture_value->frontend_result.written.generic_parameters)
    return false;
  const w_seed_frontend_generic_parameter *parameter =
      &fixture_value->generic_parameters[head->first_generic_parameter];
  if (parameter->refinement_kind != W_SEED_FRONTEND_GENERIC_REFINEMENT_PREDICATE)
    return false;
  for (size_t index = 0u;
       index < fixture_value->constir_result.written.functions; index += 1u) {
    const w_seed_constir_function *function =
        &fixture_value->constir_functions[index];
    if (function->frontend_function == parameter->predicate_function_index) {
      (void)memcpy(digest, function->body_digest, 32u);
      return true;
    }
  }
  return false;
}

static bool probe_domain_file_with_quota(const char *path, size_t step_quota,
                                         const char *module_id) {
  if (path == NULL || module_id == NULL || module_id[0] == '\0') return false;
  FILE *file = fopen(path, "rb");
  if (file == NULL) return false;
  char source[SOURCE_BYTES];
  const size_t length = fread(source, 1u, sizeof(source) - 1u, file);
  const bool read_error = ferror(file) != 0;
  const int close_status = fclose(file);
  const bool read_ok = !read_error && close_status == 0;
  if (!read_ok || length == sizeof(source) - 1u) return false;
  source[length] = '\0';
  if (!fixture_lower_with_module(&value, source, module_id)) return false;

  size_t stage_path_count = 0u;
  size_t final_call_count = 0u;
  size_t typed_const_count = 0u;
  printf("GENERIC_RESULT applications=%llu frontend=%d constir=%d\n",
         (unsigned long long)value.frontend_result.written.generic_applications,
         (int)value.frontend_result.status, (int)value.constir_result.status);
  for (size_t application_index = 0u;
       application_index < value.frontend_result.written.generic_applications;
       application_index += 1u) {
    const w_seed_frontend_generic_application *application =
        &value.generic_applications[application_index];
    const bool is_stage_path =
        application->head_name.length == 9u &&
        memcmp(application->head_name.data, "StagePath", 9u) == 0;
    const bool is_final_call =
        application->head_name.length == 14u &&
        memcmp(application->head_name.data, "FinalCallValue", 14u) == 0;
    const bool is_ultimate_answer =
        application->head_name.length == 14u &&
        memcmp(application->head_name.data, "UltimateAnswer", 14u) == 0;
    const bool is_d6 =
        (application->head_name.length == 10u &&
         memcmp(application->head_name.data, "AnswerPair", 10u) == 0) ||
        (application->head_name.length == 11u &&
         memcmp(application->head_name.data, "FailurePair", 11u) == 0);
    const bool is_typed_pending =
        application->binding_status ==
        W_SEED_FRONTEND_GENERIC_BINDING_TYPED_PENDING_CONST;
    if (!is_stage_path && !is_final_call && !is_ultimate_answer && !is_d6 &&
        !is_typed_pending)
      continue;
    w_seed_generic_validation_result result;
    const w_seed_generic_validation_state state = validate_application_at(
        &value, (uint32_t)application_index, CONVERSION_VALUES,
        (w_seed_constir_quota){step_quota, 0u, 64u, SIZE_MAX}, &result);
    uint8_t predicate_digest[32] = {0u};
    (void)predicate_body_digest_for_application(&value, application,
                                                predicate_digest);
    const bool is_d4 = is_ultimate_answer &&
                       value.frontend_result.written.const_declarations != 0u;
    const char *record_kind = is_stage_path ? "GENERIC" :
                              is_final_call ? "STRING" :
                              is_d6 ? "D6" :
                              is_d4 ? "D4" : "D3";
    printf("%s app=%llu state=%s failure=%s diagnostic=%d predicates=%llu "
           "computed=%llu receipts=%llu steps=%llu cache_hits=%llu "
           "cache_misses=%llu",
           record_kind,
           (unsigned long long)application_index,
           w_seed_generic_validation_state_name(state),
           w_seed_generic_validation_failure_name(result.failure),
           (int)result.diagnostic,
           (unsigned long long)result.predicate_count,
           (unsigned long long)result.computed_argument_count,
           (unsigned long long)result.receipts_written,
           (unsigned long long)result.evaluation.consumed_steps,
           (unsigned long long)result.evaluation.const_cache_hits,
           (unsigned long long)result.evaluation.const_cache_misses);
    print_receipt_projection(&value, &result);
    printf(" module=%.*s head=%.*s "
           "fingerprint_state=%s fingerprint_digest=",
           (int)value.modules[application->module_index].module_id.length,
           value.modules[application->module_index].module_id.data,
           (int)application->head_name.length, application->head_name.data,
           w_seed_generic_validation_fingerprint_state_name(
               result.fingerprint_state));
    if (result.fingerprint_state ==
        W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE)
      print_digest(result.fingerprint_digest);
    else
      for (size_t byte = 0u; byte < 32u; byte += 1u) (void)printf("00");
    print_specialization_projection(&value, &result);
    (void)printf(" predicate_body_digest=");
    print_digest(predicate_digest);
    (void)printf(" cycle_path=");
    for (size_t path_index = 0u;
         path_index < result.const_cycle_path_length; path_index += 1u)
      (void)printf("%s%llu", path_index == 0u ? "" : ",",
                   (unsigned long long)result.const_cycle_path[path_index]);
    (void)printf("\n");
    if (is_stage_path)
      stage_path_count += 1u;
    else if (is_final_call)
      final_call_count += 1u;
    else
      typed_const_count += 1u;
  }
  size_t static_value_count = 0u;
  for (size_t application_index = 0u;
       application_index < value.frontend_result.written.generic_applications;
       application_index += 1u) {
    const w_seed_frontend_generic_application *application =
        &value.generic_applications[application_index];
    if (application->head_name.length != 11u ||
        memcmp(application->head_name.data, "StaticValue", 11u) != 0)
      continue;
    w_seed_generic_validation_result result;
    const w_seed_generic_validation_state state = validate_application_at(
        &value, (uint32_t)application_index, CONVERSION_VALUES,
        (w_seed_constir_quota){step_quota, 0u, 64u, SIZE_MAX}, &result);
    printf("STATIC app=%llu state=%s failure=%s fingerprint_state=%s "
           "fingerprint_digest=",
           (unsigned long long)application_index,
           w_seed_generic_validation_state_name(state),
           w_seed_generic_validation_failure_name(result.failure),
           w_seed_generic_validation_fingerprint_state_name(
               result.fingerprint_state));
    if (result.fingerprint_state ==
        W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE)
      print_digest(result.fingerprint_digest);
    else
      for (size_t byte = 0u; byte < 32u; byte += 1u) (void)printf("00");
    print_specialization_projection(&value, &result);
    (void)printf("\n");
    static_value_count += 1u;
  }
  return stage_path_count != 0u || final_call_count != 0u ||
         typed_const_count != 0u || static_value_count != 0u;
}

static bool probe_domain_file(const char *path, const char *module_id) {
  return probe_domain_file_with_quota(path, 100000u, module_id);
}

static bool probe_domain_file_corrupt(const char *path) {
  if (path == NULL) return false;
  FILE *file = fopen(path, "rb");
  if (file == NULL) return false;
  char source[SOURCE_BYTES];
  const size_t length = fread(source, 1u, sizeof(source) - 1u, file);
  const bool read_error = ferror(file) != 0;
  const int close_status = fclose(file);
  if (read_error || close_status != 0 || length == sizeof(source) - 1u)
    return false;
  source[length] = '\0';
  if (!fixture_lower_with_module(&value, source, "restaurant")) return false;
  for (size_t application_index = 0u;
       application_index < value.frontend_result.written.generic_applications;
       application_index += 1u) {
    const w_seed_frontend_generic_application *application =
        &value.generic_applications[application_index];
    if (application->head_name.length != 14u ||
        memcmp(application->head_name.data, "FinalCallValue", 14u) != 0)
      continue;
    const w_seed_frontend_generic_argument *argument =
        &value.generic_arguments[application->first_argument];
    CHECK(argument->const_value_index <
          value.frontend_result.written.const_values);
    value.const_values[argument->const_value_index].first_byte = CONST_BYTES - 1u;
    w_seed_generic_validation_result result;
    const w_seed_generic_validation_state state = validate_application_at(
        &value, (uint32_t)application_index, CONVERSION_VALUES,
        (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result);
    printf("STRING_CORRUPT state=%s failure=%s diagnostic=%d steps=%llu "
           "fingerprint_state=%s fingerprint_digest=",
           w_seed_generic_validation_state_name(state),
           w_seed_generic_validation_failure_name(result.failure),
           (int)result.diagnostic,
           (unsigned long long)result.evaluation.consumed_steps,
               w_seed_generic_validation_fingerprint_state_name(
                   result.fingerprint_state));
    for (size_t byte = 0u; byte < 32u; byte += 1u) (void)printf("00");
    print_specialization_projection(&value, &result);
    (void)printf("\n");
    return state == W_SEED_GENERIC_VALIDATION_INVALID &&
           result.evaluation.consumed_steps == 0u;
  }
  return false;
}

static bool probe_typed_const_corrupt(const char *path) {
  if (path == NULL) return false;
  FILE *file = fopen(path, "rb");
  if (file == NULL) return false;
  char source[SOURCE_BYTES];
  const size_t length = fread(source, 1u, sizeof(source) - 1u, file);
  const bool read_error = ferror(file) != 0;
  const int close_status = fclose(file);
  if (read_error || close_status != 0 || length == sizeof(source) - 1u)
    return false;
  source[length] = '\0';

  static const char *const cases[] = {
      "origin", "relation", "type", "application", "duplicate"};
  bool all_invalid = true;
  for (size_t case_index = 0u;
       case_index < sizeof(cases) / sizeof(cases[0]); case_index += 1u) {
    if (!fixture_lower_with_module(&value, source, "restaurant")) return false;
    uint32_t application_index = W_SEED_FRONTEND_NONE;
    for (size_t index = 0u;
         index < value.frontend_result.written.generic_applications;
         index += 1u) {
      const w_seed_frontend_generic_application *application =
          &value.generic_applications[index];
      if (application->binding_status ==
              W_SEED_FRONTEND_GENERIC_BINDING_TYPED_PENDING_CONST &&
          application->head_name.length == 14u &&
          memcmp(application->head_name.data, "UltimateAnswer", 14u) == 0) {
        application_index = (uint32_t)index;
        break;
      }
    }
    if (application_index == W_SEED_FRONTEND_NONE) return false;
    const w_seed_frontend_generic_application *application =
        &value.generic_applications[application_index];
    const w_seed_frontend_generic_argument *argument =
        &value.generic_arguments[application->first_argument];
    const uint32_t typed_index = argument->typed_const_expression_index;
    if (typed_index == W_SEED_FRONTEND_NONE ||
        (size_t)typed_index >= value.frontend_result.written.typed_const_expressions)
      return false;
    size_t function_index = SIZE_MAX;
    for (size_t index = 0u; index < value.constir_result.written.functions;
         index += 1u) {
      if (value.constir_functions[index].origin ==
              W_SEED_CONSTIR_FUNCTION_ORIGIN_TYPED_CONST_EXPRESSION &&
          value.constir_functions[index].typed_const_expression_index ==
              typed_index) {
        function_index = index;
        break;
      }
    }
    if (function_index == SIZE_MAX) return false;
    if (strcmp(cases[case_index], "origin") == 0) {
      value.constir_functions[function_index].origin =
          W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_FUNCTION;
    } else if (strcmp(cases[case_index], "relation") == 0) {
      value.typed_const_expressions[typed_index].argument_ordinal = 1u;
    } else if (strcmp(cases[case_index], "type") == 0) {
      const uint32_t root_node = value.constir_functions[function_index].root_node;
      if (root_node == W_SEED_CONSTIR_NONE ||
          (size_t)root_node >= value.constir_result.written.nodes)
        return false;
      value.constir_nodes[root_node].type_index = W_SEED_CONSTIR_NONE;
    } else if (strcmp(cases[case_index], "application") == 0) {
      value.generic_applications[application_index].binding_status =
          W_SEED_FRONTEND_GENERIC_BINDING_INVALID;
    } else {
      bool duplicated = false;
      for (size_t index = 0u;
           index < value.constir_result.written.functions; index += 1u) {
        if (index == function_index ||
            value.constir_functions[index].origin !=
                W_SEED_CONSTIR_FUNCTION_ORIGIN_TYPED_CONST_EXPRESSION)
          continue;
        value.constir_functions[index].typed_const_expression_index = typed_index;
        duplicated = true;
        break;
      }
      if (!duplicated) return false;
    }
    w_seed_generic_validation_result result;
    const w_seed_generic_validation_state state = validate_application_at(
        &value, application_index, CONVERSION_VALUES,
        (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result);
    printf("D3_CORRUPT case=%s state=%s failure=%s diagnostic=%d steps=%llu "
           "receipts=%llu fingerprint_state=%s fingerprint_digest=",
           cases[case_index], w_seed_generic_validation_state_name(state),
           w_seed_generic_validation_failure_name(result.failure),
           (int)result.diagnostic,
           (unsigned long long)result.evaluation.consumed_steps,
           (unsigned long long)result.receipts_written,
               w_seed_generic_validation_fingerprint_state_name(
                   result.fingerprint_state));
    for (size_t byte = 0u; byte < 32u; byte += 1u) (void)printf("00");
    print_specialization_projection(&value, &result);
    (void)printf("\n");
    all_invalid = all_invalid && state == W_SEED_GENERIC_VALIDATION_INVALID &&
                  result.evaluation.consumed_steps == 0u &&
                  result.receipts_written == 0u && fingerprint_not_available(&result);
  }
  return all_invalid;
}

static bool probe_module_const_corrupt(const char *path) {
  if (path == NULL) return false;
  FILE *file = fopen(path, "rb");
  if (file == NULL) return false;
  char source[SOURCE_BYTES];
  const size_t length = fread(source, 1u, sizeof(source) - 1u, file);
  const bool read_error = ferror(file) != 0;
  const int close_status = fclose(file);
  if (read_error || close_status != 0 || length == sizeof(source) - 1u)
    return false;
  source[length] = '\0';

  static const char *const cases[] = {
      "origin", "mapping", "dependency", "type", "application"};
  bool all_invalid = true;
  for (size_t case_index = 0u;
       case_index < sizeof(cases) / sizeof(cases[0]); case_index += 1u) {
    if (!fixture_lower_with_module(&value, source, "restaurant")) return false;
    uint32_t application_index = W_SEED_FRONTEND_NONE;
    uint32_t typed_index = W_SEED_FRONTEND_NONE;
    size_t function_index = SIZE_MAX;
    uint32_t root_node = W_SEED_CONSTIR_NONE;
    for (size_t index = 0u;
         index < value.frontend_result.written.generic_applications;
         index += 1u) {
      const w_seed_frontend_generic_application *application =
          &value.generic_applications[index];
      if (application->binding_status !=
              W_SEED_FRONTEND_GENERIC_BINDING_TYPED_PENDING_CONST ||
          application->head_name.length != 14u ||
          memcmp(application->head_name.data, "UltimateAnswer", 14u) != 0)
        continue;
      if (application->first_argument == W_SEED_FRONTEND_NONE ||
          (size_t)application->first_argument >=
              value.frontend_result.written.generic_arguments)
        return false;
      const w_seed_frontend_generic_argument *argument =
          &value.generic_arguments[application->first_argument];
      if (argument->typed_const_expression_index == W_SEED_FRONTEND_NONE ||
          (size_t)argument->typed_const_expression_index >=
              value.frontend_result.written.typed_const_expressions)
        continue;
      for (size_t candidate = 0u;
           candidate < value.constir_result.written.functions; candidate += 1u) {
        const w_seed_constir_function *function = &value.constir_functions[candidate];
        if (function->origin !=
                W_SEED_CONSTIR_FUNCTION_ORIGIN_TYPED_CONST_EXPRESSION ||
            function->typed_const_expression_index !=
                argument->typed_const_expression_index ||
            function->root_node == W_SEED_CONSTIR_NONE ||
            (size_t)function->root_node >= value.constir_result.written.nodes)
          continue;
        const w_seed_constir_node *node = &value.constir_nodes[function->root_node];
        if (node->kind != W_SEED_CONSTIR_NODE_CALL ||
            node->call_target_const_declaration == W_SEED_CONSTIR_NONE)
          continue;
        application_index = (uint32_t)index;
        typed_index = argument->typed_const_expression_index;
        function_index = candidate;
        root_node = function->root_node;
        break;
      }
      if (application_index != W_SEED_FRONTEND_NONE) break;
    }
    if (application_index == W_SEED_FRONTEND_NONE ||
        function_index == SIZE_MAX || typed_index == W_SEED_FRONTEND_NONE ||
        root_node == W_SEED_CONSTIR_NONE)
      return false;
    const uint32_t expression_index =
        value.constir_nodes[root_node].frontend_expression;
    if (expression_index == W_SEED_CONSTIR_NONE ||
        (size_t)expression_index >= value.frontend_result.written.expressions)
      return false;
    if (strcmp(cases[case_index], "origin") == 0) {
      value.constir_functions[function_index].origin =
          W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_FUNCTION;
    } else if (strcmp(cases[case_index], "mapping") == 0) {
      value.expressions[expression_index].resolved_const_declaration =
          W_SEED_FRONTEND_NONE;
    } else if (strcmp(cases[case_index], "dependency") == 0) {
      value.constir_nodes[root_node].call_target_const_declaration =
          (uint32_t)value.frontend_result.written.const_declarations + 1u;
    } else if (strcmp(cases[case_index], "type") == 0) {
      if (value.frontend_result.written.const_declarations == 0u) return false;
      value.const_declarations[0].declared_type = W_SEED_FRONTEND_NONE;
    } else {
      value.generic_applications[application_index].binding_status =
          W_SEED_FRONTEND_GENERIC_BINDING_INVALID;
    }
    w_seed_generic_validation_result result;
    const w_seed_generic_validation_state state = validate_application_at(
        &value, application_index, CONVERSION_VALUES,
        (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result);
    printf("D4_CORRUPT case=%s state=%s failure=%s diagnostic=%d steps=%llu "
           "receipts=%llu cache_hits=%llu cache_misses=%llu "
           "fingerprint_state=%s fingerprint_digest=",
           cases[case_index], w_seed_generic_validation_state_name(state),
           w_seed_generic_validation_failure_name(result.failure),
           (int)result.diagnostic,
           (unsigned long long)result.evaluation.consumed_steps,
           (unsigned long long)result.receipts_written,
           (unsigned long long)result.evaluation.const_cache_hits,
           (unsigned long long)result.evaluation.const_cache_misses,
               w_seed_generic_validation_fingerprint_state_name(
                   result.fingerprint_state));
    for (size_t byte = 0u; byte < 32u; byte += 1u) (void)printf("00");
    print_specialization_projection(&value, &result);
    (void)printf("\n");
    all_invalid = all_invalid && state == W_SEED_GENERIC_VALIDATION_INVALID &&
                  result.evaluation.consumed_steps == 0u &&
                  result.evaluation.const_cache_hits == 0u &&
                  result.evaluation.const_cache_misses == 0u &&
                  result.receipts_written == 0u && fingerprint_not_available(&result);
  }
  return all_invalid;
}

static bool probe_module_const_zero_capacity(const char *path) {
  if (path == NULL) return false;
  FILE *file = fopen(path, "rb");
  if (file == NULL) return false;
  char source[SOURCE_BYTES];
  const size_t length = fread(source, 1u, sizeof(source) - 1u, file);
  const bool read_error = ferror(file) != 0;
  const int close_status = fclose(file);
  if (read_error || close_status != 0 || length == sizeof(source) - 1u)
    return false;
  source[length] = '\0';
  if (!fixture_lower_with_module(&value, source, "restaurant")) return false;
  uint32_t application_index = W_SEED_FRONTEND_NONE;
  for (size_t index = 0u;
       index < value.frontend_result.written.generic_applications;
       index += 1u) {
    const w_seed_frontend_generic_application *application =
        &value.generic_applications[index];
    if (application->head_name.length == 14u &&
        memcmp(application->head_name.data, "UltimateAnswer", 14u) == 0) {
      application_index = (uint32_t)index;
      break;
    }
  }
  if (application_index == W_SEED_FRONTEND_NONE) return false;
  (void)memset(value.conversion_values, 0xa5, sizeof(value.conversion_values));
  (void)memset(value.evidence_bytes, 0xa5, sizeof(value.evidence_bytes));
  (void)memset(value.receipts, 0xa5, sizeof(value.receipts));
  w_seed_generic_validation_result result;
  const w_seed_generic_validation_state state =
      validate_application_at_with_capacities(
          &value, application_index, 0u, value.evidence_bytes,
          W_SEED_GENERIC_VALIDATION_MAX_EVIDENCE_BYTES, 0u,
          (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result);
  printf("D4_ZERO state=%s failure=%s diagnostic=%d computed=%llu steps=%llu receipts=%llu "
         "cache_hits=%llu cache_misses=%llu fingerprint_state=%s "
         "fingerprint_digest=",
         w_seed_generic_validation_state_name(state),
         w_seed_generic_validation_failure_name(result.failure),
         (int)result.diagnostic,
         (unsigned long long)result.computed_argument_count,
         (unsigned long long)result.evaluation.consumed_steps,
         (unsigned long long)result.receipts_written,
         (unsigned long long)result.evaluation.const_cache_hits,
         (unsigned long long)result.evaluation.const_cache_misses,
         w_seed_generic_validation_fingerprint_state_name(
             result.fingerprint_state));
  for (size_t byte = 0u; byte < 32u; byte += 1u) (void)printf("00");
  print_specialization_projection(&value, &result);
  (void)printf(" cycle_path=");
  for (size_t path_index = 0u;
       path_index < result.const_cycle_path_length; path_index += 1u)
    (void)printf("%s%llu", path_index == 0u ? "" : ",",
                 (unsigned long long)result.const_cycle_path[path_index]);
  (void)printf("\n");
  bool buffers_intact = true;
  for (size_t byte = 0u; byte < sizeof(value.conversion_values); byte += 1u)
    buffers_intact = buffers_intact &&
                     ((const uint8_t *)value.conversion_values)[byte] == 0xa5u;
  for (size_t byte = 0u; byte < sizeof(value.evidence_bytes); byte += 1u)
    buffers_intact = buffers_intact && value.evidence_bytes[byte] == 0xa5u;
  for (size_t byte = 0u; byte < sizeof(value.receipts); byte += 1u)
    buffers_intact = buffers_intact &&
                     ((const uint8_t *)value.receipts)[byte] == 0xa5u;
  return state == W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED &&
         result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0002 &&
         result.evaluation.consumed_steps == 0u && result.receipts_written == 0u &&
         result.evaluation.const_cache_hits == 0u &&
         result.evaluation.const_cache_misses == 0u &&
         fingerprint_not_available(&result) && buffers_intact;
}

static bool test_verified_and_rejected_paths(void) {
  static const char standard[] =
      "struct Use { standard: StagePath<[.accepted, .reserving, .preparing, "
      ".serving, .completed]> }\n";
  static const char empty[] = "struct Use { empty: StagePath<[]> }\n";
  static const char skipped[] =
      "struct Use { skipped: StagePath<[.accepted, .completed]> }\n";
  static const char duplicate[] =
      "struct Use { duplicate: StagePath<[.accepted, .reserving, .reserving]> }\n";
  w_seed_generic_validation_result result;
  CHECK(fixture_lower_stage(&value, standard));
  CHECK(value.frontend_result.status == W_SEED_FRONTEND_OK);
  CHECK(value.frontend_result.written.generic_applications == 1u);
  CHECK(validate_application(
            &value, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_VERIFIED);
  CHECK(result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        result.predicate_count == 1u && result.receipts_written == 1u &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_NONE &&
        result.fingerprint_state ==
            W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE &&
        value.receipts[0].kind == W_SEED_GENERIC_VALIDATION_RECEIPT_PREDICATE &&
        value.receipts[0].result_is_bool && value.receipts[0].bool_value);
  w_seed_generic_validation_result first_result = result;
  w_seed_generic_validation_receipt first_receipt = value.receipts[0];
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED);
  CHECK(memcmp(&first_result, &result, sizeof(result)) == 0 &&
        memcmp(&first_receipt, &value.receipts[0], sizeof(first_receipt)) == 0);

  CHECK(fixture_lower_stage(&value, empty));
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_REJECTED);
  CHECK(result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0004 &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_PREDICATE_FALSE &&
        result.fingerprint_state ==
            W_SEED_GENERIC_VALIDATION_FINGERPRINT_NOT_AVAILABLE &&
        memcmp(result.fingerprint_digest,
               (uint8_t[W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES]){0},
               W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES) == 0 &&
        result.rejection.failure.length == 15u &&
        result.rejection.failure.data == (const char *)value.evidence_bytes &&
        result.rejection.rejection_trace[0].data ==
            (const char *)value.evidence_bytes &&
        memcmp(result.rejection.failure.data, "predicate:false", 15u) == 0 &&
        result.rejection.rejection_trace_count == 1u &&
        result.rejection.rejection_trace[0].length == 15u &&
        memcmp(result.rejection.rejection_trace[0].data, "predicate:false",
               15u) == 0 &&
        result.rejection.rejection_trace[0].length <=
            W_SEED_GENERIC_VALIDATION_MAX_EVIDENCE_BYTES &&
        result.receipts_written == 1u &&
        value.receipts[0].kind == W_SEED_GENERIC_VALIDATION_RECEIPT_PREDICATE &&
        !value.receipts[0].bool_value);
  w_seed_generic_validation_rejection empty_rejection = result.rejection;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_REJECTED &&
        memcmp(&empty_rejection, &result.rejection, sizeof(empty_rejection)) == 0);

  CHECK(fixture_lower_stage(&value, skipped));
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_REJECTED &&
        result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0004 &&
        fingerprint_not_available(&result));
  CHECK(fixture_lower_stage(&value, duplicate));
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_REJECTED &&
        result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0004 &&
        fingerprint_not_available(&result));

  CHECK(fixture_lower_stage(&value, empty));
  (void)memset(value.evidence_bytes, 0xa5, sizeof(value.evidence_bytes));
  (void)memset(value.conversion_values, 0xa5, sizeof(value.conversion_values));
  (void)memset(value.receipts, 0xa5, sizeof(value.receipts));
  CHECK(validate_application_at_with_evidence(
            &value, 0u, CONVERSION_VALUES, value.evidence_bytes,
            W_SEED_GENERIC_VALIDATION_FALLBACK_BYTES - 1u,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_CAPACITY &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_CAPACITY &&
        result.evaluation.consumed_steps == 0u && result.receipts_written == 0u &&
        fingerprint_not_available(&result));
  for (size_t byte = 0u; byte < sizeof(value.evidence_bytes); byte += 1u)
    CHECK(value.evidence_bytes[byte] == 0xa5u);
  for (size_t byte = 0u; byte < sizeof(value.conversion_values); byte += 1u)
    CHECK(((const uint8_t *)value.conversion_values)[byte] == 0xa5u);
  for (size_t byte = 0u; byte < sizeof(value.receipts); byte += 1u)
    CHECK(((const uint8_t *)value.receipts)[byte] == 0xa5u);

  CHECK(fixture_lower_stage(&value, empty));
  (void)memset(value.conversion_values, 0xa5, sizeof(value.conversion_values));
  (void)memset(value.receipts, 0xa5, sizeof(value.receipts));
  CHECK(validate_application_at_with_evidence(
            &value, 0u, CONVERSION_VALUES, NULL,
            W_SEED_GENERIC_VALIDATION_MAX_EVIDENCE_BYTES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_CAPACITY &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_CAPACITY &&
        result.evaluation.consumed_steps == 0u && result.receipts_written == 0u &&
        fingerprint_not_available(&result));
  for (size_t byte = 0u; byte < sizeof(value.receipts); byte += 1u)
    CHECK(((const uint8_t *)value.receipts)[byte] == 0xa5u);
  for (size_t byte = 0u; byte < sizeof(value.conversion_values); byte += 1u)
    CHECK(((const uint8_t *)value.conversion_values)[byte] == 0xa5u);
  return true;
}

static bool test_quota_unsupported_and_invalid(void) {
  static const char standard[] =
      "struct Use { standard: StagePath<[.accepted, .reserving, .preparing, "
      ".serving, .completed]> }\n";
  w_seed_generic_validation_result result;
  CHECK(fixture_lower_stage(&value, standard));
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){0u, 0u, 64u, SIZE_MAX},
                             &result) ==
        W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED);
  CHECK(result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003 &&
        result.evaluation.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003 &&
        result.receipts_written == 1u && fingerprint_not_available(&result));

  CHECK(fixture_lower_stage(&value, standard));
  value.generic_applications[0].binding_status =
      W_SEED_FRONTEND_GENERIC_BINDING_UNSUPPORTED;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                              &result) == W_SEED_GENERIC_VALIDATION_UNSUPPORTED &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_BINDING &&
        result.receipts_written == 0u && fingerprint_not_available(&result));

  CHECK(fixture_lower_stage(&value, standard));
  value.generic_applications[0].binding_status =
      W_SEED_FRONTEND_GENERIC_BINDING_INVALID;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        result.evaluation.consumed_steps == 0u &&
        fingerprint_not_available(&result));

  CHECK(fixture_lower_stage(&value, standard));
  value.generic_parameters[0].predicate_function_span.start_byte += 1u;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        result.evaluation.consumed_steps == 0u &&
        fingerprint_not_available(&result));

  CHECK(fixture_lower_stage(&value, standard));
  value.generic_parameters[0].module_index = 1u;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        result.evaluation.consumed_steps == 0u &&
        fingerprint_not_available(&result));

  CHECK(fixture_lower_stage(&value, standard));
  value.generic_arguments[value.generic_applications[0].first_argument]
      .binding_status = W_SEED_FRONTEND_GENERIC_BINDING_TYPED_PENDING_CONST;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                              &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        result.evaluation.consumed_steps == 0u &&
        fingerprint_not_available(&result));

  CHECK(fixture_lower_stage(&value, standard));
  value.generic_arguments[value.generic_applications[0].first_argument]
      .parameter_index = W_SEED_FRONTEND_NONE;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.receipts_written == 0u && result.evaluation.consumed_steps == 0u &&
        fingerprint_not_available(&result));

  CHECK(fixture_lower_stage(&value, standard));
  value.generic_applications[0].argument_count = 0u;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        result.evaluation.consumed_steps == 0u &&
        fingerprint_not_available(&result));

  CHECK(fixture_lower_stage(&value, standard));
  value.generic_parameters[0].predicate_function_index = FUNCTIONS + 1u;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        result.evaluation.consumed_steps == 0u &&
        fingerprint_not_available(&result));

  CHECK(fixture_lower_stage(&value, standard));
  const uint32_t predicate_frontend_function =
      value.generic_parameters[0].predicate_function_index;
  bool predicate_function_found = false;
  for (size_t function_index = 0u;
       function_index < value.constir_result.written.functions;
       function_index += 1u) {
    if (value.constir_functions[function_index].frontend_function ==
        predicate_frontend_function) {
      value.constir_functions[function_index].lowerable = false;
      predicate_function_found = true;
      break;
    }
  }
  CHECK(predicate_function_found);
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_UNSUPPORTED &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_FUNCTION &&
        result.receipts_written == 0u && fingerprint_not_available(&result));

  CHECK(fixture_lower_stage(&value, standard));
  (void)memset(value.receipts, 0xa5, sizeof(value.receipts));
  (void)memset(value.conversion_values, 0xa5, sizeof(value.conversion_values));
  CHECK(validate_application(&value, 1u,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_CAPACITY &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_CAPACITY &&
        result.receipts_written == 0u && fingerprint_not_available(&result));
  for (size_t index = 0u; index < sizeof(value.receipts); index += 1u)
    CHECK(((const uint8_t *)value.receipts)[index] == 0xa5u);
  for (size_t index = 0u; index < sizeof(value.conversion_values); index += 1u)
    CHECK(((const uint8_t *)value.conversion_values)[index] == 0xa5u);

  CHECK(fixture_lower_stage(&value, standard));
  CHECK(value.frontend_result.written.generic_applications == 1u);
  const uint32_t string_value_index =
      value.generic_arguments[value.generic_applications[0].first_argument]
          .const_value_index;
  CHECK(string_value_index < value.frontend_result.written.const_values);
  value.const_values[string_value_index].kind = W_SEED_FRONTEND_CONST_STRING;
  value.const_values[string_value_index].first_byte = 0u;
  value.const_values[string_value_index].byte_count = 0u;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        result.receipts_written == 0u && result.evaluation.consumed_steps == 0u &&
        fingerprint_not_available(&result));

  CHECK(fixture_lower_stage(&value, standard));
  const uint32_t invalid_value_index =
      value.generic_arguments[value.generic_applications[0].first_argument]
          .const_value_index;
  value.const_values[invalid_value_index].kind = W_SEED_FRONTEND_CONST_INVALID;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        result.receipts_written == 0u && result.evaluation.consumed_steps == 0u &&
        fingerprint_not_available(&result));

  CHECK(fixture_lower_stage(&value, standard));
  const uint32_t over_limit_index =
      value.generic_arguments[value.generic_applications[0].first_argument]
          .const_value_index;
  value.const_values[over_limit_index].element_count =
      W_SEED_CONSTIR_MAX_STATIC_LIST_ELEMENTS + 1u;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));

  CHECK(fixture_lower_stage(&value, standard));
  const uint32_t complete_over_limit_index =
      value.generic_arguments[value.generic_applications[0].first_argument]
          .const_value_index;
  w_seed_frontend_const_value *complete_over_limit =
      &value.const_values[complete_over_limit_index];
  const uint32_t complete_first_element = complete_over_limit->first_element;
  const uint32_t complete_child_value =
      value.const_elements[complete_first_element].value_index;
  const uint32_t complete_count = W_SEED_CONSTIR_MAX_STATIC_LIST_ELEMENTS + 1u;
  CHECK((size_t)complete_first_element + complete_count < CONST_ELEMENTS);
  for (uint32_t offset = 0u; offset < complete_count; offset += 1u) {
    value.const_elements[(size_t)complete_first_element + offset] =
        (w_seed_frontend_const_element){
            complete_over_limit_index, offset, complete_child_value,
            complete_over_limit->span};
  }
  complete_over_limit->element_count = complete_count;
  value.frontend_result.written.const_elements =
      (size_t)complete_first_element + complete_count;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_UNSUPPORTED &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_VALUE &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));

  CHECK(fixture_lower_stage(&value, standard));
  const uint32_t const_value_index =
      value.generic_arguments[value.generic_applications[0].first_argument]
          .const_value_index;
  value.const_values[const_value_index].first_element = 999999u;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));
  return true;
}

static bool test_direct_scalar_predicates(void) {
  static const char bool_source[] =
      "const fn identity(value: Bool): Bool { return value }\n"
      "struct BoolBox<_ value: Bool<(identity(.member))>> {}\n"
      "struct Use { yes: BoolBox<true> no: BoolBox<false> }\n";
  static const char integer_source[] =
      "const fn isSeven(value: u8): Bool { return value == 7 }\n"
      "struct IntBox<_ value: u8<(isSeven(.member))>> {}\n"
      "struct Use { yes: IntBox<7> no: IntBox<8> }\n";
  static const char two_predicate_source[] =
      "const fn firstPredicate(value: Bool): Bool { return value }\n"
      "const fn secondPredicate(value: Bool): Bool { return value }\n"
      "struct Pair<_ first: Bool<(firstPredicate(.member))>, _ second: Bool<(secondPredicate(.member))>> {}\n"
      "struct Use { valid: Pair<true, true> }\n";
  static const char enum_source[] =
      "enum Color { red green blue }\n"
      "enum Other { red }\n"
      "const fn isRed(value: Color): Bool { return switch value { case .red: true case .green: false case .blue: false } }\n"
      "struct ColorBox<_ value: Color<(isRed(.member))>> {}\n"
      "struct Use { yes: ColorBox<.red> no: ColorBox<.green> }\n";
  w_seed_generic_validation_result result;

  CHECK(fixture_lower(&value, bool_source));
  CHECK(value.frontend_result.written.generic_applications == 2u);
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_VERIFIED);
  CHECK(value.conversion_values[0].kind == W_SEED_CONSTIR_VALUE_BOOL &&
        value.conversion_values[0].bool_value);
  CHECK(validate_application_at(
            &value, 1u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_REJECTED &&
        result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0004);

  CHECK(fixture_lower(&value, integer_source));
  CHECK(value.frontend_result.written.generic_applications == 2u);
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_VERIFIED);
  CHECK(value.conversion_values[0].kind == W_SEED_CONSTIR_VALUE_INTEGER &&
        !value.conversion_values[0].type_is_signed &&
        value.conversion_values[0].type_bit_width == 8u &&
        value.conversion_values[0].integer_value[0] == 7u &&
        value.conversion_values[0].integer_value[1] == 0u);
  CHECK(validate_application_at(
            &value, 1u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_REJECTED);
  CHECK(value.conversion_values[0].integer_value[0] == 8u &&
        value.conversion_values[0].type_bit_width == 8u);
  value.const_values[value.generic_arguments[1].const_value_index]
      .integer_bit_width = 16u;
  CHECK(validate_application_at(
            &value, 1u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_INVALID &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        result.evaluation.consumed_steps == 0u);
  CHECK(fixture_lower(&value, integer_source));
  const uint32_t high_byte_value_index =
      value.generic_arguments[value.generic_applications[0].first_argument]
          .const_value_index;
  CHECK(value.const_values[high_byte_value_index].integer_byte_count == 1u);
  value.const_values[high_byte_value_index].integer_bytes[1] = 1u;
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_INVALID &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        result.evaluation.consumed_steps == 0u);

  CHECK(fixture_lower(&value, two_predicate_source));
  CHECK(value.frontend_result.written.generic_applications == 1u);
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        result.predicate_count == 2u && result.receipts_written == 2u);
  const uint32_t second_predicate_frontend_function =
      value.generic_parameters[1].predicate_function_index;
  bool second_function_found = false;
  for (size_t function_index = 0u;
       function_index < value.constir_result.written.functions;
       function_index += 1u) {
    w_seed_constir_function *function = &value.constir_functions[function_index];
    if (function->frontend_function != second_predicate_frontend_function)
      continue;
    CHECK(function->parameter_count == 1u);
    value.constir_parameters[function->first_parameter].type_kind =
        W_SEED_FRONTEND_TYPE_INTEGER;
    value.constir_parameters[function->first_parameter].type_is_signed = false;
    value.constir_parameters[function->first_parameter].type_bit_width = 8u;
    second_function_found = true;
    break;
  }
  CHECK(second_function_found);
  (void)memset(value.receipts, 0xa5, sizeof(value.receipts));
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        result.evaluation.consumed_steps == 0u && result.receipts_written == 0u);
  for (size_t byte = 0u; byte < sizeof(value.receipts); byte += 1u)
    CHECK(((const uint8_t *)value.receipts)[byte] == 0xa5u);

  CHECK(fixture_lower(&value, enum_source));
  uint32_t color_applications[2] = {W_SEED_FRONTEND_NONE,
                                    W_SEED_FRONTEND_NONE};
  size_t color_application_count = 0u;
  for (size_t application_index = 0u;
       application_index < value.frontend_result.written.generic_applications;
       application_index += 1u) {
    const w_seed_frontend_generic_application *application =
        &value.generic_applications[application_index];
    if (application->head_name.length == 8u &&
        memcmp(application->head_name.data, "ColorBox", 8u) == 0) {
      CHECK(color_application_count < 2u);
      color_applications[color_application_count++] =
          (uint32_t)application_index;
    }
  }
  CHECK(color_application_count == 2u);
  const uint32_t color_parameter_index =
      value.structs[value.generic_applications[color_applications[0]].head_struct]
          .first_generic_parameter;
  CHECK(validate_application_at(
            &value, color_applications[0], CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_VERIFIED);
  uint8_t full_color_fingerprint[
      W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES] = {0};
  (void)memcpy(full_color_fingerprint, result.fingerprint_digest,
               sizeof(full_color_fingerprint));
  const uint32_t red_value_index =
      value.generic_arguments[value.generic_applications[color_applications[0]]
                                  .first_argument]
          .const_value_index;
  CHECK(value.conversion_values[0].kind == W_SEED_CONSTIR_VALUE_ENUM &&
        value.conversion_values[0].enum_base_index ==
            value.const_values[red_value_index].enum_base_index &&
        value.conversion_values[0].enum_case_index ==
            value.const_values[red_value_index].enum_case_index);
  CHECK(validate_application_at(
            &value, color_applications[1], CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_REJECTED);
  const uint32_t green_value_index =
      value.generic_arguments[value.generic_applications[color_applications[1]]
                                  .first_argument]
          .const_value_index;
  value.const_values[green_value_index].enum_base_index = 1u;
  CHECK(validate_application_at(
            &value, color_applications[1], CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_INVALID &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));
  value.const_values[green_value_index].enum_base_index = 0u;
  value.enum_cases[value.const_values[green_value_index].enum_case_index]
      .first_payload = W_SEED_FRONTEND_NONE;
  value.enum_cases[value.const_values[green_value_index].enum_case_index]
      .payload_count = 1u;
  CHECK(validate_application_at(
            &value, color_applications[1], CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_INVALID &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));

  CHECK(fixture_lower(&value, enum_source));
  const uint32_t subset_type_index =
      (uint32_t)value.frontend_result.written.types;
  CHECK((size_t)subset_type_index < TYPES);
  const uint32_t subset_member_index =
      (uint32_t)value.frontend_result.written.enum_subset_members;
  CHECK((size_t)subset_member_index + 2u <= ENUM_SUBSET_MEMBERS);
  const w_seed_frontend_type full_color_type = value.types[
      value.generic_parameters[color_parameter_index].domain_type];
  w_seed_frontend_type subset_type = full_color_type;
  subset_type.kind = W_SEED_FRONTEND_TYPE_ENUM_SUBSET;
  subset_type.first_subset_member = subset_member_index;
  subset_type.subset_member_count = 2u;
  subset_type.generic_application_index = W_SEED_FRONTEND_NONE;
  value.types[subset_type_index] = subset_type;
  const uint32_t red_case_index = value.const_values[red_value_index].enum_case_index;
  const uint32_t green_case_index =
      value.const_values[green_value_index].enum_case_index;
  value.enum_subset_members[subset_member_index] =
      (w_seed_frontend_enum_subset_member){
          subset_type_index, 0u, red_case_index,
          value.const_values[red_value_index].span};
  value.enum_subset_members[subset_member_index + 1u] =
      (w_seed_frontend_enum_subset_member){
          subset_type_index, 0u, green_case_index,
          value.const_values[green_value_index].span};
  value.frontend_result.written.types += 1u;
  value.frontend_result.written.enum_subset_members += 2u;
  value.generic_parameters[color_parameter_index].domain_type =
      subset_type_index;
  value.const_values[value.generic_arguments[value.generic_applications[
      color_applications[0]].first_argument]
                         .const_value_index]
      .type_index = subset_type_index;
  value.const_values[value.generic_arguments[value.generic_applications[
      color_applications[1]].first_argument]
                         .const_value_index]
      .type_index = subset_type_index;
  CHECK(validate_application_at(
            &value, color_applications[0], CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_VERIFIED &&
        result.fingerprint_state ==
            W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE &&
        memcmp(full_color_fingerprint, result.fingerprint_digest,
               sizeof(full_color_fingerprint)) != 0);
  const w_seed_span subset_enum_span = value.enums[0].span;
  value.enums[0].span.start_byte = subset_enum_span.end_byte + 1u;
  CHECK(validate_application_at(
            &value, color_applications[0], CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_INVALID &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));
  value.enums[0].span = subset_enum_span;
  CHECK(validate_application_at(
            &value, color_applications[0], CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_VERIFIED &&
        result.fingerprint_state ==
            W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE);
  uint8_t subset_fingerprint[
      W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES] = {0};
  (void)memcpy(subset_fingerprint, result.fingerprint_digest,
               sizeof(subset_fingerprint));
  const w_seed_frontend_enum_subset_member first_subset_member =
      value.enum_subset_members[subset_member_index];
  value.enum_subset_members[subset_member_index] =
      value.enum_subset_members[subset_member_index + 1u];
  value.enum_subset_members[subset_member_index + 1u] = first_subset_member;
  CHECK(validate_application_at(
            &value, color_applications[0], CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_VERIFIED &&
        memcmp(subset_fingerprint, result.fingerprint_digest,
               sizeof(subset_fingerprint)) == 0);
  value.enum_subset_members[subset_member_index + 1u] =
      value.enum_subset_members[subset_member_index];
  value.enum_subset_members[subset_member_index] = first_subset_member;
  const uint32_t blue_case_index = red_case_index + 2u;
  CHECK(blue_case_index < value.frontend_result.written.enum_cases);
  value.enum_subset_members[subset_member_index + 1u].enum_case_index =
      blue_case_index;
  CHECK(validate_application_at(
            &value, color_applications[0], CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_VERIFIED &&
        memcmp(subset_fingerprint, result.fingerprint_digest,
               sizeof(subset_fingerprint)) != 0);
  value.enum_subset_members[subset_member_index + 1u].enum_case_index =
      green_case_index;
  CHECK(validate_application_at(
            &value, color_applications[1], CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_REJECTED);
  value.enum_subset_members[subset_member_index].owner_type =
      W_SEED_FRONTEND_NONE;
  CHECK(validate_application_at(
            &value, color_applications[0], CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_INVALID &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));
  return true;
}

static bool test_typed_const_expression_predicates(void) {
  static const char source[] =
      "const fn isUltimateAnswer(value: i64): Bool { return value == 42 }\n"
      "struct UltimateAnswer<_ value: i64<(isUltimateAnswer(.member))>> {}\n"
      "struct Use {\n"
      "  immediate: UltimateAnswer<42>\n"
      "  computed: UltimateAnswer<(6 * 7)>\n"
      "  duplicate: UltimateAnswer<(6 * 7)>\n"
      "  rejected: UltimateAnswer<(6 * 6)>\n"
      "}\n";
  CHECK(fixture_lower(&value, source));
  CHECK(value.frontend_result.written.generic_applications == 4u);
  CHECK(value.frontend_result.written.typed_const_expressions == 3u);
  CHECK(value.frontend_result.written.functions == 1u);
  CHECK(value.constir_result.written.functions == 4u);
  for (size_t application_index = 0u; application_index < 4u;
       application_index += 1u) {
    const w_seed_frontend_generic_application *application =
        &value.generic_applications[application_index];
    CHECK(application->binding_status ==
          (application_index == 0u
               ? W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE
               : W_SEED_FRONTEND_GENERIC_BINDING_TYPED_PENDING_CONST));
    CHECK(application->requires_const_evaluation);
    const w_seed_frontend_generic_argument *argument =
        &value.generic_arguments[application->first_argument];
    CHECK(argument->binding_status ==
          (application_index == 0u
               ? W_SEED_FRONTEND_GENERIC_BINDING_BOUND_IMMEDIATE
               : W_SEED_FRONTEND_GENERIC_BINDING_TYPED_PENDING_CONST));
    CHECK(argument->owner_application == application_index);
    if (application_index == 0u) {
      CHECK(argument->typed_const_expression_index == W_SEED_FRONTEND_NONE);
    } else {
      CHECK(argument->typed_const_expression_index == application_index - 1u);
      const w_seed_frontend_typed_const_expression *typed =
          &value.typed_const_expressions[application_index - 1u];
      CHECK(typed->owner_application == application_index);
      CHECK(typed->argument_ordinal == 0u);
      CHECK(typed->expected_type == typed->effective_type);
    }
  }
  for (size_t function_index = 1u; function_index < 4u; function_index += 1u) {
    const w_seed_constir_function *function =
        &value.constir_functions[function_index];
    CHECK(function->origin ==
          W_SEED_CONSTIR_FUNCTION_ORIGIN_TYPED_CONST_EXPRESSION);
    CHECK(function->frontend_function == W_SEED_CONSTIR_NONE);
    CHECK(function->typed_const_expression_index == function_index - 1u);
    CHECK(function->lowerable);
    CHECK(function->parameter_count == 0u);
  }
  CHECK(memcmp(value.constir_functions[1].body_digest,
               value.constir_functions[2].body_digest,
               sizeof(value.constir_functions[1].body_digest)) == 0);

  w_seed_generic_validation_result immediate_result;
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &immediate_result) == W_SEED_GENERIC_VALIDATION_VERIFIED);
  CHECK(immediate_result.predicate_count == 1u &&
        immediate_result.computed_argument_count == 0u &&
        immediate_result.receipts_written == 1u &&
        immediate_result.fingerprint_state ==
            W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE);
  CHECK(value.receipts[0].kind ==
            W_SEED_GENERIC_VALIDATION_RECEIPT_PREDICATE &&
        value.receipts[0].typed_const_expression_index ==
            W_SEED_FRONTEND_NONE && value.receipts[0].bool_value);

  uint8_t computed_digest[W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES];
  w_seed_generic_validation_result computed_result;
  CHECK(validate_application_at(
            &value, 1u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &computed_result) == W_SEED_GENERIC_VALIDATION_VERIFIED);
  (void)memcpy(computed_digest, computed_result.fingerprint_digest,
               sizeof(computed_digest));
  CHECK(computed_result.predicate_count == 1u &&
        computed_result.computed_argument_count == 1u &&
        computed_result.receipts_written == 2u &&
        value.receipts[0].kind ==
            W_SEED_GENERIC_VALIDATION_RECEIPT_CONST_ARGUMENT &&
        value.receipts[0].typed_const_expression_index == 0u &&
        value.receipts[0].evaluation.consumed_steps != 0u &&
        value.receipts[0].eval_value.kind ==
            W_SEED_CONSTIR_VALUE_INTEGER &&
        value.receipts[0].eval_value.integer_value[0] == 42u &&
        value.receipts[1].kind ==
            W_SEED_GENERIC_VALIDATION_RECEIPT_PREDICATE &&
        value.receipts[1].typed_const_expression_index == 0u &&
        value.receipts[1].bool_value);
  CHECK(memcmp(immediate_result.fingerprint_digest, computed_digest,
               sizeof(computed_digest)) == 0);

  const size_t computed_steps = value.receipts[0].evaluation.consumed_steps;
  const size_t predicate_steps = value.receipts[1].evaluation.consumed_steps;
  CHECK(computed_steps != 0u && predicate_steps != 0u &&
        computed_steps <= SIZE_MAX - predicate_steps &&
        computed_steps + predicate_steps > 1u);
  w_seed_generic_validation_result cumulative_quota_result;
  CHECK(validate_application_at(
            &value, 1u, CONVERSION_VALUES,
            (w_seed_constir_quota){computed_steps + predicate_steps - 1u, 0u,
                                   64u, SIZE_MAX},
            &cumulative_quota_result) ==
            W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED);
  CHECK(cumulative_quota_result.computed_argument_count == 1u &&
        cumulative_quota_result.predicate_count == 1u &&
        cumulative_quota_result.diagnostic ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003 &&
        cumulative_quota_result.evaluation.diagnostic ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003 &&
        cumulative_quota_result.receipts_written == 2u &&
        value.receipts[0].kind ==
            W_SEED_GENERIC_VALIDATION_RECEIPT_CONST_ARGUMENT &&
        value.receipts[0].evaluation.diagnostic ==
            W_SEED_CONSTIR_DIAGNOSTIC_NONE &&
        value.receipts[1].kind ==
            W_SEED_GENERIC_VALIDATION_RECEIPT_PREDICATE &&
        value.receipts[1].evaluation.diagnostic ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003 &&
        fingerprint_not_available(&cumulative_quota_result));

  w_seed_generic_validation_result duplicate_result;
  CHECK(validate_application_at(
            &value, 2u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &duplicate_result) == W_SEED_GENERIC_VALIDATION_VERIFIED);
  CHECK(duplicate_result.computed_argument_count == 1u &&
        memcmp(computed_digest, duplicate_result.fingerprint_digest,
               sizeof(computed_digest)) == 0);

  w_seed_generic_validation_result rejected_result;
  CHECK(validate_application_at(
            &value, 3u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &rejected_result) == W_SEED_GENERIC_VALIDATION_REJECTED);
  CHECK(rejected_result.failure ==
            W_SEED_GENERIC_VALIDATION_FAILURE_PREDICATE_FALSE &&
        rejected_result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0004 &&
        rejected_result.computed_argument_count == 1u &&
        rejected_result.receipts_written == 2u &&
        value.receipts[0].eval_value.integer_value[0] == 36u &&
        value.receipts[1].kind ==
            W_SEED_GENERIC_VALIDATION_RECEIPT_PREDICATE &&
        value.receipts[1].bool_value == false &&
        rejected_result.rejection.typed_const_expression_index == 2u &&
        fingerprint_not_available(&rejected_result));

  static const char computed_without_predicate[] =
      "struct Box<_ value: i64> {}\n"
      "struct Use { field: Box<(6 * 7)> }\n";
  CHECK(fixture_lower(&value, computed_without_predicate));
  (void)memset(value.conversion_values, 0xa5, sizeof(value.conversion_values));
  (void)memset(value.receipts, 0xa5, sizeof(value.receipts));
  w_seed_generic_validation_result capacity_result;
  CHECK(validate_application_at(
            &value, 0u, 0u,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &capacity_result) == W_SEED_GENERIC_VALIDATION_CAPACITY &&
        capacity_result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_CAPACITY &&
        capacity_result.computed_argument_count == 1u &&
        capacity_result.evaluation.consumed_steps == 0u &&
        capacity_result.receipts_written == 0u &&
        fingerprint_not_available(&capacity_result));
  for (size_t index = 0u; index < sizeof(value.conversion_values); index += 1u)
    CHECK(((const uint8_t *)value.conversion_values)[index] == 0xa5u);
  for (size_t index = 0u; index < sizeof(value.receipts); index += 1u)
    CHECK(((const uint8_t *)value.receipts)[index] == 0xa5u);

  CHECK(fixture_lower(&value, computed_without_predicate));
  w_seed_generic_validation_result quota_result;
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){0u, 0u, 64u, SIZE_MAX}, &quota_result) ==
            W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED &&
        quota_result.failure ==
            W_SEED_GENERIC_VALIDATION_FAILURE_EVALUATOR_DIAGNOSTIC &&
        quota_result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003 &&
        quota_result.evaluation.diagnostic ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003 &&
        quota_result.evaluation.consumed_steps == 0u &&
        quota_result.receipts_written == 1u &&
        value.receipts[0].kind ==
            W_SEED_GENERIC_VALIDATION_RECEIPT_CONST_ARGUMENT &&
        value.receipts[0].typed_const_expression_index == 0u &&
        fingerprint_not_available(&quota_result));

  static const char overflow_source[] =
      "struct Box<_ value: i8> {}\n"
      "struct Use { field: Box<(127 + 1)> }\n";
  CHECK(fixture_lower(&value, overflow_source));
  w_seed_generic_validation_result overflow_result;
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &overflow_result) == W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED &&
        overflow_result.failure ==
            W_SEED_GENERIC_VALIDATION_FAILURE_EVALUATOR_DIAGNOSTIC &&
        overflow_result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0006 &&
        overflow_result.evaluation.diagnostic ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0006 &&
        overflow_result.evaluation.consumed_steps != 0u &&
        overflow_result.receipts_written == 1u &&
        value.receipts[0].kind ==
            W_SEED_GENERIC_VALIDATION_RECEIPT_CONST_ARGUMENT &&
        value.receipts[0].typed_const_expression_index == 0u &&
        fingerprint_not_available(&overflow_result));

  static const char unsupported_call_source[] =
      "const fn helper(_ value: i64): i64 { return value }\n"
      "struct Box<_ value: i64> {}\n"
      "struct Use { field: Box<(helper(6))> }\n";
  CHECK(fixture_lower(&value, unsupported_call_source));
  CHECK(value.frontend_result.status == W_SEED_FRONTEND_OK &&
        value.constir_result.written.functions == 2u &&
        !value.constir_functions[1].lowerable);
  w_seed_generic_validation_result unsupported_call_result;
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &unsupported_call_result) ==
            W_SEED_GENERIC_VALIDATION_UNSUPPORTED &&
        unsupported_call_result.failure ==
            W_SEED_GENERIC_VALIDATION_FAILURE_FUNCTION &&
        unsupported_call_result.evaluation.consumed_steps == 0u &&
        unsupported_call_result.receipts_written == 0u &&
        fingerprint_not_available(&unsupported_call_result));

  static const char audit_only_source[] =
      "struct Pair<_ first: i64, _ second: i64> {}\n"
      "struct Use { pair: Pair<(6 * 7), (\"42\")> }\n";
  CHECK(fixture_lower(&value, audit_only_source));
  w_seed_generic_validation_result audit_only_result;
  CHECK(value.frontend_result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        value.frontend_result.written.typed_const_expressions == 1u &&
        value.constir_result.written.functions == 1u &&
        !value.constir_functions[0].lowerable &&
        validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &audit_only_result) == W_SEED_GENERIC_VALIDATION_UNSUPPORTED &&
        audit_only_result.failure ==
            W_SEED_GENERIC_VALIDATION_FAILURE_BINDING &&
        audit_only_result.evaluation.consumed_steps == 0u &&
        audit_only_result.receipts_written == 0u &&
        fingerprint_not_available(&audit_only_result));

  static const char unsupported_identifier_source[] =
      "struct Box<_ value: i64> {}\n"
      "struct Use { field: Box<(unknownValue)> }\n";
  CHECK(fixture_lower(&value, unsupported_identifier_source));
  w_seed_generic_validation_result unsupported_identifier_result;
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &unsupported_identifier_result) ==
            W_SEED_GENERIC_VALIDATION_UNSUPPORTED &&
        unsupported_identifier_result.failure ==
            W_SEED_GENERIC_VALIDATION_FAILURE_BINDING &&
        unsupported_identifier_result.evaluation.consumed_steps == 0u &&
        unsupported_identifier_result.receipts_written == 0u &&
        fingerprint_not_available(&unsupported_identifier_result));

  static const char unsupported_string_result_source[] =
      "struct Box<_ value: i64> {}\n"
      "struct Use { field: Box<(\"42\")> }\n";
  CHECK(fixture_lower(&value, unsupported_string_result_source));
  w_seed_generic_validation_result unsupported_string_result;
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &unsupported_string_result) ==
            W_SEED_GENERIC_VALIDATION_UNSUPPORTED &&
        unsupported_string_result.failure ==
            W_SEED_GENERIC_VALIDATION_FAILURE_BINDING &&
        unsupported_string_result.evaluation.consumed_steps == 0u &&
        unsupported_string_result.receipts_written == 0u &&
        fingerprint_not_available(&unsupported_string_result));

  CHECK(fixture_lower(&value, computed_without_predicate));
  value.constir_functions[0].origin =
      W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_FUNCTION;
  w_seed_generic_validation_result origin_corruption_result;
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &origin_corruption_result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        origin_corruption_result.failure ==
            W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        origin_corruption_result.evaluation.consumed_steps == 0u &&
        origin_corruption_result.receipts_written == 0u &&
        fingerprint_not_available(&origin_corruption_result));

  CHECK(fixture_lower(&value, computed_without_predicate));
  value.typed_const_expressions[0].argument_ordinal = 1u;
  w_seed_generic_validation_result relation_corruption_result;
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &relation_corruption_result) ==
            W_SEED_GENERIC_VALIDATION_INVALID &&
        relation_corruption_result.failure ==
            W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        relation_corruption_result.evaluation.consumed_steps == 0u &&
        relation_corruption_result.receipts_written == 0u &&
        fingerprint_not_available(&relation_corruption_result));

  CHECK(fixture_lower(&value, computed_without_predicate));
  value.typed_const_expressions[0].effective_type = W_SEED_FRONTEND_NONE;
  w_seed_generic_validation_result type_corruption_result;
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &type_corruption_result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        type_corruption_result.failure ==
            W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        type_corruption_result.evaluation.consumed_steps == 0u &&
        type_corruption_result.receipts_written == 0u &&
        fingerprint_not_available(&type_corruption_result));
  return true;
}

static bool test_preflight_capacity_precedence(void) {
  static const char source[] =
      "const fn isUltimateAnswer(value: i64): Bool { return value == 42 }\n"
      "struct UltimateAnswer<_ value: i64<(isUltimateAnswer(.member))>> {}\n"
      "struct Use { computed: UltimateAnswer<(6 * 7)> }\n";
  CHECK(fixture_lower(&value, source));
  CHECK(value.frontend_result.written.generic_applications == 1u &&
        value.frontend_result.written.typed_const_expressions == 1u);

  (void)memset(value.conversion_values, 0xa5, sizeof(value.conversion_values));
  (void)memset(value.receipts, 0xa5, sizeof(value.receipts));
  (void)memset(value.evidence_bytes, 0xa5, sizeof(value.evidence_bytes));
  value.generic_parameters[0].predicate_function_index = FUNCTIONS + 1u;
  w_seed_generic_validation_result invalid_result;
  CHECK(validate_application_at_with_capacities(
            &value, 0u, 0u, value.evidence_bytes,
            W_SEED_GENERIC_VALIDATION_MAX_EVIDENCE_BYTES, 0u,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &invalid_result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        invalid_result.failure ==
            W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        invalid_result.computed_argument_count == 1u &&
        invalid_result.evaluation.consumed_steps == 0u &&
        invalid_result.receipts_written == 0u &&
        fingerprint_not_available(&invalid_result));
  for (size_t byte = 0u; byte < sizeof(value.conversion_values); byte += 1u)
    CHECK(((const uint8_t *)value.conversion_values)[byte] == 0xa5u);
  for (size_t byte = 0u; byte < sizeof(value.receipts); byte += 1u)
    CHECK(((const uint8_t *)value.receipts)[byte] == 0xa5u);
  for (size_t byte = 0u; byte < sizeof(value.evidence_bytes); byte += 1u)
    CHECK(value.evidence_bytes[byte] == 0xa5u);

  CHECK(fixture_lower(&value, source));
  (void)memset(value.conversion_values, 0xa5, sizeof(value.conversion_values));
  (void)memset(value.receipts, 0xa5, sizeof(value.receipts));
  (void)memset(value.evidence_bytes, 0xa5, sizeof(value.evidence_bytes));
  w_seed_generic_validation_result capacity_result;
  CHECK(validate_application_at_with_capacities(
            &value, 0u, 0u, value.evidence_bytes,
            W_SEED_GENERIC_VALIDATION_MAX_EVIDENCE_BYTES, 0u,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &capacity_result) == W_SEED_GENERIC_VALIDATION_CAPACITY &&
        capacity_result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_CAPACITY &&
        capacity_result.computed_argument_count == 1u &&
        capacity_result.predicate_count == 1u &&
        capacity_result.evaluation.consumed_steps == 0u &&
        capacity_result.receipts_written == 0u &&
        fingerprint_not_available(&capacity_result));
  for (size_t byte = 0u; byte < sizeof(value.conversion_values); byte += 1u)
    CHECK(((const uint8_t *)value.conversion_values)[byte] == 0xa5u);
  for (size_t byte = 0u; byte < sizeof(value.receipts); byte += 1u)
    CHECK(((const uint8_t *)value.receipts)[byte] == 0xa5u);
  for (size_t byte = 0u; byte < sizeof(value.evidence_bytes); byte += 1u)
    CHECK(value.evidence_bytes[byte] == 0xa5u);
  return true;
}

static bool test_dependent_effective_domains(void) {
  static const char static_value_source[] =
      "struct StaticValue<T, _ value: T> {}\n"
      "struct Use { boolValue: StaticValue<Bool, true> "
      "stringValue: StaticValue<String, \"The final seating\"> }\n";
  static const char static_value_with_alias_source[] =
      "alias UnrelatedAlias = Bool\n"
      "struct Unrelated {}\n"
      "\nstruct StaticValue<T, _ value: T> {}\n"
      "struct Use { boolValue: StaticValue<Bool, true> }\n";
  w_seed_generic_validation_result result;
  CHECK(fixture_lower(&value, static_value_source));
  CHECK(value.frontend_result.written.generic_applications == 2u);
  const w_seed_generic_validation_state dependent_state =
      validate_application_at(
          &value, 0u, CONVERSION_VALUES,
          (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result);
  CHECK(dependent_state == W_SEED_GENERIC_VALIDATION_VERIFIED);
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_VERIFIED);
  CHECK(result.fingerprint_state ==
            W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE);
  uint8_t bool_digest[W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES] = {0u};
  (void)memcpy(bool_digest, result.fingerprint_digest, sizeof(bool_digest));
  CHECK(validate_application_at(
            &value, 1u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_VERIFIED);
  CHECK(result.fingerprint_state ==
            W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE);
  uint8_t string_digest[W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES] = {0u};
  (void)memcpy(string_digest, result.fingerprint_digest,
               sizeof(string_digest));
  CHECK(memcmp(bool_digest, string_digest, sizeof(bool_digest)) != 0);
  const uint32_t string_value_index =
      value.generic_arguments[value.generic_applications[1].first_argument + 1u]
          .const_value_index;
  CHECK(string_value_index < value.frontend_result.written.const_values);
  value.const_bytes[value.const_values[string_value_index].first_byte] = 't';
  CHECK(validate_application_at(
            &value, 1u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_VERIFIED &&
        memcmp(string_digest, result.fingerprint_digest,
               sizeof(string_digest)) != 0);

  /* Spans and allocation/index shifts do not enter the semantic preimage. */
  CHECK(fixture_lower(&value, static_value_with_alias_source));
  CHECK(value.frontend_result.written.generic_applications == 1u);
  CHECK(validate_application(
            &value, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
        W_SEED_GENERIC_VALIDATION_VERIFIED &&
        memcmp(bool_digest, result.fingerprint_digest, sizeof(bool_digest)) ==
            0);

  /* Each malformed dependent relation is rejected before ConstIR runs. */
  CHECK(fixture_lower(&value, static_value_source));
  const uint32_t dependent_parameter =
      value.structs[value.generic_applications[0].head_struct]
          .first_generic_parameter + 1u;
  value.generic_parameters[dependent_parameter]
      .dependent_type_parameter_ordinal = 1u; /* self */
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));
  static const char forward_source[] =
      "struct Forward<T, _ value: T, U> {}\n"
      "struct Use { item: Forward<Bool, true, Bool> }\n";
  CHECK(fixture_lower(&value, forward_source));
  CHECK(value.frontend_result.written.generic_applications == 1u);
  const uint32_t forward_parameter =
      value.structs[value.generic_applications[0].head_struct]
          .first_generic_parameter + 1u;
  value.generic_parameters[forward_parameter]
      .dependent_type_parameter_ordinal = 2u; /* forward, in range */
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));
  CHECK(fixture_lower(&value, forward_source));
  value.generic_parameters[forward_parameter]
      .dependent_type_parameter_ordinal = 3u; /* out of range */
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));
  CHECK(fixture_lower(&value, static_value_source));
  value.generic_parameters[dependent_parameter]
      .dependent_type_parameter_ordinal = W_SEED_FRONTEND_NONE;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));
  CHECK(fixture_lower(&value, static_value_source));
  value.generic_parameters[dependent_parameter].domain_type =
      value.generic_arguments[value.generic_applications[0].first_argument]
          .type_index; /* stale dependent domain must be NONE */
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));
  CHECK(fixture_lower(&value, static_value_source));
  const uint32_t first_parameter =
      value.structs[value.generic_applications[0].head_struct]
          .first_generic_parameter;
  value.generic_parameters[first_parameter].kind =
      W_SEED_FRONTEND_GENERIC_KIND_VALUE; /* referenced slot is VALUE */
  value.generic_parameters[first_parameter].domain_kind =
      W_SEED_FRONTEND_GENERIC_DOMAIN_CONCRETE;
  value.generic_parameters[first_parameter].domain_type =
      value.generic_arguments[value.generic_applications[0].first_argument]
          .type_index;
  const uint32_t first_argument =
      value.generic_applications[0].first_argument;
  const uint32_t dependent_argument = first_argument + 1u;
  value.generic_arguments[first_argument].kind =
      W_SEED_FRONTEND_GENERIC_ARGUMENT_VALUE;
  value.generic_arguments[first_argument].type_index = W_SEED_FRONTEND_NONE;
  value.generic_arguments[first_argument].const_value_index =
      value.generic_arguments[dependent_argument].const_value_index;
  value.generic_arguments[first_argument].span =
      value.generic_arguments[dependent_argument].span;
  /* The referenced slot is a coherent VALUE argument; this isolates the
   * resolver's required TYPE-parameter relation. */
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));
  CHECK(fixture_lower(&value, static_value_source));
  value.generic_arguments[value.generic_applications[0].first_argument]
      .kind = W_SEED_FRONTEND_GENERIC_ARGUMENT_VALUE; /* prior non-TYPE */
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));
  CHECK(fixture_lower(&value, static_value_source));
  value.generic_arguments[value.generic_applications[0].first_argument]
      .binding_status = W_SEED_FRONTEND_GENERIC_BINDING_UNSUPPORTED;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));
  CHECK(fixture_lower(&value, static_value_source));
  const uint32_t mismatch_value =
      value.generic_arguments[value.generic_applications[0].first_argument + 1u]
          .const_value_index;
  value.const_values[mismatch_value].type_index =
      value.generic_applications[0].owner_type;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));

  /* Existing concrete-domain and StagePath tests cover their regressions. */
  return true;
}

static bool test_string_predicate_conversion_boundary(void) {
  static const char source[] =
      "const fn acceptsLabel(value: String): Bool { "
      "return value == \"The final seating\" }\n"
      "struct StringBox<_ value: String<(acceptsLabel(.member))>> {}\n"
      "struct Use { item: StringBox<\"The final seating\"> }\n";
  w_seed_generic_validation_result result;
  w_seed_constir_status constir_status = W_SEED_CONSTIR_OK;
  CHECK(fixture_lower_base(&value, source, "generic_test", &constir_status));
  CHECK(constir_status == W_SEED_CONSTIR_OK);
  CHECK(value.frontend_result.written.generic_applications == 1u);
  const w_seed_generic_validation_state string_state = validate_application(
      &value, CONVERSION_VALUES,
      (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result);
  CHECK(string_state == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_NONE &&
        result.receipts_written == 1u && result.evaluation.consumed_steps != 0u &&
        result.fingerprint_state ==
            W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE);
  uint8_t first_digest[W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES] = {0u};
  (void)memcpy(first_digest, result.fingerprint_digest, sizeof(first_digest));
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        result.fingerprint_state ==
            W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE &&
        memcmp(first_digest, result.fingerprint_digest, sizeof(first_digest)) ==
            0);

  static const char false_source[] =
      "const fn acceptsLabel(value: String): Bool { "
      "return value != \"Mostly harmless\" }\n"
      "struct StringBox<_ value: String<(acceptsLabel(.member))>> {}\n"
      "struct Use { item: StringBox<\"Mostly harmless\"> }\n";
  CHECK(fixture_lower(&value, false_source));
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_REJECTED &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_PREDICATE_FALSE &&
        result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0004 &&
        result.receipts_written == 1u && result.evaluation.consumed_steps != 0u &&
        fingerprint_not_available(&result));

  static const char empty_source[] =
      "const fn acceptsEmpty(value: String): Bool { return value == \"\" }\n"
      "struct StringBox<_ value: String<(acceptsEmpty(.member))>> {}\n"
      "struct Use { item: StringBox<\"\"> }\n";
  CHECK(fixture_lower(&value, empty_source));
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        result.fingerprint_state ==
            W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE &&
        result.evaluation.consumed_steps != 0u);
  CHECK(value.frontend_result.written.const_bytes == 0u);
  value.frontend_output.const_bytes = NULL;
  value.frontend_output.const_bytes_capacity = 0u;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        result.fingerprint_state ==
            W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE &&
        result.evaluation.consumed_steps != 0u &&
        memcmp(result.fingerprint_digest,
               (uint8_t[W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES]){0},
               W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES) != 0);

  /* The limit is a feature boundary.  A body without String literals remains
   * lowerable, while an over-limit argument is rejected before evaluation. */
  static char over_limit_source[SOURCE_BYTES];
  static const char over_limit_prefix[] =
      "const fn acceptsLabel(value: String): Bool { return true }\n"
      "struct StringBox<_ value: String<(acceptsLabel(.member))>> {}\n"
      "struct Use { item: StringBox<\"";
  static const char over_limit_suffix[] = "\"> }\n";
  const size_t over_limit_prefix_length = strlen(over_limit_prefix);
  const size_t over_limit_suffix_length = strlen(over_limit_suffix);
  CHECK(over_limit_prefix_length + W_SEED_CONSTIR_MAX_STRING_BYTES +
            1u + over_limit_suffix_length + 1u <
        sizeof(over_limit_source));
  (void)memcpy(over_limit_source, over_limit_prefix,
               over_limit_prefix_length);
  (void)memset(over_limit_source + over_limit_prefix_length, 'x',
               W_SEED_CONSTIR_MAX_STRING_BYTES + 1u);
  (void)memcpy(over_limit_source + over_limit_prefix_length +
                   W_SEED_CONSTIR_MAX_STRING_BYTES + 1u,
               over_limit_suffix, over_limit_suffix_length);
  over_limit_source[over_limit_prefix_length +
                    W_SEED_CONSTIR_MAX_STRING_BYTES + 1u +
                    over_limit_suffix_length] = '\0';
  CHECK(fixture_lower(&value, over_limit_source));
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_UNSUPPORTED &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_VALUE &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));

  /* Corrupt the normalized arena relation.  The generic preflight must reject
   * it before fingerprinting or ConstIR evaluation. */
  CHECK(fixture_lower(&value, source));
  const uint32_t malformed_value_index =
      value.generic_arguments[value.generic_applications[0].first_argument]
          .const_value_index;
  CHECK(malformed_value_index < value.frontend_result.written.const_values);
  value.const_values[malformed_value_index].first_byte = CONST_BYTES - 1u;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));

  CHECK(fixture_lower(&value, source));
  const uint32_t mismatch_value_index =
      value.generic_arguments[value.generic_applications[0].first_argument]
          .const_value_index;
  CHECK(mismatch_value_index < value.frontend_result.written.const_values);
  value.const_values[mismatch_value_index].type_index =
      value.generic_applications[0].owner_type;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));

  CHECK(fixture_lower(&value, source));
  value.frontend_output.const_bytes_capacity = 0u;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));
  CHECK(fixture_lower(&value, source));
  value.frontend_output.const_bytes = NULL;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));

  /* This source-backed String witness closes the D2 conversion boundary:
   * valid borrowed values reach the ordinary evaluator and false predicates
   * preserve W-CONST-0004 with no fingerprint. */
  return true;
}

static bool test_fingerprint_adversarial_inputs(void) {
  static const char always_source[] =
      "const fn always(value: Bool): Bool { return true }\n"
      "struct Box<value: Bool<(always(.member))>> {}\n"
      "struct Use { item: Box<value: true> }\n";
  static const char always_spaced_source[] =
      "\n// formatting does not change the semantic projection\n"
      "const fn always(value: Bool): Bool { return true }\n\n"
      "struct Box<value: Bool<(always(.member))>> {}\n"
      "// named value labels remain outside the semantic projection\n"
      "struct Use { item: Box<value: true> }\n";
  static const char always_prefixed_source[] =
      "struct Unrelated {}\n"
      "const fn always(value: Bool): Bool { return true }\n"
      "struct Box<value: Bool<(always(.member))>> {}\n"
      "struct Use { item: Box<value: true> }\n";
  static const char always_label_source[] =
      "const fn always(value: Bool): Bool { return true }\n"
      "struct Box<value: Bool<(always(.member))>> {}\n"
      "struct Use { item: Box<value: true> }\n";
  static const char other_head_source[] =
      "const fn always(value: Bool): Bool { return true }\n"
      "struct OtherBox<value: Bool<(always(.member))>> {}\n"
      "struct Use { item: OtherBox<value: true> }\n";
  static const char list_source[] =
      "enum Color { red green }\n"
      "const fn alwaysList(value: StaticList<Color>): Bool { return true }\n"
      "struct ListBox<value: StaticList<Color><(alwaysList(.member))>> {}\n"
      "struct Use { item: ListBox<value: [.red, .green]> }\n";
  static const char nominal_source[] =
      "const fn keep(value: Bool): Bool { return value }\n"
      "type LocalId = u64\n"
      "struct Box<T> {}\n"
      "struct Use { item: Box<LocalId> }\n";
  static const char alias_source[] =
      "const fn keep(value: Bool): Bool { return value }\n"
      "type LocalId = u64\n"
      "alias AliasId = LocalId\n"
      "struct Box<T> {}\n"
      "struct Use { item: Box<AliasId> }\n";
  static const char unknown_nominal_source[] =
      "const fn keep(value: Bool): Bool { return value }\n"
      "struct Box<T> {}\n"
      "struct Use { item: Box<Unknown> }\n";
  static const char nested_nominal_source[] =
      "const fn keep(value: Bool): Bool { return value }\n"
      "struct Inner<T> {}\n"
      "struct Outer<T> {}\n"
      "struct Use { item: Outer<Inner<u64>> }\n";
  static const char integer_source[] =
      "const fn alwaysInt(value: u8): Bool { return true }\n"
      "struct IntBox<value: u8<(alwaysInt(.member))>> {}\n"
      "struct Use { item: IntBox<value: 7> }\n";
  static const char same_head_integer_source[] =
      "const fn always(value: u8): Bool { return true }\n"
      "struct Box<value: u8<(always(.member))>> {}\n"
      "struct Use { item: Box<value: 7> }\n";
  static const char enum_source[] =
      "enum Color { red green blue }\n"
      "const fn isRed(value: Color): Bool { return true }\n"
      "struct ColorBox<value: Color<(isRed(.member))>> {}\n"
      "struct Use { item: ColorBox<value: .red> }\n";
  w_seed_generic_validation_result result;
  CHECK(fixture_lower(&value, always_source));
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED);
  CHECK(result.fingerprint_state ==
        W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE);
  uint8_t first_digest[W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES] = {0};
  (void)memcpy(first_digest, result.fingerprint_digest,
               sizeof(first_digest));
  uint8_t first_body_digest[W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES] = {0};
  CHECK(predicate_body_digest_for_application(
      &value, &value.generic_applications[0], first_body_digest));
  uint8_t spaced_body_digest[W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES] = {0};
  CHECK(fixture_lower(&value, always_spaced_source));
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        memcmp(first_digest, result.fingerprint_digest,
               sizeof(first_digest)) == 0 &&
        predicate_body_digest_for_application(
            &value, &value.generic_applications[0], spaced_body_digest) &&
        memcmp(first_body_digest, spaced_body_digest,
               sizeof(first_body_digest)) == 0);
  uint8_t prefixed_body_digest[W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES] = {0};
  CHECK(fixture_lower(&value, always_prefixed_source));
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        memcmp(first_digest, result.fingerprint_digest,
               sizeof(first_digest)) == 0 &&
        predicate_body_digest_for_application(
            &value, &value.generic_applications[0], prefixed_body_digest) &&
        memcmp(first_body_digest, prefixed_body_digest,
               sizeof(first_body_digest)) == 0);
  CHECK(fixture_lower(&value, always_source));
  const uint32_t stale_refinement_parameter =
      value.structs[value.generic_applications[0].head_struct]
          .first_generic_parameter;
  value.generic_parameters[stale_refinement_parameter].refinement_kind =
      W_SEED_FRONTEND_GENERIC_REFINEMENT_NONE;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        result.evaluation.consumed_steps == 0u &&
        fingerprint_not_available(&result));
  CHECK(fixture_lower(&value, integer_source));
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        result.fingerprint_state ==
            W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE);
  uint8_t integer_digest[W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES] = {0};
  (void)memcpy(integer_digest, result.fingerprint_digest,
               sizeof(integer_digest));
  const uint32_t integer_value_index =
      value.generic_arguments[value.generic_applications[0].first_argument]
          .const_value_index;
  value.const_values[integer_value_index].integer_bytes[0] = 8u;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        memcmp(integer_digest, result.fingerprint_digest,
               sizeof(integer_digest)) != 0);
  CHECK(memcmp(first_digest, integer_digest, sizeof(first_digest)) != 0);
  CHECK(fixture_lower(&value, same_head_integer_source));
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        result.fingerprint_state ==
            W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE &&
        memcmp(first_digest, result.fingerprint_digest,
               sizeof(first_digest)) != 0);
  CHECK(fixture_lower(&value, enum_source));
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        result.fingerprint_state ==
            W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE);
  uint8_t enum_digest[W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES] = {0};
  (void)memcpy(enum_digest, result.fingerprint_digest, sizeof(enum_digest));
  const uint32_t enum_value_index =
      value.generic_arguments[value.generic_applications[0].first_argument]
          .const_value_index;
  const uint32_t enum_case_index =
      value.const_values[enum_value_index].enum_case_index;
  CHECK(enum_case_index + 1u < value.frontend_result.written.enum_cases);
  value.const_values[enum_value_index].enum_case_index = enum_case_index + 1u;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        memcmp(enum_digest, result.fingerprint_digest,
               sizeof(enum_digest)) != 0);
  value.const_values[enum_value_index].enum_case_index = enum_case_index;
  const w_seed_span enum_span = value.enums[0].span;
  value.enums[0].span.start_byte = enum_span.end_byte + 1u;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));
  value.enums[0].span = enum_span;
  const uint32_t enum_case_module =
      value.enum_cases[enum_case_index].module_index;
  value.enum_cases[enum_case_index].module_index = 1u;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));
  value.enum_cases[enum_case_index].module_index = enum_case_module;
  const w_seed_frontend_text enum_case_name =
      value.enum_cases[enum_case_index].name;
  value.enum_cases[enum_case_index].name = (w_seed_frontend_text){NULL, 0u};
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));
  value.enum_cases[enum_case_index].name = enum_case_name;
  const w_seed_frontend_text enum_name = value.enums[0].name;
  value.enums[0].name = (w_seed_frontend_text){NULL, 0u};
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.evaluation.consumed_steps == 0u && fingerprint_not_available(&result));
  value.enums[0].name = enum_name;
  CHECK(fixture_lower(&value, always_label_source));
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        memcmp(first_digest, result.fingerprint_digest,
               sizeof(first_digest)) == 0);
  CHECK(fixture_lower_with_module(&value, always_source, "other"));
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        result.fingerprint_state ==
            W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE &&
        memcmp(first_digest, result.fingerprint_digest,
               sizeof(first_digest)) != 0);

  CHECK(fixture_lower(&value, other_head_source));
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        memcmp(first_digest, result.fingerprint_digest,
               sizeof(first_digest)) != 0);

  CHECK(fixture_lower(&value, list_source));
  CHECK(value.frontend_result.written.generic_applications == 1u);
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        result.fingerprint_state ==
            W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE);
  uint8_t list_digest[W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES] = {0};
  (void)memcpy(list_digest, result.fingerprint_digest, sizeof(list_digest));
  const uint32_t list_value_index =
      value.generic_arguments[value.generic_applications[0].first_argument]
          .const_value_index;
  CHECK(value.const_values[list_value_index].element_count == 2u);
  const uint32_t list_first_element =
      value.const_values[list_value_index].first_element;
  CHECK(list_first_element != W_SEED_FRONTEND_NONE);
  const uint32_t list_first_case =
      value.const_values[value.const_elements[list_first_element].value_index]
          .enum_case_index;
  const uint32_t list_second_case = value.const_values[
      value.const_elements[list_first_element + 1u].value_index]
                                          .enum_case_index;
  value.const_values[value.const_elements[list_first_element].value_index]
      .enum_case_index = list_second_case;
  value.const_values[value.const_elements[list_first_element + 1u].value_index]
      .enum_case_index = list_first_case;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        memcmp(list_digest, result.fingerprint_digest, sizeof(list_digest)) != 0);
  uint8_t list_reordered_digest[W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES] = {0};
  (void)memcpy(list_reordered_digest, result.fingerprint_digest,
               sizeof(list_reordered_digest));
  value.const_values[value.const_elements[list_first_element + 1u].value_index]
      .enum_case_index = list_second_case;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        memcmp(list_reordered_digest, result.fingerprint_digest,
               sizeof(list_reordered_digest)) != 0);

  CHECK(fixture_lower(&value, always_source));
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        result.fingerprint_state ==
            W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE);
  uint8_t stable_digest[W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES] = {0};
  (void)memcpy(stable_digest, result.fingerprint_digest,
               sizeof(stable_digest));
  const uint32_t stable_value_index =
      value.generic_arguments[value.generic_applications[0].first_argument]
          .const_value_index;
  value.const_values[stable_value_index].bool_value = false;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        memcmp(stable_digest, result.fingerprint_digest,
               sizeof(stable_digest)) != 0);

  CHECK(fixture_lower(&value, always_source));
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED);
  uint8_t body_digest_before[
      W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES] = {0};
  (void)memcpy(body_digest_before, result.fingerprint_digest,
               sizeof(body_digest_before));
  const uint32_t predicate_function =
      value.generic_parameters[value.structs[0].first_generic_parameter]
          .predicate_function_index;
  for (size_t index = 0u; index < value.constir_result.written.functions;
       index += 1u) {
    if (value.constir_functions[index].frontend_function == predicate_function) {
      value.constir_functions[index].body_digest[0] ^= 0x01u;
      break;
    }
  }
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        memcmp(body_digest_before, result.fingerprint_digest,
               sizeof(body_digest_before)) != 0);

  /* A local `type` declaration is a resolvable nominal in the same module. */
  CHECK(fixture_lower(&value, nominal_source));
  CHECK(value.frontend_result.written.generic_applications == 1u);
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        result.fingerprint_state ==
            W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE);
  uint8_t nominal_digest[W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES] = {0};
  (void)memcpy(nominal_digest, result.fingerprint_digest,
               sizeof(nominal_digest));

  /* Alias names are transparent identities.  The projection reports the
   * nominal alias as unsupported, while the valid application stays VERIFIED. */
  CHECK(fixture_lower(&value, alias_source));
  CHECK(value.frontend_result.written.generic_applications == 1u);
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        fingerprint_unsupported(&result));

  /* Unknown source nominals may be rejected by the current binder.  If a
   * valid application is representable, it must remain VERIFIED with an
   * UNSUPPORTED fingerprint; otherwise it is the ordinary binding gap. */
  CHECK(fixture_lower(&value, unknown_nominal_source));
  CHECK(value.frontend_result.written.generic_applications == 1u);
  const w_seed_generic_validation_state unknown_state =
      validate_application(&value, CONVERSION_VALUES,
                           (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                           &result);
  if (unknown_state == W_SEED_GENERIC_VALIDATION_VERIFIED)
    CHECK(fingerprint_unsupported(&result));
  else
    CHECK((unknown_state == W_SEED_GENERIC_VALIDATION_UNSUPPORTED ||
           unknown_state == W_SEED_GENERIC_VALIDATION_INVALID) &&
          fingerprint_not_available(&result));

  CHECK(fixture_lower(&value, nested_nominal_source));
  CHECK(value.frontend_result.written.generic_applications >= 1u);
  uint32_t outer_application_index = W_SEED_FRONTEND_NONE;
  for (size_t application_index = 0u;
       application_index < value.frontend_result.written.generic_applications;
       application_index += 1u) {
    const w_seed_frontend_generic_application *application =
        &value.generic_applications[application_index];
    if (application->head_name.length == 5u &&
        memcmp(application->head_name.data, "Outer", 5u) == 0) {
      outer_application_index = (uint32_t)application_index;
      break;
    }
  }
  CHECK(outer_application_index != W_SEED_FRONTEND_NONE);
  const w_seed_generic_validation_state nested_state =
      validate_application_at(
          &value, outer_application_index, CONVERSION_VALUES,
          (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result);
  if (nested_state == W_SEED_GENERIC_VALIDATION_VERIFIED)
    CHECK(fingerprint_unsupported(&result));
  else
    CHECK((nested_state == W_SEED_GENERIC_VALIDATION_UNSUPPORTED ||
           nested_state == W_SEED_GENERIC_VALIDATION_INVALID) &&
          fingerprint_not_available(&result));

  CHECK(fixture_lower(&value, always_source));
  const size_t saved_type_declaration_count =
      value.frontend_result.written.type_declarations;
  w_seed_frontend_type_declaration *saved_type_declarations =
      value.frontend_output.type_declarations;
  const size_t saved_type_declaration_capacity =
      value.frontend_output.type_declaration_capacity;
  value.frontend_result.written.type_declarations = 1u;
  value.frontend_output.type_declarations = NULL;
  value.frontend_output.type_declaration_capacity = 0u;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.fingerprint_state ==
            W_SEED_GENERIC_VALIDATION_FINGERPRINT_NOT_AVAILABLE &&
        result.evaluation.consumed_steps == 0u &&
        memcmp(result.fingerprint_digest,
               (uint8_t[W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES]){0},
               W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES) == 0);
  value.frontend_result.written.type_declarations =
      saved_type_declaration_count;
  value.frontend_output.type_declarations = saved_type_declarations;
  value.frontend_output.type_declaration_capacity =
      saved_type_declaration_capacity;
  value.frontend_result.written.type_declarations = 1u;
  value.frontend_output.type_declaration_capacity = 0u;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        fingerprint_not_available(&result) &&
        result.evaluation.consumed_steps == 0u);
  value.frontend_result.written.type_declarations =
      saved_type_declaration_count;
  value.frontend_output.type_declaration_capacity =
      saved_type_declaration_capacity;
  const size_t saved_alias_count = value.frontend_result.written.aliases;
  w_seed_frontend_alias *saved_aliases = value.frontend_output.aliases;
  const size_t saved_alias_capacity = value.frontend_output.alias_capacity;
  value.frontend_result.written.aliases = 1u;
  value.frontend_output.aliases = NULL;
  value.frontend_output.alias_capacity = 0u;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        result.fingerprint_state ==
            W_SEED_GENERIC_VALIDATION_FINGERPRINT_NOT_AVAILABLE &&
        result.evaluation.consumed_steps == 0u &&
        memcmp(result.fingerprint_digest,
               (uint8_t[W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES]){0},
               W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES) == 0);
  value.frontend_result.written.aliases = saved_alias_count;
  value.frontend_output.aliases = saved_aliases;
  value.frontend_output.alias_capacity = saved_alias_capacity;
  value.frontend_result.written.aliases = 1u;
  value.frontend_output.alias_capacity = 0u;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        fingerprint_not_available(&result) &&
        result.evaluation.consumed_steps == 0u);
  value.frontend_result.written.aliases = saved_alias_count;
  value.frontend_output.alias_capacity = saved_alias_capacity;
  return true;
}

static bool test_predicate_session_isolation(void) {
  static const char source[] =
      "const answerSeed = 21\n"
      "const firstAnswerHalf = answerSeed\n"
      "const secondAnswerHalf = answerSeed\n"
      "export const assembledUltimateAnswer = firstAnswerHalf + secondAnswerHalf\n"
      "const fn isUltimateAnswer(value: i64): Bool { return value == assembledUltimateAnswer }\n"
      "struct UltimateAnswer<_ value: i64<(isUltimateAnswer(.member))>> {}\n"
      "struct Use { computed: UltimateAnswer<(assembledUltimateAnswer)> }\n";
  CHECK(fixture_lower(&value, source));
  CHECK(value.frontend_result.written.generic_applications == 1u);
  w_seed_generic_validation_result result;
  CHECK(validate_application(
            &value, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &result) == W_SEED_GENERIC_VALIDATION_VERIFIED);
  CHECK(result.computed_argument_count == 1u && result.predicate_count == 1u &&
        result.receipts_written == 2u);
  CHECK(value.receipts[0].kind ==
            W_SEED_GENERIC_VALIDATION_RECEIPT_CONST_ARGUMENT &&
        value.receipts[0].evaluation.consumed_steps == 7u &&
        value.receipts[0].evaluation.const_cache_misses == 4u &&
        value.receipts[0].evaluation.const_cache_hits == 1u &&
        value.receipts[1].kind == W_SEED_GENERIC_VALIDATION_RECEIPT_PREDICATE &&
        value.receipts[1].evaluation.consumed_steps == 9u &&
        value.receipts[1].evaluation.const_cache_misses == 4u &&
        value.receipts[1].evaluation.const_cache_hits == 1u &&
        value.receipts[0].effective_type ==
            value.receipts[0].eval_value.type_index &&
        value.types[value.receipts[1].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_INTEGER &&
        value.types[value.receipts[1].effective_type].bit_width == 64u &&
        value.receipts[1].effective_type !=
            value.receipts[1].eval_value.type_index &&
        value.receipts[1].result_is_bool && value.receipts[1].bool_value);
  return true;
}

static bool test_named_module_const_d4(void) {
  static const char named_source[] =
      "export const ultimateAnswer: i64 = 6 * 7\n"
      "const fn isUltimateAnswer(value: i64): Bool { return value == 42 }\n"
      "struct UltimateAnswer<_ value: i64<(isUltimateAnswer(.member))>> {}\n"
      "struct Use {\n"
      "  immediate: UltimateAnswer<42>\n"
      "  computed: UltimateAnswer<(6 * 7)>\n"
      "  named: UltimateAnswer<(ultimateAnswer)>\n"
      "}\n";
  CHECK(fixture_lower(&value, named_source));
  CHECK(value.frontend_result.status == W_SEED_FRONTEND_OK);
  CHECK(value.frontend_result.written.const_declarations == 1u);
  CHECK(value.constir_result.written.functions == 4u);
  CHECK(value.constir_functions[0].origin ==
            W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_FUNCTION &&
        value.constir_functions[1].origin ==
            W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_CONST_DECLARATION &&
        value.constir_functions[1].frontend_const_declaration == 0u &&
        value.constir_functions[1].lowerable);
  for (size_t index = 2u; index < 4u; index += 1u)
    CHECK(value.constir_functions[index].origin ==
              W_SEED_CONSTIR_FUNCTION_ORIGIN_TYPED_CONST_EXPRESSION &&
          value.constir_functions[index].lowerable);
  uint8_t fingerprints[3][W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES];
  for (uint32_t application = 0u; application < 3u; application += 1u) {
    w_seed_generic_validation_result result;
    CHECK(validate_application_at(
              &value, application, CONVERSION_VALUES,
              (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result) ==
          W_SEED_GENERIC_VALIDATION_VERIFIED);
    CHECK(result.fingerprint_state ==
          W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE &&
          result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_NONE);
    (void)memcpy(fingerprints[application], result.fingerprint_digest,
                 sizeof(fingerprints[application]));
    if (application == 0u)
      CHECK(result.computed_argument_count == 0u &&
            result.receipts_written == 1u);
    else
      CHECK(result.computed_argument_count == 1u &&
            result.receipts_written == 2u &&
            value.receipts[0].kind ==
                W_SEED_GENERIC_VALIDATION_RECEIPT_CONST_ARGUMENT &&
            value.receipts[0].eval_value.kind ==
                W_SEED_CONSTIR_VALUE_INTEGER &&
            value.receipts[0].eval_value.integer_value[0] == 42u);
  }
  CHECK(memcmp(fingerprints[0], fingerprints[1], sizeof(fingerprints[0])) == 0 &&
        memcmp(fingerprints[1], fingerprints[2], sizeof(fingerprints[1])) == 0);
  static const char inferred_source[] =
      "export const ultimateAnswer = 6 * 7\n"
      "const fn isUltimateAnswer(value: i64): Bool { return value == 42 }\n"
      "struct UltimateAnswer<_ value: i64<(isUltimateAnswer(.member))>> {}\n"
      "struct Use {\n"
      "  immediate: UltimateAnswer<42>\n"
      "  computed: UltimateAnswer<(6 * 7)>\n"
      "  named: UltimateAnswer<(ultimateAnswer)>\n"
      "}\n";
  CHECK(fixture_lower(&value, inferred_source));
  CHECK(value.frontend_result.status == W_SEED_FRONTEND_OK &&
        value.frontend_result.written.const_declarations == 1u &&
        !value.const_declarations[0].has_explicit_type &&
        value.const_declarations[0].declared_type == W_SEED_FRONTEND_NONE &&
        value.const_declarations[0].effective_type != W_SEED_FRONTEND_NONE);
  w_seed_generic_validation_result inferred_result;
  CHECK(validate_application_at(
            &value, 2u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &inferred_result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        inferred_result.computed_argument_count == 1u &&
        inferred_result.receipts_written == 2u &&
        memcmp(fingerprints[2], inferred_result.fingerprint_digest,
               sizeof(fingerprints[2])) == 0 &&
        value.receipts[0].effective_type ==
            value.receipts[0].eval_value.type_index);
  static const char forward_source[] =
      "const answer = target + 1\n"
      "const target = 41\n"
      "struct Box<_ value: i64> {}\n"
      "struct Use { value: Box<(answer)> }\n";
  static const char ordered_source[] =
      "const target = 41\n"
      "const answer = target + 1\n"
      "struct Box<_ value: i64> {}\n"
      "struct Use { value: Box<(answer)> }\n";
  uint8_t forward_digest[W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES];
  CHECK(fixture_lower(&value, forward_source));
  CHECK(value.frontend_result.status == W_SEED_FRONTEND_OK &&
        value.const_declarations[0].effective_type != W_SEED_FRONTEND_NONE &&
        value.const_declarations[1].effective_type != W_SEED_FRONTEND_NONE);
  w_seed_generic_validation_result forward_result;
  const w_seed_generic_validation_state forward_state = validate_application_at(
      &value, 0u, CONVERSION_VALUES,
      (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &forward_result);
  CHECK(forward_state == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        forward_result.computed_argument_count == 1u &&
        forward_result.receipts_written == 1u &&
        forward_result.evaluation.const_cache_misses == 2u &&
        forward_result.evaluation.const_cache_hits == 0u &&
        value.receipts[0].eval_value.integer_value[0] == 42u);
  (void)memcpy(forward_digest, forward_result.fingerprint_digest,
               sizeof(forward_digest));
  CHECK(fixture_lower(&value, ordered_source));
  w_seed_generic_validation_result ordered_result;
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &ordered_result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        ordered_result.computed_argument_count == 1u &&
        ordered_result.receipts_written == 1u &&
        memcmp(forward_digest, ordered_result.fingerprint_digest,
               sizeof(forward_digest)) == 0 &&
        value.receipts[0].eval_value.integer_value[0] == 42u);

  static const char bool_cycle_source[] =
      "const left = right\n"
      "const right = left == true\n"
      "struct Box<_ value: Bool> {}\n"
      "struct Use { cycle: Box<(left)> independent: Box<(true)> }\n";
  CHECK(fixture_lower(&value, bool_cycle_source));
  CHECK(value.const_declarations[0].declared_type == W_SEED_FRONTEND_NONE &&
        value.const_declarations[1].declared_type == W_SEED_FRONTEND_NONE &&
        value.const_declarations[0].effective_type != W_SEED_FRONTEND_NONE &&
        value.types[value.const_declarations[0].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_BOOL &&
        value.types[value.const_declarations[1].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_BOOL);
  w_seed_generic_validation_result bool_cycle_result;
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &bool_cycle_result) ==
            W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED &&
        bool_cycle_result.diagnostic ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0002 &&
        bool_cycle_result.evaluation.consumed_steps == 0u &&
        bool_cycle_result.evaluation.const_cache_hits == 0u &&
        bool_cycle_result.evaluation.const_cache_misses == 0u &&
        bool_cycle_result.computed_argument_count == 1u &&
        bool_cycle_result.receipts_written == 1u &&
        bool_cycle_result.const_cycle_path_length == 3u &&
        bool_cycle_result.const_cycle_path[0] == 0u &&
        bool_cycle_result.const_cycle_path[1] == 1u &&
        bool_cycle_result.const_cycle_path[2] == 0u &&
        value.receipts[0].kind ==
            W_SEED_GENERIC_VALIDATION_RECEIPT_CONST_ARGUMENT &&
        value.types[value.receipts[0].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_BOOL &&
        fingerprint_not_available(&bool_cycle_result));

  static const char integer_cycle_source[] =
      "const left = right\n"
      "const right = left + 1\n"
      "struct Box<_ value: i64> {}\n"
      "struct Use { cycle: Box<(left)> independent: Box<(6 * 7)> }\n";
  CHECK(fixture_lower(&value, integer_cycle_source));
  CHECK(value.const_declarations[0].effective_type != W_SEED_FRONTEND_NONE &&
        value.types[value.const_declarations[0].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_INTEGER &&
        value.types[value.const_declarations[0].effective_type].bit_width ==
            64u &&
        value.types[value.const_declarations[1].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_INTEGER &&
        value.types[value.const_declarations[1].effective_type].bit_width ==
            64u);
  w_seed_generic_validation_result integer_cycle_result;
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &integer_cycle_result) ==
            W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED &&
        integer_cycle_result.diagnostic ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0002 &&
        integer_cycle_result.evaluation.consumed_steps == 0u &&
        integer_cycle_result.evaluation.const_cache_hits == 0u &&
        integer_cycle_result.evaluation.const_cache_misses == 0u &&
        integer_cycle_result.computed_argument_count == 1u &&
        integer_cycle_result.receipts_written == 1u &&
        integer_cycle_result.const_cycle_path_length == 3u &&
        integer_cycle_result.const_cycle_path[0] == 0u &&
        integer_cycle_result.const_cycle_path[1] == 1u &&
        integer_cycle_result.const_cycle_path[2] == 0u &&
        value.types[value.receipts[0].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_INTEGER &&
        value.types[value.receipts[0].effective_type].bit_width == 64u &&
        fingerprint_not_available(&integer_cycle_result));

  /* Graph-first precedence also covers a cycle whose two operators impose
   * incompatible scalar constraints.  Frontend inference leaves the logical
   * declaration untyped, but the reachable cycle still owns W-CONST-0002. */
  static const char incompatible_cycle_source[] =
      "const left = right && true\n"
      "const right = left + 1\n"
      "struct Box<_ value: Bool> {}\n"
      "struct Use { cycle: Box<(left)> }\n";
  CHECK(fixture_lower(&value, incompatible_cycle_source));
  CHECK(value.frontend_result.status == W_SEED_FRONTEND_UNSUPPORTED &&
        value.const_declarations[0].effective_type == W_SEED_FRONTEND_NONE &&
        value.const_declarations[1].effective_type != W_SEED_FRONTEND_NONE &&
        value.types[value.const_declarations[1].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_INTEGER &&
        value.types[value.const_declarations[1].effective_type].bit_width ==
            64u);
  w_seed_generic_validation_result incompatible_cycle_result;
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &incompatible_cycle_result) ==
            W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED &&
        incompatible_cycle_result.diagnostic ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0002 &&
        incompatible_cycle_result.evaluation.consumed_steps == 0u &&
        incompatible_cycle_result.evaluation.const_cache_hits == 0u &&
        incompatible_cycle_result.evaluation.const_cache_misses == 0u &&
        incompatible_cycle_result.computed_argument_count == 1u &&
        incompatible_cycle_result.receipts_written == 1u &&
        incompatible_cycle_result.const_cycle_path_length == 3u &&
        incompatible_cycle_result.const_cycle_path[0] == 0u &&
        incompatible_cycle_result.const_cycle_path[1] == 1u &&
        incompatible_cycle_result.const_cycle_path[2] == 0u &&
        value.receipts[0].kind ==
            W_SEED_GENERIC_VALIDATION_RECEIPT_CONST_ARGUMENT &&
        value.types[value.receipts[0].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_BOOL &&
        fingerprint_not_available(&incompatible_cycle_result));

  /* The causal graph does not outrank a malformed application relation. */
  w_seed_frontend_generic_argument *incompatible_argument =
      &value.generic_arguments[value.generic_applications[0].first_argument];
  const uint32_t saved_argument_owner = incompatible_argument->owner_application;
  incompatible_argument->owner_application = W_SEED_FRONTEND_NONE;
  w_seed_generic_validation_result incompatible_application_corruption;
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &incompatible_application_corruption) ==
            W_SEED_GENERIC_VALIDATION_INVALID &&
        incompatible_application_corruption.computed_argument_count == 0u &&
        incompatible_application_corruption.receipts_written == 0u &&
        incompatible_application_corruption.evaluation.consumed_steps == 0u &&
        incompatible_application_corruption.evaluation.const_cache_hits == 0u &&
        incompatible_application_corruption.evaluation.const_cache_misses == 0u &&
        fingerprint_not_available(&incompatible_application_corruption));
  incompatible_argument->owner_application = saved_argument_owner;

  /* A broken child edge is invalid before the cycle diagnostic can be
   * manufactured from the remaining source-backed records. */
  const uint32_t incompatible_root = value.const_declarations[0].initializer_expression;
  CHECK(incompatible_root != W_SEED_FRONTEND_NONE);
  w_seed_frontend_expression *incompatible_expression =
      &value.expressions[incompatible_root];
  const uint32_t saved_incompatible_left = incompatible_expression->left;
  incompatible_expression->left = W_SEED_FRONTEND_NONE;
  w_seed_generic_validation_result incompatible_dependency_corruption;
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &incompatible_dependency_corruption) ==
            W_SEED_GENERIC_VALIDATION_INVALID &&
        incompatible_dependency_corruption.computed_argument_count == 0u &&
        incompatible_dependency_corruption.receipts_written == 0u &&
        incompatible_dependency_corruption.evaluation.consumed_steps == 0u &&
        incompatible_dependency_corruption.evaluation.const_cache_hits == 0u &&
        incompatible_dependency_corruption.evaluation.const_cache_misses == 0u &&
        fingerprint_not_available(&incompatible_dependency_corruption));
  incompatible_expression->left = saved_incompatible_left;

  /* Count every recovered calculated slot before the first causal cycle.
   * Receipt publication remains one causal record, even when two slots point
   * at the same source-backed cycle. */
  static const char incompatible_multi_slot_source[] =
      "const left = right && true\n"
      "const right = left + 1\n"
      "struct Box<_ first: Bool, _ second: Bool> {}\n"
      "struct Use { cycle: Box<(left), (left)> }\n";
  CHECK(fixture_lower(&value, incompatible_multi_slot_source));
  CHECK(value.frontend_result.written.generic_applications == 1u &&
        value.generic_applications[0].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_UNSUPPORTED &&
        value.generic_arguments[0].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_UNSUPPORTED &&
        value.generic_arguments[1].binding_status ==
            W_SEED_FRONTEND_GENERIC_BINDING_UNSUPPORTED);
  w_seed_generic_validation_result incompatible_multi_slot_result;
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &incompatible_multi_slot_result) ==
            W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED &&
        incompatible_multi_slot_result.computed_argument_count == 2u &&
        incompatible_multi_slot_result.receipts_written == 1u &&
        incompatible_multi_slot_result.diagnostic ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0002 &&
        incompatible_multi_slot_result.const_cycle_path_length == 3u &&
        incompatible_multi_slot_result.const_cycle_path[0] == 0u &&
        incompatible_multi_slot_result.const_cycle_path[1] == 1u &&
        incompatible_multi_slot_result.const_cycle_path[2] == 0u &&
        incompatible_multi_slot_result.evaluation.consumed_steps == 0u &&
        incompatible_multi_slot_result.evaluation.const_cache_hits == 0u &&
        incompatible_multi_slot_result.evaluation.const_cache_misses == 0u &&
        value.receipts[0].generic_argument_index == 0u &&
        value.types[value.receipts[0].effective_type].kind ==
            W_SEED_FRONTEND_TYPE_BOOL &&
        fingerprint_not_available(&incompatible_multi_slot_result));
  w_seed_generic_validation_result incompatible_multi_slot_zero_receipts;
  CHECK(validate_application_at_with_capacities(
            &value, 0u, CONVERSION_VALUES, value.evidence_bytes,
            W_SEED_GENERIC_VALIDATION_MAX_EVIDENCE_BYTES, 0u,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &incompatible_multi_slot_zero_receipts) ==
            W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED &&
        incompatible_multi_slot_zero_receipts.computed_argument_count == 2u &&
        incompatible_multi_slot_zero_receipts.receipts_written == 0u &&
        incompatible_multi_slot_zero_receipts.diagnostic ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0002 &&
        incompatible_multi_slot_zero_receipts.const_cycle_path_length == 3u &&
        incompatible_multi_slot_zero_receipts.const_cycle_path[0] == 0u &&
        incompatible_multi_slot_zero_receipts.const_cycle_path[1] == 1u &&
        incompatible_multi_slot_zero_receipts.const_cycle_path[2] == 0u &&
        incompatible_multi_slot_zero_receipts.evaluation.consumed_steps == 0u &&
        incompatible_multi_slot_zero_receipts.evaluation.const_cache_hits == 0u &&
        incompatible_multi_slot_zero_receipts.evaluation.const_cache_misses == 0u &&
        fingerprint_not_available(&incompatible_multi_slot_zero_receipts));

  /* Invalid application or argument barriers never manufacture W-CONST-0002. */
  CHECK(fixture_lower(&value, incompatible_multi_slot_source));
  value.generic_applications[0].binding_status =
      W_SEED_FRONTEND_GENERIC_BINDING_INVALID;
  w_seed_generic_validation_result incompatible_invalid_application;
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &incompatible_invalid_application) ==
            W_SEED_GENERIC_VALIDATION_INVALID &&
        incompatible_invalid_application.computed_argument_count == 0u &&
        incompatible_invalid_application.receipts_written == 0u &&
        incompatible_invalid_application.evaluation.consumed_steps == 0u &&
        incompatible_invalid_application.evaluation.const_cache_hits == 0u &&
        incompatible_invalid_application.evaluation.const_cache_misses == 0u &&
        fingerprint_not_available(&incompatible_invalid_application));
  CHECK(fixture_lower(&value, incompatible_multi_slot_source));
  value.generic_arguments[0].binding_status =
      W_SEED_FRONTEND_GENERIC_BINDING_INVALID;
  w_seed_generic_validation_result incompatible_invalid_argument;
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &incompatible_invalid_argument) ==
            W_SEED_GENERIC_VALIDATION_INVALID &&
        incompatible_invalid_argument.computed_argument_count == 0u &&
        incompatible_invalid_argument.receipts_written == 0u &&
        incompatible_invalid_argument.evaluation.consumed_steps == 0u &&
        incompatible_invalid_argument.evaluation.const_cache_hits == 0u &&
        incompatible_invalid_argument.evaluation.const_cache_misses == 0u &&
        fingerprint_not_available(&incompatible_invalid_argument));

  CHECK(fixture_lower(&value, named_source));
  const w_seed_frontend_generic_argument *named_argument =
      &value.generic_arguments[value.generic_applications[2].first_argument];
  CHECK(named_argument->typed_const_expression_index == 1u);
  w_seed_constir_function *named_function = &value.constir_functions[3];
  CHECK(named_function->root_node != W_SEED_CONSTIR_NONE);
  const w_seed_constir_node *named_node =
      &value.constir_nodes[named_function->root_node];
  CHECK(named_node->kind == W_SEED_CONSTIR_NODE_CALL &&
        named_node->call_target_function == W_SEED_CONSTIR_NONE &&
        named_node->call_target_const_declaration == 0u);

  /* D4 relation corruption is INVALID before graph traversal, even when the
   * caller later supplies no conversion or receipt capacity. */
  const uint32_t named_root = named_function->root_node;
  const uint32_t named_target = value.constir_nodes[named_root]
                                    .call_target_const_declaration;
  value.constir_nodes[named_root].call_target_const_declaration = 1u;
  w_seed_generic_validation_result corruption_result;
  CHECK(validate_application_at(
            &value, 2u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &corruption_result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        corruption_result.evaluation.consumed_steps == 0u &&
        corruption_result.evaluation.const_cache_hits == 0u &&
        corruption_result.evaluation.const_cache_misses == 0u &&
        corruption_result.receipts_written == 0u &&
        fingerprint_not_available(&corruption_result));
  value.constir_nodes[named_root].call_target_const_declaration = named_target;
  const uint32_t named_expression = value.constir_nodes[named_root].frontend_expression;
  const uint32_t named_relation =
      value.expressions[named_expression].resolved_const_declaration;
  value.expressions[named_expression].resolved_const_declaration = 1u;
  CHECK(validate_application_at(
            &value, 2u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &corruption_result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        corruption_result.evaluation.consumed_steps == 0u &&
        corruption_result.evaluation.const_cache_hits == 0u &&
        corruption_result.evaluation.const_cache_misses == 0u &&
        fingerprint_not_available(&corruption_result));
  value.expressions[named_expression].resolved_const_declaration = named_relation;
  const w_seed_constir_function_origin named_origin = named_function->origin;
  named_function->origin = W_SEED_CONSTIR_FUNCTION_ORIGIN_FRONTEND_FUNCTION;
  CHECK(validate_application_at(
            &value, 2u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &corruption_result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        corruption_result.evaluation.consumed_steps == 0u &&
        corruption_result.evaluation.const_cache_hits == 0u &&
        corruption_result.evaluation.const_cache_misses == 0u &&
        fingerprint_not_available(&corruption_result));
  named_function->origin = named_origin;
  const uint32_t named_type = value.const_declarations[0].declared_type;
  value.const_declarations[0].declared_type = W_SEED_FRONTEND_NONE;
  CHECK(validate_application_at(
            &value, 2u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &corruption_result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        corruption_result.evaluation.consumed_steps == 0u &&
        corruption_result.evaluation.const_cache_hits == 0u &&
        corruption_result.evaluation.const_cache_misses == 0u &&
        fingerprint_not_available(&corruption_result));
  value.const_declarations[0].declared_type = named_type;
  const uint32_t named_head = value.generic_applications[2].head_struct;
  value.generic_applications[2].head_struct = W_SEED_FRONTEND_NONE;
  CHECK(validate_application_at(
            &value, 2u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &corruption_result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        corruption_result.evaluation.consumed_steps == 0u &&
        corruption_result.evaluation.const_cache_hits == 0u &&
        corruption_result.evaluation.const_cache_misses == 0u &&
        fingerprint_not_available(&corruption_result));
  value.generic_applications[2].head_struct = named_head;

  static const char cycle_source[] =
      "const left = right\n"
      "const right = left\n"
      "struct Box<_ value: i64> {}\n"
      "struct Use { cycle: Box<(left)> independent: Box<(6 * 7)> }\n";
  CHECK(fixture_lower(&value, cycle_source));
  CHECK(value.frontend_result.written.const_declarations == 2u &&
        value.frontend_result.written.generic_applications == 2u);
  CHECK(value.constir_result.written.functions == 4u);
  w_seed_generic_validation_result cycle_result;
  const w_seed_generic_validation_state cycle_state = validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &cycle_result);
  CHECK(cycle_state == W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED);
  CHECK(cycle_result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0002 &&
        cycle_result.failure ==
            W_SEED_GENERIC_VALIDATION_FAILURE_EVALUATOR_DIAGNOSTIC &&
        cycle_result.evaluation.consumed_steps == 0u &&
        cycle_result.evaluation.const_cache_hits == 0u &&
        cycle_result.evaluation.const_cache_misses == 0u &&
        cycle_result.computed_argument_count == 1u &&
        cycle_result.receipts_written == 1u &&
        cycle_result.const_cycle_path_length == 3u &&
        cycle_result.const_cycle_path[0] == 0u &&
        cycle_result.const_cycle_path[1] == 1u &&
        cycle_result.const_cycle_path[2] == 0u &&
        value.receipts[0].kind ==
            W_SEED_GENERIC_VALIDATION_RECEIPT_CONST_ARGUMENT &&
        value.receipts[0].evaluation.diagnostic ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0002 &&
        value.receipts[0].evaluation.const_cache_hits == 0u &&
        value.receipts[0].evaluation.const_cache_misses == 0u);
  static const char anchored_cycle_source[] =
      "const left: i64 = right\n"
      "const right: i64 = left\n"
      "struct Box<_ value: i64> {}\n"
      "struct Use { cycle: Box<(left)> independent: Box<(6 * 7)> }\n";
  CHECK(fixture_lower(&value, anchored_cycle_source));
  w_seed_generic_validation_result anchored_cycle_result;
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &anchored_cycle_result) ==
            W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED &&
        anchored_cycle_result.diagnostic ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0002 &&
        anchored_cycle_result.evaluation.consumed_steps == 0u &&
        anchored_cycle_result.evaluation.const_cache_hits == 0u &&
        anchored_cycle_result.evaluation.const_cache_misses == 0u &&
        anchored_cycle_result.receipts_written == 1u &&
        fingerprint_not_available(&anchored_cycle_result));
  static const char self_cycle_source[] =
      "const self = self\n"
      "struct Box<_ value: i64> {}\n"
      "struct Use { cycle: Box<(self)> independent: Box<(6 * 7)> }\n";
  CHECK(fixture_lower(&value, self_cycle_source));
  w_seed_generic_validation_result self_cycle_result;
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &self_cycle_result) == W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED &&
        self_cycle_result.diagnostic ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0002 &&
        self_cycle_result.evaluation.consumed_steps == 0u &&
        self_cycle_result.evaluation.const_cache_hits == 0u &&
        self_cycle_result.evaluation.const_cache_misses == 0u &&
        self_cycle_result.receipts_written == 1u &&
        self_cycle_result.const_cycle_path_length == 2u &&
        self_cycle_result.const_cycle_path[0] == 0u &&
        self_cycle_result.const_cycle_path[1] == 0u &&
        value.receipts[0].kind ==
            W_SEED_GENERIC_VALIDATION_RECEIPT_CONST_ARGUMENT &&
        value.receipts[0].evaluation.const_cache_hits == 0u &&
        value.receipts[0].evaluation.const_cache_misses == 0u);
  w_seed_generic_validation_result independent_result;
  CHECK(validate_application_at(
            &value, 1u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &independent_result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        independent_result.computed_argument_count == 1u &&
        independent_result.fingerprint_state ==
            W_SEED_GENERIC_VALIDATION_FINGERPRINT_AVAILABLE);

  static const char three_cycle_source[] =
      "const first = second\n"
      "const second = third\n"
      "const third = first\n"
      "struct Box<_ value: i64> {}\n"
      "struct Use { cycle: Box<(first)> independent: Box<(6 * 7)> }\n";
  CHECK(fixture_lower(&value, three_cycle_source));
  w_seed_generic_validation_result three_cycle_result;
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &three_cycle_result) == W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED &&
        three_cycle_result.diagnostic ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0002 &&
        three_cycle_result.const_cycle_path_length == 4u &&
        three_cycle_result.const_cycle_path[0] == 0u &&
        three_cycle_result.const_cycle_path[1] == 1u &&
        three_cycle_result.const_cycle_path[2] == 2u &&
        three_cycle_result.const_cycle_path[3] == 0u &&
        three_cycle_result.evaluation.consumed_steps == 0u &&
        three_cycle_result.evaluation.const_cache_hits == 0u &&
        three_cycle_result.evaluation.const_cache_misses == 0u &&
        three_cycle_result.receipts_written == 1u &&
        value.receipts[0].kind ==
            W_SEED_GENERIC_VALIDATION_RECEIPT_CONST_ARGUMENT &&
        value.receipts[0].evaluation.const_cache_hits == 0u &&
        value.receipts[0].evaluation.const_cache_misses == 0u);
  CHECK(validate_application_at(
            &value, 1u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &independent_result) == W_SEED_GENERIC_VALIDATION_VERIFIED);

  static const char false_const_source[] =
      "const no: Bool = false\n"
      "const fn isTrue(value: Bool): Bool { return value }\n"
      "struct Box<_ value: Bool<(isTrue(.member))>> {}\n"
      "struct Use { rejected: Box<(no)> }\n";
  CHECK(fixture_lower(&value, false_const_source));
  w_seed_generic_validation_result false_result;
  CHECK(validate_application(
            &value, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &false_result) ==
            W_SEED_GENERIC_VALIDATION_REJECTED &&
        false_result.computed_argument_count == 1u &&
        false_result.receipts_written == 2u &&
        false_result.diagnostic == W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0004 &&
        false_result.evaluation.consumed_steps != 0u &&
        fingerprint_not_available(&false_result));

  static const char duplicate_const_source[] =
      "const duplicate: i64 = 42\n"
      "const duplicate: i64 = 42\n"
      "struct Box<_ value: i64> {}\n"
      "struct Use { duplicate: Box<(duplicate)> }\n";
  CHECK(fixture_lower(&value, duplicate_const_source));
  CHECK(value.frontend_result.status == W_SEED_FRONTEND_UNSUPPORTED);
  w_seed_generic_validation_result duplicate_result;
  CHECK(validate_application(
            &value, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &duplicate_result) == W_SEED_GENERIC_VALIDATION_UNSUPPORTED &&
        duplicate_result.evaluation.consumed_steps == 0u &&
        duplicate_result.receipts_written == 0u &&
        fingerprint_not_available(&duplicate_result));

  static const char unresolved_d4_source[] =
      "const anchor: i64 = 42\n"
      "struct Box<_ value: i64> {}\n"
      "struct Use { missing: Box<(missing)> }\n";
  CHECK(fixture_lower(&value, unresolved_d4_source));
  CHECK(value.frontend_result.status == W_SEED_FRONTEND_DIAGNOSTICS &&
        value.frontend_result.written.diagnostics != 0u);
  (void)memset(value.conversion_values, 0xa5, sizeof(value.conversion_values));
  (void)memset(value.receipts, 0xa5, sizeof(value.receipts));
  w_seed_generic_validation_result unresolved_result;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &unresolved_result) ==
            W_SEED_GENERIC_VALIDATION_INVALID &&
        unresolved_result.evaluation.consumed_steps == 0u &&
        unresolved_result.receipts_written == 0u &&
        fingerprint_not_available(&unresolved_result));
  for (size_t byte = 0u; byte < sizeof(value.conversion_values); byte += 1u)
    CHECK(((const uint8_t *)value.conversion_values)[byte] == 0xa5u);
  for (size_t byte = 0u; byte < sizeof(value.receipts); byte += 1u)
    CHECK(((const uint8_t *)value.receipts)[byte] == 0xa5u);

  static const char quantity_d4_source[] =
      "const duration: PhysicalDuration = 10<si.s>\n"
      "struct Box<_ value: i64> {}\n"
      "struct Use { unsupported: Box<(duration)> }\n";
  CHECK(fixture_lower(&value, quantity_d4_source));
  CHECK(value.frontend_result.status == W_SEED_FRONTEND_UNSUPPORTED);
  w_seed_generic_validation_result quantity_result;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &quantity_result) ==
            W_SEED_GENERIC_VALIDATION_UNSUPPORTED &&
        quantity_result.evaluation.consumed_steps == 0u &&
        quantity_result.receipts_written == 0u &&
        fingerprint_not_available(&quantity_result));

  static const char size_d4_source[] =
      "const sizeValue: usize = 1<iec.MiB>\n"
      "struct Box<_ value: i64> {}\n"
      "struct Use { unsupported: Box<(sizeValue)> }\n";
  CHECK(fixture_lower(&value, size_d4_source));
  CHECK(value.frontend_result.status == W_SEED_FRONTEND_UNSUPPORTED);
  w_seed_generic_validation_result size_result;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &size_result) ==
            W_SEED_GENERIC_VALIDATION_UNSUPPORTED &&
        size_result.evaluation.consumed_steps == 0u &&
        size_result.receipts_written == 0u &&
        fingerprint_not_available(&size_result));

  static const char named_arithmetic_overflow_source[] =
      "const overflowValue: i8 = 127 + 1\n"
      "const fn isUltimateAnswer(value: i8): Bool { return value == 42 }\n"
      "struct UltimateAnswer<_ value: i8<(isUltimateAnswer(.member))>> {}\n"
      "struct Use { overflow: UltimateAnswer<(overflowValue)> }\n";
  CHECK(fixture_lower(&value, named_arithmetic_overflow_source));
  CHECK(value.frontend_result.status == W_SEED_FRONTEND_OK &&
        value.frontend_result.written.const_declarations == 1u);
  w_seed_generic_validation_result named_arithmetic_overflow_result;
  CHECK(validate_application(
            &value, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &named_arithmetic_overflow_result) ==
            W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED &&
        named_arithmetic_overflow_result.failure ==
            W_SEED_GENERIC_VALIDATION_FAILURE_EVALUATOR_DIAGNOSTIC &&
        named_arithmetic_overflow_result.diagnostic ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0006 &&
        named_arithmetic_overflow_result.evaluation.diagnostic ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0006 &&
        named_arithmetic_overflow_result.computed_argument_count == 1u &&
        named_arithmetic_overflow_result.predicate_count == 1u &&
        named_arithmetic_overflow_result.evaluation.consumed_steps != 0u &&
        named_arithmetic_overflow_result.receipts_written == 1u &&
        value.receipts[0].kind ==
            W_SEED_GENERIC_VALIDATION_RECEIPT_CONST_ARGUMENT &&
        value.receipts[0].evaluation.diagnostic ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0006 &&
        value.receipts[0].eval_value.kind == W_SEED_CONSTIR_VALUE_INVALID &&
        fingerprint_not_available(&named_arithmetic_overflow_result));

  CHECK(fixture_lower(&value, cycle_source));
  (void)memset(value.conversion_values, 0xa5, sizeof(value.conversion_values));
  (void)memset(value.receipts, 0xa5, sizeof(value.receipts));
  w_seed_generic_validation_result zero_capacity_cycle;
  CHECK(validate_application_at_with_capacities(
            &value, 0u, 0u, value.evidence_bytes,
            W_SEED_GENERIC_VALIDATION_MAX_EVIDENCE_BYTES, 0u,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &zero_capacity_cycle) ==
        W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED &&
        zero_capacity_cycle.diagnostic ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0002 &&
        zero_capacity_cycle.receipts_written == 0u &&
        zero_capacity_cycle.evaluation.consumed_steps == 0u &&
        zero_capacity_cycle.evaluation.const_cache_hits == 0u &&
        zero_capacity_cycle.evaluation.const_cache_misses == 0u);
  for (size_t byte = 0u; byte < sizeof(value.conversion_values); byte += 1u)
    CHECK(((const uint8_t *)value.conversion_values)[byte] == 0xa5u);
  for (size_t byte = 0u; byte < sizeof(value.receipts); byte += 1u)
    CHECK(((const uint8_t *)value.receipts)[byte] == 0xa5u);

  static const char unsupported_source[] =
      "const fn helper(_ value: i64): i64 { return value }\n"
      "const unsupported: String = \"42\"\n"
      "const call: i64 = helper(6)\n"
      "struct Box<_ value: i64> {}\n"
      "struct Use { field: Box<(unsupported)> }\n";
  CHECK(fixture_lower(&value, unsupported_source));
  CHECK(value.frontend_result.written.const_declarations == 2u &&
        value.frontend_result.status == W_SEED_FRONTEND_UNSUPPORTED);
  w_seed_generic_validation_result unsupported_result;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &unsupported_result) ==
            W_SEED_GENERIC_VALIDATION_UNSUPPORTED &&
         unsupported_result.evaluation.consumed_steps == 0u &&
         unsupported_result.receipts_written == 0u &&
         fingerprint_not_available(&unsupported_result));

  /* The graph ceiling counts unique source const dependencies, not recursive
   * expression depth.  A 257-member chain is well formed but outside D4 and
   * must stop before conversion, receipts, quota, or evaluator steps. */
  char dependency_limit_source[SOURCE_BYTES];
  size_t dependency_limit_length = 0u;
  for (size_t index = 0u; index < 257u; index += 1u) {
    const int written = snprintf(
        dependency_limit_source + dependency_limit_length,
        sizeof(dependency_limit_source) - dependency_limit_length,
        "const c%llu: i64 = %s\n",
        (unsigned long long)index, index + 1u < 257u ? "cNEXT" : "42");
    CHECK(written > 0 &&
          (size_t)written < sizeof(dependency_limit_source) - dependency_limit_length);
    const size_t name_start = dependency_limit_length + (size_t)written - 6u;
    if (index + 1u < 257u) {
      const int next_written = snprintf(
          dependency_limit_source + name_start,
          sizeof(dependency_limit_source) - name_start,
          "c%llu\n", (unsigned long long)(index + 1u));
      CHECK(next_written > 0 &&
            (size_t)next_written < sizeof(dependency_limit_source) - name_start);
      dependency_limit_length = name_start + (size_t)next_written;
    } else {
      dependency_limit_length += (size_t)written;
    }
  }
  const int dependency_limit_tail = snprintf(
      dependency_limit_source + dependency_limit_length,
      sizeof(dependency_limit_source) - dependency_limit_length,
      "struct Box<_ value: i64> {}\nstruct Use { chain: Box<(c0)> }\n");
  CHECK(dependency_limit_tail > 0 &&
        (size_t)dependency_limit_tail <
            sizeof(dependency_limit_source) - dependency_limit_length);
  dependency_limit_length += (size_t)dependency_limit_tail;
  (void)dependency_limit_length;
  CHECK(fixture_lower(&value, dependency_limit_source));
  CHECK(value.frontend_result.written.const_declarations == 257u &&
        value.constir_result.written.functions == 258u);
  w_seed_generic_validation_result dependency_limit_result;
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &dependency_limit_result) ==
        W_SEED_GENERIC_VALIDATION_UNSUPPORTED &&
        dependency_limit_result.computed_argument_count == 1u &&
        dependency_limit_result.failure ==
            W_SEED_GENERIC_VALIDATION_FAILURE_DEPENDENCY_LIMIT &&
        dependency_limit_result.evaluation.consumed_steps == 0u &&
        dependency_limit_result.evaluation.const_cache_hits == 0u &&
        dependency_limit_result.evaluation.const_cache_misses == 0u &&
        dependency_limit_result.receipts_written == 0u &&
        fingerprint_not_available(&dependency_limit_result));

  /* D5 memoizes the shared seed in a local diamond.  The root CALL and every
   * declaration body still consume their ordinary node steps. */
  static const char diamond_source[] =
      "const answerSeed = 21\n"
      "const firstAnswerHalf = answerSeed\n"
      "const secondAnswerHalf = answerSeed\n"
      "export const assembledUltimateAnswer = firstAnswerHalf + secondAnswerHalf\n"
      "const fn isUltimateAnswer(value: i64): Bool { return value == 42 }\n"
      "struct UltimateAnswer<_ value: i64<(isUltimateAnswer(.member))>> {}\n"
      "struct Use { shared: UltimateAnswer<(assembledUltimateAnswer)> }\n";
  CHECK(fixture_lower(&value, diamond_source));
  CHECK(value.frontend_result.written.const_declarations == 4u);
  CHECK(value.frontend_result.written.generic_applications == 1u);
  w_seed_generic_validation_result diamond_result;
  CHECK(validate_application(
            &value, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &diamond_result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        diamond_result.computed_argument_count == 1u &&
        diamond_result.receipts_written == 2u &&
        value.receipts[0].evaluation.consumed_steps == 7u &&
        value.receipts[0].evaluation.const_cache_misses == 4u &&
        value.receipts[0].evaluation.const_cache_hits == 1u &&
        value.receipts[1].evaluation.const_cache_misses == 0u &&
        value.receipts[1].evaluation.const_cache_hits == 0u &&
        value.receipts[0].eval_value.kind ==
            W_SEED_CONSTIR_VALUE_INTEGER &&
        value.receipts[0].eval_value.integer_value[0] == 42u);
  uint8_t diamond_fingerprint[
      W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES] = {0};
  (void)memcpy(diamond_fingerprint, diamond_result.fingerprint_digest,
               sizeof(diamond_fingerprint));
  w_seed_generic_validation_result diamond_repeat;
  CHECK(validate_application(
            &value, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &diamond_repeat) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        value.receipts[0].evaluation.consumed_steps == 7u &&
        value.receipts[0].evaluation.const_cache_misses == 4u &&
        value.receipts[0].evaluation.const_cache_hits == 1u &&
        memcmp(diamond_fingerprint, diamond_repeat.fingerprint_digest,
               sizeof(diamond_fingerprint)) == 0);

  static const char diamond_without_predicate[] =
      "const answerSeed = 21\n"
      "const firstAnswerHalf = answerSeed\n"
      "const secondAnswerHalf = answerSeed\n"
      "export const assembledUltimateAnswer = firstAnswerHalf + secondAnswerHalf\n"
      "struct Box<_ value: i64> {}\n"
      "struct Use { shared: Box<(assembledUltimateAnswer)> }\n";
  CHECK(fixture_lower(&value, diamond_without_predicate));
  w_seed_generic_validation_result diamond_quota_ok;
  CHECK(validate_application(
            &value, CONVERSION_VALUES,
            (w_seed_constir_quota){7u, 0u, 64u, SIZE_MAX},
            &diamond_quota_ok) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        diamond_quota_ok.receipts_written == 1u &&
        value.receipts[0].evaluation.consumed_steps == 7u &&
        value.receipts[0].evaluation.const_cache_misses == 4u &&
        value.receipts[0].evaluation.const_cache_hits == 1u);
  w_seed_generic_validation_result diamond_quota_fail;
  CHECK(validate_application(
            &value, CONVERSION_VALUES,
            (w_seed_constir_quota){6u, 0u, 64u, SIZE_MAX},
            &diamond_quota_fail) ==
            W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED &&
        diamond_quota_fail.diagnostic ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003 &&
        value.receipts[0].evaluation.consumed_steps == 6u &&
        value.receipts[0].evaluation.const_cache_misses == 4u &&
        value.receipts[0].evaluation.const_cache_hits == 0u);

  /* D6 keeps one private session for the calculated arguments of one
   * application.  The second sibling reuses the ready root declaration while
   * its own CALL node still consumes one step. */
  static const char sibling_source[] =
      "const answerSeed = 21\n"
      "const firstAnswerHalf = answerSeed\n"
      "const secondAnswerHalf = answerSeed\n"
      "export const assembledUltimateAnswer = firstAnswerHalf + secondAnswerHalf\n"
      "struct AnswerPair<_ left: i64, _ right: i64> {}\n"
      "struct Use { pair: AnswerPair<(assembledUltimateAnswer), (assembledUltimateAnswer)> }\n";
  CHECK(fixture_lower(&value, sibling_source));
  CHECK(value.frontend_result.written.generic_applications == 1u);
  w_seed_generic_validation_result sibling_result;
  CHECK(validate_application(
            &value, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &sibling_result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        sibling_result.computed_argument_count == 2u &&
        sibling_result.predicate_count == 0u &&
        sibling_result.receipts_written == 2u &&
        value.receipts[0].evaluation.consumed_steps == 7u &&
        value.receipts[0].evaluation.const_cache_misses == 4u &&
        value.receipts[0].evaluation.const_cache_hits == 1u &&
        value.receipts[1].evaluation.consumed_steps == 1u &&
        value.receipts[1].evaluation.const_cache_misses == 0u &&
        value.receipts[1].evaluation.const_cache_hits == 1u &&
        value.receipts[0].eval_value.kind == W_SEED_CONSTIR_VALUE_INTEGER &&
        value.receipts[1].eval_value.kind == W_SEED_CONSTIR_VALUE_INTEGER &&
        value.receipts[0].eval_value.integer_value[0] == 42u &&
        value.receipts[1].eval_value.integer_value[0] == 42u);

  /* Quota consumption remains aggregate, but a zero remaining quota fails at
   * the second expression root before its session lookup. */
  w_seed_generic_validation_result sibling_quota_result;
  CHECK(validate_application(
            &value, CONVERSION_VALUES,
            (w_seed_constir_quota){7u, 0u, 64u, SIZE_MAX},
            &sibling_quota_result) ==
            W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED &&
        sibling_quota_result.diagnostic ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0003 &&
        sibling_quota_result.receipts_written == 2u &&
        value.receipts[0].evaluation.consumed_steps == 7u &&
        value.receipts[0].evaluation.const_cache_misses == 4u &&
        value.receipts[0].evaluation.const_cache_hits == 1u &&
        value.receipts[1].evaluation.consumed_steps == 0u &&
        value.receipts[1].evaluation.const_cache_misses == 0u &&
        value.receipts[1].evaluation.const_cache_hits == 0u);

  /* A second application receives a new session, so its first argument is a
   * fresh diamond evaluation. */
  static const char two_application_source[] =
      "const answerSeed = 21\n"
      "const firstAnswerHalf = answerSeed\n"
      "const secondAnswerHalf = answerSeed\n"
      "export const assembledUltimateAnswer = firstAnswerHalf + secondAnswerHalf\n"
      "struct AnswerPair<_ left: i64, _ right: i64> {}\n"
      "struct Use { first: AnswerPair<(assembledUltimateAnswer), (assembledUltimateAnswer)> second: AnswerPair<(assembledUltimateAnswer), (assembledUltimateAnswer)> }\n";
  CHECK(fixture_lower(&value, two_application_source));
  CHECK(value.frontend_result.written.generic_applications == 2u);
  w_seed_generic_validation_result first_application_result;
  w_seed_generic_validation_result second_application_result;
  CHECK(validate_application_at(
            &value, 0u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &first_application_result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        value.receipts[0].evaluation.consumed_steps == 7u &&
        value.receipts[0].evaluation.const_cache_misses == 4u &&
        value.receipts[0].evaluation.const_cache_hits == 1u &&
        value.receipts[1].evaluation.consumed_steps == 1u &&
        value.receipts[1].evaluation.const_cache_misses == 0u &&
        value.receipts[1].evaluation.const_cache_hits == 1u);
  CHECK(validate_application_at(
            &value, 1u, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &second_application_result) == W_SEED_GENERIC_VALIDATION_VERIFIED &&
        value.receipts[0].evaluation.consumed_steps == 7u &&
        value.receipts[0].evaluation.const_cache_misses == 4u &&
        value.receipts[0].evaluation.const_cache_hits == 1u &&
        value.receipts[1].evaluation.consumed_steps == 1u &&
        value.receipts[1].evaluation.const_cache_misses == 0u &&
        value.receipts[1].evaluation.const_cache_hits == 1u);

  static const char failure_first_source[] =
      "const broken: i8 = 127 + 1\n"
      "const answerSeed: i64 = 21\n"
      "const firstAnswerHalf: i64 = answerSeed\n"
      "const secondAnswerHalf: i64 = answerSeed\n"
      "export const assembledUltimateAnswer: i64 = firstAnswerHalf + secondAnswerHalf\n"
      "struct AnswerPair<_ left: i8, _ right: i64> {}\n"
      "struct Use { pair: AnswerPair<(broken), (assembledUltimateAnswer)> }\n";
  CHECK(fixture_lower(&value, failure_first_source));
  CHECK(value.frontend_result.written.generic_applications == 1u);
  w_seed_generic_validation_result failure_first_result;
  CHECK(validate_application(
            &value, CONVERSION_VALUES,
            (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
            &failure_first_result) ==
            W_SEED_GENERIC_VALIDATION_EVALUATION_FAILED &&
        failure_first_result.diagnostic ==
            W_SEED_CONSTIR_DIAGNOSTIC_W_CONST_0006 &&
        failure_first_result.receipts_written == 1u &&
        value.receipts[0].evaluation.const_cache_misses == 1u &&
        value.receipts[0].evaluation.const_cache_hits == 0u);
  return true;
}

static bool test_specialization_contract(void) {
  static const char base_source[] =
      "const fn always(value: i64): Bool { return true }\n"
      "struct Box<value: i64<(always(.member))>> {}\n"
      "struct Use { item: Box<value: 42> }\n";
  static const char label_source[] =
      "const fn always(value: i64): Bool { return true }\n"
      "struct Box<value: i64<(always(.member))>> {}\n"
      "struct Use { item: Box<value: 42> }\n";
  static const char other_head_source[] =
      "const fn always(value: i64): Bool { return true }\n"
      "struct OtherBox<value: i64<(always(.member))>> {}\n"
      "struct Use { item: OtherBox<value: 42> }\n";
  static const char other_body_source[] =
      "const fn always(value: i64): Bool { return value == value }\n"
      "struct Box<value: i64<(always(.member))>> {}\n"
      "struct Use { item: Box<value: 42> }\n";
  static const char rejected_source[] =
      "const fn never(value: i64): Bool { return false }\n"
      "struct Box<value: i64<(never(.member))>> {}\n"
      "struct Use { item: Box<value: 42> }\n";
  uint8_t base_preimage[65536];
  w_seed_generic_validation_result base_result;
  CHECK(fixture_lower(&value, base_source));
  CHECK(validate_application_at_with_specialization_capacity(
            &value, 0u, base_preimage, sizeof(base_preimage), &base_result) ==
        W_SEED_GENERIC_VALIDATION_VERIFIED);
  CHECK(base_result.specialization_state ==
            W_SEED_GENERIC_VALIDATION_SPECIALIZATION_AVAILABLE &&
        base_result.specialization_bytes_written ==
            base_result.specialization_bytes_required &&
        base_result.specialization_bytes_required != 0u);
  const size_t required = base_result.specialization_bytes_required;
  uint8_t base_digest[W_SEED_GENERIC_VALIDATION_SPECIALIZATION_DIGEST_BYTES];
  (void)memcpy(base_digest, base_result.specialization_digest,
               sizeof(base_digest));

  uint8_t exact_preimage[65536];
  (void)memset(exact_preimage, 0xa5, sizeof(exact_preimage));
  w_seed_generic_validation_result exact_result;
  CHECK(fixture_lower(&value, base_source));
  CHECK(validate_application_at_with_specialization_capacity(
            &value, 0u, exact_preimage, required, &exact_result) ==
        W_SEED_GENERIC_VALIDATION_VERIFIED &&
        exact_result.specialization_state ==
            W_SEED_GENERIC_VALIDATION_SPECIALIZATION_AVAILABLE &&
        exact_result.specialization_bytes_written == required &&
        exact_result.specialization_bytes_required == required &&
        memcmp(exact_preimage, base_preimage, required) == 0 &&
        memcmp(exact_result.specialization_digest, base_digest,
               sizeof(base_digest)) == 0);

  uint8_t short_preimage[65536];
  (void)memset(short_preimage, 0xa5, sizeof(short_preimage));
  w_seed_generic_validation_result short_result;
  CHECK(fixture_lower(&value, base_source));
  CHECK(validate_application_at_with_specialization_capacity(
            &value, 0u, short_preimage, required - 1u, &short_result) ==
        W_SEED_GENERIC_VALIDATION_VERIFIED &&
        short_result.specialization_state ==
            W_SEED_GENERIC_VALIDATION_SPECIALIZATION_CAPACITY &&
        short_result.specialization_bytes_written == 0u &&
        short_result.specialization_bytes_required == required);
  for (size_t byte = 0u; byte < sizeof(short_preimage); byte += 1u)
    CHECK(short_preimage[byte] == 0xa5u);

  uint8_t zero_preimage[65536];
  (void)memset(zero_preimage, 0xa5, sizeof(zero_preimage));
  w_seed_generic_validation_result zero_result;
  CHECK(fixture_lower(&value, base_source));
  CHECK(validate_application_at_with_specialization_capacity(
            &value, 0u, zero_preimage, 0u, &zero_result) ==
        W_SEED_GENERIC_VALIDATION_VERIFIED &&
        zero_result.specialization_state ==
            W_SEED_GENERIC_VALIDATION_SPECIALIZATION_CAPACITY &&
        zero_result.specialization_bytes_written == 0u &&
        zero_result.specialization_bytes_required == required &&
        zero_preimage[0] == 0xa5u && zero_preimage[sizeof(zero_preimage) - 1u] ==
                                        0xa5u);

  w_seed_generic_validation_result null_storage_result;
  CHECK(fixture_lower(&value, base_source));
  CHECK(validate_application_at_with_specialization_capacity(
            &value, 0u, NULL, sizeof(base_preimage), &null_storage_result) ==
        W_SEED_GENERIC_VALIDATION_INVALID &&
        null_storage_result.failure ==
            W_SEED_GENERIC_VALIDATION_FAILURE_INVALID_INPUT &&
        null_storage_result.evaluation.consumed_steps == 0u &&
        null_storage_result.receipts_written == 0u &&
        fingerprint_not_available(&null_storage_result));

  w_seed_generic_specialization_view base_view = {
      base_preimage, required, base_digest};
  w_seed_generic_specialization_view exact_view = {
      exact_preimage, required, exact_result.specialization_digest};
  CHECK(w_seed_generic_specialization_equal(&base_view, &exact_view));
  const uint8_t zero_digest[
      W_SEED_GENERIC_VALIDATION_SPECIALIZATION_DIGEST_BYTES] = {0u};
  const w_seed_generic_specialization_view zero_view = {NULL, 0u, zero_digest};
  const w_seed_generic_specialization_view zero_view_two = {
      base_preimage, 0u, zero_digest};
  CHECK(!w_seed_generic_specialization_equal(&zero_view, &zero_view) &&
        !w_seed_generic_specialization_equal(&zero_view, &zero_view_two));
  w_seed_generic_specialization_view null_preimage_view = {
      NULL, required, base_digest};
  CHECK(!w_seed_generic_specialization_equal(&base_view, &null_preimage_view));
  w_seed_generic_specialization_view null_digest_view = {
      base_preimage, required, NULL};
  CHECK(!w_seed_generic_specialization_equal(&base_view, &null_digest_view));
  uint8_t forced_digest[W_SEED_GENERIC_VALIDATION_SPECIALIZATION_DIGEST_BYTES];
  (void)memcpy(forced_digest, base_digest, sizeof(forced_digest));
  exact_preimage[required - 1u] ^= 0x01u;
  exact_view.preimage = exact_preimage;
  exact_view.digest = forced_digest;
  CHECK(!w_seed_generic_specialization_equal(&base_view, &exact_view));
  forced_digest[0] ^= 0x01u;
  exact_preimage[required - 1u] = base_preimage[required - 1u];
  CHECK(!w_seed_generic_specialization_equal(&base_view, &exact_view));

  w_seed_generic_validation_result variant_result;
  CHECK(fixture_lower(&value, label_source));
  CHECK(validate_application_at_with_specialization_capacity(
            &value, 0u, value.specialization_preimage,
            sizeof(value.specialization_preimage), &variant_result) ==
        W_SEED_GENERIC_VALIDATION_VERIFIED &&
        variant_result.specialization_state ==
            W_SEED_GENERIC_VALIDATION_SPECIALIZATION_AVAILABLE &&
        variant_result.specialization_bytes_required == required &&
        memcmp(value.specialization_preimage, base_preimage, required) == 0);

  CHECK(fixture_lower(&value, other_head_source));
  CHECK(validate_application_at_with_specialization_capacity(
            &value, 0u, value.specialization_preimage,
            sizeof(value.specialization_preimage), &variant_result) ==
        W_SEED_GENERIC_VALIDATION_VERIFIED &&
        memcmp(variant_result.specialization_digest, base_digest,
               sizeof(base_digest)) != 0);
  CHECK(fixture_lower_with_module(&value, base_source, "other_module"));
  CHECK(validate_application_at_with_specialization_capacity(
            &value, 0u, value.specialization_preimage,
            sizeof(value.specialization_preimage), &variant_result) ==
        W_SEED_GENERIC_VALIDATION_VERIFIED &&
        memcmp(variant_result.specialization_digest, base_digest,
               sizeof(base_digest)) != 0);
  CHECK(fixture_lower(&value, other_body_source));
  CHECK(validate_application_at_with_specialization_capacity(
            &value, 0u, value.specialization_preimage,
            sizeof(value.specialization_preimage), &variant_result) ==
        W_SEED_GENERIC_VALIDATION_VERIFIED &&
        memcmp(variant_result.specialization_digest, base_digest,
               sizeof(base_digest)) != 0);

  CHECK(fixture_lower(&value, rejected_source));
  CHECK(validate_application_at_with_specialization_capacity(
            &value, 0u, value.specialization_preimage,
            sizeof(value.specialization_preimage), &variant_result) ==
        W_SEED_GENERIC_VALIDATION_REJECTED &&
        variant_result.specialization_state ==
            W_SEED_GENERIC_VALIDATION_SPECIALIZATION_NOT_AVAILABLE &&
        variant_result.specialization_bytes_written == 0u &&
        variant_result.specialization_bytes_required == 0u &&
        memcmp(variant_result.specialization_digest,
               (uint8_t[W_SEED_GENERIC_VALIDATION_SPECIALIZATION_DIGEST_BYTES]){0},
               W_SEED_GENERIC_VALIDATION_SPECIALIZATION_DIGEST_BYTES) == 0);
  return true;
}

static bool nominal_test_append_bytes(uint8_t *buffer, size_t capacity,
                                      size_t *length, const uint8_t *bytes,
                                      size_t count) {
  if (buffer == NULL || length == NULL || bytes == NULL || *length > capacity ||
      count > capacity - *length)
    return false;
  (void)memcpy(buffer + *length, bytes, count);
  *length += count;
  return true;
}

static bool nominal_test_append_u8(uint8_t *buffer, size_t capacity,
                                   size_t *length, uint8_t byte_value) {
  return nominal_test_append_bytes(buffer, capacity, length, &byte_value, 1u);
}

static bool nominal_test_append_u32(uint8_t *buffer, size_t capacity,
                                    size_t *length, uint32_t integer_value) {
  const uint8_t bytes[] = {(uint8_t)(integer_value >> 24),
                           (uint8_t)(integer_value >> 16),
                           (uint8_t)(integer_value >> 8),
                           (uint8_t)integer_value};
  return nominal_test_append_bytes(buffer, capacity, length, bytes,
                                   sizeof(bytes));
}

static bool nominal_test_append_text(uint8_t *buffer, size_t capacity,
                                     size_t *length, const char *text) {
  if (text == NULL) return false;
  const size_t text_length = strlen(text);
  return nominal_test_append_u32(buffer, capacity, length,
                                 (uint32_t)text_length) &&
         nominal_test_append_bytes(buffer, capacity, length,
                                   (const uint8_t *)text, text_length);
}

static void nominal_test_digest(const uint8_t *bytes, size_t length,
                                uint8_t digest[
                                    W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_DIGEST_BYTES]) {
  w_seed_sha256_state sha;
  w_seed_sha256_init(&sha);
  w_seed_sha256_update(&sha, bytes, length);
  w_seed_sha256_final(&sha, digest);
}

static size_t nominal_test_over_ceiling_receipt(uint8_t *buffer,
                                                size_t capacity,
                                                uint32_t segment_count,
                                                uint32_t owner_count) {
  static const uint8_t prefix[] = "w-seed-nominal-origin-1";
  static const uint8_t authority[] = "w-authority-fixture-1|registry=a";
  static const char package_name[] = "last-light/restaurant";
  static const char segment_name[] = "domain";
  static const char owner_name[] = "Outer";
  static const char declared_name[] = "Box";
  size_t length = 0u;
  if (!nominal_test_append_bytes(buffer, capacity, &length, prefix,
                                 sizeof(prefix) - 1u) ||
      !nominal_test_append_u8(buffer, capacity, &length, 0x4fu) ||
      !nominal_test_append_u8(buffer, capacity, &length, 0x41u) ||
      !nominal_test_append_u32(buffer, capacity, &length,
                               (uint32_t)(sizeof(authority) - 1u)) ||
      !nominal_test_append_bytes(buffer, capacity, &length, authority,
                                 sizeof(authority) - 1u) ||
      !nominal_test_append_u8(buffer, capacity, &length, 0x50u) ||
      !nominal_test_append_text(buffer, capacity, &length, package_name) ||
      !nominal_test_append_u8(buffer, capacity, &length, 0x4du) ||
      !nominal_test_append_u32(buffer, capacity, &length, segment_count))
    return 0u;
  for (uint32_t index = 0u; index < segment_count; index += 1u) {
    if (!nominal_test_append_u8(buffer, capacity, &length, 0x49u) ||
        !nominal_test_append_text(buffer, capacity, &length, segment_name))
      return 0u;
  }
  if (!nominal_test_append_u8(buffer, capacity, &length, 0x44u) ||
      !nominal_test_append_u8(buffer, capacity, &length, 0x01u) ||
      !nominal_test_append_u32(buffer, capacity, &length, owner_count))
    return 0u;
  for (uint32_t index = 0u; index < owner_count; index += 1u) {
    if (!nominal_test_append_u8(buffer, capacity, &length, 0x01u) ||
        !nominal_test_append_text(buffer, capacity, &length, owner_name))
      return 0u;
  }
  if (!nominal_test_append_text(buffer, capacity, &length, declared_name))
    return 0u;
  return length;
}

static bool test_nominal_origin_contract(void) {
  static const uint8_t authority_a[] = "w-authority-fixture-1|registry=a";
  static const uint8_t authority_b[] = "w-authority-fixture-1|registry=b";
  static const char package_a[] = "last-light/restaurant";
  static const char package_b[] = "other/restaurant";
  static const char module_a[] = "domain";
  static const char module_b[] = "generics";
  static const char name[] = "Box";
  static const char owner_name[] = "Outer";
  const w_seed_frontend_text module_segment = {module_a, sizeof(module_a) - 1u};
  const w_seed_generic_nominal_origin origin = {
      authority_a,
      sizeof(authority_a) - 1u,
      {package_a, sizeof(package_a) - 1u},
      &module_segment,
      1u,
      W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
      NULL,
      0u,
      {name, sizeof(name) - 1u}};
  w_seed_generic_nominal_origin_result measured;
  CHECK(w_seed_generic_nominal_origin_measure(&origin, &measured) ==
            W_SEED_GENERIC_NOMINAL_ORIGIN_AVAILABLE &&
        measured.bytes_required != 0u && measured.bytes_written == 0u);
  uint8_t preimage[W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PREIMAGE_BYTES];
  (void)memset(preimage, 0xa5, sizeof(preimage));
  w_seed_generic_nominal_origin_result written;
  CHECK(w_seed_generic_nominal_origin_write(
            &origin, preimage, measured.bytes_required, &written) ==
            W_SEED_GENERIC_NOMINAL_ORIGIN_AVAILABLE &&
        written.bytes_written == measured.bytes_required &&
        written.bytes_required == measured.bytes_required &&
        memcmp(written.digest, measured.digest, sizeof(written.digest)) == 0 &&
        preimage[written.bytes_written] == 0xa5u);
  const w_seed_generic_nominal_origin_view base_view = {
      preimage, written.bytes_written, written.digest, 0u, 0u};
  CHECK(w_seed_generic_nominal_origin_view_valid(&base_view));

  uint8_t short_preimage[
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PREIMAGE_BYTES];
  (void)memset(short_preimage, 0xa5, sizeof(short_preimage));
  w_seed_generic_nominal_origin_result short_result;
  CHECK(w_seed_generic_nominal_origin_write(
            &origin, short_preimage, measured.bytes_required - 1u,
            &short_result) == W_SEED_GENERIC_NOMINAL_ORIGIN_CAPACITY &&
        short_result.bytes_required == measured.bytes_required &&
        short_result.bytes_written == 0u &&
        short_preimage[0] == 0xa5u &&
        short_preimage[sizeof(short_preimage) - 1u] == 0xa5u);
  w_seed_generic_nominal_origin_result zero_result;
  CHECK(w_seed_generic_nominal_origin_write(&origin, preimage, 0u,
                                             &zero_result) ==
            W_SEED_GENERIC_NOMINAL_ORIGIN_CAPACITY &&
        zero_result.bytes_required == measured.bytes_required &&
        zero_result.bytes_written == 0u);
  w_seed_generic_nominal_origin_result null_result;
  CHECK(w_seed_generic_nominal_origin_write(&origin, NULL, 1u, &null_result) ==
        W_SEED_GENERIC_NOMINAL_ORIGIN_INVALID);

  const w_seed_frontend_text module_segment_b = {module_b, sizeof(module_b) - 1u};
  const w_seed_generic_nominal_origin authority_origin = {
      authority_b,
      sizeof(authority_b) - 1u,
      {package_a, sizeof(package_a) - 1u},
      &module_segment,
      1u,
      W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
      NULL,
      0u,
      {name, sizeof(name) - 1u}};
  const w_seed_generic_nominal_origin package_origin = {
      authority_a,
      sizeof(authority_a) - 1u,
      {package_b, sizeof(package_b) - 1u},
      &module_segment,
      1u,
      W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
      NULL,
      0u,
      {name, sizeof(name) - 1u}};
  const w_seed_generic_nominal_origin module_origin = {
      authority_a,
      sizeof(authority_a) - 1u,
      {package_a, sizeof(package_a) - 1u},
      &module_segment_b,
      1u,
      W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
      NULL,
      0u,
      {name, sizeof(name) - 1u}};
  const w_seed_generic_nominal_owner owner = {
      W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
      {owner_name, sizeof(owner_name) - 1u}};
  const w_seed_generic_nominal_origin owner_origin = {
      authority_a,
      sizeof(authority_a) - 1u,
      {package_a, sizeof(package_a) - 1u},
      &module_segment,
      1u,
      W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
      &owner,
      1u,
      {name, sizeof(name) - 1u}};
  const w_seed_generic_nominal_origin kind_origin = {
      authority_a,
      sizeof(authority_a) - 1u,
      {package_a, sizeof(package_a) - 1u},
      &module_segment,
      1u,
      W_SEED_GENERIC_NOMINAL_DECLARATION_TYPE,
      NULL,
      0u,
      {name, sizeof(name) - 1u}};
  const w_seed_generic_nominal_origin *variants[] = {
      &authority_origin, &package_origin, &module_origin, &owner_origin,
      &kind_origin};
  uint8_t variant_preimages[5][
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PREIMAGE_BYTES];
  w_seed_generic_nominal_origin_view variant_views[5];
  for (size_t index = 0u; index < 5u; index += 1u) {
    w_seed_generic_nominal_origin_result variant_result;
    (void)memset(variant_preimages[index], 0xa5,
                 sizeof(variant_preimages[index]));
    w_seed_generic_nominal_origin_result variant_measure;
    CHECK(w_seed_generic_nominal_origin_measure(variants[index],
                                                &variant_measure) ==
          W_SEED_GENERIC_NOMINAL_ORIGIN_AVAILABLE);
    CHECK(w_seed_generic_nominal_origin_write(
               variants[index], variant_preimages[index],
               variant_measure.bytes_required, &variant_result) ==
              W_SEED_GENERIC_NOMINAL_ORIGIN_AVAILABLE);
    variant_views[index] = (w_seed_generic_nominal_origin_view){
        variant_preimages[index], variant_result.bytes_written,
        variant_result.digest, 0u, 0u};
    CHECK(w_seed_generic_nominal_origin_view_valid(&variant_views[index]) &&
          !w_seed_generic_nominal_origin_equal(&base_view, &variant_views[index]));
  }
  uint8_t forced_digest[W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_DIGEST_BYTES];
  (void)memcpy(forced_digest, written.digest, sizeof(forced_digest));
  variant_views[0].digest = forced_digest;
  CHECK(!w_seed_generic_nominal_origin_equal(&base_view, &variant_views[0]));
  uint8_t corrupt_digest[
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_DIGEST_BYTES];
  (void)memcpy(corrupt_digest, written.digest, sizeof(corrupt_digest));
  corrupt_digest[0] ^= 0x01u;
  const w_seed_generic_nominal_origin_view corrupt_view = {
      preimage, written.bytes_written, corrupt_digest, 0u, 0u};
  CHECK(!w_seed_generic_nominal_origin_view_valid(&corrupt_view));
  uint8_t trailing_preimage[
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PREIMAGE_BYTES];
  (void)memcpy(trailing_preimage, preimage, written.bytes_written);
  trailing_preimage[written.bytes_written] = 0xa5u;
  const w_seed_generic_nominal_origin_view trailing_view = {
      trailing_preimage, written.bytes_written + 1u, written.digest, 0u, 0u};
  CHECK(!w_seed_generic_nominal_origin_view_valid(&trailing_view));
  const char unicode_name[] = "B\xc3\xb3x";
  const w_seed_generic_nominal_origin unicode_origin = {
      authority_a,
      sizeof(authority_a) - 1u,
      {package_a, sizeof(package_a) - 1u},
      &module_segment,
      1u,
      W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
      NULL,
      0u,
      {unicode_name, sizeof(unicode_name) - 1u}};
  w_seed_generic_nominal_origin_result unicode_result;
  CHECK(w_seed_generic_nominal_origin_measure(&unicode_origin,
                                               &unicode_result) ==
        W_SEED_GENERIC_NOMINAL_ORIGIN_UNSUPPORTED);
  static const char invalid_package_name[] = "Last-light/restaurant";
  static const char invalid_identifier_name[] = "bad-name";
  static const uint8_t malformed_utf8_name[] = {'B', 0xc3u, 0x28u, 'x'};
  static const char non_ascii_package[] = "last-lÃ¡ght/restaurant";
  char ceiling_name[W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_NAME_BYTES +
                    1u];
  (void)memset(ceiling_name, 'A', sizeof(ceiling_name));
  const w_seed_generic_nominal_origin invalid_package_origin = {
      authority_a,
      sizeof(authority_a) - 1u,
      {invalid_package_name, sizeof(invalid_package_name) - 1u},
      &module_segment,
      1u,
      W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
      NULL,
      0u,
      {name, sizeof(name) - 1u}};
  const w_seed_generic_nominal_origin invalid_identifier_origin = {
      authority_a,
      sizeof(authority_a) - 1u,
      {package_a, sizeof(package_a) - 1u},
      &module_segment,
      1u,
      W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
      NULL,
      0u,
      {invalid_identifier_name, sizeof(invalid_identifier_name) - 1u}};
  const w_seed_generic_nominal_origin malformed_utf8_origin = {
      authority_a,
      sizeof(authority_a) - 1u,
      {package_a, sizeof(package_a) - 1u},
      &module_segment,
      1u,
      W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
      NULL,
      0u,
      {(const char *)malformed_utf8_name, sizeof(malformed_utf8_name)}};
  const w_seed_generic_nominal_origin non_ascii_package_origin = {
      authority_a,
      sizeof(authority_a) - 1u,
      {non_ascii_package, sizeof(non_ascii_package) - 1u},
      &module_segment,
      1u,
      W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
      NULL,
      0u,
      {name, sizeof(name) - 1u}};
  const w_seed_generic_nominal_origin ceiling_origin = {
      authority_a,
      sizeof(authority_a) - 1u,
      {package_a, sizeof(package_a) - 1u},
      &module_segment,
      1u,
      W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
      NULL,
      0u,
      {ceiling_name, sizeof(ceiling_name)}};
  w_seed_generic_nominal_origin_result canonical_result;
  CHECK(w_seed_generic_nominal_origin_measure(&invalid_package_origin,
                                               &canonical_result) ==
            W_SEED_GENERIC_NOMINAL_ORIGIN_INVALID &&
        w_seed_generic_nominal_origin_measure(&invalid_identifier_origin,
                                               &canonical_result) ==
            W_SEED_GENERIC_NOMINAL_ORIGIN_INVALID &&
        w_seed_generic_nominal_origin_measure(&malformed_utf8_origin,
                                               &canonical_result) ==
            W_SEED_GENERIC_NOMINAL_ORIGIN_INVALID &&
        w_seed_generic_nominal_origin_measure(&non_ascii_package_origin,
                                               &canonical_result) ==
            W_SEED_GENERIC_NOMINAL_ORIGIN_INVALID &&
        w_seed_generic_nominal_origin_measure(&ceiling_origin,
                                               &canonical_result) ==
            W_SEED_GENERIC_NOMINAL_ORIGIN_UNSUPPORTED);

  /* Caller count/length ceilings are UNSUPPORTED.  These probes use valid
   * pointers with deliberately huge metadata, so overlap preflight must not
   * multiply or walk beyond the supported arrays. */
  static uint8_t authority_ceiling[
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_AUTHORITY_BYTES + 1u];
  (void)memset(authority_ceiling, 'a', sizeof(authority_ceiling));
  const w_seed_generic_nominal_origin authority_ceiling_origin = {
      authority_ceiling,
      sizeof(authority_ceiling),
      origin.scoped_package_name,
      origin.module_path_segments,
      origin.module_path_segment_count,
      origin.declaration_kind,
      origin.owner_chain,
      origin.owner_chain_count,
      origin.declared_name};
  static const w_seed_frontend_text one_module_segment = {
      module_a, sizeof(module_a) - 1u};
  const w_seed_generic_nominal_origin huge_module_count_origin = {
      authority_a,
      sizeof(authority_a) - 1u,
      origin.scoped_package_name,
      &one_module_segment,
      SIZE_MAX,
      origin.declaration_kind,
      origin.owner_chain,
      origin.owner_chain_count,
      origin.declared_name};
  const w_seed_generic_nominal_owner one_owner = {
      W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
      {owner_name, sizeof(owner_name) - 1u}};
  const w_seed_generic_nominal_origin huge_owner_count_origin = {
      authority_a,
      sizeof(authority_a) - 1u,
      origin.scoped_package_name,
      origin.module_path_segments,
      origin.module_path_segment_count,
      origin.declaration_kind,
      &one_owner,
      SIZE_MAX,
      origin.declared_name};
  static uint8_t huge_total_authority[
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_AUTHORITY_BYTES];
  static char huge_total_segment[
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_SEGMENT_BYTES];
  static char huge_total_name[
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_NAME_BYTES];
  static w_seed_frontend_text huge_total_segments[
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_MODULE_SEGMENTS];
  static w_seed_generic_nominal_owner huge_total_owners[
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_OWNER_CHAIN];
  (void)memset(huge_total_authority, 'a', sizeof(huge_total_authority));
  (void)memset(huge_total_segment, 'A', sizeof(huge_total_segment));
  (void)memset(huge_total_name, 'A', sizeof(huge_total_name));
  for (size_t index = 0u;
       index < W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_MODULE_SEGMENTS;
       index += 1u)
    huge_total_segments[index] =
        (w_seed_frontend_text){huge_total_segment, sizeof(huge_total_segment)};
  for (size_t index = 0u;
       index < W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_OWNER_CHAIN;
       index += 1u)
    huge_total_owners[index] = (w_seed_generic_nominal_owner){
        W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
        {huge_total_name, sizeof(huge_total_name)}};
  const w_seed_generic_nominal_origin huge_total_origin = {
      huge_total_authority,
      sizeof(huge_total_authority),
      origin.scoped_package_name,
      huge_total_segments,
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_MODULE_SEGMENTS,
      origin.declaration_kind,
      huge_total_owners,
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_OWNER_CHAIN,
      {huge_total_name, sizeof(huge_total_name)}};
  const w_seed_generic_nominal_origin *ceiling_origins[] = {
      &authority_ceiling_origin, &huge_module_count_origin,
      &huge_owner_count_origin, &huge_total_origin};
  for (size_t index = 0u;
       index < sizeof(ceiling_origins) / sizeof(ceiling_origins[0]);
       index += 1u) {
    w_seed_generic_nominal_origin_result ceiling_measure;
    CHECK(w_seed_generic_nominal_origin_measure(ceiling_origins[index],
                                                 &ceiling_measure) ==
              W_SEED_GENERIC_NOMINAL_ORIGIN_UNSUPPORTED &&
          ceiling_measure.bytes_written == 0u &&
          ceiling_measure.bytes_required == 0u &&
          memcmp(ceiling_measure.digest,
                 (uint8_t[W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_DIGEST_BYTES]){0},
                 W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_DIGEST_BYTES) == 0);
    uint8_t ceiling_output[64];
    (void)memset(ceiling_output, 0xa5, sizeof(ceiling_output));
    w_seed_generic_nominal_origin_result ceiling_write;
    CHECK(w_seed_generic_nominal_origin_write(
              ceiling_origins[index], ceiling_output, sizeof(ceiling_output),
              &ceiling_write) == W_SEED_GENERIC_NOMINAL_ORIGIN_UNSUPPORTED &&
          ceiling_write.bytes_written == 0u &&
          ceiling_write.bytes_required == 0u &&
          memcmp(ceiling_write.digest,
                 (uint8_t[W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_DIGEST_BYTES]){0},
                 W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_DIGEST_BYTES) == 0);
    for (size_t byte = 0u; byte < sizeof(ceiling_output); byte += 1u)
      CHECK(ceiling_output[byte] == 0xa5u);
  }
  /* An unsupported count must not make the result or output alias an
   * unbounded caller array.  The implementation may reject the alias before
   * publishing the UNSUPPORTED state, but it must not mutate either storage. */
  union {
    w_seed_generic_nominal_origin_result result;
    w_seed_frontend_text module;
  } huge_count_result_storage;
  huge_count_result_storage.module = one_module_segment;
  uint8_t huge_count_result_snapshot[sizeof(huge_count_result_storage)];
  (void)memcpy(huge_count_result_snapshot, &huge_count_result_storage,
               sizeof(huge_count_result_snapshot));
  const w_seed_generic_nominal_origin huge_count_result_origin = {
      authority_a,
      sizeof(authority_a) - 1u,
      origin.scoped_package_name,
      &huge_count_result_storage.module,
      SIZE_MAX,
      origin.declaration_kind,
      origin.owner_chain,
      origin.owner_chain_count,
      origin.declared_name};
  CHECK(w_seed_generic_nominal_origin_measure(
            &huge_count_result_origin, &huge_count_result_storage.result) ==
            W_SEED_GENERIC_NOMINAL_ORIGIN_INVALID &&
        memcmp(huge_count_result_snapshot, &huge_count_result_storage,
               sizeof(huge_count_result_snapshot)) == 0);
  union {
    w_seed_frontend_text module;
    uint8_t output[64];
  } huge_count_output_storage;
  huge_count_output_storage.module = one_module_segment;
  uint8_t huge_count_output_snapshot[sizeof(huge_count_output_storage)];
  (void)memcpy(huge_count_output_snapshot, &huge_count_output_storage,
               sizeof(huge_count_output_snapshot));
  const w_seed_generic_nominal_origin huge_count_output_origin = {
      authority_a,
      sizeof(authority_a) - 1u,
      origin.scoped_package_name,
      &huge_count_output_storage.module,
      SIZE_MAX,
      origin.declaration_kind,
      origin.owner_chain,
      origin.owner_chain_count,
      origin.declared_name};
  w_seed_generic_nominal_origin_result huge_count_output_result;
  CHECK(w_seed_generic_nominal_origin_write(
            &huge_count_output_origin, huge_count_output_storage.output,
            sizeof(huge_count_output_storage.output),
            &huge_count_output_result) ==
            W_SEED_GENERIC_NOMINAL_ORIGIN_INVALID &&
        memcmp(huge_count_output_snapshot, &huge_count_output_storage,
               sizeof(huge_count_output_snapshot)) == 0);

  static const char validation_source[] =
      "const fn always(value: i64): Bool { return true }\n"
      "struct Box<_ value: i64<(always(.member))>> {}\n"
      "struct Use { item: Box<42> }\n";
  CHECK(fixture_lower(&value, validation_source));
  CHECK(fixture_prepare_nominal_origin(&value, 0u));
  static uint8_t over_segment_receipt[4096];
  static uint8_t over_owner_receipt[4096];
  const size_t over_segment_length = nominal_test_over_ceiling_receipt(
      over_segment_receipt, sizeof(over_segment_receipt),
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_MODULE_SEGMENTS + 1u, 0u);
  const size_t over_owner_length = nominal_test_over_ceiling_receipt(
      over_owner_receipt, sizeof(over_owner_receipt), 1u,
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_OWNER_CHAIN + 1u);
  CHECK(over_segment_length > 0u && over_segment_length <
            W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PARSE_PREIMAGE_BYTES &&
        over_owner_length != 0u);
  const uint8_t *over_receipts[] = {over_segment_receipt, over_owner_receipt};
  const size_t over_lengths[] = {over_segment_length, over_owner_length};
  uint8_t over_digests[2][
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_DIGEST_BYTES];
  for (size_t index = 0u; index < sizeof(over_lengths) / sizeof(over_lengths[0]);
       index += 1u)
    nominal_test_digest(over_receipts[index], over_lengths[index],
                        over_digests[index]);
  for (size_t index = 0u; index < sizeof(over_lengths) / sizeof(over_lengths[0]);
       index += 1u) {
    const w_seed_generic_nominal_origin_view over_view = {
        over_receipts[index], over_lengths[index],
        over_digests[index], 0u, 0u};
    CHECK(!w_seed_generic_nominal_origin_view_valid(&over_view));
    (void)memset(value.specialization_preimage, 0xa5,
                 sizeof(value.specialization_preimage));
    (void)memset(value.receipts, 0xa5, sizeof(value.receipts));
    w_seed_generic_validation_result over_result;
    CHECK(validate_application_at_with_origin_view(
              &value, 0u, &over_view, value.specialization_preimage,
              sizeof(value.specialization_preimage), &over_result) ==
              W_SEED_GENERIC_VALIDATION_VERIFIED &&
          over_result.specialization_state ==
              W_SEED_GENERIC_VALIDATION_SPECIALIZATION_UNSUPPORTED &&
          over_result.evaluation.consumed_steps != 0u &&
          over_result.receipts_written != 0u &&
          over_result.specialization_bytes_written == 0u &&
          over_result.specialization_bytes_required == 0u);
    for (size_t byte = 0u; byte < sizeof(value.specialization_preimage);
         byte += 1u)
      CHECK(value.specialization_preimage[byte] == 0xa5u);
    uint8_t corrupt_over_digest[
        W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_DIGEST_BYTES];
    (void)memcpy(corrupt_over_digest, over_digests[index],
                 sizeof(corrupt_over_digest));
    corrupt_over_digest[0] ^= 0x01u;
    const w_seed_generic_nominal_origin_view corrupt_over_view = {
        over_receipts[index], over_lengths[index], corrupt_over_digest, 0u, 0u};
    (void)memset(value.specialization_preimage, 0xa5,
                 sizeof(value.specialization_preimage));
    (void)memset(value.receipts, 0xa5, sizeof(value.receipts));
    w_seed_generic_validation_result corrupt_over_result;
    CHECK(validate_application_at_with_origin_view(
              &value, 0u, &corrupt_over_view, value.specialization_preimage,
              sizeof(value.specialization_preimage), &corrupt_over_result) ==
              W_SEED_GENERIC_VALIDATION_INVALID &&
          corrupt_over_result.evaluation.consumed_steps == 0u &&
          corrupt_over_result.receipts_written == 0u &&
          corrupt_over_result.specialization_bytes_written == 0u &&
          corrupt_over_result.specialization_bytes_required == 0u);
    for (size_t byte = 0u; byte < sizeof(value.receipts); byte += 1u)
      CHECK(((const uint8_t *)value.receipts)[byte] == 0xa5u);
    for (size_t byte = 0u; byte < sizeof(value.specialization_preimage);
         byte += 1u)
      CHECK(value.specialization_preimage[byte] == 0xa5u);
    const w_seed_generic_nominal_origin_view truncated_over_view = {
        over_receipts[index], over_lengths[index] - 1u,
        over_digests[index], 0u, 0u};
    (void)memset(value.specialization_preimage, 0xa5,
                 sizeof(value.specialization_preimage));
    (void)memset(value.receipts, 0xa5, sizeof(value.receipts));
    w_seed_generic_validation_result truncated_over_result;
    CHECK(validate_application_at_with_origin_view(
              &value, 0u, &truncated_over_view, value.specialization_preimage,
              sizeof(value.specialization_preimage), &truncated_over_result) ==
              W_SEED_GENERIC_VALIDATION_INVALID &&
          truncated_over_result.evaluation.consumed_steps == 0u &&
          truncated_over_result.receipts_written == 0u &&
          truncated_over_result.specialization_bytes_written == 0u &&
          truncated_over_result.specialization_bytes_required == 0u);
    for (size_t byte = 0u; byte < sizeof(value.receipts); byte += 1u)
      CHECK(((const uint8_t *)value.receipts)[byte] == 0xa5u);
    for (size_t byte = 0u; byte < sizeof(value.specialization_preimage);
         byte += 1u)
      CHECK(value.specialization_preimage[byte] == 0xa5u);
  }
  (void)memset(value.conversion_values, 0xa5, sizeof(value.conversion_values));
  (void)memset(value.receipts, 0xa5, sizeof(value.receipts));
  w_seed_generic_validation_result missing_result;
  CHECK(validate_application_at_with_origin_view(
            &value, 0u, NULL, value.specialization_preimage,
            sizeof(value.specialization_preimage), &missing_result) ==
            W_SEED_GENERIC_VALIDATION_VERIFIED &&
        missing_result.specialization_state ==
            W_SEED_GENERIC_VALIDATION_SPECIALIZATION_IDENTITY_REQUIRED &&
        missing_result.specialization_bytes_written == 0u &&
        missing_result.specialization_bytes_required == 0u &&
        missing_result.receipts_written == 1u);
  /* Rebuild the receipt after the semantic run, then check malformed views
   * stop before the evaluator and leave caller-owned arrays untouched. */
  CHECK(fixture_prepare_nominal_origin(&value, 0u));
  (void)memcpy(trailing_preimage, value.nominal_origin_view.preimage,
               value.nominal_origin_view.preimage_length);
  trailing_preimage[value.nominal_origin_view.preimage_length] = 0xa5u;
  const w_seed_generic_nominal_origin_view malformed_view = {
      trailing_preimage, value.nominal_origin_view.preimage_length + 1u,
      value.nominal_origin_view.digest, value.nominal_origin_view.frontend_module_index,
      value.nominal_origin_view.frontend_head_struct_index};
  (void)memset(value.conversion_values, 0xa5, sizeof(value.conversion_values));
  (void)memset(value.receipts, 0xa5, sizeof(value.receipts));
  w_seed_generic_validation_result malformed_result;
  CHECK(validate_application_at_with_origin_view(
            &value, 0u, &malformed_view, value.specialization_preimage,
            sizeof(value.specialization_preimage), &malformed_result) ==
            W_SEED_GENERIC_VALIDATION_INVALID &&
        malformed_result.receipts_written == 0u &&
        malformed_result.evaluation.consumed_steps == 0u);
  for (size_t byte = 0u; byte < sizeof(value.receipts); byte += 1u)
    CHECK(((const uint8_t *)value.receipts)[byte] == 0xa5u);
  w_seed_generic_nominal_origin_view wrong_relation = value.nominal_origin_view;
  wrong_relation.frontend_module_index += 1u;
  w_seed_generic_validation_result wrong_relation_result;
  CHECK(validate_application_at_with_origin_view(
             &value, 0u, &wrong_relation, value.specialization_preimage,
             sizeof(value.specialization_preimage), &wrong_relation_result) ==
            W_SEED_GENERIC_VALIDATION_INVALID &&
         wrong_relation_result.evaluation.consumed_steps == 0u &&
         wrong_relation_result.receipts_written == 0u);
  CHECK(fixture_lower_with_module(&value, validation_source,
                                  "platform.native"));
  CHECK(fixture_prepare_nominal_origin(&value, 0u));
  w_seed_generic_validation_result multi_segment_result;
  CHECK(validate_application_at_with_origin_view(
            &value, 0u, &value.nominal_origin_view,
            value.specialization_preimage,
            sizeof(value.specialization_preimage), &multi_segment_result) ==
        W_SEED_GENERIC_VALIDATION_VERIFIED);
  CHECK(fixture_lower_with_module(&value, validation_source, "domain"));
  CHECK(fixture_prepare_nominal_origin(&value, 0u));
  static const char prefix_segment[] = "evil";
  static const char prefix_domain_segment[] = "domain";
  const w_seed_frontend_text prefix_segments[] = {
      {prefix_segment, sizeof(prefix_segment) - 1u},
      {prefix_domain_segment, sizeof(prefix_domain_segment) - 1u}};
  const w_seed_generic_nominal_origin prefix_origin = {
      authority_a,
      sizeof(authority_a) - 1u,
      {package_a, sizeof(package_a) - 1u},
      prefix_segments,
      2u,
      W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
      NULL,
      0u,
      value.structs[0].name};
  uint8_t prefix_preimage[
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PREIMAGE_BYTES];
  w_seed_generic_nominal_origin_result prefix_measure;
  CHECK(w_seed_generic_nominal_origin_measure(&prefix_origin,
                                               &prefix_measure) ==
        W_SEED_GENERIC_NOMINAL_ORIGIN_AVAILABLE);
  w_seed_generic_nominal_origin_result prefix_written;
  CHECK(w_seed_generic_nominal_origin_write(
            &prefix_origin, prefix_preimage, prefix_measure.bytes_required,
            &prefix_written) == W_SEED_GENERIC_NOMINAL_ORIGIN_AVAILABLE);
  const w_seed_generic_nominal_origin_view prefix_view = {
      prefix_preimage, prefix_written.bytes_written, prefix_written.digest,
      value.nominal_origin_view.frontend_module_index,
      value.nominal_origin_view.frontend_head_struct_index};
  (void)memset(value.receipts, 0xa5, sizeof(value.receipts));
  w_seed_generic_validation_result prefix_result;
  CHECK(validate_application_at_with_origin_view(
            &value, 0u, &prefix_view, value.specialization_preimage,
            sizeof(value.specialization_preimage), &prefix_result) ==
            W_SEED_GENERIC_VALIDATION_INVALID &&
        prefix_result.evaluation.consumed_steps == 0u &&
        prefix_result.receipts_written == 0u);
  for (size_t byte = 0u; byte < sizeof(value.receipts); byte += 1u)
    CHECK(((const uint8_t *)value.receipts)[byte] == 0xa5u);
  uint8_t alias_authority[sizeof(authority_a)];
  (void)memcpy(alias_authority, authority_a, sizeof(alias_authority));
  const w_seed_generic_nominal_origin alias_origin = {
      alias_authority,
      sizeof(alias_authority) - 1u,
      origin.scoped_package_name,
      origin.module_path_segments,
      origin.module_path_segment_count,
      origin.declaration_kind,
      origin.owner_chain,
      origin.owner_chain_count,
      origin.declared_name};
  w_seed_generic_nominal_origin_result alias_origin_result;
  CHECK(w_seed_generic_nominal_origin_write(
            &alias_origin, alias_authority, sizeof(alias_authority),
            &alias_origin_result) == W_SEED_GENERIC_NOMINAL_ORIGIN_INVALID &&
        memcmp(alias_authority, authority_a, sizeof(alias_authority)) == 0);
  uint8_t origin_preimage_snapshot[
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PREIMAGE_BYTES];
  (void)memcpy(origin_preimage_snapshot, value.nominal_origin_view.preimage,
               value.nominal_origin_view.preimage_length);
  w_seed_generic_validation_result alias_validation_result;
  CHECK(validate_application_at_with_origin_view(
            &value, 0u, &value.nominal_origin_view,
            (uint8_t *)value.nominal_origin_view.preimage,
            value.nominal_origin_view.preimage_length,
            &alias_validation_result) == W_SEED_GENERIC_VALIDATION_INVALID &&
        memcmp(origin_preimage_snapshot, value.nominal_origin_view.preimage,
               value.nominal_origin_view.preimage_length) == 0);
  uint8_t digest_snapshot[
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_DIGEST_BYTES];
  (void)memcpy(digest_snapshot, value.nominal_origin_view.digest,
               sizeof(digest_snapshot));
  CHECK(validate_application_at_with_origin_view(
            &value, 0u, &value.nominal_origin_view,
            (uint8_t *)value.nominal_origin_view.digest,
            sizeof(digest_snapshot), &alias_validation_result) ==
            W_SEED_GENERIC_VALIDATION_INVALID &&
        memcmp(digest_snapshot, value.nominal_origin_view.digest,
               sizeof(digest_snapshot)) == 0);
  return true;
}

static const char *nominal_origin_state_name_local(
    w_seed_generic_nominal_origin_state state) {
  switch (state) {
    case W_SEED_GENERIC_NOMINAL_ORIGIN_INVALID:
      return "INVALID";
    case W_SEED_GENERIC_NOMINAL_ORIGIN_AVAILABLE:
      return "AVAILABLE";
    case W_SEED_GENERIC_NOMINAL_ORIGIN_UNSUPPORTED:
      return "UNSUPPORTED";
    case W_SEED_GENERIC_NOMINAL_ORIGIN_CAPACITY:
      return "CAPACITY";
  }
  return "UNKNOWN";
}

static void print_nominal_origin_result_line(
    const char *case_name, const w_seed_generic_nominal_origin_result *result,
    const uint8_t *preimage) {
  if (case_name == NULL || result == NULL) return;
  (void)printf("ORIGIN case=%s state=%s written=%llu required=%llu digest=",
               case_name, nominal_origin_state_name_local(result->state),
               (unsigned long long)result->bytes_written,
               (unsigned long long)result->bytes_required);
  print_digest(result->digest);
  if (result->state == W_SEED_GENERIC_NOMINAL_ORIGIN_AVAILABLE &&
      preimage != NULL) {
    (void)printf(" preimage=");
    print_bytes_hex(preimage, result->bytes_written);
  }
  (void)putchar('\n');
}

static void print_nominal_validation_line(
    const char *case_name, const fixture *fixture_value,
    const w_seed_generic_validation_result *result,
    const uint8_t predicate_digest[32]) {
  if (case_name == NULL || fixture_value == NULL || result == NULL) return;
  (void)printf("NOMINAL_VALIDATION case=%s state=%s specialization_state=%s "
               "specialization_written=%llu specialization_required=%llu "
               "specialization_digest=",
               case_name, w_seed_generic_validation_state_name(result->state),
               w_seed_generic_validation_specialization_state_name(
                   result->specialization_state),
               (unsigned long long)result->specialization_bytes_written,
               (unsigned long long)result->specialization_bytes_required);
  if (result->specialization_state ==
      W_SEED_GENERIC_VALIDATION_SPECIALIZATION_AVAILABLE)
    print_digest(result->specialization_digest);
  else
    for (size_t index = 0u; index < 32u; index += 1u) (void)printf("00");
  if (result->specialization_state ==
      W_SEED_GENERIC_VALIDATION_SPECIALIZATION_AVAILABLE) {
    (void)printf(" specialization_preimage=");
    print_bytes_hex(fixture_value->specialization_preimage,
                    result->specialization_bytes_written);
  }
  (void)printf(" steps=%llu receipts=%llu predicate_body_digest=",
               (unsigned long long)result->evaluation.consumed_steps,
               (unsigned long long)result->receipts_written);
  if (predicate_digest != NULL)
    print_digest(predicate_digest);
  else
    for (size_t index = 0u; index < 32u; index += 1u) (void)printf("00");
  (void)putchar('\n');
}

static bool probe_nominal_origin_matrix(const uint8_t *authority_a,
                                        size_t authority_a_length,
                                        const uint8_t *authority_b,
                                        size_t authority_b_length) {
  if (authority_a == NULL || authority_a_length == 0u ||
      authority_b == NULL || authority_b_length == 0u)
    return false;
  static const char package_a[] = "last-light/restaurant";
  static const char package_b[] = "other/restaurant";
  static const char module_a[] = "domain";
  static const char module_b[] = "generics";
  static const char declaration_name[] = "Box";
  static const char owner_name[] = "Outer";
  const w_seed_frontend_text segment_a = {module_a, sizeof(module_a) - 1u};
  const w_seed_frontend_text segment_b = {module_b, sizeof(module_b) - 1u};
  const w_seed_generic_nominal_owner owner = {
      W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
      {owner_name, sizeof(owner_name) - 1u}};
  const w_seed_generic_nominal_origin base = {
      authority_a,
      authority_a_length,
      {package_a, sizeof(package_a) - 1u},
      &segment_a,
      1u,
      W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
      NULL,
      0u,
      {declaration_name, sizeof(declaration_name) - 1u}};
  const w_seed_generic_nominal_origin authority = {
      authority_b,
      authority_b_length,
      {package_a, sizeof(package_a) - 1u},
      &segment_a,
      1u,
      W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
      NULL,
      0u,
      {declaration_name, sizeof(declaration_name) - 1u}};
  const w_seed_generic_nominal_origin package = {
      authority_a,
      authority_a_length,
      {package_b, sizeof(package_b) - 1u},
      &segment_a,
      1u,
      W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
      NULL,
      0u,
      {declaration_name, sizeof(declaration_name) - 1u}};
  const w_seed_generic_nominal_origin module = {
      authority_a,
      authority_a_length,
      {package_a, sizeof(package_a) - 1u},
      &segment_b,
      1u,
      W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
      NULL,
      0u,
      {declaration_name, sizeof(declaration_name) - 1u}};
  const w_seed_generic_nominal_origin kind = {
      authority_a,
      authority_a_length,
      {package_a, sizeof(package_a) - 1u},
      &segment_a,
      1u,
      W_SEED_GENERIC_NOMINAL_DECLARATION_TYPE,
      NULL,
      0u,
      {declaration_name, sizeof(declaration_name) - 1u}};
  const w_seed_generic_nominal_origin owner_origin = {
      authority_a,
      authority_a_length,
      {package_a, sizeof(package_a) - 1u},
      &segment_a,
      1u,
      W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
      &owner,
      1u,
      {declaration_name, sizeof(declaration_name) - 1u}};
  const w_seed_generic_nominal_origin *origins[] = {
      &base, &authority, &package, &module, &kind, &owner_origin};
  const char *origin_names[] = {
      "base", "authority", "package", "module", "kind", "owner"};
  uint8_t origin_preimages[6][
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PREIMAGE_BYTES];
  w_seed_generic_nominal_origin_result origin_results[6];
  for (size_t index = 0u; index < 6u; index += 1u) {
    w_seed_generic_nominal_origin_result origin_measure;
    if (w_seed_generic_nominal_origin_measure(origins[index],
                                              &origin_measure) !=
        W_SEED_GENERIC_NOMINAL_ORIGIN_AVAILABLE)
      return false;
    (void)memset(origin_preimages[index], 0xa5,
                 sizeof(origin_preimages[index]));
    if (w_seed_generic_nominal_origin_write(
            origins[index], origin_preimages[index],
            origin_measure.bytes_required, &origin_results[index]) !=
        W_SEED_GENERIC_NOMINAL_ORIGIN_AVAILABLE)
      return false;
    if (origin_results[index].bytes_written != origin_measure.bytes_required ||
        origin_preimages[index][origin_results[index].bytes_written] != 0xa5u)
      return false;
    print_nominal_origin_result_line(origin_names[index], &origin_results[index],
                                     origin_preimages[index]);
  }
  uint8_t short_origin[
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PREIMAGE_BYTES];
  (void)memset(short_origin, 0xa5, sizeof(short_origin));
  w_seed_generic_nominal_origin_result short_result;
  if (w_seed_generic_nominal_origin_write(
          &base, short_origin, origin_results[0].bytes_required - 1u,
          &short_result) != W_SEED_GENERIC_NOMINAL_ORIGIN_CAPACITY)
    return false;
  print_nominal_origin_result_line("short", &short_result, NULL);
  w_seed_generic_nominal_origin_result zero_result;
  if (w_seed_generic_nominal_origin_write(&base, short_origin, 0u,
                                          &zero_result) !=
      W_SEED_GENERIC_NOMINAL_ORIGIN_CAPACITY)
    return false;
  print_nominal_origin_result_line("zero", &zero_result, NULL);
  w_seed_generic_nominal_origin_result null_result;
  if (w_seed_generic_nominal_origin_write(&base, NULL, 1u, &null_result) !=
      W_SEED_GENERIC_NOMINAL_ORIGIN_INVALID)
    return false;
  print_nominal_origin_result_line("null", &null_result, NULL);
  const char unicode_name[] = "B\xc3\xb3x";
  const w_seed_generic_nominal_origin unicode = {
      authority_a,
      authority_a_length,
      {package_a, sizeof(package_a) - 1u},
      &segment_a,
      1u,
      W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
      NULL,
      0u,
      {unicode_name, sizeof(unicode_name) - 1u}};
  w_seed_generic_nominal_origin_result unicode_result;
  if (w_seed_generic_nominal_origin_measure(&unicode, &unicode_result) !=
      W_SEED_GENERIC_NOMINAL_ORIGIN_UNSUPPORTED)
    return false;
  print_nominal_origin_result_line("unicode", &unicode_result, NULL);

  static const char validation_source[] =
      "const fn always(value: i64): Bool { return true }\n"
      "struct Box<_ value: i64<(always(.member))>> {}\n"
      "struct Use { item: Box<42> }\n";
  if (!fixture_lower_with_module(&value, validation_source, module_a) ||
      !fixture_prepare_nominal_origin_with_authority(
          &value, 0u, authority_a, authority_a_length))
    return false;
  w_seed_generic_validation_result valid_result;
  if (validate_application_at_with_origin_view(
          &value, 0u, &value.nominal_origin_view, value.specialization_preimage,
          sizeof(value.specialization_preimage), &valid_result) !=
      W_SEED_GENERIC_VALIDATION_VERIFIED)
    return false;
  uint8_t predicate_digest[32] = {0u};
  (void)predicate_body_digest_for_application(
      &value, &value.generic_applications[0], predicate_digest);
  print_nominal_validation_line("available", &value, &valid_result,
                                predicate_digest);
  static const char predicate_body_variant_source[] =
      "const fn always(value: i64): Bool { return value == value }\n"
      "struct Box<_ value: i64<(always(.member))>> {}\n"
      "struct Use { item: Box<42> }\n";
  CHECK(fixture_lower_with_module(&value, predicate_body_variant_source,
                                  module_a) &&
        fixture_prepare_nominal_origin_with_authority(
            &value, 0u, authority_a, authority_a_length));
  w_seed_generic_validation_result body_result;
  CHECK(validate_application_at_with_origin_view(
            &value, 0u, &value.nominal_origin_view, value.specialization_preimage,
            sizeof(value.specialization_preimage), &body_result) ==
        W_SEED_GENERIC_VALIDATION_VERIFIED);
  uint8_t body_predicate_digest[32] = {0u};
  CHECK(predicate_body_digest_for_application(
      &value, &value.generic_applications[0], body_predicate_digest));
  print_nominal_validation_line("body", &value, &body_result,
                                body_predicate_digest);
  CHECK(fixture_lower_with_module(&value, validation_source, module_a) &&
        fixture_prepare_nominal_origin_with_authority(
            &value, 0u, authority_a, authority_a_length));
  w_seed_generic_validation_result missing_result;
  if (validate_application_at_with_origin_view(
          &value, 0u, NULL, value.specialization_preimage,
          sizeof(value.specialization_preimage), &missing_result) !=
      W_SEED_GENERIC_VALIDATION_VERIFIED)
    return false;
  print_nominal_validation_line("missing", &value, &missing_result,
                                predicate_digest);
  uint8_t malformed_origin[
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PREIMAGE_BYTES];
  (void)memcpy(malformed_origin, value.nominal_origin_view.preimage,
               value.nominal_origin_view.preimage_length);
  malformed_origin[value.nominal_origin_view.preimage_length] = 0xa5u;
  const w_seed_generic_nominal_origin_view malformed_view = {
      malformed_origin, value.nominal_origin_view.preimage_length + 1u,
      value.nominal_origin_view.digest,
      value.nominal_origin_view.frontend_module_index,
      value.nominal_origin_view.frontend_head_struct_index};
  uint8_t truncated_origin[
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PREIMAGE_BYTES];
  (void)memcpy(truncated_origin, value.nominal_origin_view.preimage,
               value.nominal_origin_view.preimage_length);
  const w_seed_generic_nominal_origin_view truncated_view = {
      truncated_origin, value.nominal_origin_view.preimage_length - 1u,
      value.nominal_origin_view.digest,
      value.nominal_origin_view.frontend_module_index,
      value.nominal_origin_view.frontend_head_struct_index};
  (void)memset(value.specialization_preimage, 0xa5,
               sizeof(value.specialization_preimage));
  w_seed_generic_validation_result malformed_result;
  if (validate_application_at_with_origin_view(
          &value, 0u, &malformed_view, value.specialization_preimage,
          sizeof(value.specialization_preimage), &malformed_result) !=
      W_SEED_GENERIC_VALIDATION_INVALID)
    return false;
  if (malformed_result.receipts_written != 0u ||
      malformed_result.evaluation.consumed_steps != 0u ||
      value.specialization_preimage[0] != 0xa5u ||
      value.specialization_preimage[sizeof(value.specialization_preimage) - 1u] !=
          0xa5u)
    return false;
  print_nominal_validation_line("trailing", &value, &malformed_result,
                                predicate_digest);
  uint8_t corrupt_digest[32];
  (void)memcpy(corrupt_digest, value.nominal_origin_view.digest,
               sizeof(corrupt_digest));
  corrupt_digest[0] ^= 0x01u;
  const w_seed_generic_nominal_origin_view digest_view = {
      value.nominal_origin_view.preimage, value.nominal_origin_view.preimage_length,
      corrupt_digest, value.nominal_origin_view.frontend_module_index,
      value.nominal_origin_view.frontend_head_struct_index};
  w_seed_generic_validation_result digest_result;
  if (validate_application_at_with_origin_view(
          &value, 0u, &digest_view, value.specialization_preimage,
          sizeof(value.specialization_preimage), &digest_result) !=
      W_SEED_GENERIC_VALIDATION_INVALID)
    return false;
  print_nominal_validation_line("digest", &value, &digest_result,
                                predicate_digest);
  (void)memset(value.specialization_preimage, 0xa5,
               sizeof(value.specialization_preimage));
  w_seed_generic_validation_result truncated_result;
  if (validate_application_at_with_origin_view(
          &value, 0u, &truncated_view, value.specialization_preimage,
          sizeof(value.specialization_preimage), &truncated_result) !=
          W_SEED_GENERIC_VALIDATION_INVALID ||
      truncated_result.receipts_written != 0u ||
      truncated_result.evaluation.consumed_steps != 0u ||
      value.specialization_preimage[0] != 0xa5u ||
      value.specialization_preimage[sizeof(value.specialization_preimage) - 1u] !=
          0xa5u)
    return false;
  print_nominal_validation_line("truncated", &value, &truncated_result,
                                predicate_digest);
  w_seed_generic_nominal_origin_view relation_view = value.nominal_origin_view;
  relation_view.frontend_module_index += 1u;
  w_seed_generic_validation_result relation_result;
  if (validate_application_at_with_origin_view(
          &value, 0u, &relation_view, value.specialization_preimage,
          sizeof(value.specialization_preimage), &relation_result) !=
      W_SEED_GENERIC_VALIDATION_INVALID)
    return false;
  print_nominal_validation_line("process", &value, &relation_result,
                                predicate_digest);
  static const char evil_segment[] = "evil";
  static const char domain_segment[] = "domain";
  static const char wrong_declared_name[] = "Other";
  static const char wrong_owner_name[] = "Outer";
  const w_seed_frontend_text prefix_segments[] = {
      {evil_segment, sizeof(evil_segment) - 1u},
      {domain_segment, sizeof(domain_segment) - 1u}};
  const w_seed_frontend_text domain_segments[] = {
      {domain_segment, sizeof(domain_segment) - 1u}};
  const w_seed_generic_nominal_owner wrong_owner = {
      W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT,
      {wrong_owner_name, sizeof(wrong_owner_name) - 1u}};
  const w_seed_generic_nominal_origin relation_origins[] = {
      {authority_a, authority_a_length,
       {package_a, sizeof(package_a) - 1u}, prefix_segments, 2u,
       W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT, NULL, 0u,
       {declaration_name, sizeof(declaration_name) - 1u}},
      {authority_a, authority_a_length,
       {package_a, sizeof(package_a) - 1u}, domain_segments, 1u,
       W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT, NULL, 0u,
       {wrong_declared_name, sizeof(wrong_declared_name) - 1u}},
      {authority_a, authority_a_length,
       {package_a, sizeof(package_a) - 1u}, domain_segments, 1u,
       W_SEED_GENERIC_NOMINAL_DECLARATION_TYPE, NULL, 0u,
       {declaration_name, sizeof(declaration_name) - 1u}},
      {authority_a, authority_a_length,
       {package_a, sizeof(package_a) - 1u}, domain_segments, 1u,
       W_SEED_GENERIC_NOMINAL_DECLARATION_STRUCT, &wrong_owner, 1u,
       {declaration_name, sizeof(declaration_name) - 1u}},
  };
  const char *relation_origin_names[] = {"module", "head", "kind", "owner"};
  uint8_t relation_preimage[
      W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PREIMAGE_BYTES];
  for (size_t index = 0u; index < 4u; index += 1u) {
    w_seed_generic_nominal_origin_result relation_measure;
    w_seed_generic_nominal_origin_result relation_written;
    if (w_seed_generic_nominal_origin_measure(&relation_origins[index],
                                              &relation_measure) !=
            W_SEED_GENERIC_NOMINAL_ORIGIN_AVAILABLE ||
        w_seed_generic_nominal_origin_write(
            &relation_origins[index], relation_preimage,
            relation_measure.bytes_required, &relation_written) !=
            W_SEED_GENERIC_NOMINAL_ORIGIN_AVAILABLE)
      return false;
    const w_seed_generic_nominal_origin_view relation_origin_view = {
        relation_preimage, relation_written.bytes_written, relation_written.digest,
        value.nominal_origin_view.frontend_module_index,
        value.nominal_origin_view.frontend_head_struct_index};
    (void)memset(value.specialization_preimage, 0xa5,
                 sizeof(value.specialization_preimage));
    w_seed_generic_validation_result relation_origin_result;
    if (validate_application_at_with_origin_view(
            &value, 0u, &relation_origin_view, value.specialization_preimage,
            sizeof(value.specialization_preimage), &relation_origin_result) !=
            W_SEED_GENERIC_VALIDATION_INVALID ||
        relation_origin_result.receipts_written != 0u ||
        relation_origin_result.evaluation.consumed_steps != 0u ||
        value.specialization_preimage[0] != 0xa5u ||
        value.specialization_preimage[sizeof(value.specialization_preimage) - 1u] !=
            0xa5u)
      return false;
    print_nominal_validation_line(relation_origin_names[index], &value,
                                  &relation_origin_result, predicate_digest);
  }
  w_seed_generic_validation_result short_validation_result;
  if (validate_application_at_with_origin_view(
          &value, 0u, &value.nominal_origin_view, value.specialization_preimage,
          valid_result.specialization_bytes_required - 1u,
          &short_validation_result) != W_SEED_GENERIC_VALIDATION_VERIFIED)
    return false;
  print_nominal_validation_line("short", &value, &short_validation_result,
                                predicate_digest);
  w_seed_generic_validation_result zero_validation_result;
  if (validate_application_at_with_origin_view(
          &value, 0u, &value.nominal_origin_view, value.specialization_preimage,
          0u, &zero_validation_result) != W_SEED_GENERIC_VALIDATION_VERIFIED)
    return false;
  print_nominal_validation_line("zero", &value, &zero_validation_result,
                                predicate_digest);
  uint8_t collision_preimage[65536];
  (void)memcpy(collision_preimage, value.specialization_preimage,
               valid_result.specialization_bytes_written);
  collision_preimage[valid_result.specialization_bytes_written - 1u] ^= 0x01u;
  const w_seed_generic_specialization_view collision_left = {
      value.specialization_preimage, valid_result.specialization_bytes_written,
      valid_result.specialization_digest};
  const w_seed_generic_specialization_view collision_right = {
      collision_preimage, valid_result.specialization_bytes_written,
      valid_result.specialization_digest};
  (void)printf("SPECIALIZATION_COLLISION equal=%d\n",
               w_seed_generic_specialization_equal(&collision_left,
                                                    &collision_right) ? 1 : 0);
  return true;
}

static int nominal_test_hex_digit(char digit) {
  if (digit >= '0' && digit <= '9') return digit - '0';
  if (digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
  if (digit >= 'A' && digit <= 'F') return digit - 'A' + 10;
  return -1;
}

static bool nominal_test_decode_hex(const char *text, uint8_t *output,
                                    size_t output_capacity,
                                    size_t *output_length) {
  if (text == NULL || output == NULL || output_length == NULL) return false;
  const size_t text_length = strlen(text);
  if ((text_length & 1u) != 0u || text_length / 2u > output_capacity)
    return false;
  for (size_t index = 0u; index < text_length / 2u; index += 1u) {
    const int high = nominal_test_hex_digit(text[index * 2u]);
    const int low = nominal_test_hex_digit(text[index * 2u + 1u]);
    if (high < 0 || low < 0) return false;
    output[index] = (uint8_t)((high << 4) | low);
  }
  *output_length = text_length / 2u;
  return *output_length != 0u;
}

static bool nominal_test_configure_source_authority(
    const char *hex, uint8_t *storage, size_t storage_capacity) {
  size_t length = 0u;
  if (!nominal_test_decode_hex(hex, storage, storage_capacity, &length))
    return false;
  nominal_test_source_authority = storage;
  nominal_test_source_authority_length = length;
  return true;
}

int main(int argc, char **argv) {
  if (argc == 4 && strcmp(argv[1], "--nominal-origin-matrix") == 0) {
    uint8_t authority_a[W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PREIMAGE_BYTES];
    uint8_t authority_b[W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PREIMAGE_BYTES];
    size_t authority_a_length = 0u;
    size_t authority_b_length = 0u;
    if (!nominal_test_decode_hex(argv[2], authority_a, sizeof(authority_a),
                                 &authority_a_length) ||
        !nominal_test_decode_hex(argv[3], authority_b, sizeof(authority_b),
                                 &authority_b_length))
      return 1;
    nominal_test_source_authority = authority_a;
    nominal_test_source_authority_length = authority_a_length;
    return probe_nominal_origin_matrix(
               authority_a, authority_a_length, authority_b,
               authority_b_length)
               ? 0
               : 1;
  }
  if (argc == 4 && strcmp(argv[1], "--domain-witness") == 0) {
    uint8_t authority[W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PREIMAGE_BYTES];
    if (!nominal_test_configure_source_authority(
            argv[3], authority, sizeof(authority)))
      return 1;
    return probe_domain_file(argv[2], "restaurant") ? 0 : 1;
  }
  if (argc == 5 && strcmp(argv[1], "--domain-witness-module") == 0) {
    uint8_t authority[W_SEED_GENERIC_VALIDATION_NOMINAL_ORIGIN_MAX_PREIMAGE_BYTES];
    if (!nominal_test_configure_source_authority(
            argv[4], authority, sizeof(authority)))
      return 1;
    return probe_domain_file(argv[2], argv[3]) ? 0 : 1;
  }
  if (argc == 3 && strcmp(argv[1], "--domain-witness") == 0)
    return probe_domain_file(argv[2], "restaurant") ? 0 : 1;
  if (argc == 4 && strcmp(argv[1], "--domain-witness-module") == 0)
    return probe_domain_file(argv[2], argv[3]) ? 0 : 1;
  if (argc == 4 && strcmp(argv[1], "--domain-witness-quota") == 0) {
    char *end = NULL;
    const unsigned long long parsed = strtoull(argv[3], &end, 10);
    if (end == argv[3] || *end != '\0' || parsed > SIZE_MAX) return 1;
    return probe_domain_file_with_quota(argv[2], (size_t)parsed,
                                        "restaurant") ? 0 : 1;
  }
  if (argc == 5 && strcmp(argv[1], "--domain-witness-quota-module") == 0) {
    char *end = NULL;
    const unsigned long long parsed = strtoull(argv[3], &end, 10);
    if (end == argv[3] || *end != '\0' || parsed > SIZE_MAX) return 1;
    return probe_domain_file_with_quota(argv[2], (size_t)parsed, argv[4]) ? 0 : 1;
  }
  if (argc == 3 && strcmp(argv[1], "--domain-witness-corrupt") == 0)
    return probe_domain_file_corrupt(argv[2]) ? 0 : 1;
  if (argc == 3 && strcmp(argv[1], "--domain-witness-d3-corrupt") == 0)
    return probe_typed_const_corrupt(argv[2]) ? 0 : 1;
  if (argc == 3 && strcmp(argv[1], "--domain-witness-d4-corrupt") == 0)
    return probe_module_const_corrupt(argv[2]) ? 0 : 1;
  if (argc == 3 && strcmp(argv[1], "--domain-witness-d4-zero-capacity") == 0)
    return probe_module_const_zero_capacity(argv[2]) ? 0 : 1;
  if (argc != 1) return 1;
  if (!test_verified_and_rejected_paths() ||
      !test_quota_unsupported_and_invalid() ||
      !test_direct_scalar_predicates() ||
      !test_typed_const_expression_predicates() ||
      !test_predicate_session_isolation() ||
      !test_preflight_capacity_precedence() ||
      !test_dependent_effective_domains() ||
      !test_string_predicate_conversion_boundary() ||
      !test_fingerprint_adversarial_inputs() ||
      !test_named_module_const_d4() ||
      !test_specialization_contract() ||
      !test_nominal_origin_contract())
    return 1;
  return 0;
}
