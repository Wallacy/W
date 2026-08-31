#include "w_seed_mlir0.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "w_seed_sha256.h"

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "mlir0 check failed: %s (%s:%d)\n", #condition, \
                    __FILE__, __LINE__);                                      \
      return false;                                                            \
    }                                                                          \
  } while (0)

static const w_seed_mlir0_target TARGET = {
    W_SEED_MLIR0_TARGET_X86_64_UNKNOWN_LINUX_GNU};

static void set_text(char *destination, size_t capacity, const char *value) {
  const size_t length = strlen(value);
  (void)memset(destination, 0, capacity);
  (void)memcpy(destination, value, length);
}

static void set_payload(w_seed_hlo0_plan *plan, const uint8_t *payload,
                        size_t payload_bytes) {
  (void)memset(plan->payload, 0, sizeof(plan->payload));
  if (payload_bytes != 0u) (void)memcpy(plan->payload, payload, payload_bytes);
  plan->payload_bytes = payload_bytes;
  plan->stdout_bytes = payload_bytes + 1u;
  static const uint8_t line_feed = 0x0au;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, plan->payload, plan->payload_bytes);
  w_seed_sha256_update(&state, &line_feed, 1u);
  w_seed_sha256_final(&state, plan->stdout_sha256);
}

static w_seed_hlo0_plan canonical_plan(void) {
  w_seed_hlo0_plan plan;
  (void)memset(&plan, 0, sizeof(plan));
  set_text(plan.schema, sizeof(plan.schema), W_SEED_HLO0_SCHEMA_VERSION);
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
  set_payload(&plan, payload, sizeof(payload) - 1u);
  plan.exit_success = true;
  return plan;
}

static bool contains_bytes(const uint8_t *bytes, size_t length,
                           const char *needle) {
  if (bytes == NULL || needle == NULL) return false;
  const size_t needle_length = strlen(needle);
  if (needle_length == 0u || needle_length > length) return false;
  for (size_t offset = 0u; offset + needle_length <= length; offset += 1u)
    if (memcmp(bytes + offset, needle, needle_length) == 0) return true;
  return false;
}

static bool emit(const w_seed_hlo0_plan *plan, const w_seed_mlir0_target *target,
                 uint8_t *bytes, size_t capacity,
                 w_seed_mlir0_result *result) {
  return w_seed_mlir0_emit(plan, target,
                           &(w_seed_mlir0_output){bytes, capacity}, result) ==
         W_SEED_MLIR0_OK;
}

