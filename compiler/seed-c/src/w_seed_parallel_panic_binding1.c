#include "w_seed_parallel_panic_binding1.h"

#include "w_seed_scalar_evaluator0.h"
#include "w_seed_sha256.h"

#include <limits.h>
#include <string.h>

typedef struct {
  uintptr_t start;
  uintptr_t end;
  bool active;
} panic_binding1_range;

typedef struct {
  uint32_t source_task_index;
  uint32_t lexical_index;
  uint32_t call_index;
  uint32_t function_index;
  uint32_t module_index;
  uint32_t source_expression;
  uint32_t terminator_index;
  uint32_t message_value_index;
  w_seed_hir0_panic_code panic_code;
  w_seed_span function_span;
  w_seed_span call_span;
  w_seed_span terminator_span;
  size_t source_length;
  uint8_t source_sha256[32];
  const uint8_t *source_id_bytes;
  size_t source_id_byte_count;
  const uint8_t *module_id_bytes;
  size_t module_id_byte_count;
  const uint8_t *message_bytes;
  size_t message_byte_count;
} panic_binding1_identity;

typedef struct {
  const w_seed_parallel_panic_binding1_input *input;
} panic_binding1_callback_context;

enum {
  W_PANIC_BINDING1_INPUT_RANGE_CAPACITY = 64u,
  W_PANIC_BINDING1_WRITABLE_RANGE_CAPACITY = 16u,
};

static bool range_make(const void *pointer, size_t count, size_t element_size,
                       panic_binding1_range *range) {
  if (range == NULL) return false;
  *range = (panic_binding1_range){0u, 0u, false};
  if (count == 0u) return true;
  if (pointer == NULL || element_size == 0u || count > SIZE_MAX / element_size)
    return false;
  const size_t bytes = count * element_size;
  const uintptr_t start = (uintptr_t)pointer;
  if (start > UINTPTR_MAX - (uintptr_t)bytes) return false;
  *range = (panic_binding1_range){start, start + (uintptr_t)bytes, true};
  return true;
}

static bool ranges_overlap(panic_binding1_range left,
                           panic_binding1_range right) {
  return left.active && right.active && left.start < right.end &&
         right.start < left.end;
}

static bool add_range(panic_binding1_range *ranges, size_t capacity,
                      size_t *count, const void *pointer, size_t elements,
                      size_t element_size) {
  if (ranges == NULL || count == NULL || *count >= capacity ||
      !range_make(pointer, elements, element_size, &ranges[*count]))
    return false;
  *count += 1u;
  return true;
}

static bool bytes_equal(const uint8_t *left, const uint8_t *right,
                        size_t count) {
  if (count == 0u) return true;
  return left != NULL && right != NULL && memcmp(left, right, count) == 0;
}

static bool padded_literal_equal(const char *bytes, size_t count,
                                 const char *literal) {
  if (bytes == NULL || literal == NULL) return false;
  const size_t literal_count = strlen(literal) + 1u;
  if (literal_count > count ||
      memcmp(bytes, literal, literal_count) != 0)
    return false;
  for (size_t index = literal_count; index < count; index += 1u)
    if (bytes[index] != '\0') return false;
  return true;
}

static bool text_bytes(const w_seed_hir0_program *program,
                       w_seed_hir0_text text, const uint8_t **bytes) {
  if (program == NULL || bytes == NULL ||
      (size_t)text.offset > program->text_byte_count ||
      (size_t)text.count > program->text_byte_count - (size_t)text.offset ||
      (text.count != 0u && program->text_bytes == NULL))
    return false;
  *bytes = text.count == 0u ? NULL : program->text_bytes + text.offset;
  return true;
}

static bool value_bytes(const w_seed_hir0_program *program, uint32_t offset,
                        uint32_t count, const uint8_t **bytes) {
  if (program == NULL || bytes == NULL || (size_t)offset >
                                                 program->value_byte_count ||
      (size_t)count > program->value_byte_count - (size_t)offset ||
      (count != 0u && program->value_bytes == NULL))
    return false;
  *bytes = count == 0u ? NULL : program->value_bytes + offset;
  return true;
}

static bool append_hir_ranges(const w_seed_hir0_program *program,
                              panic_binding1_range *ranges, size_t capacity,
                              size_t *count) {
  if (program == NULL || ranges == NULL || count == NULL) return false;
#define W_PANIC_BINDING1_HIR(pointer, number)                                \
  do {                                                                         \
    if (!add_range(ranges, capacity, count, (pointer), (number),              \
                   sizeof(*(pointer))))                                       \
      return false;                                                            \
  } while (0)
  W_PANIC_BINDING1_HIR(program, 1u);
  W_PANIC_BINDING1_HIR(program->modules, program->module_capacity);
  W_PANIC_BINDING1_HIR(program->identities, program->identity_capacity);
  W_PANIC_BINDING1_HIR(program->types, program->type_capacity);
  W_PANIC_BINDING1_HIR(program->enums, program->enum_capacity);
  W_PANIC_BINDING1_HIR(program->enum_cases, program->enum_case_capacity);
  W_PANIC_BINDING1_HIR(program->enum_case_parameters,
                       program->enum_case_parameter_capacity);
  W_PANIC_BINDING1_HIR(program->enum_subset_members,
                       program->enum_subset_member_capacity);
  W_PANIC_BINDING1_HIR(program->functions, program->function_capacity);
  W_PANIC_BINDING1_HIR(program->parameters, program->parameter_capacity);
  W_PANIC_BINDING1_HIR(program->blocks, program->block_capacity);
  W_PANIC_BINDING1_HIR(program->block_arguments,
                       program->block_argument_capacity);
  W_PANIC_BINDING1_HIR(program->edge_arguments,
                       program->edge_argument_capacity);
  W_PANIC_BINDING1_HIR(program->switch_edges,
                       program->switch_edge_capacity);
  W_PANIC_BINDING1_HIR(program->switch_captures,
                       program->switch_capture_capacity);
  W_PANIC_BINDING1_HIR(program->instructions,
                       program->instruction_capacity);
  W_PANIC_BINDING1_HIR(program->bindings, program->binding_capacity);
  W_PANIC_BINDING1_HIR(program->calls, program->call_capacity);
  W_PANIC_BINDING1_HIR(program->host_parameters,
                       program->host_parameter_capacity);
  W_PANIC_BINDING1_HIR(program->arguments, program->argument_capacity);
  W_PANIC_BINDING1_HIR(program->enum_payloads,
                       program->enum_payload_capacity);
  W_PANIC_BINDING1_HIR(program->requirements,
                       program->requirement_capacity);
  W_PANIC_BINDING1_HIR(program->values, program->value_capacity);
  W_PANIC_BINDING1_HIR(program->interpolation_segments,
                       program->interpolation_segment_capacity);
  W_PANIC_BINDING1_HIR(program->terminators,
                       program->terminator_capacity);
  W_PANIC_BINDING1_HIR(program->entries, program->entry_capacity);
  W_PANIC_BINDING1_HIR(program->text_bytes, program->text_byte_capacity);
  W_PANIC_BINDING1_HIR(program->value_bytes, program->value_byte_capacity);
  W_PANIC_BINDING1_HIR(program->receipt, program->receipt_capacity);
  W_PANIC_BINDING1_HIR(program->external_modules,
                       program->external_module_capacity);
  W_PANIC_BINDING1_HIR(program->external_symbols,
                       program->external_symbol_capacity);
  W_PANIC_BINDING1_HIR(program->cleanups, program->cleanup_capacity);
#undef W_PANIC_BINDING1_HIR
  return true;
}

