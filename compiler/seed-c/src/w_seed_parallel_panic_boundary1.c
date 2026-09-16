#include "w_seed_parallel_panic_boundary1.h"

#include "w_seed_sha256.h"

#include <limits.h>
#include <string.h>

_Static_assert(sizeof(W_SEED_PARALLEL_PANIC_BOUNDARY1_SCHEMA_VERSION) <=
                   W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_SCHEMA_BYTES,
               "panic boundary schema must fit its wire slot");
_Static_assert(W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PROVENANCE_DIGEST_OFFSET +
                   32u == W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES,
               "panic boundary wire layout must be exact");
_Static_assert(W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_HEADER_BYTES == 32u,
               "panic boundary header must be explicit and fixed");

#if defined(_WIN32) && defined(_WIN64)

static bool panic_code_valid(uint32_t code) {
  return code >= (uint32_t)W_SEED_PARALLEL_PLATFORM1_PANIC_EXPLICIT &&
         code <=
             (uint32_t)W_SEED_PARALLEL_PLATFORM1_PANIC_INTERNAL_CONTRACT;
}

static bool fault_valid(w_seed_parallel_panic_boundary1_fault fault) {
  return fault >= W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_NONE &&
         fault <= W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_STALE_GENERATION;
}

static bool bounded_c_string_length(const char *text, size_t limit,
                                    size_t *length) {
  if (text == NULL || length == NULL) return false;
  for (size_t index = 0u; index < limit; index += 1u) {
    if (text[index] == '\0') {
      *length = index;
      return true;
    }
  }
  return false;
}

static void wire_put_u32(
    uint8_t wire[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES], size_t offset,
    uint32_t value) {
  wire[offset] = (uint8_t)(value >> 24u);
  wire[offset + 1u] = (uint8_t)(value >> 16u);
  wire[offset + 2u] = (uint8_t)(value >> 8u);
  wire[offset + 3u] = (uint8_t)value;
}

static uint32_t wire_get_u32(
    const uint8_t wire[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES],
    size_t offset) {
  return ((uint32_t)wire[offset] << 24u) |
         ((uint32_t)wire[offset + 1u] << 16u) |
         ((uint32_t)wire[offset + 2u] << 8u) |
         (uint32_t)wire[offset + 3u];
}

static void wire_put_u64(
    uint8_t wire[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES], size_t offset,
    uint64_t value) {
  wire_put_u32(wire, offset, (uint32_t)(value >> 32u));
  wire_put_u32(wire, offset + 4u, (uint32_t)value);
}

static uint64_t wire_get_u64(
    const uint8_t wire[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES],
    size_t offset) {
  return ((uint64_t)wire_get_u32(wire, offset) << 32u) |
         (uint64_t)wire_get_u32(wire, offset + 4u);
}

