#include "w_seed_parallel_typed_binding1.h"

#include "w_seed_scalar_evaluator0.h"
#include "w_seed_sha256.h"

#include <limits.h>
#include <string.h>

typedef struct {
  uintptr_t start;
  uintptr_t end;
  bool active;
} typed_binding1_range;

enum { W_TYPED_BINDING1_INPUT_RANGE_CAPACITY = 48u };

static bool range_make(const void *pointer, size_t count, size_t element_size,
                       typed_binding1_range *range) {
  if (range == NULL) return false;
  *range = (typed_binding1_range){0u, 0u, false};
  if (count == 0u) return true;
  if (pointer == NULL || element_size == 0u || count > SIZE_MAX / element_size)
    return false;
  const size_t bytes = count * element_size;
  const uintptr_t start = (uintptr_t)pointer;
  if (start > UINTPTR_MAX - bytes) return false;
  *range = (typed_binding1_range){start, start + bytes, true};
  return true;
}

static bool ranges_overlap(typed_binding1_range left,
                           typed_binding1_range right) {
  return left.active && right.active && left.start < right.end &&
         right.start < left.end;
}

static bool add_range(typed_binding1_range *ranges, size_t capacity,
                      size_t *count, const void *pointer, size_t elements,
                      size_t element_size) {
  if (ranges == NULL || count == NULL || *count >= capacity ||
      !range_make(pointer, elements, element_size, &ranges[*count]))
    return false;
  *count += 1u;
  return true;
}

static bool append_hir_ranges(const w_seed_hir0_program *program,
                              typed_binding1_range *ranges, size_t capacity,
                              size_t *count) {
  if (program == NULL || ranges == NULL || count == NULL) return false;
#define W_TYPED_BINDING1_ADD(pointer, number)                                 \
  do {                                                                        \
    if (!add_range(ranges, capacity, count, (pointer), (number),             \
                   sizeof(*(pointer))))                                      \
      return false;                                                           \
  } while (0)
  W_TYPED_BINDING1_ADD(program, 1u);
  W_TYPED_BINDING1_ADD(program->modules, program->module_capacity);
  W_TYPED_BINDING1_ADD(program->identities, program->identity_capacity);
  W_TYPED_BINDING1_ADD(program->types, program->type_capacity);
  W_TYPED_BINDING1_ADD(program->enums, program->enum_capacity);
  W_TYPED_BINDING1_ADD(program->enum_cases, program->enum_case_capacity);
  W_TYPED_BINDING1_ADD(program->enum_case_parameters,
                       program->enum_case_parameter_capacity);
  W_TYPED_BINDING1_ADD(program->enum_subset_members,
                       program->enum_subset_member_capacity);
  W_TYPED_BINDING1_ADD(program->functions, program->function_capacity);
  W_TYPED_BINDING1_ADD(program->parameters, program->parameter_capacity);
  W_TYPED_BINDING1_ADD(program->blocks, program->block_capacity);
  W_TYPED_BINDING1_ADD(program->block_arguments,
                       program->block_argument_capacity);
  W_TYPED_BINDING1_ADD(program->edge_arguments,
                       program->edge_argument_capacity);
  W_TYPED_BINDING1_ADD(program->switch_edges,
                       program->switch_edge_capacity);
  W_TYPED_BINDING1_ADD(program->switch_captures,
                       program->switch_capture_capacity);
  W_TYPED_BINDING1_ADD(program->instructions, program->instruction_capacity);
  W_TYPED_BINDING1_ADD(program->bindings, program->binding_capacity);
  W_TYPED_BINDING1_ADD(program->calls, program->call_capacity);
  W_TYPED_BINDING1_ADD(program->host_parameters,
                       program->host_parameter_capacity);
  W_TYPED_BINDING1_ADD(program->arguments, program->argument_capacity);
  W_TYPED_BINDING1_ADD(program->enum_payloads,
                       program->enum_payload_capacity);
  W_TYPED_BINDING1_ADD(program->requirements, program->requirement_capacity);
  W_TYPED_BINDING1_ADD(program->values, program->value_capacity);
  W_TYPED_BINDING1_ADD(program->interpolation_segments,
                       program->interpolation_segment_capacity);
  W_TYPED_BINDING1_ADD(program->terminators, program->terminator_capacity);
  W_TYPED_BINDING1_ADD(program->entries, program->entry_capacity);
  W_TYPED_BINDING1_ADD(program->text_bytes, program->text_byte_capacity);
  W_TYPED_BINDING1_ADD(program->value_bytes, program->value_byte_capacity);
  W_TYPED_BINDING1_ADD(program->receipt, program->receipt_capacity);
  W_TYPED_BINDING1_ADD(program->external_modules,
                       program->external_module_capacity);
  W_TYPED_BINDING1_ADD(program->external_symbols,
                       program->external_symbol_capacity);
  W_TYPED_BINDING1_ADD(program->cleanups, program->cleanup_capacity);
#undef W_TYPED_BINDING1_ADD
  return true;
}

static bool append_input_ranges(
    const w_seed_parallel_typed_binding1_input *input,
    typed_binding1_range *ranges, size_t capacity, size_t *count) {
  if (input == NULL || ranges == NULL || count == NULL ||
      !add_range(ranges, capacity, count, input, 1u, sizeof(*input)) ||
      !add_range(ranges, capacity, count, input->hir_result, 1u,
                 sizeof(*input->hir_result)) ||
      !append_hir_ranges(input->hir_program, ranges, capacity, count) ||
      !add_range(ranges, capacity, count, input->tasks, input->task_capacity,
                 sizeof(*input->tasks)))
    return false;
  return true;
}

