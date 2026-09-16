#include "w_seed_parallel_panic_lifecycle1.h"

#include "w_seed_sha256.h"

#include <limits.h>
#include <string.h>

typedef struct {
  uintptr_t start;
  uintptr_t end;
  bool active;
} panic_lifecycle1_range;

enum {
  W_PANIC_LIFECYCLE1_INPUT_RANGE_CAPACITY = 96u,
  W_PANIC_LIFECYCLE1_WRITABLE_RANGE_CAPACITY = 8u,
};

static bool range_make(const void *pointer, size_t count, size_t element_size,
                       panic_lifecycle1_range *range) {
  if (range == NULL) return false;
  *range = (panic_lifecycle1_range){0u, 0u, false};
  if (count == 0u) return true;
  if (pointer == NULL || element_size == 0u || count > SIZE_MAX / element_size)
    return false;
  const size_t bytes = count * element_size;
  if (bytes > (size_t)UINTPTR_MAX) return false;
  const uintptr_t start = (uintptr_t)pointer;
  if (start > UINTPTR_MAX - (uintptr_t)bytes) return false;
  *range = (panic_lifecycle1_range){start, start + (uintptr_t)bytes, true};
  return true;
}

static bool ranges_overlap(panic_lifecycle1_range left,
                           panic_lifecycle1_range right) {
  return left.active && right.active && left.start < right.end &&
         right.start < left.end;
}

