#include "w_seed_parallel_elision0.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  uintptr_t start;
  uintptr_t end;
  bool active;
} elision_range;

static bool range_make(const void *pointer, size_t count, size_t element_size,
                       elision_range *range) {
  if (range == NULL) return false;
  *range = (elision_range){0u, 0u, false};
  if (count == 0u) return true;
  if (pointer == NULL || element_size == 0u || count > SIZE_MAX / element_size)
    return false;
  const size_t bytes = count * element_size;
  const uintptr_t start = (uintptr_t)pointer;
  if (start > UINTPTR_MAX - bytes) return false;
  *range = (elision_range){start, start + bytes, true};
  return true;
}

static bool ranges_overlap(elision_range left, elision_range right) {
  return left.active && right.active && left.start < right.end &&
         right.start < left.end;
}

/* Called only after HIR and PARSEL0 verification. */
static bool certificate_aliases_input(
    const w_seed_parallel_elision0_certificate *certificate,
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection) {
  elision_range output;
  elision_range input;
  if (!range_make(certificate, 1u, sizeof(*certificate), &output) ||
      !range_make(program, 1u, sizeof(*program), &input) ||
      ranges_overlap(output, input) ||
      !range_make(hir_result, 1u, sizeof(*hir_result), &input) ||
      ranges_overlap(output, input) ||
      !range_make(selection, 1u, sizeof(*selection), &input) ||
      ranges_overlap(output, input))
    return true;
#define W_PARELIDE0_REJECT_OVERLAP(pointer, capacity)                        \
  do {                                                                        \
    if (!range_make((pointer), (capacity), sizeof(*(pointer)), &input) ||      \
        ranges_overlap(output, input))                                         \
      return true;                                                             \
  } while (0)
  W_PARELIDE0_REJECT_OVERLAP(program->modules, program->module_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->identities, program->identity_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->types, program->type_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->enums, program->enum_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->enum_cases, program->enum_case_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->enum_case_parameters,
                             program->enum_case_parameter_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->enum_subset_members,
                             program->enum_subset_member_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->functions, program->function_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->parameters, program->parameter_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->blocks, program->block_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->block_arguments,
                             program->block_argument_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->edge_arguments,
                             program->edge_argument_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->switch_edges,
                             program->switch_edge_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->switch_captures,
                             program->switch_capture_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->instructions,
                             program->instruction_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->bindings, program->binding_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->calls, program->call_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->host_parameters,
                             program->host_parameter_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->arguments, program->argument_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->enum_payloads,
                             program->enum_payload_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->requirements,
                             program->requirement_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->values, program->value_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->interpolation_segments,
                             program->interpolation_segment_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->terminators,
                             program->terminator_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->entries, program->entry_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->text_bytes, program->text_byte_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->value_bytes,
                             program->value_byte_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->receipt, program->receipt_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->external_modules,
                             program->external_module_capacity);
  W_PARELIDE0_REJECT_OVERLAP(program->external_symbols,
                             program->external_symbol_capacity);
#undef W_PARELIDE0_REJECT_OVERLAP
  return false;
}

static bool bounded_range(uint32_t first, uint32_t count, size_t total) {
  return first != W_SEED_HIR0_NONE && first <= total && count <= total - first;
}

static bool i64_type(const w_seed_hir0_program *program, uint32_t type_index) {
  return program != NULL && type_index < program->type_count &&
         program->types[type_index].kind == W_SEED_HIR0_TYPE_I64;
}

