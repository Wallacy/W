#include "w_seed_parallel_panic_host_registry1.h"

#include "w_seed_sha256.h"

#include <limits.h>
#include <string.h>

#if defined(_WIN32) && defined(_WIN64)
#include <windows.h>

typedef struct {
  uintptr_t start;
  uintptr_t end;
  bool active;
} panic_host_registry1_range;

typedef struct {
  const w_seed_parallel_panic_lifecycle1_input *lifecycle_input;
  const w_seed_parallel_panic_lifecycle1_output *lifecycle_output;
  const w_seed_parallel_panic_lifecycle1_result *lifecycle_result;
  const w_seed_parallel_panic_binding1_input *panic_input;
  const w_seed_parallel_panic_binding1_output *panic_output;
  const w_seed_parallel_panic_binding1_result *panic_result;
  const w_seed_parallel_panic_binding1_signal *signal;
} panic_host_registry1_validation;

enum {
  W_PANIC_HOST_REGISTRY1_RANGE_CAPACITY = 128u,
  W_PANIC_HOST_REGISTRY1_WRITABLE_RANGE_CAPACITY = 8u,
};

static const uint8_t panic_host_registry1_seal = 0x93u;

static bool range_make(const void *pointer, size_t count, size_t element_size,
                       panic_host_registry1_range *range) {
  if (range == NULL) return false;
  *range = (panic_host_registry1_range){0u, 0u, false};
  if (count == 0u) return true;
  if (pointer == NULL || element_size == 0u || count > SIZE_MAX / element_size)
    return false;
  const size_t bytes = count * element_size;
  if (bytes > (size_t)UINTPTR_MAX) return false;
  const uintptr_t start = (uintptr_t)pointer;
  if (start > UINTPTR_MAX - (uintptr_t)bytes) return false;
  *range = (panic_host_registry1_range){start, start + (uintptr_t)bytes,
                                        true};
  return true;
}

static bool ranges_overlap(panic_host_registry1_range left,
                           panic_host_registry1_range right) {
  return left.active && right.active && left.start < right.end &&
         right.start < left.end;
}

static bool add_range(panic_host_registry1_range *ranges, size_t capacity,
                      size_t *count, const void *pointer, size_t elements,
                      size_t element_size) {
  if (ranges == NULL || count == NULL || *count >= capacity ||
      !range_make(pointer, elements, element_size, &ranges[*count]))
    return false;
  *count += 1u;
  return true;
}

static bool append_hir_ranges(const w_seed_hir0_program *program,
                              panic_host_registry1_range *ranges,
                              size_t capacity, size_t *count) {
  if (program == NULL || ranges == NULL || count == NULL) return false;
#define W_PANIC_HOST_REGISTRY1_HIR(pointer, number)                         \
  do {                                                                       \
    if (!add_range(ranges, capacity, count, (pointer), (number),             \
                   sizeof(*(pointer))))                                      \
      return false;                                                          \
  } while (0)
  W_PANIC_HOST_REGISTRY1_HIR(program, 1u);
  W_PANIC_HOST_REGISTRY1_HIR(program->modules, program->module_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->identities, program->identity_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->types, program->type_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->enums, program->enum_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->enum_cases, program->enum_case_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->enum_case_parameters,
                             program->enum_case_parameter_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->enum_subset_members,
                             program->enum_subset_member_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->functions, program->function_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->parameters, program->parameter_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->blocks, program->block_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->block_arguments,
                             program->block_argument_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->edge_arguments,
                             program->edge_argument_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->switch_edges,
                             program->switch_edge_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->switch_captures,
                             program->switch_capture_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->instructions,
                             program->instruction_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->bindings, program->binding_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->calls, program->call_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->host_parameters,
                             program->host_parameter_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->arguments, program->argument_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->enum_payloads,
                             program->enum_payload_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->requirements,
                             program->requirement_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->values, program->value_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->interpolation_segments,
                             program->interpolation_segment_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->terminators,
                             program->terminator_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->entries, program->entry_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->text_bytes, program->text_byte_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->value_bytes,
                             program->value_byte_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->receipt, program->receipt_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->external_modules,
                             program->external_module_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->external_symbols,
                             program->external_symbol_capacity);
  W_PANIC_HOST_REGISTRY1_HIR(program->cleanups, program->cleanup_capacity);
#undef W_PANIC_HOST_REGISTRY1_HIR
  return true;
}

/* The new output must not alias any producer range, including the complete
 * HIR/selection/invocation graph that the upstream verifier reads. */