static bool add_range(panic_lifecycle1_range *ranges, size_t capacity,
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

static bool append_hir_ranges(const w_seed_hir0_program *program,
                              panic_lifecycle1_range *ranges, size_t capacity,
                              size_t *count) {
  if (program == NULL || ranges == NULL || count == NULL) return false;
#define W_PANIC_LIFECYCLE1_HIR(pointer, number)                              \
  do {                                                                        \
    if (!add_range(ranges, capacity, count, (pointer), (number),              \
                   sizeof(*(pointer))))                                       \
      return false;                                                           \
  } while (0)
  W_PANIC_LIFECYCLE1_HIR(program, 1u);
  W_PANIC_LIFECYCLE1_HIR(program->modules, program->module_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->identities, program->identity_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->types, program->type_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->enums, program->enum_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->enum_cases, program->enum_case_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->enum_case_parameters,
                         program->enum_case_parameter_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->enum_subset_members,
                         program->enum_subset_member_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->functions, program->function_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->parameters, program->parameter_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->blocks, program->block_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->block_arguments,
                         program->block_argument_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->edge_arguments,
                         program->edge_argument_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->switch_edges,
                         program->switch_edge_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->switch_captures,
                         program->switch_capture_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->instructions,
                         program->instruction_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->bindings, program->binding_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->calls, program->call_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->host_parameters,
                         program->host_parameter_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->arguments, program->argument_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->enum_payloads,
                         program->enum_payload_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->requirements,
                         program->requirement_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->values, program->value_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->interpolation_segments,
                         program->interpolation_segment_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->terminators,
                         program->terminator_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->entries, program->entry_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->text_bytes, program->text_byte_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->value_bytes,
                         program->value_byte_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->receipt, program->receipt_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->external_modules,
                         program->external_module_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->external_symbols,
                         program->external_symbol_capacity);
  W_PANIC_LIFECYCLE1_HIR(program->cleanups, program->cleanup_capacity);
#undef W_PANIC_LIFECYCLE1_HIR
  return true;
}

/* Reconstruct the complete immutable range set that PARPANIC1 verifies. This
 * preflight is intentionally independent of the upstream implementation so a
 * new output cannot alias a producer before the upstream verifier runs. */
static bool append_upstream_ranges(
    const w_seed_parallel_panic_lifecycle1_input *input,
    panic_lifecycle1_range *ranges, size_t capacity, size_t *count) {
  if (input == NULL || ranges == NULL || count == NULL ||
      input->panic_input == NULL || input->panic_workspace == NULL ||
      input->panic_output == NULL || input->panic_result == NULL)
    return false;
  const w_seed_parallel_panic_binding1_input *panic_input =
      input->panic_input;
  const w_seed_parallel_panic_binding1_workspace *panic_workspace =
      input->panic_workspace;
  const w_seed_parallel_panic_binding1_output *panic_output =
      input->panic_output;
#define W_PANIC_LIFECYCLE1_ADD(pointer, number)                              \
  do {                                                                        \
    if (!add_range(ranges, capacity, count, (pointer), (number),              \
                   sizeof(*(pointer))))                                       \
      return false;                                                           \
  } while (0)
  W_PANIC_LIFECYCLE1_ADD(input, 1u);
  W_PANIC_LIFECYCLE1_ADD(panic_input, 1u);
  W_PANIC_LIFECYCLE1_ADD(panic_input->hir_result, 1u);
  if (!append_hir_ranges(panic_input->hir_program, ranges, capacity, count))
    return false;
  W_PANIC_LIFECYCLE1_ADD(panic_input->selection, 1u);
  W_PANIC_LIFECYCLE1_ADD(panic_input->selection->tasks,
                         panic_input->selection->task_capacity);
  W_PANIC_LIFECYCLE1_ADD(panic_input->selection_result, 1u);
  W_PANIC_LIFECYCLE1_ADD(panic_input->invocation, 1u);
  W_PANIC_LIFECYCLE1_ADD(panic_input->invocation->tasks,
                         panic_input->invocation->task_capacity);
  W_PANIC_LIFECYCLE1_ADD(panic_input->invocation->arguments,
                         panic_input->invocation->argument_capacity);
  W_PANIC_LIFECYCLE1_ADD(panic_input->invocation_result, 1u);
  W_PANIC_LIFECYCLE1_ADD(panic_input->provider_authority, 1u);
  W_PANIC_LIFECYCLE1_ADD(panic_workspace, 1u);
  W_PANIC_LIFECYCLE1_ADD(panic_workspace->completions,
                         panic_workspace->completion_capacity);
  W_PANIC_LIFECYCLE1_ADD(panic_workspace->receipt, 1u);
  W_PANIC_LIFECYCLE1_ADD(panic_workspace->provider_kind, 1u);
  W_PANIC_LIFECYCLE1_ADD(panic_output, 1u);
  W_PANIC_LIFECYCLE1_ADD(panic_output->source_id_bytes,
                         panic_output->source_id_capacity);
  W_PANIC_LIFECYCLE1_ADD(panic_output->module_id_bytes,
                         panic_output->module_id_capacity);
  W_PANIC_LIFECYCLE1_ADD(panic_output->message_bytes,
                         panic_output->message_capacity);
  W_PANIC_LIFECYCLE1_ADD(panic_output->signal, 1u);
  W_PANIC_LIFECYCLE1_ADD(input->panic_result, 1u);
#undef W_PANIC_LIFECYCLE1_ADD
  return true;
}

static bool top_level_ranges_disjoint(
    const w_seed_parallel_panic_lifecycle1_input *input,
    const w_seed_parallel_panic_lifecycle1_output *output,
    const w_seed_parallel_panic_lifecycle1_result *result) {
  if (input == NULL || output == NULL || result == NULL) return false;
  panic_lifecycle1_range ranges[3];
  if (!range_make(input, 1u, sizeof(*input), &ranges[0]) ||
      !range_make(output, 1u, sizeof(*output), &ranges[1]) ||
      !range_make(result, 1u, sizeof(*result), &ranges[2]))
    return false;
  for (size_t left = 0u; left < 3u; left += 1u)
    for (size_t right = left + 1u; right < 3u; right += 1u)
      if (ranges_overlap(ranges[left], ranges[right])) return false;
  return true;
}

static bool measure_ranges_disjoint(
    const w_seed_parallel_panic_lifecycle1_input *input,
    const w_seed_parallel_panic_lifecycle1_counts *counts,
    const w_seed_parallel_panic_lifecycle1_result *result) {
  if (input == NULL || counts == NULL || result == NULL) return false;
  panic_lifecycle1_range writable[2];
  if (!range_make(counts, 1u, sizeof(*counts), &writable[0]) ||
      !range_make(result, 1u, sizeof(*result), &writable[1]) ||
      ranges_overlap(writable[0], writable[1]))
    return false;
  panic_lifecycle1_range producers[W_PANIC_LIFECYCLE1_INPUT_RANGE_CAPACITY];
  size_t producer_count = 0u;
  if (!append_upstream_ranges(input, producers,
                              W_PANIC_LIFECYCLE1_INPUT_RANGE_CAPACITY,
                              &producer_count))
    return false;
  for (size_t writable_index = 0u; writable_index < 2u; writable_index += 1u)
    for (size_t producer_index = 0u; producer_index < producer_count;
         producer_index += 1u)
      if (ranges_overlap(writable[writable_index],
                         producers[producer_index]))
        return false;
  return true;
}

static bool run_ranges_disjoint(
    const w_seed_parallel_panic_lifecycle1_input *input,
    const w_seed_parallel_panic_lifecycle1_output *output,
    const w_seed_parallel_panic_lifecycle1_result *result) {
  if (input == NULL || output == NULL || result == NULL) return false;
  panic_lifecycle1_range writable[W_PANIC_LIFECYCLE1_WRITABLE_RANGE_CAPACITY];
  size_t writable_count = 0u;
#define W_PANIC_LIFECYCLE1_RANGE(pointer, number)                            \
  do {                                                                        \
    if (!add_range(writable, W_PANIC_LIFECYCLE1_WRITABLE_RANGE_CAPACITY,      \
                   &writable_count, (pointer), (number), sizeof(*(pointer)))) \
      return false;                                                           \
  } while (0)
  W_PANIC_LIFECYCLE1_RANGE(output, 1u);
  W_PANIC_LIFECYCLE1_RANGE(output->message_bytes, output->message_capacity);
  W_PANIC_LIFECYCLE1_RANGE(output->events, output->event_capacity);
  W_PANIC_LIFECYCLE1_RANGE(output->decision, 1u);
  W_PANIC_LIFECYCLE1_RANGE(result, 1u);
#undef W_PANIC_LIFECYCLE1_RANGE
  for (size_t left = 0u; left < writable_count; left += 1u)
    for (size_t right = left + 1u; right < writable_count; right += 1u)
      if (ranges_overlap(writable[left], writable[right])) return false;

  panic_lifecycle1_range producers[W_PANIC_LIFECYCLE1_INPUT_RANGE_CAPACITY];
  size_t producer_count = 0u;
  if (!append_upstream_ranges(input, producers,
                              W_PANIC_LIFECYCLE1_INPUT_RANGE_CAPACITY,
                              &producer_count))
    return false;
  for (size_t writable_index = 0u; writable_index < writable_count;
       writable_index += 1u)
    for (size_t producer_index = 0u; producer_index < producer_count;
         producer_index += 1u)
      if (ranges_overlap(writable[writable_index],
                         producers[producer_index]))
        return false;
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

static void sha_bytes(w_seed_sha256_state *state, const uint8_t *bytes,
                      size_t count) {
  if (count != 0u) w_seed_sha256_update(state, bytes, count);
}

static void sha_span(w_seed_sha256_state *state, w_seed_span span) {
  sha_u64(state, (uint64_t)span.start_byte);
  sha_u64(state, (uint64_t)span.end_byte);
}

static void sha_bool(w_seed_sha256_state *state, bool value) {
  const uint8_t byte = value ? 1u : 0u;
  w_seed_sha256_update(state, &byte, sizeof(byte));
}

static void sha_authority(
    w_seed_sha256_state *state,
    const w_seed_parallel_local_provider1_receipt *authority) {
  w_seed_sha256_update(state, (const uint8_t *)authority->schema,
                       sizeof(authority->schema));
  sha_u32(state, authority->target);
  sha_u32(state, authority->generation);
  w_seed_sha256_update(state, (const uint8_t *)authority->domain,
                       sizeof(authority->domain));
  w_seed_sha256_update(state, (const uint8_t *)authority->profile,
                       sizeof(authority->profile));
  w_seed_sha256_update(state, (const uint8_t *)authority->identity,
                       sizeof(authority->identity));
  w_seed_sha256_update(state, authority->contract_digest,
                       sizeof(authority->contract_digest));
  sha_u32(state, (uint32_t)authority->assurance);
}

static void sha_platform_receipt(
    w_seed_sha256_state *state,
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

static void sha_event(w_seed_sha256_state *state,
                      const w_seed_parallel_panic_lifecycle1_event *event) {
  sha_u32(state, event->sequence);
  sha_u32(state, (uint32_t)event->kind);
}

static void expected_events(
    w_seed_parallel_panic_lifecycle1_event events[
        W_SEED_PARALLEL_PANIC_LIFECYCLE1_EVENT_COUNT]) {
  events[0] = (w_seed_parallel_panic_lifecycle1_event){
      1u, W_SEED_PARALLEL_PANIC_LIFECYCLE1_EVENT_PANIC_OBSERVED};
  events[1] = (w_seed_parallel_panic_lifecycle1_event){
      2u,
      W_SEED_PARALLEL_PANIC_LIFECYCLE1_EVENT_NORMAL_OUTCOME_PUBLICATION_FORBIDDEN};
  events[2] = (w_seed_parallel_panic_lifecycle1_event){
      3u,
      W_SEED_PARALLEL_PANIC_LIFECYCLE1_EVENT_FAULT_BOUNDARY_TERMINATION_REQUIRED};
}

static void seal_semantic(
    const w_seed_parallel_panic_binding1_signal *signal,
    const w_seed_parallel_panic_lifecycle1_event events[
        W_SEED_PARALLEL_PANIC_LIFECYCLE1_EVENT_COUNT], uint8_t digest[32]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(
      &state,
      (const uint8_t *)"w-seed-parallel-panic-lifecycle1-semantic-1",
      sizeof("w-seed-parallel-panic-lifecycle1-semantic-1") - 1u);
  sha_u32(&state, (uint32_t)signal->panic_code);
  sha_u64(&state, (uint64_t)signal->message_byte_count);
  sha_bytes(&state, signal->message_bytes, signal->message_byte_count);
  sha_u32(&state,
          W_SEED_PARALLEL_PANIC_LIFECYCLE1_STATE_BOUNDARY_TERMINATION_REQUIRED);
  sha_u32(&state, W_SEED_PARALLEL_PANIC_LIFECYCLE1_NORMAL_OUTCOME_NONE);
  sha_u32(
      &state,
      W_SEED_PARALLEL_PANIC_LIFECYCLE1_BOUNDARY_ACTION_TERMINATE_FAULT_BOUNDARY);
  sha_u32(&state,
          W_SEED_PARALLEL_PANIC_LIFECYCLE1_CLEANUP_OWNER_BOUNDARY_HOST);
  sha_u32(&state,
          W_SEED_PARALLEL_PANIC_LIFECYCLE1_USER_CLEANUP_NOT_CLAIMED);
  sha_u32(
      &state,
      W_SEED_PARALLEL_PANIC_LIFECYCLE1_RESOURCE_REGISTRY_NOT_EVALUATED);
  sha_u32(&state, W_SEED_PARALLEL_PANIC_LIFECYCLE1_EVENT_COUNT);
  for (size_t index = 0u;
       index < W_SEED_PARALLEL_PANIC_LIFECYCLE1_EVENT_COUNT; index += 1u)
    sha_event(&state, &events[index]);
  w_seed_sha256_final(&state, digest);
}

static void seal_provenance(
    const w_seed_parallel_panic_binding1_signal *signal,
    const w_seed_parallel_panic_binding1_result *upstream_result,
    const uint8_t semantic_digest[32], uint8_t digest[32]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(
      &state,
      (const uint8_t *)"w-seed-parallel-panic-lifecycle1-provenance-1",
      sizeof("w-seed-parallel-panic-lifecycle1-provenance-1") - 1u);
  w_seed_sha256_update(&state, semantic_digest, 32u);
  w_seed_sha256_update(&state, (const uint8_t *)upstream_result->schema,
                       sizeof(upstream_result->schema));
  sha_u32(&state, upstream_result->task_count);
  sha_u32(&state, upstream_result->panic_count);
  sha_u32(&state, upstream_result->source_task_index);
  sha_u32(&state, upstream_result->generation);
  sha_u32(&state, (uint32_t)signal->panic_code);
  sha_u32(&state, signal->source_task_index);
  sha_u32(&state, signal->lexical_index);
  sha_u32(&state, signal->call_index);
  sha_u32(&state, signal->function_index);
  sha_u32(&state, signal->module_index);
  sha_u32(&state, signal->source_expression);
  sha_u32(&state, signal->terminator_index);
  sha_u32(&state, signal->message_value_index);
  sha_span(&state, signal->function_span);
  sha_span(&state, signal->call_span);
  sha_span(&state, signal->terminator_span);
  sha_u64(&state, (uint64_t)signal->source_length);
  sha_bytes(&state, signal->source_sha256, sizeof(signal->source_sha256));
  sha_u64(&state, (uint64_t)signal->source_id_byte_count);
  sha_bytes(&state, signal->source_id_bytes, signal->source_id_byte_count);
  sha_u64(&state, (uint64_t)signal->module_id_byte_count);
  sha_bytes(&state, signal->module_id_bytes, signal->module_id_byte_count);
  sha_bool(&state, signal->semantic_value_published);
  sha_u32(&state, (uint32_t)signal->provider_kind);
  sha_u32(&state, signal->provider_capacity);
  sha_u32(&state, signal->generation);
  sha_authority(&state, &signal->authority_receipt);
  sha_platform_receipt(&state, &signal->platform_receipt);
  w_seed_sha256_update(&state, signal->hir_semantic_digest, 32u);
  w_seed_sha256_update(&state, signal->hir_provenance_digest, 32u);
  w_seed_sha256_update(&state, signal->selection_semantic_digest, 32u);
  w_seed_sha256_update(&state, signal->invocation_semantic_digest, 32u);
  w_seed_sha256_update(&state, upstream_result->hir_semantic_digest, 32u);
  w_seed_sha256_update(&state, upstream_result->hir_provenance_digest, 32u);
  w_seed_sha256_update(&state, upstream_result->selection_semantic_digest,
                       32u);
  w_seed_sha256_update(&state, upstream_result->invocation_semantic_digest,
                       32u);
  w_seed_sha256_update(&state, upstream_result->provenance_digest, 32u);
  w_seed_sha256_final(&state, digest);
}

static bool upstream_rederives_no_panic(
    const w_seed_parallel_panic_lifecycle1_input *input) {
  if (input == NULL || input->panic_input == NULL) return false;
  w_seed_parallel_panic_binding1_counts counts;
  w_seed_parallel_panic_binding1_result result;
  return w_seed_parallel_panic_binding1_measure(
             input->panic_input, &counts, &result) ==
         W_SEED_PARALLEL_PANIC_BINDING1_NO_PANIC;
}

static bool upstream_valid(
    const w_seed_parallel_panic_lifecycle1_input *input,
    const w_seed_parallel_panic_binding1_signal **signal) {
  if (input == NULL || signal == NULL || input->panic_input == NULL ||
      input->panic_workspace == NULL || input->panic_output == NULL ||
      input->panic_result == NULL)
    return false;
  if (input->panic_result->status != W_SEED_PARALLEL_PANIC_BINDING1_OK ||
      !w_seed_parallel_panic_binding1_verify(
          input->panic_input, input->panic_workspace, input->panic_output,
          input->panic_result) ||
      input->panic_result->panic_count == 0u ||
      input->panic_output->signal == NULL)
    return false;
  *signal = input->panic_output->signal;
  return true;
}

static void result_base(
    const w_seed_parallel_panic_lifecycle1_input *input,
    const w_seed_parallel_panic_binding1_signal *signal,
    const w_seed_parallel_panic_binding1_result *upstream_result,
    w_seed_parallel_panic_lifecycle1_counts required,
    w_seed_parallel_panic_lifecycle1_result *result) {
  (void)memset(result, 0, sizeof(*result));
  result->status = W_SEED_PARALLEL_PANIC_LIFECYCLE1_OK;
  result->required = required;
  (void)memcpy(result->schema,
               W_SEED_PARALLEL_PANIC_LIFECYCLE1_SCHEMA_VERSION,
               sizeof(result->schema));
  result->event_count = W_SEED_PARALLEL_PANIC_LIFECYCLE1_EVENT_COUNT;
  result->panic_count = upstream_result->panic_count;
  result->source_task_index = signal->source_task_index;
  result->lexical_index = signal->lexical_index;
  result->call_index = signal->call_index;
  result->function_index = signal->function_index;
  result->module_index = signal->module_index;
  result->source_expression = signal->source_expression;
  result->terminator_index = signal->terminator_index;
  result->message_value_index = signal->message_value_index;
  result->panic_code = signal->panic_code;
  result->function_span = signal->function_span;
  result->call_span = signal->call_span;
  result->terminator_span = signal->terminator_span;
  result->source_length = signal->source_length;
  (void)memcpy(result->source_sha256, signal->source_sha256,
               sizeof(result->source_sha256));
  result->source_id_bytes = signal->source_id_bytes;
  result->source_id_byte_count = signal->source_id_byte_count;
  result->module_id_bytes = signal->module_id_bytes;
  result->module_id_byte_count = signal->module_id_byte_count;
  (void)memcpy(result->hir_semantic_digest, signal->hir_semantic_digest,
               sizeof(result->hir_semantic_digest));
  (void)memcpy(result->hir_provenance_digest, signal->hir_provenance_digest,
               sizeof(result->hir_provenance_digest));
  (void)memcpy(result->selection_semantic_digest,
               signal->selection_semantic_digest,
               sizeof(result->selection_semantic_digest));
  (void)memcpy(result->invocation_semantic_digest,
               signal->invocation_semantic_digest,
               sizeof(result->invocation_semantic_digest));
  result->provider_kind = signal->provider_kind;
  result->provider_capacity = signal->provider_capacity;
  result->generation = signal->generation;
  result->authority_receipt = signal->authority_receipt;
  result->platform_receipt = signal->platform_receipt;
  (void)input;
}

static void decision_base(
    const w_seed_parallel_panic_lifecycle1_output *output,
    const w_seed_parallel_panic_binding1_signal *signal,
    const uint8_t semantic_digest[32],
    w_seed_parallel_panic_lifecycle1_decision *decision) {
  (void)memset(decision, 0, sizeof(*decision));
  decision->panic_code = signal->panic_code;
  decision->message_bytes = output->message_bytes;
  decision->message_byte_count = signal->message_byte_count;
  decision->state =
      W_SEED_PARALLEL_PANIC_LIFECYCLE1_STATE_BOUNDARY_TERMINATION_REQUIRED;
  decision->normal_outcome =
      W_SEED_PARALLEL_PANIC_LIFECYCLE1_NORMAL_OUTCOME_NONE;
  decision->boundary_action =
      W_SEED_PARALLEL_PANIC_LIFECYCLE1_BOUNDARY_ACTION_TERMINATE_FAULT_BOUNDARY;
  decision->cleanup_owner =
      W_SEED_PARALLEL_PANIC_LIFECYCLE1_CLEANUP_OWNER_BOUNDARY_HOST;
  decision->user_cleanup =
      W_SEED_PARALLEL_PANIC_LIFECYCLE1_USER_CLEANUP_NOT_CLAIMED;
  decision->resource_registry =
      W_SEED_PARALLEL_PANIC_LIFECYCLE1_RESOURCE_REGISTRY_NOT_EVALUATED;
  decision->event_count = W_SEED_PARALLEL_PANIC_LIFECYCLE1_EVENT_COUNT;
  (void)memcpy(decision->semantic_digest, semantic_digest,
               sizeof(decision->semantic_digest));
}

static bool output_capacity_valid(
    const w_seed_parallel_panic_lifecycle1_output *output,
    const w_seed_parallel_panic_binding1_signal *signal) {
  return output != NULL && signal != NULL && output->decision != NULL &&
         output->events != NULL &&
         output->event_capacity == W_SEED_PARALLEL_PANIC_LIFECYCLE1_EVENT_COUNT &&
         output->message_capacity >= signal->message_byte_count &&
         (signal->message_byte_count == 0u || output->message_bytes != NULL);
}

static bool decision_matches(
    const w_seed_parallel_panic_lifecycle1_decision *decision,
    const w_seed_parallel_panic_lifecycle1_output *output,
    const w_seed_parallel_panic_binding1_signal *signal,
    const uint8_t semantic_digest[32]) {
  return decision != NULL && output != NULL && signal != NULL &&
         decision->panic_code == signal->panic_code &&
         decision->message_bytes == output->message_bytes &&
         decision->message_byte_count == signal->message_byte_count &&
         decision->state ==
             W_SEED_PARALLEL_PANIC_LIFECYCLE1_STATE_BOUNDARY_TERMINATION_REQUIRED &&
         decision->normal_outcome ==
             W_SEED_PARALLEL_PANIC_LIFECYCLE1_NORMAL_OUTCOME_NONE &&
         decision->boundary_action ==
             W_SEED_PARALLEL_PANIC_LIFECYCLE1_BOUNDARY_ACTION_TERMINATE_FAULT_BOUNDARY &&
         decision->cleanup_owner ==
             W_SEED_PARALLEL_PANIC_LIFECYCLE1_CLEANUP_OWNER_BOUNDARY_HOST &&
         decision->user_cleanup ==
             W_SEED_PARALLEL_PANIC_LIFECYCLE1_USER_CLEANUP_NOT_CLAIMED &&
         decision->resource_registry ==
             W_SEED_PARALLEL_PANIC_LIFECYCLE1_RESOURCE_REGISTRY_NOT_EVALUATED &&
         decision->event_count == W_SEED_PARALLEL_PANIC_LIFECYCLE1_EVENT_COUNT &&
         memcmp(decision->semantic_digest, semantic_digest,
                sizeof(decision->semantic_digest)) == 0 &&
         bytes_equal(decision->message_bytes, signal->message_bytes,
                     signal->message_byte_count);
}

static bool result_matches(
    const w_seed_parallel_panic_lifecycle1_result *result,
    const w_seed_parallel_panic_lifecycle1_input *input,
    const w_seed_parallel_panic_binding1_signal *signal,
    const w_seed_parallel_panic_binding1_result *upstream_result,
    w_seed_parallel_panic_lifecycle1_counts required,
    const uint8_t provenance_digest[32]) {
  if (result == NULL || input == NULL || signal == NULL ||
      upstream_result == NULL ||
      result->status != W_SEED_PARALLEL_PANIC_LIFECYCLE1_OK ||
      memcmp(result->schema, W_SEED_PARALLEL_PANIC_LIFECYCLE1_SCHEMA_VERSION,
             sizeof(result->schema)) != 0 ||
      memcmp(&result->required, &required, sizeof(required)) != 0 ||
      memcmp(&result->written, &required, sizeof(required)) != 0 ||
      result->event_count != W_SEED_PARALLEL_PANIC_LIFECYCLE1_EVENT_COUNT ||
      result->panic_count != upstream_result->panic_count ||
      result->source_task_index != signal->source_task_index ||
      result->lexical_index != signal->lexical_index ||
      result->call_index != signal->call_index ||
      result->function_index != signal->function_index ||
      result->module_index != signal->module_index ||
      result->source_expression != signal->source_expression ||
      result->terminator_index != signal->terminator_index ||
      result->message_value_index != signal->message_value_index ||
      result->panic_code != signal->panic_code ||
      memcmp(&result->function_span, &signal->function_span,
             sizeof(result->function_span)) != 0 ||
      memcmp(&result->call_span, &signal->call_span,
             sizeof(result->call_span)) != 0 ||
      memcmp(&result->terminator_span, &signal->terminator_span,
             sizeof(result->terminator_span)) != 0 ||
      result->source_length != signal->source_length ||
      memcmp(result->source_sha256, signal->source_sha256,
             sizeof(result->source_sha256)) != 0 ||
      result->source_id_bytes != signal->source_id_bytes ||
      result->source_id_byte_count != signal->source_id_byte_count ||
      result->module_id_bytes != signal->module_id_bytes ||
      result->module_id_byte_count != signal->module_id_byte_count ||
      !bytes_equal(result->source_id_bytes, signal->source_id_bytes,
                   signal->source_id_byte_count) ||
      !bytes_equal(result->module_id_bytes, signal->module_id_bytes,
                   signal->module_id_byte_count) ||
      memcmp(result->hir_semantic_digest, signal->hir_semantic_digest,
             sizeof(result->hir_semantic_digest)) != 0 ||
      memcmp(result->hir_provenance_digest, signal->hir_provenance_digest,
             sizeof(result->hir_provenance_digest)) != 0 ||
      memcmp(result->selection_semantic_digest,
             signal->selection_semantic_digest,
             sizeof(result->selection_semantic_digest)) != 0 ||
      memcmp(result->invocation_semantic_digest,
             signal->invocation_semantic_digest,
             sizeof(result->invocation_semantic_digest)) != 0 ||
      result->provider_kind != signal->provider_kind ||
      result->provider_capacity != signal->provider_capacity ||
      result->generation != signal->generation ||
      !w_seed_parallel_local_provider1_receipt_equal(
          &result->authority_receipt, &signal->authority_receipt) ||
      !w_seed_parallel_platform1_receipt_equal(&result->platform_receipt,
                                                &signal->platform_receipt) ||
      memcmp(result->provenance_digest, provenance_digest,
             sizeof(result->provenance_digest)) != 0)
    return false;
  return true;
}

w_seed_parallel_panic_lifecycle1_status
w_seed_parallel_panic_lifecycle1_measure(
    const w_seed_parallel_panic_lifecycle1_input *input,
    w_seed_parallel_panic_lifecycle1_counts *counts,
    w_seed_parallel_panic_lifecycle1_result *result) {
  if (input == NULL || counts == NULL || result == NULL)
    return W_SEED_PARALLEL_PANIC_LIFECYCLE1_INVALID;
  const w_seed_parallel_panic_binding1_signal *signal = NULL;
  if (!upstream_valid(input, &signal)) {
    if (upstream_rederives_no_panic(input))
      return W_SEED_PARALLEL_PANIC_LIFECYCLE1_NO_PANIC;
    return W_SEED_PARALLEL_PANIC_LIFECYCLE1_UPSTREAM;
  }
  if (!measure_ranges_disjoint(input, counts, result))
    return W_SEED_PARALLEL_PANIC_LIFECYCLE1_ALIAS;
  const w_seed_parallel_panic_lifecycle1_counts required = {
      signal->message_byte_count,
      W_SEED_PARALLEL_PANIC_LIFECYCLE1_EVENT_COUNT};
  w_seed_parallel_panic_lifecycle1_result candidate;
  result_base(input, signal, input->panic_result, required, &candidate);
  candidate.written = (w_seed_parallel_panic_lifecycle1_counts){0u, 0u};
  *counts = required;
  *result = candidate;
  return W_SEED_PARALLEL_PANIC_LIFECYCLE1_OK;
}

w_seed_parallel_panic_lifecycle1_status
w_seed_parallel_panic_lifecycle1_run(
    const w_seed_parallel_panic_lifecycle1_input *input,
    const w_seed_parallel_panic_lifecycle1_output *output,
    w_seed_parallel_panic_lifecycle1_result *result) {
  if (input == NULL || output == NULL || result == NULL ||
      !top_level_ranges_disjoint(input, output, result))
    return W_SEED_PARALLEL_PANIC_LIFECYCLE1_INVALID;
  const w_seed_parallel_panic_binding1_signal *signal = NULL;
  if (!upstream_valid(input, &signal)) {
    if (upstream_rederives_no_panic(input))
      return W_SEED_PARALLEL_PANIC_LIFECYCLE1_NO_PANIC;
    return W_SEED_PARALLEL_PANIC_LIFECYCLE1_UPSTREAM;
  }
  const w_seed_parallel_panic_lifecycle1_counts required = {
      signal->message_byte_count,
      W_SEED_PARALLEL_PANIC_LIFECYCLE1_EVENT_COUNT};
  if (!output_capacity_valid(output, signal))
    return W_SEED_PARALLEL_PANIC_LIFECYCLE1_CAPACITY;
  if (!run_ranges_disjoint(input, output, result))
    return W_SEED_PARALLEL_PANIC_LIFECYCLE1_ALIAS;

  w_seed_parallel_panic_lifecycle1_event candidate_events[
      W_SEED_PARALLEL_PANIC_LIFECYCLE1_EVENT_COUNT];
  expected_events(candidate_events);
  uint8_t semantic_digest[32];
  uint8_t provenance_digest[32];
  seal_semantic(signal, candidate_events, semantic_digest);
  seal_provenance(signal, input->panic_result, semantic_digest,
                  provenance_digest);
  w_seed_parallel_panic_lifecycle1_decision candidate_decision;
  decision_base(output, signal, semantic_digest, &candidate_decision);
  w_seed_parallel_panic_lifecycle1_result candidate_result;
  result_base(input, signal, input->panic_result, required, &candidate_result);
  candidate_result.written = required;
  (void)memcpy(candidate_result.provenance_digest, provenance_digest,
               sizeof(candidate_result.provenance_digest));

  /* The checks and digest construction are complete before this first output
   * write. The remaining copies and assignments are infallible. */
  if (required.message_bytes != 0u)
    (void)memcpy(output->message_bytes, signal->message_bytes,
                 required.message_bytes);
  (void)memcpy(output->events, candidate_events, sizeof(candidate_events));
  *output->decision = candidate_decision;
  *result = candidate_result;
  return W_SEED_PARALLEL_PANIC_LIFECYCLE1_OK;
}

bool w_seed_parallel_panic_lifecycle1_verify(
    const w_seed_parallel_panic_lifecycle1_input *input,
    const w_seed_parallel_panic_lifecycle1_output *output,
    const w_seed_parallel_panic_lifecycle1_result *result) {
  if (input == NULL || output == NULL || result == NULL ||
      !top_level_ranges_disjoint(input, output, result))
    return false;
  const w_seed_parallel_panic_binding1_signal *signal = NULL;
  if (!upstream_valid(input, &signal)) return false;
  const w_seed_parallel_panic_lifecycle1_counts required = {
      signal->message_byte_count,
      W_SEED_PARALLEL_PANIC_LIFECYCLE1_EVENT_COUNT};
  if (!output_capacity_valid(output, signal) ||
      !run_ranges_disjoint(input, output, result))
    return false;
  w_seed_parallel_panic_lifecycle1_event expected[
      W_SEED_PARALLEL_PANIC_LIFECYCLE1_EVENT_COUNT];
  expected_events(expected);
  if (memcmp(output->events, expected, sizeof(expected)) != 0)
    return false;
  uint8_t semantic_digest[32];
  uint8_t provenance_digest[32];
  seal_semantic(signal, expected, semantic_digest);
  seal_provenance(signal, input->panic_result, semantic_digest,
                  provenance_digest);
  return decision_matches(output->decision, output, signal, semantic_digest) &&
         result_matches(result, input, signal, input->panic_result, required,
                        provenance_digest);
}