static bool wire_bool(
    const uint8_t wire[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES],
    size_t offset, bool *value) {
  if (value == NULL) return false;
  const uint32_t raw = wire_get_u32(wire, offset);
  if (raw > 1u) return false;
  *value = raw != 0u;
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

static void sha_bool(w_seed_sha256_state *state, bool value) {
  const uint8_t byte = value ? 1u : 0u;
  w_seed_sha256_update(state, &byte, sizeof(byte));
}

static void wire_seal_provenance(
    uint8_t wire[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(
      &state,
      (const uint8_t *)"w-seed-parallel-panic-boundary1-wire-provenance-1",
      sizeof("w-seed-parallel-panic-boundary1-wire-provenance-1") - 1u);
  w_seed_sha256_update(&state, wire,
                       W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PROVENANCE_DIGEST_OFFSET);
  w_seed_sha256_final(
      &state,
      wire + W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PROVENANCE_DIGEST_OFFSET);
}

static void seal_receipt_provenance(
    const w_seed_parallel_panic_boundary1_receipt *receipt,
    uint8_t digest[32]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(
      &state,
      (const uint8_t *)"w-seed-parallel-panic-boundary1-receipt-provenance-1",
      sizeof("w-seed-parallel-panic-boundary1-receipt-provenance-1") - 1u);
  w_seed_sha256_update(&state, (const uint8_t *)receipt->schema,
                       sizeof(receipt->schema));
  sha_u32(&state, receipt->protocol_version);
  sha_u32(&state, receipt->frame_type);
  sha_u32(&state, receipt->frame_length);
  sha_u64(&state, receipt->invocation_nonce);
  sha_u32(&state, receipt->sequence);
  sha_u32(&state, (uint32_t)receipt->wire_status);
  sha_u32(&state, receipt->target);
  sha_u32(&state, receipt->provider_capacity);
  sha_u32(&state, receipt->provider_kind);
  sha_u32(&state, receipt->generation);
  sha_u32(&state, receipt->task_count);
  sha_u32(&state, receipt->panic_source_index);
  sha_u32(&state, receipt->panic_source_lexical_index);
  sha_u32(&state, receipt->panic_source_call_index);
  sha_u32(&state, (uint32_t)receipt->panic_code);
  sha_bool(&state, receipt->semantic_result_published);
  sha_u32(&state, receipt->child_pid);
  sha_u32(&state, receipt->started_count);
  sha_u32(&state, receipt->settled_count);
  sha_u32(&state, receipt->canceled_before_start_count);
  sha_u32(&state, receipt->maximum_active);
  sha_u32(&state, receipt->cancellation_source_index);
  sha_bool(&state, receipt->cancellation_requested);
  sha_u32(&state, receipt->panic_receipt_source_index);
  sha_bool(&state, receipt->panic_requested);
  w_seed_sha256_update(&state, receipt->hir_semantic_digest,
                       sizeof(receipt->hir_semantic_digest));
  w_seed_sha256_update(&state, receipt->error_identity_digest,
                       sizeof(receipt->error_identity_digest));
  w_seed_sha256_update(&state, receipt->authority_digest,
                       sizeof(receipt->authority_digest));
  w_seed_sha256_update(&state, receipt->wire_provenance_digest,
                       sizeof(receipt->wire_provenance_digest));
  sha_u32(&state, receipt->child_exit_code);
  sha_bool(&state, receipt->child_was_live_at_teardown);
  sha_bool(&state, receipt->child_terminated);
  sha_bool(&state, receipt->child_joined);
  sha_bool(&state, receipt->handles_closed);
  w_seed_sha256_final(&state, digest);
}

typedef struct {
  uintptr_t start;
  uintptr_t end;
  bool active;
} panic_boundary1_range;

enum { W_PANIC_BOUNDARY1_INPUT_RANGE_CAPACITY = 64u };

static bool panic_boundary1_range_make(const void *pointer, size_t count,
                                       size_t element_size,
                                       panic_boundary1_range *range) {
  if (range == NULL) return false;
  *range = (panic_boundary1_range){0u, 0u, false};
  if (count == 0u) return true;
  if (pointer == NULL || element_size == 0u ||
      count > SIZE_MAX / element_size)
    return false;
  const size_t bytes = count * element_size;
  if (bytes > (size_t)UINTPTR_MAX) return false;
  const uintptr_t start = (uintptr_t)pointer;
  if (start > UINTPTR_MAX - (uintptr_t)bytes) return false;
  *range = (panic_boundary1_range){start, start + (uintptr_t)bytes, true};
  return true;
}

static bool panic_boundary1_ranges_overlap(panic_boundary1_range left,
                                           panic_boundary1_range right) {
  return left.active && right.active && left.start < right.end &&
         right.start < left.end;
}

static bool panic_boundary1_add_range(panic_boundary1_range *ranges,
                                      size_t capacity, size_t *count,
                                      const void *pointer, size_t elements,
                                      size_t element_size) {
  if (ranges == NULL || count == NULL || *count >= capacity ||
      !panic_boundary1_range_make(pointer, elements, element_size,
                                  &ranges[*count]))
    return false;
  *count += 1u;
  return true;
}

static bool panic_boundary1_append_hir_ranges(
    const w_seed_hir0_program *program, panic_boundary1_range *ranges,
    size_t capacity, size_t *count) {
  if (program == NULL || ranges == NULL || count == NULL) return false;
#define W_PANIC_BOUNDARY1_ADD(pointer, number)                                \
  do {                                                                        \
    if (!panic_boundary1_add_range(ranges, capacity, count, (pointer),        \
                                   (number), sizeof(*(pointer))))             \
      return false;                                                           \
  } while (0)
  W_PANIC_BOUNDARY1_ADD(program, 1u);
  W_PANIC_BOUNDARY1_ADD(program->modules, program->module_capacity);
  W_PANIC_BOUNDARY1_ADD(program->identities, program->identity_capacity);
  W_PANIC_BOUNDARY1_ADD(program->types, program->type_capacity);
  W_PANIC_BOUNDARY1_ADD(program->enums, program->enum_capacity);
  W_PANIC_BOUNDARY1_ADD(program->enum_cases, program->enum_case_capacity);
  W_PANIC_BOUNDARY1_ADD(program->enum_case_parameters,
                        program->enum_case_parameter_capacity);
  W_PANIC_BOUNDARY1_ADD(program->enum_subset_members,
                        program->enum_subset_member_capacity);
  W_PANIC_BOUNDARY1_ADD(program->functions, program->function_capacity);
  W_PANIC_BOUNDARY1_ADD(program->parameters, program->parameter_capacity);
  W_PANIC_BOUNDARY1_ADD(program->blocks, program->block_capacity);
  W_PANIC_BOUNDARY1_ADD(program->block_arguments,
                        program->block_argument_capacity);
  W_PANIC_BOUNDARY1_ADD(program->edge_arguments,
                        program->edge_argument_capacity);
  W_PANIC_BOUNDARY1_ADD(program->switch_edges, program->switch_edge_capacity);
  W_PANIC_BOUNDARY1_ADD(program->switch_captures,
                        program->switch_capture_capacity);
  W_PANIC_BOUNDARY1_ADD(program->instructions, program->instruction_capacity);
  W_PANIC_BOUNDARY1_ADD(program->bindings, program->binding_capacity);
  W_PANIC_BOUNDARY1_ADD(program->calls, program->call_capacity);
  W_PANIC_BOUNDARY1_ADD(program->host_parameters,
                        program->host_parameter_capacity);
  W_PANIC_BOUNDARY1_ADD(program->arguments, program->argument_capacity);
  W_PANIC_BOUNDARY1_ADD(program->enum_payloads,
                        program->enum_payload_capacity);
  W_PANIC_BOUNDARY1_ADD(program->requirements, program->requirement_capacity);
  W_PANIC_BOUNDARY1_ADD(program->values, program->value_capacity);
  W_PANIC_BOUNDARY1_ADD(program->interpolation_segments,
                        program->interpolation_segment_capacity);
  W_PANIC_BOUNDARY1_ADD(program->terminators, program->terminator_capacity);
  W_PANIC_BOUNDARY1_ADD(program->entries, program->entry_capacity);
  W_PANIC_BOUNDARY1_ADD(program->text_bytes, program->text_byte_capacity);
  W_PANIC_BOUNDARY1_ADD(program->value_bytes, program->value_byte_capacity);
  W_PANIC_BOUNDARY1_ADD(program->receipt, program->receipt_capacity);
  W_PANIC_BOUNDARY1_ADD(program->external_modules,
                        program->external_module_capacity);
  W_PANIC_BOUNDARY1_ADD(program->external_symbols,
                        program->external_symbol_capacity);
  W_PANIC_BOUNDARY1_ADD(program->cleanups, program->cleanup_capacity);
#undef W_PANIC_BOUNDARY1_ADD
  return true;
}

static bool panic_boundary1_append_input_ranges(
    const w_seed_parallel_panic_boundary1_input *input,
    panic_boundary1_range *ranges, size_t capacity, size_t *count) {
  if (input == NULL || ranges == NULL || count == NULL ||
      input->typed_input == NULL ||
      !panic_boundary1_add_range(ranges, capacity, count, input, 1u,
                                 sizeof(*input)) ||
      !panic_boundary1_add_range(ranges, capacity, count, input->typed_input,
                                 1u, sizeof(*input->typed_input)) ||
      !panic_boundary1_add_range(ranges, capacity, count,
                                 input->typed_input->hir_result, 1u,
                                 sizeof(*input->typed_input->hir_result)) ||
      !panic_boundary1_append_hir_ranges(input->typed_input->hir_program,
                                         ranges, capacity, count) ||
      !panic_boundary1_add_range(
          ranges, capacity, count, input->typed_input->tasks,
          input->typed_input->task_capacity,
          sizeof(*input->typed_input->tasks)) ||
      !panic_boundary1_add_range(
          ranges, capacity, count, input->typed_input->provider_authority, 1u,
          sizeof(*input->typed_input->provider_authority)) ||
      !panic_boundary1_add_range(ranges, capacity, count,
                                 &input->typed_input->provider_job, 1u,
                                 sizeof(input->typed_input->provider_job)))
    return false;
  const w_seed_parallel_platform1_job *job =
      &input->typed_input->provider_job;
  if ((job->context == NULL && job->context_bytes != 0u) ||
      (job->context != NULL && job->context_bytes == 0u) ||
      (job->context != NULL &&
       !panic_boundary1_add_range(ranges, capacity, count, job->context,
                                  job->context_bytes, 1u)))
    return false;
  size_t helper_length = 0u;
  if (!bounded_c_string_length(input->helper_path, 4096u, &helper_length) ||
      helper_length == SIZE_MAX ||
      !panic_boundary1_add_range(ranges, capacity, count, input->helper_path,
                                 helper_length + 1u, 1u))
    return false;
  return true;
}

static bool output_aliases_input(
    const w_seed_parallel_panic_boundary1_input *input,
    const w_seed_parallel_panic_boundary1_output *output) {
  if (input == NULL || output == NULL || output->receipt == NULL) return true;
  panic_boundary1_range receipt_range;
  panic_boundary1_range output_range;
  if (!panic_boundary1_range_make(output->receipt, 1u,
                                  sizeof(*output->receipt), &receipt_range) ||
      !panic_boundary1_range_make(output, 1u, sizeof(*output), &output_range) ||
      panic_boundary1_ranges_overlap(receipt_range, output_range))
    return true;
  panic_boundary1_range inputs[W_PANIC_BOUNDARY1_INPUT_RANGE_CAPACITY];
  size_t input_count = 0u;
  if (!panic_boundary1_append_input_ranges(
          input, inputs, W_PANIC_BOUNDARY1_INPUT_RANGE_CAPACITY,
          &input_count))
    return true;
  for (size_t index = 0u; index < input_count; index += 1u)
    if (panic_boundary1_ranges_overlap(receipt_range, inputs[index]))
      return true;
  return false;
}

static bool typed_input_is_boundary_witness(
    const w_seed_parallel_typed_binding1_input *typed_input,
    w_seed_parallel_typed_binding1_result *measured) {
  if (typed_input == NULL || measured == NULL || typed_input->hir_program == NULL ||
      typed_input->hir_result == NULL || typed_input->provider_authority == NULL ||
      typed_input->provider.target !=
          W_SEED_PARALLEL_TYPED_BINDING1_TARGET_WINDOWS_AMD64 ||
      typed_input->provider_capacity != 2u ||
      typed_input->task_count !=
          W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT ||
      typed_input->task_capacity < typed_input->task_count ||
      typed_input->tasks == NULL || typed_input->generation == 0u ||
      typed_input->tasks[0].lexical_index != 0u ||
      typed_input->tasks[1].lexical_index != 1u ||
      typed_input->provider_job.invoke == NULL)
    return false;
  if (!w_seed_parallel_local_provider1_verify(typed_input->provider_authority) ||
      !w_seed_hir0_verify(typed_input->hir_program, typed_input->hir_result))
    return false;
  w_seed_parallel_typed_binding1_counts counts;
  if (w_seed_parallel_typed_binding1_measure(typed_input, &counts, measured) !=
          W_SEED_PARALLEL_TYPED_BINDING1_OK ||
      counts.completions != W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT ||
      counts.records != W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT ||
      measured->written.completions != 0u || measured->written.records != 0u)
    return false;
  return true;
}

static bool binding_identity_equal(
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

static bool measurement_matches(
    const w_seed_parallel_typed_binding1_input *typed_input,
    const w_seed_parallel_typed_binding1_result *measured,
    const w_seed_parallel_typed_binding1_result *canonical) {
  return typed_input != NULL && measured != NULL && canonical != NULL &&
         measured->status == W_SEED_PARALLEL_TYPED_BINDING1_OK &&
         measured->required.completions ==
             W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT &&
         measured->required.records ==
             W_SEED_PARALLEL_TYPED_BINDING1_WITNESS_TASK_COUNT &&
         measured->written.completions == 0u && measured->written.records == 0u &&
         measured->task_count == canonical->task_count &&
         measured->invoke_terminator == canonical->invoke_terminator &&
         measured->generation == typed_input->generation &&
         memcmp(measured->schema, canonical->schema,
                sizeof(measured->schema)) == 0 &&
         memcmp(measured->hir_semantic_digest, canonical->hir_semantic_digest,
                sizeof(measured->hir_semantic_digest)) == 0 &&
         binding_identity_equal(&measured->error_identity,
                                &canonical->error_identity);
}

static w_seed_parallel_panic_boundary1_status input_validate(
    const w_seed_parallel_panic_boundary1_input *input,
    w_seed_parallel_typed_binding1_result *measured, uint32_t *timeout_ms) {
  if (input == NULL || measured == NULL || timeout_ms == NULL ||
      input->typed_input == NULL || input->helper_path == NULL ||
      !panic_code_valid(input->expected_panic_code) ||
      !fault_valid(input->helper_fault) || input->invocation_nonce == 0u)
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_INVALID;
  size_t helper_path_length = 0u;
  if (!bounded_c_string_length(input->helper_path, 4096u, &helper_path_length) ||
      helper_path_length == 0u)
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_INVALID;
  (void)helper_path_length;
  if (input->timeout_ms == 0u)
    *timeout_ms = W_SEED_PARALLEL_PANIC_BOUNDARY1_DEFAULT_TIMEOUT_MS;
  else if (input->timeout_ms <= W_SEED_PARALLEL_PANIC_BOUNDARY1_MAX_TIMEOUT_MS)
    *timeout_ms = input->timeout_ms;
  else
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_INVALID;
  const w_seed_parallel_typed_binding1_input *typed = input->typed_input;
  if (typed->provider.target !=
      W_SEED_PARALLEL_TYPED_BINDING1_TARGET_WINDOWS_AMD64)
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_UNSUPPORTED;
  if (typed->provider_authority == NULL ||
      !w_seed_parallel_local_provider1_verify(typed->provider_authority))
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_AUTHORITY;
  if (!w_seed_hir0_verify(typed->hir_program, typed->hir_result))
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_HIR;
  if (!typed_input_is_boundary_witness(typed, measured))
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_PARBIND;
  return W_SEED_PARALLEL_PANIC_BOUNDARY1_OK;
}

bool w_seed_parallel_panic_boundary1_encode_wire(
    const w_seed_parallel_typed_binding1_input *typed_input,
    const w_seed_parallel_typed_binding1_result *measured,
    const w_seed_parallel_typed_binding1_panic_signal *signal,
    uint32_t child_pid, uint64_t invocation_nonce,
    uint8_t wire[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES]) {
  w_seed_parallel_typed_binding1_result canonical;
  if (typed_input == NULL || measured == NULL || signal == NULL || wire == NULL ||
      child_pid == 0u || invocation_nonce == 0u ||
      !typed_input_is_boundary_witness(typed_input, &canonical) ||
      !measurement_matches(typed_input, measured, &canonical) ||
      signal->source_index != 1u || signal->source_index >= typed_input->task_count ||
      signal->lexical_index != typed_input->tasks[signal->source_index].lexical_index ||
      signal->call_index != typed_input->tasks[signal->source_index].call_index ||
      !panic_code_valid((uint32_t)signal->panic_code) ||
      !signal->panic_requested || !signal->cancellation_requested ||
      signal->panic_source_index != signal->source_index ||
      signal->cancellation_source_index != signal->source_index ||
      signal->started_count != typed_input->task_count ||
      signal->settled_count != typed_input->task_count ||
      signal->canceled_before_start_count != 0u ||
      signal->maximum_active != typed_input->provider_capacity ||
      signal->semantic_result_published ||
      memcmp(signal->hir_semantic_digest, typed_input->hir_result->semantic_digest,
             sizeof(signal->hir_semantic_digest)) != 0 ||
      memcmp(signal->error_identity_digest, measured->error_identity.digest,
             sizeof(signal->error_identity_digest)) != 0)
    return false;

  (void)memset(wire, 0, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES);
  wire_put_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_MAGIC_OFFSET,
               W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_MAGIC);
  wire_put_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_VERSION_OFFSET,
               W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PROTOCOL_VERSION);
  wire_put_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_TYPE_OFFSET,
               W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_TYPE_PANIC);
  wire_put_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_LENGTH_OFFSET,
               W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES);
  wire_put_u64(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_NONCE_OFFSET,
               invocation_nonce);
  wire_put_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_SEQUENCE_OFFSET, 1u);
  wire_put_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_RESERVED_OFFSET, 0u);
  (void)memcpy(wire + W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_SCHEMA_OFFSET,
               W_SEED_PARALLEL_PANIC_BOUNDARY1_SCHEMA_VERSION,
               sizeof(W_SEED_PARALLEL_PANIC_BOUNDARY1_SCHEMA_VERSION) - 1u);
  wire_put_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_STATUS_OFFSET,
               W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_STATUS_PANIC);
  wire_put_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_TARGET_OFFSET,
               (uint32_t)typed_input->provider.target);
  wire_put_u32(wire,
               W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PROVIDER_CAPACITY_OFFSET,
               typed_input->provider_capacity);
  wire_put_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PROVIDER_KIND_OFFSET,
               (uint32_t)W_SEED_PARALLEL_PROVIDER0_KIND_WINDOWS_KERNEL32);
  wire_put_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_GENERATION_OFFSET,
               typed_input->generation);
  wire_put_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_TASK_COUNT_OFFSET,
               (uint32_t)typed_input->task_count);
  wire_put_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_SOURCE_INDEX_OFFSET,
               signal->source_index);
  wire_put_u32(wire,
               W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_LEXICAL_INDEX_OFFSET,
               signal->lexical_index);
  wire_put_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_CALL_INDEX_OFFSET,
               signal->call_index);
  wire_put_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PANIC_CODE_OFFSET,
               (uint32_t)signal->panic_code);
  wire_put_u32(
      wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_SEMANTIC_PUBLISHED_OFFSET,
      signal->semantic_result_published ? 1u : 0u);
  wire_put_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_CHILD_PID_OFFSET,
               child_pid);
  wire_put_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_STARTED_COUNT_OFFSET,
               signal->started_count);
  wire_put_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_SETTLED_COUNT_OFFSET,
               signal->settled_count);
  wire_put_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_CANCELED_COUNT_OFFSET,
               signal->canceled_before_start_count);
  wire_put_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_MAXIMUM_ACTIVE_OFFSET,
               signal->maximum_active);
  wire_put_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_CANCEL_SOURCE_OFFSET,
               signal->cancellation_source_index);
  wire_put_u32(
      wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_CANCEL_REQUESTED_OFFSET,
      signal->cancellation_requested ? 1u : 0u);
  wire_put_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PANIC_SOURCE_OFFSET,
               signal->panic_source_index);
  wire_put_u32(
      wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PANIC_REQUESTED_OFFSET,
      signal->panic_requested ? 1u : 0u);
  (void)memcpy(wire + W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_HIR_DIGEST_OFFSET,
               signal->hir_semantic_digest,
               sizeof(signal->hir_semantic_digest));
  (void)memcpy(
      wire + W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_ERROR_DIGEST_OFFSET,
      signal->error_identity_digest, sizeof(signal->error_identity_digest));
  (void)memcpy(
      wire + W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_AUTHORITY_DIGEST_OFFSET,
      typed_input->provider_authority->receipt.contract_digest,
      sizeof(typed_input->provider_authority->receipt.contract_digest));
  wire_seal_provenance(wire);
  return true;
}