static bool closed_pure_function(
    const w_seed_hir0_program *program, uint32_t function_index,
    uint8_t state[W_SEED_HIR0_COOPERATIVE_MAX_FUNCTIONS],
    uint32_t *reachable_count) {
  if (program == NULL || state == NULL || reachable_count == NULL ||
      function_index >= program->function_count ||
      function_index >= W_SEED_HIR0_COOPERATIVE_MAX_FUNCTIONS)
    return false;
  if (state[function_index] == 2u) return true;
  if (state[function_index] == 1u) return false;
  const w_seed_hir0_function *function = &program->functions[function_index];
  if (function->is_async || function->is_const || function->is_throws ||
      function->is_unsafe || function->has_borrow_clause ||
      function->is_anonymous_entry ||
      function->suspension != W_SEED_HIR0_SUSPENSION_NEVER ||
      !i64_type(program, function->return_type) ||
      (function->parameter_count != 0u &&
       !bounded_range(function->first_parameter, function->parameter_count,
                      program->parameter_count)) ||
      !bounded_range(function->first_block, function->block_count,
                     program->block_count))
    return false;
  for (size_t parameter = 0u; parameter < function->parameter_count;
       parameter += 1u)
    if (!i64_type(program,
                  program->parameters[(size_t)function->first_parameter +
                                      parameter]
                      .type_index))
      return false;
  state[function_index] = 1u;
  for (size_t block_ordinal = 0u; block_ordinal < function->block_count;
       block_ordinal += 1u) {
    const uint32_t block_index = function->first_block + (uint32_t)block_ordinal;
    const w_seed_hir0_block *block = &program->blocks[block_index];
    if (block->owner_function != function_index ||
        !bounded_range(block->first_instruction, block->instruction_count,
                       program->instruction_count))
      return false;
    for (size_t ordinal = 0u; ordinal < block->instruction_count;
         ordinal += 1u) {
      const w_seed_hir0_instruction *instruction =
          &program->instructions[(size_t)block->first_instruction + ordinal];
      if (instruction->owner_block != block_index ||
          instruction->kind == W_SEED_HIR0_INSTRUCTION_EXECUTION_YIELD)
        return false;
      if (instruction->kind == W_SEED_HIR0_INSTRUCTION_BINDING) continue;
      if (instruction->kind != W_SEED_HIR0_INSTRUCTION_CALL) return false;
      if (instruction->call_index >= program->call_count) return false;
      const w_seed_hir0_call *call = &program->calls[instruction->call_index];
      if (call->execution_kind != W_SEED_HIR0_CALL_DIRECT ||
          call->callee_identity >= program->identity_count)
        return false;
      const w_seed_hir0_identity *callee =
          &program->identities[call->callee_identity];
      if (callee->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
          callee->target_index >= program->function_count ||
          program->functions[callee->target_index].module_index !=
              function->module_index ||
          !closed_pure_function(program, callee->target_index, state,
                                reachable_count))
        return false;
    }
  }
  state[function_index] = 2u;
  if (*reachable_count == UINT32_MAX) return false;
  *reachable_count += 1u;
  return true;
}