static bool test_exact_and_deterministic(void) {
  const w_seed_hlo0_plan plan = canonical_plan();
  CHECK(w_seed_mlir0_target_is_supported(&TARGET));
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  CHECK(w_seed_mlir0_measure(&plan, &TARGET, &counts, &measured) ==
        W_SEED_MLIR0_OK);
  CHECK(counts.mlir_bytes > 0u && counts.mlir_bytes < W_SEED_MLIR0_MAX_BYTES);
  CHECK(measured.status == W_SEED_MLIR0_OK &&
        measured.required.mlir_bytes == counts.mlir_bytes &&
        measured.written.mlir_bytes == 0u);
  uint8_t first[W_SEED_MLIR0_MAX_BYTES];
  uint8_t second[W_SEED_MLIR0_MAX_BYTES];
  (void)memset(first, 0xa5u, sizeof(first));
  (void)memset(second, 0x5au, sizeof(second));
  w_seed_mlir0_result first_result;
  w_seed_mlir0_result second_result;
  CHECK(emit(&plan, &TARGET, first, sizeof(first), &first_result));
  CHECK(emit(&plan, &TARGET, second, sizeof(second), &second_result));
  CHECK(first_result.required.mlir_bytes == counts.mlir_bytes &&
        first_result.written.mlir_bytes == counts.mlir_bytes &&
        second_result.required.mlir_bytes == counts.mlir_bytes &&
        second_result.written.mlir_bytes == counts.mlir_bytes);
  CHECK(memcmp(first, second, counts.mlir_bytes) == 0);
  CHECK(memcmp(first, "// " W_SEED_MLIR0_SCHEMA_VERSION "\n",
               strlen("// " W_SEED_MLIR0_SCHEMA_VERSION "\n")) == 0);
  CHECK(contains_bytes(first, counts.mlir_bytes,
                       "// " W_SEED_MLIR0_SCHEMA_VERSION "\nmodule "));
  CHECK(memcmp(first_result.mlir_sha256, second_result.mlir_sha256,
               sizeof(first_result.mlir_sha256)) == 0);
  CHECK(first[counts.mlir_bytes] == 0xa5u &&
        second[counts.mlir_bytes] == 0x5au);
  CHECK(contains_bytes(first, counts.mlir_bytes,
                       "module attributes {llvm.target_triple = \""));
  CHECK(contains_bytes(first, counts.mlir_bytes,
                       "llvm.mlir.global private constant @w_seed_mlir0_payload(\""));
  CHECK(contains_bytes(first, counts.mlir_bytes,
                       "llvm.func @write(%fd: i32, %buffer: !llvm.ptr, %count: i64) -> i64"));
  CHECK(contains_bytes(first, counts.mlir_bytes,
                       "llvm.call @write(%fd, %data, %length)"));
  CHECK(contains_bytes(first, counts.mlir_bytes,
                       "llvm.icmp \"eq\" %written, %expected : i64"));
  CHECK(contains_bytes(first, counts.mlir_bytes, "\\48\\65\\6c\\6c"));
  CHECK(first[counts.mlir_bytes - 1u] == '\n');
  for (size_t index = 0u; index < counts.mlir_bytes; index += 1u)
    CHECK(first[index] != 0u);
  return true;
}

static bool test_exact_capacity_and_input_bytes(void) {
  w_seed_hlo0_plan plan = canonical_plan();
  set_text(plan.entry_target, sizeof(plan.entry_target), "serve");
  set_text(plan.handler, sizeof(plan.handler), "serve");
  static const uint8_t payload[] = {'A', 0x00u, 'B'};
  set_payload(&plan, payload, sizeof(payload));
  CHECK(w_seed_hlo0_verify_plan(&plan));
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  CHECK(w_seed_mlir0_measure(&plan, &TARGET, &counts, &measured) ==
        W_SEED_MLIR0_OK);
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_result result;
  (void)memset(output, 0x6du, sizeof(output));
  CHECK(emit(&plan, &TARGET, output, counts.mlir_bytes, &result));
  CHECK(result.required.mlir_bytes == counts.mlir_bytes &&
        result.written.mlir_bytes == counts.mlir_bytes);
  CHECK(output[counts.mlir_bytes] == 0x6du);
  CHECK(contains_bytes(output, counts.mlir_bytes, "\\41\\00\\42\\0a"));
  CHECK(contains_bytes(output, counts.mlir_bytes, "!llvm.array<4 x i8>"));
  CHECK(contains_bytes(output, counts.mlir_bytes, "@main() -> i32"));
  return true;
}

static bool test_capacity_and_no_partial_output(void) {
  const w_seed_hlo0_plan plan = canonical_plan();
  w_seed_mlir0_counts counts;
  w_seed_mlir0_result measured;
  CHECK(w_seed_mlir0_measure(&plan, &TARGET, &counts, &measured) ==
        W_SEED_MLIR0_OK);
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_result result;
  (void)memset(output, 0xc3u, sizeof(output));
  (void)memset(&result, 0x2bu, sizeof(result));
  const w_seed_mlir0_result result_snapshot = result;
  CHECK(w_seed_mlir0_emit(
            &plan, &TARGET,
            &(w_seed_mlir0_output){output, counts.mlir_bytes - 1u}, &result) ==
        W_SEED_MLIR0_CAPACITY);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0xc3u);
  CHECK(memcmp(&result, &result_snapshot, sizeof(result)) == 0);
  CHECK(w_seed_mlir0_emit(&plan, &TARGET, NULL, &result) ==
        W_SEED_MLIR0_CAPACITY);
  CHECK(memcmp(&result, &result_snapshot, sizeof(result)) == 0);
  CHECK(w_seed_mlir0_emit(
            &plan, &TARGET, &(w_seed_mlir0_output){NULL, 0u}, &result) ==
        W_SEED_MLIR0_CAPACITY);
  CHECK(memcmp(&result, &result_snapshot, sizeof(result)) == 0);
  CHECK(w_seed_mlir0_measure(&plan, &TARGET, NULL, &result) ==
        W_SEED_MLIR0_INVALID_PLAN);
  CHECK(w_seed_mlir0_measure(&plan, &TARGET, &counts, NULL) ==
        W_SEED_MLIR0_INVALID_PLAN);
  return true;
}

