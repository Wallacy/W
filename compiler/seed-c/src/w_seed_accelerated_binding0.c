#include "w_seed_accelerated_binding0.h"

#include <limits.h>
#include <string.h>

#include "w_seed_sha256.h"
#include "w_seed_source.h"

static const char ACCBIND0_SEMANTIC_TAG[] =
    "w-seed-accelerated-binding0-semantic-1";
static const char ACCBIND0_PROVENANCE_TAG[] =
    "w-seed-accelerated-binding0-provenance-1";

enum { ACCBIND0_TEXT_FIELDS = 15, ACCBIND0_MAX_RANGES = 40 };

typedef struct {
  uintptr_t begin;
  uintptr_t end;
  bool active;
} accbind0_range;

typedef struct {
  const uint8_t *data;
  size_t bytes;
} accbind0_text;

typedef struct {
  w_seed_accelerated_binding0_counts counts;
  w_seed_accelerated_binding0_record relation;
  accbind0_text text[ACCBIND0_TEXT_FIELDS];
  uint8_t semantic_digest[W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];
  uint8_t provenance_digest[W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];
} accbind0_scan;

static bool size_add(size_t left, size_t right, size_t *result) {
  if (result == NULL || right > SIZE_MAX - left) return false;
  *result = left + right;
  return true;
}

static bool size_multiply(size_t count, size_t width, size_t *result) {
  if (result == NULL || (width != 0u && count > SIZE_MAX / width)) return false;
  *result = count * width;
  return true;
}

static bool range_make(const void *pointer, size_t bytes, accbind0_range *range) {
  if (range == NULL || (bytes != 0u && pointer == NULL)) return false;
  *range = (accbind0_range){0u, 0u, false};
  if (bytes == 0u) return true;
  const uintptr_t begin = (uintptr_t)pointer;
  if (bytes > UINTPTR_MAX - begin) return false;
  range->begin = begin;
  range->end = begin + bytes;
  range->active = true;
  return true;
}

static bool ranges_overlap(accbind0_range left, accbind0_range right) {
  return left.active && right.active && left.begin < right.end &&
         right.begin < left.end;
}

static bool add_range(accbind0_range *ranges, size_t capacity, size_t *count,
                      const void *pointer, size_t bytes) {
  if (ranges == NULL || count == NULL || *count >= capacity) return false;
  if (!range_make(pointer, bytes, &ranges[*count])) return false;
  *count += 1u;
  return true;
}

static bool text_valid(w_seed_accelerated_binding0_text text) {
  if (text.data == NULL || text.bytes == 0u ||
      memchr(text.data, '\0', text.bytes) != NULL)
    return false;
  w_seed_source source;
  w_seed_source_error error;
  return w_seed_source_init(
      (w_seed_byte_view){(const uint8_t *)text.data, text.bytes}, &source,
      &error);
}

static bool text_equal(w_seed_accelerated_binding0_text left,
                       w_seed_accelerated_binding0_text right) {
  return text_valid(left) && text_valid(right) && left.bytes == right.bytes &&
         memcmp(left.data, right.data, left.bytes) == 0;
}

static bool text_equal_bytes(w_seed_accelerated_binding0_text left,
                             const uint8_t *right, size_t right_bytes) {
  return text_valid(left) && right != NULL && left.bytes == right_bytes &&
         memcmp(left.data, right, right_bytes) == 0;
}

static bool digest_equal(const uint8_t *left, const uint8_t *right) {
  return left != NULL && right != NULL &&
         memcmp(left, right, W_SEED_ACCELERATED_BINDING0_SHA256_BYTES) == 0;
}

