#include "w_seed_gpu0.h"

#include <limits.h>
#include <string.h>

#include "w_seed_sha256.h"
#include "w_seed_source.h"
#include "w_seed_unicode.h"

enum {
  GPU0_DIGEST_BYTES = W_SEED_GPU0_SHA256_BYTES,
  GPU0_PHASE_COUNT = 8,
  GPU0_HOST_OPERATION_COUNT = 6,
  GPU0_DEVICE_OPERATION_COUNT = 1,
};

static const char GPU0_SEMANTIC_TAG[] = "w-seed-gpu0-semantic-1";
static const char GPU0_HOST_IDENTITY_TAG[] = "w-seed-gpu0-host-identity-1";
static const char GPU0_DEVICE_IDENTITY_TAG[] =
    "w-seed-gpu0-device-identity-1";

typedef struct {
  uintptr_t begin;
  uintptr_t end;
  bool active;
} gpu0_memory_range;

typedef struct {
  uint8_t host[W_SEED_GPU0_EVIDENCE_ARTIFACT_CAPACITY];
  uint8_t device[W_SEED_GPU0_EVIDENCE_ARTIFACT_CAPACITY];
  size_t host_bytes;
  size_t device_bytes;
  w_seed_gpu0_measurement measurement;
  w_seed_gpu0_result result;
} gpu0_candidate;

typedef struct {
  uint8_t *bytes;
  size_t capacity;
  size_t offset;
} gpu0_writer;

_Static_assert(CHAR_BIT == 8, "GPU0 requires 8-bit bytes");
_Static_assert(W_SEED_GPU0_RESULT_BYTES == sizeof(int32_t),
               "GPU0 result size must be one i32");
_Static_assert(W_SEED_GPU0_EVIDENCE_FUNCTION_CAPACITY <= UINT32_MAX,
               "GPU0 function capacity must fit semantic indices");
_Static_assert(W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY <= UINT32_MAX,
               "GPU0 operation capacity must fit semantic indices");
_Static_assert(GPU0_PHASE_COUNT ==
                   (int)(W_SEED_GPU0_PHASE_JOINED + 1),
               "GPU0 phase count must cover the closed lifecycle");

static bool bytes_equal(const void *left, const void *right, size_t count) {
  if (count == 0u) return true;
  if (left == NULL || right == NULL) return false;
  return memcmp(left, right, count) == 0;
}

static void copy_bytes(void *destination, const void *source, size_t count) {
  if (count == 0u) return;
  (void)memcpy(destination, source, count);
}

static bool memory_range_make(const void *pointer, size_t count,
                              gpu0_memory_range *range) {
  if (range == NULL) return false;
  *range = (gpu0_memory_range){0u, 0u, false};
  if (count == 0u) return true;
  if (pointer == NULL || count > SIZE_MAX / sizeof(uint8_t)) return false;
  const uintptr_t begin = (uintptr_t)pointer;
  const size_t bytes = count * sizeof(uint8_t);
  if (bytes > (size_t)UINTPTR_MAX ||
      begin > UINTPTR_MAX - (uintptr_t)bytes)
    return false;
  *range = (gpu0_memory_range){begin, begin + (uintptr_t)bytes, true};
  return true;
}

static bool memory_ranges_overlap(const void *left, size_t left_count,
                                  const void *right, size_t right_count) {
  if (left_count == 0u || right_count == 0u) return false;
  gpu0_memory_range left_range;
  gpu0_memory_range right_range;
  if (!memory_range_make(left, left_count, &left_range) ||
      !memory_range_make(right, right_count, &right_range))
    return true;
  return left_range.active && right_range.active &&
         left_range.begin < right_range.end &&
         right_range.begin < left_range.end;
}

static bool range_equal(const w_seed_gpu0_range *left,
                        const w_seed_gpu0_range *right) {
  return left != NULL && right != NULL &&
         left->address_space == right->address_space &&
         left->offset == right->offset && left->bytes == right->bytes &&
         left->capacity_bytes == right->capacity_bytes;
}

static bool range_empty(const w_seed_gpu0_range *range) {
  return range != NULL && range->address_space == W_SEED_GPU0_ADDRESS_HOST &&
         range->offset == 0u && range->bytes == 0u &&
         range->capacity_bytes == 0u;
}

static bool semantic_ranges_overlap(const w_seed_gpu0_range *left,
                                    const w_seed_gpu0_range *right) {
  if (left == NULL || right == NULL || left->bytes == 0u ||
      right->bytes == 0u || left->address_space != right->address_space)
    return false;
  const uint64_t left_end = (uint64_t)left->offset + (uint64_t)left->bytes;
  const uint64_t right_end = (uint64_t)right->offset + (uint64_t)right->bytes;
  return (uint64_t)left->offset < right_end &&
         (uint64_t)right->offset < left_end;
}

