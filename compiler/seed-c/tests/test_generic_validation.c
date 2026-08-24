#include "w_seed_generic_validation.h"

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
  CONST_VALUES = 8192,
  CONST_ELEMENTS = 16384,
  CONST_BYTES = 65536,
  ENUMS = 32,
  ENUM_CASES = 256,
  ENUM_CASE_PARAMETERS = 256,
  ENUM_SUBSET_MEMBERS = 512,
  FIELDS = 256,
  DECLARATIONS = 64,
  STATEMENTS = 8192,
  EXPRESSIONS = 32768,
  ARGUMENTS = 8192,
  SWITCH_ARMS = 8192,
  MEMBERSHIP_CASES = 32768,
  SYMBOLS = 4096,
  FACTS = 4096,
  DIAGNOSTICS = 2048,
  FRONTEND_RECEIPT = 4 * 1024 * 1024,
  CONSTIR_FUNCTIONS = 128,
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
  w_seed_frontend_generic_parameter generic_parameters[GENERIC_PARAMETERS];
  w_seed_frontend_generic_application generic_applications[GENERIC_APPLICATIONS];
  w_seed_frontend_generic_argument generic_arguments[GENERIC_ARGUMENTS];
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
  w_seed_generic_validation_receipt receipts[VALIDATION_RECEIPTS];
  w_seed_constir_eval_frame eval_frames[EVAL_FRAMES];
} fixture;

static fixture value;

static void fixture_init_outputs(fixture *fixture_value) {
  fixture_value->frontend_input =
      (w_seed_frontend_input){&fixture_value->document, 1u, NULL, 0u};
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
      {module_id, strlen(module_id)}, {module_id, strlen(module_id)},
      &fixture_value->source,
      fixture_value->cst_nodes, fixture_value->parse.node_count,
      fixture_value->parse};
  fixture_init_outputs(fixture_value);
  const w_seed_frontend_status frontend_status = w_seed_frontend_run(
      &fixture_value->frontend_input, &fixture_value->frontend_output,
      &fixture_value->frontend_result);
  CHECK(frontend_status == W_SEED_FRONTEND_OK ||
        frontend_status == W_SEED_FRONTEND_UNSUPPORTED);
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
                                   "generic-test");
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

static w_seed_generic_validation_state validate_application_at_with_evidence(
    fixture *fixture_value, uint32_t application_index,
    size_t conversion_capacity, uint8_t *evidence_bytes,
    size_t evidence_byte_capacity, w_seed_constir_quota quota,
    w_seed_generic_validation_result *result) {
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
      .receipt_capacity = VALIDATION_RECEIPTS};
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

static bool fingerprint_not_available(const w_seed_generic_validation_result *result) {
  return result != NULL &&
         result->fingerprint_state ==
             W_SEED_GENERIC_VALIDATION_FINGERPRINT_NOT_AVAILABLE &&
         memcmp(result->fingerprint_digest,
                (uint8_t[W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES]){0},
                W_SEED_GENERIC_VALIDATION_FINGERPRINT_BYTES) == 0;
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

static bool probe_domain_file(const char *path) {
  if (path == NULL) return false;
  FILE *file = fopen(path, "rb");
  if (file == NULL) return false;
  char source[SOURCE_BYTES];
  const size_t length = fread(source, 1u, sizeof(source) - 1u, file);
  const bool read_error = ferror(file) != 0;
  const int close_status = fclose(file);
  const bool read_ok = !read_error && close_status == 0;
  if (!read_ok || length == sizeof(source) - 1u) return false;
  source[length] = '\0';
  if (!fixture_lower_with_module(&value, source, "restaurant")) return false;

  size_t stage_path_count = 0u;
  printf("GENERIC_RESULT applications=%llu frontend=%d constir=%d\n",
         (unsigned long long)value.frontend_result.written.generic_applications,
         (int)value.frontend_result.status, (int)value.constir_result.status);
  for (size_t application_index = 0u;
       application_index < value.frontend_result.written.generic_applications;
       application_index += 1u) {
    const w_seed_frontend_generic_application *application =
        &value.generic_applications[application_index];
    if (application->head_name.length != 9u ||
        memcmp(application->head_name.data, "StagePath", 9u) != 0)
      continue;
    w_seed_generic_validation_result result;
    const w_seed_generic_validation_state state = validate_application_at(
        &value, (uint32_t)application_index, CONVERSION_VALUES,
        (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result);
    uint8_t predicate_digest[32] = {0u};
    (void)predicate_body_digest_for_application(&value, application,
                                                predicate_digest);
    printf("GENERIC app=%llu state=%s failure=%s diagnostic=%d predicates=%llu "
           "receipts=%llu steps=%llu module=%.*s head=%.*s "
           "fingerprint_state=%s fingerprint_digest=",
           (unsigned long long)application_index,
           w_seed_generic_validation_state_name(state),
           w_seed_generic_validation_failure_name(result.failure),
           (int)result.diagnostic,
           (unsigned long long)result.predicate_count,
           (unsigned long long)result.receipts_written,
           (unsigned long long)result.evaluation.consumed_steps,
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
    (void)printf(" predicate_body_digest=");
    print_digest(predicate_digest);
    (void)printf("\n");
    stage_path_count += 1u;
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
        (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX}, &result);
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
    (void)printf("\n");
    static_value_count += 1u;
  }
  return stage_path_count != 0u || static_value_count != 0u;
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
        result.receipts_written == 1u && !value.receipts[0].bool_value);
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
        result.receipts_written == 0u && fingerprint_not_available(&result));

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
                             &result) == W_SEED_GENERIC_VALIDATION_UNSUPPORTED &&
        result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_BINDING &&
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
      "const fn acceptsLabel(value: String): Bool { return true }\n"
      "struct StringBox<_ value: String<(acceptsLabel(.member))>> {}\n"
      "struct Use { item: StringBox<\"The final seating\"> }\n";
  w_seed_generic_validation_result result;
  w_seed_constir_status constir_status = W_SEED_CONSTIR_OK;
  CHECK(fixture_lower_base(&value, source, "generic-test", &constir_status));
  CHECK(constir_status == W_SEED_CONSTIR_INVALID);
  CHECK(value.frontend_result.written.generic_applications == 1u);
  CHECK(validate_application(&value, CONVERSION_VALUES,
                             (w_seed_constir_quota){100000u, 0u, 64u, SIZE_MAX},
                             &result) == W_SEED_GENERIC_VALIDATION_UNSUPPORTED &&
        (result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_VALUE ||
         result.failure == W_SEED_GENERIC_VALIDATION_FAILURE_FUNCTION) &&
        result.receipts_written == 0u && result.evaluation.consumed_steps == 0u &&
        fingerprint_not_available(&result));
  /* This source-backed String witness proves the D1 conversion boundary. A
   * non-lowerable predicate may report FUNCTION before value conversion. */
  return true;
}