static bool digest_nonzero(const uint8_t *digest) {
  if (digest == NULL) return false;
  uint8_t bits = 0u;
  for (size_t index = 0u;
       index < W_SEED_ACCELERATED_BINDING0_SHA256_BYTES; index += 1u)
    bits = (uint8_t)(bits | digest[index]);
  return bits != 0u;
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

static void hash_text(w_seed_sha256_state *state, const uint8_t *data,
                      size_t bytes) {
  hash_u64(state, (uint64_t)bytes);
  if (bytes != 0u) w_seed_sha256_update(state, data, bytes);
}

static void hash_limits(w_seed_sha256_state *state,
                        w_seed_accelerated_binding0_limits limits) {
  hash_u64(state, limits.maximum_in_flight);
  hash_u64(state, limits.maximum_command_bytes);
  hash_u64(state, limits.maximum_argument_bytes);
  hash_u64(state, limits.maximum_result_bytes);
  hash_u64(state, limits.maximum_dependency_edges);
  hash_u64(state, limits.maximum_retained_device_bytes);
  hash_u64(state, limits.maximum_completion_records);
  hash_u64(state, limits.maximum_cleanup_steps);
}

static bool limits_valid(w_seed_accelerated_binding0_limits limits) {
  return limits.maximum_in_flight != 0u &&
         limits.maximum_command_bytes != 0u &&
         limits.maximum_argument_bytes != 0u &&
         limits.maximum_result_bytes != 0u &&
         limits.maximum_dependency_edges != 0u &&
         limits.maximum_retained_device_bytes != 0u &&
         limits.maximum_completion_records != 0u &&
         limits.maximum_cleanup_steps != 0u;
}

static uint64_t minimum5(uint64_t first, uint64_t second, uint64_t third,
                         uint64_t fourth, uint64_t fifth) {
  uint64_t value = first;
  if (second < value) value = second;
  if (third < value) value = third;
  if (fourth < value) value = fourth;
  if (fifth < value) value = fifth;
  return value;
}

static bool accinv_text(const w_seed_accelerated_invocation0_program *program,
                        size_t offset, size_t bytes, accbind0_text *text) {
  if (program == NULL || text == NULL || offset > program->text_bytes ||
      bytes > program->text_bytes - offset || bytes == 0u ||
      program->text == NULL)
    return false;
  text->data = program->text + offset;
  text->bytes = bytes;
  return memchr(text->data, '\0', text->bytes) == NULL;
}

static void relation_hashes(const w_seed_accelerated_binding0_record *relation,
                            const accbind0_text text[ACCBIND0_TEXT_FIELDS],
                            uint8_t semantic[32], uint8_t provenance[32]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, (const uint8_t *)ACCBIND0_SEMANTIC_TAG,
                       sizeof(ACCBIND0_SEMANTIC_TAG) - 1u);
  for (size_t index = 0u; index < 12u; index += 1u)
    hash_text(&state, text[index].data, text[index].bytes);
  /* Queue, device, and generation are deployment provenance only. */
  hash_u64(&state, relation->required_maximum);
  hash_u64(&state, relation->effective_maximum);
  hash_limits(&state, relation->limits);
  hash_u32(&state, relation->result_bit_width);
  hash_u32(&state, relation->result_is_signed ? 1u : 0u);
  hash_u32(&state, (uint32_t)relation->submission);
  hash_u32(&state, (uint32_t)relation->numeric_mode);
  hash_u32(&state, (uint32_t)relation->fallback);
  w_seed_sha256_update(&state, relation->accinv_semantic_digest, 32u);
  w_seed_sha256_update(&state, relation->profile_semantic_digest, 32u);
  w_seed_sha256_update(&state, relation->artifact_provider_abi_digest, 32u);
  w_seed_sha256_update(&state, relation->provider_abi_digest, 32u);
  w_seed_sha256_update(&state, relation->artifact_instances_digest, 32u);
  w_seed_sha256_update(&state, relation->root_binding_digest, 32u);
  w_seed_sha256_final(&state, semantic);

  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, (const uint8_t *)ACCBIND0_PROVENANCE_TAG,
                       sizeof(ACCBIND0_PROVENANCE_TAG) - 1u);
  w_seed_sha256_update(&state, semantic, 32u);
  for (size_t index = 12u; index < ACCBIND0_TEXT_FIELDS; index += 1u)
    hash_text(&state, text[index].data, text[index].bytes);
  hash_u32(&state, relation->accinv_gpu_module_index);
  hash_u32(&state, relation->accinv_gpu_kernel_index);
  hash_u32(&state, relation->bound_gpu_module_index);
  hash_u32(&state, relation->bound_gpu_kernel_index);
  w_seed_sha256_update(&state, relation->accinv_provenance_digest, 32u);
  w_seed_sha256_update(&state, relation->profile_provenance_digest, 32u);
  w_seed_sha256_final(&state, provenance);
}