static bool semantic_range_valid(const w_seed_gpu0_range *range,
                                 const w_seed_gpu0_program *program) {
  if (range == NULL || program == NULL) return false;
  if (range->bytes == 0u)
    return range_empty(range);
  uint32_t expected_capacity = 0u;
  if (range->address_space == W_SEED_GPU0_ADDRESS_HOST)
    expected_capacity = program->host_result_capacity_bytes;
  else if (range->address_space == W_SEED_GPU0_ADDRESS_DEVICE)
    expected_capacity = program->device_result_capacity_bytes;
  else
    return false;
  return range->capacity_bytes == expected_capacity &&
         range->offset <= range->capacity_bytes &&
         range->bytes <= range->capacity_bytes - range->offset;
}

static uint32_t decode_valid_utf8(const uint8_t *bytes, size_t *width) {
  const uint8_t first = bytes[0];
  if (first < 0x80u) {
    *width = 1u;
    return (uint32_t)first;
  }
  if (first < 0xe0u) {
    *width = 2u;
    return (((uint32_t)first & UINT32_C(0x1f)) << 6) |
           ((uint32_t)bytes[1] & UINT32_C(0x3f));
  }
  if (first < 0xf0u) {
    *width = 3u;
    return (((uint32_t)first & UINT32_C(0x0f)) << 12) |
           (((uint32_t)bytes[1] & UINT32_C(0x3f)) << 6) |
           ((uint32_t)bytes[2] & UINT32_C(0x3f));
  }
  *width = 4u;
  return (((uint32_t)first & UINT32_C(0x07)) << 18) |
         (((uint32_t)bytes[1] & UINT32_C(0x3f)) << 12) |
         (((uint32_t)bytes[2] & UINT32_C(0x3f)) << 6) |
         ((uint32_t)bytes[3] & UINT32_C(0x3f));
}

static bool identifier_name(const char *name, size_t length) {
  if (name == NULL || length == 0u) return false;
  w_seed_source source;
  w_seed_source_error error;
  if (!w_seed_source_init(
          (w_seed_byte_view){(const uint8_t *)name, length}, &source, &error))
    return false;
  size_t offset = 0u;
  bool first = true;
  while (offset < length) {
    size_t width = 0u;
    const uint32_t code_point =
        decode_valid_utf8((const uint8_t *)name + offset, &width);
    if ((first && !w_seed_unicode_is_identifier_start(code_point)) ||
        (!first && !w_seed_unicode_is_identifier_continue(code_point)))
      return false;
    first = false;
    offset += width;
  }
  return true;
}

static bool result_range_valid(const w_seed_gpu0_range *range,
                               w_seed_gpu0_address_space address_space,
                               uint32_t capacity_bytes) {
  return range != NULL && range->address_space == address_space &&
         range->bytes == W_SEED_GPU0_RESULT_BYTES &&
         range->capacity_bytes == capacity_bytes && range->offset == 0u &&
         range->bytes <= range->capacity_bytes;
}

static bool operation_range_pair_valid(const w_seed_gpu0_operation *operation) {
  return operation != NULL && !semantic_ranges_overlap(&operation->source,
                                                        &operation->destination);
}