static bool derive(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection,
    w_seed_parallel_elision0_certificate *certificate) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      certificate == NULL || selection->task_count != 1u ||
      program->function_count > W_SEED_HIR0_COOPERATIVE_MAX_FUNCTIONS ||
      program->function_count > UINT32_MAX ||
      program->instruction_count > UINT32_MAX ||
      program->binding_count > UINT32_MAX || program->call_count > UINT32_MAX ||
      program->value_count > UINT32_MAX)
    return false;

  const uint32_t call_index = selection->task_call_indices[0];
  const uint32_t function_index = selection->task_function_indices[0];
  const uint32_t launch_index = selection->launch_binding_indices[0];
  const uint32_t join_index = selection->join_binding_indices[0];
  if (call_index >= program->call_count ||
      function_index >= program->function_count ||
      launch_index >= program->binding_count ||
      join_index >= program->binding_count)
    return false;
  const w_seed_hir0_call *call = &program->calls[call_index];
  const w_seed_hir0_binding *launch = &program->bindings[launch_index];
  const w_seed_hir0_binding *join = &program->bindings[join_index];
  if (call->execution_kind !=
          W_SEED_HIR0_CALL_STRUCTURED_ASYNC_PARALLEL_DOMAIN_DISPATCH ||
      launch->task_role != W_SEED_HIR0_TASK_ROLE_LAUNCH ||
      join->task_role != W_SEED_HIR0_TASK_ROLE_AWAIT_RESULT ||
      launch->task_peer_binding != join_index ||
      join->task_peer_binding != launch_index ||
      call->owner_block != launch->owner_block ||
      launch->owner_block != join->owner_block ||
      call->owner_instruction == W_SEED_HIR0_NONE ||
      launch->owner_instruction != call->owner_instruction + 1u ||
      join->owner_instruction != launch->owner_instruction + 1u ||
      launch->initializer_value >= program->value_count ||
      join->initializer_value >= program->value_count)
    return false;
  const w_seed_hir0_function *task_function =
      &program->functions[function_index];
  if (!i64_type(program, call->result_type) ||
      !i64_type(program, task_function->return_type) ||
      call->argument_count != task_function->parameter_count ||
      call->argument_count > W_SEED_PARALLEL_ELISION0_MAX_ARGUMENTS ||
      (call->argument_count != 0u &&
       (!bounded_range(call->first_argument, call->argument_count,
                       program->argument_count) ||
        !bounded_range(task_function->first_parameter,
                       task_function->parameter_count,
                       program->parameter_count))))
    return false;
  bool seen_parameters[W_SEED_PARALLEL_ELISION0_MAX_ARGUMENTS] = {false};
  for (size_t ordinal = 0u; ordinal < call->argument_count; ordinal += 1u) {
    const w_seed_hir0_argument *argument =
        &program->arguments[(size_t)call->first_argument + ordinal];
    if (argument->owner_call != call_index ||
        argument->ordinal != ordinal ||
        argument->parameter_ordinal >= call->argument_count ||
        seen_parameters[argument->parameter_ordinal] ||
        argument->value_index >= program->value_count ||
        !i64_type(program, argument->type_index) ||
        !i64_type(program, program->values[argument->value_index].type_index) ||
        !i64_type(program,
                  program->parameters[(size_t)task_function->first_parameter +
                                      argument->parameter_ordinal]
                      .type_index))
      return false;
    seen_parameters[argument->parameter_ordinal] = true;
  }
  const w_seed_hir0_value *launch_value =
      &program->values[launch->initializer_value];
  const w_seed_hir0_value *join_value = &program->values[join->initializer_value];
  if (launch_value->kind != W_SEED_HIR0_VALUE_CALL_RESULT ||
      launch_value->call_index != call_index ||
      join_value->kind != W_SEED_HIR0_VALUE_BINDING_READ ||
      join_value->binding_index != launch_index)
    return false;
  size_t launch_reads = 0u;
  for (size_t value = 0u; value < program->value_count; value += 1u)
    if (program->values[value].kind == W_SEED_HIR0_VALUE_BINDING_READ &&
        program->values[value].binding_index == launch_index) {
      if (value != join->initializer_value) return false;
      launch_reads += 1u;
    }
  if (launch_reads != 1u) return false;

  uint8_t state[W_SEED_HIR0_COOPERATIVE_MAX_FUNCTIONS] = {0};
  uint32_t reachable_count = 0u;
  if (!closed_pure_function(program, function_index, state,
                            &reachable_count) ||
      reachable_count == 0u)
    return false;

  w_seed_parallel_elision0_certificate candidate;
  (void)memset(&candidate, 0, sizeof(candidate));
  (void)memcpy(candidate.schema, W_SEED_PARALLEL_ELISION0_SCHEMA_VERSION,
               sizeof(candidate.schema));
  candidate.root_function_index = selection->root_function_index;
  candidate.task_call_index = call_index;
  candidate.task_function_index = function_index;
  candidate.launch_binding_index = launch_index;
  candidate.join_binding_index = join_index;
  candidate.launch_instruction_index = launch->owner_instruction;
  candidate.join_instruction_index = join->owner_instruction;
  candidate.reachable_function_count = reachable_count;
  candidate.proof_facts = W_SEED_PARALLEL_ELISION0_REQUIRED_FACTS;
  candidate.function_count = (uint32_t)program->function_count;
  candidate.instruction_count = (uint32_t)program->instruction_count;
  candidate.binding_count = (uint32_t)program->binding_count;
  candidate.call_count = (uint32_t)program->call_count;
  candidate.value_count = (uint32_t)program->value_count;
  (void)memcpy(candidate.hir_semantic_digest, hir_result->semantic_digest,
               sizeof(candidate.hir_semantic_digest));
  *certificate = candidate;
  return true;
}

w_seed_parallel_elision0_status w_seed_parallel_elision0_certify(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection,
    w_seed_parallel_elision0_certificate *certificate) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      certificate == NULL || !w_seed_hir0_verify(program, hir_result) ||
      !w_seed_parallel_selection0_verify(program, hir_result, selection))
    return W_SEED_PARALLEL_ELISION0_INVALID;
  if (certificate_aliases_input(certificate, program, hir_result, selection))
    return W_SEED_PARALLEL_ELISION0_INVALID;
  w_seed_parallel_elision0_certificate candidate;
  if (!derive(program, hir_result, selection, &candidate))
    return W_SEED_PARALLEL_ELISION0_UNSUPPORTED;
  *certificate = candidate;
  return W_SEED_PARALLEL_ELISION0_OK;
}

bool w_seed_parallel_elision0_verify(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection,
    const w_seed_parallel_elision0_certificate *certificate) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      certificate == NULL || !w_seed_hir0_verify(program, hir_result) ||
      !w_seed_parallel_selection0_verify(program, hir_result, selection) ||
      certificate_aliases_input(certificate, program, hir_result, selection))
    return false;
  w_seed_parallel_elision0_certificate expected;
  return derive(program, hir_result, selection, &expected) &&
         memcmp(certificate, &expected, sizeof(expected)) == 0;
}
