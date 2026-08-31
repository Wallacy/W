#include "w_seed_hlo1.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      (void)fprintf(stderr, "hlo1 check failed: %s (%s:%d)\n", #condition, \
                    __FILE__, __LINE__);                                       \
      return false;                                                             \
    }                                                                           \
  } while (0)

static const char EXPECTED_C[] =
    "/* w-seed-hlo1-1 */\n"
    "#include <stdio.h>\n"
    "#if defined(_WIN32)\n"
    "#include <fcntl.h>\n"
    "#include <io.h>\n"
    "#endif\n"
    "\n"
    "int main(void) {\n"
    "  static const unsigned char w_output[] = {\n"
    "    0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x2c, 0x20, 0x77, 0x6f, 0x72, 0x6c, "
    "0x64, 0x21, 0x0a\n"
    "  };\n"
    "#if defined(_WIN32)\n"
    "  if (_setmode(_fileno(stdout), _O_BINARY) == -1) {\n"
    "    return 1;\n"
    "  }\n"
    "#endif\n"
    "  if (fwrite(w_output, 1u, sizeof(w_output), stdout) !=\n"
    "      sizeof(w_output)) {\n"
    "    return 1;\n"
    "  }\n"
    "  if (fflush(stdout) != 0) {\n"
    "    return 1;\n"
    "  }\n"
    "  return 0;\n"
    "}\n";

static const uint8_t EXPECTED_C_SHA256[32] = {
    0x7bu, 0x75u, 0xdbu, 0x5eu, 0xafu, 0xbcu, 0x48u, 0x14u,
    0x46u, 0xc5u, 0x1fu, 0xa7u, 0xc8u, 0x19u, 0x03u, 0x80u,
    0xbcu, 0x7eu, 0x2au, 0xc2u, 0x06u, 0x89u, 0xaeu, 0xe9u,
    0x1fu, 0x5au, 0xb9u, 0xe7u, 0x6cu, 0x00u, 0x73u, 0x32u,
};

static void set_text(char *destination, size_t capacity, const char *value) {
  const size_t length = strlen(value);
  (void)memset(destination, 0, capacity);
  (void)memcpy(destination, value, length);
}

static w_seed_hlo0_plan canonical_plan(void) {
  w_seed_hlo0_plan plan;
  (void)memset(&plan, 0, sizeof(plan));
  set_text(plan.schema, sizeof(plan.schema), "w-seed-hlo0-1");
  set_text(plan.profile, sizeof(plan.profile), "native-process@1");
  set_text(plan.slot, sizeof(plan.slot), ".default");
  set_text(plan.entry_target, sizeof(plan.entry_target), "main");
  set_text(plan.handler, sizeof(plan.handler), "main");
  set_text(plan.callee, sizeof(plan.callee), "print");
  set_text(plan.requirement, sizeof(plan.requirement), "Console");
  plan.zero_parameters = true;
  plan.unit_return = true;
  plan.newline_policy = W_SEED_HLO0_NEWLINE_ADD_LF;
  static const uint8_t payload[] = "Hello, world!";
  (void)memcpy(plan.payload, payload, sizeof(payload) - 1u);
  plan.payload_bytes = sizeof(payload) - 1u;
  plan.stdout_bytes = sizeof(payload);
  plan.exit_success = true;
  static const uint8_t line_feed = 0x0au;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, plan.payload, plan.payload_bytes);
  w_seed_sha256_update(&state, &line_feed, 1u);
  w_seed_sha256_final(&state, plan.stdout_sha256);
  return plan;
}

static bool emit_canonical(const w_seed_hlo0_plan *plan, uint8_t *output,
                           size_t capacity, w_seed_hlo1_result *result) {
  const w_seed_hlo1_output destination = {output, capacity};
  return w_seed_hlo1_emit(plan, &destination, result) == W_SEED_HLO1_OK;
}