static w_seed_gpu0_status validate_program(
    const w_seed_gpu0_program *program) {
  if (program == NULL) return W_SEED_GPU0_INVALID_ARGUMENT;
  if (program->function_count > W_SEED_GPU0_EVIDENCE_FUNCTION_CAPACITY ||
      program->operation_count > W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY ||
      program->function_capacity > W_SEED_GPU0_EVIDENCE_FUNCTION_CAPACITY ||
      program->operation_capacity > W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY ||
      program->function_capacity < program->function_count ||
      program->operation_capacity < program->operation_count)
    return W_SEED_GPU0_CAPACITY;
  if (program->functions == NULL || program->operations == NULL)
    return W_SEED_GPU0_INVALID_ARGUMENT;
  if (program->function_count != W_SEED_GPU0_EVIDENCE_FUNCTION_CAPACITY ||
      program->operation_count != W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY)
    return W_SEED_GPU0_INCONSISTENT;
  if (program->host_result_capacity_bytes < W_SEED_GPU0_RESULT_BYTES ||
      program->device_result_capacity_bytes < W_SEED_GPU0_RESULT_BYTES ||
      program->host_result_capacity_bytes >
          W_SEED_GPU0_EVIDENCE_RESULT_CAPACITY ||
      program->device_result_capacity_bytes >
          W_SEED_GPU0_EVIDENCE_RESULT_CAPACITY)
    return W_SEED_GPU0_CAPACITY;

  const w_seed_gpu0_function *host = &program->functions[0];
  const w_seed_gpu0_function *kernel = &program->functions[1];
  if (host->role != W_SEED_GPU0_FUNCTION_HOST_ROOT ||
      kernel->role != W_SEED_GPU0_FUNCTION_DEVICE_KERNEL ||
      !identifier_name(host->name, host->name_length) ||
      !identifier_name(kernel->name, kernel->name_length) ||
      host->interface_name != NULL || host->interface_name_length != 0u ||
      !identifier_name(kernel->interface_name,
                       kernel->interface_name_length))
    return W_SEED_GPU0_INCONSISTENT;
  if (host->first_operation != 0u ||
      host->operation_count != GPU0_HOST_OPERATION_COUNT ||
      kernel->first_operation != GPU0_HOST_OPERATION_COUNT ||
      kernel->operation_count != GPU0_DEVICE_OPERATION_COUNT ||
      host->parameter_count != 0u || kernel->parameter_count != 1u)
    return W_SEED_GPU0_RANGE;
  if (host->effects != W_SEED_GPU0_EFFECT_NONE)
    return W_SEED_GPU0_UNSUPPORTED;
  if (kernel->effects != W_SEED_GPU0_EFFECT_NONE)
    return W_SEED_GPU0_UNSUPPORTED;

  const w_seed_gpu0_range host_result = {
      W_SEED_GPU0_ADDRESS_HOST, 0u, W_SEED_GPU0_RESULT_BYTES,
      program->host_result_capacity_bytes};
  const w_seed_gpu0_range device_result = {
      W_SEED_GPU0_ADDRESS_DEVICE, 0u, W_SEED_GPU0_RESULT_BYTES,
      program->device_result_capacity_bytes};
  if (!result_range_valid(&host_result, W_SEED_GPU0_ADDRESS_HOST,
                          program->host_result_capacity_bytes) ||
      !result_range_valid(&device_result, W_SEED_GPU0_ADDRESS_DEVICE,
                          program->device_result_capacity_bytes))
    return W_SEED_GPU0_RANGE;

  const w_seed_gpu0_operation *operations = program->operations;
  for (size_t index = 0u; index < program->operation_count; index += 1u) {
    if (!semantic_range_valid(&operations[index].source, program) ||
        !semantic_range_valid(&operations[index].destination, program))
      return W_SEED_GPU0_RANGE;
    if (!operation_range_pair_valid(&operations[index]))
      return W_SEED_GPU0_ALIAS;
  }
  const w_seed_gpu0_operation *allocate = &operations[0];
  if (allocate->kind != W_SEED_GPU0_OPERATION_ALLOCATE_DEVICE_RESULT ||
      !range_empty(&allocate->source) ||
      !range_equal(&allocate->destination, &device_result) ||
      allocate->function_index != W_SEED_GPU0_NONE ||
      allocate->dependency_operation != W_SEED_GPU0_NONE ||
      allocate->i32_value != 0 || !operation_range_pair_valid(allocate))
    return W_SEED_GPU0_RANGE;

  const w_seed_gpu0_operation *copy_to_device = &operations[1];
  if (copy_to_device->kind != W_SEED_GPU0_OPERATION_COPY_HOST_TO_DEVICE ||
      !range_equal(&copy_to_device->source, &host_result) ||
      !range_equal(&copy_to_device->destination, &device_result) ||
      copy_to_device->function_index != W_SEED_GPU0_NONE ||
      copy_to_device->dependency_operation != W_SEED_GPU0_NONE ||
      copy_to_device->i32_value != 0 ||
      !operation_range_pair_valid(copy_to_device))
    return W_SEED_GPU0_RANGE;

  const w_seed_gpu0_operation *launch = &operations[2];
  if (launch->kind != W_SEED_GPU0_OPERATION_LAUNCH ||
      !range_empty(&launch->source) || !range_empty(&launch->destination) ||
      launch->function_index != 1u ||
      launch->dependency_operation != W_SEED_GPU0_NONE ||
      launch->i32_value != 0 || !operation_range_pair_valid(launch))
    return W_SEED_GPU0_INCONSISTENT;

  const w_seed_gpu0_operation *join = &operations[3];
  if (join->kind != W_SEED_GPU0_OPERATION_JOIN ||
      !range_empty(&join->source) || !range_empty(&join->destination) ||
      join->function_index != W_SEED_GPU0_NONE ||
      join->dependency_operation != 2u || join->i32_value != 0 ||
      !operation_range_pair_valid(join))
    return W_SEED_GPU0_INCONSISTENT;

  const w_seed_gpu0_operation *copy_to_host = &operations[4];
  if (copy_to_host->kind != W_SEED_GPU0_OPERATION_COPY_DEVICE_TO_HOST ||
      !range_equal(&copy_to_host->source, &device_result) ||
      !range_equal(&copy_to_host->destination, &host_result) ||
      copy_to_host->function_index != W_SEED_GPU0_NONE ||
      copy_to_host->dependency_operation != W_SEED_GPU0_NONE ||
      copy_to_host->i32_value != 0 || !operation_range_pair_valid(copy_to_host))
    return W_SEED_GPU0_RANGE;

  const w_seed_gpu0_operation *verify = &operations[5];
  const w_seed_gpu0_operation *store = &operations[6];
  if (verify->kind != W_SEED_GPU0_OPERATION_VERIFY_RESULT ||
      !range_equal(&verify->source, &host_result) ||
      !range_empty(&verify->destination) ||
      verify->function_index != W_SEED_GPU0_NONE ||
      verify->dependency_operation != W_SEED_GPU0_NONE ||
      !operation_range_pair_valid(verify))
    return W_SEED_GPU0_RANGE;
  if (store->kind != W_SEED_GPU0_OPERATION_STORE_I32 ||
      !range_empty(&store->source) ||
      !range_equal(&store->destination, &device_result) ||
      store->function_index != W_SEED_GPU0_NONE ||
      store->dependency_operation != W_SEED_GPU0_NONE ||
      store->i32_value != verify->i32_value ||
      !operation_range_pair_valid(store))
    return W_SEED_GPU0_RANGE;
  return W_SEED_GPU0_OK;
}