static bool ranges_are_disjoint(
    const w_seed_parallel_typed_binding1_input *input,
    const w_seed_parallel_typed_binding1_workspace *workspace,
    const w_seed_parallel_typed_binding1_output *output,
    const w_seed_parallel_typed_binding1_counts *counts,
    const w_seed_parallel_typed_binding1_result *result) {
  typed_binding1_range writable[10];
  size_t writable_count = 0u;
#define W_TYPED_BINDING1_RANGE(pointer, number)                                \
  do {                                                                         \
    if (!add_range(writable, sizeof(writable) / sizeof(writable[0]),           \
                   &writable_count, (pointer), (number), sizeof(*(pointer)))) \
      return false;                                                            \
  } while (0)
  if (workspace != NULL) {
    W_TYPED_BINDING1_RANGE(workspace, 1u);
    W_TYPED_BINDING1_RANGE(workspace->completions,
                           W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT);
    W_TYPED_BINDING1_RANGE(workspace->receipt, 1u);
    W_TYPED_BINDING1_RANGE(workspace->provider_kind, 1u);
  }
  if (output != NULL) {
    W_TYPED_BINDING1_RANGE(output, 1u);
    W_TYPED_BINDING1_RANGE(output->records,
                           W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT);
  }
  if (counts != NULL) W_TYPED_BINDING1_RANGE(counts, 1u);
  W_TYPED_BINDING1_RANGE(result, 1u);
#undef W_TYPED_BINDING1_RANGE

  for (size_t left = 0u; left < writable_count; left += 1u)
    for (size_t right = left + 1u; right < writable_count; right += 1u)
      if (ranges_overlap(writable[left], writable[right])) return false;

  typed_binding1_range inputs[W_TYPED_BINDING1_INPUT_RANGE_CAPACITY];
  size_t input_count = 0u;
  if (!append_input_ranges(input, inputs,
                           W_TYPED_BINDING1_INPUT_RANGE_CAPACITY,
                           &input_count))
    return false;
  for (size_t out = 0u; out < writable_count; out += 1u)
    for (size_t in = 0u; in < input_count; in += 1u)
      if (ranges_overlap(writable[out], inputs[in])) return false;
  return true;
}

