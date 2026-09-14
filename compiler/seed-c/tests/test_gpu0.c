#include "w_seed_gpu0.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { GPU0_CANONICAL_PAYLOAD = 42 };

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "gpu0 check failed: %s (%s:%d)\n", #condition, \
                    __FILE__, __LINE__);                                      \
      return false;                                                            \
    }                                                                          \
  } while (0)

static const w_seed_gpu0_function
    FUNCTIONS[W_SEED_GPU0_EVIDENCE_FUNCTION_CAPACITY] = {
    {.name = "hostRoot",
     .name_length = 8u,
     .interface_name = NULL,
     .interface_name_length = 0u,
     .role = W_SEED_GPU0_FUNCTION_HOST_ROOT,
     .first_operation = 0u,
     .operation_count = 6u,
     .parameter_count = 0u,
     .effects = W_SEED_GPU0_EFFECT_NONE},
    {.name = "helloKernel",
     .name_length = 11u,
     .interface_name = "hello",
     .interface_name_length = 5u,
     .role = W_SEED_GPU0_FUNCTION_DEVICE_KERNEL,
     .first_operation = 6u,
     .operation_count = 1u,
     .parameter_count = 1u,
     .effects = W_SEED_GPU0_EFFECT_NONE},
};

static const w_seed_gpu0_range HOST_RESULT = {
    .address_space = W_SEED_GPU0_ADDRESS_HOST,
    .offset = 0u,
    .bytes = W_SEED_GPU0_RESULT_BYTES,
    .capacity_bytes = W_SEED_GPU0_RESULT_BYTES};
static const w_seed_gpu0_range DEVICE_RESULT = {
    .address_space = W_SEED_GPU0_ADDRESS_DEVICE,
    .offset = 0u,
    .bytes = W_SEED_GPU0_RESULT_BYTES,
    .capacity_bytes = W_SEED_GPU0_RESULT_BYTES};

static const w_seed_gpu0_operation
    OPERATIONS[W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY] = {
    {.kind = W_SEED_GPU0_OPERATION_ALLOCATE_DEVICE_RESULT,
     .source = {0},
     .destination = DEVICE_RESULT,
     .function_index = W_SEED_GPU0_NONE,
     .dependency_operation = W_SEED_GPU0_NONE,
     .i32_value = 0},
    {.kind = W_SEED_GPU0_OPERATION_COPY_HOST_TO_DEVICE,
     .source = HOST_RESULT,
     .destination = DEVICE_RESULT,
     .function_index = W_SEED_GPU0_NONE,
     .dependency_operation = W_SEED_GPU0_NONE,
     .i32_value = 0},
    {.kind = W_SEED_GPU0_OPERATION_LAUNCH,
     .source = {0},
     .destination = {0},
     .function_index = 1u,
     .dependency_operation = W_SEED_GPU0_NONE,
     .i32_value = 0},
    {.kind = W_SEED_GPU0_OPERATION_JOIN,
     .source = {0},
     .destination = {0},
     .function_index = W_SEED_GPU0_NONE,
     .dependency_operation = 2u,
     .i32_value = 0},
    {.kind = W_SEED_GPU0_OPERATION_COPY_DEVICE_TO_HOST,
     .source = DEVICE_RESULT,
     .destination = HOST_RESULT,
     .function_index = W_SEED_GPU0_NONE,
     .dependency_operation = W_SEED_GPU0_NONE,
     .i32_value = 0},
    {.kind = W_SEED_GPU0_OPERATION_VERIFY_RESULT,
     .source = HOST_RESULT,
     .destination = {0},
     .function_index = W_SEED_GPU0_NONE,
     .dependency_operation = W_SEED_GPU0_NONE,
     .i32_value = GPU0_CANONICAL_PAYLOAD},
    {.kind = W_SEED_GPU0_OPERATION_STORE_I32,
     .source = {0},
     .destination = DEVICE_RESULT,
     .function_index = W_SEED_GPU0_NONE,
     .dependency_operation = W_SEED_GPU0_NONE,
     .i32_value = GPU0_CANONICAL_PAYLOAD},
};

