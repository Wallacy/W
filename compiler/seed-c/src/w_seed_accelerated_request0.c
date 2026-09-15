#include "w_seed_accelerated_request0.h"

#include <limits.h>
#include <string.h>

#include "w_seed_sha256.h"
#include "w_seed_source.h"

static const char ACCREQ0_SEMANTIC_TAG[] =
    "w-seed-accelerated-request0-semantic-1";
static const char ACCREQ0_PROVENANCE_TAG[] =
    "w-seed-accelerated-request0-provenance-1";
static const uint8_t ACCREQ0_KERNEL_SYMBOL[] = "w_gpu0_kernel";

enum { ACCREQ0_TEXT_FIELDS = 14, ACCREQ0_INPUT_RANGES = 24 };

typedef struct {
  const uint8_t *data;
  size_t bytes;
} request_text;

typedef struct {
  uintptr_t begin;
  uintptr_t end;
  bool active;
} request_range;

typedef struct {
  w_seed_accelerated_request0_counts counts;
  w_seed_accelerated_request0_record record;
  request_text text[ACCREQ0_TEXT_FIELDS];
  const uint8_t *artifact;
  uint8_t semantic[32];
  uint8_t provenance[32];
} request_scan;

static bool add_size(size_t left, size_t right, size_t *value) {
  if (value == NULL || right > SIZE_MAX - left) return false;
  *value = left + right;
  return true;
}

static bool multiply_size(size_t count, size_t width, size_t *value) {
  if (value == NULL || (width != 0u && count > SIZE_MAX / width)) return false;
  *value = count * width;
  return true;
}

static bool range_make(const void *pointer, size_t bytes, request_range *range) {
  if (range == NULL || (bytes != 0u && pointer == NULL)) return false;
  *range = (request_range){0u, 0u, false};
  if (bytes == 0u) return true;
  const uintptr_t begin = (uintptr_t)pointer;
  if (bytes > UINTPTR_MAX - begin) return false;
  *range = (request_range){begin, begin + bytes, true};
  return true;
}

static bool overlaps(request_range left, request_range right) {
  return left.active && right.active && left.begin < right.end &&
         right.begin < left.end;
}

static bool digest_equal(const uint8_t *left, const uint8_t *right) {
  return left != NULL && right != NULL && memcmp(left, right, 32u) == 0;
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

static void hash_text(w_seed_sha256_state *state, request_text text) {
  hash_u64(state, (uint64_t)text.bytes);
  w_seed_sha256_update(state, text.data, text.bytes);
}

static bool text_valid(request_text text) {
  if (text.data == NULL || text.bytes == 0u ||
      memchr(text.data, '\0', text.bytes) != NULL)
    return false;
  w_seed_source source;
  w_seed_source_error error;
  return w_seed_source_init((w_seed_byte_view){text.data, text.bytes}, &source,
                            &error);
}

static bool binding_text(
    const w_seed_accelerated_binding0_program *program, size_t offset,
    size_t bytes, request_text *text) {
  if (program == NULL || text == NULL || offset > program->text_bytes ||
      bytes > program->text_bytes - offset || program->text == NULL)
    return false;
  *text = (request_text){program->text + offset, bytes};
  return text_valid(*text);
}

static bool text_matches(request_text left, const char *right,
                         size_t right_bytes) {
  return text_valid(left) && right != NULL && left.bytes == right_bytes &&
         memcmp(left.data, right, right_bytes) == 0;
}

static void request_hashes(const w_seed_accelerated_request0_record *record,
                           const request_text text[ACCREQ0_TEXT_FIELDS],
                           uint8_t semantic[32], uint8_t provenance[32]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, (const uint8_t *)ACCREQ0_SEMANTIC_TAG,
                       sizeof(ACCREQ0_SEMANTIC_TAG) - 1u);
  for (size_t index = 0u; index < 11u; index += 1u)
    hash_text(&state, text[index]);
  hash_u64(&state, (uint64_t)record->device_artifact_bytes);
  hash_u32(&state, record->result_bytes);
  hash_u32(&state, record->result_bit_width);
  hash_u32(&state, record->result_is_signed ? 1u : 0u);
  hash_u32(&state, (uint32_t)record->sentinel_expected_i32);
  w_seed_sha256_update(&state, record->binding_semantic_digest, 32u);
  w_seed_sha256_update(&state, record->gpu_semantic_identity, 32u);
  w_seed_sha256_update(&state, record->device_artifact_identity_digest, 32u);
  w_seed_sha256_update(&state, record->device_artifact_digest, 32u);
  w_seed_sha256_final(&state, semantic);

  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, (const uint8_t *)ACCREQ0_PROVENANCE_TAG,
                       sizeof(ACCREQ0_PROVENANCE_TAG) - 1u);
  w_seed_sha256_update(&state, semantic, 32u);
  for (size_t index = 11u; index < ACCREQ0_TEXT_FIELDS; index += 1u)
    hash_text(&state, text[index]);
  w_seed_sha256_update(&state, record->binding_provenance_digest, 32u);
  w_seed_sha256_final(&state, provenance);
}

