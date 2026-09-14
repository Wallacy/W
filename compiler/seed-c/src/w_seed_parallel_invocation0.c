#include "w_seed_parallel_invocation0.h"

#include "w_seed_scalar_evaluator0.h"

#include <string.h>

typedef struct {
  uintptr_t start;
  uintptr_t end;
  bool active;
} invocation_memory_range;

static bool memory_range_make(const void *pointer, size_t count,
                              size_t element_size,
                              invocation_memory_range *range) {
  if (range == NULL) return false;
  *range = (invocation_memory_range){0u, 0u, false};
  if (count == 0u) return true;
  if (pointer == NULL || element_size == 0u || count > SIZE_MAX / element_size)
    return false;
  const size_t bytes = count * element_size;
  const uintptr_t start = (uintptr_t)pointer;
  if (start > UINTPTR_MAX - bytes) return false;
  *range = (invocation_memory_range){start, start + bytes, true};
  return true;
}

static bool memory_ranges_overlap(invocation_memory_range left,
                                  invocation_memory_range right) {
  return left.active && right.active && left.start < right.end &&
         right.start < left.end;
}

/* Called only after HIR and PARSEL0 verification, so descriptor capacities
 * are already safe to inspect. */
static bool plan_aliases_input(
    const w_seed_parallel_invocation0_plan *plan,
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection) {
  invocation_memory_range output;
  invocation_memory_range input;
  if (!memory_range_make(plan, 1u, sizeof(*plan), &output) ||
      !memory_range_make(program, 1u, sizeof(*program), &input) ||
      memory_ranges_overlap(output, input) ||
      !memory_range_make(hir_result, 1u, sizeof(*hir_result), &input) ||
      memory_ranges_overlap(output, input) ||
      !memory_range_make(selection, 1u, sizeof(*selection), &input) ||
      memory_ranges_overlap(output, input))
    return true;
#define W_PARINV0_REJECT_OVERLAP(pointer, capacity)                           \
  do {                                                                         \
    if (!memory_range_make((pointer), (capacity), sizeof(*(pointer)), &input) || \
        memory_ranges_overlap(output, input))                                 \
      return true;                                                              \
  } while (0)
  W_PARINV0_REJECT_OVERLAP(program->modules, program->module_capacity);
  W_PARINV0_REJECT_OVERLAP(program->identities, program->identity_capacity);
  W_PARINV0_REJECT_OVERLAP(program->types, program->type_capacity);
  W_PARINV0_REJECT_OVERLAP(program->enums, program->enum_capacity);
  W_PARINV0_REJECT_OVERLAP(program->enum_cases, program->enum_case_capacity);
  W_PARINV0_REJECT_OVERLAP(program->enum_case_parameters,
                           program->enum_case_parameter_capacity);
  W_PARINV0_REJECT_OVERLAP(program->enum_subset_members,
                           program->enum_subset_member_capacity);
  W_PARINV0_REJECT_OVERLAP(program->functions, program->function_capacity);
  W_PARINV0_REJECT_OVERLAP(program->parameters, program->parameter_capacity);
  W_PARINV0_REJECT_OVERLAP(program->blocks, program->block_capacity);
  W_PARINV0_REJECT_OVERLAP(program->block_arguments,
                           program->block_argument_capacity);
  W_PARINV0_REJECT_OVERLAP(program->edge_arguments,
                           program->edge_argument_capacity);
  W_PARINV0_REJECT_OVERLAP(program->switch_edges,
                           program->switch_edge_capacity);
  W_PARINV0_REJECT_OVERLAP(program->switch_captures,
                           program->switch_capture_capacity);
  W_PARINV0_REJECT_OVERLAP(program->instructions,
                           program->instruction_capacity);
  W_PARINV0_REJECT_OVERLAP(program->bindings, program->binding_capacity);
  W_PARINV0_REJECT_OVERLAP(program->calls, program->call_capacity);
  W_PARINV0_REJECT_OVERLAP(program->host_parameters,
                           program->host_parameter_capacity);
  W_PARINV0_REJECT_OVERLAP(program->arguments, program->argument_capacity);
  W_PARINV0_REJECT_OVERLAP(program->enum_payloads,
                           program->enum_payload_capacity);
  W_PARINV0_REJECT_OVERLAP(program->requirements,
                           program->requirement_capacity);
  W_PARINV0_REJECT_OVERLAP(program->values, program->value_capacity);
  W_PARINV0_REJECT_OVERLAP(program->interpolation_segments,
                           program->interpolation_segment_capacity);
  W_PARINV0_REJECT_OVERLAP(program->terminators,
                           program->terminator_capacity);
  W_PARINV0_REJECT_OVERLAP(program->entries, program->entry_capacity);
  W_PARINV0_REJECT_OVERLAP(program->text_bytes, program->text_byte_capacity);
  W_PARINV0_REJECT_OVERLAP(program->value_bytes, program->value_byte_capacity);
  W_PARINV0_REJECT_OVERLAP(program->receipt, program->receipt_capacity);
  W_PARINV0_REJECT_OVERLAP(program->external_modules,
                           program->external_module_capacity);
  W_PARINV0_REJECT_OVERLAP(program->external_symbols,
                           program->external_symbol_capacity);
#undef W_PARINV0_REJECT_OVERLAP
  return false;
}