static w_seed_gpu0_program fixture_program(void) {
  return (w_seed_gpu0_program){
      .functions = FUNCTIONS,
      .function_count = W_SEED_GPU0_EVIDENCE_FUNCTION_CAPACITY,
      .function_capacity = W_SEED_GPU0_EVIDENCE_FUNCTION_CAPACITY,
      .operations = OPERATIONS,
      .operation_count = W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY,
      .operation_capacity = W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY,
      .host_result_capacity_bytes = W_SEED_GPU0_RESULT_BYTES,
      .device_result_capacity_bytes = W_SEED_GPU0_RESULT_BYTES};
}

typedef struct {
  uint8_t host_artifact[W_SEED_GPU0_EVIDENCE_ARTIFACT_CAPACITY];
  uint8_t device_artifact[W_SEED_GPU0_EVIDENCE_ARTIFACT_CAPACITY];
  int32_t device_result;
  int32_t host_result;
  w_seed_gpu0_result result;
} test_storage;

static w_seed_gpu0_output output_for(test_storage *storage) {
  return (w_seed_gpu0_output){
      .host_artifact = storage->host_artifact,
      .host_capacity = sizeof(storage->host_artifact),
      .device_artifact = storage->device_artifact,
      .device_capacity = sizeof(storage->device_artifact),
      .device_result = &storage->device_result,
      .host_result = &storage->host_result};
}

static bool contains_bytes(const uint8_t *bytes, size_t length,
                           const char *needle) {
  if (bytes == NULL || needle == NULL) return false;
  const size_t needle_length = strlen(needle);
  if (needle_length == 0u || needle_length > length) return false;
  for (size_t index = 0u; index <= length - needle_length; index += 1u)
    if (memcmp(bytes + index, needle, needle_length) == 0) return true;
  return false;
}

static bool all_bytes_are(const uint8_t *bytes, size_t length,
                          uint8_t expected) {
  if (bytes == NULL) return false;
  for (size_t index = 0u; index < length; index += 1u)
    if (bytes[index] != expected) return false;
  return true;
}

static void initialize_storage(test_storage *storage, uint8_t marker) {
  (void)memset(storage, marker, sizeof(*storage));
  storage->device_result = -1;
  storage->host_result = -1;
}