static bool append_text(request_scan *scan, size_t index, request_text text,
                        size_t *cursor, size_t *offset, size_t *bytes) {
  if (scan == NULL || cursor == NULL || offset == NULL || bytes == NULL ||
      index >= ACCREQ0_TEXT_FIELDS || !text_valid(text))
    return false;
  scan->text[index] = text;
  *offset = *cursor;
  *bytes = text.bytes;
  return add_size(*cursor, text.bytes, cursor);
}

static w_seed_accelerated_request0_status collect(
    const w_seed_accelerated_request0_input *input, request_scan *scan) {
  if (input == NULL || scan == NULL || input->binding_program == NULL ||
      input->binding_result == NULL || input->gpu_program == NULL ||
      input->gpu_output == NULL || input->gpu_result == NULL)
    return W_SEED_ACCELERATED_REQUEST0_INVALID_ARGUMENT;
  if (!w_seed_accelerated_binding0_verify(input->binding_program,
                                          input->binding_result))
    return W_SEED_ACCELERATED_REQUEST0_INVALID_BINDING;
  if (!w_seed_gpu0_verify(input->gpu_program, input->gpu_output,
                          input->gpu_result))
    return W_SEED_ACCELERATED_REQUEST0_INVALID_GPU0;
  if (input->binding_program->relation_count != 1u ||
      input->gpu_program->function_count != 2u ||
      input->gpu_program->operation_count != 7u)
    return W_SEED_ACCELERATED_REQUEST0_UNSUPPORTED;

  const w_seed_accelerated_binding0_record *binding =
      &input->binding_program->relations[0];
  const w_seed_gpu0_function *host = &input->gpu_program->functions[0];
  const w_seed_gpu0_function *device = &input->gpu_program->functions[1];
  request_text source[ACCREQ0_TEXT_FIELDS];
#define BIND_TEXT(INDEX, FIELD)                                                 \
  do {                                                                          \
    if (!binding_text(input->binding_program, binding->FIELD##_offset,          \
                      binding->FIELD##_bytes, &source[INDEX]))                  \
      return W_SEED_ACCELERATED_REQUEST0_INCONSISTENT;                          \
  } while (0)
  BIND_TEXT(0u, root);
  BIND_TEXT(1u, domain);
  BIND_TEXT(2u, descriptor);
  BIND_TEXT(3u, module_identity);
  BIND_TEXT(4u, artifact_identity);
  BIND_TEXT(5u, kernel_label);
  BIND_TEXT(6u, function_name);
  BIND_TEXT(7u, kernel_instance);
  BIND_TEXT(8u, artifact_target);
  BIND_TEXT(9u, provider_class);
  source[10] = (request_text){ACCREQ0_KERNEL_SYMBOL,
                              sizeof(ACCREQ0_KERNEL_SYMBOL) - 1u};
  BIND_TEXT(11u, queue);
  BIND_TEXT(12u, device);
  BIND_TEXT(13u, generation);
#undef BIND_TEXT

  if (!text_matches(source[2], host->name, host->name_length) ||
      !text_matches(source[5], device->interface_name,
                    device->interface_name_length) ||
      !text_matches(source[6], device->name, device->name_length) ||
      binding->result_bit_width != 32u || !binding->result_is_signed ||
      input->gpu_program->host_result_capacity_bytes != sizeof(int32_t) ||
      input->gpu_program->device_result_capacity_bytes != sizeof(int32_t) ||
      input->gpu_result->host_result_value !=
          input->gpu_result->device_result_value)
    return W_SEED_ACCELERATED_REQUEST0_MISMATCH;
  if (input->gpu_result->measurement.device_artifact_bytes == 0u ||
      input->gpu_result->measurement.device_artifact_bytes >
          input->gpu_output->device_capacity ||
      input->gpu_output->device_artifact == NULL)
    return W_SEED_ACCELERATED_REQUEST0_RANGE;

  *scan = (request_scan){0};
  scan->counts.requests = 1u;
  scan->counts.device_artifact_bytes =
      input->gpu_result->measurement.device_artifact_bytes;
  scan->artifact = input->gpu_output->device_artifact;
  w_seed_accelerated_request0_record *record = &scan->record;
  record->device_artifact_bytes = scan->counts.device_artifact_bytes;
  record->result_bytes = sizeof(int32_t);
  record->result_bit_width = binding->result_bit_width;
  record->result_is_signed = binding->result_is_signed;
  record->sentinel_expected_i32 = input->gpu_result->host_result_value;
  memcpy(record->binding_semantic_digest,
         input->binding_result->semantic_digest, 32u);
  memcpy(record->binding_provenance_digest,
         input->binding_result->provenance_digest, 32u);
  memcpy(record->gpu_semantic_identity,
         input->gpu_result->measurement.semantic_identity, 32u);
  memcpy(record->device_artifact_identity_digest,
         input->gpu_result->device.identity_digest, 32u);
  memcpy(record->device_artifact_digest,
         input->gpu_result->device.artifact_digest, 32u);

  size_t cursor = 0u;
#define APPEND(INDEX, FIELD)                                                    \
  do {                                                                          \
    if (!append_text(scan, INDEX, source[INDEX], &cursor,                       \
                     &record->FIELD##_offset, &record->FIELD##_bytes))          \
      return W_SEED_ACCELERATED_REQUEST0_RANGE;                                 \
  } while (0)
  APPEND(0u, root);
  APPEND(1u, domain);
  APPEND(2u, descriptor);
  APPEND(3u, module_identity);
  APPEND(4u, artifact_identity);
  APPEND(5u, kernel_label);
  APPEND(6u, function_name);
  APPEND(7u, kernel_instance);
  APPEND(8u, target);
  APPEND(9u, provider_class);
  APPEND(10u, kernel_symbol);
  APPEND(11u, queue);
  APPEND(12u, device);
  APPEND(13u, generation);
#undef APPEND
  scan->counts.text_bytes = cursor;
  request_hashes(record, scan->text, scan->semantic, scan->provenance);
  return W_SEED_ACCELERATED_REQUEST0_OK;
}

static w_seed_accelerated_request0_result make_result(const request_scan *scan,
                                                       bool written) {
  w_seed_accelerated_request0_result result = {0};
  result.status = W_SEED_ACCELERATED_REQUEST0_OK;
  result.required = scan->counts;
  if (written) result.written = scan->counts;
  memcpy(result.schema, W_SEED_ACCELERATED_REQUEST0_SCHEMA_VERSION,
         sizeof(result.schema));
  memcpy(result.binding_schema, W_SEED_ACCELERATED_BINDING0_SCHEMA_VERSION,
         sizeof(result.binding_schema));
  memcpy(result.gpu_schema, W_SEED_GPU0_SCHEMA_VERSION,
         sizeof(result.gpu_schema));
  memcpy(result.semantic_digest, scan->semantic, 32u);
  memcpy(result.provenance_digest, scan->provenance, 32u);
  return result;
}

static bool input_ranges(const w_seed_accelerated_request0_input *input,
                         request_range ranges[ACCREQ0_INPUT_RANGES],
                         size_t *count) {
  size_t binding_records = 0u;
  size_t gpu_functions = 0u;
  size_t gpu_operations = 0u;
  if (!multiply_size(input->binding_program->relation_capacity,
                     sizeof(*input->binding_program->relations),
                     &binding_records) ||
      !multiply_size(input->gpu_program->function_capacity,
                     sizeof(*input->gpu_program->functions), &gpu_functions) ||
      !multiply_size(input->gpu_program->operation_capacity,
                     sizeof(*input->gpu_program->operations), &gpu_operations))
    return false;
  const struct {
    const void *pointer;
    size_t bytes;
  } source[] = {
      {input, sizeof(*input)},
      {input->binding_program, sizeof(*input->binding_program)},
      {input->binding_result, sizeof(*input->binding_result)},
      {input->binding_program->relations, binding_records},
      {input->binding_program->text, input->binding_program->text_capacity},
      {input->gpu_program, sizeof(*input->gpu_program)},
      {input->gpu_output, sizeof(*input->gpu_output)},
      {input->gpu_result, sizeof(*input->gpu_result)},
      {input->gpu_program->functions, gpu_functions},
      {input->gpu_program->operations, gpu_operations},
      {input->gpu_output->host_artifact, input->gpu_output->host_capacity},
      {input->gpu_output->device_artifact, input->gpu_output->device_capacity},
      {input->gpu_output->host_result, sizeof(*input->gpu_output->host_result)},
      {input->gpu_output->device_result,
       sizeof(*input->gpu_output->device_result)},
      {input->gpu_program->functions[0].name,
       input->gpu_program->functions[0].name_length},
      {input->gpu_program->functions[0].interface_name,
       input->gpu_program->functions[0].interface_name_length},
      {input->gpu_program->functions[1].name,
       input->gpu_program->functions[1].name_length},
      {input->gpu_program->functions[1].interface_name,
       input->gpu_program->functions[1].interface_name_length},
      {ACCREQ0_KERNEL_SYMBOL, sizeof(ACCREQ0_KERNEL_SYMBOL) - 1u},
  };
  *count = sizeof(source) / sizeof(source[0]);
  for (size_t index = 0u; index < *count; index += 1u)
    if (!range_make(source[index].pointer, source[index].bytes, &ranges[index]))
      return false;
  return true;
}

static bool destinations_free(const w_seed_accelerated_request0_input *input,
                              const void *first, size_t first_bytes,
                              const void *second, size_t second_bytes,
                              const void *third, size_t third_bytes,
                              const void *fourth, size_t fourth_bytes,
                              const void *fifth, size_t fifth_bytes) {
  request_range sources[ACCREQ0_INPUT_RANGES];
  size_t source_count = 0u;
  if (!input_ranges(input, sources, &source_count)) return false;
  request_range destinations[5];
  if (!range_make(first, first_bytes, &destinations[0]) ||
      !range_make(second, second_bytes, &destinations[1]) ||
      !range_make(third, third_bytes, &destinations[2]) ||
      !range_make(fourth, fourth_bytes, &destinations[3]) ||
      !range_make(fifth, fifth_bytes, &destinations[4]))
    return false;
  for (size_t left = 0u; left < 5u; left += 1u) {
    for (size_t right = left + 1u; right < 5u; right += 1u)
      if (overlaps(destinations[left], destinations[right])) return false;
    for (size_t source = 0u; source < source_count; source += 1u)
      if (overlaps(destinations[left], sources[source])) return false;
  }
  return true;
}

w_seed_accelerated_request0_status w_seed_accelerated_request0_measure(
    const w_seed_accelerated_request0_input *input,
    w_seed_accelerated_request0_counts *counts,
    w_seed_accelerated_request0_result *result) {
  if (counts == NULL || result == NULL)
    return W_SEED_ACCELERATED_REQUEST0_INVALID_ARGUMENT;
  request_scan scan;
  const w_seed_accelerated_request0_status status = collect(input, &scan);
  if (status != W_SEED_ACCELERATED_REQUEST0_OK) return status;
  if (!destinations_free(input, counts, sizeof(*counts), result,
                         sizeof(*result), NULL, 0u, NULL, 0u, NULL, 0u))
    return W_SEED_ACCELERATED_REQUEST0_ALIAS;
  const w_seed_accelerated_request0_result candidate =
      make_result(&scan, false);
  *counts = scan.counts;
  *result = candidate;
  return W_SEED_ACCELERATED_REQUEST0_OK;
}

w_seed_accelerated_request0_status w_seed_accelerated_request0_run(
    const w_seed_accelerated_request0_input *input,
    const w_seed_accelerated_request0_output *output,
    w_seed_accelerated_request0_result *result) {
  if (output == NULL || result == NULL)
    return W_SEED_ACCELERATED_REQUEST0_INVALID_ARGUMENT;
  request_scan scan;
  const w_seed_accelerated_request0_status status = collect(input, &scan);
  if (status != W_SEED_ACCELERATED_REQUEST0_OK) return status;
  size_t request_bytes = 0u;
  if (!multiply_size(output->request_capacity, sizeof(*output->requests),
                     &request_bytes))
    return W_SEED_ACCELERATED_REQUEST0_RANGE;
  if (output->requests == NULL || output->text == NULL ||
      output->device_artifact == NULL ||
      output->request_capacity < scan.counts.requests ||
      output->text_capacity < scan.counts.text_bytes ||
      output->device_artifact_capacity < scan.counts.device_artifact_bytes)
    return W_SEED_ACCELERATED_REQUEST0_CAPACITY;
  if (!destinations_free(input, output->requests, request_bytes, output->text,
                         output->text_capacity, output->device_artifact,
                         output->device_artifact_capacity, result,
                         sizeof(*result), output, sizeof(*output)))
    return W_SEED_ACCELERATED_REQUEST0_ALIAS;
  const w_seed_accelerated_request0_result candidate = make_result(&scan, true);
  /* No operation below the first write can fail. */
  output->requests[0] = scan.record;
  size_t cursor = 0u;
  for (size_t index = 0u; index < ACCREQ0_TEXT_FIELDS; index += 1u) {
    memcpy(output->text + cursor, scan.text[index].data,
           scan.text[index].bytes);
    cursor += scan.text[index].bytes;
  }
  memcpy(output->device_artifact, scan.artifact,
         scan.counts.device_artifact_bytes);
  *result = candidate;
  return W_SEED_ACCELERATED_REQUEST0_OK;
}

static bool program_texts(const w_seed_accelerated_request0_program *program,
                          const w_seed_accelerated_request0_record *record,
                          request_text text[ACCREQ0_TEXT_FIELDS]) {
  const size_t offsets[] = {
      record->root_offset,          record->domain_offset,
      record->descriptor_offset,    record->module_identity_offset,
      record->artifact_identity_offset, record->kernel_label_offset,
      record->function_name_offset, record->kernel_instance_offset,
      record->target_offset,        record->provider_class_offset,
      record->kernel_symbol_offset, record->queue_offset,
      record->device_offset,        record->generation_offset,
  };
  const size_t bytes[] = {
      record->root_bytes,          record->domain_bytes,
      record->descriptor_bytes,    record->module_identity_bytes,
      record->artifact_identity_bytes, record->kernel_label_bytes,
      record->function_name_bytes, record->kernel_instance_bytes,
      record->target_bytes,        record->provider_class_bytes,
      record->kernel_symbol_bytes, record->queue_bytes,
      record->device_bytes,        record->generation_bytes,
  };
  size_t cursor = 0u;
  for (size_t index = 0u; index < ACCREQ0_TEXT_FIELDS; index += 1u) {
    if (offsets[index] != cursor || offsets[index] > program->text_bytes ||
        bytes[index] > program->text_bytes - offsets[index] ||
        program->text == NULL)
      return false;
    text[index] = (request_text){program->text + offsets[index], bytes[index]};
    if (!text_valid(text[index]) || !add_size(cursor, bytes[index], &cursor))
      return false;
  }
  return cursor == program->text_bytes;
}

static bool result_valid(const w_seed_accelerated_request0_result *result) {
  return result != NULL && result->status == W_SEED_ACCELERATED_REQUEST0_OK &&
         memcmp(result->schema, W_SEED_ACCELERATED_REQUEST0_SCHEMA_VERSION,
                sizeof(result->schema)) == 0 &&
         memcmp(result->binding_schema,
                W_SEED_ACCELERATED_BINDING0_SCHEMA_VERSION,
                sizeof(result->binding_schema)) == 0 &&
         memcmp(result->gpu_schema, W_SEED_GPU0_SCHEMA_VERSION,
                sizeof(result->gpu_schema)) == 0 &&
         result->required.requests == 1u &&
         result->required.requests == result->written.requests &&
         result->required.text_bytes == result->written.text_bytes &&
         result->required.device_artifact_bytes ==
             result->written.device_artifact_bytes &&
         result->written.text_bytes != 0u &&
         result->written.device_artifact_bytes != 0u;
}

static bool program_ranges_free(
    const w_seed_accelerated_request0_program *program,
    const w_seed_accelerated_request0_result *result) {
  size_t request_bytes = 0u;
  if (!multiply_size(program->request_capacity, sizeof(*program->requests),
                     &request_bytes))
    return false;
  request_range ranges[5];
  if (!range_make(program, sizeof(*program), &ranges[0]) ||
      !range_make(result, sizeof(*result), &ranges[1]) ||
      !range_make(program->requests, request_bytes, &ranges[2]) ||
      !range_make(program->text, program->text_capacity, &ranges[3]) ||
      !range_make(program->device_artifact,
                  program->device_artifact_capacity, &ranges[4]))
    return false;
  for (size_t left = 0u; left < 5u; left += 1u)
    for (size_t right = left + 1u; right < 5u; right += 1u)
      if (overlaps(ranges[left], ranges[right])) return false;
  return true;
}

bool w_seed_accelerated_request0_verify(
    const w_seed_accelerated_request0_program *program,
    const w_seed_accelerated_request0_result *result) {
  if (program == NULL || !result_valid(result) || program->requests == NULL ||
      program->request_count != 1u ||
      program->request_capacity < program->request_count ||
      program->text_bytes != result->written.text_bytes ||
      program->text_capacity < program->text_bytes ||
      program->device_artifact_bytes !=
          result->written.device_artifact_bytes ||
      program->device_artifact_capacity < program->device_artifact_bytes ||
      program->device_artifact == NULL || !program_ranges_free(program, result))
    return false;
  const w_seed_accelerated_request0_record *record = &program->requests[0];
  if (record->device_artifact_bytes != program->device_artifact_bytes ||
      record->result_bytes != sizeof(int32_t) ||
      record->result_bit_width != 32u || !record->result_is_signed)
    return false;
  request_text text[ACCREQ0_TEXT_FIELDS];
  if (!program_texts(program, record, text) ||
      text[10].bytes != sizeof(ACCREQ0_KERNEL_SYMBOL) - 1u ||
      memcmp(text[10].data, ACCREQ0_KERNEL_SYMBOL, text[10].bytes) != 0)
    return false;
  uint8_t artifact_digest[32];
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, program->device_artifact,
                       program->device_artifact_bytes);
  w_seed_sha256_final(&state, artifact_digest);
  if (!digest_equal(artifact_digest, record->device_artifact_digest))
    return false;
  uint8_t semantic[32];
  uint8_t provenance[32];
  request_hashes(record, text, semantic, provenance);
  return digest_equal(semantic, program->semantic_digest) &&
         digest_equal(provenance, program->provenance_digest) &&
         digest_equal(semantic, result->semantic_digest) &&
         digest_equal(provenance, result->provenance_digest);
}

bool w_seed_accelerated_request0_program_from_output(
    const w_seed_accelerated_request0_output *output,
    const w_seed_accelerated_request0_result *result,
    w_seed_accelerated_request0_program *program) {
  if (output == NULL || program == NULL || !result_valid(result) ||
      output->requests == NULL || output->text == NULL ||
      output->device_artifact == NULL ||
      output->request_capacity < result->written.requests ||
      output->text_capacity < result->written.text_bytes ||
      output->device_artifact_capacity <
          result->written.device_artifact_bytes)
    return false;
  w_seed_accelerated_request0_program candidate = {
      output->requests,
      result->written.requests,
      output->request_capacity,
      output->text,
      result->written.text_bytes,
      output->text_capacity,
      output->device_artifact,
      result->written.device_artifact_bytes,
      output->device_artifact_capacity,
      {0},
      {0},
  };
  memcpy(candidate.semantic_digest, result->semantic_digest, 32u);
  memcpy(candidate.provenance_digest, result->provenance_digest, 32u);
  if (!w_seed_accelerated_request0_verify(&candidate, result)) return false;
  size_t request_bytes = 0u;
  if (!multiply_size(output->request_capacity, sizeof(*output->requests),
                     &request_bytes))
    return false;
  request_range destination;
  request_range sources[5];
  if (!range_make(program, sizeof(*program), &destination) ||
      !range_make(output, sizeof(*output), &sources[0]) ||
      !range_make(result, sizeof(*result), &sources[1]) ||
      !range_make(output->requests, request_bytes, &sources[2]) ||
      !range_make(output->text, output->text_capacity, &sources[3]) ||
      !range_make(output->device_artifact,
                  output->device_artifact_capacity, &sources[4]))
    return false;
  for (size_t index = 0u; index < 5u; index += 1u)
    if (overlaps(destination, sources[index])) return false;
  for (size_t left = 0u; left < 5u; left += 1u)
    for (size_t right = left + 1u; right < 5u; right += 1u)
      if (overlaps(sources[left], sources[right])) return false;
  *program = candidate;
  return true;
}