static int32_t program_payload(const w_seed_gpu0_program *program) {
  return program->operations[5].i32_value;
}

static void hash_u32(w_seed_sha256_state *state, uint32_t value) {
  uint8_t bytes[sizeof(value)];
  bytes[0] = (uint8_t)value;
  bytes[1] = (uint8_t)(value >> 8);
  bytes[2] = (uint8_t)(value >> 16);
  bytes[3] = (uint8_t)(value >> 24);
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void hash_u64(w_seed_sha256_state *state, uint64_t value) {
  uint8_t bytes[sizeof(value)];
  for (size_t index = 0u; index < sizeof(bytes); index += 1u)
    bytes[index] = (uint8_t)(value >> (index * 8u));
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void hash_name(w_seed_sha256_state *state, const char *name,
                      size_t length) {
  hash_u64(state, (uint64_t)length);
  w_seed_sha256_update(state, (const uint8_t *)name, length);
}

static void hash_semantic_range(w_seed_sha256_state *state,
                                const w_seed_gpu0_range *range) {
  hash_u32(state, (uint32_t)range->address_space);
  hash_u32(state, range->offset);
  hash_u32(state, range->bytes);
}

static void hash_operation(w_seed_sha256_state *state,
                           const w_seed_gpu0_operation *operation) {
  hash_u32(state, (uint32_t)operation->kind);
  hash_semantic_range(state, &operation->source);
  hash_semantic_range(state, &operation->destination);
  hash_u32(state, operation->function_index);
  hash_u32(state, operation->dependency_operation);
  hash_u32(state, (uint32_t)operation->i32_value);
}

static void semantic_identity(const w_seed_gpu0_program *program,
                              uint8_t digest[GPU0_DIGEST_BYTES]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, (const uint8_t *)GPU0_SEMANTIC_TAG,
                       sizeof(GPU0_SEMANTIC_TAG) - 1u);
  hash_u64(&state, (uint64_t)program->function_count);
  hash_u64(&state, (uint64_t)program->operation_count);
  for (size_t index = 0u; index < program->function_count; index += 1u) {
    const w_seed_gpu0_function *function = &program->functions[index];
    hash_name(&state, function->name, function->name_length);
    hash_name(&state, function->interface_name,
              function->interface_name_length);
    hash_u32(&state, (uint32_t)function->role);
    hash_u32(&state, function->first_operation);
    hash_u32(&state, function->operation_count);
    hash_u32(&state, function->parameter_count);
    hash_u32(&state, function->effects);
  }
  for (size_t index = 0u; index < program->operation_count; index += 1u)
    hash_operation(&state, &program->operations[index]);
  w_seed_sha256_final(&state, digest);
}

static void artifact_identity(const char *tag,
                              const uint8_t semantic[GPU0_DIGEST_BYTES],
                              const w_seed_gpu0_program *program,
                              size_t first_operation, size_t operation_count,
                              uint8_t digest[GPU0_DIGEST_BYTES]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, (const uint8_t *)tag, strlen(tag));
  w_seed_sha256_update(&state, semantic, GPU0_DIGEST_BYTES);
  hash_u64(&state, (uint64_t)first_operation);
  hash_u64(&state, (uint64_t)operation_count);
  for (size_t index = first_operation;
       index < first_operation + operation_count; index += 1u)
    hash_operation(&state, &program->operations[index]);
  w_seed_sha256_final(&state, digest);
}

static bool writer_append(gpu0_writer *writer, const void *bytes,
                          size_t count) {
  if (writer == NULL || (count != 0u && bytes == NULL) ||
      writer->offset > writer->capacity ||
      count > writer->capacity - writer->offset)
    return false;
  copy_bytes(writer->bytes + writer->offset, bytes, count);
  writer->offset += count;
  return true;
}

static bool writer_literal(gpu0_writer *writer, const char *literal) {
  return literal != NULL &&
         writer_append(writer, literal, strlen(literal));
}

static bool writer_hex(gpu0_writer *writer,
                       const uint8_t digest[GPU0_DIGEST_BYTES]) {
  static const char hex[] = "0123456789abcdef";
  if (writer == NULL || digest == NULL) return false;
  for (size_t index = 0u; index < GPU0_DIGEST_BYTES; index += 1u) {
    const char pair[2] = {hex[digest[index] >> 4],
                          hex[digest[index] & (uint8_t)0x0fu]};
    if (!writer_append(writer, pair, sizeof(pair))) return false;
  }
  return true;
}