static bool test_run_measure_verify(void) {
  const w_seed_gpu0_program program = fixture_program();
  test_storage storage;
  initialize_storage(&storage, 0xa5u);
  const w_seed_gpu0_output output = output_for(&storage);
  CHECK(w_seed_gpu0_run(&program, &output, &storage.result) ==
        W_SEED_GPU0_OK);
  CHECK(storage.device_result == GPU0_CANONICAL_PAYLOAD);
  CHECK(storage.host_result == GPU0_CANONICAL_PAYLOAD);
  CHECK(storage.result.status == W_SEED_GPU0_OK);
  CHECK(storage.result.phase_count == 8u);
  for (uint32_t phase = 0u; phase < storage.result.phase_count; phase += 1u)
    CHECK(storage.result.phases[phase] == (w_seed_gpu0_phase)phase);
  CHECK(storage.result.device_result_value ==
        GPU0_CANONICAL_PAYLOAD);
  CHECK(storage.result.host_result_value == GPU0_CANONICAL_PAYLOAD);
  CHECK(storage.result.measurement.metrics.dispatch_count == 1u);
  CHECK(storage.result.measurement.metrics.join_count == 1u);
  CHECK(storage.result.measurement.metrics.memory_operation_count == 3u);
  CHECK(storage.result.measurement.metrics.end_to_end_phase_count == 8u);
  CHECK(storage.result.measurement.metrics.device_allocation_bytes ==
        W_SEED_GPU0_RESULT_BYTES);
  CHECK(storage.result.measurement.metrics.host_to_device_bytes ==
        W_SEED_GPU0_RESULT_BYTES);
  CHECK(storage.result.measurement.metrics.device_to_host_bytes ==
        W_SEED_GPU0_RESULT_BYTES);
  CHECK(storage.result.measurement.host_artifact_bytes > 0u);
  CHECK(storage.result.measurement.device_artifact_bytes > 0u);
  CHECK(storage.result.measurement.total_artifact_bytes ==
        storage.result.measurement.host_artifact_bytes +
            storage.result.measurement.device_artifact_bytes);
  CHECK(contains_bytes(storage.host_artifact,
                       storage.result.measurement.host_artifact_bytes,
                       "func.call @w_gpu0_launch"));
  CHECK(contains_bytes(storage.host_artifact,
                       storage.result.measurement.host_artifact_bytes,
                       "func.call @w_gpu0_join"));
  CHECK(contains_bytes(storage.device_artifact,
                       storage.result.measurement.device_artifact_bytes,
                       "gpu.module @w_gpu0_device"));
  CHECK(contains_bytes(storage.device_artifact,
                       storage.result.measurement.device_artifact_bytes,
                       "memref.store %value"));
  CHECK(contains_bytes(storage.device_artifact,
                       storage.result.measurement.device_artifact_bytes,
                       "// kernel=hello"));
  CHECK(contains_bytes(storage.device_artifact,
                       storage.result.measurement.device_artifact_bytes,
                       "// implementation=helloKernel"));
  CHECK(contains_bytes(storage.device_artifact,
                       storage.result.measurement.device_artifact_bytes,
                       "memref<1xi32, 1>"));
  CHECK(contains_bytes(storage.device_artifact,
                       storage.result.measurement.device_artifact_bytes,
                       "42 : i32"));
  CHECK(!contains_bytes(storage.host_artifact,
                        storage.result.measurement.host_artifact_bytes,
                        "cuda"));
  CHECK(!contains_bytes(storage.device_artifact,
                        storage.result.measurement.device_artifact_bytes,
                        "vulkan"));

  w_seed_gpu0_measurement measurement;
  (void)memset(&measurement, 0x3cu, sizeof(measurement));
  CHECK(w_seed_gpu0_measure(&program, &measurement) == W_SEED_GPU0_OK);
  CHECK(memcmp(&measurement, &storage.result.measurement,
               sizeof(measurement)) == 0);
  CHECK(w_seed_gpu0_verify(&program, &output, &storage.result));
  return true;
}

static bool test_deterministic_outputs(void) {
  const w_seed_gpu0_program program = fixture_program();
  test_storage left;
  test_storage right;
  initialize_storage(&left, 0x11u);
  initialize_storage(&right, 0x22u);
  const w_seed_gpu0_output left_output = output_for(&left);
  const w_seed_gpu0_output right_output = output_for(&right);
  CHECK(w_seed_gpu0_run(&program, &left_output, &left.result) ==
        W_SEED_GPU0_OK);
  CHECK(w_seed_gpu0_run(&program, &right_output, &right.result) ==
        W_SEED_GPU0_OK);
  CHECK(memcmp(left.host_artifact, right.host_artifact,
               left.result.measurement.host_artifact_bytes) == 0);
  CHECK(memcmp(left.device_artifact, right.device_artifact,
               left.result.measurement.device_artifact_bytes) == 0);
  CHECK(memcmp(&left.result, &right.result, sizeof(left.result)) == 0);
  return true;
}

static bool test_data_driven_payload(void) {
  w_seed_gpu0_operation operations[W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY];
  (void)memcpy(operations, OPERATIONS, sizeof(operations));
  operations[5].i32_value = INT32_C(43);
  operations[6].i32_value = INT32_C(43);
  w_seed_gpu0_program program = fixture_program();
  program.operations = operations;

  test_storage storage;
  initialize_storage(&storage, 0x47u);
  const w_seed_gpu0_output output = output_for(&storage);
  CHECK(w_seed_gpu0_run(&program, &output, &storage.result) ==
        W_SEED_GPU0_OK);
  CHECK(storage.device_result == INT32_C(43) &&
        storage.host_result == INT32_C(43) &&
        storage.result.device_result_value == INT32_C(43) &&
        storage.result.host_result_value == INT32_C(43));
  CHECK(contains_bytes(storage.device_artifact,
                       storage.result.measurement.device_artifact_bytes,
                       "memref.store %value") &&
        contains_bytes(storage.device_artifact,
                       storage.result.measurement.device_artifact_bytes,
                       "43 : i32") &&
        !contains_bytes(storage.device_artifact,
                        storage.result.measurement.device_artifact_bytes,
                        "memref.store %c42"));
  CHECK(w_seed_gpu0_verify(&program, &output, &storage.result));

  operations[5].i32_value = INT32_C(44);
  const test_storage before = storage;
  CHECK(w_seed_gpu0_run(&program, &output, &storage.result) ==
        W_SEED_GPU0_RANGE);
  CHECK(memcmp(&storage, &before, sizeof(storage)) == 0);
  return true;
}