static bool assign_text(accbind0_scan *scan, size_t slot,
                        const uint8_t *data, size_t bytes, size_t *offset,
                        size_t *record_offset, size_t *record_bytes) {
  if (scan == NULL || data == NULL || bytes == 0u || offset == NULL ||
      record_offset == NULL || record_bytes == NULL ||
      slot >= ACCBIND0_TEXT_FIELDS || memchr(data, '\0', bytes) != NULL)
    return false;
  *record_offset = *offset;
  *record_bytes = bytes;
  scan->text[slot] = (accbind0_text){data, bytes};
  return size_add(*offset, bytes, offset);
}

static w_seed_accelerated_binding0_status collect(
    const w_seed_accelerated_binding0_input *input, accbind0_scan *scan) {
  if (input == NULL || scan == NULL || input->invocation_program == NULL ||
      input->invocation_result == NULL || input->profile == NULL)
    return W_SEED_ACCELERATED_BINDING0_INVALID_ARGUMENT;
  if (!w_seed_accelerated_invocation0_verify(input->invocation_program,
                                              input->invocation_result))
    return W_SEED_ACCELERATED_BINDING0_ACCINV0_INVALID;
  if (input->invocation_program->invocation_count != 1u)
    return W_SEED_ACCELERATED_BINDING0_UNSUPPORTED;

  const w_seed_accelerated_invocation0_record *invocation =
      &input->invocation_program->invocations[0];
  const w_seed_accelerated_binding0_closed_profile *profile = input->profile;
  if (profile->schema == NULL ||
      profile->schema_bytes !=
          sizeof(W_SEED_ACCELERATED_BINDING0_PROFILE_SCHEMA_VERSION) - 1u ||
      memcmp(profile->schema,
             W_SEED_ACCELERATED_BINDING0_PROFILE_SCHEMA_VERSION,
             profile->schema_bytes) != 0)
    return W_SEED_ACCELERATED_BINDING0_INVALID_SCHEMA;
  if (!profile->artifact_closed || !profile->root_owned ||
      !profile->provider_resolved || !profile->queue_device_match ||
      !profile->selected_instance_member)
    return W_SEED_ACCELERATED_BINDING0_PROFILE_NOT_CLOSED;
  if (profile->fallback != W_SEED_ACCELERATED_BINDING0_FALLBACK_REJECT)
    return W_SEED_ACCELERATED_BINDING0_UNSUPPORTED;
  if ((profile->submission != W_SEED_FRONTEND_DOMAIN_MODE_SERIAL &&
       profile->submission != W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT) ||
      (profile->numeric_mode != W_SEED_ACCELERATED_BINDING0_NUMERIC_STRICT &&
       profile->numeric_mode !=
           W_SEED_ACCELERATED_BINDING0_NUMERIC_REPRODUCIBLE &&
       profile->numeric_mode != W_SEED_ACCELERATED_BINDING0_NUMERIC_FAST) ||
      !limits_valid(profile->limits) || profile->profile_maximum == 0u ||
      profile->root_maximum == 0u || profile->deployment_maximum == 0u)
    return W_SEED_ACCELERATED_BINDING0_RANGE;

  const w_seed_accelerated_binding0_text profile_text[] = {
      profile->root_identity,
      profile->domain_identity,
      profile->descriptor_name,
      profile->module_identity,
      profile->artifact_identity,
      profile->artifact_module_identity,
      profile->kernel_label,
      profile->kernel_instance_identity,
      profile->artifact_target,
      profile->device_target,
      profile->provider_class,
      profile->queue_identity,
      profile->device_identity,
      profile->provider_generation,
  };
  for (size_t index = 0u; index < sizeof(profile_text) / sizeof(profile_text[0]);
       index += 1u)
    if (!text_valid(profile_text[index]))
      return W_SEED_ACCELERATED_BINDING0_RANGE;
  if (profile->domain_identity.data[0] == '.')
    return W_SEED_ACCELERATED_BINDING0_MISMATCH;
  if (!text_equal(profile->module_identity,
                  profile->artifact_module_identity) ||
      !text_equal(profile->artifact_target, profile->device_target) ||
      !digest_equal(profile->artifact_provider_abi_digest,
                    profile->provider_abi_digest) ||
      !digest_nonzero(profile->provider_abi_digest) ||
      !digest_nonzero(profile->artifact_instances_digest) ||
      !digest_nonzero(profile->profile_semantic_digest) ||
      !digest_nonzero(profile->profile_provenance_digest) ||
      !digest_nonzero(profile->root_binding_digest))
    return W_SEED_ACCELERATED_BINDING0_MISMATCH;
  if (profile->bound_gpu_module_index != invocation->gpu_module_index ||
      profile->bound_gpu_kernel_index != invocation->gpu_kernel_index ||
      profile->submission != invocation->submission)
    return W_SEED_ACCELERATED_BINDING0_MISMATCH;

  accbind0_text domain;
  accbind0_text descriptor;
  accbind0_text kernel;
  accbind0_text function;
  if (!accinv_text(input->invocation_program, invocation->domain_name_offset,
                   invocation->domain_name_bytes, &domain) ||
      !accinv_text(input->invocation_program, invocation->module_name_offset,
                   invocation->module_name_bytes, &descriptor) ||
      !accinv_text(input->invocation_program, invocation->kernel_label_offset,
                   invocation->kernel_label_bytes, &kernel) ||
      !accinv_text(input->invocation_program, invocation->function_name_offset,
                   invocation->function_name_bytes, &function))
    return W_SEED_ACCELERATED_BINDING0_INCONSISTENT;
  if (domain.bytes < 2u || domain.data[0] != '.' ||
      !text_equal_bytes(profile->domain_identity, domain.data + 1u,
                        domain.bytes - 1u) ||
      !text_equal_bytes(profile->descriptor_name, descriptor.data,
                        descriptor.bytes) ||
      !text_equal_bytes(profile->kernel_label, kernel.data, kernel.bytes))
    return W_SEED_ACCELERATED_BINDING0_MISMATCH;

  *scan = (accbind0_scan){0};
  scan->counts.relations = 1u;
  w_seed_accelerated_binding0_record *relation = &scan->relation;
  relation->accinv_gpu_module_index = invocation->gpu_module_index;
  relation->accinv_gpu_kernel_index = invocation->gpu_kernel_index;
  relation->bound_gpu_module_index = profile->bound_gpu_module_index;
  relation->bound_gpu_kernel_index = profile->bound_gpu_kernel_index;
  relation->required_maximum = invocation->domain_maximum;
  relation->effective_maximum =
      minimum5(relation->required_maximum, profile->profile_maximum,
               profile->root_maximum, profile->deployment_maximum,
               profile->limits.maximum_in_flight);
  if (relation->required_maximum == 0u || relation->effective_maximum == 0u)
    return W_SEED_ACCELERATED_BINDING0_RANGE;
  relation->limits = profile->limits;
  relation->limits.maximum_in_flight = relation->effective_maximum;
  relation->result_bit_width = invocation->result_bit_width;
  relation->result_is_signed = invocation->result_is_signed;
  relation->submission = profile->submission;
  relation->numeric_mode = profile->numeric_mode;
  relation->fallback = profile->fallback;
  memcpy(relation->accinv_semantic_digest,
         input->invocation_result->semantic_digest, 32u);
  memcpy(relation->accinv_provenance_digest,
         input->invocation_result->provenance_digest, 32u);
  memcpy(relation->profile_semantic_digest, profile->profile_semantic_digest,
         32u);
  memcpy(relation->profile_provenance_digest,
         profile->profile_provenance_digest, 32u);
  memcpy(relation->artifact_provider_abi_digest,
         profile->artifact_provider_abi_digest, 32u);
  memcpy(relation->provider_abi_digest, profile->provider_abi_digest, 32u);
  memcpy(relation->artifact_instances_digest,
         profile->artifact_instances_digest, 32u);
  memcpy(relation->root_binding_digest, profile->root_binding_digest, 32u);

  size_t offset = 0u;
#define ASSIGN(SLOT, DATA, BYTES, FIELD)                                        \
  do {                                                                          \
    if (!assign_text(scan, (SLOT), (DATA), (BYTES), &offset,                    \
                     &relation->FIELD##_offset, &relation->FIELD##_bytes))       \
      return W_SEED_ACCELERATED_BINDING0_RANGE;                                 \
  } while (0)
  ASSIGN(0u, (const uint8_t *)profile->root_identity.data,
         profile->root_identity.bytes, root);
  ASSIGN(1u, (const uint8_t *)profile->domain_identity.data,
         profile->domain_identity.bytes, domain);
  ASSIGN(2u, (const uint8_t *)profile->descriptor_name.data,
         profile->descriptor_name.bytes, descriptor);
  ASSIGN(3u, (const uint8_t *)profile->module_identity.data,
         profile->module_identity.bytes, module_identity);
  ASSIGN(4u, (const uint8_t *)profile->artifact_identity.data,
         profile->artifact_identity.bytes, artifact_identity);
  ASSIGN(5u, (const uint8_t *)profile->artifact_module_identity.data,
         profile->artifact_module_identity.bytes, artifact_module_identity);
  ASSIGN(6u, kernel.data, kernel.bytes, kernel_label);
  ASSIGN(7u, function.data, function.bytes, function_name);
  ASSIGN(8u, (const uint8_t *)profile->kernel_instance_identity.data,
         profile->kernel_instance_identity.bytes, kernel_instance);
  ASSIGN(9u, (const uint8_t *)profile->artifact_target.data,
         profile->artifact_target.bytes, artifact_target);
  ASSIGN(10u, (const uint8_t *)profile->device_target.data,
         profile->device_target.bytes, device_target);
  ASSIGN(11u, (const uint8_t *)profile->provider_class.data,
         profile->provider_class.bytes, provider_class);
  ASSIGN(12u, (const uint8_t *)profile->queue_identity.data,
         profile->queue_identity.bytes, queue);
  ASSIGN(13u, (const uint8_t *)profile->device_identity.data,
         profile->device_identity.bytes, device);
  ASSIGN(14u, (const uint8_t *)profile->provider_generation.data,
         profile->provider_generation.bytes, generation);
#undef ASSIGN
  scan->counts.text_bytes = offset;
  relation_hashes(relation, scan->text, scan->semantic_digest,
                  scan->provenance_digest);
  return W_SEED_ACCELERATED_BINDING0_OK;
}

static w_seed_accelerated_binding0_result result_from_scan(
    const accbind0_scan *scan, bool written) {
  w_seed_accelerated_binding0_result result = {0};
  result.status = W_SEED_ACCELERATED_BINDING0_OK;
  result.required = scan->counts;
  if (written) result.written = scan->counts;
  memcpy(result.schema, W_SEED_ACCELERATED_BINDING0_SCHEMA_VERSION,
         sizeof(result.schema));
  memcpy(result.invocation_schema,
         W_SEED_ACCELERATED_INVOCATION0_SCHEMA_VERSION,
         sizeof(result.invocation_schema));
  memcpy(result.profile_schema,
         W_SEED_ACCELERATED_BINDING0_PROFILE_SCHEMA_VERSION,
         sizeof(result.profile_schema));
  result.required_maximum = scan->relation.required_maximum;
  result.effective_maximum = scan->relation.effective_maximum;
  memcpy(result.accinv_semantic_digest,
         scan->relation.accinv_semantic_digest, 32u);
  memcpy(result.accinv_provenance_digest,
         scan->relation.accinv_provenance_digest, 32u);
  memcpy(result.profile_semantic_digest,
         scan->relation.profile_semantic_digest, 32u);
  memcpy(result.profile_provenance_digest,
         scan->relation.profile_provenance_digest, 32u);
  memcpy(result.semantic_digest, scan->semantic_digest, 32u);
  memcpy(result.provenance_digest, scan->provenance_digest, 32u);
  return result;
}

static bool append_input_ranges(const w_seed_accelerated_binding0_input *input,
                                accbind0_range ranges[ACCBIND0_MAX_RANGES],
                                size_t *count) {
  size_t relation_bytes = 0u;
  if (!size_multiply(input->invocation_program->invocation_capacity,
                     sizeof(*input->invocation_program->invocations),
                     &relation_bytes))
    return false;
#define ADD(PTR, BYTES)                                                         \
  do {                                                                          \
    if (!add_range(ranges, ACCBIND0_MAX_RANGES, count, (PTR), (BYTES)))         \
      return false;                                                             \
  } while (0)
  ADD(input, sizeof(*input));
  ADD(input->invocation_program, sizeof(*input->invocation_program));
  ADD(input->invocation_result, sizeof(*input->invocation_result));
  ADD(input->invocation_program->invocations, relation_bytes);
  ADD(input->invocation_program->text,
      input->invocation_program->text_capacity);
  ADD(input->profile, sizeof(*input->profile));
  ADD(input->profile->schema, input->profile->schema_bytes);
  const w_seed_accelerated_binding0_text text[] = {
      input->profile->root_identity,
      input->profile->domain_identity,
      input->profile->descriptor_name,
      input->profile->kernel_label,
      input->profile->module_identity,
      input->profile->artifact_identity,
      input->profile->artifact_module_identity,
      input->profile->kernel_instance_identity,
      input->profile->artifact_target,
      input->profile->device_target,
      input->profile->provider_class,
      input->profile->queue_identity,
      input->profile->device_identity,
      input->profile->provider_generation,
  };
  for (size_t index = 0u; index < sizeof(text) / sizeof(text[0]); index += 1u)
    ADD(text[index].data, text[index].bytes);
#undef ADD
  return true;
}

static bool destinations_free(const w_seed_accelerated_binding0_input *input,
                              const void *first, size_t first_bytes,
                              const void *second, size_t second_bytes,
                              const void *third, size_t third_bytes) {
  accbind0_range inputs[ACCBIND0_MAX_RANGES];
  size_t input_count = 0u;
  if (!append_input_ranges(input, inputs, &input_count)) return false;
  accbind0_range outputs[3];
  if (!range_make(first, first_bytes, &outputs[0]) ||
      !range_make(second, second_bytes, &outputs[1]) ||
      !range_make(third, third_bytes, &outputs[2]))
    return false;
  for (size_t left = 0u; left < 3u; left += 1u) {
    for (size_t right = left + 1u; right < 3u; right += 1u)
      if (ranges_overlap(outputs[left], outputs[right])) return false;
    for (size_t source = 0u; source < input_count; source += 1u)
      if (ranges_overlap(outputs[left], inputs[source])) return false;
  }
  return true;
}

static bool result_shape_valid(const w_seed_accelerated_binding0_result *result,
                               bool require_written) {
  return result != NULL && result->status == W_SEED_ACCELERATED_BINDING0_OK &&
         memcmp(result->schema, W_SEED_ACCELERATED_BINDING0_SCHEMA_VERSION,
                sizeof(result->schema)) == 0 &&
         memcmp(result->invocation_schema,
                W_SEED_ACCELERATED_INVOCATION0_SCHEMA_VERSION,
                sizeof(result->invocation_schema)) == 0 &&
         memcmp(result->profile_schema,
                W_SEED_ACCELERATED_BINDING0_PROFILE_SCHEMA_VERSION,
                sizeof(result->profile_schema)) == 0 &&
         result->required.relations == 1u &&
         result->required.text_bytes != 0u &&
         (!require_written ||
          (result->written.relations == result->required.relations &&
           result->written.text_bytes == result->required.text_bytes)) &&
         result->required_maximum != 0u && result->effective_maximum != 0u &&
         result->effective_maximum <= result->required_maximum;
}

w_seed_accelerated_binding0_status w_seed_accelerated_binding0_measure(
    const w_seed_accelerated_binding0_input *input,
    w_seed_accelerated_binding0_counts *counts,
    w_seed_accelerated_binding0_result *result) {
  if (counts == NULL || result == NULL)
    return W_SEED_ACCELERATED_BINDING0_INVALID_ARGUMENT;
  accbind0_scan scan;
  const w_seed_accelerated_binding0_status status = collect(input, &scan);
  if (status != W_SEED_ACCELERATED_BINDING0_OK) return status;
  if (!destinations_free(input, counts, sizeof(*counts), result,
                         sizeof(*result), NULL, 0u))
    return W_SEED_ACCELERATED_BINDING0_ALIAS;
  const w_seed_accelerated_binding0_result candidate =
      result_from_scan(&scan, false);
  *counts = scan.counts;
  *result = candidate;
  return W_SEED_ACCELERATED_BINDING0_OK;
}

w_seed_accelerated_binding0_status w_seed_accelerated_binding0_run(
    const w_seed_accelerated_binding0_input *input,
    const w_seed_accelerated_binding0_output *output,
    w_seed_accelerated_binding0_result *result) {
  if (output == NULL || result == NULL)
    return W_SEED_ACCELERATED_BINDING0_INVALID_ARGUMENT;
  accbind0_scan scan;
  const w_seed_accelerated_binding0_status status = collect(input, &scan);
  if (status != W_SEED_ACCELERATED_BINDING0_OK) return status;
  size_t relation_bytes = 0u;
  if (!size_multiply(output->relation_capacity, sizeof(*output->relations),
                     &relation_bytes))
    return W_SEED_ACCELERATED_BINDING0_RANGE;
  if (output->relation_capacity < scan.counts.relations ||
      output->text_capacity < scan.counts.text_bytes ||
      output->relations == NULL || output->text == NULL)
    return W_SEED_ACCELERATED_BINDING0_CAPACITY;
  if (!destinations_free(input, output->relations, relation_bytes, output->text,
                         output->text_capacity, result, sizeof(*result)))
    return W_SEED_ACCELERATED_BINDING0_ALIAS;
  const w_seed_accelerated_binding0_result candidate =
      result_from_scan(&scan, true);
  /* Nothing below the first write can fail. */
  output->relations[0] = scan.relation;
  size_t offset = 0u;
  for (size_t index = 0u; index < ACCBIND0_TEXT_FIELDS; index += 1u) {
    memcpy(output->text + offset, scan.text[index].data, scan.text[index].bytes);
    offset += scan.text[index].bytes;
  }
  *result = candidate;
  return W_SEED_ACCELERATED_BINDING0_OK;
}

static bool program_texts(
    const w_seed_accelerated_binding0_program *program,
    const w_seed_accelerated_binding0_record *relation,
    accbind0_text text[ACCBIND0_TEXT_FIELDS]) {
  const size_t offsets[] = {
      relation->root_offset,
      relation->domain_offset,
      relation->descriptor_offset,
      relation->module_identity_offset,
      relation->artifact_identity_offset,
      relation->artifact_module_identity_offset,
      relation->kernel_label_offset,
      relation->function_name_offset,
      relation->kernel_instance_offset,
      relation->artifact_target_offset,
      relation->device_target_offset,
      relation->provider_class_offset,
      relation->queue_offset,
      relation->device_offset,
      relation->generation_offset,
  };
  const size_t sizes[] = {
      relation->root_bytes,
      relation->domain_bytes,
      relation->descriptor_bytes,
      relation->module_identity_bytes,
      relation->artifact_identity_bytes,
      relation->artifact_module_identity_bytes,
      relation->kernel_label_bytes,
      relation->function_name_bytes,
      relation->kernel_instance_bytes,
      relation->artifact_target_bytes,
      relation->device_target_bytes,
      relation->provider_class_bytes,
      relation->queue_bytes,
      relation->device_bytes,
      relation->generation_bytes,
  };
  size_t cursor = 0u;
  for (size_t index = 0u; index < ACCBIND0_TEXT_FIELDS; index += 1u) {
    if (offsets[index] != cursor || sizes[index] == 0u ||
        offsets[index] > program->text_bytes ||
        sizes[index] > program->text_bytes - offsets[index] ||
        program->text == NULL ||
        memchr(program->text + offsets[index], '\0', sizes[index]) != NULL)
      return false;
    text[index] =
        (accbind0_text){program->text + offsets[index], sizes[index]};
    if (!text_valid((w_seed_accelerated_binding0_text){
            (const char *)text[index].data, text[index].bytes}))
      return false;
    if (!size_add(cursor, sizes[index], &cursor)) return false;
  }
  return cursor == program->text_bytes;
}

static bool program_ranges_free(
    const w_seed_accelerated_binding0_program *program,
    const w_seed_accelerated_binding0_result *result) {
  size_t relation_bytes = 0u;
  if (program == NULL || result == NULL ||
      !size_multiply(program->relation_capacity, sizeof(*program->relations),
                     &relation_bytes))
    return false;
  accbind0_range ranges[4];
  if (!range_make(program, sizeof(*program), &ranges[0]) ||
      !range_make(result, sizeof(*result), &ranges[1]) ||
      !range_make(program->relations, relation_bytes, &ranges[2]) ||
      !range_make(program->text, program->text_capacity, &ranges[3]))
    return false;
  for (size_t left = 0u; left < 4u; left += 1u)
    for (size_t right = left + 1u; right < 4u; right += 1u)
      if (ranges_overlap(ranges[left], ranges[right])) return false;
  return true;
}

bool w_seed_accelerated_binding0_verify(
    const w_seed_accelerated_binding0_program *program,
    const w_seed_accelerated_binding0_result *result) {
  if (program == NULL || !result_shape_valid(result, true) ||
      program->relations == NULL || program->relation_count != 1u ||
      program->relation_capacity < program->relation_count ||
      program->text_bytes != result->written.text_bytes ||
      program->text_capacity < program->text_bytes ||
      !program_ranges_free(program, result))
    return false;
  const w_seed_accelerated_binding0_record *relation = &program->relations[0];
  if (relation->accinv_gpu_module_index != relation->bound_gpu_module_index ||
      relation->accinv_gpu_kernel_index != relation->bound_gpu_kernel_index ||
      (relation->submission != W_SEED_FRONTEND_DOMAIN_MODE_SERIAL &&
       relation->submission != W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT) ||
      (relation->numeric_mode != W_SEED_ACCELERATED_BINDING0_NUMERIC_STRICT &&
       relation->numeric_mode !=
           W_SEED_ACCELERATED_BINDING0_NUMERIC_REPRODUCIBLE &&
       relation->numeric_mode != W_SEED_ACCELERATED_BINDING0_NUMERIC_FAST) ||
      relation->fallback != W_SEED_ACCELERATED_BINDING0_FALLBACK_REJECT ||
      !limits_valid(relation->limits) || relation->required_maximum == 0u ||
      relation->effective_maximum == 0u ||
      relation->effective_maximum > relation->required_maximum ||
      relation->result_bit_width != 32u || !relation->result_is_signed ||
      relation->limits.maximum_in_flight != relation->effective_maximum ||
      relation->required_maximum != result->required_maximum ||
      relation->effective_maximum != result->effective_maximum ||
      !digest_equal(relation->accinv_semantic_digest,
                    result->accinv_semantic_digest) ||
      !digest_equal(relation->accinv_provenance_digest,
                    result->accinv_provenance_digest) ||
      !digest_equal(relation->profile_semantic_digest,
                    result->profile_semantic_digest) ||
      !digest_equal(relation->profile_provenance_digest,
                    result->profile_provenance_digest) ||
      !digest_equal(program->semantic_digest, result->semantic_digest) ||
      !digest_equal(program->provenance_digest, result->provenance_digest))
    return false;
  accbind0_text text[ACCBIND0_TEXT_FIELDS];
  if (!program_texts(program, relation, text) || text[1].data[0] == '.' ||
      text[3].bytes != text[5].bytes ||
      memcmp(text[3].data, text[5].data, text[3].bytes) != 0 ||
      text[9].bytes != text[10].bytes ||
      memcmp(text[9].data, text[10].data, text[9].bytes) != 0 ||
      !digest_equal(relation->artifact_provider_abi_digest,
                    relation->provider_abi_digest) ||
      !digest_nonzero(relation->provider_abi_digest) ||
      !digest_nonzero(relation->artifact_instances_digest) ||
      !digest_nonzero(relation->root_binding_digest))
    return false;
  uint8_t semantic[32];
  uint8_t provenance[32];
  relation_hashes(relation, text, semantic, provenance);
  return digest_equal(semantic, program->semantic_digest) &&
         digest_equal(provenance, program->provenance_digest);
}

bool w_seed_accelerated_binding0_program_from_output(
    const w_seed_accelerated_binding0_output *output,
    const w_seed_accelerated_binding0_result *result,
    w_seed_accelerated_binding0_program *program) {
  if (output == NULL || program == NULL || !result_shape_valid(result, true) ||
      output->relations == NULL || output->text == NULL ||
      output->relation_capacity < result->written.relations ||
      output->text_capacity < result->written.text_bytes)
    return false;
  w_seed_accelerated_binding0_program candidate = {
      .relations = output->relations,
      .relation_count = result->written.relations,
      .relation_capacity = output->relation_capacity,
      .text = output->text,
      .text_bytes = result->written.text_bytes,
      .text_capacity = output->text_capacity,
  };
  memcpy(candidate.semantic_digest, result->semantic_digest, 32u);
  memcpy(candidate.provenance_digest, result->provenance_digest, 32u);
  if (!w_seed_accelerated_binding0_verify(&candidate, result)) return false;
  size_t relation_bytes = 0u;
  if (!size_multiply(output->relation_capacity, sizeof(*output->relations),
                     &relation_bytes))
    return false;
  accbind0_range destination;
  accbind0_range sources[4];
  if (!range_make(program, sizeof(*program), &destination) ||
      !range_make(output, sizeof(*output), &sources[0]) ||
      !range_make(result, sizeof(*result), &sources[1]) ||
      !range_make(output->relations, relation_bytes, &sources[2]) ||
      !range_make(output->text, output->text_capacity, &sources[3]))
    return false;
  for (size_t index = 0u; index < 4u; index += 1u)
    if (ranges_overlap(destination, sources[index])) return false;
  *program = candidate;
  return true;
}