static bool writer_i32(gpu0_writer *writer, int32_t value) {
  char digits[11];
  size_t count = 0u;
  uint32_t magnitude = 0u;
  const bool negative = value < 0;
  if (negative)
    magnitude = (uint32_t)(-(int64_t)value);
  else
    magnitude = (uint32_t)value;
  do {
    digits[count] = (char)('0' + (magnitude % 10u));
    magnitude /= 10u;
    count += 1u;
  } while (magnitude != 0u);
  if (negative) {
    digits[count] = '-';
    count += 1u;
  }
  for (size_t left = 0u; left < count / 2u; left += 1u) {
    const size_t right = count - left - 1u;
    const char saved = digits[left];
    digits[left] = digits[right];
    digits[right] = saved;
  }
  return writer_append(writer, digits, count);
}

static bool writer_name(gpu0_writer *writer, const char *name, size_t length) {
  return identifier_name(name, length) && writer_append(writer, name, length);
}

static bool emit_host_artifact(const w_seed_gpu0_program *program,
                               const uint8_t semantic[GPU0_DIGEST_BYTES],
                               uint8_t *bytes, size_t *written) {
  if (program == NULL || semantic == NULL || bytes == NULL || written == NULL)
    return false;
  gpu0_writer writer = {bytes, W_SEED_GPU0_EVIDENCE_ARTIFACT_CAPACITY, 0u};
  const w_seed_gpu0_function *host = &program->functions[0];
  const w_seed_gpu0_function *kernel = &program->functions[1];
  if (!writer_literal(&writer, "// " W_SEED_GPU0_HOST_ARTIFACT_SCHEMA_VERSION
                      "\n// host_root=") ||
      !writer_name(&writer, host->name, host->name_length) ||
      !writer_literal(&writer, "\n// kernel=") ||
      !writer_name(&writer, kernel->interface_name,
                   kernel->interface_name_length) ||
      !writer_literal(&writer, "\n// implementation=") ||
      !writer_name(&writer, kernel->name, kernel->name_length) ||
      !writer_literal(&writer, "\n// semantic_identity=") ||
      !writer_hex(&writer, semantic) ||
      !writer_literal(&writer,
                      "\nmodule {\n"
                      "  func.func private @w_gpu0_allocate_device_result()\n"
                      "  func.func private @w_gpu0_copy_host_to_device()\n"
                      "  func.func private @w_gpu0_launch()\n"
                      "  func.func private @w_gpu0_join()\n"
                      "  func.func private @w_gpu0_copy_device_to_host()\n"
                      "  func.func private @w_gpu0_verify_result()\n"
                      "  func.func @w_gpu0_host_root() {\n"
                      "    func.call @w_gpu0_allocate_device_result() : () -> ()\n"
                      "    func.call @w_gpu0_copy_host_to_device() : () -> ()\n"
                      "    func.call @w_gpu0_launch() : () -> ()\n"
                      "    func.call @w_gpu0_join() : () -> ()\n"
                      "    func.call @w_gpu0_copy_device_to_host() : () -> ()\n"
                      "    func.call @w_gpu0_verify_result() : () -> ()\n"
                      "    func.return\n"
                      "  }\n"
                      "}\n"))
    return false;
  *written = writer.offset;
  return true;
}

static bool emit_device_artifact(const w_seed_gpu0_program *program,
                                 const uint8_t semantic[GPU0_DIGEST_BYTES],
                                 int32_t payload,
                                 uint8_t *bytes, size_t *written) {
  if (program == NULL || semantic == NULL || bytes == NULL || written == NULL)
    return false;
  gpu0_writer writer = {bytes, W_SEED_GPU0_EVIDENCE_ARTIFACT_CAPACITY, 0u};
  const w_seed_gpu0_function *kernel = &program->functions[1];
  if (!writer_literal(&writer,
                      "// " W_SEED_GPU0_DEVICE_ARTIFACT_SCHEMA_VERSION
                      "\n// kernel=") ||
      !writer_name(&writer, kernel->interface_name,
                   kernel->interface_name_length) ||
      !writer_literal(&writer, "\n// implementation=") ||
      !writer_name(&writer, kernel->name, kernel->name_length) ||
      !writer_literal(&writer, "\n// semantic_identity=") ||
      !writer_hex(&writer, semantic) ||
      !writer_literal(&writer,
                      "\nmodule {\n"
                      "  gpu.module @w_gpu0_device {\n"
                      "    gpu.func @w_gpu0_kernel(%result: memref<1xi32, 1>) kernel {\n"
                      "      %c0 = arith.constant 0 : index\n"
                      "      %value = arith.constant ") ||
      !writer_i32(&writer, payload) ||
      !writer_literal(&writer,
                      " : i32\n"
                      "      memref.store %value, %result[%c0] : memref<1xi32, 1>\n"
                      "      gpu.return\n"
                      "    }\n"
                      "  }\n"
                      "}\n"))
    return false;
  *written = writer.offset;
  return true;
}