static bool test_signed_payload_literals(void) {
  const int32_t payloads[] = {INT32_C(0), INT32_C(-7), INT32_MIN};
  for (size_t payload_index = 0u;
       payload_index < sizeof(payloads) / sizeof(payloads[0]);
       payload_index += 1u) {
    w_seed_gpu0_operation operations[
        W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY];
    (void)memcpy(operations, OPERATIONS, sizeof(operations));
    operations[5].i32_value = payloads[payload_index];
    operations[6].i32_value = payloads[payload_index];
    w_seed_gpu0_program program = fixture_program();
    program.operations = operations;

    test_storage storage;
    initialize_storage(&storage, (uint8_t)(0x51u + payload_index));
    const w_seed_gpu0_output output = output_for(&storage);
    CHECK(w_seed_gpu0_run(&program, &output, &storage.result) ==
          W_SEED_GPU0_OK);
    CHECK(storage.device_result == payloads[payload_index] &&
          storage.host_result == payloads[payload_index] &&
          storage.result.device_result_value == payloads[payload_index] &&
          storage.result.host_result_value == payloads[payload_index]);
    CHECK(contains_bytes(storage.device_artifact,
                         storage.result.measurement.device_artifact_bytes,
                         "%value = arith.constant ") &&
          !contains_bytes(storage.device_artifact,
                          storage.result.measurement.device_artifact_bytes,
                          "%c-"));
    if (payloads[payload_index] == INT32_C(-7))
      CHECK(contains_bytes(storage.device_artifact,
                           storage.result.measurement.device_artifact_bytes,
                           "-7 : i32"));
    if (payloads[payload_index] == INT32_MIN)
      CHECK(contains_bytes(storage.device_artifact,
                           storage.result.measurement.device_artifact_bytes,
                           "-2147483648 : i32"));
    CHECK(w_seed_gpu0_verify(&program, &output, &storage.result));
  }
  return true;
}

static bool test_capacity_independent_identity(void) {
  const w_seed_gpu0_program base = fixture_program();
  w_seed_gpu0_operation
      operations[W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY];
  (void)memcpy(operations, OPERATIONS, sizeof(operations));
  const uint32_t wider_capacity = 8u;
  for (size_t index = 0u; index < W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY;
       index += 1u) {
    if (operations[index].source.bytes != 0u)
      operations[index].source.capacity_bytes = wider_capacity;
    if (operations[index].destination.bytes != 0u)
      operations[index].destination.capacity_bytes = wider_capacity;
  }
  w_seed_gpu0_program wider = base;
  wider.operations = operations;
  wider.host_result_capacity_bytes = wider_capacity;
  wider.device_result_capacity_bytes = wider_capacity;

  test_storage narrow_storage;
  test_storage wider_storage;
  initialize_storage(&narrow_storage, 0x31u);
  initialize_storage(&wider_storage, 0x73u);
  const w_seed_gpu0_output narrow_output = output_for(&narrow_storage);
  const w_seed_gpu0_output wider_output = output_for(&wider_storage);
  CHECK(w_seed_gpu0_run(&base, &narrow_output, &narrow_storage.result) ==
        W_SEED_GPU0_OK);
  CHECK(w_seed_gpu0_run(&wider, &wider_output, &wider_storage.result) ==
        W_SEED_GPU0_OK);
  CHECK(narrow_storage.result.measurement.host_artifact_bytes ==
        wider_storage.result.measurement.host_artifact_bytes);
  CHECK(narrow_storage.result.measurement.device_artifact_bytes ==
        wider_storage.result.measurement.device_artifact_bytes);
  CHECK(memcmp(narrow_storage.host_artifact, wider_storage.host_artifact,
               narrow_storage.result.measurement.host_artifact_bytes) == 0);
  CHECK(memcmp(narrow_storage.device_artifact, wider_storage.device_artifact,
               narrow_storage.result.measurement.device_artifact_bytes) == 0);
  CHECK(memcmp(&narrow_storage.result, &wider_storage.result,
               sizeof(narrow_storage.result)) == 0);
  return true;
}