static bool wire_header_valid(
    const uint8_t wire[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES]) {
  uint8_t expected_schema[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_SCHEMA_BYTES] =
      {0};
  (void)memcpy(expected_schema, W_SEED_PARALLEL_PANIC_BOUNDARY1_SCHEMA_VERSION,
               sizeof(W_SEED_PARALLEL_PANIC_BOUNDARY1_SCHEMA_VERSION) - 1u);
  return wire != NULL &&
         wire_get_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_MAGIC_OFFSET) ==
             W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_MAGIC &&
         wire_get_u32(wire,
                      W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_VERSION_OFFSET) ==
             W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PROTOCOL_VERSION &&
         wire_get_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_TYPE_OFFSET) ==
             W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_TYPE_PANIC &&
         wire_get_u32(wire,
                      W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_LENGTH_OFFSET) ==
             W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES &&
         wire_get_u64(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_NONCE_OFFSET) !=
             0u &&
         wire_get_u32(wire,
                      W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_SEQUENCE_OFFSET) ==
             1u &&
         wire_get_u32(wire,
                      W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_RESERVED_OFFSET) ==
             0u &&
         memcmp(wire + W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_SCHEMA_OFFSET,
                expected_schema, sizeof(expected_schema)) == 0;
}

static bool wire_provenance_valid(
    const uint8_t wire[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES]) {
  uint8_t candidate[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES];
  if (wire == NULL) return false;
  (void)memcpy(candidate, wire, sizeof(candidate));
  (void)memset(candidate +
                   W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PROVENANCE_DIGEST_OFFSET,
               0, 32u);
  wire_seal_provenance(candidate);
  return memcmp(candidate +
                    W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PROVENANCE_DIGEST_OFFSET,
                wire +
                    W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PROVENANCE_DIGEST_OFFSET,
                32u) == 0;
}