static bool test_fingerprint_adversarial_inputs(void) {
  static const char always_source[] =
      "const fn always(value: Bool): Bool { return true }\n"
      "struct Box<_ value: Bool<(always(.member))>> {}\n"
      "struct Use { item: Box<true> }\n";
  static const char always_spaced_source[] =
      "\n// formatting does not change the semantic projection\n"
      "const fn always(value: Bool): Bool { return true }\n\n"
      "struct Box<_ value: Bool<(always(.member))>> {}\n"
      "// optional labels remain outside the semantic projection\n"
      "struct Use { item: Box<true> }\n";
  static const char always_prefixed_source[] =
      "struct Unrelated {}\n"
      "const fn always(value: Bool): Bool { return true }\n"
      "struct Box<_ value: Bool<(always(.member))>> {}\n"
      "struct Use { item: Box<true> }\n";
  static const char always_label_source[] =
      "const fn always(value: Bool): Bool { return true }\n"
      "struct Box<_ value: Bool<(always(.member))>> {}\n"
      "struct Use { item: Box<value: true> }\n";
  static const char other_head_source[] =
      "const fn always(value: Bool): Bool { return true }\n"
      "struct OtherBox<_ value: Bool<(always(.member))>> {}\n"
      "struct Use { item: OtherBox<true> }\n";
  static const char list_source[] =
      "enum Color { red green }\n"
      "const fn alwaysList(value: StaticList<Color>): Bool { return true }\n"
      "struct ListBox<_ value: StaticList<Color><(alwaysList(.member))>> {}\n"
      "struct Use { item: ListBox<[.red, .green]> }\n";
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
      "struct IntBox<_ value: u8<(alwaysInt(.member))>> {}\n"
      "struct Use { item: IntBox<7> }\n";
  static const char same_head_integer_source[] =
      "const fn always(value: u8): Bool { return true }\n"
      "struct Box<_ value: u8<(always(.member))>> {}\n"
      "struct Use { item: Box<7> }\n";
  static const char enum_source[] =
      "enum Color { red green blue }\n"
      "const fn isRed(value: Color): Bool { return true }\n"
      "struct ColorBox<_ value: Color<(isRed(.member))>> {}\n"
      "struct Use { item: ColorBox<.red> }\n";
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

int main(int argc, char **argv) {
  if (argc == 3 && strcmp(argv[1], "--domain-witness") == 0)
    return probe_domain_file(argv[2]) ? 0 : 1;
  if (argc != 1) return 1;
  if (!test_verified_and_rejected_paths() ||
      !test_quota_unsupported_and_invalid() ||
      !test_direct_scalar_predicates() ||
      !test_dependent_effective_domains() ||
      !test_string_predicate_conversion_boundary() ||
      !test_fingerprint_adversarial_inputs())
    return 1;
  return 0;
}