static bool test_unicode_identifiers(void) {
  static const char host_name[] = "h\xc3\xb3spede";
  static const char kernel_name[] = "n\xc3\xba" "cleo";
  w_seed_gpu0_function
      functions[W_SEED_GPU0_EVIDENCE_FUNCTION_CAPACITY];
  (void)memcpy(functions, FUNCTIONS, sizeof(functions));
  functions[0].name = host_name;
  functions[0].name_length = sizeof(host_name) - 1u;
  functions[1].name = kernel_name;
  functions[1].name_length = sizeof(kernel_name) - 1u;
  w_seed_gpu0_program program = fixture_program();
  program.functions = functions;
  test_storage storage;
  initialize_storage(&storage, 0x19u);
  const w_seed_gpu0_output output = output_for(&storage);
  CHECK(w_seed_gpu0_run(&program, &output, &storage.result) ==
        W_SEED_GPU0_OK);
  CHECK(w_seed_gpu0_verify(&program, &output, &storage.result));

  static const char invalid_utf8[] = "\xc0";
  functions[1].name = invalid_utf8;
  functions[1].name_length = sizeof(invalid_utf8) - 1u;
  const test_storage before = storage;
  CHECK(w_seed_gpu0_run(&program, &output, &storage.result) ==
        W_SEED_GPU0_INCONSISTENT);
  CHECK(memcmp(&storage, &before, sizeof(storage)) == 0);
  return true;
}

static bool test_reject_effects(void) {
  const uint32_t effects[] = {
      W_SEED_GPU0_EFFECT_CAPTURE,
      W_SEED_GPU0_EFFECT_SUSPENSION,
      W_SEED_GPU0_EFFECT_RECURSION,
      W_SEED_GPU0_EFFECT_HOST_IO,
      W_SEED_GPU0_EFFECT_DYNAMIC_DISPATCH,
      W_SEED_GPU0_EFFECT_HOST_FFI,
  };
  for (size_t effect_index = 0u;
       effect_index < sizeof(effects) / sizeof(effects[0]); effect_index += 1u) {
    w_seed_gpu0_function
        functions[W_SEED_GPU0_EVIDENCE_FUNCTION_CAPACITY];
    (void)memcpy(functions, FUNCTIONS, sizeof(functions));
    functions[1].effects = effects[effect_index];
    w_seed_gpu0_program program = fixture_program();
    program.functions = functions;
    test_storage storage;
    initialize_storage(&storage, 0xa7u);
    const test_storage before = storage;
    const w_seed_gpu0_output output = output_for(&storage);
    w_seed_gpu0_measurement measurement;
    (void)memset(&measurement, 0x6bu, sizeof(measurement));
    const w_seed_gpu0_measurement measurement_before = measurement;
    CHECK(w_seed_gpu0_measure(&program, &measurement) ==
          W_SEED_GPU0_UNSUPPORTED);
    CHECK(memcmp(&measurement, &measurement_before, sizeof(measurement)) == 0);
    CHECK(w_seed_gpu0_run(&program, &output, &storage.result) ==
          W_SEED_GPU0_UNSUPPORTED);
    CHECK(memcmp(&storage, &before, sizeof(storage)) == 0);

    (void)memcpy(functions, FUNCTIONS, sizeof(functions));
    functions[0].effects = effects[effect_index];
    program.functions = functions;
    initialize_storage(&storage, 0xa7u);
    const test_storage host_before = storage;
    CHECK(w_seed_gpu0_run(&program, &output, &storage.result) ==
          W_SEED_GPU0_UNSUPPORTED);
    CHECK(memcmp(&storage, &host_before, sizeof(storage)) == 0);
  }
  return true;
}