static bool wire_expected(
    const uint8_t wire[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES],
    const w_seed_parallel_panic_boundary1_input *input,
    const w_seed_parallel_typed_binding1_result *measured, uint32_t child_pid) {
  if (!wire_header_valid(wire) || !wire_provenance_valid(wire) || input == NULL ||
      input->typed_input == NULL || measured == NULL || child_pid == 0u)
    return false;
  bool semantic_published = false;
  bool cancel_requested = false;
  bool panic_requested = false;
  if (!wire_bool(wire,
                 W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_SEMANTIC_PUBLISHED_OFFSET,
                 &semantic_published) ||
      !wire_bool(wire,
                 W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_CANCEL_REQUESTED_OFFSET,
                 &cancel_requested) ||
      !wire_bool(wire,
                 W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PANIC_REQUESTED_OFFSET,
                 &panic_requested))
    return false;
  const w_seed_parallel_typed_binding1_input *typed = input->typed_input;
  return wire_get_u64(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_NONCE_OFFSET) ==
             input->invocation_nonce &&
         wire_get_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_STATUS_OFFSET) ==
             W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_STATUS_PANIC &&
         wire_get_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_TARGET_OFFSET) ==
             (uint32_t)typed->provider.target &&
         wire_get_u32(
             wire,
             W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PROVIDER_CAPACITY_OFFSET) ==
             typed->provider_capacity &&
         wire_get_u32(wire,
                      W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PROVIDER_KIND_OFFSET) ==
             (uint32_t)W_SEED_PARALLEL_PROVIDER0_KIND_WINDOWS_KERNEL32 &&
         wire_get_u32(wire,
                      W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_GENERATION_OFFSET) ==
             typed->generation &&
         wire_get_u32(wire,
                      W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_TASK_COUNT_OFFSET) ==
             typed->task_count &&
         wire_get_u32(wire,
                      W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_SOURCE_INDEX_OFFSET) ==
             1u &&
         wire_get_u32(
             wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_LEXICAL_INDEX_OFFSET) ==
             typed->tasks[1].lexical_index &&
         wire_get_u32(wire,
                      W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_CALL_INDEX_OFFSET) ==
             typed->tasks[1].call_index &&
         wire_get_u32(wire,
                      W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PANIC_CODE_OFFSET) ==
             input->expected_panic_code &&
         !semantic_published &&
         wire_get_u32(wire,
                      W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_CHILD_PID_OFFSET) ==
             child_pid &&
         wire_get_u32(
             wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_STARTED_COUNT_OFFSET) ==
             typed->task_count &&
         wire_get_u32(
             wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_SETTLED_COUNT_OFFSET) ==
             typed->task_count &&
         wire_get_u32(
             wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_CANCELED_COUNT_OFFSET) ==
             0u &&
         wire_get_u32(
             wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_MAXIMUM_ACTIVE_OFFSET) ==
             typed->provider_capacity &&
         wire_get_u32(
             wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_CANCEL_SOURCE_OFFSET) ==
             1u &&
         cancel_requested &&
         wire_get_u32(
             wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PANIC_SOURCE_OFFSET) ==
             1u &&
         panic_requested &&
         memcmp(wire + W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_HIR_DIGEST_OFFSET,
                typed->hir_result->semantic_digest, 32u) == 0 &&
         memcmp(wire + W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_ERROR_DIGEST_OFFSET,
                measured->error_identity.digest, 32u) == 0 &&
         memcmp(
             wire + W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_AUTHORITY_DIGEST_OFFSET,
             typed->provider_authority->receipt.contract_digest, 32u) == 0;
}

static void receipt_from_wire(
    const uint8_t wire[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES],
    w_seed_parallel_panic_boundary1_receipt *receipt) {
  (void)memset(receipt, 0, sizeof(*receipt));
  (void)memcpy(receipt->schema, wire +
                   W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_SCHEMA_OFFSET,
               sizeof(receipt->schema));
  receipt->protocol_version = wire_get_u32(
      wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_VERSION_OFFSET);
  receipt->frame_type =
      wire_get_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_TYPE_OFFSET);
  receipt->frame_length =
      wire_get_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_LENGTH_OFFSET);
  receipt->invocation_nonce =
      wire_get_u64(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_NONCE_OFFSET);
  receipt->sequence =
      wire_get_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_SEQUENCE_OFFSET);
  receipt->wire_status = (w_seed_parallel_panic_boundary1_wire_status)wire_get_u32(
      wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_STATUS_OFFSET);
  receipt->target =
      wire_get_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_TARGET_OFFSET);
  receipt->provider_capacity = wire_get_u32(
      wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PROVIDER_CAPACITY_OFFSET);
  receipt->provider_kind = wire_get_u32(
      wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PROVIDER_KIND_OFFSET);
  receipt->generation =
      wire_get_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_GENERATION_OFFSET);
  receipt->task_count =
      wire_get_u32(wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_TASK_COUNT_OFFSET);
  receipt->panic_source_index = wire_get_u32(
      wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_SOURCE_INDEX_OFFSET);
  receipt->panic_source_lexical_index = wire_get_u32(
      wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_LEXICAL_INDEX_OFFSET);
  receipt->panic_source_call_index = wire_get_u32(
      wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_CALL_INDEX_OFFSET);
  receipt->panic_code = (w_seed_parallel_platform1_panic_code)wire_get_u32(
      wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PANIC_CODE_OFFSET);
  (void)wire_bool(
      wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_SEMANTIC_PUBLISHED_OFFSET,
      &receipt->semantic_result_published);
  receipt->child_pid = wire_get_u32(
      wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_CHILD_PID_OFFSET);
  receipt->started_count = wire_get_u32(
      wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_STARTED_COUNT_OFFSET);
  receipt->settled_count = wire_get_u32(
      wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_SETTLED_COUNT_OFFSET);
  receipt->canceled_before_start_count = wire_get_u32(
      wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_CANCELED_COUNT_OFFSET);
  receipt->maximum_active = wire_get_u32(
      wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_MAXIMUM_ACTIVE_OFFSET);
  receipt->cancellation_source_index = wire_get_u32(
      wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_CANCEL_SOURCE_OFFSET);
  (void)wire_bool(
      wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_CANCEL_REQUESTED_OFFSET,
      &receipt->cancellation_requested);
  receipt->panic_receipt_source_index = wire_get_u32(
      wire, W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PANIC_SOURCE_OFFSET);
  (void)wire_bool(wire,
                  W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PANIC_REQUESTED_OFFSET,
                  &receipt->panic_requested);
  (void)memcpy(receipt->hir_semantic_digest,
               wire + W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_HIR_DIGEST_OFFSET,
               sizeof(receipt->hir_semantic_digest));
  (void)memcpy(
      receipt->error_identity_digest,
      wire + W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_ERROR_DIGEST_OFFSET,
      sizeof(receipt->error_identity_digest));
  (void)memcpy(
      receipt->authority_digest,
      wire + W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_AUTHORITY_DIGEST_OFFSET,
      sizeof(receipt->authority_digest));
  (void)memcpy(
      receipt->wire_provenance_digest,
      wire + W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PROVENANCE_DIGEST_OFFSET,
      sizeof(receipt->wire_provenance_digest));
}