static bool append_producer_ranges(
    const w_seed_parallel_panic_host_registry1_input *input,
    panic_host_registry1_range *ranges, size_t capacity, size_t *count) {
  if (input == NULL || ranges == NULL || count == NULL ||
      input->lifecycle_input == NULL || input->lifecycle_output == NULL ||
      input->lifecycle_result == NULL ||
      input->lifecycle_input->panic_input == NULL ||
      input->lifecycle_input->panic_workspace == NULL ||
      input->lifecycle_input->panic_output == NULL ||
      input->lifecycle_input->panic_result == NULL ||
      input->lifecycle_input->panic_input->hir_result == NULL ||
      input->lifecycle_input->panic_input->hir_program == NULL ||
      input->lifecycle_input->panic_input->selection == NULL ||
      input->lifecycle_input->panic_input->selection_result == NULL ||
      input->lifecycle_input->panic_input->invocation == NULL ||
      input->lifecycle_input->panic_input->invocation_result == NULL ||
      input->lifecycle_input->panic_input->provider_authority == NULL ||
      input->lifecycle_input->panic_output->signal == NULL)
    return false;
  const w_seed_parallel_panic_binding1_input *panic_input =
      input->lifecycle_input->panic_input;
  const w_seed_parallel_panic_binding1_workspace *panic_workspace =
      input->lifecycle_input->panic_workspace;
  const w_seed_parallel_panic_binding1_output *panic_output =
      input->lifecycle_input->panic_output;
  const w_seed_parallel_panic_lifecycle1_output *lifecycle_output =
      input->lifecycle_output;
#define W_PANIC_HOST_REGISTRY1_ADD(pointer, number)                         \
  do {                                                                       \
    if (!add_range(ranges, capacity, count, (pointer), (number),             \
                   sizeof(*(pointer))))                                      \
      return false;                                                          \
  } while (0)
  W_PANIC_HOST_REGISTRY1_ADD(input, 1u);
  W_PANIC_HOST_REGISTRY1_ADD(input->lifecycle_input, 1u);
  W_PANIC_HOST_REGISTRY1_ADD(panic_input, 1u);
  W_PANIC_HOST_REGISTRY1_ADD(panic_input->hir_result, 1u);
  if (!append_hir_ranges(panic_input->hir_program, ranges, capacity, count))
    return false;
  W_PANIC_HOST_REGISTRY1_ADD(panic_input->selection, 1u);
  W_PANIC_HOST_REGISTRY1_ADD(panic_input->selection->tasks,
                             panic_input->selection->task_capacity);
  W_PANIC_HOST_REGISTRY1_ADD(panic_input->selection_result, 1u);
  W_PANIC_HOST_REGISTRY1_ADD(panic_input->invocation, 1u);
  W_PANIC_HOST_REGISTRY1_ADD(panic_input->invocation->tasks,
                             panic_input->invocation->task_capacity);
  W_PANIC_HOST_REGISTRY1_ADD(panic_input->invocation->arguments,
                             panic_input->invocation->argument_capacity);
  W_PANIC_HOST_REGISTRY1_ADD(panic_input->invocation_result, 1u);
  W_PANIC_HOST_REGISTRY1_ADD(panic_input->provider_authority, 1u);
  W_PANIC_HOST_REGISTRY1_ADD(panic_workspace, 1u);
  W_PANIC_HOST_REGISTRY1_ADD(panic_workspace->completions,
                             panic_workspace->completion_capacity);
  W_PANIC_HOST_REGISTRY1_ADD(panic_workspace->receipt, 1u);
  W_PANIC_HOST_REGISTRY1_ADD(panic_workspace->provider_kind, 1u);
  W_PANIC_HOST_REGISTRY1_ADD(panic_output, 1u);
  W_PANIC_HOST_REGISTRY1_ADD(panic_output->source_id_bytes,
                             panic_output->source_id_capacity);
  W_PANIC_HOST_REGISTRY1_ADD(panic_output->module_id_bytes,
                             panic_output->module_id_capacity);
  W_PANIC_HOST_REGISTRY1_ADD(panic_output->message_bytes,
                             panic_output->message_capacity);
  W_PANIC_HOST_REGISTRY1_ADD(panic_output->signal, 1u);
  W_PANIC_HOST_REGISTRY1_ADD(input->lifecycle_input->panic_result, 1u);
  W_PANIC_HOST_REGISTRY1_ADD(lifecycle_output, 1u);
  W_PANIC_HOST_REGISTRY1_ADD(lifecycle_output->message_bytes,
                             lifecycle_output->message_capacity);
  W_PANIC_HOST_REGISTRY1_ADD(lifecycle_output->events,
                             lifecycle_output->event_capacity);
  W_PANIC_HOST_REGISTRY1_ADD(lifecycle_output->decision, 1u);
  W_PANIC_HOST_REGISTRY1_ADD(input->lifecycle_result, 1u);
#undef W_PANIC_HOST_REGISTRY1_ADD
  return true;
}

static bool append_barrier_ranges(
    const w_seed_parallel_panic_host_registry1_input *input,
    const w_seed_parallel_panic_host_registry1_authority *authority,
    const w_seed_parallel_panic_host_registry1_registry *registry,
    panic_host_registry1_range *ranges, size_t capacity, size_t *count) {
  if (!append_producer_ranges(input, ranges, capacity, count) ||
      !add_range(ranges, capacity, count, authority, 1u, sizeof(*authority)) ||
      !add_range(ranges, capacity, count, registry, 1u, sizeof(*registry)))
    return false;
  for (size_t left = 0u; left < *count; left += 1u)
    for (size_t right = left + 1u; right < *count; right += 1u)
      if (ranges_overlap(ranges[left], ranges[right])) return false;
  return true;
}

static bool output_capacity_valid(
    const w_seed_parallel_panic_host_registry1_output *output) {
  return output != NULL && output->record != NULL && output->events != NULL &&
         output->record_capacity == 1u &&
         output->event_capacity ==
             W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_EVENT_COUNT;
}

static bool writable_ranges_valid(
    const w_seed_parallel_panic_host_registry1_input *input,
    const w_seed_parallel_panic_host_registry1_authority *authority,
    const w_seed_parallel_panic_host_registry1_registry *registry,
    const w_seed_parallel_panic_host_registry1_output *output,
    const w_seed_parallel_panic_host_registry1_result *result) {
  if (!output_capacity_valid(output) || result == NULL) return false;
  panic_host_registry1_range writable[
      W_PANIC_HOST_REGISTRY1_WRITABLE_RANGE_CAPACITY];
  size_t writable_count = 0u;
#define W_PANIC_HOST_REGISTRY1_WRITABLE(pointer, number)                    \
  do {                                                                       \
    if (!add_range(writable, W_PANIC_HOST_REGISTRY1_WRITABLE_RANGE_CAPACITY, \
                   &writable_count, (pointer), (number), sizeof(*(pointer)))) \
      return false;                                                          \
  } while (0)
  W_PANIC_HOST_REGISTRY1_WRITABLE(output, 1u);
  W_PANIC_HOST_REGISTRY1_WRITABLE(output->record, output->record_capacity);
  W_PANIC_HOST_REGISTRY1_WRITABLE(output->events, output->event_capacity);
  W_PANIC_HOST_REGISTRY1_WRITABLE(result, 1u);
#undef W_PANIC_HOST_REGISTRY1_WRITABLE
  for (size_t left = 0u; left < writable_count; left += 1u)
    for (size_t right = left + 1u; right < writable_count; right += 1u)
      if (ranges_overlap(writable[left], writable[right])) return false;
  panic_host_registry1_range barriers[W_PANIC_HOST_REGISTRY1_RANGE_CAPACITY];
  size_t barrier_count = 0u;
  if (!append_barrier_ranges(input, authority, registry, barriers,
                             W_PANIC_HOST_REGISTRY1_RANGE_CAPACITY,
                             &barrier_count))
    return false;
  for (size_t writable_index = 0u; writable_index < writable_count;
       writable_index += 1u)
    for (size_t barrier_index = 0u; barrier_index < barrier_count;
         barrier_index += 1u)
      if (ranges_overlap(writable[writable_index],
                         barriers[barrier_index]))
        return false;
  return true;
}