static bool test_reject_shape_capacity_range_and_alias(void) {
  const w_seed_gpu0_program base = fixture_program();
  test_storage storage;
  initialize_storage(&storage, 0x5au);
  const w_seed_gpu0_output output = output_for(&storage);

  w_seed_gpu0_program capacity = base;
  capacity.function_capacity = 1u;
  const test_storage capacity_before = storage;
  CHECK(w_seed_gpu0_run(&capacity, &output, &storage.result) ==
        W_SEED_GPU0_CAPACITY);
  CHECK(memcmp(&storage, &capacity_before, sizeof(storage)) == 0);

  w_seed_gpu0_operation
      operations[W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY];
  (void)memcpy(operations, OPERATIONS, sizeof(operations));
  operations[6].destination.offset = 1u;
  w_seed_gpu0_program range = base;
  range.operations = operations;
  const test_storage range_before = storage;
  CHECK(w_seed_gpu0_run(&range, &output, &storage.result) ==
        W_SEED_GPU0_RANGE);
  CHECK(memcmp(&storage, &range_before, sizeof(storage)) == 0);

  (void)memcpy(operations, OPERATIONS, sizeof(operations));
  operations[6].source = DEVICE_RESULT;
  w_seed_gpu0_program alias = base;
  alias.operations = operations;
  const test_storage alias_before = storage;
  CHECK(w_seed_gpu0_run(&alias, &output, &storage.result) ==
        W_SEED_GPU0_ALIAS);
  CHECK(memcmp(&storage, &alias_before, sizeof(storage)) == 0);

  w_seed_gpu0_output small = output;
  small.host_capacity = 1u;
  const test_storage small_before = storage;
  CHECK(w_seed_gpu0_run(&base, &small, &storage.result) ==
        W_SEED_GPU0_CAPACITY);
  CHECK(memcmp(&storage, &small_before, sizeof(storage)) == 0);
  small = output;
  small.device_capacity = 1u;
  CHECK(w_seed_gpu0_run(&base, &small, &storage.result) ==
        W_SEED_GPU0_CAPACITY);
  CHECK(memcmp(&storage, &small_before, sizeof(storage)) == 0);

  w_seed_gpu0_output invalid = output;
  invalid.host_artifact = NULL;
  const test_storage invalid_before = storage;
  CHECK(w_seed_gpu0_run(&base, &invalid, &storage.result) ==
        W_SEED_GPU0_INVALID_ARGUMENT);
  CHECK(memcmp(&storage, &invalid_before, sizeof(storage)) == 0);
  invalid = output;
  invalid.device_artifact = NULL;
  CHECK(w_seed_gpu0_run(&base, &invalid, &storage.result) ==
        W_SEED_GPU0_INVALID_ARGUMENT);
  CHECK(memcmp(&storage, &invalid_before, sizeof(storage)) == 0);
  invalid = output;
  invalid.host_result = NULL;
  CHECK(w_seed_gpu0_run(&base, &invalid, &storage.result) ==
        W_SEED_GPU0_INVALID_ARGUMENT);
  CHECK(memcmp(&storage, &invalid_before, sizeof(storage)) == 0);
  invalid = output;
  invalid.device_result = NULL;
  CHECK(w_seed_gpu0_run(&base, &invalid, &storage.result) ==
        W_SEED_GPU0_INVALID_ARGUMENT);
  CHECK(memcmp(&storage, &invalid_before, sizeof(storage)) == 0);

  (void)memcpy(operations, OPERATIONS, sizeof(operations));
  operations[6].destination.offset = UINT32_MAX;
  w_seed_gpu0_program overflow = base;
  overflow.operations = operations;
  const test_storage overflow_before = storage;
  CHECK(w_seed_gpu0_run(&overflow, &output, &storage.result) ==
        W_SEED_GPU0_RANGE);
  CHECK(memcmp(&storage, &overflow_before, sizeof(storage)) == 0);

  w_seed_gpu0_output same_artifact = output;
  same_artifact.device_artifact = same_artifact.host_artifact;
  const test_storage output_alias_before = storage;
  CHECK(w_seed_gpu0_run(&base, &same_artifact, &storage.result) ==
        W_SEED_GPU0_ALIAS);
  CHECK(memcmp(&storage, &output_alias_before, sizeof(storage)) == 0);

  w_seed_gpu0_output same_result = output;
  same_result.host_result = same_result.device_result;
  const test_storage slot_alias_before = storage;
  CHECK(w_seed_gpu0_run(&base, &same_result, &storage.result) ==
        W_SEED_GPU0_ALIAS);
  CHECK(memcmp(&storage, &slot_alias_before, sizeof(storage)) == 0);

  CHECK(w_seed_gpu0_measure(
            &base, (w_seed_gpu0_measurement *)(void *)&OPERATIONS[0]) ==
        W_SEED_GPU0_ALIAS);

  w_seed_gpu0_output aliased_descriptor = output;
  const w_seed_gpu0_output descriptor_before = aliased_descriptor;
  CHECK(w_seed_gpu0_run(
            &base, &aliased_descriptor,
            (w_seed_gpu0_result *)(void *)&aliased_descriptor) ==
        W_SEED_GPU0_ALIAS);
  CHECK(memcmp(&aliased_descriptor, &descriptor_before,
               sizeof(aliased_descriptor)) == 0);
  return true;
}

