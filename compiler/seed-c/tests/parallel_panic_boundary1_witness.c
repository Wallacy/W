#include "parallel_panic_boundary1_witness.h"

#include <string.h>

static const char WITNESS_SOURCE[] =
    "enum Failure: Error { denied }\n"
    "fn clean() { }\n"
    "fn succeed(): i64 { return 42 }\n"
    "fn successCaller(): i64 { return succeed() }\n"
    "fn leaf(): i64 throws Failure { throw .denied }\n"
    "fn relay(): i64 throws Failure { defer { clean() } return try leaf() }\n"
    "entry { }\n";

static bool text_is(const w_seed_hir0_program *program, w_seed_hir0_text text,
                    const char *expected) {
  if (program == NULL || expected == NULL ||
      text.offset > program->text_byte_capacity ||
      text.count > program->text_byte_capacity - text.offset)
    return false;
  const size_t expected_count = strlen(expected);
  return text.count == expected_count &&
         memcmp(program->text_bytes + text.offset, expected, expected_count) ==
             0;
}

static uint32_t function_named(const w_seed_hir0_program *program,
                               const char *name) {
  if (program == NULL || name == NULL) return W_SEED_HIR0_NONE;
  for (size_t index = 0u; index < program->function_count; index += 1u)
    if (text_is(program, program->functions[index].name, name))
      return (uint32_t)index;
  return W_SEED_HIR0_NONE;
}

bool w_seed_parallel_panic_boundary1_provider_invoke(
    void *raw, size_t task_index,
    w_seed_parallel_platform1_completion *completion) {
  w_seed_parallel_panic_boundary1_provider_context *context =
      (w_seed_parallel_panic_boundary1_provider_context *)raw;
  if (context == NULL || completion == NULL || task_index >= 2u || context->fail)
    return false;
  context->calls[task_index] += 1u;
  *completion = context->requested[task_index];
  return true;
}