static void sha_u32(w_seed_sha256_state *state, uint32_t value) {
  const uint8_t bytes[4] = {(uint8_t)(value >> 24u),
                            (uint8_t)(value >> 16u),
                            (uint8_t)(value >> 8u), (uint8_t)value};
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void sha_u64(w_seed_sha256_state *state, uint64_t value) {
  const uint8_t bytes[8] = {
      (uint8_t)(value >> 56u), (uint8_t)(value >> 48u),
      (uint8_t)(value >> 40u), (uint8_t)(value >> 32u),
      (uint8_t)(value >> 24u), (uint8_t)(value >> 16u),
      (uint8_t)(value >> 8u), (uint8_t)value};
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void sha_i64(w_seed_sha256_state *state, int64_t value) {
  sha_u64(state, (uint64_t)value);
}

static void sha_bool(w_seed_sha256_state *state, bool value) {
  const uint8_t byte = value ? 1u : 0u;
  w_seed_sha256_update(state, &byte, sizeof(byte));
}

static void sha_text(w_seed_sha256_state *state,
                     const w_seed_hir0_program *program,
                     w_seed_hir0_text text) {
  sha_u32(state, text.count);
  if (text.count != 0u)
    w_seed_sha256_update(state, program->text_bytes + text.offset, text.count);
}

static bool provider_descriptor_valid(
    const w_seed_parallel_typed_binding1_provider *provider) {
  if (provider == NULL ||
      provider->target !=
          W_SEED_PARALLEL_TYPED_BINDING1_TARGET_WINDOWS_AMD64)
    return false;
  static const char expected_profile[
      W_SEED_PARALLEL_TYPED_BINDING1_PROVIDER_PROFILE_BYTES] =
      W_SEED_PARALLEL_TYPED_BINDING1_WINDOWS_PROFILE;
  static const char expected_identity[
      W_SEED_PARALLEL_TYPED_BINDING1_PROVIDER_IDENTITY_BYTES] =
      W_SEED_PARALLEL_TYPED_BINDING1_WINDOWS_IDENTITY;
  return memcmp(provider->profile, expected_profile,
                sizeof(expected_profile)) == 0 &&
         memcmp(provider->identity, expected_identity,
                sizeof(expected_identity)) == 0;
}

static bool range_u32_valid(uint32_t first, uint32_t count, size_t capacity) {
  return (size_t)first <= capacity && (size_t)count <= capacity - first;
}

static bool identity_equal(
    const w_seed_parallel_typed_binding1_error_identity *left,
    const w_seed_parallel_typed_binding1_error_identity *right) {
  return left != NULL && right != NULL &&
         left->invoke_terminator == right->invoke_terminator &&
         left->call_index == right->call_index &&
         left->callee_function == right->callee_function &&
         left->error_type == right->error_type &&
         left->enum_index == right->enum_index &&
         left->enum_case_index == right->enum_case_index &&
         left->case_ordinal == right->case_ordinal &&
         left->case_tag == right->case_tag &&
         memcmp(left->digest, right->digest, sizeof(left->digest)) == 0;
}

/* HIR41 is selected deliberately: one cleanup registration is part of this
 * witness, and both typed successors must carry the same verified Unit call.
 * This local rederivation is equivalent to the private typed-cleanup
 * selector, while retaining the extra success-call relation required by this
 * transaction. */
static bool validate_hir41_typed_cleanup(
    const w_seed_hir0_program *program, uint32_t caller_function,
    uint32_t invoke_terminator, const w_seed_hir0_terminator *invoke) {
  if (program == NULL || invoke == NULL || program->cleanup_count != 1u ||
      program->cleanups == NULL || caller_function >= program->function_count ||
      invoke->owner_block >= program->block_count ||
      invoke->target_block >= program->block_count ||
      invoke->else_block >= program->block_count)
    return false;
  const w_seed_hir0_cleanup *cleanup = &program->cleanups[0];
  const uint32_t normal_block = invoke->target_block;
  const uint32_t error_block = invoke->else_block;
  if (cleanup->owner_function != caller_function ||
      cleanup->invoke_terminator != invoke_terminator ||
      cleanup->normal_block != normal_block ||
      cleanup->error_block != error_block ||
      cleanup->normal_instruction >= program->instruction_count ||
      cleanup->error_instruction >= program->instruction_count ||
      cleanup->normal_call >= program->call_count ||
      cleanup->error_call >= program->call_count ||
      cleanup->normal_instruction == cleanup->error_instruction ||
      cleanup->normal_call == cleanup->error_call)
    return false;
  const w_seed_hir0_block *normal = &program->blocks[normal_block];
  const w_seed_hir0_block *error = &program->blocks[error_block];
  if (normal->owner_function != caller_function ||
      error->owner_function != caller_function ||
      normal->instruction_count != 1u || error->instruction_count != 1u ||
      normal->first_instruction != cleanup->normal_instruction ||
      error->first_instruction != cleanup->error_instruction)
    return false;
  const w_seed_hir0_instruction *normal_instruction =
      &program->instructions[cleanup->normal_instruction];
  const w_seed_hir0_instruction *error_instruction =
      &program->instructions[cleanup->error_instruction];
  if (normal_instruction->kind != W_SEED_HIR0_INSTRUCTION_CALL ||
      error_instruction->kind != W_SEED_HIR0_INSTRUCTION_CALL ||
      normal_instruction->owner_block != normal_block ||
      error_instruction->owner_block != error_block ||
      normal_instruction->call_index != cleanup->normal_call ||
      error_instruction->call_index != cleanup->error_call)
    return false;
  const w_seed_hir0_call *normal_call = &program->calls[cleanup->normal_call];
  const w_seed_hir0_call *error_call = &program->calls[cleanup->error_call];
  if (normal_call->owner_instruction != cleanup->normal_instruction ||
      error_call->owner_instruction != cleanup->error_instruction ||
      normal_call->owner_terminator != W_SEED_HIR0_NONE ||
      error_call->owner_terminator != W_SEED_HIR0_NONE ||
      normal_call->owner_block != normal_block ||
      error_call->owner_block != error_block ||
      normal_call->execution_kind != W_SEED_HIR0_CALL_DIRECT ||
      error_call->execution_kind != W_SEED_HIR0_CALL_DIRECT ||
      normal_call->argument_count != 0u || error_call->argument_count != 0u ||
      normal_call->result_type != W_SEED_HIR0_TYPE_UNIT ||
      error_call->result_type != W_SEED_HIR0_TYPE_UNIT ||
      normal_call->callee_identity != cleanup->cleanup_identity ||
      error_call->callee_identity != cleanup->cleanup_identity ||
      cleanup->cleanup_identity >= program->identity_count)
    return false;
  const w_seed_hir0_identity *cleanup_identity =
      &program->identities[cleanup->cleanup_identity];
  if (cleanup_identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
      cleanup_identity->target_index >= program->function_count)
    return false;
  const w_seed_hir0_function *cleanup_function =
      &program->functions[cleanup_identity->target_index];
  const w_seed_hir0_function *caller = &program->functions[caller_function];
  return cleanup_function->module_index == caller->module_index &&
         !cleanup_function->is_async && !cleanup_function->is_throws &&
         !cleanup_function->is_unsafe && !cleanup_function->has_borrow_clause &&
         !cleanup_function->is_anonymous_entry &&
         cleanup_function->parameter_count == 0u &&
         cleanup_function->return_type == W_SEED_HIR0_TYPE_UNIT;
}

/* Task zero is an ordinary direct i64 call.  Its identity is intentionally
 * independent from the throwing INVOKE used by task one. */
static bool success_call_valid(
    const w_seed_hir0_program *program, uint32_t call_index,
    const w_seed_parallel_typed_binding1_error_identity *identity) {
  if (program == NULL || identity == NULL || call_index >= program->call_count ||
      call_index == identity->call_index)
    return false;
  const w_seed_hir0_call *call = &program->calls[call_index];
  if (call->owner_instruction == W_SEED_HIR0_NONE ||
      call->owner_instruction >= program->instruction_count ||
      call->owner_terminator != W_SEED_HIR0_NONE ||
      call->owner_block >= program->block_count ||
      call->execution_kind != W_SEED_HIR0_CALL_DIRECT ||
      call->argument_count != 0u || call->result_type != W_SEED_HIR0_TYPE_I64 ||
      call->callee_identity >= program->identity_count)
    return false;
  const w_seed_hir0_instruction *instruction =
      &program->instructions[call->owner_instruction];
  if (instruction->kind != W_SEED_HIR0_INSTRUCTION_CALL ||
      instruction->owner_block != call->owner_block ||
      instruction->call_index != call_index ||
      instruction->result_type != W_SEED_HIR0_TYPE_I64)
    return false;
  const w_seed_hir0_block *block = &program->blocks[call->owner_block];
  if (block->owner_function >= program->function_count) return false;
  const w_seed_hir0_identity *callee_identity =
      &program->identities[call->callee_identity];
  if (callee_identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
      callee_identity->target_index >= program->function_count)
    return false;
  const w_seed_hir0_function *caller =
      &program->functions[block->owner_function];
  const w_seed_hir0_function *callee =
      &program->functions[callee_identity->target_index];
  return callee->module_index == caller->module_index && !callee->is_throws &&
         !callee->is_async && callee->return_type == W_SEED_HIR0_TYPE_I64;
}

static uint32_t call_callee_function(const w_seed_hir0_program *program,
                                     uint32_t call_index) {
  if (program == NULL || call_index >= program->call_count) return UINT32_MAX;
  const w_seed_hir0_call *call = &program->calls[call_index];
  if (call->callee_identity >= program->identity_count) return UINT32_MAX;
  const w_seed_hir0_identity *identity =
      &program->identities[call->callee_identity];
  return identity->kind == W_SEED_HIR0_IDENTITY_FUNCTION
             ? identity->target_index
             : UINT32_MAX;
}

static bool derive_error_identity(
    const w_seed_parallel_typed_binding1_input *input,
    w_seed_parallel_typed_binding1_error_identity *identity) {
  if (input == NULL || identity == NULL || input->hir_program == NULL ||
      input->hir_result == NULL ||
      memcmp(input->hir_result->schema, W_SEED_HIR0_SCHEMA_VERSION,
             sizeof(input->hir_result->schema)) != 0 ||
      !w_seed_hir0_verify(input->hir_program, input->hir_result) ||
      input->invoke_terminator >= input->hir_program->terminator_count)
    return false;
  const w_seed_hir0_program *program = input->hir_program;
  const w_seed_hir0_terminator *invoke =
      &program->terminators[input->invoke_terminator];
  if (invoke->kind != W_SEED_HIR0_TERMINATOR_INVOKE ||
      invoke->call_index >= program->call_count ||
      invoke->owner_block >= program->block_count)
    return false;
  const w_seed_hir0_block *invoke_block = &program->blocks[invoke->owner_block];
  if (invoke_block->owner_function >= program->function_count ||
      !validate_hir41_typed_cleanup(program, invoke_block->owner_function,
                                    input->invoke_terminator, invoke))
    return false;
  const w_seed_hir0_function *caller =
      &program->functions[invoke_block->owner_function];
  const w_seed_hir0_call *call = &program->calls[invoke->call_index];
  if (call->owner_terminator != input->invoke_terminator ||
      call->owner_block != invoke->owner_block ||
      call->owner_instruction != W_SEED_HIR0_NONE ||
      call->execution_kind != W_SEED_HIR0_CALL_DIRECT ||
      call->callee_identity >= program->identity_count ||
      call->result_type != W_SEED_HIR0_TYPE_I64 ||
      invoke->result_type != W_SEED_HIR0_TYPE_I64 ||
      caller->error_type != invoke->error_type || !caller->is_throws)
    return false;
  const w_seed_hir0_identity *callee_identity =
      &program->identities[call->callee_identity];
  if (callee_identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
      callee_identity->target_index >= program->function_count)
    return false;
  const uint32_t callee_function_index = callee_identity->target_index;
  const w_seed_hir0_function *callee =
      &program->functions[callee_function_index];
  if (!callee->is_throws || callee->is_async ||
      callee->return_type != W_SEED_HIR0_TYPE_I64 ||
      callee->error_type != invoke->error_type ||
      callee->module_index != caller->module_index ||
      callee->block_count != 1u ||
      !range_u32_valid(callee->first_block, callee->block_count,
                       program->block_count))
    return false;
  const w_seed_hir0_block *callee_block = &program->blocks[callee->first_block];
  if (callee_block->owner_function != callee_function_index ||
      callee_block->terminator_index >= program->terminator_count)
    return false;
  const w_seed_hir0_terminator *throw_term =
      &program->terminators[callee_block->terminator_index];
  if (throw_term->kind != W_SEED_HIR0_TERMINATOR_THROW ||
      throw_term->value_index >= program->value_count ||
      throw_term->result_type != callee->error_type)
    return false;
  const w_seed_hir0_value *error_value =
      &program->values[throw_term->value_index];
  if (error_value->kind != W_SEED_HIR0_VALUE_ENUM_CASE ||
      error_value->type_index != callee->error_type ||
      error_value->enum_index >= program->enum_count ||
      error_value->enum_case_index >= program->enum_case_count)
    return false;
  const w_seed_hir0_type *error_type = &program->types[callee->error_type];
  if (error_type->kind != W_SEED_HIR0_TYPE_ENUM ||
      error_type->enum_index != error_value->enum_index)
    return false;
  const w_seed_hir0_enum *error_enum = &program->enums[error_value->enum_index];
  if (!error_enum->error_conformance ||
      !range_u32_valid(error_enum->first_case, error_enum->case_count,
                       program->enum_case_count) ||
      error_value->enum_case_index < error_enum->first_case ||
      error_value->enum_case_index >=
          error_enum->first_case + error_enum->case_count)
    return false;
  const w_seed_hir0_enum_case *error_case =
      &program->enum_cases[error_value->enum_case_index];
  if (error_case->owner_enum != error_value->enum_index ||
      error_case->payload_count != 0u ||
      error_case->ordinal != error_case->tag ||
      error_case->ordinal >= error_enum->case_count)
    return false;

  (void)memset(identity, 0, sizeof(*identity));
  identity->invoke_terminator = input->invoke_terminator;
  identity->call_index = invoke->call_index;
  identity->callee_function = callee_function_index;
  identity->error_type = callee->error_type;
  identity->enum_index = error_value->enum_index;
  identity->enum_case_index = error_value->enum_case_index;
  identity->case_ordinal = error_case->ordinal;
  identity->case_tag = error_case->tag;

  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(
      &state,
      (const uint8_t *)"w-seed-parallel-typed-error-identity-1",
      sizeof("w-seed-parallel-typed-error-identity-1") - 1u);
  w_seed_sha256_update(&state, input->hir_result->semantic_digest, 32u);
  sha_u32(&state, identity->invoke_terminator);
  sha_u32(&state, identity->call_index);
  sha_u32(&state, identity->callee_function);
  sha_u32(&state, identity->error_type);
  sha_u32(&state, identity->enum_index);
  sha_u32(&state, identity->enum_case_index);
  sha_u32(&state, identity->case_ordinal);
  sha_u32(&state, identity->case_tag);
  sha_text(&state, program, error_enum->name);
  sha_text(&state, program, error_case->name);
  w_seed_sha256_final(&state, identity->digest);
  return true;
}

static w_seed_parallel_typed_binding1_status validate_input(
    const w_seed_parallel_typed_binding1_input *input,
    w_seed_parallel_typed_binding1_error_identity *identity) {
  if (input == NULL || identity == NULL || input->hir_program == NULL ||
      input->hir_result == NULL || input->tasks == NULL ||
      input->task_count !=
          W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT ||
      input->task_capacity < input->task_count || input->generation == 0u ||
      (input->provider_capacity != 1u && input->provider_capacity != 2u) ||
      input->provider_job.invoke == NULL ||
      !provider_descriptor_valid(&input->provider))
    return W_SEED_PARALLEL_TYPED_BINDING1_INVALID;
  if (!derive_error_identity(input, identity) ||
      input->tasks[0].lexical_index != 0u ||
      !success_call_valid(input->hir_program, input->tasks[0].call_index,
                          identity) ||
      input->tasks[1].lexical_index != 1u ||
      input->tasks[1].call_index != identity->call_index)
    return W_SEED_PARALLEL_TYPED_BINDING1_HIR;
  return W_SEED_PARALLEL_TYPED_BINDING1_OK;
}

static bool workspace_valid(
    const w_seed_parallel_typed_binding1_workspace *workspace) {
  return workspace != NULL && workspace->completions != NULL &&
         workspace->receipt != NULL && workspace->provider_kind != NULL;
}

static bool output_valid(const w_seed_parallel_typed_binding1_output *output) {
  return output != NULL && output->records != NULL;
}

static bool completion_is_success(
    const w_seed_parallel_platform1_completion *completion) {
  return completion != NULL &&
         completion->kind == W_SEED_PARALLEL_PLATFORM1_COMPLETION_SUCCESS &&
         completion->error_code == 0u && completion->cancel_reason == 0u &&
         completion->error_case_ordinal == 0u;
}

static bool completion_is_error(
    const w_seed_parallel_platform1_completion *completion,
    const w_seed_parallel_typed_binding1_error_identity *identity) {
  return completion != NULL && identity != NULL &&
         completion->kind == W_SEED_PARALLEL_PLATFORM1_COMPLETION_ERROR &&
         completion->success_value == 0 && completion->error_code != 0u &&
         completion->cancel_reason == 0u &&
         completion->error_case_ordinal == identity->case_ordinal;
}

static bool physical_facts_valid(
    const w_seed_parallel_typed_binding1_input *input,
    const w_seed_parallel_platform1_completion *completions,
    const w_seed_parallel_platform1_receipt *receipt,
    w_seed_parallel_provider0_kind provider_kind,
    const w_seed_parallel_typed_binding1_error_identity *identity) {
  if (input == NULL || completions == NULL || receipt == NULL ||
      identity == NULL || provider_kind != W_SEED_PARALLEL_PROVIDER0_KIND_WINDOWS_KERNEL32 ||
      receipt->started_count !=
          W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT ||
      receipt->settled_count !=
          W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT ||
      receipt->canceled_before_start_count != 0u || receipt->maximum_active == 0u ||
      receipt->maximum_active > input->provider_capacity)
    return false;
  int64_t expected_success = 0;
  size_t budget = 4096u;
  return completion_is_success(&completions[0]) &&
         w_seed_scalar_evaluator0_evaluate_call(
             input->hir_program, input->tasks[0].call_index, &budget,
             &expected_success) &&
         completions[0].success_value == expected_success &&
         completion_is_error(&completions[1], identity) &&
         receipt->cancellation_requested &&
         receipt->cancellation_source_index == 1u;
}

static bool build_records(
    const w_seed_parallel_typed_binding1_input *input,
    const w_seed_parallel_typed_binding1_error_identity *identity,
    const w_seed_parallel_platform1_completion *completions,
    w_seed_parallel_typed_binding1_record records[
        W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT],
    uint32_t *primary_error_task,
    w_seed_parallel_typed_binding1_outcome_kind *scope_outcome) {
  if (input == NULL || identity == NULL || completions == NULL ||
      records == NULL || primary_error_task == NULL || scope_outcome == NULL)
    return false;
  (void)memset(records, 0,
               sizeof(w_seed_parallel_typed_binding1_record) *
                   W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT);
  *primary_error_task = UINT32_MAX;
  *scope_outcome = W_SEED_PARALLEL_TYPED_BINDING1_OUTCOME_SUCCESS;
  for (size_t task = 0u; task < input->task_count; task += 1u) {
    w_seed_parallel_typed_binding1_record *record = &records[task];
    record->lexical_index = input->tasks[task].lexical_index;
    record->call_index = input->tasks[task].call_index;
    record->callee_function =
        call_callee_function(input->hir_program, record->call_index);
    if (record->callee_function == UINT32_MAX) return false;
    if (completion_is_success(&completions[task])) {
      record->outcome = W_SEED_PARALLEL_TYPED_BINDING1_OUTCOME_SUCCESS;
      record->success_value = completions[task].success_value;
      continue;
    }
    if (!completion_is_error(&completions[task], identity)) return false;
    record->outcome = W_SEED_PARALLEL_TYPED_BINDING1_OUTCOME_ERROR;
    record->error_type = identity->error_type;
    record->error_enum_index = identity->enum_index;
    record->error_case_index = identity->enum_case_index;
    record->error_case_ordinal = identity->case_ordinal;
    record->error_case_tag = identity->case_tag;
    (void)memcpy(record->error_identity_digest, identity->digest,
                 sizeof(record->error_identity_digest));
    *scope_outcome = W_SEED_PARALLEL_TYPED_BINDING1_OUTCOME_ERROR;
    if (*primary_error_task == UINT32_MAX ||
        record->lexical_index < *primary_error_task)
      *primary_error_task = record->lexical_index;
  }
  return true;
}

static void seal_semantic(
    const w_seed_parallel_typed_binding1_error_identity *identity,
    const w_seed_parallel_typed_binding1_record *records,
    size_t record_count,
    w_seed_parallel_typed_binding1_outcome_kind scope_outcome,
    uint32_t primary_error_task, uint8_t digest[32]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(
      &state,
      (const uint8_t *)W_SEED_PARALLEL_TYPED_BINDING1_SCHEMA_VERSION,
      sizeof(W_SEED_PARALLEL_TYPED_BINDING1_SCHEMA_VERSION) - 1u);
  w_seed_sha256_update(&state, identity->digest, sizeof(identity->digest));
  sha_u32(&state, (uint32_t)record_count);
  for (size_t index = 0u; index < record_count; index += 1u) {
    const w_seed_parallel_typed_binding1_record *record = &records[index];
    sha_u32(&state, record->lexical_index);
    sha_u32(&state, record->call_index);
    sha_u32(&state, record->callee_function);
    sha_u32(&state, (uint32_t)record->outcome);
    if (record->outcome == W_SEED_PARALLEL_TYPED_BINDING1_OUTCOME_SUCCESS) {
      sha_i64(&state, record->success_value);
    } else {
      sha_u32(&state, record->error_type);
      sha_u32(&state, record->error_enum_index);
      sha_u32(&state, record->error_case_index);
      sha_u32(&state, record->error_case_ordinal);
      sha_u32(&state, record->error_case_tag);
      w_seed_sha256_update(&state, record->error_identity_digest,
                           sizeof(record->error_identity_digest));
    }
  }
  sha_u32(&state, (uint32_t)scope_outcome);
  sha_u32(&state, primary_error_task);
  w_seed_sha256_final(&state, digest);
}

static void seal_provenance(
    const w_seed_parallel_typed_binding1_input *input,
    w_seed_parallel_provider0_kind provider_kind,
    const w_seed_parallel_platform1_completion *completions,
    const w_seed_parallel_platform1_receipt *receipt,
    const uint8_t semantic_digest[32], uint8_t digest[32]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(
      &state,
      (const uint8_t *)"w-seed-parallel-typed-binding1-provenance-1",
      sizeof("w-seed-parallel-typed-binding1-provenance-1") - 1u);
  w_seed_sha256_update(&state, semantic_digest, 32u);
  w_seed_sha256_update(&state, input->hir_result->provenance_digest, 32u);
  w_seed_sha256_update(
      &state, (const uint8_t *)input->provider.profile,
      sizeof(input->provider.profile));
  w_seed_sha256_update(
      &state, (const uint8_t *)input->provider.identity,
      sizeof(input->provider.identity));
  sha_u32(&state, (uint32_t)input->provider.target);
  sha_u32(&state, (uint32_t)provider_kind);
  sha_u32(&state, input->provider_capacity);
  sha_u32(&state, input->generation);
  for (size_t task = 0u;
       task < W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT;
       task += 1u) {
    sha_u32(&state, (uint32_t)completions[task].kind);
    sha_i64(&state, completions[task].success_value);
    sha_u32(&state, completions[task].error_code);
    sha_u32(&state, completions[task].cancel_reason);
    sha_u32(&state, completions[task].error_case_ordinal);
  }
  sha_u32(&state, receipt->started_count);
  sha_u32(&state, receipt->settled_count);
  sha_u32(&state, receipt->canceled_before_start_count);
  sha_u32(&state, receipt->maximum_active);
  sha_u32(&state, receipt->cancellation_source_index);
  sha_bool(&state, receipt->cancellation_requested);
  sha_u32(
      &state,
      (uint32_t)W_SEED_PARALLEL_TYPED_BINDING1_ASSURANCE_EXECUTION_INTEGRITY);
  w_seed_sha256_final(&state, digest);
}

static void result_base(
    const w_seed_parallel_typed_binding1_input *input,
    const w_seed_parallel_typed_binding1_error_identity *identity,
    w_seed_parallel_typed_binding1_result *result) {
  (void)memset(result, 0, sizeof(*result));
  result->status = W_SEED_PARALLEL_TYPED_BINDING1_OK;
  result->required = (w_seed_parallel_typed_binding1_counts){
      W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT,
      W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT};
  (void)memcpy(result->schema,
               W_SEED_PARALLEL_TYPED_BINDING1_SCHEMA_VERSION,
               sizeof(result->schema));
  result->task_count = W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT;
  result->invoke_terminator = identity->invoke_terminator;
  result->generation = input->generation;
  result->primary_error_task = UINT32_MAX;
  result->scope_outcome = W_SEED_PARALLEL_TYPED_BINDING1_OUTCOME_NONE;
  (void)memcpy(result->hir_semantic_digest,
               input->hir_result->semantic_digest,
               sizeof(result->hir_semantic_digest));
  result->error_identity = *identity;
}

static bool result_semantic_fields_valid(
    const w_seed_parallel_typed_binding1_input *input,
    const w_seed_parallel_typed_binding1_error_identity *identity,
    const w_seed_parallel_typed_binding1_result *result) {
  if (input == NULL || identity == NULL || result == NULL ||
      result->status != W_SEED_PARALLEL_TYPED_BINDING1_OK ||
      memcmp(result->schema, W_SEED_PARALLEL_TYPED_BINDING1_SCHEMA_VERSION,
             sizeof(result->schema)) != 0 ||
      result->required.completions !=
          W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT ||
      result->required.records !=
          W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT ||
      result->written.completions !=
          W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT ||
      result->written.records !=
          W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT ||
      result->task_count !=
          W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT ||
      result->invoke_terminator != identity->invoke_terminator ||
      result->generation != input->generation ||
      memcmp(result->hir_semantic_digest, input->hir_result->semantic_digest,
             sizeof(result->hir_semantic_digest)) != 0 ||
      !identity_equal(&result->error_identity, identity))
    return false;
  return true;
}

w_seed_parallel_typed_binding1_status w_seed_parallel_typed_binding1_measure(
    const w_seed_parallel_typed_binding1_input *input,
    w_seed_parallel_typed_binding1_counts *counts,
    w_seed_parallel_typed_binding1_result *result) {
  if (counts == NULL || result == NULL) return W_SEED_PARALLEL_TYPED_BINDING1_INVALID;
  w_seed_parallel_typed_binding1_error_identity identity;
  const w_seed_parallel_typed_binding1_status input_status =
      validate_input(input, &identity);
  if (input_status != W_SEED_PARALLEL_TYPED_BINDING1_OK) return input_status;
  if (!ranges_are_disjoint(input, NULL, NULL, counts, result))
    return W_SEED_PARALLEL_TYPED_BINDING1_ALIAS;
  w_seed_parallel_typed_binding1_result candidate;
  result_base(input, &identity, &candidate);
  candidate.written = (w_seed_parallel_typed_binding1_counts){0u, 0u};
  *counts = candidate.required;
  *result = candidate;
  return W_SEED_PARALLEL_TYPED_BINDING1_OK;
}

w_seed_parallel_typed_binding1_status w_seed_parallel_typed_binding1_run(
    const w_seed_parallel_typed_binding1_input *input,
    const w_seed_parallel_typed_binding1_workspace *workspace,
    const w_seed_parallel_typed_binding1_output *output,
    w_seed_parallel_typed_binding1_result *result) {
  w_seed_parallel_typed_binding1_error_identity identity;
  const w_seed_parallel_typed_binding1_status input_status =
      validate_input(input, &identity);
  if (input_status != W_SEED_PARALLEL_TYPED_BINDING1_OK) return input_status;
  if (!workspace_valid(workspace) || !output_valid(output) || result == NULL)
    return W_SEED_PARALLEL_TYPED_BINDING1_INVALID;
  if (workspace->completion_capacity <
          W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT ||
      output->record_capacity <
          W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT)
    return W_SEED_PARALLEL_TYPED_BINDING1_CAPACITY;
  if (!ranges_are_disjoint(input, workspace, output, NULL, result))
    return W_SEED_PARALLEL_TYPED_BINDING1_ALIAS;

  /* PLATFORM1 is allowed to alter its completion scratch on failure.  Keep
   * all physical outputs local until semantic and typed-identity checks pass. */
  w_seed_parallel_platform1_completion completions[
      W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT];
  (void)memset(completions, 0, sizeof(completions));
  w_seed_parallel_platform1_receipt receipt;
  (void)memset(&receipt, 0, sizeof(receipt));
  receipt.cancellation_source_index = UINT32_MAX;
  w_seed_parallel_provider0_kind provider_kind =
      W_SEED_PARALLEL_PROVIDER0_KIND_NONE;
  const w_seed_parallel_provider0_platform_status platform_status =
      w_seed_parallel_platform1_execute(
          &input->provider_job, input->task_count, input->provider_capacity,
          completions, &receipt, &provider_kind);
  if (platform_status == W_SEED_PARALLEL_PROVIDER0_PLATFORM_UNSUPPORTED)
    return W_SEED_PARALLEL_TYPED_BINDING1_UNSUPPORTED;
  if (platform_status == W_SEED_PARALLEL_PROVIDER0_PLATFORM_TASK_FAILURE)
    return W_SEED_PARALLEL_TYPED_BINDING1_TASK_FAILURE;
  if (platform_status != W_SEED_PARALLEL_PROVIDER0_PLATFORM_OK)
    return W_SEED_PARALLEL_TYPED_BINDING1_PROVIDER_FAILURE;
  if (provider_kind != W_SEED_PARALLEL_PROVIDER0_KIND_WINDOWS_KERNEL32)
    return W_SEED_PARALLEL_TYPED_BINDING1_PROVIDER_FAILURE;
  if (!physical_facts_valid(input, completions, &receipt, provider_kind,
                            &identity)) {
    for (size_t task = 0u;
         task < W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT;
         task += 1u)
      if (completions[task].kind ==
          W_SEED_PARALLEL_PLATFORM1_COMPLETION_CANCELED)
        return W_SEED_PARALLEL_TYPED_BINDING1_CANCELED;
    return W_SEED_PARALLEL_TYPED_BINDING1_PROVIDER_FAILURE;
  }

  w_seed_parallel_typed_binding1_error_identity post_identity;
  if (validate_input(input, &post_identity) !=
          W_SEED_PARALLEL_TYPED_BINDING1_OK ||
      !identity_equal(&identity, &post_identity))
    return W_SEED_PARALLEL_TYPED_BINDING1_HIR;
  w_seed_parallel_typed_binding1_record records[
      W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT];
  uint32_t primary_error_task = UINT32_MAX;
  w_seed_parallel_typed_binding1_outcome_kind scope_outcome =
      W_SEED_PARALLEL_TYPED_BINDING1_OUTCOME_NONE;
  if (!build_records(input, &identity, completions, records,
                     &primary_error_task, &scope_outcome))
    return W_SEED_PARALLEL_TYPED_BINDING1_FORGERY;

  w_seed_parallel_typed_binding1_result candidate;
  result_base(input, &identity, &candidate);
  candidate.written = candidate.required;
  candidate.primary_error_task = primary_error_task;
  candidate.scope_outcome = scope_outcome;
  seal_semantic(&identity, records,
                W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT,
                scope_outcome,
                primary_error_task, candidate.semantic_digest);
  candidate.provenance.provider = input->provider;
  candidate.provenance.provider_kind = provider_kind;
  candidate.provenance.provider_capacity = input->provider_capacity;
  candidate.provenance.generation = input->generation;
  candidate.provenance.upstream_receipt = receipt;
  candidate.provenance.assurance =
      W_SEED_PARALLEL_TYPED_BINDING1_ASSURANCE_EXECUTION_INTEGRITY;
  (void)memcpy(candidate.provenance.semantic_digest,
               candidate.semantic_digest,
               sizeof(candidate.provenance.semantic_digest));
  seal_provenance(input, provider_kind, completions, &receipt,
                  candidate.semantic_digest,
                  candidate.provenance.provenance_digest);

  (void)memcpy(workspace->completions, completions, sizeof(completions));
  *workspace->receipt = receipt;
  *workspace->provider_kind = provider_kind;
  (void)memcpy(output->records, records, sizeof(records));
  *result = candidate;
  return W_SEED_PARALLEL_TYPED_BINDING1_OK;
}

bool w_seed_parallel_typed_binding1_verify(
    const w_seed_parallel_typed_binding1_input *input,
    const w_seed_parallel_typed_binding1_workspace *workspace,
    const w_seed_parallel_typed_binding1_output *output,
    const w_seed_parallel_typed_binding1_result *result) {
  w_seed_parallel_typed_binding1_error_identity identity;
  if (validate_input(input, &identity) !=
          W_SEED_PARALLEL_TYPED_BINDING1_OK ||
      !workspace_valid(workspace) ||
      !output_valid(output) || result == NULL ||
      workspace->completion_capacity <
          W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT ||
      output->record_capacity <
          W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT ||
      !ranges_are_disjoint(input, workspace, output, NULL, result) ||
      !result_semantic_fields_valid(input, &identity, result) ||
      !physical_facts_valid(input, workspace->completions,
                            workspace->receipt, *workspace->provider_kind,
                            &identity))
    return false;
  w_seed_parallel_typed_binding1_record records[
      W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT];
  uint32_t primary_error_task = UINT32_MAX;
  w_seed_parallel_typed_binding1_outcome_kind scope_outcome =
      W_SEED_PARALLEL_TYPED_BINDING1_OUTCOME_NONE;
  if (!build_records(input, &identity, workspace->completions, records,
                     &primary_error_task, &scope_outcome) ||
      memcmp(records, output->records, sizeof(records)) != 0 ||
      result->primary_error_task != primary_error_task ||
      result->scope_outcome != scope_outcome)
    return false;
  uint8_t semantic_digest[32];
  seal_semantic(&identity, records,
                W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT,
                scope_outcome,
                primary_error_task, semantic_digest);
  if (memcmp(result->semantic_digest, semantic_digest,
             sizeof(semantic_digest)) != 0 ||
      memcmp(result->provenance.semantic_digest, semantic_digest,
             sizeof(semantic_digest)) != 0)
    return false;
  if (result->provenance.provider_kind != *workspace->provider_kind ||
      result->provenance.provider_capacity != input->provider_capacity ||
      result->provenance.generation != input->generation ||
      result->provenance.assurance !=
          W_SEED_PARALLEL_TYPED_BINDING1_ASSURANCE_EXECUTION_INTEGRITY ||
      result->provenance.provider.target != input->provider.target ||
      memcmp(result->provenance.provider.profile, input->provider.profile,
             sizeof(input->provider.profile)) != 0 ||
      memcmp(result->provenance.provider.identity, input->provider.identity,
             sizeof(input->provider.identity)) != 0 ||
      memcmp(&result->provenance.upstream_receipt, workspace->receipt,
             sizeof(*workspace->receipt)) != 0)
    return false;
  uint8_t provenance_digest[32];
  seal_provenance(input, *workspace->provider_kind, workspace->completions,
                  workspace->receipt, semantic_digest, provenance_digest);
  return memcmp(result->provenance.provenance_digest, provenance_digest,
                sizeof(provenance_digest)) == 0;
}