static w_seed_parallel_panic_boundary1_status parent_revalidate(
    const w_seed_parallel_panic_boundary1_input *input,
    const w_seed_parallel_typed_binding1_result *before) {
  if (input == NULL || before == NULL || input->typed_input == NULL)
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_HIR;
  if (!w_seed_parallel_local_provider1_verify(
          input->typed_input->provider_authority))
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_AUTHORITY;
  if (!w_seed_hir0_verify(input->typed_input->hir_program,
                          input->typed_input->hir_result))
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_HIR;
  w_seed_parallel_typed_binding1_result after;
  if (!typed_input_is_boundary_witness(input->typed_input, &after))
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_PARBIND;
  return measurement_matches(input->typed_input, before, &after)
             ? W_SEED_PARALLEL_PANIC_BOUNDARY1_OK
             : W_SEED_PARALLEL_PANIC_BOUNDARY1_HIR;
}

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct {
  PROCESS_INFORMATION process;
  HANDLE job;
} panic_boundary1_child;

/* The job object contains this private helper invocation.  PANICBOUNDARY1
 * records and verifies only the root process handle below.  It does not
 * claim that a descendant tree was drained. */

static void snapshot_bytes(w_seed_sha256_state *state, uint32_t ordinal,
                           const void *pointer, size_t bytes) {
  sha_u32(state, ordinal);
  sha_u64(state, (uint64_t)bytes);
  if (bytes != 0u)
    w_seed_sha256_update(state, (const uint8_t *)pointer, bytes);
}

static bool snapshot_array(w_seed_sha256_state *state, uint32_t ordinal,
                           const void *pointer, size_t count,
                           size_t element_size) {
  if (state == NULL || element_size == 0u ||
      count > SIZE_MAX / element_size ||
      (count != 0u && pointer == NULL))
    return false;
  snapshot_bytes(state, ordinal, pointer, count * element_size);
  return true;
}

static bool snapshot_hir(w_seed_sha256_state *state,
                         const w_seed_hir0_program *program) {
  if (state == NULL || program == NULL) return false;
  snapshot_bytes(state, 1u, program, sizeof(*program));
#define W_PANIC_BOUNDARY1_SNAPSHOT(pointer, number, ordinal)                  \
  do {                                                                        \
    if (!snapshot_array(state, (ordinal), (pointer), (number),               \
                        sizeof(*(pointer))))                                 \
      return false;                                                           \
  } while (0)
  W_PANIC_BOUNDARY1_SNAPSHOT(program->modules, program->module_count, 2u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->identities, program->identity_count,
                             3u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->types, program->type_count, 4u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->enums, program->enum_count, 5u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->enum_cases, program->enum_case_count,
                             6u);
  W_PANIC_BOUNDARY1_SNAPSHOT(
      program->enum_case_parameters, program->enum_case_parameter_count, 7u);
  W_PANIC_BOUNDARY1_SNAPSHOT(
      program->enum_subset_members, program->enum_subset_member_count, 8u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->functions, program->function_count, 9u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->parameters, program->parameter_count,
                             10u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->blocks, program->block_count, 11u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->block_arguments,
                             program->block_argument_count, 12u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->edge_arguments,
                             program->edge_argument_count, 13u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->switch_edges,
                             program->switch_edge_count, 14u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->switch_captures,
                             program->switch_capture_count, 15u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->instructions,
                             program->instruction_count, 16u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->bindings, program->binding_count, 17u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->calls, program->call_count, 18u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->host_parameters,
                             program->host_parameter_count, 19u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->arguments, program->argument_count,
                             20u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->enum_payloads,
                             program->enum_payload_count, 21u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->requirements,
                             program->requirement_count, 22u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->values, program->value_count, 23u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->interpolation_segments,
                             program->interpolation_segment_count, 24u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->terminators,
                             program->terminator_count, 25u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->entries, program->entry_count, 26u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->text_bytes, program->text_byte_count,
                             27u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->value_bytes, program->value_byte_count,
                             28u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->receipt, program->receipt_count, 29u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->external_modules,
                             program->external_module_count, 30u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->external_symbols,
                             program->external_symbol_count, 31u);
  W_PANIC_BOUNDARY1_SNAPSHOT(program->cleanups, program->cleanup_count, 32u);
#undef W_PANIC_BOUNDARY1_SNAPSHOT
  return true;
}

static bool boundary_input_snapshot(
    const w_seed_parallel_panic_boundary1_input *input,
    uint8_t digest[32]) {
  if (input == NULL || digest == NULL || input->typed_input == NULL ||
      input->typed_input->hir_program == NULL ||
      input->typed_input->hir_result == NULL ||
      input->typed_input->provider_authority == NULL)
    return false;
  size_t helper_length = 0u;
  if (!bounded_c_string_length(input->helper_path, 4096u, &helper_length) ||
      helper_length == SIZE_MAX)
    return false;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(
      &state,
      (const uint8_t *)"w-seed-parallel-panic-boundary1-input-snapshot-1",
      sizeof("w-seed-parallel-panic-boundary1-input-snapshot-1") - 1u);
  snapshot_bytes(&state, 100u, input, sizeof(*input));
  const w_seed_parallel_typed_binding1_input *typed = input->typed_input;
  snapshot_bytes(&state, 101u, typed, sizeof(*typed));
  snapshot_bytes(&state, 102u, typed->hir_result, sizeof(*typed->hir_result));
  if (!snapshot_hir(&state, typed->hir_program) ||
      !snapshot_array(&state, 103u, typed->tasks, typed->task_count,
                      sizeof(*typed->tasks)))
    return false;
  snapshot_bytes(&state, 104u, typed->provider_authority,
                 sizeof(*typed->provider_authority));
  snapshot_bytes(&state, 105u, &typed->provider_job,
                 sizeof(typed->provider_job));
  sha_u64(&state, (uint64_t)(uintptr_t)typed->provider_job.context);
  sha_u64(&state, (uint64_t)typed->provider_job.context_bytes);
  snapshot_bytes(&state, 106u, input->helper_path, helper_length + 1u);
  w_seed_sha256_final(&state, digest);
  return true;
}

static bool boundary_input_snapshot_matches(
    const w_seed_parallel_panic_boundary1_input *input,
    const uint8_t expected[32]) {
  uint8_t actual[32];
  return boundary_input_snapshot(input, actual) &&
         memcmp(actual, expected, sizeof(actual)) == 0;
}

static uint64_t deadline_make(uint32_t timeout_ms) {
  const uint64_t now = GetTickCount64();
  return now > UINT64_MAX - timeout_ms ? UINT64_MAX : now + timeout_ms;
}

static DWORD deadline_remaining(uint64_t deadline) {
  const uint64_t now = GetTickCount64();
  if (now >= deadline) return 0u;
  const uint64_t difference = deadline - now;
  return difference > (uint64_t)MAXDWORD ? MAXDWORD : (DWORD)difference;
}

static uint64_t read_deadline(uint64_t deadline, uint32_t timeout_ms) {
  const uint32_t teardown_reserve_ms = 100u;
  if (timeout_ms <= teardown_reserve_ms) return deadline;
  return deadline - teardown_reserve_ms;
}

static bool close_handle_checked(HANDLE handle) {
  if (handle == NULL) return true;
  const BOOL closed = CloseHandle(handle);
  return closed != FALSE;
}

static bool append_wide_char(wchar_t *buffer, size_t capacity, size_t *length,
                             wchar_t value) {
  if (buffer == NULL || length == NULL || *length >= capacity - 1u) return false;
  buffer[*length] = value;
  *length += 1u;
  buffer[*length] = L'\0';
  return true;
}

static bool append_wide_literal(wchar_t *buffer, size_t capacity, size_t *length,
                               const wchar_t *literal) {
  if (literal == NULL) return false;
  for (size_t index = 0u; literal[index] != L'\0'; index += 1u)
    if (!append_wide_char(buffer, capacity, length, literal[index])) return false;
  return true;
}