static bool test_exact_and_deterministic(void) {
  const w_seed_hlo0_plan plan = canonical_plan();
  w_seed_hlo1_counts counts;
  w_seed_hlo1_result measured;
  CHECK(w_seed_hlo1_measure(&plan, &counts, &measured) == W_SEED_HLO1_OK);
  CHECK(counts.c_bytes == sizeof(EXPECTED_C) - 1u);
  CHECK(measured.required.c_bytes == counts.c_bytes);
  CHECK(measured.written.c_bytes == 0u);
  uint8_t first[W_SEED_HLO1_MAX_C_BYTES];
  uint8_t second[W_SEED_HLO1_MAX_C_BYTES];
  (void)memset(first, 0xa5, sizeof(first));
  (void)memset(second, 0x5a, sizeof(second));
  w_seed_hlo1_result first_result;
  w_seed_hlo1_result second_result;
  CHECK(emit_canonical(&plan, first, sizeof(first), &first_result));
  CHECK(emit_canonical(&plan, second, sizeof(second), &second_result));
  CHECK(first_result.required.c_bytes == counts.c_bytes &&
        first_result.written.c_bytes == counts.c_bytes);
  CHECK(second_result.required.c_bytes == counts.c_bytes &&
        second_result.written.c_bytes == counts.c_bytes);
  CHECK(memcmp(first, EXPECTED_C, counts.c_bytes) == 0);
  CHECK(memcmp(first, second, counts.c_bytes) == 0);
  CHECK(first[counts.c_bytes] == 0xa5u && second[counts.c_bytes] == 0x5au);
  CHECK(memcmp(first_result.c_sha256, second_result.c_sha256,
               sizeof(first_result.c_sha256)) == 0);
  uint8_t expected_digest[sizeof(EXPECTED_C_SHA256)];
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, (const uint8_t *)EXPECTED_C,
                       sizeof(EXPECTED_C) - 1u);
  w_seed_sha256_final(&state, expected_digest);
  CHECK(memcmp(expected_digest, EXPECTED_C_SHA256,
               sizeof(EXPECTED_C_SHA256)) == 0);
  CHECK(memcmp(measured.c_sha256, EXPECTED_C_SHA256,
               sizeof(EXPECTED_C_SHA256)) == 0);
  CHECK(memcmp(first_result.c_sha256, EXPECTED_C_SHA256,
               sizeof(EXPECTED_C_SHA256)) == 0);
  CHECK(memcmp(second_result.c_sha256, EXPECTED_C_SHA256,
               sizeof(EXPECTED_C_SHA256)) == 0);
  return true;
}

static bool output_is(const uint8_t *output, size_t length, uint8_t value) {
  if (output == NULL) return false;
  for (size_t index = 0u; index < length; index += 1u)
    if (output[index] != value) return false;
  return true;
}

static bool test_exact_capacity(void) {
  const w_seed_hlo0_plan plan = canonical_plan();
  uint8_t output[W_SEED_HLO1_MAX_C_BYTES];
  w_seed_hlo1_result result;
  (void)memset(output, 0x6d, sizeof(output));
  const size_t required = sizeof(EXPECTED_C) - 1u;
  CHECK(emit_canonical(&plan, output, required, &result));
  CHECK(result.required.c_bytes == required &&
        result.written.c_bytes == required);
  CHECK(memcmp(output, EXPECTED_C, required) == 0);
  CHECK(output[required] == 0x6du &&
        output[sizeof(output) - 1u] == 0x6du);
  return true;
}

static bool test_capacity_and_no_partial_output(void) {
  const w_seed_hlo0_plan plan = canonical_plan();
  uint8_t output[W_SEED_HLO1_MAX_C_BYTES];
  w_seed_hlo1_result result;
  (void)memset(&result, 0x2bu, sizeof(result));
  const w_seed_hlo1_result result_snapshot = result;
  (void)memset(output, 0xc3, sizeof(output));
  CHECK(w_seed_hlo1_emit(
            &plan,
            &(w_seed_hlo1_output){output, sizeof(EXPECTED_C) - 2u}, &result) ==
        W_SEED_HLO1_CAPACITY);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0xc3u);
  CHECK(memcmp(&result, &result_snapshot, sizeof(result)) == 0);
  CHECK(w_seed_hlo1_emit(&plan, &(w_seed_hlo1_output){NULL, 0u}, &result) ==
        W_SEED_HLO1_CAPACITY);
  CHECK(w_seed_hlo1_emit(&plan, NULL, &result) == W_SEED_HLO1_CAPACITY);
  CHECK(w_seed_hlo1_emit(&plan, &(w_seed_hlo1_output){output, 0u}, &result) ==
        W_SEED_HLO1_CAPACITY);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0xc3u);
  CHECK(memcmp(&result, &result_snapshot, sizeof(result)) == 0);
  CHECK(w_seed_hlo1_measure(&plan, NULL, &result) ==
        W_SEED_HLO1_INVALID_PLAN);
  CHECK(w_seed_hlo1_measure(&plan, &(w_seed_hlo1_counts){0u}, NULL) ==
        W_SEED_HLO1_INVALID_PLAN);
  return true;
}

static bool expect_invalid_plan_unchanged(w_seed_hlo0_plan *plan) {
  uint8_t output[W_SEED_HLO1_MAX_C_BYTES];
  w_seed_hlo1_result result;
  (void)memset(&result, 0x4a, sizeof(result));
  const w_seed_hlo1_result result_snapshot = result;
  (void)memset(output, 0x7e, sizeof(output));
  CHECK(emit_canonical(plan, output, sizeof(output), &result) == false);
  CHECK(memcmp(&result, &result_snapshot, sizeof(result)) == 0);
  CHECK(output_is(output, sizeof(output), 0x7eu));
  return true;
}

