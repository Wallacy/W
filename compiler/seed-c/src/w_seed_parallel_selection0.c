#include "w_seed_parallel_selection0.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  uintptr_t start;
  uintptr_t end;
  bool active;
} parallel_range;

static bool parallel_range_make(const void *pointer, size_t count,
                                size_t element_size, parallel_range *range) {
  if (range == NULL) return false;
  *range = (parallel_range){0u, 0u, false};
  if (count == 0u) return true;
  if (pointer == NULL || element_size == 0u || count > SIZE_MAX / element_size)
    return false;
  const size_t bytes = count * element_size;
  const uintptr_t start = (uintptr_t)pointer;
  if (start > UINTPTR_MAX - bytes) return false;
  *range = (parallel_range){start, start + bytes, true};
  return true;
}

static bool parallel_ranges_overlap(parallel_range left,
                                    parallel_range right) {
  return left.active && right.active && left.start < right.end &&
         right.start < left.end;
}

static bool parallel_selection_aliases_input(
    const w_seed_parallel_selection0 *selection,
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result) {
  parallel_range output;
  parallel_range input;
  if (!parallel_range_make(selection, 1u, sizeof(*selection), &output) ||
      !parallel_range_make(program, 1u, sizeof(*program), &input) ||
      parallel_ranges_overlap(output, input) ||
      !parallel_range_make(hir_result, 1u, sizeof(*hir_result), &input) ||
      parallel_ranges_overlap(output, input))
    return true;

#define W_PARSEL0_REJECT_OVERLAP(pointer, capacity)                           \
  do {                                                                         \
    if (!parallel_range_make((pointer), (capacity), sizeof(*(pointer)),        \
                             &input) ||                                         \
        parallel_ranges_overlap(output, input))                                \
      return true;                                                              \
  } while (0)

  W_PARSEL0_REJECT_OVERLAP(program->modules, program->module_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->identities, program->identity_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->types, program->type_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->enums, program->enum_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->enum_cases, program->enum_case_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->enum_case_parameters,
                           program->enum_case_parameter_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->enum_subset_members,
                           program->enum_subset_member_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->functions, program->function_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->parameters, program->parameter_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->blocks, program->block_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->block_arguments,
                           program->block_argument_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->edge_arguments,
                           program->edge_argument_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->switch_edges,
                           program->switch_edge_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->switch_captures,
                           program->switch_capture_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->instructions,
                           program->instruction_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->bindings, program->binding_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->calls, program->call_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->host_parameters,
                           program->host_parameter_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->arguments, program->argument_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->enum_payloads,
                           program->enum_payload_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->requirements,
                           program->requirement_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->values, program->value_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->interpolation_segments,
                           program->interpolation_segment_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->terminators,
                           program->terminator_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->entries, program->entry_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->text_bytes, program->text_byte_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->value_bytes, program->value_byte_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->receipt, program->receipt_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->external_modules,
                           program->external_module_capacity);
  W_PARSEL0_REJECT_OVERLAP(program->external_symbols,
                           program->external_symbol_capacity);
#undef W_PARSEL0_REJECT_OVERLAP
  return false;
}

static bool parallel_text_is(const w_seed_hir0_program *program,
                             w_seed_hir0_text text, const char *literal) {
  if (program == NULL || literal == NULL) return false;
  const size_t length = strlen(literal);
  return text.count == length && text.offset <= program->text_byte_count &&
         text.count <= program->text_byte_count - text.offset &&
         (length == 0u ||
          memcmp(program->text_bytes + text.offset, literal, length) == 0);
}