static bool test_alias_barriers(void) {
  w_seed_hlo0_plan plan = canonical_plan();
  const w_seed_hlo0_plan plan_snapshot = plan;
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  (void)memset(output, 0x7eu, sizeof(output));
  w_seed_mlir0_result result;
  (void)memset(&result, 0x4au, sizeof(result));
  const w_seed_mlir0_result result_snapshot = result;
  CHECK(w_seed_mlir0_emit(
            &plan, &TARGET,
            &(w_seed_mlir0_output){(uint8_t *)&plan,
                                   W_SEED_MLIR0_MAX_BYTES},
            &result) == W_SEED_MLIR0_ALIAS);
  CHECK(memcmp(&plan, &plan_snapshot, sizeof(plan)) == 0 &&
        memcmp(&result, &result_snapshot, sizeof(result)) == 0);
  CHECK(w_seed_mlir0_emit(
            &plan, &TARGET,
            &(w_seed_mlir0_output){(uint8_t *)&result,
                                   W_SEED_MLIR0_MAX_BYTES},
            &result) == W_SEED_MLIR0_ALIAS);
  CHECK(memcmp(&result, &result_snapshot, sizeof(result)) == 0);
  w_seed_mlir0_target target_snapshot = TARGET;
  (void)memset(&result, 0x4au, sizeof(result));
  const w_seed_mlir0_result target_result_snapshot = result;
  CHECK(w_seed_mlir0_emit(
            &plan, &target_snapshot,
            &(w_seed_mlir0_output){(uint8_t *)&target_snapshot,
                                   W_SEED_MLIR0_MAX_BYTES},
            &result) == W_SEED_MLIR0_ALIAS);
  CHECK(memcmp(&target_snapshot, &TARGET, sizeof(target_snapshot)) == 0 &&
        memcmp(&result, &target_result_snapshot, sizeof(result)) == 0);
  CHECK(w_seed_mlir0_emit(
            &plan, &TARGET, (const w_seed_mlir0_output *)&result, &result) ==
        W_SEED_MLIR0_ALIAS);
  CHECK(memcmp(&result, &result_snapshot, sizeof(result)) == 0);
  union {
    w_seed_mlir0_counts counts;
    w_seed_mlir0_result result;
  } records;
  (void)memset(&records, 0x63u, sizeof(records));
  const uint8_t *record_bytes = (const uint8_t *)(const void *)&records;
  CHECK(w_seed_mlir0_measure(
            &plan, &TARGET, (w_seed_mlir0_counts *)(void *)&records,
            (w_seed_mlir0_result *)(void *)&records) ==
        W_SEED_MLIR0_ALIAS);
  for (size_t index = 0u; index < sizeof(records); index += 1u)
    CHECK(record_bytes[index] == 0x63u);
  CHECK(w_seed_mlir0_measure(
            &plan, &TARGET, (w_seed_mlir0_counts *)(void *)&plan,
            &result) == W_SEED_MLIR0_ALIAS);
  CHECK(memcmp(&plan, &plan_snapshot, sizeof(plan)) == 0);
  CHECK(w_seed_mlir0_measure(
            &plan, &TARGET, &(w_seed_mlir0_counts){0u},
            (w_seed_mlir0_result *)(void *)&plan) == W_SEED_MLIR0_ALIAS);

  struct {
    uint8_t prefix[8];
    w_seed_mlir0_target target;
    uint8_t suffix[W_SEED_MLIR0_MAX_BYTES];
  } target_in_write_range;
  (void)memset(&target_in_write_range, 0x2du, sizeof(target_in_write_range));
  target_in_write_range.target = TARGET;
  uint8_t target_range_snapshot[sizeof(target_in_write_range)];
  (void)memcpy(target_range_snapshot, &target_in_write_range,
               sizeof(target_range_snapshot));
  (void)memset(&result, 0x19u, sizeof(result));
  const w_seed_mlir0_result target_range_result_snapshot = result;
  CHECK(w_seed_mlir0_emit(
            &plan, &target_in_write_range.target,
            &(w_seed_mlir0_output){target_in_write_range.prefix,
                                   W_SEED_MLIR0_MAX_BYTES},
            &result) == W_SEED_MLIR0_ALIAS);
  CHECK(memcmp(&target_in_write_range, target_range_snapshot,
               sizeof(target_range_snapshot)) == 0 &&
        memcmp(&result, &target_range_result_snapshot, sizeof(result)) == 0);
  return true;
}