static void fill_measurement(const w_seed_gpu0_program *program,
                             size_t host_bytes, size_t device_bytes,
                             const uint8_t semantic[GPU0_DIGEST_BYTES],
                             const uint8_t host_identity[GPU0_DIGEST_BYTES],
                             const uint8_t device_identity[GPU0_DIGEST_BYTES],
                             w_seed_gpu0_measurement *measurement) {
  (void)memset(measurement, 0, sizeof(*measurement));
  copy_bytes(measurement->schema, W_SEED_GPU0_SCHEMA_VERSION,
             sizeof(measurement->schema));
  measurement->host_artifact_bytes = host_bytes;
  measurement->device_artifact_bytes = device_bytes;
  measurement->total_artifact_bytes = host_bytes + device_bytes;
  copy_bytes(measurement->semantic_identity, semantic, GPU0_DIGEST_BYTES);
  measurement->host.bytes = host_bytes;
  copy_bytes(measurement->host.identity_digest, host_identity,
             GPU0_DIGEST_BYTES);
  measurement->device.bytes = device_bytes;
  copy_bytes(measurement->device.identity_digest, device_identity,
             GPU0_DIGEST_BYTES);
  measurement->metrics = (w_seed_gpu0_metrics){
      .dispatch_count = 1u,
      .join_count = 1u,
      .memory_operation_count = 3u,
      .end_to_end_phase_count = GPU0_PHASE_COUNT,
      .device_allocation_bytes = W_SEED_GPU0_RESULT_BYTES,
      .host_to_device_bytes = W_SEED_GPU0_RESULT_BYTES,
      .device_to_host_bytes = W_SEED_GPU0_RESULT_BYTES,
  };
  (void)program;
}

static void digest_bytes(const uint8_t *bytes, size_t count,
                         uint8_t digest[GPU0_DIGEST_BYTES]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, bytes, count);
  w_seed_sha256_final(&state, digest);
}

static w_seed_gpu0_status build_candidate(const w_seed_gpu0_program *program,
                                          gpu0_candidate *candidate) {
  if (candidate == NULL) return W_SEED_GPU0_INVALID_ARGUMENT;
  const w_seed_gpu0_status validation = validate_program(program);
  if (validation != W_SEED_GPU0_OK) return validation;
  (void)memset(candidate, 0, sizeof(*candidate));

  uint8_t semantic[GPU0_DIGEST_BYTES];
  uint8_t host_identity[GPU0_DIGEST_BYTES];
  uint8_t device_identity[GPU0_DIGEST_BYTES];
  const int32_t payload = program_payload(program);
  semantic_identity(program, semantic);
  artifact_identity(GPU0_HOST_IDENTITY_TAG, semantic, program, 0u,
                    GPU0_HOST_OPERATION_COUNT, host_identity);
  artifact_identity(GPU0_DEVICE_IDENTITY_TAG, semantic, program,
                    GPU0_HOST_OPERATION_COUNT, GPU0_DEVICE_OPERATION_COUNT,
                    device_identity);
  if (!emit_host_artifact(program, semantic, candidate->host,
                          &candidate->host_bytes) ||
      !emit_device_artifact(program, semantic, payload, candidate->device,
                            &candidate->device_bytes))
    return W_SEED_GPU0_CAPACITY;
  fill_measurement(program, candidate->host_bytes, candidate->device_bytes,
                   semantic, host_identity, device_identity,
                   &candidate->measurement);
  (void)memset(&candidate->result, 0, sizeof(candidate->result));
  copy_bytes(candidate->result.schema, W_SEED_GPU0_SCHEMA_VERSION,
             sizeof(candidate->result.schema));
  candidate->result.status = W_SEED_GPU0_OK;
  copy_bytes(&candidate->result.measurement, &candidate->measurement,
             sizeof(candidate->measurement));
  copy_bytes(candidate->result.host.schema, W_SEED_GPU0_SCHEMA_VERSION,
             sizeof(candidate->result.host.schema));
  candidate->result.host.bytes = candidate->host_bytes;
  copy_bytes(candidate->result.host.identity_digest, host_identity,
             GPU0_DIGEST_BYTES);
  digest_bytes(candidate->host, candidate->host_bytes,
               candidate->result.host.artifact_digest);
  copy_bytes(candidate->result.device.schema, W_SEED_GPU0_SCHEMA_VERSION,
             sizeof(candidate->result.device.schema));
  candidate->result.device.bytes = candidate->device_bytes;
  copy_bytes(candidate->result.device.identity_digest, device_identity,
             GPU0_DIGEST_BYTES);
  digest_bytes(candidate->device, candidate->device_bytes,
               candidate->result.device.artifact_digest);
  candidate->result.phase_count = GPU0_PHASE_COUNT;
  for (uint32_t phase = 0u; phase < GPU0_PHASE_COUNT; phase += 1u)
    candidate->result.phases[phase] = (w_seed_gpu0_phase)phase;
  candidate->result.device_result_value = payload;
  candidate->result.host_result_value = payload;
  return W_SEED_GPU0_OK;
}