static bool range_valid(uint32_t first, uint32_t count, size_t total) {
  return first != W_SEED_HIR0_NONE && first <= total && count <= total - first;
}

static bool i64_type(const w_seed_hir0_program *program,
                     uint32_t type_index) {
  return program != NULL && type_index < program->type_count &&
         program->types[type_index].kind == W_SEED_HIR0_TYPE_I64;
}

static bool derive(const w_seed_hir0_program *program,
                   const w_seed_hir0_result *hir_result,
                   const w_seed_parallel_selection0 *selection,
                   w_seed_parallel_invocation0_plan *plan) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      plan == NULL || selection->task_count == 0u ||
      selection->task_count > W_SEED_PARALLEL_INVOCATION0_MAX_TASKS ||
      program->value_count > UINT32_MAX)
    return false;
  w_seed_parallel_invocation0_plan candidate;
  (void)memset(&candidate, 0, sizeof(candidate));
  (void)memcpy(candidate.schema,
               W_SEED_PARALLEL_INVOCATION0_SCHEMA_VERSION,
               sizeof(candidate.schema));
  candidate.root_function_index = selection->root_function_index;
  candidate.task_count = selection->task_count;
  candidate.function_count = selection->function_count;
  candidate.instruction_count = selection->instruction_count;
  candidate.binding_count = selection->binding_count;
  candidate.call_count = selection->call_count;
  candidate.value_count = (uint32_t)program->value_count;
  (void)memcpy(candidate.hir_semantic_digest, hir_result->semantic_digest,
               sizeof(candidate.hir_semantic_digest));

  for (size_t task = 0u; task < selection->task_count; task += 1u) {
    const uint32_t call_index = selection->task_call_indices[task];
    const uint32_t function_index = selection->task_function_indices[task];
    if (call_index >= program->call_count ||
        function_index >= program->function_count)
      return false;
    const w_seed_hir0_call *call = &program->calls[call_index];
    const w_seed_hir0_function *function = &program->functions[function_index];
    if (call->execution_kind !=
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_PARALLEL_DOMAIN_DISPATCH ||
        call->argument_count != function->parameter_count ||
        call->argument_count > W_SEED_PARALLEL_INVOCATION0_MAX_ARGUMENTS ||
        !range_valid(call->first_argument, call->argument_count,
                     program->argument_count) ||
        !range_valid(function->first_parameter, function->parameter_count,
                     program->parameter_count) ||
        !i64_type(program, call->result_type) ||
        !i64_type(program, function->return_type) || function->is_async ||
        function->is_const || function->is_throws || function->is_unsafe ||
        function->has_borrow_clause || function->is_anonymous_entry ||
        function->block_count != 1u)
      return false;
    w_seed_parallel_invocation0_task *target = &candidate.tasks[task];
    target->call_index = call_index;
    target->function_index = function_index;
    target->argument_count = call->argument_count;
    bool seen[W_SEED_PARALLEL_INVOCATION0_MAX_ARGUMENTS] = {false};
    for (size_t ordinal = 0u; ordinal < call->argument_count; ordinal += 1u) {
      const w_seed_hir0_argument *argument =
          &program->arguments[(size_t)call->first_argument + ordinal];
      if (argument->owner_call != call_index ||
          argument->parameter_ordinal >= call->argument_count ||
          seen[argument->parameter_ordinal] ||
          argument->value_index >= program->value_count ||
          !i64_type(program, argument->type_index) ||
          !i64_type(program,
                    program->parameters[(size_t)function->first_parameter +
                                        argument->parameter_ordinal]
                        .type_index))
        return false;
      seen[argument->parameter_ordinal] = true;
      target->argument_value_indices[argument->parameter_ordinal] =
          argument->value_index;
    }
    int64_t proof_value = 0;
    size_t budget = W_SEED_PARALLEL_INVOCATION0_STEP_BUDGET;
    if (!w_seed_scalar_evaluator0_evaluate_call(program, call_index, &budget,
                                                &proof_value))
      return false;
  }
  *plan = candidate;
  return true;
}

