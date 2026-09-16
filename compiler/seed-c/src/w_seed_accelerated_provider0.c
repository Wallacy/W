#include "w_seed_accelerated_provider0.h"

#include "w_seed_sha256.h"

#include <limits.h>
#include <string.h>

static const uint8_t ACCPROV0_AUTHORITY_SEAL = 0xa7u;
static const char ACCPROV0_ARTIFACT_LINK_TAG[] =
    "w-seed-accelerated-native-artifact-link-1";
static const char ACCPROV0_SEMANTIC_TAG[] =
    "w-seed-accelerated-provider0-semantic-1";
static const char ACCPROV0_PROVENANCE_TAG[] =
    "w-seed-accelerated-provider0-provenance-1";

enum {
  ACCPROV0_INPUT_RANGE_CAPACITY = 16u,
  ACCPROV0_OUTPUT_RANGE_CAPACITY = 3u,
};

typedef struct {
  uintptr_t begin;
  uintptr_t end;
  bool active;
} accprov0_range;

typedef struct {
  const char *data;
  size_t bytes;
} accprov0_text;

static bool range_make(const void *pointer, size_t bytes,
                       accprov0_range *range) {
  if (range == NULL || (bytes != 0u && pointer == NULL)) return false;
  *range = (accprov0_range){0u, 0u, false};
  if (bytes == 0u) return true;
  const uintptr_t begin = (uintptr_t)pointer;
  if (bytes > UINTPTR_MAX - begin) return false;
  *range = (accprov0_range){begin, begin + bytes, true};
  return true;
}

static bool ranges_overlap(accprov0_range left, accprov0_range right) {
  return left.active && right.active && left.begin < right.end &&
         right.begin < left.end;
}

static bool add_range(accprov0_range *ranges, size_t capacity, size_t *count,
                      const void *pointer, size_t bytes) {
  if (ranges == NULL || count == NULL || *count >= capacity ||
      !range_make(pointer, bytes, &ranges[*count]))
    return false;
  *count += 1u;
  return true;
}

static bool text_valid(accprov0_text text) {
  return text.data != NULL && text.bytes != 0u &&
         text.bytes < W_SEED_ACCELERATED_PROVIDER0_MAX_IDENTITY_BYTES &&
         memchr(text.data, '\0', text.bytes) == NULL;
}

static bool text_equal(accprov0_text left, accprov0_text right) {
  return text_valid(left) && text_valid(right) && left.bytes == right.bytes &&
         memcmp(left.data, right.data, left.bytes) == 0;
}

static bool fixed_text_length(const char *data, size_t capacity,
                              size_t *length) {
  if (data == NULL || capacity == 0u || length == NULL) return false;
  size_t cursor = 0u;
  while (cursor < capacity && data[cursor] != '\0') cursor += 1u;
  if (cursor == 0u || cursor == capacity) return false;
  for (size_t index = cursor + 1u; index < capacity; index += 1u)
    if (data[index] != '\0') return false;
  *length = cursor;
  return true;
}

static bool fixed_text_optional(const char *data, size_t capacity) {
  if (data == NULL || capacity == 0u) return false;
  size_t cursor = 0u;
  while (cursor < capacity && data[cursor] != '\0') cursor += 1u;
  if (cursor == capacity) return false;
  for (size_t index = cursor + 1u; index < capacity; index += 1u)
    if (data[index] != '\0') return false;
  return true;
}

static bool copy_fixed(char destination[], size_t capacity,
                       accprov0_text source) {
  if (destination == NULL || capacity == 0u || !text_valid(source) ||
      source.bytes >= capacity)
    return false;
  (void)memset(destination, 0, capacity);
  (void)memcpy(destination, source.data, source.bytes);
  return true;
}