static bool append_wide_u64(wchar_t *buffer, size_t capacity, size_t *length,
                            uint64_t value) {
  wchar_t digits[32];
  size_t digit_count = 0u;
  do {
    digits[digit_count] = (wchar_t)(L'0' + (wchar_t)(value % 10u));
    digit_count += 1u;
    value /= 10u;
  } while (value != 0u && digit_count < sizeof(digits) / sizeof(digits[0]));
  if (value != 0u) return false;
  while (digit_count != 0u) {
    digit_count -= 1u;
    if (!append_wide_char(buffer, capacity, length, digits[digit_count]))
      return false;
  }
  return true;
}

static bool path_to_wide(const char *path, wchar_t *wide, size_t capacity) {
  if (path == NULL || wide == NULL || capacity == 0u || capacity > INT_MAX)
    return false;
  const int path_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                               path, -1, wide, (int)capacity);
  return path_length > 1 && (size_t)path_length <= capacity;
}

static bool command_line_make(
    const wchar_t *wide_path, HANDLE write_handle,
    w_seed_parallel_panic_boundary1_fault fault, uint64_t invocation_nonce,
    wchar_t command_line[8192]) {
  if (wide_path == NULL || write_handle == NULL || command_line == NULL)
    return false;
  command_line[0] = L'\0';
  size_t length = 0u;
  if (!append_wide_char(command_line, 8192u, &length, L'"')) return false;
  size_t trailing_backslashes = 0u;
  for (size_t index = 0u; wide_path[index] != L'\0'; index += 1u) {
    if (wide_path[index] == L'"') return false;
    if (!append_wide_char(command_line, 8192u, &length, wide_path[index]))
      return false;
    trailing_backslashes = wide_path[index] == L'\\'
                               ? trailing_backslashes + 1u
                               : 0u;
  }
  for (size_t index = 0u; index < trailing_backslashes; index += 1u)
    if (!append_wide_char(command_line, 8192u, &length, L'\\')) return false;
  if (!append_wide_char(command_line, 8192u, &length, L'"') ||
      !append_wide_char(command_line, 8192u, &length, L' ') ||
      !append_wide_literal(command_line, 8192u, &length,
                           L"--w-seed-write-handle=") ||
      !append_wide_u64(command_line, 8192u, &length,
                       (uint64_t)(uintptr_t)write_handle) ||
      !append_wide_char(command_line, 8192u, &length, L' ') ||
      !append_wide_literal(command_line, 8192u, &length, L"--w-seed-fault=") ||
      !append_wide_u64(command_line, 8192u, &length, (uint64_t)fault) ||
      !append_wide_char(command_line, 8192u, &length, L' ') ||
      !append_wide_literal(command_line, 8192u, &length,
                           L"--w-seed-invocation-nonce=") ||
      !append_wide_u64(command_line, 8192u, &length, invocation_nonce))
    return false;
  return true;
}

static bool failed_child_dispose(panic_boundary1_child *child,
                                 uint64_t deadline) {
  if (child == NULL) return false;
  bool ok = true;
  if (child->process.hProcess != NULL) {
    const DWORD state = WaitForSingleObject(child->process.hProcess, 0u);
    if (state == WAIT_FAILED) {
      ok = false;
    } else if (state == WAIT_TIMEOUT) {
      if (TerminateProcess(
              child->process.hProcess,
              W_SEED_PARALLEL_PANIC_BOUNDARY1_TERMINATION_EXIT_CODE) == FALSE)
        ok = false;
      const DWORD joined = WaitForSingleObject(
          child->process.hProcess, deadline_remaining(deadline));
      if (joined != WAIT_OBJECT_0) ok = false;
    } else if (state != WAIT_OBJECT_0) {
      ok = false;
    }
  }
  ok = close_handle_checked(child->process.hThread) && ok;
  ok = close_handle_checked(child->process.hProcess) && ok;
  ok = close_handle_checked(child->job) && ok;
  child->process.hThread = NULL;
  child->process.hProcess = NULL;
  child->job = NULL;
  return ok;
}

static bool create_child(
    const char *path, HANDLE write_handle,
    w_seed_parallel_panic_boundary1_fault fault, uint64_t invocation_nonce,
    uint64_t deadline, panic_boundary1_child *child) {
  if (path == NULL || write_handle == NULL || child == NULL) return false;
  (void)memset(child, 0, sizeof(*child));
  wchar_t wide_path[4096];
  wchar_t command_line[8192];
  if (!path_to_wide(path, wide_path, sizeof(wide_path) / sizeof(wide_path[0])) ||
      !command_line_make(wide_path, write_handle, fault, invocation_nonce,
                         command_line))
    return false;

  SIZE_T attribute_bytes = 0u;
  if (InitializeProcThreadAttributeList(NULL, 1u, 0u, &attribute_bytes) != FALSE ||
      GetLastError() != ERROR_INSUFFICIENT_BUFFER || attribute_bytes == 0u ||
      attribute_bytes > 4096u)
    return false;
  LPPROC_THREAD_ATTRIBUTE_LIST attributes =
      (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                              attribute_bytes);
  if (attributes == NULL ||
      InitializeProcThreadAttributeList(attributes, 1u, 0u, &attribute_bytes) ==
          FALSE) {
    (void)HeapFree(GetProcessHeap(), 0u, attributes);
    return false;
  }
  HANDLE inherited_handles[1] = {write_handle};
  if (UpdateProcThreadAttribute(
          attributes, 0u, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
          inherited_handles, sizeof(inherited_handles), NULL, NULL) == FALSE) {
    (void)DeleteProcThreadAttributeList(attributes);
    (void)HeapFree(GetProcessHeap(), 0u, attributes);
    return false;
  }

  child->job = CreateJobObjectW(NULL, NULL);
  if (child->job == NULL) {
    (void)DeleteProcThreadAttributeList(attributes);
    (void)HeapFree(GetProcessHeap(), 0u, attributes);
    return false;
  }
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {0};
  limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (SetInformationJobObject(child->job, JobObjectExtendedLimitInformation,
                               &limits, sizeof(limits)) == FALSE) {
    const bool disposed = failed_child_dispose(child, deadline);
    (void)DeleteProcThreadAttributeList(attributes);
    (void)HeapFree(GetProcessHeap(), 0u, attributes);
    if (!disposed) return false;
    return false;
  }

  STARTUPINFOEXW startup = {0};
  startup.StartupInfo.cb = sizeof(startup);
  startup.lpAttributeList = attributes;
  const DWORD creation_flags = EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW |
                               CREATE_SUSPENDED;
  const BOOL created = CreateProcessW(
      wide_path, command_line, NULL, NULL, TRUE, creation_flags, NULL, NULL,
      &startup.StartupInfo, &child->process);
  (void)DeleteProcThreadAttributeList(attributes);
  (void)HeapFree(GetProcessHeap(), 0u, attributes);
  if (created == FALSE || child->process.hProcess == NULL ||
      child->process.hThread == NULL) {
    if (!failed_child_dispose(child, deadline)) return false;
    return false;
  }
  if (AssignProcessToJobObject(child->job, child->process.hProcess) == FALSE ||
      deadline_remaining(deadline) == 0u || ResumeThread(child->process.hThread) ==
          (DWORD)-1) {
    if (!failed_child_dispose(child, deadline)) return false;
    return false;
  }
  return true;
}

static bool pipe_has_trailing(HANDLE read_handle, bool *trailing) {
  if (read_handle == NULL || trailing == NULL) return false;
  DWORD available = 0u;
  if (PeekNamedPipe(read_handle, NULL, 0u, NULL, &available, NULL) == FALSE) {
    if (GetLastError() == ERROR_BROKEN_PIPE) {
      *trailing = false;
      return true;
    }
    return false;
  }
  *trailing = available != 0u;
  return true;
}