static bool test_transactional_verify_and_input_alias(void) {
  const w_seed_gpu0_program base = fixture_program();
  test_storage storage;
  initialize_storage(&storage, 0x8du);
  const w_seed_gpu0_output output = output_for(&storage);
  CHECK(w_seed_gpu0_run(&base, &output, &storage.result) == W_SEED_GPU0_OK);
  CHECK(w_seed_gpu0_verify(&base, &output, &storage.result));

  const uint8_t saved_host_byte = storage.host_artifact[0];
  storage.host_artifact[0] ^= 0x01u;
  CHECK(!w_seed_gpu0_verify(&base, &output, &storage.result));
  storage.host_artifact[0] = saved_host_byte;
  const w_seed_gpu0_status saved_status = storage.result.status;
  storage.result.status = W_SEED_GPU0_INVALID_SCHEMA;
  CHECK(!w_seed_gpu0_verify(&base, &output, &storage.result));
  storage.result.status = saved_status;
  CHECK(w_seed_gpu0_verify(&base, &output, &storage.result));
  storage.result.phase_count = 9u;
  CHECK(!w_seed_gpu0_verify(&base, &output, &storage.result));
  storage.result.phase_count = 8u;
  CHECK(w_seed_gpu0_verify(&base, &output, &storage.result));

  w_seed_gpu0_result *aliased_result = (w_seed_gpu0_result *)(void *)&base;
  const test_storage before = storage;
  CHECK(w_seed_gpu0_run(&base, &output, aliased_result) ==
        W_SEED_GPU0_ALIAS);
  CHECK(memcmp(&storage, &before, sizeof(storage)) == 0);
  CHECK(all_bytes_are(storage.host_artifact,
                      storage.result.measurement.host_artifact_bytes, 0x8du) ==
        false);

  const w_seed_gpu0_output *aliased_output =
      (const w_seed_gpu0_output *)(const void *)&base;
  const test_storage output_before = storage;
  CHECK(w_seed_gpu0_run(&base, aliased_output, &storage.result) ==
        W_SEED_GPU0_ALIAS);
  CHECK(memcmp(&storage, &output_before, sizeof(storage)) == 0);
  return true;
}

int main(void) {
  if (!test_run_measure_verify() || !test_deterministic_outputs() ||
      !test_data_driven_payload() ||
      !test_signed_payload_literals() ||
      !test_capacity_independent_identity() ||
      !test_unicode_identifiers() ||
      !test_reject_effects() ||
      !test_reject_shape_capacity_range_and_alias() ||
      !test_transactional_verify_and_input_alias())
    return 1;
  (void)fputs("GPU0 target-neutral logical witness: passed\n", stdout);
  return 0;
}