static bool program_aliases(const w_seed_gpu0_program *program,
                            const void *destination, size_t destination_bytes) {
  if (program == NULL || destination == NULL || destination_bytes == 0u)
    return false;
  if (memory_ranges_overlap(program, sizeof(*program), destination,
                            destination_bytes) ||
      memory_ranges_overlap(program->functions,
                            program->function_capacity *
                                sizeof(*program->functions),
                            destination, destination_bytes) ||
      memory_ranges_overlap(program->operations,
                            program->operation_capacity *
                                sizeof(*program->operations),
                            destination, destination_bytes))
    return true;
  for (size_t index = 0u; index < program->function_count; index += 1u)
    if (memory_ranges_overlap(program->functions[index].name,
                              program->functions[index].name_length,
                              destination, destination_bytes) ||
        memory_ranges_overlap(program->functions[index].interface_name,
                              program->functions[index].interface_name_length,
                              destination, destination_bytes))
      return true;
  return false;
}

static bool output_aliases_result(const w_seed_gpu0_output *output,
                                  const w_seed_gpu0_result *result) {
  if (output == NULL || result == NULL) return true;
  return memory_ranges_overlap(output, sizeof(*output), result,
                               sizeof(*result)) ||
         memory_ranges_overlap(output->host_artifact, output->host_capacity,
                               result, sizeof(*result)) ||
         memory_ranges_overlap(output->device_artifact,
                               output->device_capacity, result,
                               sizeof(*result)) ||
         memory_ranges_overlap(output->host_result, sizeof(int32_t), result,
                               sizeof(*result)) ||
         memory_ranges_overlap(output->device_result, sizeof(int32_t), result,
                               sizeof(*result));
}

static w_seed_gpu0_status validate_output(const w_seed_gpu0_program *program,
                                          const w_seed_gpu0_output *output,
                                          const w_seed_gpu0_result *result,
                                          const gpu0_candidate *candidate) {
  if (program == NULL || output == NULL || result == NULL ||
      candidate == NULL)
    return W_SEED_GPU0_INVALID_ARGUMENT;
  if (program_aliases(program, output, sizeof(*output)) ||
      program_aliases(program, result, sizeof(*result)) ||
      memory_ranges_overlap(output, sizeof(*output), result, sizeof(*result)))
    return W_SEED_GPU0_ALIAS;
  if (output->host_artifact == NULL || output->device_artifact == NULL ||
      output->host_result == NULL || output->device_result == NULL)
    return W_SEED_GPU0_INVALID_ARGUMENT;
  if (output->host_capacity > W_SEED_GPU0_EVIDENCE_ARTIFACT_CAPACITY ||
      output->device_capacity > W_SEED_GPU0_EVIDENCE_ARTIFACT_CAPACITY ||
      output->host_capacity < candidate->host_bytes ||
      output->device_capacity < candidate->device_bytes)
    return W_SEED_GPU0_CAPACITY;
  if (program_aliases(program, output->host_artifact,
                      output->host_capacity) ||
      program_aliases(program, output->device_artifact,
                      output->device_capacity) ||
      program_aliases(program, output->host_result, sizeof(int32_t)) ||
      program_aliases(program, output->device_result, sizeof(int32_t)))
    return W_SEED_GPU0_ALIAS;
  if (output_aliases_result(output, result) ||
      memory_ranges_overlap(output->host_artifact, output->host_capacity,
                            output->device_artifact,
                            output->device_capacity) ||
      memory_ranges_overlap(output->host_artifact, output->host_capacity,
                            output->host_result, sizeof(int32_t)) ||
      memory_ranges_overlap(output->host_artifact, output->host_capacity,
                            output->device_result, sizeof(int32_t)) ||
      memory_ranges_overlap(output->device_artifact, output->device_capacity,
                            output->host_result, sizeof(int32_t)) ||
      memory_ranges_overlap(output->device_artifact, output->device_capacity,
                            output->device_result, sizeof(int32_t)) ||
      memory_ranges_overlap(output->host_result, sizeof(int32_t),
                            output->device_result, sizeof(int32_t)))
    return W_SEED_GPU0_ALIAS;
  return W_SEED_GPU0_OK;
}