w_seed_parallel_invocation0_status w_seed_parallel_invocation0_select(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection,
    w_seed_parallel_invocation0_plan *plan) {
  if (program == NULL || hir_result == NULL || selection == NULL || plan == NULL ||
      !w_seed_parallel_selection0_verify(program, hir_result, selection))
    return W_SEED_PARALLEL_INVOCATION0_INVALID;
  if (plan_aliases_input(plan, program, hir_result, selection))
    return W_SEED_PARALLEL_INVOCATION0_INVALID;
  w_seed_parallel_invocation0_plan candidate;
  if (!derive(program, hir_result, selection, &candidate))
    return W_SEED_PARALLEL_INVOCATION0_UNSUPPORTED;
  *plan = candidate;
  return W_SEED_PARALLEL_INVOCATION0_OK;
}

bool w_seed_parallel_invocation0_verify(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection,
    const w_seed_parallel_invocation0_plan *plan) {
  if (program == NULL || hir_result == NULL || selection == NULL || plan == NULL ||
      !w_seed_parallel_selection0_verify(program, hir_result, selection) ||
      plan_aliases_input(plan, program, hir_result, selection))
    return false;
  w_seed_parallel_invocation0_plan expected;
  return derive(program, hir_result, selection, &expected) &&
         memcmp(plan, &expected, sizeof(expected)) == 0;
}

w_seed_parallel_invocation0_status w_seed_parallel_invocation0_evaluate_task(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection,
    const w_seed_parallel_invocation0_plan *plan, uint32_t task_index,
    int64_t *value) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      plan == NULL || value == NULL ||
      !w_seed_parallel_invocation0_verify(program, hir_result, selection,
                                          plan) ||
      task_index >= plan->task_count ||
      task_index >= W_SEED_PARALLEL_INVOCATION0_MAX_TASKS)
    return W_SEED_PARALLEL_INVOCATION0_INVALID;
  const w_seed_parallel_invocation0_task *task = &plan->tasks[task_index];
  if (task->call_index >= program->call_count ||
      task->function_index >= program->function_count ||
      task->argument_count > W_SEED_PARALLEL_INVOCATION0_MAX_ARGUMENTS)
    return W_SEED_PARALLEL_INVOCATION0_INVALID;
  int64_t candidate = 0;
  size_t budget = W_SEED_PARALLEL_INVOCATION0_STEP_BUDGET;
  if (!w_seed_scalar_evaluator0_evaluate_call(
          program, task->call_index, &budget, &candidate))
    return W_SEED_PARALLEL_INVOCATION0_EVALUATION_FAILURE;
  *value = candidate;
  return W_SEED_PARALLEL_INVOCATION0_OK;
}