static bool parallel_call_is_exact(const w_seed_hir0_program *program,
                                   const w_seed_hir0_call *call,
                                   uint32_t root_block,
                                   uint32_t *target_function) {
  if (program == NULL || call == NULL || target_function == NULL ||
      call->execution_kind !=
          W_SEED_HIR0_CALL_STRUCTURED_ASYNC_PARALLEL_DOMAIN_DISPATCH ||
      call->placement != W_SEED_HIR0_CALL_PLACEMENT_PARALLEL_DOMAIN ||
      call->domain_mode != W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT ||
      call->domain_capabilities !=
          W_SEED_FRONTEND_DOMAIN_CAPABILITY_PARALLEL ||
      !parallel_text_is(program, call->domain_identity,
                        W_SEED_FRONTEND_DOMAIN_IDENTITY) ||
      call->owner_block != root_block ||
      call->callee_identity >= program->identity_count)
    return false;
  const w_seed_hir0_identity *identity =
      &program->identities[call->callee_identity];
  if (identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
      identity->target_index >= program->function_count)
    return false;
  *target_function = identity->target_index;
  return true;
}

static bool parallel_selection_derive(
    const w_seed_hir0_program *program,
    const w_seed_hir0_result *hir_result,
    w_seed_parallel_selection0 *selection) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      program->module_count != 1u || program->entry_count != 1u ||
      program->function_count == 0u || program->function_count > UINT32_MAX ||
      program->instruction_count > UINT32_MAX ||
      program->binding_count > UINT32_MAX || program->call_count > UINT32_MAX)
    return false;
  const w_seed_hir0_entry *entry = &program->entries[0];
  if (entry->module_index != 0u ||
      entry->target_function >= program->function_count)
    return false;
  const uint32_t root_index = entry->target_function;
  const w_seed_hir0_function *root = &program->functions[root_index];
  const bool anonymous_root =
      entry->is_body &&
      entry->adapter_kind == W_SEED_HIR0_ENTRY_ADAPTER_DEFAULT_UNIT &&
      root->is_anonymous_entry && !root->is_async &&
      root->parameter_count == 0u;
  const bool process_root =
      !entry->is_body &&
      entry->adapter_kind == W_SEED_HIR0_ENTRY_ADAPTER_NATIVE_PROCESS &&
      !root->is_anonymous_entry && root->is_async &&
      root->parameter_count == 2u;
  if ((!anonymous_root && !process_root) || root->module_index != 0u ||
      root->is_const || root->is_throws || root->is_unsafe ||
      root->has_borrow_clause ||
      root->block_count != 1u || root->first_block >= program->block_count)
    return false;
  const w_seed_hir0_block *block = &program->blocks[root->first_block];
  if (block->owner_function != root_index ||
      block->first_instruction > program->instruction_count ||
      block->instruction_count >
          program->instruction_count - block->first_instruction)
    return false;

  uint32_t call_indices[W_SEED_PARALLEL_SELECTION0_MAX_TASKS] = {
      W_SEED_HIR0_NONE};
  uint32_t function_indices[W_SEED_PARALLEL_SELECTION0_MAX_TASKS] = {
      W_SEED_HIR0_NONE};
  uint32_t launch_indices[W_SEED_PARALLEL_SELECTION0_MAX_TASKS] = {
      W_SEED_HIR0_NONE};
  uint32_t join_indices[W_SEED_PARALLEL_SELECTION0_MAX_TASKS] = {
      W_SEED_HIR0_NONE};
  size_t task_count = 0u;
  size_t launch_count = 0u;
  size_t join_count = 0u;
  size_t total_parallel_calls = 0u;
  bool joins_started = false;
  uint32_t process_prelude_call = W_SEED_HIR0_NONE;
  uint32_t process_prelude_binding = W_SEED_HIR0_NONE;

  for (size_t index = 0u; index < program->call_count; index += 1u) {
    const w_seed_hir0_call_execution_kind kind =
        program->calls[index].execution_kind;
    if (kind == W_SEED_HIR0_CALL_STRUCTURED_ASYNC_PARALLEL_DOMAIN_DISPATCH)
      total_parallel_calls += 1u;
    else if (kind != W_SEED_HIR0_CALL_DIRECT)
      return false;
  }

  for (size_t ordinal = 0u; ordinal < block->instruction_count;
       ordinal += 1u) {
    const uint32_t instruction_index =
        block->first_instruction + (uint32_t)ordinal;
    const w_seed_hir0_instruction *instruction =
        &program->instructions[instruction_index];
    if (instruction->owner_block != root->first_block ||
        instruction->ordinal != ordinal)
      return false;
    if (instruction->kind == W_SEED_HIR0_INSTRUCTION_CALL) {
      if (instruction->call_index >= program->call_count) return false;
      const w_seed_hir0_call *call = &program->calls[instruction->call_index];
      if (call->execution_kind == W_SEED_HIR0_CALL_DIRECT) {
        if (process_root) {
          if (process_prelude_call != W_SEED_HIR0_NONE || task_count != 0u ||
              launch_count != 0u || join_count != 0u || joins_started ||
              call->owner_block != root->first_block ||
              call->owner_instruction != instruction_index ||
              call->callee_identity >= program->identity_count)
            return false;
          const w_seed_hir0_identity *identity =
              &program->identities[call->callee_identity];
          if (identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
              identity->target_index >= program->function_count ||
              program->functions[identity->target_index].module_index !=
                  root->module_index)
            return false;
          process_prelude_call = instruction->call_index;
        }
        continue;
      }
      if (call->execution_kind !=
          W_SEED_HIR0_CALL_STRUCTURED_ASYNC_PARALLEL_DOMAIN_DISPATCH)
        return false;
      if (joins_started || task_count >= W_SEED_PARALLEL_SELECTION0_MAX_TASKS ||
          call->owner_instruction != instruction_index ||
          !parallel_call_is_exact(program, call, root->first_block,
                                  &function_indices[task_count]))
        return false;
      call_indices[task_count] = instruction->call_index;
      task_count += 1u;
    } else if (instruction->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
      if (instruction->binding_index >= program->binding_count) return false;
      const w_seed_hir0_binding *binding =
          &program->bindings[instruction->binding_index];
      if (binding->owner_instruction != instruction_index ||
          binding->owner_block != root->first_block)
        return false;
      if (binding->task_role == W_SEED_HIR0_TASK_ROLE_LAUNCH) {
        if (joins_started || launch_count >= task_count ||
            launch_count >= W_SEED_PARALLEL_SELECTION0_MAX_TASKS)
          return false;
        launch_indices[launch_count] = instruction->binding_index;
        launch_count += 1u;
      } else if (binding->task_role ==
                 W_SEED_HIR0_TASK_ROLE_AWAIT_RESULT) {
        joins_started = true;
        if (launch_count != task_count ||
            join_count >= W_SEED_PARALLEL_SELECTION0_MAX_TASKS)
          return false;
        join_indices[join_count] = instruction->binding_index;
        join_count += 1u;
      } else if (process_root &&
                 binding->task_role == W_SEED_HIR0_TASK_ROLE_NONE &&
                 process_prelude_binding == W_SEED_HIR0_NONE &&
                 task_count == 0u && launch_count == 0u && join_count == 0u &&
                 binding->initializer_value < program->value_count) {
        const w_seed_hir0_value *value =
            &program->values[binding->initializer_value];
        if (value->kind != W_SEED_HIR0_VALUE_CALL_RESULT ||
            value->call_index != process_prelude_call)
          return false;
        process_prelude_binding = instruction->binding_index;
      } else {
        return false;
      }
    } else {
      return false;
    }
  }
  if (task_count == 0u || task_count != total_parallel_calls ||
      launch_count != task_count || join_count != task_count ||
      (process_root &&
       (process_prelude_call == W_SEED_HIR0_NONE ||
        process_prelude_binding == W_SEED_HIR0_NONE)))
    return false;
  for (size_t task = 0u; task < task_count; task += 1u) {
    const w_seed_hir0_call *call = &program->calls[call_indices[task]];
    const w_seed_hir0_binding *launch = &program->bindings[launch_indices[task]];
    const w_seed_hir0_binding *join = &program->bindings[join_indices[task]];
    if (launch->task_peer_binding != join_indices[task] ||
        join->task_peer_binding != launch_indices[task] ||
        launch->owner_instruction <= call->owner_instruction ||
        join->owner_instruction <= launch->owner_instruction)
      return false;
  }

  (void)memset(selection, 0, sizeof(*selection));
  (void)memcpy(selection->schema, W_SEED_PARALLEL_SELECTION0_SCHEMA_VERSION,
               sizeof(selection->schema));
  selection->root_function_index = root_index;
  selection->task_count = (uint32_t)task_count;
  selection->function_count = (uint32_t)program->function_count;
  selection->instruction_count = (uint32_t)program->instruction_count;
  selection->binding_count = (uint32_t)program->binding_count;
  selection->call_count = (uint32_t)program->call_count;
  for (size_t task = 0u; task < task_count; task += 1u) {
    selection->task_call_indices[task] = call_indices[task];
    selection->task_function_indices[task] = function_indices[task];
    selection->launch_binding_indices[task] = launch_indices[task];
    selection->join_binding_indices[task] = join_indices[task];
  }
  selection->placement = W_SEED_HIR0_CALL_PLACEMENT_PARALLEL_DOMAIN;
  (void)memcpy(selection->domain_identity,
               W_SEED_FRONTEND_DOMAIN_IDENTITY,
               sizeof(selection->domain_identity));
  selection->domain_mode = W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT;
  selection->domain_capabilities =
      W_SEED_FRONTEND_DOMAIN_CAPABILITY_PARALLEL;
  (void)memcpy(selection->hir_semantic_digest, hir_result->semantic_digest,
               sizeof(selection->hir_semantic_digest));
  return true;
}