static bool test_alias_and_plan_mutations(void) {
  w_seed_hlo0_plan plan = canonical_plan();
  w_seed_hlo0_plan plan_snapshot = plan;
  uint8_t output[W_SEED_HLO1_MAX_C_BYTES];
  w_seed_hlo1_result result;
  (void)memset(output, 0x7e, sizeof(output));
  CHECK(w_seed_hlo1_emit(
            &plan, &(w_seed_hlo1_output){(uint8_t *)&plan,
                                         W_SEED_HLO1_MAX_C_BYTES}, &result) ==
        W_SEED_HLO1_ALIAS);
  CHECK(memcmp(&plan, &plan_snapshot, sizeof(plan)) == 0);

  w_seed_hlo1_counts counts_snapshot;
  w_seed_hlo1_result result_snapshot;
  (void)memset(&counts_snapshot, 0x31, sizeof(counts_snapshot));
  (void)memset(&result_snapshot, 0x42, sizeof(result_snapshot));
  CHECK(w_seed_hlo1_measure(
            &plan, (w_seed_hlo1_counts *)&plan, &result_snapshot) ==
        W_SEED_HLO1_ALIAS);
  CHECK(memcmp(&plan, &plan_snapshot, sizeof(plan)) == 0);
  CHECK(output_is((const uint8_t *)&result_snapshot,
                  sizeof(result_snapshot), 0x42u));

  (void)memset(&counts_snapshot, 0x31, sizeof(counts_snapshot));
  result_snapshot = (w_seed_hlo1_result){0};
  (void)memset(&result_snapshot, 0x52, sizeof(result_snapshot));
  CHECK(w_seed_hlo1_measure(
            &plan, &counts_snapshot, (w_seed_hlo1_result *)&plan) ==
        W_SEED_HLO1_ALIAS);
  CHECK(memcmp(&plan, &plan_snapshot, sizeof(plan)) == 0);
  CHECK(output_is((const uint8_t *)&counts_snapshot,
                  sizeof(counts_snapshot), 0x31u));

  uint8_t records[sizeof(w_seed_hlo1_result)];
  (void)memset(records, 0x63, sizeof(records));
  CHECK(w_seed_hlo1_measure(
            &plan, (w_seed_hlo1_counts *)records,
            (w_seed_hlo1_result *)records) == W_SEED_HLO1_ALIAS);
  CHECK(output_is(records, sizeof(records), 0x63u));

  (void)memset(&result_snapshot, 0x74, sizeof(result_snapshot));
  const w_seed_hlo1_output output_alias_result = {
      (uint8_t *)&result_snapshot, W_SEED_HLO1_MAX_C_BYTES};
  CHECK(w_seed_hlo1_emit(&plan, &output_alias_result, &result_snapshot) ==
        W_SEED_HLO1_ALIAS);
  CHECK(output_is((const uint8_t *)&result_snapshot,
                  sizeof(result_snapshot), 0x74u));

  (void)memset(&result_snapshot, 0x85, sizeof(result_snapshot));
  CHECK(w_seed_hlo1_emit(&plan, (w_seed_hlo1_output *)&result_snapshot,
                         &result_snapshot) == W_SEED_HLO1_ALIAS);
  CHECK(output_is((const uint8_t *)&result_snapshot,
                  sizeof(result_snapshot), 0x85u));

  (void)memset(&result_snapshot, 0x96, sizeof(result_snapshot));
  CHECK(w_seed_hlo1_emit(&plan, (const w_seed_hlo1_output *)&plan,
                         &result_snapshot) == W_SEED_HLO1_ALIAS);
  CHECK(memcmp(&plan, &plan_snapshot, sizeof(plan)) == 0);
  CHECK(output_is((const uint8_t *)&result_snapshot,
                  sizeof(result_snapshot), 0x96u));

  (void)memset(&result_snapshot, 0xa7, sizeof(result_snapshot));
  uint8_t output_snapshot[sizeof(output)];
  (void)memcpy(output_snapshot, output, sizeof(output));
  CHECK(w_seed_hlo1_emit(&plan, &(w_seed_hlo1_output){output, sizeof(output)},
                         (w_seed_hlo1_result *)&plan) == W_SEED_HLO1_ALIAS);
  CHECK(memcmp(&plan, &plan_snapshot, sizeof(plan)) == 0);
  CHECK(memcmp(output, output_snapshot, sizeof(output)) == 0);

  w_seed_hlo1_output output_descriptor;
  (void)memset(&output_descriptor, 0xb8, sizeof(output_descriptor));
  (void)memset(&result_snapshot, 0xc9, sizeof(result_snapshot));
  output_descriptor.bytes = (uint8_t *)&output_descriptor;
  output_descriptor.capacity = sizeof(output);
  const w_seed_hlo1_output output_descriptor_snapshot = output_descriptor;
  CHECK(w_seed_hlo1_emit(&plan, &output_descriptor, &result_snapshot) ==
        W_SEED_HLO1_ALIAS);
  CHECK(memcmp(&output_descriptor, &output_descriptor_snapshot,
               sizeof(output_descriptor)) == 0);
  CHECK(output_is((const uint8_t *)&result_snapshot,
                  sizeof(result_snapshot), 0xc9u));

  plan = canonical_plan();
  plan.exit_success = false;
  CHECK(expect_invalid_plan_unchanged(&plan));
  plan = canonical_plan();
  plan.stdout_bytes = 13u;
  CHECK(expect_invalid_plan_unchanged(&plan));
  plan = canonical_plan();
  plan.payload_bytes = 12u;
  CHECK(expect_invalid_plan_unchanged(&plan));
  plan = canonical_plan();
  plan.is_async = true;
  CHECK(expect_invalid_plan_unchanged(&plan));
  plan = canonical_plan();
  plan.is_throws = true;
  CHECK(expect_invalid_plan_unchanged(&plan));
  plan = canonical_plan();
  plan.is_unsafe = true;
  CHECK(expect_invalid_plan_unchanged(&plan));
  plan = canonical_plan();
  plan.has_borrow_clause = true;
  CHECK(expect_invalid_plan_unchanged(&plan));
  plan = canonical_plan();
  set_text(plan.callee, sizeof(plan.callee), "noop");
  CHECK(expect_invalid_plan_unchanged(&plan));
  plan = canonical_plan();
  (void)memset(plan.callee, 'x', sizeof(plan.callee));
  CHECK(expect_invalid_plan_unchanged(&plan));

  plan = canonical_plan();
  plan.payload[0] = 0x58u;
  CHECK(expect_invalid_plan_unchanged(&plan));

  plan = canonical_plan();
  plan.stdout_sha256[0] ^= 0xffu;
  CHECK(expect_invalid_plan_unchanged(&plan));

  plan = canonical_plan();
  (void)memset(plan.schema, 'x', sizeof(plan.schema));
  CHECK(expect_invalid_plan_unchanged(&plan));

  plan = canonical_plan();
  plan.newline_policy = (w_seed_hlo0_newline_policy)99;
  CHECK(expect_invalid_plan_unchanged(&plan));

  CHECK(w_seed_hlo1_emit(NULL, &(w_seed_hlo1_output){output, sizeof(output)},
                         &result) == W_SEED_HLO1_INVALID_PLAN);
  CHECK(w_seed_hlo1_emit(&plan, &(w_seed_hlo1_output){output, sizeof(output)},
                         NULL) == W_SEED_HLO1_INVALID_PLAN);

  plan = canonical_plan();
  plan_snapshot = plan;
  CHECK(w_seed_hlo1_measure(&plan, NULL,
                            (w_seed_hlo1_result *)&plan) ==
        W_SEED_HLO1_INVALID_PLAN);
  CHECK(memcmp(&plan, &plan_snapshot, sizeof(plan)) == 0);

  const w_seed_hlo1_output short_adjacent_to_plan = {
      (uint8_t *)&plan + sizeof(plan), sizeof(EXPECTED_C) - 2u};
  (void)memset(output, 0xdau, sizeof(output));
  (void)memset(&result, 0xebu, sizeof(result));
  const w_seed_hlo1_result adjacent_result_snapshot = result;
  CHECK(w_seed_hlo1_emit(&plan, &short_adjacent_to_plan, &result) ==
        W_SEED_HLO1_CAPACITY);
  CHECK(memcmp(&result, &adjacent_result_snapshot, sizeof(result)) == 0);
  const w_seed_hlo1_output zero_adjacent_to_result = {
      (uint8_t *)&result + sizeof(result), 0u};
  CHECK(w_seed_hlo1_emit(&plan, &zero_adjacent_to_result, &result) ==
        W_SEED_HLO1_CAPACITY);
  CHECK(memcmp(&result, &adjacent_result_snapshot, sizeof(result)) == 0);
  return true;
}

int main(void) {
  if (!test_exact_and_deterministic()) return 1;
  if (!test_exact_capacity()) return 1;
  if (!test_capacity_and_no_partial_output()) return 1;
  if (!test_alias_and_plan_mutations()) return 1;
  (void)puts("seed HLO1: deterministic conservative C emitter and all-or-nothing barriers passed");
  return 0;
}