static bool fixture_parse(w_seed_parallel_panic_boundary1_witness *witness) {
  if (witness == NULL || strlen(WITNESS_SOURCE) >= sizeof(witness->source_bytes))
    return false;
  witness->source_length = strlen(WITNESS_SOURCE);
  (void)memcpy(witness->source_bytes, WITNESS_SOURCE,
               witness->source_length);
  w_seed_source_error source_error;
  if (!w_seed_source_init(
          (w_seed_byte_view){witness->source_bytes, witness->source_length},
          &witness->source, &source_error))
    return false;
  w_seed_lex_error lex_error;
  if (!w_seed_parser_init(
          &witness->source, (w_seed_span){0u, witness->source_length},
          (w_seed_foreign_limits){65536u, 256u}, witness->lexer_frames,
          W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_LEXER_FRAMES,
          witness->tokens, W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_TOKENS,
          witness->nodes, W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_NODES,
          witness->parse_frames,
          W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_PARSE_FRAMES, witness->issues,
          W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_ISSUES, &witness->parser,
          &lex_error) ||
      !w_seed_parser_parse(&witness->parser, &witness->parse))
    return false;
  witness->document = (w_seed_frontend_document){
      .logical_source_id = (w_seed_frontend_text){"panic-boundary-test", 19u},
      .module_id = (w_seed_frontend_text){"panic-boundary-test", 19u},
      .local_module_name =
          (w_seed_frontend_text){"panic-boundary-test", 19u},
      .source = &witness->source,
      .nodes = witness->nodes,
      .node_count = witness->parse.node_count,
      .parse = witness->parse};
  witness->frontend_input = (w_seed_frontend_input){
      .documents = &witness->document,
      .document_count = 1u,
      .external_modules = NULL,
      .external_module_count = 0u,
      .host_scope = NULL,
      .import_resolution_complete = false,
      .resolved_imports = NULL,
      .resolved_import_count = 0u};
  witness->frontend_output = (w_seed_frontend_output){
      .modules = witness->modules,
      .module_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_MODULES,
      .imports = witness->imports,
      .import_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_IMPORTS,
      .import_items = witness->import_items,
      .import_item_capacity =
          W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_IMPORT_ITEMS,
      .structs = witness->structs,
      .struct_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_STRUCTS,
      .fields = witness->fields,
      .field_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_FIELDS,
      .enums = witness->enums,
      .enum_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_ENUMS,
      .enum_cases = witness->enum_cases,
      .enum_case_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_ENUM_CASES,
      .enum_case_parameters = witness->enum_case_parameters,
      .enum_case_parameter_capacity =
          W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_ENUM_CASE_PARAMETERS,
      .enum_subset_members = witness->enum_subset_members,
      .enum_subset_member_capacity =
          W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .type_declarations = witness->type_declarations,
      .type_declaration_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_STRUCTS,
      .aliases = witness->aliases,
      .alias_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_STRUCTS,
      .types = witness->types,
      .type_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_TYPES,
      .functions = witness->functions,
      .function_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_FUNCTIONS,
      .parameters = witness->parameters,
      .parameter_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_PARAMETERS,
      .kernel_modules = NULL,
      .kernel_module_capacity = 0u,
      .kernel_bindings = NULL,
      .kernel_binding_capacity = 0u,
      .entries = witness->entries,
      .entry_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_ENTRIES,
      .statements = witness->statements,
      .statement_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_STATEMENTS,
      .expressions = witness->expressions,
      .expression_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_EXPRESSIONS,
      .arguments = witness->arguments,
      .argument_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_ARGUMENTS,
      .switch_arms = witness->switch_arms,
      .switch_arm_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_SWITCH_ARMS,
      .pattern_captures = witness->pattern_captures,
      .pattern_capture_capacity =
          W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_SWITCH_ARMS,
      .interpolation_segments = witness->interpolation_segments,
      .interpolation_segment_capacity =
          W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_INTERPOLATION_SEGMENTS,
      .symbols = witness->symbols,
      .symbol_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_SYMBOLS,
      .facts = witness->facts,
      .fact_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_FACTS,
      .diagnostics = witness->diagnostics,
      .diagnostic_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_DIAGNOSTICS,
      .diagnostic_facts = witness->diagnostic_facts,
      .diagnostic_fact_capacity =
          W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_DIAGNOSTICS * 5u,
      .diagnostic_items = witness->diagnostic_items,
      .diagnostic_item_capacity =
          W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_DIAGNOSTICS * 4u,
      .diagnostic_labels = witness->diagnostic_labels,
      .diagnostic_label_capacity =
          W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_DIAGNOSTICS * 2u,
      .receipt = witness->frontend_receipt,
      .receipt_capacity = sizeof(witness->frontend_receipt),
      .const_bytes = witness->const_bytes,
      .const_bytes_capacity = sizeof(witness->const_bytes)};
  return true;
}