static bool measurement_equal(const w_seed_gpu0_measurement *left,
                              const w_seed_gpu0_measurement *right) {
  return left != NULL && right != NULL &&
         bytes_equal(left->schema, right->schema, sizeof(left->schema)) &&
         left->host_artifact_bytes == right->host_artifact_bytes &&
         left->device_artifact_bytes == right->device_artifact_bytes &&
         left->total_artifact_bytes == right->total_artifact_bytes &&
         bytes_equal(left->semantic_identity, right->semantic_identity,
                     GPU0_DIGEST_BYTES) &&
         left->host.bytes == right->host.bytes &&
         bytes_equal(left->host.identity_digest, right->host.identity_digest,
                     GPU0_DIGEST_BYTES) &&
         left->device.bytes == right->device.bytes &&
         bytes_equal(left->device.identity_digest,
                     right->device.identity_digest, GPU0_DIGEST_BYTES) &&
         left->metrics.dispatch_count == right->metrics.dispatch_count &&
         left->metrics.join_count == right->metrics.join_count &&
         left->metrics.memory_operation_count ==
             right->metrics.memory_operation_count &&
         left->metrics.end_to_end_phase_count ==
             right->metrics.end_to_end_phase_count &&
         left->metrics.device_allocation_bytes ==
             right->metrics.device_allocation_bytes &&
         left->metrics.host_to_device_bytes ==
             right->metrics.host_to_device_bytes &&
         left->metrics.device_to_host_bytes ==
             right->metrics.device_to_host_bytes;
}

static bool result_equal(const w_seed_gpu0_result *left,
                         const w_seed_gpu0_result *right) {
  if (left == NULL || right == NULL ||
      !bytes_equal(left->schema, right->schema, sizeof(left->schema)) ||
      left->status != right->status ||
      !measurement_equal(&left->measurement, &right->measurement) ||
      !bytes_equal(left->host.schema, right->host.schema,
                   sizeof(left->host.schema)) ||
      left->host.bytes != right->host.bytes ||
      !bytes_equal(left->host.identity_digest, right->host.identity_digest,
                   GPU0_DIGEST_BYTES) ||
      !bytes_equal(left->host.artifact_digest, right->host.artifact_digest,
                   GPU0_DIGEST_BYTES) ||
      !bytes_equal(left->device.schema, right->device.schema,
                   sizeof(left->device.schema)) ||
      left->device.bytes != right->device.bytes ||
      !bytes_equal(left->device.identity_digest,
                   right->device.identity_digest, GPU0_DIGEST_BYTES) ||
      !bytes_equal(left->device.artifact_digest,
                   right->device.artifact_digest, GPU0_DIGEST_BYTES) ||
      left->phase_count != right->phase_count ||
      left->phase_count != GPU0_PHASE_COUNT ||
      left->device_result_value != right->device_result_value ||
      left->host_result_value != right->host_result_value)
    return false;
  for (uint32_t index = 0u; index < left->phase_count; index += 1u)
    if (left->phases[index] != right->phases[index]) return false;
  return true;
}

w_seed_gpu0_status w_seed_gpu0_measure(
    const w_seed_gpu0_program *program, w_seed_gpu0_measurement *measurement) {
  if (program == NULL || measurement == NULL)
    return W_SEED_GPU0_INVALID_ARGUMENT;
  gpu0_candidate candidate;
  const w_seed_gpu0_status status = build_candidate(program, &candidate);
  if (status != W_SEED_GPU0_OK) return status;
  if (program_aliases(program, measurement, sizeof(*measurement)))
    return W_SEED_GPU0_ALIAS;
  copy_bytes(measurement, &candidate.measurement, sizeof(*measurement));
  return W_SEED_GPU0_OK;
}

w_seed_gpu0_status w_seed_gpu0_run(const w_seed_gpu0_program *program,
                                   const w_seed_gpu0_output *output,
                                   w_seed_gpu0_result *result) {
  if (program == NULL || output == NULL || result == NULL)
    return W_SEED_GPU0_INVALID_ARGUMENT;
  gpu0_candidate candidate;
  const w_seed_gpu0_status status = build_candidate(program, &candidate);
  if (status != W_SEED_GPU0_OK) return status;
  const w_seed_gpu0_status output_status =
      validate_output(program, output, result, &candidate);
  if (output_status != W_SEED_GPU0_OK) return output_status;

  copy_bytes(output->host_artifact, candidate.host, candidate.host_bytes);
  copy_bytes(output->device_artifact, candidate.device, candidate.device_bytes);
  copy_bytes(output->device_result, &candidate.result.device_result_value,
             sizeof(candidate.result.device_result_value));
  copy_bytes(output->host_result, &candidate.result.host_result_value,
             sizeof(candidate.result.host_result_value));
  copy_bytes(result, &candidate.result, sizeof(*result));
  return W_SEED_GPU0_OK;
}

bool w_seed_gpu0_verify(const w_seed_gpu0_program *program,
                        const w_seed_gpu0_output *output,
                        const w_seed_gpu0_result *result) {
  if (program == NULL || output == NULL || result == NULL) return false;
  gpu0_candidate candidate;
  if (build_candidate(program, &candidate) != W_SEED_GPU0_OK ||
      validate_output(program, output, result, &candidate) != W_SEED_GPU0_OK)
    return false;
  const int32_t payload = program_payload(program);
  if (*output->device_result != payload || *output->host_result != payload)
    return false;
  if (!bytes_equal(output->host_artifact, candidate.host,
                   candidate.host_bytes) ||
      !bytes_equal(output->device_artifact, candidate.device,
                   candidate.device_bytes))
    return false;
  return result_equal(result, &candidate.result);
}