static bool pipe_read_bounded(
    HANDLE process, HANDLE read_handle, uint64_t deadline,
    uint8_t wire[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES],
    w_seed_parallel_panic_boundary1_status *status) {
  if (process == NULL || read_handle == NULL || wire == NULL || status == NULL)
    return false;
  for (;;) {
    DWORD available = 0u;
    if (PeekNamedPipe(read_handle, NULL, 0u, NULL, &available, NULL) == FALSE) {
      if (GetLastError() == ERROR_BROKEN_PIPE) {
        const DWORD state =
            WaitForSingleObject(process, deadline_remaining(deadline));
        if (state == WAIT_OBJECT_0)
          *status = W_SEED_PARALLEL_PANIC_BOUNDARY1_EARLY_EXIT;
        else if (state == WAIT_FAILED)
          *status = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEARDOWN;
        else
          *status = W_SEED_PARALLEL_PANIC_BOUNDARY1_PROTOCOL;
      } else {
        *status = W_SEED_PARALLEL_PANIC_BOUNDARY1_PROTOCOL;
      }
      return false;
    }
    if (available > W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES) {
      *status = W_SEED_PARALLEL_PANIC_BOUNDARY1_PROTOCOL;
      return false;
    }
    if (available == W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES) {
      DWORD read_bytes = 0u;
      /* Peek proves the complete fixed frame is already buffered.  The sole
       * reader is this parent and the sole writer is the child, so this exact
       * ReadFile cannot wait for a future byte or exceed the deadline. */
      if (ReadFile(read_handle, wire,
                   W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES, &read_bytes,
                   NULL) == FALSE ||
          read_bytes != W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES) {
        *status = W_SEED_PARALLEL_PANIC_BOUNDARY1_PARTIAL;
        return false;
      }
      DWORD trailing = 0u;
      if (PeekNamedPipe(read_handle, NULL, 0u, NULL, &trailing, NULL) == FALSE) {
        *status = W_SEED_PARALLEL_PANIC_BOUNDARY1_PROTOCOL;
        return false;
      }
      if (trailing != 0u) {
        *status = W_SEED_PARALLEL_PANIC_BOUNDARY1_PROTOCOL;
        return false;
      }
      return true;
    }
    const DWORD state = WaitForSingleObject(process, 0u);
    if (state == WAIT_FAILED) {
      *status = W_SEED_PARALLEL_PANIC_BOUNDARY1_TEARDOWN;
      return false;
    }
    if (state == WAIT_OBJECT_0) {
      *status = available == 0u
                    ? W_SEED_PARALLEL_PANIC_BOUNDARY1_EARLY_EXIT
                    : W_SEED_PARALLEL_PANIC_BOUNDARY1_PARTIAL;
      return false;
    }
    const DWORD remaining = deadline_remaining(deadline);
    if (remaining == 0u) {
      *status = available == 0u
                    ? W_SEED_PARALLEL_PANIC_BOUNDARY1_TIMEOUT
                    : W_SEED_PARALLEL_PANIC_BOUNDARY1_PARTIAL;
      return false;
    }
    (void)Sleep(remaining < 1u ? remaining : 1u);
  }
}

static w_seed_parallel_panic_boundary1_status child_cleanup(
    panic_boundary1_child *child, HANDLE read_handle, HANDLE write_handle,
    uint64_t deadline, bool terminate, bool *terminated, bool *joined,
    uint32_t *exit_code, bool *handles_closed, bool *trailing) {
  if (child == NULL || terminated == NULL || joined == NULL || exit_code == NULL ||
      handles_closed == NULL || trailing == NULL)
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_TEARDOWN;
  bool ok = true;
  *terminated = false;
  *joined = false;
  *exit_code = 0u;
  *handles_closed = false;
  *trailing = false;
  ok = close_handle_checked(write_handle) && ok;
  if (child->process.hProcess == NULL || child->process.hThread == NULL ||
      child->job == NULL)
    ok = false;
  if (child->process.hProcess != NULL) {
    DWORD state = WaitForSingleObject(child->process.hProcess, 0u);
    if (state == WAIT_FAILED)
      ok = false;
    if (terminate && state == WAIT_TIMEOUT) {
      if (TerminateProcess(
              child->process.hProcess,
              W_SEED_PARALLEL_PANIC_BOUNDARY1_TERMINATION_EXIT_CODE) != FALSE) {
        *terminated = true;
      }
      /* A failed termination can mean that the child exited after the
       * zero-time probe. The bounded join below is the authoritative result;
       * it still fails closed when the process remains live. */
    }
    if (state == WAIT_OBJECT_0 ||
        WaitForSingleObject(child->process.hProcess,
                           deadline_remaining(deadline)) == WAIT_OBJECT_0)
      *joined = true;
    else
      ok = false;
    DWORD observed_exit = 0u;
    if (!GetExitCodeProcess(child->process.hProcess, &observed_exit))
      ok = false;
    else
      *exit_code = observed_exit;
  }
  if (*joined && !pipe_has_trailing(read_handle, trailing))
    ok = false;
  ok = close_handle_checked(read_handle) && ok;
  ok = close_handle_checked(child->process.hThread) && ok;
  ok = close_handle_checked(child->process.hProcess) && ok;
  ok = close_handle_checked(child->job) && ok;
  child->process.hThread = NULL;
  child->process.hProcess = NULL;
  child->job = NULL;
  *handles_closed = ok;
  return ok ? W_SEED_PARALLEL_PANIC_BOUNDARY1_OK
            : W_SEED_PARALLEL_PANIC_BOUNDARY1_TEARDOWN;
}

w_seed_parallel_panic_boundary1_status w_seed_parallel_panic_boundary1_run(
    const w_seed_parallel_panic_boundary1_input *input,
    const w_seed_parallel_panic_boundary1_output *output) {
  w_seed_parallel_typed_binding1_result measured;
  uint32_t timeout_ms = 0u;
  w_seed_parallel_panic_boundary1_status status = input_validate(
      input, &measured, &timeout_ms);
  if (status != W_SEED_PARALLEL_PANIC_BOUNDARY1_OK) return status;
  if (output == NULL || output->receipt == NULL)
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_INVALID;
  if (output->receipt_capacity == 0u)
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_CAPACITY;
  if (output_aliases_input(input, output))
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_ALIAS;
  uint8_t input_snapshot[32];
  if (!boundary_input_snapshot(input, input_snapshot))
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_PARBIND;

  const uint64_t deadline = deadline_make(timeout_ms);
  SECURITY_ATTRIBUTES security = {sizeof(security), NULL, TRUE};
  HANDLE read_handle = NULL;
  HANDLE write_handle = NULL;
  panic_boundary1_child child;
  (void)memset(&child, 0, sizeof(child));
  if (!CreatePipe(&read_handle, &write_handle, &security,
                  W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES) ||
      !SetHandleInformation(read_handle, HANDLE_FLAG_INHERIT, 0u)) {
    (void)close_handle_checked(read_handle);
    (void)close_handle_checked(write_handle);
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_TEARDOWN;
  }
  if (!create_child(input->helper_path, write_handle, input->helper_fault,
                    input->invocation_nonce, deadline, &child)) {
    (void)close_handle_checked(read_handle);
    (void)close_handle_checked(write_handle);
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_EARLY_EXIT;
  }
  if (!close_handle_checked(write_handle)) {
    bool terminated = false;
    bool joined = false;
    uint32_t exit_code = 0u;
    bool handles_closed = false;
    bool trailing = false;
    (void)child_cleanup(&child, read_handle, write_handle, deadline, true,
                         &terminated, &joined, &exit_code, &handles_closed,
                         &trailing);
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_TEARDOWN;
  }
  write_handle = NULL;

  uint8_t wire[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES];
  w_seed_parallel_panic_boundary1_status read_status =
      W_SEED_PARALLEL_PANIC_BOUNDARY1_PROTOCOL;
  const bool read_ok = pipe_read_bounded(
      child.process.hProcess, read_handle, read_deadline(deadline, timeout_ms),
      wire, &read_status);
  bool terminate = true;
  bool candidate_pending = false;
  w_seed_parallel_panic_boundary1_receipt candidate;
  if (!read_ok) {
    status = read_status;
  } else if (!wire_expected(wire, input, &measured, child.process.dwProcessId)) {
    status = wire_header_valid(wire) &&
                      wire_get_u32(wire,
                                   W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_STATUS_OFFSET) !=
                          W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_STATUS_PANIC
                  ? W_SEED_PARALLEL_PANIC_BOUNDARY1_NON_PANIC
                  : W_SEED_PARALLEL_PANIC_BOUNDARY1_PROTOCOL;
  } else if (WaitForSingleObject(child.process.hProcess, 0u) != WAIT_TIMEOUT) {
    status = W_SEED_PARALLEL_PANIC_BOUNDARY1_NOT_LIVE;
    terminate = false;
  } else {
    DWORD exit_code = 0u;
    if (!GetExitCodeProcess(child.process.hProcess, &exit_code) ||
        exit_code != STILL_ACTIVE) {
      status = W_SEED_PARALLEL_PANIC_BOUNDARY1_NOT_LIVE;
      terminate = false;
    } else {
      receipt_from_wire(wire, &candidate);
      candidate.child_was_live_at_teardown = true;
      candidate_pending = true;
      status = W_SEED_PARALLEL_PANIC_BOUNDARY1_OK;
    }
  }

  bool terminated = false;
  bool joined = false;
  uint32_t child_exit_code = 0u;
  bool handles_closed = false;
  bool trailing = false;
  const w_seed_parallel_panic_boundary1_status cleanup_status = child_cleanup(
      &child, read_handle, write_handle, deadline, terminate, &terminated,
                         &joined, &child_exit_code, &handles_closed, &trailing);
  if (cleanup_status != W_SEED_PARALLEL_PANIC_BOUNDARY1_OK)
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_TEARDOWN;
  if (trailing &&
      (status == W_SEED_PARALLEL_PANIC_BOUNDARY1_OK || candidate_pending))
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_PROTOCOL;
  if (!candidate_pending) return status;
  if (!boundary_input_snapshot_matches(input, input_snapshot))
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_PARBIND;
  candidate.child_exit_code = child_exit_code;
  candidate.child_terminated = terminated;
  candidate.child_joined = joined;
  candidate.handles_closed = handles_closed;
  if (!terminated || !joined || !handles_closed ||
      child_exit_code != W_SEED_PARALLEL_PANIC_BOUNDARY1_TERMINATION_EXIT_CODE)
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_TEARDOWN;
  status = parent_revalidate(input, &measured);
  if (status != W_SEED_PARALLEL_PANIC_BOUNDARY1_OK) return status;
  if (!boundary_input_snapshot_matches(input, input_snapshot))
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_PARBIND;
  seal_receipt_provenance(&candidate, candidate.provenance_digest);
  if (!boundary_input_snapshot_matches(input, input_snapshot))
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_PARBIND;
  *output->receipt = candidate;
  return W_SEED_PARALLEL_PANIC_BOUNDARY1_OK;
}