static bool measure_ranges_valid(
    const w_seed_parallel_panic_host_registry1_input *input,
    const w_seed_parallel_panic_host_registry1_authority *authority,
    const w_seed_parallel_panic_host_registry1_registry *registry,
    const w_seed_parallel_panic_host_registry1_counts *counts,
    const w_seed_parallel_panic_host_registry1_result *result) {
  if (counts == NULL || result == NULL) return false;
  panic_host_registry1_range writable[2];
  if (!range_make(counts, 1u, sizeof(*counts), &writable[0]) ||
      !range_make(result, 1u, sizeof(*result), &writable[1]) ||
      ranges_overlap(writable[0], writable[1]))
    return false;
  panic_host_registry1_range barriers[W_PANIC_HOST_REGISTRY1_RANGE_CAPACITY];
  size_t barrier_count = 0u;
  if (!append_barrier_ranges(input, authority, registry, barriers,
                             W_PANIC_HOST_REGISTRY1_RANGE_CAPACITY,
                             &barrier_count))
    return false;
  for (size_t writable_index = 0u; writable_index < 2u;
       writable_index += 1u)
    for (size_t barrier_index = 0u; barrier_index < barrier_count;
         barrier_index += 1u)
      if (ranges_overlap(writable[writable_index],
                         barriers[barrier_index]))
        return false;
  return true;
}