static bool append_input_ranges(
    const w_seed_parallel_panic_binding1_input *input,
    panic_binding1_range *ranges, size_t capacity, size_t *count) {
  if (input == NULL || ranges == NULL || count == NULL ||
      !add_range(ranges, capacity, count, input, 1u, sizeof(*input)) ||
      !add_range(ranges, capacity, count, input->hir_result, 1u,
                 sizeof(*input->hir_result)) ||
      !append_hir_ranges(input->hir_program, ranges, capacity, count) ||
      !add_range(ranges, capacity, count, input->selection, 1u,
                 sizeof(*input->selection)) ||
      !add_range(ranges, capacity, count, input->selection->tasks,
                 input->selection->task_capacity,
                 sizeof(*input->selection->tasks)) ||
      !add_range(ranges, capacity, count, input->selection_result, 1u,
                 sizeof(*input->selection_result)) ||
      !add_range(ranges, capacity, count, input->invocation, 1u,
                 sizeof(*input->invocation)) ||
      !add_range(ranges, capacity, count, input->invocation->tasks,
                 input->invocation->task_capacity,
                 sizeof(*input->invocation->tasks)) ||
      !add_range(ranges, capacity, count, input->invocation->arguments,
                 input->invocation->argument_capacity,
                 sizeof(*input->invocation->arguments)) ||
      !add_range(ranges, capacity, count, input->invocation_result, 1u,
                 sizeof(*input->invocation_result)) ||
      !add_range(ranges, capacity, count, input->provider_authority, 1u,
                 sizeof(*input->provider_authority)))
    return false;
  return true;
}

static bool measure_ranges_disjoint(
    const w_seed_parallel_panic_binding1_input *input,
    const w_seed_parallel_panic_binding1_counts *counts,
    const w_seed_parallel_panic_binding1_result *result) {
  panic_binding1_range writable[2];
  if (counts == NULL || result == NULL ||
      !range_make(counts, 1u, sizeof(*counts), &writable[0]) ||
      !range_make(result, 1u, sizeof(*result), &writable[1]) ||
      ranges_overlap(writable[0], writable[1]))
    return false;
  panic_binding1_range inputs[W_PANIC_BINDING1_INPUT_RANGE_CAPACITY];
  size_t input_count = 0u;
  if (!append_input_ranges(input, inputs,
                           W_PANIC_BINDING1_INPUT_RANGE_CAPACITY,
                           &input_count))
    return false;
  for (size_t output = 0u; output < 2u; output += 1u)
    for (size_t index = 0u; index < input_count; index += 1u)
      if (ranges_overlap(writable[output], inputs[index])) return false;
  return true;
}

static bool run_ranges_disjoint(
    const w_seed_parallel_panic_binding1_input *input,
    const w_seed_parallel_panic_binding1_workspace *workspace,
    const w_seed_parallel_panic_binding1_output *output,
    const w_seed_parallel_panic_binding1_result *result) {
  if (input == NULL || workspace == NULL || output == NULL || result == NULL)
    return false;
  panic_binding1_range writable[W_PANIC_BINDING1_WRITABLE_RANGE_CAPACITY];
  size_t writable_count = 0u;
#define W_PANIC_BINDING1_RANGE(pointer, number)                              \
  do {                                                                         \
    if (!add_range(writable, W_PANIC_BINDING1_WRITABLE_RANGE_CAPACITY,        \
                   &writable_count, (pointer), (number), sizeof(*(pointer)))) \
      return false;                                                            \
  } while (0)
  W_PANIC_BINDING1_RANGE(workspace, 1u);
  W_PANIC_BINDING1_RANGE(workspace->completions,
                        workspace->completion_capacity);
  W_PANIC_BINDING1_RANGE(workspace->receipt, 1u);
  W_PANIC_BINDING1_RANGE(workspace->provider_kind, 1u);
  W_PANIC_BINDING1_RANGE(output, 1u);
  W_PANIC_BINDING1_RANGE(output->source_id_bytes,
                        output->source_id_capacity);
  W_PANIC_BINDING1_RANGE(output->module_id_bytes,
                        output->module_id_capacity);
  W_PANIC_BINDING1_RANGE(output->message_bytes, output->message_capacity);
  W_PANIC_BINDING1_RANGE(output->signal, 1u);
  W_PANIC_BINDING1_RANGE(result, 1u);
#undef W_PANIC_BINDING1_RANGE
  for (size_t left = 0u; left < writable_count; left += 1u)
    for (size_t right = left + 1u; right < writable_count; right += 1u)
      if (ranges_overlap(writable[left], writable[right])) return false;

  panic_binding1_range inputs[W_PANIC_BINDING1_INPUT_RANGE_CAPACITY];
  size_t input_count = 0u;
  if (!append_input_ranges(input, inputs,
                           W_PANIC_BINDING1_INPUT_RANGE_CAPACITY,
                           &input_count))
    return false;
  for (size_t output_index = 0u; output_index < writable_count;
       output_index += 1u)
    for (size_t input_index = 0u; input_index < input_count; input_index += 1u)
      if (ranges_overlap(writable[output_index], inputs[input_index]))
        return false;
  return true;
}

static bool top_level_ranges_disjoint(
    const w_seed_parallel_panic_binding1_workspace *workspace,
    const w_seed_parallel_panic_binding1_output *output,
    const w_seed_parallel_panic_binding1_result *result) {
  if (workspace == NULL || output == NULL || result == NULL) return true;
  panic_binding1_range ranges[3];
  if (!range_make(workspace, 1u, sizeof(*workspace), &ranges[0]) ||
      !range_make(output, 1u, sizeof(*output), &ranges[1]) ||
      !range_make(result, 1u, sizeof(*result), &ranges[2]))
    return false;
  for (size_t left = 0u; left < 3u; left += 1u)
    for (size_t right = left + 1u; right < 3u; right += 1u)
      if (ranges_overlap(ranges[left], ranges[right])) return false;
  return true;
}

