#include "w_seed_accelerated_binding0.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "w_seed_sha256.h"

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      (void)fprintf(stderr, "accelerated binding check failed: %s (%s:%d)\n",  \
                    #condition, __FILE__, __LINE__);                            \
      return false;                                                             \
    }                                                                           \
  } while (0)

static const char ACCINV_SEMANTIC_TAG[] =
    "w-seed-accelerated-invocation0-semantic-1";
static const char ACCINV_PROVENANCE_TAG[] =
    "w-seed-accelerated-invocation0-provenance-1";

typedef struct {
  w_seed_accelerated_invocation0_record record;
  uint8_t text[64];
  w_seed_accelerated_invocation0_program program;
  w_seed_accelerated_invocation0_result result;
} invocation_fixture;

typedef struct {
  w_seed_accelerated_binding0_record relation[1];
  uint8_t text[512];
} binding_storage;

static invocation_fixture invocation;
static binding_storage binding;
static binding_storage alternate;

static bool all_bytes_equal(const void *data, size_t bytes, uint8_t value) {
  const uint8_t *cursor = (const uint8_t *)data;
  if (bytes != 0u && data == NULL) return false;
  for (size_t index = 0u; index < bytes; index += 1u)
    if (cursor[index] != value) return false;
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

static void hash_bytes(w_seed_sha256_state *state, const uint8_t *bytes,
                       size_t count) {
  hash_u64(state, (uint64_t)count);
  if (count != 0u) w_seed_sha256_update(state, bytes, count);
}

static void hash_text(w_seed_sha256_state *state, const uint8_t *bytes,
                      size_t count) {
  hash_bytes(state, bytes, count);
}

static void hash_span(w_seed_sha256_state *state, w_seed_span span) {
  hash_u64(state, (uint64_t)span.start_byte);
  hash_u64(state, (uint64_t)span.end_byte);
}

static void fill_digest(uint8_t digest[32], uint8_t seed) {
  for (size_t index = 0u; index < 32u; index += 1u)
    digest[index] = (uint8_t)(seed + (uint8_t)index);
}

static bool make_verified_invocation(void) {
  memset(&invocation, 0, sizeof(invocation));
  static const uint8_t text[] = ".inferencekernelshellokernel";
  memcpy(invocation.text, text, sizeof(text) - 1u);
  w_seed_accelerated_invocation0_record *record = &invocation.record;
  *record = (w_seed_accelerated_invocation0_record){
      .frontend_module_index = 0u,
      .frontend_owner_function_index = 0u,
      .frontend_accelerator_module_index = 0u,
      .frontend_accelerator_kernel_index = 0u,
      .gpu_module_index = 0u,
      .gpu_kernel_index = 0u,
      .source_launch_expression = 1u,
      .source_call_expression = 0u,
      .source_await_expression = 2u,
      .source_launch_binding_statement = 0u,
      .source_result_binding_statement = 1u,
      .result_type_index = 0u,
      .kernel_return_type_index = 0u,
      .domain_index = 0u,
      .domain_kind = W_SEED_FRONTEND_DOMAIN_ACCELERATED,
      .submission = W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT,
      .domain_capabilities = W_SEED_FRONTEND_DOMAIN_CAPABILITY_DEVICE,
      .domain_maximum = 4u,
      .result_bit_width = 32u,
      .result_is_signed = true,
      .domain_name_offset = 0u,
      .domain_name_bytes = 10u,
      .module_name_offset = 10u,
      .module_name_bytes = 7u,
      .kernel_label_offset = 17u,
      .kernel_label_bytes = 5u,
      .function_name_offset = 22u,
      .function_name_bytes = 6u,
      .launch_span = {20u, 54u},
      .call_span = {40u, 53u},
      .await_span = {70u, 82u},
      .launch_binding_span = {16u, 55u},
      .result_binding_span = {60u, 83u},
  };
  invocation.program = (w_seed_accelerated_invocation0_program){
      .invocations = &invocation.record,
      .invocation_count = 1u,
      .invocation_capacity = 1u,
      .text = invocation.text,
      .text_bytes = sizeof(text) - 1u,
      .text_capacity = sizeof(invocation.text),
      .frontend_module_count = 1u,
      .frontend_function_count = 1u,
      .frontend_statement_count = 2u,
      .frontend_expression_count = 3u,
      .frontend_type_count = 1u,
      .frontend_domain_count = 1u,
      .frontend_accelerator_module_count = 1u,
      .frontend_accelerator_kernel_count = 1u,
      .gpu_module_count = 1u,
      .gpu_kernel_count = 1u,
  };
  fill_digest(invocation.program.frontend_receipt_digest, UINT8_C(0x10));
  fill_digest(invocation.program.gpu_semantic_digest, UINT8_C(0x40));
  fill_digest(invocation.program.gpu_provenance_digest, UINT8_C(0x70));

  w_seed_sha256_state semantic;
  w_seed_sha256_state provenance;
  w_seed_sha256_init(&semantic);
  w_seed_sha256_init(&provenance);
  hash_bytes(&semantic, (const uint8_t *)ACCINV_SEMANTIC_TAG,
             sizeof(ACCINV_SEMANTIC_TAG) - 1u);
  hash_bytes(&provenance, (const uint8_t *)ACCINV_PROVENANCE_TAG,
             sizeof(ACCINV_PROVENANCE_TAG) - 1u);
  hash_u64(&semantic, 1u);
  hash_u64(&semantic, sizeof(text) - 1u);
  hash_u64(&provenance, 1u);
  hash_u64(&provenance, sizeof(text) - 1u);
  hash_u32(&semantic, (uint32_t)record->domain_kind);
  hash_u32(&semantic, (uint32_t)record->submission);
  hash_u32(&semantic, record->domain_capabilities);
  hash_u32(&semantic, record->domain_maximum);
  hash_text(&semantic, invocation.text + record->domain_name_offset,
            record->domain_name_bytes);
  hash_text(&semantic, invocation.text + record->module_name_offset,
            record->module_name_bytes);
  hash_text(&semantic, invocation.text + record->kernel_label_offset,
            record->kernel_label_bytes);
  hash_text(&semantic, invocation.text + record->function_name_offset,
            record->function_name_bytes);
  hash_u32(&semantic, record->result_bit_width);
  hash_u32(&semantic, 1u);
  hash_bytes(&semantic, invocation.program.gpu_semantic_digest, 32u);

  hash_u32(&provenance, record->frontend_module_index);
  hash_u32(&provenance, record->frontend_owner_function_index);
  hash_u32(&provenance, record->frontend_accelerator_module_index);
  hash_u32(&provenance, record->frontend_accelerator_kernel_index);
  hash_u32(&provenance, record->gpu_module_index);
  hash_u32(&provenance, record->gpu_kernel_index);
  hash_u32(&provenance, record->source_launch_expression);
  hash_u32(&provenance, record->source_call_expression);
  hash_u32(&provenance, record->source_await_expression);
  hash_u32(&provenance, record->source_launch_binding_statement);
  hash_u32(&provenance, record->source_result_binding_statement);
  hash_u32(&provenance, record->result_type_index);
  hash_u32(&provenance, record->kernel_return_type_index);
  hash_u32(&provenance, record->domain_index);
  hash_u32(&provenance, (uint32_t)record->domain_kind);
  hash_u32(&provenance, (uint32_t)record->submission);
  hash_u32(&provenance, record->domain_capabilities);
  hash_u32(&provenance, record->domain_maximum);
  hash_u32(&provenance, record->result_bit_width);
  hash_u32(&provenance, 1u);
  hash_span(&provenance, record->launch_span);
  hash_span(&provenance, record->call_span);
  hash_span(&provenance, record->await_span);
  hash_span(&provenance, record->launch_binding_span);
  hash_span(&provenance, record->result_binding_span);
  hash_bytes(&provenance, invocation.program.frontend_receipt_digest, 32u);
  hash_bytes(&provenance, invocation.program.gpu_semantic_digest, 32u);
  hash_bytes(&provenance, invocation.program.gpu_provenance_digest, 32u);

  uint8_t semantic_digest[32];
  uint8_t provenance_digest[32];
  w_seed_sha256_final(&semantic, semantic_digest);
  w_seed_sha256_update(&provenance, semantic_digest, 32u);
  w_seed_sha256_final(&provenance, provenance_digest);
  invocation.result = (w_seed_accelerated_invocation0_result){
      .status = W_SEED_ACCELERATED_INVOCATION0_OK,
      .required = {1u, sizeof(text) - 1u},
      .written = {1u, sizeof(text) - 1u},
      .frontend_module_count = 1u,
      .frontend_function_count = 1u,
      .frontend_statement_count = 2u,
      .frontend_expression_count = 3u,
      .frontend_type_count = 1u,
      .frontend_domain_count = 1u,
      .frontend_accelerator_module_count = 1u,
      .frontend_accelerator_kernel_count = 1u,
      .gpu_module_count = 1u,
      .gpu_kernel_count = 1u,
  };
  memcpy(invocation.result.schema,
         W_SEED_ACCELERATED_INVOCATION0_SCHEMA_VERSION,
         sizeof(invocation.result.schema));
  memcpy(invocation.result.frontend_schema, W_SEED_FRONTEND_SCHEMA_VERSION,
         sizeof(invocation.result.frontend_schema));
  memcpy(invocation.result.gpu_module_schema, W_SEED_GPU_MODULE_SCHEMA_VERSION,
         sizeof(invocation.result.gpu_module_schema));
  memcpy(invocation.result.frontend_receipt_digest,
         invocation.program.frontend_receipt_digest, 32u);
  memcpy(invocation.result.gpu_semantic_digest,
         invocation.program.gpu_semantic_digest, 32u);
  memcpy(invocation.result.gpu_provenance_digest,
         invocation.program.gpu_provenance_digest, 32u);
  memcpy(invocation.result.semantic_digest, semantic_digest, 32u);
  memcpy(invocation.result.provenance_digest, provenance_digest, 32u);
  return w_seed_accelerated_invocation0_verify(&invocation.program,
                                                &invocation.result);
}

static w_seed_accelerated_binding0_closed_profile make_profile(void) {
  w_seed_accelerated_binding0_closed_profile profile = {
      .schema = W_SEED_ACCELERATED_BINDING0_PROFILE_SCHEMA_VERSION,
      .schema_bytes =
          sizeof(W_SEED_ACCELERATED_BINDING0_PROFILE_SCHEMA_VERSION) - 1u,
      .root_identity = {"root:main", 9u},
      .domain_identity = {"inference", 9u},
      .descriptor_name = {"kernels", 7u},
      .kernel_label = {"hello", 5u},
      .module_identity = {"example.kernels@1", 17u},
      .artifact_identity = {"artifact:gpu0", 13u},
      .artifact_module_identity = {"example.kernels@1", 17u},
      .kernel_instance_identity = {"instance:hello:0", 16u},
      .artifact_target = {"nvptx64-nvidia-cuda", 19u},
      .device_target = {"nvptx64-nvidia-cuda", 19u},
      .provider_class = {"cuda", 4u},
      .queue_identity = {"queue:0", 7u},
      .device_identity = {"device:0", 8u},
      .provider_generation = {"generation:1", 12u},
      .bound_gpu_module_index = 0u,
      .bound_gpu_kernel_index = 0u,
      .profile_maximum = 8u,
      .root_maximum = 6u,
      .deployment_maximum = 3u,
      .limits = {5u, 4096u, 1024u, 1024u, 8u, 65536u, 8u, 8u},
      .submission = W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT,
      .numeric_mode = W_SEED_ACCELERATED_BINDING0_NUMERIC_REPRODUCIBLE,
      .fallback = W_SEED_ACCELERATED_BINDING0_FALLBACK_REJECT,
      .artifact_closed = true,
      .root_owned = true,
      .provider_resolved = true,
      .queue_device_match = true,
      .selected_instance_member = true,
  };
  fill_digest(profile.artifact_provider_abi_digest, UINT8_C(0x20));
  memcpy(profile.provider_abi_digest, profile.artifact_provider_abi_digest,
         32u);
  fill_digest(profile.artifact_instances_digest, UINT8_C(0x50));
  fill_digest(profile.profile_semantic_digest, UINT8_C(0x80));
  fill_digest(profile.profile_provenance_digest, UINT8_C(0xa0));
  fill_digest(profile.root_binding_digest, UINT8_C(0xc0));
  return profile;
}

static bool execute_binding(
    const w_seed_accelerated_binding0_closed_profile *profile,
    binding_storage *storage, w_seed_accelerated_binding0_program *program,
    w_seed_accelerated_binding0_result *result) {
  const w_seed_accelerated_binding0_input input = {
      &invocation.program, &invocation.result, profile};
  w_seed_accelerated_binding0_counts counts;
  memset(&counts, 0xa5, sizeof(counts));
  memset(result, 0xa5, sizeof(*result));
  CHECK(w_seed_accelerated_binding0_measure(&input, &counts, result) ==
        W_SEED_ACCELERATED_BINDING0_OK);
  CHECK(counts.relations == 1u && counts.text_bytes != 0u &&
        result->written.relations == 0u);
  memset(storage, 0xa5, sizeof(*storage));
  const w_seed_accelerated_binding0_output output = {
      storage->relation, 1u, storage->text, sizeof(storage->text)};
  CHECK(w_seed_accelerated_binding0_run(&input, &output, result) ==
        W_SEED_ACCELERATED_BINDING0_OK);
  CHECK(w_seed_accelerated_binding0_program_from_output(&output, result,
                                                        program));
  CHECK(w_seed_accelerated_binding0_verify(program, result));
  return true;
}

static bool negative_measure(
    const w_seed_accelerated_binding0_closed_profile *profile,
    w_seed_accelerated_binding0_status expected) {
  const w_seed_accelerated_binding0_input input = {
      &invocation.program, &invocation.result, profile};
  w_seed_accelerated_binding0_counts counts;
  w_seed_accelerated_binding0_result result;
  memset(&counts, 0x5a, sizeof(counts));
  memset(&result, 0x5a, sizeof(result));
  const w_seed_accelerated_binding0_counts before_counts = counts;
  const w_seed_accelerated_binding0_result before_result = result;
  CHECK(w_seed_accelerated_binding0_measure(&input, &counts, &result) ==
        expected);
  CHECK(memcmp(&counts, &before_counts, sizeof(counts)) == 0);
  CHECK(memcmp(&result, &before_result, sizeof(result)) == 0);
  return true;
}

static bool test_binding(void) {
  CHECK(make_verified_invocation());
  w_seed_accelerated_binding0_closed_profile profile = make_profile();
  w_seed_accelerated_binding0_program program;
  w_seed_accelerated_binding0_result result;
  CHECK(execute_binding(&profile, &binding, &program, &result));
  CHECK(program.relations[0].required_maximum == 4u &&
        program.relations[0].effective_maximum == 3u &&
        program.relations[0].limits.maximum_in_flight == 3u &&
        program.relations[0].result_bit_width == 32u &&
        program.relations[0].result_is_signed);

  w_seed_accelerated_binding0_closed_profile changed = profile;
  changed.queue_identity =
      (w_seed_accelerated_binding0_text){"queue:other", 11u};
  changed.device_identity =
      (w_seed_accelerated_binding0_text){"device:other", 12u};
  changed.provider_generation =
      (w_seed_accelerated_binding0_text){"generation:2", 12u};
  w_seed_accelerated_binding0_program changed_program;
  w_seed_accelerated_binding0_result changed_result;
  CHECK(execute_binding(&changed, &alternate, &changed_program,
                        &changed_result));
  CHECK(memcmp(result.semantic_digest, changed_result.semantic_digest, 32u) ==
        0);
  CHECK(memcmp(result.provenance_digest, changed_result.provenance_digest,
               32u) != 0);

  const w_seed_accelerated_binding0_input input = {
      &invocation.program, &invocation.result, &profile};
  w_seed_accelerated_binding0_result untouched_result;
  memset(&untouched_result, 0x5a, sizeof(untouched_result));
  const w_seed_accelerated_binding0_result before_result = untouched_result;
  binding_storage untouched;
  memset(&untouched, 0x5a, sizeof(untouched));
  w_seed_accelerated_binding0_output output = {
      untouched.relation, 0u, untouched.text, sizeof(untouched.text)};
  CHECK(w_seed_accelerated_binding0_run(&input, &output, &untouched_result) ==
        W_SEED_ACCELERATED_BINDING0_CAPACITY);
  CHECK(all_bytes_equal(&untouched, sizeof(untouched), 0x5a));
  CHECK(memcmp(&untouched_result, &before_result, sizeof(before_result)) == 0);
  output.relation_capacity = 1u;
  output.text_capacity = result.required.text_bytes - 1u;
  CHECK(w_seed_accelerated_binding0_run(&input, &output, &untouched_result) ==
        W_SEED_ACCELERATED_BINDING0_CAPACITY);
  CHECK(all_bytes_equal(&untouched, sizeof(untouched), 0x5a));
  output.text = (uint8_t *)(void *)untouched.relation;
  output.text_capacity = sizeof(untouched.relation);
  CHECK(w_seed_accelerated_binding0_run(&input, &output, &untouched_result) ==
        W_SEED_ACCELERATED_BINDING0_ALIAS);
  CHECK(all_bytes_equal(&untouched, sizeof(untouched), 0x5a));
  const w_seed_accelerated_binding0_closed_profile profile_before_alias =
      profile;
  output = (w_seed_accelerated_binding0_output){
      (w_seed_accelerated_binding0_record *)(void *)&profile, 1u,
      untouched.text, sizeof(untouched.text)};
  CHECK(w_seed_accelerated_binding0_run(&input, &output, &untouched_result) ==
        W_SEED_ACCELERATED_BINDING0_ALIAS);
  CHECK(memcmp(&profile, &profile_before_alias, sizeof(profile)) == 0);

  w_seed_accelerated_binding0_result alias_measure_result;
  memset(&alias_measure_result, 0x5a, sizeof(alias_measure_result));
  const w_seed_accelerated_binding0_result alias_measure_before =
      alias_measure_result;
  CHECK(w_seed_accelerated_binding0_measure(
            &input,
            (w_seed_accelerated_binding0_counts *)(void *)&alias_measure_result,
            &alias_measure_result) == W_SEED_ACCELERATED_BINDING0_ALIAS);
  CHECK(memcmp(&alias_measure_result, &alias_measure_before,
               sizeof(alias_measure_result)) == 0);

  w_seed_accelerated_binding0_closed_profile invalid = profile;
  invalid.fallback = W_SEED_ACCELERATED_BINDING0_FALLBACK_COMPATIBLE;
  CHECK(negative_measure(&invalid, W_SEED_ACCELERATED_BINDING0_UNSUPPORTED));
  invalid = profile;
  invalid.artifact_closed = false;
  CHECK(negative_measure(&invalid,
                         W_SEED_ACCELERATED_BINDING0_PROFILE_NOT_CLOSED));
  invalid = profile;
  invalid.domain_identity =
      (w_seed_accelerated_binding0_text){".inference", 10u};
  CHECK(negative_measure(&invalid, W_SEED_ACCELERATED_BINDING0_MISMATCH));
  invalid = profile;
  invalid.bound_gpu_kernel_index = 1u;
  CHECK(negative_measure(&invalid, W_SEED_ACCELERATED_BINDING0_MISMATCH));
  invalid = profile;
  invalid.deployment_maximum = 0u;
  CHECK(negative_measure(&invalid, W_SEED_ACCELERATED_BINDING0_RANGE));
  invalid = profile;
  invalid.provider_abi_digest[0] ^= UINT8_C(1);
  CHECK(negative_measure(&invalid, W_SEED_ACCELERATED_BINDING0_MISMATCH));
  static const char invalid_utf8[] = {(char)0xc0, (char)0x80};
  invalid = profile;
  invalid.provider_generation =
      (w_seed_accelerated_binding0_text){invalid_utf8, sizeof(invalid_utf8)};
  CHECK(negative_measure(&invalid, W_SEED_ACCELERATED_BINDING0_RANGE));

  w_seed_accelerated_binding0_record saved = binding.relation[0];
  binding.relation[0].effective_maximum += 1u;
  CHECK(!w_seed_accelerated_binding0_verify(&program, &result));
  binding.relation[0] = saved;
  const uint8_t saved_text = binding.text[0];
  binding.text[0] ^= UINT8_C(1);
  CHECK(!w_seed_accelerated_binding0_verify(&program, &result));
  binding.text[0] = saved_text;
  w_seed_accelerated_binding0_result forged_result = result;
  forged_result.semantic_digest[0] ^= UINT8_C(1);
  CHECK(!w_seed_accelerated_binding0_verify(&program, &forged_result));
  saved = binding.relation[0];
  binding.relation[0].queue_offset += 1u;
  CHECK(!w_seed_accelerated_binding0_verify(&program, &result));
  binding.relation[0] = saved;
  saved = binding.relation[0];
  binding.relation[0].result_bit_width = 64u;
  CHECK(!w_seed_accelerated_binding0_verify(&program, &result));
  binding.relation[0] = saved;
  forged_result = result;
  memset(forged_result.schema, 'x', sizeof(forged_result.schema));
  CHECK(!w_seed_accelerated_binding0_verify(&program, &forged_result));
  const w_seed_accelerated_binding0_output good_output = {
      binding.relation, 1u, binding.text, sizeof(binding.text)};
  const w_seed_accelerated_binding0_result result_before_alias = result;
  CHECK(!w_seed_accelerated_binding0_program_from_output(
      &good_output, &result,
      (w_seed_accelerated_binding0_program *)(void *)&result));
  CHECK(memcmp(&result, &result_before_alias, sizeof(result)) == 0);
  CHECK(w_seed_accelerated_binding0_verify(&program, &result));

  /* Profile and upstream owners can be released after the relation is copied. */
  memset(&profile, 0, sizeof(profile));
  memset(&invocation, 0, sizeof(invocation));
  CHECK(w_seed_accelerated_binding0_verify(&program, &result));
  return true;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    (void)fprintf(stderr, "usage: %s <accelerated-invocation-fixture>\n",
                  argv[0]);
    return 2;
  }
  FILE *fixture = fopen(argv[1], "rb");
  if (fixture == NULL) return 2;
  (void)fclose(fixture);
  if (!test_binding()) return 1;
  (void)printf("ACCBIND0 verified static root binding: PASS\n");
  (void)printf("ACCBIND0 budgets/digests/teardown/negative barriers: PASS\n");
  return 0;
}