static void sha_u32(w_seed_sha256_state *state, uint32_t value) {
  const uint8_t bytes[4] = {(uint8_t)(value >> 24u),
                            (uint8_t)(value >> 16u),
                            (uint8_t)(value >> 8u), (uint8_t)value};
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void sha_bool(w_seed_sha256_state *state, bool value) {
  const uint8_t byte = value ? 1u : 0u;
  w_seed_sha256_update(state, &byte, sizeof(byte));
}

static void sha_bytes(w_seed_sha256_state *state, const uint8_t *bytes,
                      size_t count) {
  if (count != 0u) w_seed_sha256_update(state, bytes, count);
}

static void sha_authority_receipt(
    w_seed_sha256_state *state,
    const w_seed_parallel_panic_host_registry1_authority_receipt *receipt) {
  w_seed_sha256_update(state, (const uint8_t *)receipt->schema,
                       sizeof(receipt->schema));
  sha_u32(state, receipt->target);
  sha_u32(state, receipt->generation);
  sha_u32(state, (uint32_t)receipt->resource_kind);
  sha_u32(state, (uint32_t)receipt->assurance);
  sha_bytes(state, receipt->contract_digest, sizeof(receipt->contract_digest));
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
                      const w_seed_parallel_panic_host_registry1_event *event) {
  sha_u32(state, event->sequence);
  sha_u32(state, (uint32_t)event->kind);
}

static void expected_events(
    w_seed_parallel_panic_host_registry1_event events[
        W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_EVENT_COUNT]) {
  events[0] = (w_seed_parallel_panic_host_registry1_event){
      1u, W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_EVENT_RESOURCE_REGISTERED};
  events[1] = (w_seed_parallel_panic_host_registry1_event){
      2u,
      W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_EVENT_RESOURCE_CLOSE_COMMITTED};
}

static void seal_semantic(
    const w_seed_parallel_panic_host_registry1_event events[
        W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_EVENT_COUNT], uint8_t digest[32]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(
      &state,
      (const uint8_t *)"w-seed-parallel-panic-host-registry1-semantic-1",
      sizeof("w-seed-parallel-panic-host-registry1-semantic-1") - 1u);
  sha_u32(&state, W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_RESOURCE_EVENT);
  sha_u32(&state, W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_REGISTERED);
  sha_u32(&state, W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_RELEASED);
  sha_u32(&state, 1u);
  sha_u32(&state, 1u);
  sha_u32(&state, W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_EVENT_COUNT);
  for (size_t index = 0u;
       index < W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_EVENT_COUNT; index += 1u)
    sha_event(&state, &events[index]);
  w_seed_sha256_final(&state, digest);
}

static void seal_provenance(
    const panic_host_registry1_validation *validation,
    const w_seed_parallel_panic_host_registry1_authority *authority,
    const uint8_t semantic_digest[32],
    const uint8_t lifecycle_semantic_digest[32],
    w_seed_parallel_panic_host_registry1_state final_state,
    bool close_attempted, bool close_succeeded, uint8_t digest[32]) {
  const w_seed_parallel_panic_lifecycle1_result *lifecycle_result =
      validation->lifecycle_result;
  const w_seed_parallel_panic_binding1_result *panic_result =
      validation->panic_result;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(
      &state,
      (const uint8_t *)"w-seed-parallel-panic-host-registry1-provenance-1",
      sizeof("w-seed-parallel-panic-host-registry1-provenance-1") - 1u);
  /* The semantic digest is a link to the pointer-free record, not a second
   * copy of its fields in the provenance domain. */
  sha_bytes(&state, semantic_digest, 32u);
  /* PANICLIFE1's semantic decision is upstream provenance here; retain its
   * digest as a link without importing its source/message fields. */
  sha_bytes(&state, lifecycle_semantic_digest, 32u);
  sha_u32(&state, (uint32_t)final_state);
  sha_u32(&state, validation->signal->source_task_index);
  sha_u32(&state, panic_result->panic_count);
  sha_u32(&state, validation->panic_input->generation);
  sha_u32(&state, lifecycle_result->provider_capacity);
  sha_u32(&state, (uint32_t)lifecycle_result->provider_kind);
  sha_authority_receipt(&state, &authority->receipt);
  sha_platform_receipt(&state, &lifecycle_result->platform_receipt);
  sha_bytes(&state, panic_result->semantic_digest,
            sizeof(panic_result->semantic_digest));
  sha_bytes(&state, panic_result->provenance_digest,
            sizeof(panic_result->provenance_digest));
  sha_bytes(&state, panic_result->hir_semantic_digest,
            sizeof(panic_result->hir_semantic_digest));
  sha_bytes(&state, panic_result->hir_provenance_digest,
            sizeof(panic_result->hir_provenance_digest));
  sha_bytes(&state, panic_result->selection_semantic_digest,
            sizeof(panic_result->selection_semantic_digest));
  sha_bytes(&state, panic_result->invocation_semantic_digest,
            sizeof(panic_result->invocation_semantic_digest));
  sha_bytes(&state, lifecycle_result->hir_semantic_digest,
            sizeof(lifecycle_result->hir_semantic_digest));
  sha_bytes(&state, lifecycle_result->hir_provenance_digest,
            sizeof(lifecycle_result->hir_provenance_digest));
  sha_bytes(&state, lifecycle_result->selection_semantic_digest,
            sizeof(lifecycle_result->selection_semantic_digest));
  sha_bytes(&state, lifecycle_result->invocation_semantic_digest,
            sizeof(lifecycle_result->invocation_semantic_digest));
  sha_bytes(&state, lifecycle_result->provenance_digest,
            sizeof(lifecycle_result->provenance_digest));
  sha_bool(&state, close_attempted);
  sha_bool(&state, close_succeeded);
  w_seed_sha256_final(&state, digest);
}

static bool authority_receipt_equal(
    const w_seed_parallel_panic_host_registry1_authority_receipt *left,
    const w_seed_parallel_panic_host_registry1_authority_receipt *right) {
  return left != NULL && right != NULL &&
         memcmp(left->schema, right->schema, sizeof(left->schema)) == 0 &&
         left->target == right->target && left->generation == right->generation &&
         left->resource_kind == right->resource_kind &&
         left->assurance == right->assurance &&
         memcmp(left->contract_digest, right->contract_digest,
                sizeof(left->contract_digest)) == 0;
}

static void canonical_authority_receipt(
    w_seed_parallel_panic_host_registry1_authority_receipt *receipt) {
  (void)memset(receipt, 0, sizeof(*receipt));
  (void)memcpy(receipt->schema,
               W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_SCHEMA_VERSION,
               sizeof(receipt->schema));
  receipt->target = W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_WINDOWS_AMD64;
  receipt->generation = 1u;
  receipt->resource_kind =
      W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_RESOURCE_EVENT;
  receipt->assurance =
      W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_ASSURANCE_STATIC_LOCAL_BINDING;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(
      &state,
      (const uint8_t *)W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_SCHEMA_VERSION,
      sizeof(W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_SCHEMA_VERSION) - 1u);
  sha_u32(&state, receipt->target);
  sha_u32(&state, receipt->generation);
  sha_u32(&state, (uint32_t)receipt->resource_kind);
  sha_u32(&state, (uint32_t)receipt->assurance);
  w_seed_sha256_final(&state, receipt->contract_digest);
}

static bool registry_identity_valid(
    const w_seed_parallel_panic_host_registry1_authority *authority,
    const w_seed_parallel_panic_host_registry1_registry *registry) {
  return authority != NULL && registry != NULL && registry->self == registry &&
         registry->authority == authority &&
         registry->expected_primary !=
             W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_PRIMARY_NONE &&
         registry->expected_generation != 0u;
}

static bool registry_state_valid(
    const w_seed_parallel_panic_host_registry1_registry *registry,
    w_seed_parallel_panic_host_registry1_state expected_state) {
  if (registry == NULL || registry->state != expected_state) return false;
  switch (expected_state) {
    case W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_REGISTERED:
      return registry->private_event_handle != 0u &&
             registry->close_attempt_count == 0u &&
             registry->close_success_count == 0u &&
             registry->test_fault <=
                 W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_TEST_FAULT_POST_CLOSE_RESULT_UNCERTAIN;
    case W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_RELEASED:
      return registry->private_event_handle == 0u &&
             registry->close_attempt_count == 1u &&
             registry->close_success_count == 1u &&
             registry->test_fault ==
                 W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_TEST_FAULT_NONE;
    case W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_UNCERTAIN:
      return registry->private_event_handle == 0u &&
             registry->close_attempt_count == 1u &&
             registry->close_success_count == 0u;
    case W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_DESTROYED:
      return registry->private_event_handle == 0u;
    default:
      return false;
  }
}

static w_seed_parallel_panic_host_registry1_status validate_view(
    const w_seed_parallel_panic_host_registry1_input *input,
    const w_seed_parallel_panic_host_registry1_authority *authority,
    const w_seed_parallel_panic_host_registry1_registry *registry,
    w_seed_parallel_panic_host_registry1_state expected_state,
    panic_host_registry1_validation *validation) {
  if (input == NULL || authority == NULL || registry == NULL ||
      validation == NULL)
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_INVALID;
  if (!w_seed_parallel_panic_host_registry1_authority_verify(authority))
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_AUTHORITY;
  if (!registry_identity_valid(authority, registry))
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_FORGERY;
  if (registry->state ==
          W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_UNCERTAIN &&
      expected_state !=
          W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_UNCERTAIN)
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_RELEASE_UNCERTAIN;
  if (!registry_state_valid(registry, expected_state))
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE;
  const w_seed_parallel_panic_lifecycle1_input *lifecycle_input =
      input->lifecycle_input;
  const w_seed_parallel_panic_lifecycle1_output *lifecycle_output =
      input->lifecycle_output;
  const w_seed_parallel_panic_lifecycle1_result *lifecycle_result =
      input->lifecycle_result;
  if (lifecycle_input == NULL || lifecycle_output == NULL ||
      lifecycle_result == NULL || lifecycle_input->panic_input == NULL ||
      lifecycle_input->panic_workspace == NULL ||
      lifecycle_input->panic_output == NULL ||
      lifecycle_input->panic_result == NULL)
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_UPSTREAM;

  if (!w_seed_parallel_panic_lifecycle1_verify(
          lifecycle_input, lifecycle_output, lifecycle_result)) {
    w_seed_parallel_panic_lifecycle1_counts counts;
    w_seed_parallel_panic_lifecycle1_result measured;
    if (w_seed_parallel_panic_lifecycle1_measure(
            lifecycle_input, &counts, &measured) ==
        W_SEED_PARALLEL_PANIC_LIFECYCLE1_NO_PANIC)
      return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_NO_PANIC;
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_UPSTREAM;
  }
  const w_seed_parallel_panic_binding1_input *panic_input =
      lifecycle_input->panic_input;
  const w_seed_parallel_panic_binding1_output *panic_output =
      lifecycle_input->panic_output;
  const w_seed_parallel_panic_binding1_result *panic_result =
      lifecycle_input->panic_result;
  const w_seed_parallel_panic_binding1_signal *signal = panic_output->signal;
  if (signal == NULL || panic_result->panic_count == 0u ||
      lifecycle_result->panic_count == 0u ||
      lifecycle_result->panic_count != panic_result->panic_count ||
      lifecycle_result->source_task_index != panic_result->source_task_index ||
      panic_result->source_task_index != signal->source_task_index ||
      signal->platform_receipt.panic_source_index != signal->source_task_index ||
      lifecycle_result->generation != panic_result->generation ||
      lifecycle_result->generation != panic_input->generation ||
      lifecycle_result->generation != registry->expected_generation ||
      lifecycle_result->source_task_index != registry->expected_primary ||
      lifecycle_result->panic_code != signal->panic_code)
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_FORGERY;
  *validation = (panic_host_registry1_validation){
      lifecycle_input, lifecycle_output, lifecycle_result, panic_input,
      panic_output, panic_result, signal};
  return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_OK;
}

static void result_base(
    const panic_host_registry1_validation *validation,
    const w_seed_parallel_panic_host_registry1_authority *authority,
    w_seed_parallel_panic_host_registry1_counts required,
    w_seed_parallel_panic_host_registry1_counts written,
    w_seed_parallel_panic_host_registry1_state final_state,
    bool close_attempted, bool close_succeeded,
    const uint8_t provenance_digest[32],
    w_seed_parallel_panic_host_registry1_result *result) {
  (void)memset(result, 0, sizeof(*result));
  result->status = W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_OK;
  result->required = required;
  result->written = written;
  (void)memcpy(result->schema,
               W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_SCHEMA_VERSION,
               sizeof(result->schema));
  result->resource_kind =
      W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_RESOURCE_EVENT;
  result->final_state = final_state;
  result->event_count = W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_EVENT_COUNT;
  result->primary_source_index = validation->signal->source_task_index;
  result->panic_count = validation->panic_result->panic_count;
  result->generation = validation->panic_input->generation;
  result->provider_capacity = validation->lifecycle_result->provider_capacity;
  result->provider_kind = validation->lifecycle_result->provider_kind;
  result->authority_receipt = authority->receipt;
  result->platform_receipt = validation->lifecycle_result->platform_receipt;
  (void)memcpy(result->panic_semantic_digest,
               validation->panic_result->semantic_digest,
               sizeof(result->panic_semantic_digest));
  (void)memcpy(result->panic_provenance_digest,
               validation->panic_result->provenance_digest,
               sizeof(result->panic_provenance_digest));
  (void)memcpy(result->hir_semantic_digest,
               validation->lifecycle_result->hir_semantic_digest,
               sizeof(result->hir_semantic_digest));
  (void)memcpy(result->hir_provenance_digest,
               validation->lifecycle_result->hir_provenance_digest,
               sizeof(result->hir_provenance_digest));
  (void)memcpy(result->selection_semantic_digest,
               validation->lifecycle_result->selection_semantic_digest,
               sizeof(result->selection_semantic_digest));
  (void)memcpy(result->invocation_semantic_digest,
               validation->lifecycle_result->invocation_semantic_digest,
               sizeof(result->invocation_semantic_digest));
  (void)memcpy(result->lifecycle_semantic_digest,
               validation->lifecycle_output->decision->semantic_digest,
               sizeof(result->lifecycle_semantic_digest));
  (void)memcpy(result->lifecycle_provenance_digest,
               validation->lifecycle_result->provenance_digest,
               sizeof(result->lifecycle_provenance_digest));
  result->close_attempted = close_attempted;
  result->close_succeeded = close_succeeded;
  (void)memcpy(result->provenance_digest, provenance_digest,
               sizeof(result->provenance_digest));
}

static void record_base(
    const uint8_t semantic_digest[32],
    w_seed_parallel_panic_host_registry1_record *record) {
  *record = (w_seed_parallel_panic_host_registry1_record){
      W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_RESOURCE_EVENT,
      W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_REGISTERED,
      W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_RELEASED, 1u, 1u,
      W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_EVENT_COUNT, {0u}};
  (void)memcpy(record->semantic_digest, semantic_digest,
               sizeof(record->semantic_digest));
}

static bool record_matches(
    const w_seed_parallel_panic_host_registry1_record *record,
    const w_seed_parallel_panic_host_registry1_event *events,
    const uint8_t semantic_digest[32]) {
  w_seed_parallel_panic_host_registry1_event expected[
      W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_EVENT_COUNT];
  expected_events(expected);
  return record != NULL && events != NULL &&
         record->resource_kind ==
             W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_RESOURCE_EVENT &&
         record->from_state ==
             W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_REGISTERED &&
         record->to_state ==
             W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_RELEASED &&
         record->registered_count == 1u && record->released_count == 1u &&
         record->event_count ==
             W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_EVENT_COUNT &&
         memcmp(record->semantic_digest, semantic_digest,
                sizeof(record->semantic_digest)) == 0 &&
         memcmp(events, expected, sizeof(expected)) == 0;
}

static bool result_matches(
    const w_seed_parallel_panic_host_registry1_result *result,
    const panic_host_registry1_validation *validation,
    const w_seed_parallel_panic_host_registry1_authority *authority,
    const uint8_t provenance_digest[32]) {
  const w_seed_parallel_panic_lifecycle1_result *lifecycle_result =
      validation->lifecycle_result;
  const w_seed_parallel_panic_binding1_result *panic_result =
      validation->panic_result;
  const w_seed_parallel_panic_host_registry1_counts required = {1u, 2u};
  return result != NULL &&
         result->status == W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_OK &&
         memcmp(result->schema,
                W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_SCHEMA_VERSION,
                sizeof(result->schema)) == 0 &&
         memcmp(&result->required, &required, sizeof(required)) == 0 &&
         memcmp(&result->written, &required, sizeof(required)) == 0 &&
         result->resource_kind ==
             W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_RESOURCE_EVENT &&
         result->final_state ==
             W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_RELEASED &&
         result->event_count ==
             W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_EVENT_COUNT &&
         result->primary_source_index == validation->signal->source_task_index &&
         result->panic_count == panic_result->panic_count &&
         result->generation == validation->panic_input->generation &&
         result->provider_capacity == lifecycle_result->provider_capacity &&
         result->provider_kind == lifecycle_result->provider_kind &&
         authority_receipt_equal(&result->authority_receipt,
                                 &authority->receipt) &&
         w_seed_parallel_platform1_receipt_equal(&result->platform_receipt,
                                                 &lifecycle_result->platform_receipt) &&
         memcmp(result->panic_semantic_digest, panic_result->semantic_digest,
                sizeof(result->panic_semantic_digest)) == 0 &&
         memcmp(result->panic_provenance_digest,
                panic_result->provenance_digest,
                sizeof(result->panic_provenance_digest)) == 0 &&
         memcmp(result->hir_semantic_digest,
                lifecycle_result->hir_semantic_digest,
                sizeof(result->hir_semantic_digest)) == 0 &&
         memcmp(result->hir_provenance_digest,
                lifecycle_result->hir_provenance_digest,
                sizeof(result->hir_provenance_digest)) == 0 &&
         memcmp(result->selection_semantic_digest,
                lifecycle_result->selection_semantic_digest,
                sizeof(result->selection_semantic_digest)) == 0 &&
         memcmp(result->invocation_semantic_digest,
                lifecycle_result->invocation_semantic_digest,
                sizeof(result->invocation_semantic_digest)) == 0 &&
         memcmp(result->lifecycle_semantic_digest,
                validation->lifecycle_output->decision->semantic_digest,
                sizeof(result->lifecycle_semantic_digest)) == 0 &&
         memcmp(result->lifecycle_provenance_digest,
                lifecycle_result->provenance_digest,
                sizeof(result->lifecycle_provenance_digest)) == 0 &&
         result->close_attempted && result->close_succeeded &&
         memcmp(result->provenance_digest, provenance_digest,
                sizeof(result->provenance_digest)) == 0;
}

bool w_seed_parallel_panic_host_registry1_open(
    uint32_t target, w_seed_parallel_panic_host_registry1_authority *authority) {
  if (authority == NULL ||
      target != W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_WINDOWS_AMD64)
    return false;
  w_seed_parallel_panic_host_registry1_authority candidate;
  (void)memset(&candidate, 0, sizeof(candidate));
  candidate.private_seal = (uintptr_t)&panic_host_registry1_seal;
  candidate.self = authority;
  canonical_authority_receipt(&candidate.receipt);
  *authority = candidate;
  return true;
}

bool w_seed_parallel_panic_host_registry1_authority_verify(
    const w_seed_parallel_panic_host_registry1_authority *authority) {
  if (authority == NULL || authority->private_seal !=
                              (uintptr_t)&panic_host_registry1_seal ||
      authority->self != authority)
    return false;
  w_seed_parallel_panic_host_registry1_authority_receipt expected;
  canonical_authority_receipt(&expected);
  return authority_receipt_equal(&authority->receipt, &expected);
}

bool w_seed_parallel_panic_host_registry1_authority_receipt_equal(
    const w_seed_parallel_panic_host_registry1_authority_receipt *left,
    const w_seed_parallel_panic_host_registry1_authority_receipt *right) {
  return authority_receipt_equal(left, right);
}

w_seed_parallel_panic_host_registry1_status
w_seed_parallel_panic_host_registry1_open_event(
    const w_seed_parallel_panic_host_registry1_authority *authority,
    w_seed_parallel_panic_host_registry1_registry *registry,
    uint32_t expected_primary, uint32_t expected_generation) {
  if (authority == NULL || registry == NULL)
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_INVALID;
  if (!w_seed_parallel_panic_host_registry1_authority_verify(authority))
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_AUTHORITY;
  if (expected_primary ==
          W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_PRIMARY_NONE ||
      expected_generation == 0u)
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_FORGERY;
  if (registry->state != W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_EMPTY &&
      registry->state !=
          W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_DESTROYED)
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE;
  if (registry->self != NULL && registry->self != registry)
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_FORGERY;
  if (registry->private_event_handle != 0u)
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_FORGERY;
  HANDLE event_handle = CreateEventW(NULL, TRUE, FALSE, NULL);
  if (event_handle == NULL)
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_CREATE_FAILED;
  w_seed_parallel_panic_host_registry1_registry candidate = {
      registry, authority, expected_primary, expected_generation,
      W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_REGISTERED,
      W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_TEST_FAULT_NONE, 0u, 0u,
      (uintptr_t)event_handle};
  *registry = candidate;
  return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_OK;
}

w_seed_parallel_panic_host_registry1_status
w_seed_parallel_panic_host_registry1_measure(
    const w_seed_parallel_panic_host_registry1_input *input,
    const w_seed_parallel_panic_host_registry1_authority *authority,
    const w_seed_parallel_panic_host_registry1_registry *registry,
    w_seed_parallel_panic_host_registry1_counts *counts,
    w_seed_parallel_panic_host_registry1_result *result) {
  if (input == NULL || authority == NULL || registry == NULL ||
      counts == NULL || result == NULL)
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_INVALID;
  panic_host_registry1_validation validation;
  const w_seed_parallel_panic_host_registry1_status validation_status =
      validate_view(input, authority, registry,
                    W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_REGISTERED,
                    &validation);
  if (validation_status != W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_OK)
    return validation_status;
  if (!measure_ranges_valid(input, authority, registry, counts, result))
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_ALIAS;
  const w_seed_parallel_panic_host_registry1_counts required = {1u, 2u};
  w_seed_parallel_panic_host_registry1_event events[
      W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_EVENT_COUNT];
  expected_events(events);
  uint8_t semantic_digest[32];
  uint8_t provenance_digest[32];
  seal_semantic(events, semantic_digest);
  seal_provenance(&validation, authority, semantic_digest,
                  validation.lifecycle_output->decision->semantic_digest,
                  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_REGISTERED, false,
                  false, provenance_digest);
  w_seed_parallel_panic_host_registry1_result candidate;
  result_base(&validation, authority, required, (w_seed_parallel_panic_host_registry1_counts){0u, 0u},
              W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_REGISTERED, false,
              false, provenance_digest, &candidate);
  *counts = required;
  *result = candidate;
  return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_OK;
}

w_seed_parallel_panic_host_registry1_status
w_seed_parallel_panic_host_registry1_release(
    const w_seed_parallel_panic_host_registry1_input *input,
    const w_seed_parallel_panic_host_registry1_authority *authority,
    w_seed_parallel_panic_host_registry1_registry *registry,
    const w_seed_parallel_panic_host_registry1_output *output,
    w_seed_parallel_panic_host_registry1_result *result) {
  if (input == NULL || authority == NULL || registry == NULL ||
      output == NULL || result == NULL)
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_INVALID;
  panic_host_registry1_validation validation;
  const w_seed_parallel_panic_host_registry1_status validation_status =
      validate_view(input, authority, registry,
                    W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_REGISTERED,
                    &validation);
  if (validation_status != W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_OK)
    return validation_status;
  if (!output_capacity_valid(output))
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_CAPACITY;
  if (!writable_ranges_valid(input, authority, registry, output, result))
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_ALIAS;
  if (registry->test_fault ==
      W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_TEST_FAULT_PRE_CLOSE_FAILURE)
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_RELEASE_BLOCKED;

  const w_seed_parallel_panic_host_registry1_counts required = {1u, 2u};
  w_seed_parallel_panic_host_registry1_event candidate_events[
      W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_EVENT_COUNT];
  expected_events(candidate_events);
  uint8_t semantic_digest[32];
  seal_semantic(candidate_events, semantic_digest);
  w_seed_parallel_panic_host_registry1_record candidate_record;
  record_base(semantic_digest, &candidate_record);

  /* All validation, range checks, and digest construction finish before this
   * sole physical effect. CloseHandle success is the only physical close fact
   * claimed; a false return becomes UNCERTAIN and is never retried. */
  HANDLE event_handle = (HANDLE)(uintptr_t)registry->private_event_handle;
  const BOOL close_succeeded = CloseHandle(event_handle);
  registry->close_attempt_count = 1u;
  registry->private_event_handle = 0u;
  if (close_succeeded == FALSE ||
      registry->test_fault ==
          W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_TEST_FAULT_POST_CLOSE_RESULT_UNCERTAIN) {
    registry->state = W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_UNCERTAIN;
    registry->close_success_count = 0u;
    registry->test_fault =
        W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_TEST_FAULT_NONE;
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_RELEASE_UNCERTAIN;
  }
  registry->state = W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_RELEASED;
  registry->close_success_count = 1u;
  registry->test_fault = W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_TEST_FAULT_NONE;
  uint8_t provenance_digest[32];
  seal_provenance(&validation, authority, semantic_digest,
                  validation.lifecycle_output->decision->semantic_digest,
                  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_RELEASED, true,
                  true, provenance_digest);
  w_seed_parallel_panic_host_registry1_result candidate_result;
  result_base(&validation, authority, required, required,
              W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_RELEASED, true, true,
              provenance_digest, &candidate_result);

  /* No operation below can fail. The result and both fixed output arrays are
   * caller-owned, fully range-checked storage. */
  *output->record = candidate_record;
  (void)memcpy(output->events, candidate_events, sizeof(candidate_events));
  *result = candidate_result;
  return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_OK;
}

w_seed_parallel_panic_host_registry1_status
w_seed_parallel_panic_host_registry1_run(
    const w_seed_parallel_panic_host_registry1_input *input,
    const w_seed_parallel_panic_host_registry1_authority *authority,
    w_seed_parallel_panic_host_registry1_registry *registry,
    const w_seed_parallel_panic_host_registry1_output *output,
    w_seed_parallel_panic_host_registry1_result *result) {
  return w_seed_parallel_panic_host_registry1_release(input, authority,
                                                        registry, output,
                                                        result);
}

bool w_seed_parallel_panic_host_registry1_verify(
    const w_seed_parallel_panic_host_registry1_input *input,
    const w_seed_parallel_panic_host_registry1_authority *authority,
    const w_seed_parallel_panic_host_registry1_registry *registry,
    const w_seed_parallel_panic_host_registry1_output *output,
    const w_seed_parallel_panic_host_registry1_result *result) {
  if (input == NULL || authority == NULL || registry == NULL ||
      output == NULL || result == NULL ||
      !registry_identity_valid(authority, registry) ||
      registry->state != W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_RELEASED ||
      registry->private_event_handle != 0u || registry->close_attempt_count != 1u ||
      registry->close_success_count != 1u ||
      registry->test_fault !=
          W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_TEST_FAULT_NONE ||
      !w_seed_parallel_panic_host_registry1_authority_verify(authority))
    return false;
  panic_host_registry1_validation validation;
  if (validate_view(input, authority, registry,
                    W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_RELEASED,
                    &validation) != W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_OK)
    return false;
  if (!output_capacity_valid(output) ||
      !writable_ranges_valid(input, authority, registry, output, result))
    return false;
  w_seed_parallel_panic_host_registry1_event expected[
      W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_EVENT_COUNT];
  expected_events(expected);
  uint8_t semantic_digest[32];
  uint8_t provenance_digest[32];
  seal_semantic(expected, semantic_digest);
  seal_provenance(&validation, authority, semantic_digest,
                  validation.lifecycle_output->decision->semantic_digest,
                  W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_RELEASED, true,
                  true, provenance_digest);
  return record_matches(output->record, output->events, semantic_digest) &&
         result_matches(result, &validation, authority, provenance_digest);
}

w_seed_parallel_panic_host_registry1_status
w_seed_parallel_panic_host_registry1_destroy(
    const w_seed_parallel_panic_host_registry1_authority *authority,
    w_seed_parallel_panic_host_registry1_registry *registry) {
  if (authority == NULL || registry == NULL)
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_INVALID;
  if (!w_seed_parallel_panic_host_registry1_authority_verify(authority))
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_AUTHORITY;
  if (!registry_identity_valid(authority, registry))
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_FORGERY;
  if (registry->state == W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_DESTROYED)
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_OK;
  if (registry->state == W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_UNCERTAIN)
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_RELEASE_UNCERTAIN;
  if (registry->state == W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_RELEASED) {
    registry->private_event_handle = 0u;
    registry->test_fault =
        W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_TEST_FAULT_NONE;
    registry->state = W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_DESTROYED;
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_OK;
  }
  if (registry->state !=
          W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_REGISTERED ||
      registry->private_event_handle == 0u || registry->close_attempt_count != 0u)
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE;
  HANDLE event_handle = (HANDLE)(uintptr_t)registry->private_event_handle;
  const BOOL close_succeeded = CloseHandle(event_handle);
  registry->close_attempt_count = 1u;
  registry->private_event_handle = 0u;
  registry->test_fault =
      W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_TEST_FAULT_NONE;
  if (close_succeeded == FALSE) {
    registry->state = W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_UNCERTAIN;
    registry->close_success_count = 0u;
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_RELEASE_UNCERTAIN;
  }
  registry->state = W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_DESTROYED;
  registry->close_success_count = 1u;
  return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_OK;
}

w_seed_parallel_panic_host_registry1_status
w_seed_parallel_panic_host_registry1_test_set_fault(
    w_seed_parallel_panic_host_registry1_registry *registry,
    w_seed_parallel_panic_host_registry1_test_fault fault) {
  if (registry == NULL) return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_INVALID;
  if (registry->self != registry ||
      registry->state !=
          W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_STATE_REGISTERED ||
      fault >
          W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_TEST_FAULT_POST_CLOSE_RESULT_UNCERTAIN)
    return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_FORGERY;
  registry->test_fault = fault;
  return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_OK;
}

#else

bool w_seed_parallel_panic_host_registry1_open(
    uint32_t target, w_seed_parallel_panic_host_registry1_authority *authority) {
  (void)target;
  (void)authority;
  return false;
}

bool w_seed_parallel_panic_host_registry1_authority_verify(
    const w_seed_parallel_panic_host_registry1_authority *authority) {
  (void)authority;
  return false;
}

bool w_seed_parallel_panic_host_registry1_authority_receipt_equal(
    const w_seed_parallel_panic_host_registry1_authority_receipt *left,
    const w_seed_parallel_panic_host_registry1_authority_receipt *right) {
  (void)left;
  (void)right;
  return false;
}

w_seed_parallel_panic_host_registry1_status
w_seed_parallel_panic_host_registry1_open_event(
    const w_seed_parallel_panic_host_registry1_authority *authority,
    w_seed_parallel_panic_host_registry1_registry *registry,
    uint32_t expected_primary, uint32_t expected_generation) {
  (void)authority;
  (void)registry;
  (void)expected_primary;
  (void)expected_generation;
  return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_UNSUPPORTED;
}

w_seed_parallel_panic_host_registry1_status
w_seed_parallel_panic_host_registry1_measure(
    const w_seed_parallel_panic_host_registry1_input *input,
    const w_seed_parallel_panic_host_registry1_authority *authority,
    const w_seed_parallel_panic_host_registry1_registry *registry,
    w_seed_parallel_panic_host_registry1_counts *counts,
    w_seed_parallel_panic_host_registry1_result *result) {
  (void)input;
  (void)authority;
  (void)registry;
  (void)counts;
  (void)result;
  return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_UNSUPPORTED;
}

w_seed_parallel_panic_host_registry1_status
w_seed_parallel_panic_host_registry1_release(
    const w_seed_parallel_panic_host_registry1_input *input,
    const w_seed_parallel_panic_host_registry1_authority *authority,
    w_seed_parallel_panic_host_registry1_registry *registry,
    const w_seed_parallel_panic_host_registry1_output *output,
    w_seed_parallel_panic_host_registry1_result *result) {
  (void)input;
  (void)authority;
  (void)registry;
  (void)output;
  (void)result;
  return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_UNSUPPORTED;
}

w_seed_parallel_panic_host_registry1_status
w_seed_parallel_panic_host_registry1_run(
    const w_seed_parallel_panic_host_registry1_input *input,
    const w_seed_parallel_panic_host_registry1_authority *authority,
    w_seed_parallel_panic_host_registry1_registry *registry,
    const w_seed_parallel_panic_host_registry1_output *output,
    w_seed_parallel_panic_host_registry1_result *result) {
  (void)input;
  (void)authority;
  (void)registry;
  (void)output;
  (void)result;
  return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_UNSUPPORTED;
}

bool w_seed_parallel_panic_host_registry1_verify(
    const w_seed_parallel_panic_host_registry1_input *input,
    const w_seed_parallel_panic_host_registry1_authority *authority,
    const w_seed_parallel_panic_host_registry1_registry *registry,
    const w_seed_parallel_panic_host_registry1_output *output,
    const w_seed_parallel_panic_host_registry1_result *result) {
  (void)input;
  (void)authority;
  (void)registry;
  (void)output;
  (void)result;
  return false;
}

w_seed_parallel_panic_host_registry1_status
w_seed_parallel_panic_host_registry1_destroy(
    const w_seed_parallel_panic_host_registry1_authority *authority,
    w_seed_parallel_panic_host_registry1_registry *registry) {
  (void)authority;
  (void)registry;
  return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_UNSUPPORTED;
}

w_seed_parallel_panic_host_registry1_status
w_seed_parallel_panic_host_registry1_test_set_fault(
    w_seed_parallel_panic_host_registry1_registry *registry,
    w_seed_parallel_panic_host_registry1_test_fault fault) {
  (void)registry;
  (void)fault;
  return W_SEED_PARALLEL_PANIC_HOST_REGISTRY1_UNSUPPORTED;
}

#endif