#else

bool w_seed_parallel_panic_boundary1_encode_wire(
    const w_seed_parallel_typed_binding1_input *typed_input,
    const w_seed_parallel_typed_binding1_result *measured,
    const w_seed_parallel_typed_binding1_panic_signal *signal,
    uint32_t child_pid, uint64_t invocation_nonce,
    uint8_t wire[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES]) {
  (void)typed_input;
  (void)measured;
  (void)signal;
  (void)child_pid;
  (void)invocation_nonce;
  (void)wire;
  return false;
}

w_seed_parallel_panic_boundary1_status w_seed_parallel_panic_boundary1_run(
    const w_seed_parallel_panic_boundary1_input *input,
    const w_seed_parallel_panic_boundary1_output *output) {
  if (input == NULL || output == NULL || output->receipt == NULL)
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_INVALID;
  if (output->receipt_capacity == 0u)
    return W_SEED_PARALLEL_PANIC_BOUNDARY1_CAPACITY;
  return W_SEED_PARALLEL_PANIC_BOUNDARY1_UNSUPPORTED;
}

#endif

bool w_seed_parallel_panic_boundary1_verify(
    const w_seed_parallel_panic_boundary1_input *input,
    const w_seed_parallel_panic_boundary1_receipt *receipt) {
#if !defined(_WIN32) || !defined(_WIN64)
  (void)input;
  (void)receipt;
  return false;
#else
  w_seed_parallel_typed_binding1_result measured;
  uint32_t timeout_ms = 0u;
  if (input == NULL || receipt == NULL ||
      input_validate(input, &measured, &timeout_ms) !=
          W_SEED_PARALLEL_PANIC_BOUNDARY1_OK ||
      receipt->protocol_version !=
          W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PROTOCOL_VERSION ||
      receipt->frame_type != W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_TYPE_PANIC ||
      receipt->frame_length != W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES ||
      receipt->invocation_nonce != input->invocation_nonce ||
      receipt->sequence != 1u ||
      memcmp(receipt->schema, W_SEED_PARALLEL_PANIC_BOUNDARY1_SCHEMA_VERSION,
             sizeof(receipt->schema)) != 0 ||
      receipt->wire_status != W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_STATUS_PANIC ||
      receipt->target !=
          (uint32_t)input->typed_input->provider.target ||
      receipt->provider_capacity != input->typed_input->provider_capacity ||
      receipt->provider_kind !=
          (uint32_t)W_SEED_PARALLEL_PROVIDER0_KIND_WINDOWS_KERNEL32 ||
      receipt->generation != input->typed_input->generation ||
      receipt->task_count != input->typed_input->task_count ||
      receipt->panic_source_index != 1u ||
      receipt->panic_source_lexical_index != input->typed_input->tasks[1].lexical_index ||
      receipt->panic_source_call_index != input->typed_input->tasks[1].call_index ||
      receipt->panic_code !=
          (w_seed_parallel_platform1_panic_code)input->expected_panic_code ||
      receipt->semantic_result_published || receipt->child_pid == 0u ||
      receipt->started_count != input->typed_input->task_count ||
      receipt->settled_count != input->typed_input->task_count ||
      receipt->canceled_before_start_count != 0u ||
      receipt->maximum_active != input->typed_input->provider_capacity ||
      receipt->cancellation_source_index != 1u ||
      !receipt->cancellation_requested ||
      receipt->panic_receipt_source_index != 1u || !receipt->panic_requested ||
      memcmp(receipt->hir_semantic_digest,
             input->typed_input->hir_result->semantic_digest,
             sizeof(receipt->hir_semantic_digest)) != 0 ||
      memcmp(receipt->error_identity_digest, measured.error_identity.digest,
             sizeof(receipt->error_identity_digest)) != 0 ||
      memcmp(receipt->authority_digest,
             input->typed_input->provider_authority->receipt.contract_digest,
             sizeof(receipt->authority_digest)) != 0 ||
      !receipt->child_was_live_at_teardown || !receipt->child_terminated ||
      !receipt->child_joined || !receipt->handles_closed ||
      receipt->child_exit_code !=
          W_SEED_PARALLEL_PANIC_BOUNDARY1_TERMINATION_EXIT_CODE)
    return false;
  w_seed_parallel_typed_binding1_panic_signal signal = {0};
  signal.source_index = receipt->panic_source_index;
  signal.lexical_index = receipt->panic_source_lexical_index;
  signal.call_index = receipt->panic_source_call_index;
  signal.panic_code = receipt->panic_code;
  signal.semantic_result_published = receipt->semantic_result_published;
  (void)memcpy(signal.hir_semantic_digest, receipt->hir_semantic_digest,
               sizeof(signal.hir_semantic_digest));
  (void)memcpy(signal.error_identity_digest, receipt->error_identity_digest,
               sizeof(signal.error_identity_digest));
  signal.started_count = receipt->started_count;
  signal.settled_count = receipt->settled_count;
  signal.canceled_before_start_count = receipt->canceled_before_start_count;
  signal.maximum_active = receipt->maximum_active;
  signal.cancellation_source_index = receipt->cancellation_source_index;
  signal.cancellation_requested = receipt->cancellation_requested;
  signal.panic_source_index = receipt->panic_receipt_source_index;
  signal.panic_requested = receipt->panic_requested;
  uint8_t wire[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES];
  if (!w_seed_parallel_panic_boundary1_encode_wire(
          input->typed_input, &measured, &signal, receipt->child_pid,
          receipt->invocation_nonce, wire) ||
      memcmp(wire + W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PROVENANCE_DIGEST_OFFSET,
             receipt->wire_provenance_digest,
             sizeof(receipt->wire_provenance_digest)) != 0)
    return false;
  uint8_t provenance_digest[32];
  seal_receipt_provenance(receipt, provenance_digest);
  return memcmp(provenance_digest, receipt->provenance_digest,
                sizeof(provenance_digest)) == 0;
#endif
}