static bool parallel_selection_equal(
    const w_seed_parallel_selection0 *left,
    const w_seed_parallel_selection0 *right) {
  if (left == NULL || right == NULL ||
      memcmp(left->schema, right->schema, sizeof(left->schema)) != 0 ||
      left->root_function_index != right->root_function_index ||
      left->task_count != right->task_count ||
      left->function_count != right->function_count ||
      left->instruction_count != right->instruction_count ||
      left->binding_count != right->binding_count ||
      left->call_count != right->call_count ||
      left->placement != right->placement ||
      memcmp(left->domain_identity, right->domain_identity,
             sizeof(left->domain_identity)) != 0 ||
      left->domain_mode != right->domain_mode ||
      left->domain_capabilities != right->domain_capabilities ||
      memcmp(left->reserved, right->reserved, sizeof(left->reserved)) != 0 ||
      memcmp(left->hir_semantic_digest, right->hir_semantic_digest,
             sizeof(left->hir_semantic_digest)) != 0)
    return false;
  return memcmp(left->task_call_indices, right->task_call_indices,
                sizeof(left->task_call_indices)) == 0 &&
         memcmp(left->task_function_indices, right->task_function_indices,
                sizeof(left->task_function_indices)) == 0 &&
         memcmp(left->launch_binding_indices, right->launch_binding_indices,
                sizeof(left->launch_binding_indices)) == 0 &&
         memcmp(left->join_binding_indices, right->join_binding_indices,
                sizeof(left->join_binding_indices)) == 0;
}

w_seed_parallel_selection0_status w_seed_parallel_selection0_select(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    w_seed_parallel_selection0 *selection) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      !w_seed_hir0_verify(program, hir_result))
    return W_SEED_PARALLEL_SELECTION0_INVALID;
  w_seed_parallel_selection0 candidate;
  if (!parallel_selection_derive(program, hir_result, &candidate))
    return W_SEED_PARALLEL_SELECTION0_UNSUPPORTED;
  if (parallel_selection_aliases_input(selection, program, hir_result))
    return W_SEED_PARALLEL_SELECTION0_INVALID;
  *selection = candidate;
  return W_SEED_PARALLEL_SELECTION0_OK;
}

bool w_seed_parallel_selection0_verify(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      !w_seed_hir0_verify(program, hir_result) ||
      parallel_selection_aliases_input(selection, program, hir_result))
    return false;
  w_seed_parallel_selection0 expected;
  return parallel_selection_derive(program, hir_result, &expected) &&
         parallel_selection_equal(selection, &expected);
}