static void configure_host(w_seed_parallel_panic_boundary1_witness *witness) {
  witness->host_requirements[0] = (w_seed_frontend_host_requirement){
      .name = (w_seed_frontend_text){"Console", 7u}};
  witness->host_parameters[0] = (w_seed_frontend_external_parameter){
      .name = (w_seed_frontend_text){"message", 7u},
      .type = (w_seed_frontend_text){"String", 6u},
      .label_kind = W_SEED_FRONTEND_LABEL_REQUIRED};
  witness->host_parameters[1] = (w_seed_frontend_external_parameter){
      .name = (w_seed_frontend_text){"suffix", 6u},
      .type = (w_seed_frontend_text){"String", 6u},
      .label_kind = W_SEED_FRONTEND_LABEL_REQUIRED};
  witness->host_symbols[0] = (w_seed_frontend_host_prelude_symbol){
      .name = (w_seed_frontend_text){"noop", 4u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"()", 2u},
      .is_const = false,
      .requirements = NULL,
      .requirement_count = 0u};
  witness->host_symbols[1] = (w_seed_frontend_host_prelude_symbol){
      .name = (w_seed_frontend_text){"print", 5u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .parameters = witness->host_parameters,
      .parameter_count = 2u,
      .return_type = (w_seed_frontend_text){"()", 2u},
      .is_const = false,
      .requirements = witness->host_requirements,
      .requirement_count = 1u};
  witness->host_scope = (w_seed_frontend_host_prelude){
      .profile = (w_seed_frontend_text){"native-process@1", 16u},
      .symbols = witness->host_symbols,
      .symbol_count = 2u};
  witness->frontend_input.host_scope = &witness->host_scope;
}

static void setup_hir_output(w_seed_parallel_panic_boundary1_witness *witness) {
  witness->hir_output = (w_seed_hir0_output){
      .modules = witness->hir_modules,
      .module_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .identities = witness->hir_identities,
      .identity_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_IDENTITIES,
      .types = witness->hir_types,
      .type_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .enums = witness->hir_enums,
      .enum_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .enum_cases = witness->hir_enum_cases,
      .enum_case_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .enum_case_parameters = witness->hir_enum_case_parameters,
      .enum_case_parameter_capacity =
          W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .enum_subset_members = witness->hir_enum_subset_members,
      .enum_subset_member_capacity =
          W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .functions = witness->hir_functions,
      .function_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .parameters = witness->hir_parameters,
      .parameter_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .blocks = witness->hir_blocks,
      .block_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .block_arguments = witness->hir_block_arguments,
      .block_argument_capacity =
          W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .edge_arguments = witness->hir_edge_arguments,
      .edge_argument_capacity =
          W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .switch_edges = witness->hir_switch_edges,
      .switch_edge_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .switch_captures = witness->hir_switch_captures,
      .switch_capture_capacity =
          W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .instructions = witness->hir_instructions,
      .instruction_capacity =
          W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .bindings = witness->hir_bindings,
      .binding_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .calls = witness->hir_calls,
      .call_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .host_parameters = witness->hir_host_parameters,
      .host_parameter_capacity =
          W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .arguments = witness->hir_arguments,
      .argument_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .enum_payloads = witness->hir_enum_payloads,
      .enum_payload_capacity =
          W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .requirements = witness->hir_requirements,
      .requirement_capacity =
          W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .values = witness->hir_values,
      .value_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .interpolation_segments = witness->hir_interpolation_segments,
      .interpolation_segment_capacity =
          W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .terminators = witness->hir_terminators,
      .terminator_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .entries = witness->hir_entries,
      .entry_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS,
      .text_bytes = witness->hir_text,
      .text_byte_capacity = sizeof(witness->hir_text),
      .value_bytes = witness->hir_value_bytes,
      .value_byte_capacity = sizeof(witness->hir_value_bytes),
      .receipt = witness->hir_receipt,
      .receipt_capacity = sizeof(witness->hir_receipt),
      .external_modules = witness->hir_external_modules,
      .external_module_capacity = 2u,
      .external_symbols = witness->hir_external_symbols,
      .external_symbol_capacity = 8u,
      .cleanups = witness->hir_cleanups,
      .cleanup_capacity = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEST_HIR_RECORDS};
}

bool w_seed_parallel_panic_boundary1_witness_init(
    w_seed_parallel_panic_boundary1_witness *witness) {
  if (witness == NULL) return false;
  (void)memset(witness, 0, sizeof(*witness));
  if (!fixture_parse(witness)) return false;
  configure_host(witness);
  if (w_seed_frontend_run(&witness->frontend_input, &witness->frontend_output,
                          &witness->frontend_result) != W_SEED_FRONTEND_OK)
    return false;
  setup_hir_output(witness);
  const w_seed_hir0_input hir_input = {
      .frontend_input = &witness->frontend_input,
      .frontend_output = &witness->frontend_output,
      .frontend_result = &witness->frontend_result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  w_seed_hir0_counts measured_counts;
  w_seed_hir0_result measured_result;
  if (w_seed_hir0_measure(&hir_input, &measured_counts, &measured_result) !=
          W_SEED_HIR0_OK ||
      w_seed_hir0_run(&hir_input, &witness->hir_output, &witness->hir_result) !=
          W_SEED_HIR0_OK ||
      !w_seed_hir0_program_from_output(&witness->hir_output,
                                       &witness->hir_result,
                                       &witness->hir_program) ||
      !w_seed_hir0_verify(&witness->hir_program, &witness->hir_result))
    return false;
  witness->hir_counts = measured_counts;

  const uint32_t success_caller =
      function_named(&witness->hir_program, "successCaller");
  const uint32_t relay = function_named(&witness->hir_program, "relay");
  if (success_caller == W_SEED_HIR0_NONE || relay == W_SEED_HIR0_NONE ||
      success_caller >= witness->hir_program.function_count ||
      relay >= witness->hir_program.function_count)
    return false;
  const w_seed_hir0_function *success_function =
      &witness->hir_program.functions[success_caller];
  const w_seed_hir0_function *relay_function =
      &witness->hir_program.functions[relay];
  if (success_function->block_count != 1u || relay_function->block_count != 3u ||
      success_function->first_block >= witness->hir_program.block_count ||
      relay_function->first_block >= witness->hir_program.block_count)
    return false;
  const w_seed_hir0_block *success_block =
      &witness->hir_program.blocks[success_function->first_block];
  if (success_block->instruction_count != 1u ||
      success_block->first_instruction >= witness->hir_program.instruction_count)
    return false;
  const w_seed_hir0_instruction *success_instruction =
      &witness->hir_program.instructions[success_block->first_instruction];
  const uint32_t success_call = success_instruction->call_index;
  const uint32_t invoke_terminator =
      witness->hir_program.blocks[relay_function->first_block].terminator_index;
  if (success_call >= witness->hir_program.call_count ||
      invoke_terminator >= witness->hir_program.terminator_count ||
      witness->hir_program.terminators[invoke_terminator].kind !=
          W_SEED_HIR0_TERMINATOR_INVOKE)
    return false;
  const uint32_t error_call =
      witness->hir_program.terminators[invoke_terminator].call_index;
  if (error_call >= witness->hir_program.call_count || success_call == error_call)
    return false;

  witness->tasks[0] = (w_seed_parallel_typed_binding1_task){0u, success_call};
  witness->tasks[1] = (w_seed_parallel_typed_binding1_task){1u, error_call};
  witness->provider_context.requested[0] =
      (w_seed_parallel_platform1_completion){
          .kind = W_SEED_PARALLEL_PLATFORM1_COMPLETION_SUCCESS,
          .success_value = 42};
  witness->provider_context.requested[1] =
      (w_seed_parallel_platform1_completion){
          .kind = W_SEED_PARALLEL_PLATFORM1_COMPLETION_PANIC,
          .panic_code = W_SEED_PARALLEL_PLATFORM1_PANIC_EXPLICIT};
  witness->provider = (w_seed_parallel_typed_binding1_provider){
      .target = W_SEED_PARALLEL_TYPED_BINDING1_TARGET_WINDOWS_AMD64};
  (void)memcpy(witness->provider.profile,
               W_SEED_PARALLEL_TYPED_BINDING1_WINDOWS_PROFILE,
               sizeof(W_SEED_PARALLEL_TYPED_BINDING1_WINDOWS_PROFILE));
  (void)memcpy(witness->provider.identity,
               W_SEED_PARALLEL_TYPED_BINDING1_WINDOWS_IDENTITY,
               sizeof(W_SEED_PARALLEL_TYPED_BINDING1_WINDOWS_IDENTITY));
  if (!w_seed_parallel_local_provider1_open(
          W_SEED_PARALLEL_LOCAL_PROVIDER1_WINDOWS_AMD64,
          &witness->authority) ||
      !w_seed_parallel_local_provider1_verify(&witness->authority))
    return false;
  witness->typed_input = (w_seed_parallel_typed_binding1_input){
      .hir_program = &witness->hir_program,
      .hir_result = &witness->hir_result,
      .invoke_terminator = invoke_terminator,
      .tasks = witness->tasks,
      .task_count = 2u,
      .task_capacity = 2u,
      .provider_job = {w_seed_parallel_panic_boundary1_provider_invoke,
                       &witness->provider_context,
                       sizeof(witness->provider_context)},
      .provider_authority = &witness->authority,
      .provider_capacity = 2u,
      .generation = 41u,
      .provider = witness->provider};
  return true;
}