static void hash_u32(w_seed_sha256_state *state, uint32_t value) {
  const uint8_t bytes[4] = {(uint8_t)value, (uint8_t)(value >> 8u),
                            (uint8_t)(value >> 16u),
                            (uint8_t)(value >> 24u)};
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void hash_u64(w_seed_sha256_state *state, uint64_t value) {
  uint8_t bytes[8];
  for (size_t index = 0u; index < sizeof(bytes); index += 1u)
    bytes[index] = (uint8_t)(value >> (index * 8u));
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void hash_bool(w_seed_sha256_state *state, bool value) {
  hash_u32(state, value ? 1u : 0u);
}

static void hash_bytes(w_seed_sha256_state *state, const uint8_t *bytes,
                       size_t count) {
  hash_u64(state, (uint64_t)count);
  if (count != 0u) w_seed_sha256_update(state, bytes, count);
}

static void hash_text(w_seed_sha256_state *state, accprov0_text text) {
  hash_bytes(state, (const uint8_t *)text.data, text.bytes);
}

static void hash_fixed_text(w_seed_sha256_state *state, const char *text,
                            size_t capacity) {
  size_t bytes = 0u;
  if (fixed_text_length(text, capacity, &bytes))
    hash_text(state, (accprov0_text){text, bytes});
  else
    hash_bytes(state, NULL, 0u);
}

static void digest_bytes(const uint8_t *bytes, size_t count,
                         uint8_t digest[W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, bytes, count);
  w_seed_sha256_final(&state, digest);
}

static void digest_text(accprov0_text text,
                        uint8_t digest[W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  hash_text(&state, text);
  w_seed_sha256_final(&state, digest);
}

static void digest_artifact_link(
    const uint8_t request_digest[W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES],
    const uint8_t target_digest[W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES],
    const uint8_t provider_abi_digest
        [W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES],
    const uint8_t native_digest[W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES],
    uint8_t link_digest[W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, (const uint8_t *)ACCPROV0_ARTIFACT_LINK_TAG,
                       sizeof(ACCPROV0_ARTIFACT_LINK_TAG) - 1u);
  w_seed_sha256_update(&state, request_digest, 32u);
  w_seed_sha256_update(&state, target_digest, 32u);
  w_seed_sha256_update(&state, provider_abi_digest, 32u);
  w_seed_sha256_update(&state, native_digest, 32u);
  w_seed_sha256_final(&state, link_digest);
}

static bool digest_equal(const uint8_t *left, const uint8_t *right) {
  return left != NULL && right != NULL && memcmp(left, right, 32u) == 0;
}

static bool digest_nonzero(const uint8_t *digest) {
  if (digest == NULL) return false;
  uint8_t bits = 0u;
  for (size_t index = 0u; index < 32u; index += 1u)
    bits = (uint8_t)(bits | digest[index]);
  return bits != 0u;
}

static bool request_text(const w_seed_accelerated_request0_program *program,
                         size_t offset, size_t bytes, accprov0_text *text) {
  if (program == NULL || text == NULL || program->text == NULL ||
      bytes == 0u || offset > program->text_bytes ||
      bytes > program->text_bytes - offset ||
      bytes >= W_SEED_ACCELERATED_PROVIDER0_MAX_IDENTITY_BYTES ||
      memchr(program->text + offset, '\0', bytes) != NULL)
    return false;
  *text = (accprov0_text){(const char *)program->text + offset, bytes};
  return text_valid(*text);
}

static bool request_identities(
    const w_seed_accelerated_request0_program *program,
    accprov0_text *target, accprov0_text *provider_abi_class,
    accprov0_text *generation) {
  if (program == NULL || program->requests == NULL ||
      program->request_count != 1u || target == NULL ||
      provider_abi_class == NULL || generation == NULL)
    return false;
  const w_seed_accelerated_request0_record *record = &program->requests[0];
  return request_text(program, record->target_offset, record->target_bytes,
                      target) &&
         request_text(program, record->provider_class_offset,
                      record->provider_class_bytes, provider_abi_class) &&
         request_text(program, record->generation_offset,
                      record->generation_bytes, generation);
}

static bool artifact_receipt_valid(
    const w_seed_accelerated_request0_program *request_program,
    const w_seed_accelerated_request0_result *request_result,
    const w_seed_accelerated_provider0_native_artifact *artifact,
    uint8_t native_digest[W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES]) {
  if (request_program == NULL || request_result == NULL || artifact == NULL ||
      artifact->receipt == NULL || artifact->bytes == NULL ||
      artifact->byte_count == 0u ||
      artifact->byte_count >
          W_SEED_ACCELERATED_PROVIDER0_MAX_NATIVE_ARTIFACT_BYTES ||
      !text_valid((accprov0_text){artifact->target.data,
                                  artifact->target.bytes}) ||
      !text_valid((accprov0_text){artifact->provider_abi_class.data,
                                  artifact->provider_abi_class.bytes}) ||
      !w_seed_accelerated_request0_verify(request_program, request_result))
    return false;
  const w_seed_accelerated_request0_record *request =
      &request_program->requests[0];
  accprov0_text request_target;
  accprov0_text request_provider_class;
  accprov0_text request_generation;
  if (!request_identities(request_program, &request_target,
                          &request_provider_class, &request_generation) ||
      !text_equal((accprov0_text){artifact->target.data, artifact->target.bytes},
                  request_target) ||
      !text_equal((accprov0_text){artifact->provider_abi_class.data,
                                  artifact->provider_abi_class.bytes},
                  request_provider_class))
    return false;

  const w_seed_accelerated_provider0_artifact_receipt *receipt =
      artifact->receipt;
  if (memcmp(receipt->schema,
             W_SEED_ACCELERATED_PROVIDER0_ARTIFACT_RECEIPT_SCHEMA_VERSION,
             sizeof(receipt->schema)) != 0 ||
      !digest_nonzero(receipt->request_device_artifact_digest) ||
      !digest_nonzero(receipt->target_digest) ||
      !digest_nonzero(receipt->provider_abi_class_digest) ||
      !digest_nonzero(receipt->native_artifact_digest) ||
      !digest_nonzero(receipt->link_digest) ||
      !digest_equal(receipt->request_device_artifact_digest,
                    request->device_artifact_digest))
    return false;

  uint8_t expected_target_digest[32];
  uint8_t expected_provider_digest[32];
  digest_text((accprov0_text){artifact->target.data, artifact->target.bytes},
              expected_target_digest);
  digest_text((accprov0_text){artifact->provider_abi_class.data,
                              artifact->provider_abi_class.bytes},
              expected_provider_digest);
  digest_bytes(artifact->bytes, artifact->byte_count, native_digest);
  if (!digest_equal(receipt->target_digest, expected_target_digest) ||
      !digest_equal(receipt->provider_abi_class_digest,
                    expected_provider_digest) ||
      !digest_equal(receipt->native_artifact_digest, native_digest))
    return false;
  uint8_t expected_link[32];
  digest_artifact_link(request->device_artifact_digest,
                       expected_target_digest, expected_provider_digest,
                       native_digest, expected_link);
  return digest_equal(receipt->link_digest, expected_link);
}

static bool authority_fields_valid(
    const w_seed_accelerated_provider0_authority *authority) {
  if (authority == NULL || authority->private_seal !=
                               (uintptr_t)&ACCPROV0_AUTHORITY_SEAL ||
      authority->vtable == NULL || authority->vtable->stage == NULL ||
      authority->vtable->submit == NULL || authority->vtable->join == NULL ||
      authority->vtable->cleanup == NULL || authority->generation == 0u ||
      !fixed_text_length(
          authority->implementation_identity,
          sizeof(authority->implementation_identity), &(size_t){0u}))
    return false;
  if (authority->context_bytes != 0u && authority->context == NULL) return false;
  return authority->context_bytes == 0u || authority->context != NULL;
}

bool w_seed_accelerated_provider0_authority_open(
    const w_seed_accelerated_provider0_vtable *vtable, void *context,
    size_t context_bytes, w_seed_accelerated_provider0_text implementation,
    uint32_t generation, w_seed_accelerated_provider0_authority *authority) {
  if (vtable == NULL || vtable->stage == NULL || vtable->submit == NULL ||
      vtable->join == NULL || vtable->cleanup == NULL ||
      !text_valid((accprov0_text){implementation.data, implementation.bytes}) ||
      (context_bytes != 0u && context == NULL) || generation == 0u ||
      authority == NULL ||
      implementation.bytes >= sizeof(authority->implementation_identity))
    return false;
  w_seed_accelerated_provider0_authority candidate;
  (void)memset(&candidate, 0, sizeof(candidate));
  candidate.private_seal = (uintptr_t)&ACCPROV0_AUTHORITY_SEAL;
  candidate.vtable = vtable;
  candidate.context = context;
  candidate.context_bytes = context_bytes;
  candidate.generation = generation;
  if (!copy_fixed(candidate.implementation_identity,
                  sizeof(candidate.implementation_identity),
                  (accprov0_text){implementation.data, implementation.bytes}))
    return false;
  if (!authority_fields_valid(&candidate)) return false;
  *authority = candidate;
  return true;
}

bool w_seed_accelerated_provider0_authority_verify(
    const w_seed_accelerated_provider0_authority *authority) {
  return authority_fields_valid(authority);
}

static bool add_request_ranges(
    const w_seed_accelerated_request0_program *program,
    accprov0_range ranges[ACCPROV0_INPUT_RANGE_CAPACITY], size_t *count) {
  size_t request_bytes = 0u;
  if (program == NULL || ranges == NULL || count == NULL ||
      program->request_capacity > SIZE_MAX / sizeof(*program->requests) ||
      (request_bytes = program->request_capacity * sizeof(*program->requests),
       !add_range(ranges, ACCPROV0_INPUT_RANGE_CAPACITY, count, program,
                  sizeof(*program))) ||
      !add_range(ranges, ACCPROV0_INPUT_RANGE_CAPACITY, count,
                 program->requests, request_bytes) ||
      !add_range(ranges, ACCPROV0_INPUT_RANGE_CAPACITY, count, program->text,
                 program->text_capacity) ||
      !add_range(ranges, ACCPROV0_INPUT_RANGE_CAPACITY, count,
                 program->device_artifact, program->device_artifact_capacity))
    return false;
  return true;
}

static bool input_ranges(
    const w_seed_accelerated_provider0_input *input,
    accprov0_range ranges[ACCPROV0_INPUT_RANGE_CAPACITY], size_t *count) {
  if (input == NULL || ranges == NULL || count == NULL || input->artifact == NULL ||
      input->authority == NULL ||
      !add_range(ranges, ACCPROV0_INPUT_RANGE_CAPACITY, count, input,
                 sizeof(*input)) ||
      !add_request_ranges(input->request_program, ranges, count) ||
      !add_range(ranges, ACCPROV0_INPUT_RANGE_CAPACITY, count,
                 input->request_result, sizeof(*input->request_result)) ||
      !add_range(ranges, ACCPROV0_INPUT_RANGE_CAPACITY, count, input->artifact,
                 sizeof(*input->artifact)) ||
      !add_range(ranges, ACCPROV0_INPUT_RANGE_CAPACITY, count,
                 input->artifact->bytes, input->artifact->byte_count) ||
      !add_range(ranges, ACCPROV0_INPUT_RANGE_CAPACITY, count,
                 input->artifact->receipt, sizeof(*input->artifact->receipt)) ||
      !add_range(ranges, ACCPROV0_INPUT_RANGE_CAPACITY, count,
                 input->authority, sizeof(*input->authority)) ||
      !add_range(ranges, ACCPROV0_INPUT_RANGE_CAPACITY, count,
                 input->authority->vtable, sizeof(*input->authority->vtable)) ||
      !add_range(ranges, ACCPROV0_INPUT_RANGE_CAPACITY, count,
                 input->authority->context, input->authority->context_bytes))
    return false;
  return true;
}

static bool outputs_disjoint_from_input(
    const w_seed_accelerated_provider0_input *input,
    const void *state, size_t state_bytes, const void *outcome,
    size_t outcome_bytes, const void *receipt, size_t receipt_bytes) {
  accprov0_range outputs[ACCPROV0_OUTPUT_RANGE_CAPACITY];
  if (!range_make(state, state_bytes, &outputs[0]) ||
      !range_make(outcome, outcome_bytes, &outputs[1]) ||
      !range_make(receipt, receipt_bytes, &outputs[2]))
    return false;
  for (size_t left = 0u; left < ACCPROV0_OUTPUT_RANGE_CAPACITY; left += 1u)
    for (size_t right = left + 1u; right < ACCPROV0_OUTPUT_RANGE_CAPACITY;
         right += 1u)
      if (ranges_overlap(outputs[left], outputs[right])) return false;
  accprov0_range inputs[ACCPROV0_INPUT_RANGE_CAPACITY];
  size_t input_count = 0u;
  if (!input_ranges(input, inputs, &input_count)) return false;
  for (size_t output_index = 0u; output_index < ACCPROV0_OUTPUT_RANGE_CAPACITY;
       output_index += 1u)
    for (size_t input_index = 0u; input_index < input_count; input_index += 1u)
      if (ranges_overlap(outputs[output_index], inputs[input_index]))
        return false;
  return true;
}

static bool state_is_fresh(const w_seed_accelerated_provider0_state *state) {
  return state != NULL && state->phase == W_SEED_ACCELERATED_PROVIDER0_PHASE_NONE &&
         !state->terminal && !state->stage_called && !state->submit_called &&
         !state->join_called && !state->effect_started &&
         !state->effect_uncertain && !state->cleanup_attempted &&
         !state->cleanup_succeeded && !state->stage_succeeded &&
         state->failure_status == W_SEED_ACCELERATED_PROVIDER0_OK &&
         state->raw_status == 0u && state->result_value == 0 &&
         state->expected_generation == 0u && state->authority == NULL &&
         state->request_program == NULL && state->request_result == NULL &&
         state->artifact == NULL && state->outcome == NULL &&
         state->receipt == NULL;
}

static size_t phase_count_for(w_seed_accelerated_provider0_phase phase) {
  switch (phase) {
    case W_SEED_ACCELERATED_PROVIDER0_PHASE_NONE:
      return 0u;
    case W_SEED_ACCELERATED_PROVIDER0_PHASE_STAGED:
      return 1u;
    case W_SEED_ACCELERATED_PROVIDER0_PHASE_SUBMITTED:
      return 2u;
    case W_SEED_ACCELERATED_PROVIDER0_PHASE_DEVICE_RUNNING:
      return 3u;
    case W_SEED_ACCELERATED_PROVIDER0_PHASE_BODY_SETTLED:
      return 4u;
    case W_SEED_ACCELERATED_PROVIDER0_PHASE_PROVIDER_DRAINED:
      return 5u;
    case W_SEED_ACCELERATED_PROVIDER0_PHASE_CLEANUP:
      return 6u;
    case W_SEED_ACCELERATED_PROVIDER0_PHASE_OUTCOME_COMMITTED:
      return 7u;
    case W_SEED_ACCELERATED_PROVIDER0_PHASE_JOINED:
      return 8u;
  }
  return SIZE_MAX;
}

static bool append_phase(w_seed_accelerated_provider0_state *state,
                         w_seed_accelerated_provider0_receipt *receipt,
                         w_seed_accelerated_provider0_phase phase) {
  if (state == NULL || receipt == NULL || phase ==
      W_SEED_ACCELERATED_PROVIDER0_PHASE_NONE ||
      receipt->phase_count >= W_SEED_ACCELERATED_PROVIDER0_MAX_PHASES ||
      (phase == W_SEED_ACCELERATED_PROVIDER0_PHASE_CLEANUP
           ? receipt->phase_count == 0u || receipt->phase_count >= 6u
           : phase_count_for(phase) != (size_t)receipt->phase_count + 1u))
    return false;
  receipt->phases[receipt->phase_count] = phase;
  receipt->phase_count += 1u;
  state->phase = phase;
  return true;
}

static bool event_shape_valid(
    const w_seed_accelerated_provider0_callback_event *event) {
  if (event == NULL ||
      event->status > W_SEED_ACCELERATED_PROVIDER0_CALLBACK_PROTOCOL_MISMATCH ||
      !fixed_text_optional(event->device, sizeof(event->device)) ||
      !fixed_text_optional(event->queue, sizeof(event->queue)) ||
      !fixed_text_optional(event->generation_text,
                           sizeof(event->generation_text)))
    return false;
  return true;
}

static w_seed_accelerated_provider0_status callback_status(
    const w_seed_accelerated_provider0_callback_event *event,
    bool callback_returned, uint32_t expected_generation) {
  if (!callback_returned || event == NULL)
    return W_SEED_ACCELERATED_PROVIDER0_PROVIDER_FAILURE;
  if (event->generation != 0u && event->generation != expected_generation)
    return W_SEED_ACCELERATED_PROVIDER0_STALE_GENERATION;
  switch (event->status) {
    case W_SEED_ACCELERATED_PROVIDER0_CALLBACK_OK:
      return W_SEED_ACCELERATED_PROVIDER0_OK;
    case W_SEED_ACCELERATED_PROVIDER0_CALLBACK_UNSUPPORTED:
      return W_SEED_ACCELERATED_PROVIDER0_UNSUPPORTED;
    case W_SEED_ACCELERATED_PROVIDER0_CALLBACK_FAILURE:
      return W_SEED_ACCELERATED_PROVIDER0_PROVIDER_FAILURE;
    case W_SEED_ACCELERATED_PROVIDER0_CALLBACK_DEVICE_LOST:
      return W_SEED_ACCELERATED_PROVIDER0_DEVICE_LOST;
    case W_SEED_ACCELERATED_PROVIDER0_CALLBACK_STALE_GENERATION:
      return W_SEED_ACCELERATED_PROVIDER0_STALE_GENERATION;
    case W_SEED_ACCELERATED_PROVIDER0_CALLBACK_PROTOCOL_MISMATCH:
      return W_SEED_ACCELERATED_PROVIDER0_PROTOCOL_MISMATCH;
  }
  return W_SEED_ACCELERATED_PROVIDER0_PROTOCOL_MISMATCH;
}

static void capture_event(
    w_seed_accelerated_provider0_state *state,
    w_seed_accelerated_provider0_receipt *receipt,
    const w_seed_accelerated_provider0_callback_event *event) {
  if (state == NULL || receipt == NULL || event == NULL) return;
  state->raw_status = event->raw_status;
  receipt->raw_status = event->raw_status;
  if (event->device[0] != '\0') {
    (void)memcpy(state->device, event->device, sizeof(state->device));
    (void)memcpy(receipt->device, event->device, sizeof(receipt->device));
  }
  if (event->queue[0] != '\0') {
    (void)memcpy(state->queue, event->queue, sizeof(state->queue));
    (void)memcpy(receipt->queue, event->queue, sizeof(receipt->queue));
  }
  if (event->generation_text[0] != '\0') {
    (void)memcpy(state->generation, event->generation_text,
                 sizeof(state->generation));
    (void)memcpy(receipt->generation, event->generation_text,
                 sizeof(receipt->generation));
  }
}

static void mark_failure(w_seed_accelerated_provider0_state *state,
                         w_seed_accelerated_provider0_receipt *receipt,
                         w_seed_accelerated_provider0_status status,
                         bool uncertain) {
  if (state == NULL || receipt == NULL) return;
  if (state->failure_status == W_SEED_ACCELERATED_PROVIDER0_OK)
    state->failure_status = status;
  state->effect_uncertain = state->effect_uncertain || uncertain;
  receipt->effect_uncertain = state->effect_uncertain;
}

static void digest_outcome(
    const w_seed_accelerated_provider0_outcome *outcome,
    uint8_t digest[W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, (const uint8_t *)ACCPROV0_SEMANTIC_TAG,
                       sizeof(ACCPROV0_SEMANTIC_TAG) - 1u);
  w_seed_sha256_update(&state, outcome->request_semantic_digest, 32u);
  hash_u32(&state, outcome->result_bytes);
  hash_u32(&state, outcome->result_bit_width);
  hash_bool(&state, outcome->result_is_signed);
  hash_bool(&state, outcome->success);
  hash_u32(&state, (uint32_t)outcome->value);
  hash_u32(&state, outcome->phase_count);
  for (size_t index = 0u; index < outcome->phase_count; index += 1u)
    hash_u32(&state, (uint32_t)outcome->phases[index]);
  w_seed_sha256_final(&state, digest);
}

static void digest_receipt(
    const w_seed_accelerated_provider0_receipt *receipt,
    uint8_t digest[W_SEED_ACCELERATED_PROVIDER0_SHA256_BYTES]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, (const uint8_t *)ACCPROV0_PROVENANCE_TAG,
                       sizeof(ACCPROV0_PROVENANCE_TAG) - 1u);
  hash_u32(&state, receipt->phase_count);
  for (size_t index = 0u; index < receipt->phase_count; index += 1u)
    hash_u32(&state, (uint32_t)receipt->phases[index]);
  hash_fixed_text(&state, receipt->provider_abi_class,
                  sizeof(receipt->provider_abi_class));
  hash_fixed_text(&state, receipt->target, sizeof(receipt->target));
  hash_fixed_text(&state, receipt->implementation_identity,
                  sizeof(receipt->implementation_identity));
  hash_fixed_text(&state, receipt->device, sizeof(receipt->device));
  hash_fixed_text(&state, receipt->queue, sizeof(receipt->queue));
  hash_fixed_text(&state, receipt->generation, sizeof(receipt->generation));
  w_seed_sha256_update(&state, receipt->request_semantic_digest, 32u);
  w_seed_sha256_update(&state, receipt->request_device_artifact_digest, 32u);
  w_seed_sha256_update(&state, receipt->native_artifact_digest, 32u);
  hash_u32(&state, receipt->raw_status);
  hash_bool(&state, receipt->stage_called);
  hash_bool(&state, receipt->submit_called);
  hash_bool(&state, receipt->join_called);
  hash_bool(&state, receipt->effect_uncertain);
  hash_bool(&state, receipt->body_settled);
  hash_bool(&state, receipt->provider_drained);
  hash_bool(&state, receipt->cleanup_attempted);
  hash_bool(&state, receipt->cleanup_succeeded);
  w_seed_sha256_final(&state, digest);
}

static void seal_receipt(w_seed_accelerated_provider0_receipt *receipt) {
  if (receipt == NULL) return;
  digest_receipt(receipt, receipt->provenance_digest);
}

static void initialize_receipt(
    const w_seed_accelerated_request0_program *request_program,
    const w_seed_accelerated_request0_result *request_result,
    const w_seed_accelerated_provider0_native_artifact *artifact,
    const w_seed_accelerated_provider0_authority *authority,
    const uint8_t native_digest[32],
    w_seed_accelerated_provider0_receipt *receipt) {
  (void)memset(receipt, 0, sizeof(*receipt));
  (void)memcpy(receipt->schema, W_SEED_ACCELERATED_PROVIDER0_SCHEMA_VERSION,
               sizeof(receipt->schema));
  const w_seed_accelerated_request0_record *request =
      &request_program->requests[0];
  (void)memcpy(receipt->request_semantic_digest,
               request_result->semantic_digest, 32u);
  (void)memcpy(receipt->request_device_artifact_digest,
               request->device_artifact_digest, 32u);
  (void)memcpy(receipt->native_artifact_digest, native_digest, 32u);
  (void)memcpy(receipt->provider_abi_class, artifact->provider_abi_class.data,
               artifact->provider_abi_class.bytes);
  (void)memcpy(receipt->target, artifact->target.data, artifact->target.bytes);
  (void)memcpy(receipt->implementation_identity,
               authority->implementation_identity,
               sizeof(receipt->implementation_identity));
  accprov0_text generation;
  if (request_identities(request_program, &(accprov0_text){0},
                         &(accprov0_text){0}, &generation))
    (void)memcpy(receipt->generation, generation.data, generation.bytes);
}

static bool receipt_matches_state(
    const w_seed_accelerated_provider0_state *state,
    const w_seed_accelerated_provider0_receipt *receipt) {
  if (state == NULL || receipt == NULL ||
      memcmp(receipt->schema, W_SEED_ACCELERATED_PROVIDER0_SCHEMA_VERSION,
             sizeof(receipt->schema)) != 0 ||
      receipt->phase_count != phase_count_for(state->phase) ||
      receipt->phase_count > W_SEED_ACCELERATED_PROVIDER0_MAX_PHASES ||
      (receipt->phase_count != 0u &&
       receipt->phases[receipt->phase_count - 1u] != state->phase) ||
      !digest_equal(receipt->request_semantic_digest,
                    state->request_semantic_digest) ||
      !digest_equal(receipt->request_device_artifact_digest,
                    state->request_device_artifact_digest) ||
      !digest_equal(receipt->native_artifact_digest,
                    state->native_artifact_digest))
    return false;
  return true;
}

static bool state_input(const w_seed_accelerated_provider0_state *state,
                        w_seed_accelerated_provider0_input *input) {
  if (state == NULL || input == NULL || state->authority == NULL ||
      state->request_program == NULL || state->request_result == NULL ||
      state->artifact == NULL)
    return false;
  *input = (w_seed_accelerated_provider0_input){
      state->request_program, state->request_result, state->artifact,
      state->authority};
  return true;
}

static bool state_outputs_match(
    const w_seed_accelerated_provider0_state *state,
    const w_seed_accelerated_provider0_outcome *outcome,
    const w_seed_accelerated_provider0_receipt *receipt) {
  return state != NULL && state->outcome == outcome && state->receipt == receipt;
}

static bool state_outputs_disjoint(
    const w_seed_accelerated_provider0_state *state, const void *outcome,
    size_t outcome_bytes, const void *receipt, size_t receipt_bytes) {
  w_seed_accelerated_provider0_input input;
  if (!state_input(state, &input)) return false;
  return outputs_disjoint_from_input(&input, state, sizeof(*state), outcome,
                                     outcome_bytes, receipt, receipt_bytes);
}

static bool state_receipt_text_valid(
    const w_seed_accelerated_provider0_receipt *receipt) {
  return receipt != NULL && fixed_text_optional(receipt->provider_abi_class,
                                                 sizeof(receipt->provider_abi_class)) &&
         fixed_text_optional(receipt->target, sizeof(receipt->target)) &&
         fixed_text_optional(receipt->implementation_identity,
                             sizeof(receipt->implementation_identity)) &&
         fixed_text_optional(receipt->device, sizeof(receipt->device)) &&
         fixed_text_optional(receipt->queue, sizeof(receipt->queue)) &&
         fixed_text_optional(receipt->generation, sizeof(receipt->generation));
}

static bool receipt_phase_shape_valid(
    const w_seed_accelerated_provider0_receipt *receipt) {
  if (receipt == NULL ||
      receipt->phase_count > W_SEED_ACCELERATED_PROVIDER0_MAX_PHASES ||
      (receipt->cleanup_succeeded && !receipt->cleanup_attempted) ||
      (receipt->provider_drained && !receipt->body_settled) ||
      (receipt->submit_called && !receipt->stage_called) ||
      (receipt->join_called && !receipt->submit_called))
    return false;
  static const w_seed_accelerated_provider0_phase phases[] = {
      W_SEED_ACCELERATED_PROVIDER0_PHASE_STAGED,
      W_SEED_ACCELERATED_PROVIDER0_PHASE_SUBMITTED,
      W_SEED_ACCELERATED_PROVIDER0_PHASE_DEVICE_RUNNING,
      W_SEED_ACCELERATED_PROVIDER0_PHASE_BODY_SETTLED,
      W_SEED_ACCELERATED_PROVIDER0_PHASE_PROVIDER_DRAINED,
      W_SEED_ACCELERATED_PROVIDER0_PHASE_CLEANUP,
      W_SEED_ACCELERATED_PROVIDER0_PHASE_OUTCOME_COMMITTED,
      W_SEED_ACCELERATED_PROVIDER0_PHASE_JOINED,
  };
  size_t cleanup_index = SIZE_MAX;
  for (size_t index = 0u; index < receipt->phase_count; index += 1u) {
    if (receipt->phases[index] == W_SEED_ACCELERATED_PROVIDER0_PHASE_CLEANUP) {
      if (cleanup_index != SIZE_MAX || index == 0u) return false;
      cleanup_index = index;
      continue;
    }
    if (receipt->phases[index] != phases[index]) return false;
  }
  if (cleanup_index != SIZE_MAX && cleanup_index + 1u != receipt->phase_count &&
      receipt->phase_count != W_SEED_ACCELERATED_PROVIDER0_MAX_PHASES)
    return false;
  if (receipt->phase_count == W_SEED_ACCELERATED_PROVIDER0_MAX_PHASES) {
    if (memcmp(receipt->phases, phases, sizeof(phases)) != 0 ||
        !receipt->stage_called || !receipt->submit_called ||
        !receipt->join_called || receipt->effect_uncertain ||
        !receipt->body_settled || !receipt->provider_drained ||
        !receipt->cleanup_attempted || !receipt->cleanup_succeeded)
      return false;
  }
  return true;
}

static w_seed_accelerated_provider0_status begin_validation(
    const w_seed_accelerated_provider0_input *input,
    const w_seed_accelerated_provider0_state *state,
    const w_seed_accelerated_provider0_outcome *outcome,
    const w_seed_accelerated_provider0_receipt *receipt) {
  if (input == NULL || state == NULL || !state_is_fresh(state) ||
      input->request_program == NULL || input->request_result == NULL ||
      input->artifact == NULL || input->authority == NULL || outcome == NULL ||
      receipt == NULL)
    return W_SEED_ACCELERATED_PROVIDER0_INVALID_ARGUMENT;
  /* This is the complete writable-range preflight. It must happen before the
   * stage callback and the state captures these exact addresses below. */
  if (!outputs_disjoint_from_input(input, state, sizeof(*state), outcome,
                                   sizeof(*outcome), receipt, sizeof(*receipt)))
    return W_SEED_ACCELERATED_PROVIDER0_ALIAS;
  if (!w_seed_accelerated_provider0_authority_verify(input->authority))
    return W_SEED_ACCELERATED_PROVIDER0_INVALID_AUTHORITY;
  if (!w_seed_accelerated_request0_verify(input->request_program,
                                          input->request_result))
    return W_SEED_ACCELERATED_PROVIDER0_INVALID_REQUEST;
  if (input->artifact->byte_count >
      W_SEED_ACCELERATED_PROVIDER0_MAX_NATIVE_ARTIFACT_BYTES)
    return W_SEED_ACCELERATED_PROVIDER0_CAPACITY;
  uint8_t native_digest[32];
  if (!artifact_receipt_valid(input->request_program, input->request_result,
                              input->artifact, native_digest))
    return W_SEED_ACCELERATED_PROVIDER0_INVALID_ARTIFACT;
  (void)native_digest;
  return W_SEED_ACCELERATED_PROVIDER0_OK;
}

w_seed_accelerated_provider0_status w_seed_accelerated_provider0_begin(
    const w_seed_accelerated_provider0_input *input,
    w_seed_accelerated_provider0_state *state,
    w_seed_accelerated_provider0_outcome *outcome,
    w_seed_accelerated_provider0_receipt *receipt) {
  const w_seed_accelerated_provider0_status validation =
      begin_validation(input, state, outcome, receipt);
  if (validation != W_SEED_ACCELERATED_PROVIDER0_OK)
    return validation;

  uint8_t native_digest[32];
  if (!artifact_receipt_valid(input->request_program, input->request_result,
                              input->artifact, native_digest))
    return W_SEED_ACCELERATED_PROVIDER0_INVALID_ARTIFACT;
  w_seed_accelerated_provider0_state state_candidate;
  (void)memset(&state_candidate, 0, sizeof(state_candidate));
  state_candidate.stage_called = true;
  state_candidate.expected_generation = input->authority->generation;
  state_candidate.authority = input->authority;
  state_candidate.request_program = input->request_program;
  state_candidate.request_result = input->request_result;
  state_candidate.artifact = input->artifact;
  state_candidate.outcome = outcome;
  state_candidate.receipt = receipt;
  (void)memcpy(state_candidate.request_semantic_digest,
               input->request_result->semantic_digest, 32u);
  (void)memcpy(state_candidate.request_device_artifact_digest,
               input->request_program->requests[0].device_artifact_digest, 32u);
  (void)memcpy(state_candidate.native_artifact_digest, native_digest, 32u);
  accprov0_text request_generation;
  accprov0_text ignored_target;
  accprov0_text ignored_provider;
  if (!request_identities(input->request_program, &ignored_target,
                          &ignored_provider, &request_generation) ||
      request_generation.bytes >= sizeof(state_candidate.generation))
    return W_SEED_ACCELERATED_PROVIDER0_INVALID_REQUEST;
  (void)memcpy(state_candidate.generation, request_generation.data,
               request_generation.bytes);

  w_seed_accelerated_provider0_receipt receipt_candidate;
  initialize_receipt(input->request_program, input->request_result,
                     input->artifact, input->authority, native_digest,
                     &receipt_candidate);
  w_seed_accelerated_provider0_callback_event event;
  (void)memset(&event, 0, sizeof(event));
  const bool callback_returned = input->authority->vtable->stage(
      input->authority->context, input->request_program, input->request_result,
      input->artifact, &event);
  const bool event_shape = event_shape_valid(&event);
  if (!event_shape) {
    event.status = W_SEED_ACCELERATED_PROVIDER0_CALLBACK_PROTOCOL_MISMATCH;
    event.raw_status = 0u;
  }
  capture_event(&state_candidate, &receipt_candidate, &event);
  receipt_candidate.stage_called = true;
  const w_seed_accelerated_provider0_status status =
      event_shape ? callback_status(&event, callback_returned,
                                     input->authority->generation)
                  : W_SEED_ACCELERATED_PROVIDER0_PROTOCOL_MISMATCH;
  if (status != W_SEED_ACCELERATED_PROVIDER0_OK) {
    /* A failed or malformed stage callback may have performed an effect even
     * when its event says otherwise. Treat every attempted callback as
     * cleanup-requiring; only preflight failures skip cleanup entirely. */
    state_candidate.effect_started = true;
    state_candidate.effect_uncertain = true;
    state_candidate.failure_status = status;
    receipt_candidate.effect_uncertain = state_candidate.effect_uncertain;
    receipt_candidate.provider_drained = false;
  } else {
    state_candidate.stage_succeeded = true;
    state_candidate.effect_started = true;
    receipt_candidate.stage_called = true;
    if (!append_phase(&state_candidate, &receipt_candidate,
                      W_SEED_ACCELERATED_PROVIDER0_PHASE_STAGED))
      return W_SEED_ACCELERATED_PROVIDER0_PROTOCOL_MISMATCH;
  }
  seal_receipt(&receipt_candidate);
  *state = state_candidate;
  *receipt = receipt_candidate;
  return status;
}

w_seed_accelerated_provider0_status w_seed_accelerated_provider0_submit(
    w_seed_accelerated_provider0_state *state,
    w_seed_accelerated_provider0_receipt *receipt) {
  if (state == NULL || receipt == NULL || state->terminal ||
      !state->stage_succeeded || state->failure_status !=
                                     W_SEED_ACCELERATED_PROVIDER0_OK ||
      state->phase != W_SEED_ACCELERATED_PROVIDER0_PHASE_STAGED ||
      state->receipt != receipt ||
      !receipt_matches_state(state, receipt) || !state_receipt_text_valid(receipt))
    return W_SEED_ACCELERATED_PROVIDER0_INVALID_STATE;
  if (!state_outputs_disjoint(state, NULL, 0u, receipt, sizeof(*receipt)))
    return W_SEED_ACCELERATED_PROVIDER0_ALIAS;

  w_seed_accelerated_provider0_callback_event event;
  (void)memset(&event, 0, sizeof(event));
  const bool callback_returned = state->authority->vtable->submit(
      state->authority->context, &event);
  const bool event_shape = event_shape_valid(&event);
  if (!event_shape) event.status =
      W_SEED_ACCELERATED_PROVIDER0_CALLBACK_PROTOCOL_MISMATCH;
  w_seed_accelerated_provider0_receipt receipt_candidate = *receipt;
  capture_event(state, &receipt_candidate, &event);
  state->submit_called = true;
  receipt_candidate.submit_called = true;
  state->effect_started = true;
  const w_seed_accelerated_provider0_status status =
      event_shape ? callback_status(&event, callback_returned,
                                     state->expected_generation)
                  : W_SEED_ACCELERATED_PROVIDER0_PROTOCOL_MISMATCH;
  if (status != W_SEED_ACCELERATED_PROVIDER0_OK) {
    mark_failure(state, &receipt_candidate, status, true);
  } else if (event.body_settled || event.provider_drained ||
             !append_phase(state, &receipt_candidate,
                           W_SEED_ACCELERATED_PROVIDER0_PHASE_SUBMITTED) ||
             !append_phase(state, &receipt_candidate,
                           W_SEED_ACCELERATED_PROVIDER0_PHASE_DEVICE_RUNNING)) {
    mark_failure(state, &receipt_candidate,
                 W_SEED_ACCELERATED_PROVIDER0_PROTOCOL_MISMATCH, true);
  }
  receipt_candidate.effect_uncertain = state->effect_uncertain;
  seal_receipt(&receipt_candidate);
  *receipt = receipt_candidate;
  return status == W_SEED_ACCELERATED_PROVIDER0_OK &&
                 state->failure_status != W_SEED_ACCELERATED_PROVIDER0_OK
             ? state->failure_status
             : status;
}

w_seed_accelerated_provider0_status w_seed_accelerated_provider0_join(
    w_seed_accelerated_provider0_state *state,
    w_seed_accelerated_provider0_receipt *receipt) {
  if (state == NULL || receipt == NULL || state->terminal ||
      state->failure_status != W_SEED_ACCELERATED_PROVIDER0_OK ||
      state->phase != W_SEED_ACCELERATED_PROVIDER0_PHASE_DEVICE_RUNNING ||
      !state->submit_called || state->receipt != receipt ||
      !receipt_matches_state(state, receipt) ||
      !state_receipt_text_valid(receipt))
    return W_SEED_ACCELERATED_PROVIDER0_INVALID_STATE;
  if (!state_outputs_disjoint(state, NULL, 0u, receipt, sizeof(*receipt)))
    return W_SEED_ACCELERATED_PROVIDER0_ALIAS;

  w_seed_accelerated_provider0_callback_event event;
  (void)memset(&event, 0, sizeof(event));
  const bool callback_returned = state->authority->vtable->join(
      state->authority->context, &event);
  const bool event_shape = event_shape_valid(&event);
  if (!event_shape) event.status =
      W_SEED_ACCELERATED_PROVIDER0_CALLBACK_PROTOCOL_MISMATCH;
  w_seed_accelerated_provider0_receipt receipt_candidate = *receipt;
  capture_event(state, &receipt_candidate, &event);
  state->join_called = true;
  receipt_candidate.join_called = true;
  const w_seed_accelerated_provider0_status status =
      event_shape ? callback_status(&event, callback_returned,
                                     state->expected_generation)
                  : W_SEED_ACCELERATED_PROVIDER0_PROTOCOL_MISMATCH;
  if (status != W_SEED_ACCELERATED_PROVIDER0_OK) {
    mark_failure(state, &receipt_candidate, status, true);
  } else if (!event.body_settled || !event.provider_drained) {
    mark_failure(state, &receipt_candidate,
                 W_SEED_ACCELERATED_PROVIDER0_PROTOCOL_MISMATCH, true);
  } else if (event.result_value !=
             state->request_program->requests[0].sentinel_expected_i32) {
    mark_failure(state, &receipt_candidate,
                 W_SEED_ACCELERATED_PROVIDER0_RESULT_MISMATCH, true);
  } else if (!append_phase(state, &receipt_candidate,
                           W_SEED_ACCELERATED_PROVIDER0_PHASE_BODY_SETTLED) ||
             !append_phase(state, &receipt_candidate,
                           W_SEED_ACCELERATED_PROVIDER0_PHASE_PROVIDER_DRAINED)) {
    mark_failure(state, &receipt_candidate,
                 W_SEED_ACCELERATED_PROVIDER0_PROTOCOL_MISMATCH, true);
  } else {
    state->result_value = event.result_value;
    receipt_candidate.body_settled = true;
    receipt_candidate.provider_drained = true;
  }
  receipt_candidate.effect_uncertain = state->effect_uncertain;
  seal_receipt(&receipt_candidate);
  *receipt = receipt_candidate;
  return status == W_SEED_ACCELERATED_PROVIDER0_OK &&
                 state->failure_status != W_SEED_ACCELERATED_PROVIDER0_OK
             ? state->failure_status
             : status;
}

static w_seed_accelerated_provider0_status cleanup_status(
    w_seed_accelerated_provider0_state *state,
    w_seed_accelerated_provider0_receipt *receipt) {
  if (state == NULL || receipt == NULL || state->authority == NULL) return
      W_SEED_ACCELERATED_PROVIDER0_INVALID_STATE;
  w_seed_accelerated_provider0_callback_event event;
  (void)memset(&event, 0, sizeof(event));
  const bool callback_returned = state->effect_started &&
      state->authority->vtable->cleanup(state->authority->context, &event);
  const bool event_shape = event_shape_valid(&event);
  if (!event_shape) event.status =
      W_SEED_ACCELERATED_PROVIDER0_CALLBACK_PROTOCOL_MISMATCH;
  capture_event(state, receipt, &event);
  receipt->cleanup_attempted = true;
  state->cleanup_attempted = true;
  if (!state->effect_started) {
    state->cleanup_succeeded = true;
    receipt->cleanup_succeeded = true;
    return W_SEED_ACCELERATED_PROVIDER0_OK;
  }
  const w_seed_accelerated_provider0_status status =
      event_shape ? callback_status(&event, callback_returned,
                                     state->expected_generation)
                  : W_SEED_ACCELERATED_PROVIDER0_PROTOCOL_MISMATCH;
  if (status != W_SEED_ACCELERATED_PROVIDER0_OK ||
      !event.cleanup_succeeded) {
    state->cleanup_succeeded = false;
    state->effect_uncertain = true;
    receipt->cleanup_succeeded = false;
    receipt->effect_uncertain = true;
    return W_SEED_ACCELERATED_PROVIDER0_CLEANUP_UNCERTAIN;
  }
  state->cleanup_succeeded = true;
  receipt->cleanup_succeeded = true;
  return W_SEED_ACCELERATED_PROVIDER0_OK;
}

w_seed_accelerated_provider0_status w_seed_accelerated_provider0_destroy(
    w_seed_accelerated_provider0_state *state,
    w_seed_accelerated_provider0_outcome *outcome,
    w_seed_accelerated_provider0_receipt *receipt) {
  if (state == NULL || receipt == NULL || state->terminal ||
      outcome == NULL || state->cleanup_attempted ||
      !state_outputs_match(state, outcome, receipt) ||
      !receipt_matches_state(state, receipt))
    return W_SEED_ACCELERATED_PROVIDER0_INVALID_STATE;
  const bool can_commit = state->failure_status ==
                              W_SEED_ACCELERATED_PROVIDER0_OK &&
                          state->phase ==
                              W_SEED_ACCELERATED_PROVIDER0_PHASE_PROVIDER_DRAINED;
  w_seed_accelerated_provider0_receipt receipt_candidate = *receipt;
  const w_seed_accelerated_provider0_status cleanup =
      cleanup_status(state, &receipt_candidate);
  if (state->phase != W_SEED_ACCELERATED_PROVIDER0_PHASE_NONE &&
      !append_phase(state, &receipt_candidate,
                    W_SEED_ACCELERATED_PROVIDER0_PHASE_CLEANUP)) {
    state->failure_status = W_SEED_ACCELERATED_PROVIDER0_PROTOCOL_MISMATCH;
    state->effect_uncertain = true;
  }
  if (cleanup != W_SEED_ACCELERATED_PROVIDER0_OK) {
    mark_failure(state, &receipt_candidate, cleanup, true);
    state->terminal = true;
    seal_receipt(&receipt_candidate);
    *receipt = receipt_candidate;
    return cleanup;
  }
  if (state->failure_status != W_SEED_ACCELERATED_PROVIDER0_OK || !can_commit) {
    state->terminal = true;
    seal_receipt(&receipt_candidate);
    *receipt = receipt_candidate;
    return state->failure_status == W_SEED_ACCELERATED_PROVIDER0_OK
               ? W_SEED_ACCELERATED_PROVIDER0_INVALID_STATE
               : state->failure_status;
  }

  if (!append_phase(state, &receipt_candidate,
                    W_SEED_ACCELERATED_PROVIDER0_PHASE_OUTCOME_COMMITTED) ||
      !append_phase(state, &receipt_candidate,
                    W_SEED_ACCELERATED_PROVIDER0_PHASE_JOINED)) {
    state->terminal = true;
    state->failure_status = W_SEED_ACCELERATED_PROVIDER0_PROTOCOL_MISMATCH;
    state->effect_uncertain = true;
    seal_receipt(&receipt_candidate);
    *receipt = receipt_candidate;
    return W_SEED_ACCELERATED_PROVIDER0_PROTOCOL_MISMATCH;
  }
  w_seed_accelerated_provider0_outcome outcome_candidate;
  (void)memset(&outcome_candidate, 0, sizeof(outcome_candidate));
  (void)memcpy(outcome_candidate.schema,
               W_SEED_ACCELERATED_PROVIDER0_SCHEMA_VERSION,
               sizeof(outcome_candidate.schema));
  outcome_candidate.result_bytes = sizeof(int32_t);
  outcome_candidate.result_bit_width = 32u;
  outcome_candidate.result_is_signed = true;
  outcome_candidate.success = true;
  outcome_candidate.value = state->result_value;
  outcome_candidate.phase_count = W_SEED_ACCELERATED_PROVIDER0_MAX_PHASES;
  for (size_t index = 0u; index < W_SEED_ACCELERATED_PROVIDER0_MAX_PHASES;
       index += 1u)
    outcome_candidate.phases[index] = receipt_candidate.phases[index];
  (void)memcpy(outcome_candidate.request_semantic_digest,
               state->request_semantic_digest, 32u);
  digest_outcome(&outcome_candidate, outcome_candidate.semantic_digest);
  state->terminal = true;
  receipt_candidate.body_settled = true;
  receipt_candidate.provider_drained = true;
  seal_receipt(&receipt_candidate);
  *outcome = outcome_candidate;
  *receipt = receipt_candidate;
  return W_SEED_ACCELERATED_PROVIDER0_OK;
}

w_seed_accelerated_provider0_status w_seed_accelerated_provider0_run(
    const w_seed_accelerated_provider0_input *input,
    w_seed_accelerated_provider0_state *state,
    w_seed_accelerated_provider0_outcome *outcome,
    w_seed_accelerated_provider0_receipt *receipt) {
  if (input == NULL || state == NULL || outcome == NULL || receipt == NULL)
    return W_SEED_ACCELERATED_PROVIDER0_INVALID_ARGUMENT;
  if (!outputs_disjoint_from_input(input, state, sizeof(*state), outcome,
                                   sizeof(*outcome), receipt, sizeof(*receipt)))
    return W_SEED_ACCELERATED_PROVIDER0_ALIAS;
  w_seed_accelerated_provider0_status status =
      w_seed_accelerated_provider0_begin(input, state, outcome, receipt);
  if (status != W_SEED_ACCELERATED_PROVIDER0_OK) {
    if (!state->stage_called) return status;
    const w_seed_accelerated_provider0_status cleanup =
        w_seed_accelerated_provider0_destroy(state, outcome, receipt);
    return cleanup == W_SEED_ACCELERATED_PROVIDER0_OK ? status : cleanup;
  }
  status = w_seed_accelerated_provider0_submit(state, receipt);
  if (status != W_SEED_ACCELERATED_PROVIDER0_OK) {
    const w_seed_accelerated_provider0_status cleanup =
        w_seed_accelerated_provider0_destroy(state, outcome, receipt);
    return cleanup == W_SEED_ACCELERATED_PROVIDER0_OK ? status : cleanup;
  }
  status = w_seed_accelerated_provider0_join(state, receipt);
  if (status != W_SEED_ACCELERATED_PROVIDER0_OK) {
    const w_seed_accelerated_provider0_status cleanup =
        w_seed_accelerated_provider0_destroy(state, outcome, receipt);
    return cleanup == W_SEED_ACCELERATED_PROVIDER0_OK ? status : cleanup;
  }
  return w_seed_accelerated_provider0_destroy(state, outcome, receipt);
}

bool w_seed_accelerated_provider0_verify_outcome(
    const w_seed_accelerated_provider0_input *input,
    const w_seed_accelerated_provider0_outcome *outcome) {
  if (input == NULL || outcome == NULL ||
      !w_seed_accelerated_request0_verify(input->request_program,
                                          input->request_result) ||
      memcmp(outcome->schema, W_SEED_ACCELERATED_PROVIDER0_SCHEMA_VERSION,
             sizeof(outcome->schema)) != 0 || !outcome->success ||
      outcome->result_bytes != sizeof(int32_t) ||
      outcome->result_bit_width != 32u || !outcome->result_is_signed ||
      outcome->phase_count != W_SEED_ACCELERATED_PROVIDER0_MAX_PHASES ||
      !digest_equal(outcome->request_semantic_digest,
                    input->request_result->semantic_digest))
    return false;
  static const w_seed_accelerated_provider0_phase phases[] = {
      W_SEED_ACCELERATED_PROVIDER0_PHASE_STAGED,
      W_SEED_ACCELERATED_PROVIDER0_PHASE_SUBMITTED,
      W_SEED_ACCELERATED_PROVIDER0_PHASE_DEVICE_RUNNING,
      W_SEED_ACCELERATED_PROVIDER0_PHASE_BODY_SETTLED,
      W_SEED_ACCELERATED_PROVIDER0_PHASE_PROVIDER_DRAINED,
      W_SEED_ACCELERATED_PROVIDER0_PHASE_CLEANUP,
      W_SEED_ACCELERATED_PROVIDER0_PHASE_OUTCOME_COMMITTED,
      W_SEED_ACCELERATED_PROVIDER0_PHASE_JOINED,
  };
  if (memcmp(outcome->phases, phases, sizeof(phases)) != 0) return false;
  uint8_t expected[32];
  digest_outcome(outcome, expected);
  return digest_equal(outcome->semantic_digest, expected);
}

bool w_seed_accelerated_provider0_verify_receipt(
    const w_seed_accelerated_provider0_input *input,
    const w_seed_accelerated_provider0_receipt *receipt) {
  if (input == NULL || receipt == NULL ||
      !w_seed_accelerated_provider0_authority_verify(input->authority) ||
      !w_seed_accelerated_request0_verify(input->request_program,
                                          input->request_result) ||
      !state_receipt_text_valid(receipt) ||
      !receipt_phase_shape_valid(receipt) ||
      memcmp(receipt->schema, W_SEED_ACCELERATED_PROVIDER0_SCHEMA_VERSION,
             sizeof(receipt->schema)) != 0 ||
      !digest_equal(receipt->request_semantic_digest,
                    input->request_result->semantic_digest) ||
      !digest_equal(receipt->request_device_artifact_digest,
                    input->request_program->requests[0].device_artifact_digest))
    return false;
  uint8_t native_digest[32];
  if (!artifact_receipt_valid(input->request_program, input->request_result,
                              input->artifact, native_digest) ||
      !digest_equal(receipt->native_artifact_digest, native_digest) ||
      !text_equal((accprov0_text){receipt->target, strlen(receipt->target)},
                  (accprov0_text){input->artifact->target.data,
                                  input->artifact->target.bytes}) ||
      !text_equal((accprov0_text){receipt->provider_abi_class,
                                  strlen(receipt->provider_abi_class)},
                  (accprov0_text){input->artifact->provider_abi_class.data,
                                  input->artifact->provider_abi_class.bytes}) ||
      !text_equal((accprov0_text){receipt->implementation_identity,
                                  strlen(receipt->implementation_identity)},
                  (accprov0_text){input->authority->implementation_identity,
                                  strlen(input->authority->implementation_identity)}))
    return false;
  uint8_t expected[32];
  digest_receipt(receipt, expected);
  return digest_equal(receipt->provenance_digest, expected);
}