static bool expect_invalid_plan(w_seed_hlo0_plan *plan) {
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_result result;
  (void)memset(output, 0x91u, sizeof(output));
  (void)memset(&result, 0x82u, sizeof(result));
  const w_seed_mlir0_result snapshot = result;
  CHECK(w_seed_mlir0_emit(plan, &TARGET,
                          &(w_seed_mlir0_output){output, sizeof(output)},
                          &result) == W_SEED_MLIR0_INVALID_PLAN);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0x91u);
  CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
  return true;
}

static bool test_target_and_plan_rejection(void) {
  w_seed_hlo0_plan plan = canonical_plan();
  w_seed_mlir0_target unsupported = {W_SEED_MLIR0_TARGET_UNSUPPORTED};
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_result result;
  (void)memset(output, 0x33u, sizeof(output));
  (void)memset(&result, 0x44u, sizeof(result));
  const w_seed_mlir0_result snapshot = result;
  CHECK(!w_seed_mlir0_target_is_supported(&unsupported));
  CHECK(w_seed_mlir0_measure(&plan, &unsupported,
                             &(w_seed_mlir0_counts){0u}, &result) ==
        W_SEED_MLIR0_UNSUPPORTED);
  CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
  CHECK(w_seed_mlir0_emit(
            &plan, &unsupported,
            &(w_seed_mlir0_output){output, sizeof(output)}, &result) ==
        W_SEED_MLIR0_UNSUPPORTED);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0x33u);
  CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
  plan.profile[0] = 'X';
  CHECK(expect_invalid_plan(&plan));
  plan = canonical_plan();
  plan.payload[0] ^= 1u;
  CHECK(expect_invalid_plan(&plan));
  plan = canonical_plan();
  plan.payload[plan.payload_bytes] = 1u;
  CHECK(expect_invalid_plan(&plan));
  plan = canonical_plan();
  plan.stdout_bytes -= 1u;
  CHECK(expect_invalid_plan(&plan));
  plan = canonical_plan();
  plan.exit_success = false;
  CHECK(expect_invalid_plan(&plan));
  plan = canonical_plan();
  plan.newline_policy = (w_seed_hlo0_newline_policy)99;
  CHECK(expect_invalid_plan(&plan));
  return true;
}

int main(void) {
  if (!test_exact_and_deterministic()) return 1;
  if (!test_exact_capacity_and_input_bytes()) return 1;
  if (!test_capacity_and_no_partial_output()) return 1;
  if (!test_alias_barriers()) return 1;
  if (!test_target_and_plan_rejection()) return 1;
  (void)puts("seed MLIR0: deterministic LLVM dialect emitter and all-or-nothing barriers passed");
  return 0;
}