static void sha_u32(w_seed_sha256_state *state, uint32_t value) {
  const uint8_t bytes[4] = {(uint8_t)value, (uint8_t)(value >> 8u),
                            (uint8_t)(value >> 16u),
                            (uint8_t)(value >> 24u)};
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void sha_u64(w_seed_sha256_state *state, uint64_t value) {
  uint8_t bytes[8];
  for (size_t index = 0u; index < sizeof(bytes); index += 1u)
    bytes[index] = (uint8_t)(value >> (index * 8u));
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void sha_bytes(w_seed_sha256_state *state, const uint8_t *bytes,
                      size_t count) {
  if (count != 0u) w_seed_sha256_update(state, bytes, count);
}

static void sha_span(w_seed_sha256_state *state, w_seed_span span) {
  sha_u64(state, (uint64_t)span.start_byte);
  sha_u64(state, (uint64_t)span.end_byte);
}

static void sha_bool(w_seed_sha256_state *state, bool value) {
  sha_u32(state, value ? 1u : 0u);
}

static void sha_completion(
    w_seed_sha256_state *state,
    const w_seed_parallel_platform1_completion *completion) {
  sha_u32(state, (uint32_t)completion->kind);
  sha_u64(state, (uint64_t)completion->success_value);
  sha_u32(state, completion->error_code);
  sha_u32(state, completion->cancel_reason);
  sha_u32(state, completion->error_case_ordinal);
  sha_u32(state, (uint32_t)completion->panic_code);
}

static void sha_receipt(w_seed_sha256_state *state,
                        const w_seed_parallel_platform1_receipt *receipt) {
  sha_u32(state, receipt->started_count);
  sha_u32(state, receipt->settled_count);
  sha_u32(state, receipt->canceled_before_start_count);
  sha_u32(state, receipt->maximum_active);
  sha_u32(state, receipt->cancellation_source_index);
  sha_bool(state, receipt->cancellation_requested);
  sha_u32(state, receipt->panic_source_index);
  sha_u32(state, (uint32_t)receipt->panic_code);
  sha_bool(state, receipt->panic_requested);
}

static void sha_authority(
    w_seed_sha256_state *state,
    const w_seed_parallel_local_provider1_receipt *authority) {
  sha_bytes(state, (const uint8_t *)authority->schema,
            sizeof(authority->schema));
  sha_u32(state, authority->target);
  sha_u32(state, authority->generation);
  sha_bytes(state, (const uint8_t *)authority->domain,
            sizeof(authority->domain));
  sha_bytes(state, (const uint8_t *)authority->profile,
            sizeof(authority->profile));
  sha_bytes(state, (const uint8_t *)authority->identity,
            sizeof(authority->identity));
  sha_bytes(state, authority->contract_digest,
            sizeof(authority->contract_digest));
  sha_u32(state, (uint32_t)authority->assurance);
}

static bool derive_panic_identity(
    const w_seed_hir0_program *program,
    const w_seed_parallel_invocation1_task *task, uint32_t task_index,
    panic_binding1_identity *identity) {
  if (program == NULL || task == NULL || identity == NULL ||
      task->kind != W_SEED_PARALLEL_INVOCATION1_TASK_PANIC ||
      task->function_index >= program->function_count ||
      task->call_index >= program->call_count ||
      task->panic_code != W_SEED_HIR0_PANIC_CODE_EXPLICIT ||
      task->panic_terminator >= program->terminator_count ||
      task->panic_message_value >= program->value_count)
    return false;
  const w_seed_hir0_call *call = &program->calls[task->call_index];
  const w_seed_hir0_function *function =
      &program->functions[task->function_index];
  if (call->execution_kind !=
          W_SEED_HIR0_CALL_STRUCTURED_ASYNC_PARALLEL_DOMAIN_DISPATCH ||
      call->result_type >= program->type_count ||
      program->types[call->result_type].kind != W_SEED_HIR0_TYPE_I64 ||
      function->return_type >= program->type_count ||
      program->types[function->return_type].kind != W_SEED_HIR0_TYPE_I64 ||
      function->module_index >= program->module_count ||
      function->block_count != 1u ||
      function->first_block >= program->block_count)
    return false;
  const w_seed_hir0_identity *callee_identity =
      call->callee_identity < program->identity_count
          ? &program->identities[call->callee_identity]
          : NULL;
  if (callee_identity == NULL ||
      callee_identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
      callee_identity->target_index != task->function_index)
    return false;
  const w_seed_hir0_block *block = &program->blocks[function->first_block];
  if (block->owner_function != task->function_index ||
      block->instruction_count != 0u ||
      block->terminator_index != task->panic_terminator ||
      block->terminator_index >= program->terminator_count)
    return false;
  const w_seed_hir0_terminator *terminator =
      &program->terminators[task->panic_terminator];
  if (terminator->owner_block != function->first_block ||
      terminator->kind != W_SEED_HIR0_TERMINATOR_PANIC ||
      terminator->call_index != W_SEED_HIR0_NONE ||
      terminator->error_type != W_SEED_HIR0_NONE ||
      terminator->result_type >= program->type_count ||
      program->types[terminator->result_type].kind !=
          W_SEED_HIR0_TYPE_NEVER ||
      terminator->value_index != task->panic_message_value ||
      terminator->panic_code != task->panic_code)
    return false;
  const w_seed_hir0_value *message =
      &program->values[task->panic_message_value];
  if (message->kind != W_SEED_HIR0_VALUE_CONST_STRING ||
      message->owner_kind != W_SEED_HIR0_VALUE_OWNER_TERMINATOR ||
      message->owner_index != task->panic_terminator ||
      message->owner_ordinal != 0u || message->type_index >= program->type_count ||
      program->types[message->type_index].kind != W_SEED_HIR0_TYPE_STRING)
    return false;
  const uint8_t *source_id = NULL;
  const uint8_t *module_id = NULL;
  const uint8_t *message_bytes = NULL;
  const w_seed_hir0_module *module = &program->modules[function->module_index];
  if (!text_bytes(program, module->source_id, &source_id) ||
      !text_bytes(program, module->module_id, &module_id) ||
      !value_bytes(program, message->byte_offset, message->byte_count,
                   &message_bytes))
    return false;
  (void)memset(identity, 0, sizeof(*identity));
  identity->source_task_index = task_index;
  identity->lexical_index = task_index;
  identity->call_index = task->call_index;
  identity->function_index = task->function_index;
  identity->module_index = function->module_index;
  identity->source_expression = call->source_expression;
  identity->terminator_index = task->panic_terminator;
  identity->message_value_index = task->panic_message_value;
  identity->panic_code = task->panic_code;
  identity->function_span = function->source_span;
  identity->call_span = call->source_span;
  identity->terminator_span = terminator->source_span;
  identity->source_length = module->source_length;
  (void)memcpy(identity->source_sha256, module->source_sha256,
               sizeof(identity->source_sha256));
  identity->source_id_bytes = source_id;
  identity->source_id_byte_count = module->source_id.count;
  identity->module_id_bytes = module_id;
  identity->module_id_byte_count = module->module_id.count;
  identity->message_bytes = message_bytes;
  identity->message_byte_count = message->byte_count;
  return true;
}

static bool identity_bytes_equal(const panic_binding1_identity *left,
                                 const panic_binding1_identity *right) {
  return left != NULL && right != NULL &&
         left->source_id_byte_count == right->source_id_byte_count &&
         left->module_id_byte_count == right->module_id_byte_count &&
         left->message_byte_count == right->message_byte_count &&
         bytes_equal(left->source_id_bytes, right->source_id_bytes,
                     left->source_id_byte_count) &&
         bytes_equal(left->module_id_bytes, right->module_id_bytes,
                     left->module_id_byte_count) &&
         bytes_equal(left->message_bytes, right->message_bytes,
                     left->message_byte_count);
}

static bool identity_equal(const panic_binding1_identity *left,
                           const panic_binding1_identity *right) {
  return left != NULL && right != NULL &&
         left->source_task_index == right->source_task_index &&
         left->lexical_index == right->lexical_index &&
         left->call_index == right->call_index &&
         left->function_index == right->function_index &&
         left->module_index == right->module_index &&
         left->source_expression == right->source_expression &&
         left->terminator_index == right->terminator_index &&
         left->message_value_index == right->message_value_index &&
         left->panic_code == right->panic_code &&
         memcmp(&left->function_span, &right->function_span,
                sizeof(left->function_span)) == 0 &&
         memcmp(&left->call_span, &right->call_span,
                sizeof(left->call_span)) == 0 &&
         memcmp(&left->terminator_span, &right->terminator_span,
                sizeof(left->terminator_span)) == 0 &&
         left->source_length == right->source_length &&
         memcmp(left->source_sha256, right->source_sha256,
                sizeof(left->source_sha256)) == 0 &&
         identity_bytes_equal(left, right);
}

static w_seed_parallel_panic_binding1_status validate_input(
    const w_seed_parallel_panic_binding1_input *input,
    panic_binding1_identity *primary, uint32_t *panic_count) {
  if (input == NULL || primary == NULL || panic_count == NULL ||
      input->hir_program == NULL || input->hir_result == NULL ||
      input->selection == NULL || input->selection_result == NULL ||
      input->invocation == NULL || input->invocation_result == NULL ||
      input->provider_authority == NULL || input->generation == 0u ||
      (input->provider_capacity != 1u && input->provider_capacity != 2u))
    return W_SEED_PARALLEL_PANIC_BINDING1_INVALID;
  if (!w_seed_hir0_verify(input->hir_program, input->hir_result))
    return W_SEED_PARALLEL_PANIC_BINDING1_HIR;
  if (!w_seed_parallel_local_provider1_verify(input->provider_authority))
    return W_SEED_PARALLEL_PANIC_BINDING1_AUTHORITY;
  if (input->provider_authority->receipt.target !=
           W_SEED_PARALLEL_LOCAL_PROVIDER1_WINDOWS_AMD64 ||
       !padded_literal_equal(
           input->provider_authority->receipt.domain,
           sizeof(input->provider_authority->receipt.domain),
           W_SEED_PARALLEL_LOCAL_PROVIDER1_WINDOWS_DOMAIN) ||
       !padded_literal_equal(
           input->provider_authority->receipt.profile,
           sizeof(input->provider_authority->receipt.profile),
           W_SEED_PARALLEL_LOCAL_PROVIDER1_WINDOWS_PROFILE) ||
       !padded_literal_equal(
           input->provider_authority->receipt.identity,
           sizeof(input->provider_authority->receipt.identity),
           W_SEED_PARALLEL_LOCAL_PROVIDER1_WINDOWS_IDENTITY))
    return W_SEED_PARALLEL_PANIC_BINDING1_AUTHORITY;
  if (!w_seed_parallel_selection1_verify(
          input->hir_program, input->hir_result, input->selection,
          input->selection_result) ||
      !w_seed_parallel_invocation1_verify(
          input->hir_program, input->hir_result, input->selection,
          input->selection_result, input->invocation,
          input->invocation_result) ||
      input->selection->task_count == 0u ||
      input->selection->task_count != input->invocation->task_count ||
      input->selection->task_count > UINT32_MAX ||
      input->selection->placement !=
          W_SEED_HIR0_CALL_PLACEMENT_PARALLEL_DOMAIN ||
      input->selection->domain_mode != W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT ||
       input->selection->domain_capabilities !=
           W_SEED_FRONTEND_DOMAIN_CAPABILITY_PARALLEL)
    return W_SEED_PARALLEL_PANIC_BINDING1_HIR;

  *panic_count = 0u;
  bool found = false;
  for (size_t task_index = 0u; task_index < input->invocation->task_count;
       task_index += 1u) {
    const w_seed_parallel_invocation1_task *task =
        &input->invocation->tasks[task_index];
    if (task->kind == W_SEED_PARALLEL_INVOCATION1_TASK_VALUE_I64) {
      if (task->panic_terminator != W_SEED_HIR0_NONE ||
          task->panic_message_value != W_SEED_HIR0_NONE ||
          task->panic_code != W_SEED_HIR0_PANIC_CODE_INVALID)
        return W_SEED_PARALLEL_PANIC_BINDING1_HIR;
      continue;
    }
    if (task->kind != W_SEED_PARALLEL_INVOCATION1_TASK_PANIC ||
        *panic_count == UINT32_MAX)
      return W_SEED_PARALLEL_PANIC_BINDING1_HIR;
    panic_binding1_identity candidate;
    if (!derive_panic_identity(input->hir_program, task,
                               (uint32_t)task_index, &candidate))
      return W_SEED_PARALLEL_PANIC_BINDING1_HIR;
    if (!found) {
      *primary = candidate;
      found = true;
    }
    *panic_count += 1u;
  }
  return found ? W_SEED_PARALLEL_PANIC_BINDING1_OK
               : W_SEED_PARALLEL_PANIC_BINDING1_NO_PANIC;
}

static bool callback_invoke(void *raw, size_t task_index,
                            w_seed_parallel_platform1_completion *completion) {
  const panic_binding1_callback_context *context =
      (const panic_binding1_callback_context *)raw;
  if (context == NULL || context->input == NULL || completion == NULL ||
      task_index >= context->input->invocation->task_count)
    return false;
  (void)memset(completion, 0, sizeof(*completion));
  const w_seed_parallel_invocation1_task *task =
      &context->input->invocation->tasks[task_index];
  if (task->kind == W_SEED_PARALLEL_INVOCATION1_TASK_VALUE_I64) {
    int64_t value = 0;
    size_t budget = W_SEED_PARALLEL_INVOCATION1_STEP_BUDGET;
    if (!w_seed_scalar_evaluator0_evaluate_call(
            context->input->hir_program, task->call_index, &budget, &value))
      return false;
    completion->kind = W_SEED_PARALLEL_PLATFORM1_COMPLETION_SUCCESS;
    completion->success_value = value;
    return true;
  }
  if (task->kind != W_SEED_PARALLEL_INVOCATION1_TASK_PANIC ||
      task->panic_code != W_SEED_HIR0_PANIC_CODE_EXPLICIT)
    return false;
  completion->kind = W_SEED_PARALLEL_PLATFORM1_COMPLETION_PANIC;
  completion->panic_code = W_SEED_PARALLEL_PLATFORM1_PANIC_EXPLICIT;
  return true;
}

static bool completion_is_success(
    const w_seed_parallel_platform1_completion *completion, int64_t expected) {
  return completion != NULL &&
         completion->kind == W_SEED_PARALLEL_PLATFORM1_COMPLETION_SUCCESS &&
         completion->success_value == expected && completion->error_code == 0u &&
         completion->cancel_reason == 0u &&
         completion->error_case_ordinal == 0u &&
         completion->panic_code == W_SEED_PARALLEL_PLATFORM1_PANIC_NONE;
}

static bool completion_is_panic(
    const w_seed_parallel_platform1_completion *completion) {
  return completion != NULL &&
         completion->kind == W_SEED_PARALLEL_PLATFORM1_COMPLETION_PANIC &&
         completion->success_value == 0 && completion->error_code == 0u &&
         completion->cancel_reason == 0u &&
         completion->error_case_ordinal == 0u &&
         completion->panic_code == W_SEED_PARALLEL_PLATFORM1_PANIC_EXPLICIT;
}

static bool completion_is_panic_cancel(
    const w_seed_parallel_platform1_completion *completion) {
  return completion != NULL &&
         completion->kind == W_SEED_PARALLEL_PLATFORM1_COMPLETION_CANCELED &&
         completion->success_value == 0 && completion->error_code == 0u &&
         completion->cancel_reason ==
             W_SEED_PARALLEL_PLATFORM1_CANCEL_PANIC_BOUNDARY &&
         completion->error_case_ordinal == 0u &&
         completion->panic_code == W_SEED_PARALLEL_PLATFORM1_PANIC_NONE;
}

static bool physical_facts_valid(
    const w_seed_parallel_panic_binding1_input *input,
    const panic_binding1_identity *primary, uint32_t panic_count,
    const w_seed_parallel_platform1_completion *completions,
    const w_seed_parallel_platform1_receipt *receipt,
    w_seed_parallel_provider0_kind provider_kind) {
  if (input == NULL || primary == NULL || completions == NULL ||
      receipt == NULL || provider_kind !=
                                 W_SEED_PARALLEL_PROVIDER0_KIND_WINDOWS_KERNEL32 ||
      panic_count == 0u || primary->source_task_index >=
                                input->invocation->task_count ||
      receipt->panic_source_index != primary->source_task_index ||
      receipt->panic_code != W_SEED_PARALLEL_PLATFORM1_PANIC_EXPLICIT ||
      !receipt->panic_requested || !receipt->cancellation_requested ||
      receipt->cancellation_source_index != primary->source_task_index)
    return false;
  const size_t task_count = input->invocation->task_count;
  const size_t wave_start =
      ((size_t)primary->source_task_index / input->provider_capacity) *
      input->provider_capacity;
  const size_t wave_size =
      task_count - wave_start < input->provider_capacity
          ? task_count - wave_start
          : input->provider_capacity;
  const size_t wave_end = wave_start + wave_size;
  const uint32_t expected_maximum =
      (uint32_t)(task_count < input->provider_capacity ? task_count
                                                       : input->provider_capacity);
  if (wave_start >= task_count || primary->source_task_index >= wave_end ||
      wave_end > UINT32_MAX || task_count - wave_end > UINT32_MAX ||
      receipt->started_count != (uint32_t)wave_end ||
      receipt->settled_count != (uint32_t)wave_end ||
      receipt->canceled_before_start_count != (uint32_t)(task_count - wave_end) ||
      receipt->maximum_active != expected_maximum)
    return false;

  uint32_t observed_panics = 0u;
  for (size_t task_index = 0u; task_index < task_count; task_index += 1u) {
    const w_seed_parallel_invocation1_task *task =
        &input->invocation->tasks[task_index];
    const w_seed_parallel_platform1_completion *completion =
        &completions[task_index];
    if (task_index >= wave_end) {
      if (!completion_is_panic_cancel(completion)) return false;
      continue;
    }
    if (task->kind == W_SEED_PARALLEL_INVOCATION1_TASK_VALUE_I64) {
      int64_t expected = 0;
      size_t budget = W_SEED_PARALLEL_INVOCATION1_STEP_BUDGET;
      if (!w_seed_scalar_evaluator0_evaluate_call(
              input->hir_program, task->call_index, &budget, &expected) ||
          !completion_is_success(completion, expected))
        return false;
      continue;
    }
    if (task->kind != W_SEED_PARALLEL_INVOCATION1_TASK_PANIC ||
        task->panic_code != W_SEED_HIR0_PANIC_CODE_EXPLICIT ||
        !completion_is_panic(completion))
      return false;
    observed_panics += 1u;
  }
  return observed_panics != 0u && observed_panics <= panic_count;
}

static void seal_semantic(const w_seed_parallel_panic_binding1_input *input,
                          const panic_binding1_identity *identity,
                          uint8_t digest[32]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(
      &state,
      (const uint8_t *)W_SEED_PARALLEL_PANIC_BINDING1_SCHEMA_VERSION,
      sizeof(W_SEED_PARALLEL_PANIC_BINDING1_SCHEMA_VERSION) - 1u);
  w_seed_sha256_update(&state, input->hir_result->semantic_digest, 32u);
  w_seed_sha256_update(&state, input->selection->semantic_digest, 32u);
  w_seed_sha256_update(&state, input->invocation->semantic_digest, 32u);
  sha_u32(&state, identity->source_task_index);
  sha_u32(&state, identity->lexical_index);
  sha_u32(&state, identity->call_index);
  sha_u32(&state, identity->function_index);
  sha_u32(&state, identity->module_index);
  sha_u32(&state, identity->source_expression);
  sha_u32(&state, identity->terminator_index);
  sha_u32(&state, identity->message_value_index);
  sha_u32(&state, (uint32_t)identity->panic_code);
  sha_u64(&state, (uint64_t)identity->source_id_byte_count);
  sha_bytes(&state, identity->source_id_bytes, identity->source_id_byte_count);
  sha_u64(&state, (uint64_t)identity->module_id_byte_count);
  sha_bytes(&state, identity->module_id_bytes, identity->module_id_byte_count);
  sha_bytes(&state, identity->source_sha256, sizeof(identity->source_sha256));
  sha_u64(&state, (uint64_t)identity->source_length);
  sha_span(&state, identity->function_span);
  sha_span(&state, identity->call_span);
  sha_span(&state, identity->terminator_span);
  sha_u64(&state, (uint64_t)identity->message_byte_count);
  sha_bytes(&state, identity->message_bytes, identity->message_byte_count);
  w_seed_sha256_final(&state, digest);
}

static void seal_provenance(
    const w_seed_parallel_panic_binding1_input *input,
    w_seed_parallel_provider0_kind provider_kind,
    const w_seed_parallel_platform1_completion *completions,
    const w_seed_parallel_platform1_receipt *receipt,
    const uint8_t semantic_digest[32], uint8_t digest[32]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(
      &state, (const uint8_t *)"w-seed-parallel-panic-binding1-provenance-1",
      sizeof("w-seed-parallel-panic-binding1-provenance-1") - 1u);
  w_seed_sha256_update(&state, semantic_digest, 32u);
  w_seed_sha256_update(&state, input->hir_result->provenance_digest, 32u);
  sha_authority(&state, &input->provider_authority->receipt);
  sha_u32(&state, (uint32_t)provider_kind);
  sha_u32(&state, input->provider_capacity);
  sha_u32(&state, input->generation);
  sha_u32(&state, (uint32_t)input->invocation->task_count);
  for (size_t index = 0u; index < input->invocation->task_count; index += 1u)
    sha_completion(&state, &completions[index]);
  sha_receipt(&state, receipt);
  w_seed_sha256_final(&state, digest);
}

static void result_base(const w_seed_parallel_panic_binding1_input *input,
                        const panic_binding1_identity *identity,
                        uint32_t panic_count,
                        w_seed_parallel_panic_binding1_counts required,
                        w_seed_parallel_panic_binding1_result *result) {
  (void)memset(result, 0, sizeof(*result));
  result->status = W_SEED_PARALLEL_PANIC_BINDING1_OK;
  result->required = required;
  (void)memcpy(result->schema,
               W_SEED_PARALLEL_PANIC_BINDING1_SCHEMA_VERSION,
               sizeof(result->schema));
  result->task_count = (uint32_t)input->invocation->task_count;
  result->panic_count = panic_count;
  result->source_task_index = identity->source_task_index;
  result->generation = input->generation;
  (void)memcpy(result->hir_semantic_digest,
               input->hir_result->semantic_digest,
               sizeof(result->hir_semantic_digest));
  (void)memcpy(result->hir_provenance_digest,
               input->hir_result->provenance_digest,
               sizeof(result->hir_provenance_digest));
  (void)memcpy(result->selection_semantic_digest,
               input->selection->semantic_digest,
               sizeof(result->selection_semantic_digest));
  (void)memcpy(result->invocation_semantic_digest,
               input->invocation->semantic_digest,
               sizeof(result->invocation_semantic_digest));
}

static void signal_base(const w_seed_parallel_panic_binding1_output *output,
                        const w_seed_parallel_panic_binding1_input *input,
                        const panic_binding1_identity *identity,
                        const w_seed_parallel_platform1_receipt *receipt,
                        w_seed_parallel_provider0_kind provider_kind,
                        const uint8_t semantic_digest[32],
                        const uint8_t provenance_digest[32],
                        w_seed_parallel_panic_binding1_signal *signal) {
  (void)memset(signal, 0, sizeof(*signal));
  signal->source_task_index = identity->source_task_index;
  signal->lexical_index = identity->lexical_index;
  signal->call_index = identity->call_index;
  signal->function_index = identity->function_index;
  signal->module_index = identity->module_index;
  signal->source_expression = identity->source_expression;
  signal->terminator_index = identity->terminator_index;
  signal->message_value_index = identity->message_value_index;
  signal->panic_code = identity->panic_code;
  signal->semantic_value_published = false;
  signal->function_span = identity->function_span;
  signal->call_span = identity->call_span;
  signal->terminator_span = identity->terminator_span;
  signal->source_length = identity->source_length;
  (void)memcpy(signal->source_sha256, identity->source_sha256,
               sizeof(signal->source_sha256));
  signal->source_id_bytes = output->source_id_bytes;
  signal->source_id_byte_count = identity->source_id_byte_count;
  signal->module_id_bytes = output->module_id_bytes;
  signal->module_id_byte_count = identity->module_id_byte_count;
  signal->message_bytes = output->message_bytes;
  signal->message_byte_count = identity->message_byte_count;
  signal->provider_kind = provider_kind;
  signal->provider_capacity = input->provider_capacity;
  signal->generation = input->generation;
  signal->authority_receipt = input->provider_authority->receipt;
  signal->platform_receipt = *receipt;
  (void)memcpy(signal->hir_semantic_digest,
               input->hir_result->semantic_digest,
               sizeof(signal->hir_semantic_digest));
  (void)memcpy(signal->hir_provenance_digest,
               input->hir_result->provenance_digest,
               sizeof(signal->hir_provenance_digest));
  (void)memcpy(signal->selection_semantic_digest,
               input->selection->semantic_digest,
               sizeof(signal->selection_semantic_digest));
  (void)memcpy(signal->invocation_semantic_digest,
               input->invocation->semantic_digest,
               sizeof(signal->invocation_semantic_digest));
  (void)memcpy(signal->semantic_digest, semantic_digest,
               sizeof(signal->semantic_digest));
  (void)memcpy(signal->provenance_digest, provenance_digest,
               sizeof(signal->provenance_digest));
}

static bool output_capacity_valid(
    const w_seed_parallel_panic_binding1_output *output,
    const w_seed_parallel_panic_binding1_workspace *workspace,
    const w_seed_parallel_panic_binding1_result *result,
    w_seed_parallel_panic_binding1_counts required, size_t task_count) {
  return output != NULL && workspace != NULL && result != NULL &&
         output->signal != NULL && workspace->receipt != NULL &&
         workspace->provider_kind != NULL &&
         workspace->completion_capacity >= task_count &&
         workspace->completions != NULL &&
         output->source_id_capacity >= required.source_id_bytes &&
         output->module_id_capacity >= required.module_id_bytes &&
         output->message_capacity >= required.message_bytes &&
         (required.source_id_bytes == 0u || output->source_id_bytes != NULL) &&
         (required.module_id_bytes == 0u || output->module_id_bytes != NULL) &&
         (required.message_bytes == 0u || output->message_bytes != NULL);
}

static bool signal_matches_identity(
    const w_seed_parallel_panic_binding1_signal *signal,
    const panic_binding1_identity *identity) {
  if (signal == NULL || identity == NULL || signal->source_task_index !=
                                                   identity->source_task_index ||
      signal->lexical_index != identity->lexical_index ||
      signal->call_index != identity->call_index ||
      signal->function_index != identity->function_index ||
      signal->module_index != identity->module_index ||
      signal->source_expression != identity->source_expression ||
      signal->terminator_index != identity->terminator_index ||
      signal->message_value_index != identity->message_value_index ||
      signal->panic_code != identity->panic_code ||
      signal->semantic_value_published ||
      memcmp(&signal->function_span, &identity->function_span,
             sizeof(signal->function_span)) != 0 ||
      memcmp(&signal->call_span, &identity->call_span,
             sizeof(signal->call_span)) != 0 ||
      memcmp(&signal->terminator_span, &identity->terminator_span,
             sizeof(signal->terminator_span)) != 0 ||
      signal->source_length != identity->source_length ||
      memcmp(signal->source_sha256, identity->source_sha256,
             sizeof(signal->source_sha256)) != 0 ||
      signal->source_id_byte_count != identity->source_id_byte_count ||
      signal->module_id_byte_count != identity->module_id_byte_count ||
      signal->message_byte_count != identity->message_byte_count ||
      !bytes_equal(signal->source_id_bytes, identity->source_id_bytes,
                   identity->source_id_byte_count) ||
      !bytes_equal(signal->module_id_bytes, identity->module_id_bytes,
                   identity->module_id_byte_count) ||
      !bytes_equal(signal->message_bytes, identity->message_bytes,
                   identity->message_byte_count))
    return false;
  return true;
}

static bool result_fields_valid(
    const w_seed_parallel_panic_binding1_input *input,
    const panic_binding1_identity *identity, uint32_t panic_count,
    w_seed_parallel_panic_binding1_counts required,
    const w_seed_parallel_panic_binding1_result *result) {
  return input != NULL && identity != NULL && result != NULL &&
         result->status == W_SEED_PARALLEL_PANIC_BINDING1_OK &&
         memcmp(result->schema,
                W_SEED_PARALLEL_PANIC_BINDING1_SCHEMA_VERSION,
                sizeof(result->schema)) == 0 &&
         memcmp(&result->required, &required, sizeof(required)) == 0 &&
         memcmp(&result->written, &required, sizeof(required)) == 0 &&
         result->task_count == input->invocation->task_count &&
         result->panic_count == panic_count &&
         result->source_task_index == identity->source_task_index &&
         result->generation == input->generation &&
         memcmp(result->hir_semantic_digest,
                input->hir_result->semantic_digest,
                sizeof(result->hir_semantic_digest)) == 0 &&
         memcmp(result->hir_provenance_digest,
                input->hir_result->provenance_digest,
                sizeof(result->hir_provenance_digest)) == 0 &&
         memcmp(result->selection_semantic_digest,
                input->selection->semantic_digest,
                sizeof(result->selection_semantic_digest)) == 0 &&
         memcmp(result->invocation_semantic_digest,
                input->invocation->semantic_digest,
                sizeof(result->invocation_semantic_digest)) == 0;
}

w_seed_parallel_panic_binding1_status
w_seed_parallel_panic_binding1_measure(
    const w_seed_parallel_panic_binding1_input *input,
    w_seed_parallel_panic_binding1_counts *counts,
    w_seed_parallel_panic_binding1_result *result) {
  if (counts == NULL || result == NULL)
    return W_SEED_PARALLEL_PANIC_BINDING1_INVALID;
  panic_binding1_identity identity;
  uint32_t panic_count = 0u;
  const w_seed_parallel_panic_binding1_status input_status =
      validate_input(input, &identity, &panic_count);
  if (input_status != W_SEED_PARALLEL_PANIC_BINDING1_OK) return input_status;
  const w_seed_parallel_panic_binding1_counts required = {
      identity.source_id_byte_count, identity.module_id_byte_count,
      identity.message_byte_count};
  if (!measure_ranges_disjoint(input, counts, result))
    return W_SEED_PARALLEL_PANIC_BINDING1_ALIAS;
  w_seed_parallel_panic_binding1_result candidate;
  result_base(input, &identity, panic_count, required, &candidate);
  candidate.written = (w_seed_parallel_panic_binding1_counts){0u, 0u, 0u};
  *counts = required;
  *result = candidate;
  return W_SEED_PARALLEL_PANIC_BINDING1_OK;
}

w_seed_parallel_panic_binding1_status w_seed_parallel_panic_binding1_run(
    const w_seed_parallel_panic_binding1_input *input,
    const w_seed_parallel_panic_binding1_workspace *workspace,
    const w_seed_parallel_panic_binding1_output *output,
    w_seed_parallel_panic_binding1_result *result) {
  panic_binding1_identity identity;
  uint32_t panic_count = 0u;
  const w_seed_parallel_panic_binding1_status input_status =
      validate_input(input, &identity, &panic_count);
  if (input_status != W_SEED_PARALLEL_PANIC_BINDING1_OK) return input_status;
  const w_seed_parallel_panic_binding1_counts required = {
      identity.source_id_byte_count, identity.module_id_byte_count,
      identity.message_byte_count};
  if (!top_level_ranges_disjoint(workspace, output, result))
    return W_SEED_PARALLEL_PANIC_BINDING1_ALIAS;
  if (!output_capacity_valid(output, workspace, result, required,
                             input->invocation->task_count))
    return W_SEED_PARALLEL_PANIC_BINDING1_CAPACITY;
  if (!run_ranges_disjoint(input, workspace, output, result))
    return W_SEED_PARALLEL_PANIC_BINDING1_ALIAS;

  panic_binding1_callback_context callback_context = {input};
  const w_seed_parallel_platform1_job provider_job = {
      callback_invoke, &callback_context, sizeof(callback_context)};
  const w_seed_parallel_provider0_platform_status platform_status =
      w_seed_parallel_local_provider1_execute(
          input->provider_authority, &provider_job,
          input->invocation->task_count, input->provider_capacity,
          workspace->completions, workspace->receipt,
          workspace->provider_kind);
  if (platform_status == W_SEED_PARALLEL_PROVIDER0_PLATFORM_UNSUPPORTED)
    return W_SEED_PARALLEL_PANIC_BINDING1_UNSUPPORTED;
  if (platform_status == W_SEED_PARALLEL_PROVIDER0_PLATFORM_TASK_FAILURE)
    return W_SEED_PARALLEL_PANIC_BINDING1_TASK_FAILURE;
  if (platform_status != W_SEED_PARALLEL_PROVIDER0_PLATFORM_OK)
    return W_SEED_PARALLEL_PANIC_BINDING1_PROVIDER_FAILURE;

  panic_binding1_identity post_identity;
  uint32_t post_panic_count = 0u;
  const w_seed_parallel_panic_binding1_status post_status =
      validate_input(input, &post_identity, &post_panic_count);
  if (post_status != W_SEED_PARALLEL_PANIC_BINDING1_OK ||
      post_panic_count != panic_count ||
      !identity_equal(&identity, &post_identity))
    return post_status == W_SEED_PARALLEL_PANIC_BINDING1_OK
               ? W_SEED_PARALLEL_PANIC_BINDING1_FORGERY
               : post_status;
  if (!physical_facts_valid(input, &identity, panic_count,
                            workspace->completions, workspace->receipt,
                            *workspace->provider_kind))
    return W_SEED_PARALLEL_PANIC_BINDING1_FORGERY;

  uint8_t semantic_digest[32];
  uint8_t provenance_digest[32];
  seal_semantic(input, &identity, semantic_digest);
  seal_provenance(input, *workspace->provider_kind, workspace->completions,
                  workspace->receipt, semantic_digest, provenance_digest);
  w_seed_parallel_panic_binding1_signal signal_candidate;
  signal_base(output, input, &identity, workspace->receipt,
              *workspace->provider_kind, semantic_digest, provenance_digest,
              &signal_candidate);
  w_seed_parallel_panic_binding1_result result_candidate;
  result_base(input, &identity, panic_count, required, &result_candidate);
  result_candidate.written = required;
  (void)memcpy(result_candidate.semantic_digest, semantic_digest,
               sizeof(result_candidate.semantic_digest));
  (void)memcpy(result_candidate.provenance_digest, provenance_digest,
               sizeof(result_candidate.provenance_digest));

  /* This is the sole publication point. All checks and digest construction
   * above are complete before the first caller-owned output byte is written. */
  if (required.source_id_bytes != 0u)
    (void)memcpy(output->source_id_bytes, identity.source_id_bytes,
                 required.source_id_bytes);
  if (required.module_id_bytes != 0u)
    (void)memcpy(output->module_id_bytes, identity.module_id_bytes,
                 required.module_id_bytes);
  if (required.message_bytes != 0u)
    (void)memcpy(output->message_bytes, identity.message_bytes,
                 required.message_bytes);
  *output->signal = signal_candidate;
  *result = result_candidate;
  return W_SEED_PARALLEL_PANIC_BINDING1_OK;
}

bool w_seed_parallel_panic_binding1_verify(
    const w_seed_parallel_panic_binding1_input *input,
    const w_seed_parallel_panic_binding1_workspace *workspace,
    const w_seed_parallel_panic_binding1_output *output,
    const w_seed_parallel_panic_binding1_result *result) {
  panic_binding1_identity identity;
  uint32_t panic_count = 0u;
  if (validate_input(input, &identity, &panic_count) !=
          W_SEED_PARALLEL_PANIC_BINDING1_OK ||
      !top_level_ranges_disjoint(workspace, output, result) ||
      !output_capacity_valid(
          output, workspace, result,
          (w_seed_parallel_panic_binding1_counts){
              identity.source_id_byte_count, identity.module_id_byte_count,
              identity.message_byte_count},
          input->invocation->task_count) ||
      !run_ranges_disjoint(input, workspace, output, result))
    return false;
  const w_seed_parallel_panic_binding1_counts required = {
      identity.source_id_byte_count, identity.module_id_byte_count,
      identity.message_byte_count};
  const w_seed_parallel_panic_binding1_signal *signal = output->signal;
  if (signal->source_id_bytes != output->source_id_bytes ||
      signal->module_id_bytes != output->module_id_bytes ||
      signal->message_bytes != output->message_bytes ||
      signal->provider_kind != *workspace->provider_kind ||
      signal->provider_capacity != input->provider_capacity ||
      signal->generation != input->generation ||
      !w_seed_parallel_local_provider1_receipt_equal(
          &signal->authority_receipt,
          &input->provider_authority->receipt) ||
      !w_seed_parallel_platform1_receipt_equal(
          &signal->platform_receipt, workspace->receipt) ||
      !signal_matches_identity(signal, &identity) ||
      !result_fields_valid(input, &identity, panic_count, required, result))
    return false;
  if (!bytes_equal(signal->source_id_bytes, output->source_id_bytes,
                   signal->source_id_byte_count) ||
      !bytes_equal(signal->module_id_bytes, output->module_id_bytes,
                   signal->module_id_byte_count) ||
      !bytes_equal(signal->message_bytes, output->message_bytes,
                   signal->message_byte_count))
    return false;
  if (!physical_facts_valid(input, &identity, panic_count,
                            workspace->completions, workspace->receipt,
                            *workspace->provider_kind))
    return false;
  uint8_t semantic_digest[32];
  uint8_t provenance_digest[32];
  seal_semantic(input, &identity, semantic_digest);
  seal_provenance(input, *workspace->provider_kind, workspace->completions,
                  workspace->receipt, semantic_digest, provenance_digest);
  return memcmp(signal->hir_semantic_digest, input->hir_result->semantic_digest,
                sizeof(signal->hir_semantic_digest)) == 0 &&
         memcmp(signal->hir_provenance_digest,
                input->hir_result->provenance_digest,
                sizeof(signal->hir_provenance_digest)) == 0 &&
         memcmp(signal->selection_semantic_digest,
                input->selection->semantic_digest,
                sizeof(signal->selection_semantic_digest)) == 0 &&
         memcmp(signal->invocation_semantic_digest,
                input->invocation->semantic_digest,
                sizeof(signal->invocation_semantic_digest)) == 0 &&
         memcmp(signal->semantic_digest, semantic_digest,
                sizeof(signal->semantic_digest)) == 0 &&
         memcmp(signal->provenance_digest, provenance_digest,
                sizeof(signal->provenance_digest)) == 0 &&
         memcmp(result->semantic_digest, semantic_digest,
                sizeof(result->semantic_digest)) == 0 &&
         memcmp(result->provenance_digest, provenance_digest,
                sizeof(result->provenance_digest)) == 0;
}
