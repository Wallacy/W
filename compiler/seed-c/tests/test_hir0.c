#include "w_seed_hir0.h"
#include "w_seed_mlir0.h"
#include "w_seed_parallel_invocation0.h"
#include "w_seed_parallel_invocation1.h"
#include "w_seed_parallel_panic_binding1.h"
#include "w_seed_parallel_lifecycle1.h"
#include "w_seed_parallel_provider0.h"
#include "w_seed_parallel_provider1.h"
#include "w_seed_parallel_typed_binding1.h"
#include "w_seed_parallel_typed_lifecycle1.h"
#include "w_seed_parallel_selection0.h"
#include "w_seed_parallel_selection1.h"
#include "w_seed_parallel_elision0.h"
#include "w_seed_product_closure0.h"
#include "w_seed_scalar_evaluator0.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Test-only seam for resealing an in-memory HIR mutation. This keeps the
 * production digest helpers private while distinguishing lifecycle rejection
 * from the ordinary unchanged-digest mutation checks below. The included
 * translation unit supplies the public HIR symbols, so the archive object is
 * not extracted a second time at link. */
#include "../src/w_seed_hir0.c"
#include "../src/w_seed_native_subset0.h"
#include "../src/w_seed_parallel_provider0_platform.h"

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "hir0 check failed: %s (%s:%d)\n", #condition, \
                    __FILE__, __LINE__);                                      \
      return false;                                                            \
    }                                                                          \
  } while (0)

static bool test_scalar_evaluator_edges(void) {
  int64_t value = INT64_MIN;
  CHECK(w_seed_scalar_evaluator0_checked_binary(
            W_SEED_HIR0_BINARY_REMAINDER, INT64_MIN, -1, &value) &&
        value == 0);
  value = 0x5151;
  CHECK(!w_seed_scalar_evaluator0_checked_binary(
            W_SEED_HIR0_BINARY_DIVIDE, INT64_MIN, -1, &value) &&
        value == 0x5151);
  return true;
}

static bool bytes_contain(const uint8_t *bytes, size_t count,
                          const char *needle) {
  if (bytes == NULL || needle == NULL) return false;
  const size_t needle_count = strlen(needle);
  if (needle_count == 0u || needle_count > count) return false;
  for (size_t offset = 0u; offset <= count - needle_count; offset += 1u)
    if (memcmp(bytes + offset, needle, needle_count) == 0) return true;
  return false;
}

static bool test_parallel_mlir_entries(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection) {
  w_seed_parallel_invocation0_plan invocation;
  CHECK(w_seed_parallel_invocation0_select(program, hir_result, selection,
                                            &invocation) ==
        W_SEED_PARALLEL_INVOCATION0_OK);
  w_seed_mlir0_parallel_entry_counts counts;
  w_seed_mlir0_parallel_entry_result measured;
  CHECK(w_seed_mlir0_measure_parallel_entries(
            program, hir_result, selection, &invocation, &counts, &measured) ==
            W_SEED_MLIR0_OK &&
        counts.mlir_bytes > 0u && counts.mlir_bytes < W_SEED_MLIR0_MAX_BYTES &&
        counts.task_count == selection->task_count &&
        counts.runtime_argument_count == 3u &&
        counts.reachable_function_count == 3u &&
        measured.required.mlir_bytes == counts.mlir_bytes &&
        measured.written.mlir_bytes == 0u);

  union measure_alias_storage {
    w_seed_mlir0_parallel_entry_counts counts;
    w_seed_mlir0_parallel_entry_result result;
  } measure_alias;
  (void)memset(&measure_alias, 0x6a, sizeof(measure_alias));
  const union measure_alias_storage measure_alias_before = measure_alias;
  CHECK(w_seed_mlir0_measure_parallel_entries(
            program, hir_result, selection, &invocation,
            &measure_alias.counts, &measure_alias.result) ==
            W_SEED_MLIR0_ALIAS &&
        memcmp(&measure_alias, &measure_alias_before,
               sizeof(measure_alias)) == 0);

  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  (void)memset(artifact, 0xa5, sizeof(artifact));
  w_seed_mlir0_parallel_entry_result emitted;
  CHECK(w_seed_mlir0_emit_parallel_entries(
            program, hir_result, selection, &invocation,
            &(w_seed_mlir0_parallel_entry_output){artifact, sizeof(artifact)},
            &emitted) == W_SEED_MLIR0_OK &&
        emitted.written.mlir_bytes == counts.mlir_bytes &&
        w_seed_mlir0_verify_parallel_entries(
            program, hir_result, selection, &invocation, artifact,
            counts.mlir_bytes, &emitted) &&
        bytes_contain(artifact, counts.mlir_bytes,
                      "w-seed-mlir0-parallel-entry-2") &&
        bytes_contain(artifact, counts.mlir_bytes,
                      "func.func @w_seed_parallel_task_0(%arg0: i64) -> i64") &&
        bytes_contain(artifact, counts.mlir_bytes,
                      "func.func @w_seed_parallel_task_1(%arg0: i64, "
                      "%arg1: i64) -> i64") &&
        bytes_contain(artifact, counts.mlir_bytes,
                      "func.call @w_coop_fn_2(%arg0, %arg1) : "
                      "(i64, i64) -> i64") &&
        !bytes_contain(artifact, counts.mlir_bytes,
                       "%cv0 = arith.constant 20 : i64") &&
        !bytes_contain(artifact, counts.mlir_bytes,
                       "%cv2 = arith.constant 2 : i64") &&
        bytes_contain(artifact, counts.mlir_bytes,
                      "func.func private @w_coop_fn_0") &&
        bytes_contain(artifact, counts.mlir_bytes,
                      "func.func private @w_coop_fn_1") &&
        bytes_contain(artifact, counts.mlir_bytes,
                      "func.func private @w_coop_fn_2") &&
        !bytes_contain(artifact, counts.mlir_bytes, "@w_coop_fn_3") &&
        !bytes_contain(artifact, counts.mlir_bytes,
                       "@w_seed_cooperative_core"));

  uint8_t short_artifact[W_SEED_MLIR0_MAX_BYTES];
  (void)memset(short_artifact, 0x3c, sizeof(short_artifact));
  uint8_t short_before[W_SEED_MLIR0_MAX_BYTES];
  (void)memcpy(short_before, short_artifact, sizeof(short_before));
  w_seed_mlir0_parallel_entry_result sentinel;
  (void)memset(&sentinel, 0x4d, sizeof(sentinel));
  const w_seed_mlir0_parallel_entry_result sentinel_before = sentinel;
  CHECK(w_seed_mlir0_emit_parallel_entries(
            program, hir_result, selection, &invocation,
            &(w_seed_mlir0_parallel_entry_output){short_artifact,
                                                  counts.mlir_bytes - 1u},
            &sentinel) == W_SEED_MLIR0_CAPACITY &&
        memcmp(short_artifact, short_before, sizeof(short_before)) == 0 &&
        memcmp(&sentinel, &sentinel_before, sizeof(sentinel_before)) == 0);

  union {
    uint8_t bytes[W_SEED_MLIR0_MAX_BYTES];
    w_seed_mlir0_parallel_entry_result result;
  } result_alias;
  (void)memset(&result_alias, 0x71, sizeof(result_alias));
  uint8_t result_alias_before[sizeof(result_alias)];
  (void)memcpy(result_alias_before, &result_alias, sizeof(result_alias));
  CHECK(w_seed_mlir0_emit_parallel_entries(
            program, hir_result, selection, &invocation,
            &(w_seed_mlir0_parallel_entry_output){result_alias.bytes,
                                                  sizeof(result_alias.bytes)},
            &result_alias.result) == W_SEED_MLIR0_ALIAS &&
        memcmp(&result_alias, result_alias_before, sizeof(result_alias)) == 0);

  CHECK(program->text_byte_capacity >= counts.mlir_bytes);
  uint8_t hir_text_before[W_SEED_MLIR0_MAX_BYTES];
  (void)memcpy(hir_text_before, program->text_bytes, counts.mlir_bytes);
  CHECK(w_seed_mlir0_emit_parallel_entries(
            program, hir_result, selection, &invocation,
            &(w_seed_mlir0_parallel_entry_output){
                (uint8_t *)program->text_bytes, counts.mlir_bytes},
            &sentinel) == W_SEED_MLIR0_ALIAS &&
        memcmp(program->text_bytes, hir_text_before, counts.mlir_bytes) == 0 &&
        memcmp(&sentinel, &sentinel_before, sizeof(sentinel_before)) == 0);

  w_seed_parallel_invocation0_plan forged = invocation;
  forged.tasks[0].function_index ^= 1u;
  CHECK(w_seed_mlir0_emit_parallel_entries(
            program, hir_result, selection, &forged,
            &(w_seed_mlir0_parallel_entry_output){artifact, sizeof(artifact)},
            &sentinel) == W_SEED_MLIR0_INVALID_HIR &&
        memcmp(&sentinel, &sentinel_before, sizeof(sentinel_before)) == 0);
  return true;
}

static bool test_parallel_mlir_entries1(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_program *selection,
    const w_seed_parallel_selection1_result *selection_result,
    const w_seed_parallel_invocation1_program *invocation,
    const w_seed_parallel_invocation1_result *invocation_result,
    const w_seed_parallel_selection0 *compatible_selection,
    uint32_t expected_tasks, uint32_t expected_arguments,
    const char *expected_symbol) {
  w_seed_mlir0_parallel_entry_counts counts;
  w_seed_mlir1_parallel_entry_result measured;
  const w_seed_mlir0_status measure_status =
      w_seed_mlir1_measure_parallel_entries(
          program, hir_result, selection, selection_result, invocation,
          invocation_result, &counts, &measured);
  CHECK(measure_status == W_SEED_MLIR0_OK &&
        counts.mlir_bytes > 0u && counts.mlir_bytes < W_SEED_MLIR0_MAX_BYTES &&
        counts.task_count == expected_tasks &&
        counts.runtime_argument_count == expected_arguments &&
        measured.required.mlir_bytes == counts.mlir_bytes &&
        measured.written.mlir_bytes == 0u &&
        strcmp(measured.schema,
               W_SEED_MLIR1_PARALLEL_ENTRY_RESULT_SCHEMA_VERSION) == 0);

  union {
    w_seed_mlir0_parallel_entry_counts counts;
    w_seed_mlir1_parallel_entry_result result;
  } measure_alias;
  (void)memset(&measure_alias, 0x49, sizeof(measure_alias));
  unsigned char measure_alias_before[sizeof(measure_alias)];
  (void)memcpy(measure_alias_before, &measure_alias, sizeof(measure_alias));
  CHECK(w_seed_mlir1_measure_parallel_entries(
            program, hir_result, selection, selection_result, invocation,
            invocation_result, &measure_alias.counts,
            &measure_alias.result) == W_SEED_MLIR0_ALIAS &&
        memcmp(&measure_alias, measure_alias_before, sizeof(measure_alias)) ==
            0);

  static uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  (void)memset(artifact, 0xa5, sizeof(artifact));
  w_seed_mlir1_parallel_entry_result emitted;
  CHECK(w_seed_mlir1_emit_parallel_entries(
            program, hir_result, selection, selection_result, invocation,
            invocation_result,
            &(w_seed_mlir0_parallel_entry_output){artifact, sizeof(artifact)},
            &emitted) == W_SEED_MLIR0_OK &&
        emitted.written.mlir_bytes == counts.mlir_bytes &&
        w_seed_mlir1_verify_parallel_entries(
            program, hir_result, selection, selection_result, invocation,
            invocation_result, artifact, counts.mlir_bytes, &emitted) &&
        bytes_contain(artifact, counts.mlir_bytes, expected_symbol));

  if (compatible_selection != NULL) {
    w_seed_parallel_invocation0_plan legacy_invocation;
    CHECK(w_seed_parallel_invocation0_select(
              program, hir_result, compatible_selection, &legacy_invocation) ==
          W_SEED_PARALLEL_INVOCATION0_OK);
    static uint8_t legacy_artifact[W_SEED_MLIR0_MAX_BYTES];
    w_seed_mlir0_parallel_entry_result legacy_result;
    CHECK(w_seed_mlir0_emit_parallel_entries(
              program, hir_result, compatible_selection, &legacy_invocation,
              &(w_seed_mlir0_parallel_entry_output){legacy_artifact,
                                                    sizeof(legacy_artifact)},
              &legacy_result) == W_SEED_MLIR0_OK &&
          legacy_result.written.mlir_bytes == counts.mlir_bytes &&
          memcmp(legacy_artifact, artifact, counts.mlir_bytes) == 0);
  }

  uint8_t short_artifact[32];
  (void)memset(short_artifact, 0x5a, sizeof(short_artifact));
  const uint8_t short_before[32] = {
      0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a,
      0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a,
      0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a,
      0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a};
  w_seed_mlir1_parallel_entry_result sentinel;
  (void)memset(&sentinel, 0x6b, sizeof(sentinel));
  const w_seed_mlir1_parallel_entry_result sentinel_before = sentinel;
  CHECK(w_seed_mlir1_emit_parallel_entries(
            program, hir_result, selection, selection_result, invocation,
            invocation_result,
            &(w_seed_mlir0_parallel_entry_output){short_artifact,
                                                  sizeof(short_artifact)},
            &sentinel) == W_SEED_MLIR0_CAPACITY &&
        memcmp(short_artifact, short_before, sizeof(short_artifact)) == 0 &&
        memcmp(&sentinel, &sentinel_before, sizeof(sentinel)) == 0);

  static union {
    uint8_t bytes[W_SEED_MLIR0_MAX_BYTES];
    w_seed_mlir1_parallel_entry_result result;
  } alias;
  (void)memset(&alias, 0x7c, sizeof(alias));
  static uint8_t alias_before[sizeof(alias)];
  (void)memcpy(alias_before, &alias, sizeof(alias));
  CHECK(w_seed_mlir1_emit_parallel_entries(
            program, hir_result, selection, selection_result, invocation,
            invocation_result,
            &(w_seed_mlir0_parallel_entry_output){alias.bytes,
                                                  sizeof(alias.bytes)},
            &alias.result) == W_SEED_MLIR0_ALIAS &&
        memcmp(&alias, alias_before, sizeof(alias)) == 0);

  w_seed_parallel_invocation1_result forged_invocation_result =
      *invocation_result;
  forged_invocation_result.semantic_digest[0] ^= 1u;
  CHECK(w_seed_mlir1_emit_parallel_entries(
            program, hir_result, selection, selection_result, invocation,
            &forged_invocation_result,
            &(w_seed_mlir0_parallel_entry_output){short_artifact,
                                                  sizeof(short_artifact)},
            &sentinel) == W_SEED_MLIR0_INVALID_HIR &&
        memcmp(short_artifact, short_before, sizeof(short_artifact)) == 0 &&
        memcmp(&sentinel, &sentinel_before, sizeof(sentinel)) == 0);
  return true;
}

#if defined(_WIN32) && defined(_WIN64)
typedef struct {
  w_seed_parallel_platform1_completion requested[5];
  uint32_t calls[5];
} test_platform1_context;

static bool test_platform1_invoke(
    void *raw, size_t task_index,
    w_seed_parallel_platform1_completion *completion) {
  test_platform1_context *context = (test_platform1_context *)raw;
  if (context == NULL || completion == NULL || task_index >= 5u) return false;
  context->calls[task_index] += 1u;
  *completion = context->requested[task_index];
  return true;
}

static bool test_parallel_platform1_typed_completions(void) {
  test_platform1_context parallel_context = {0};
  parallel_context.requested[0] = (w_seed_parallel_platform1_completion){
      W_SEED_PARALLEL_PLATFORM1_COMPLETION_SUCCESS, 10, 0u, 0u, 0u,
      W_SEED_PARALLEL_PLATFORM1_PANIC_NONE};
  parallel_context.requested[1] = (w_seed_parallel_platform1_completion){
      W_SEED_PARALLEL_PLATFORM1_COMPLETION_ERROR, 0, 2u, 0u, 0u,
      W_SEED_PARALLEL_PLATFORM1_PANIC_NONE};
  for (size_t index = 2u; index < 5u; index += 1u)
    parallel_context.requested[index] =
        (w_seed_parallel_platform1_completion){
            W_SEED_PARALLEL_PLATFORM1_COMPLETION_SUCCESS,
            10 + (int64_t)index, 0u, 0u, 0u,
            W_SEED_PARALLEL_PLATFORM1_PANIC_NONE};
  const w_seed_parallel_platform1_job parallel_job = {
      test_platform1_invoke, &parallel_context, sizeof(parallel_context)};
  w_seed_parallel_platform1_completion parallel_completions[5];
  w_seed_parallel_platform1_receipt parallel_receipt;
  w_seed_parallel_provider0_kind parallel_kind =
      W_SEED_PARALLEL_PROVIDER0_KIND_NONE;
  CHECK(w_seed_parallel_platform1_execute(
            &parallel_job, 5u, 2u, parallel_completions, &parallel_receipt,
            &parallel_kind) == W_SEED_PARALLEL_PROVIDER0_PLATFORM_OK &&
        parallel_kind == W_SEED_PARALLEL_PROVIDER0_KIND_WINDOWS_KERNEL32 &&
        parallel_receipt.started_count == 2u &&
        parallel_receipt.settled_count == 2u &&
        parallel_receipt.canceled_before_start_count == 3u &&
        parallel_receipt.maximum_active == 2u &&
        parallel_receipt.cancellation_requested &&
        parallel_receipt.cancellation_source_index == 1u &&
        parallel_completions[0].kind ==
            W_SEED_PARALLEL_PLATFORM1_COMPLETION_SUCCESS &&
        parallel_completions[0].success_value == 10 &&
        parallel_completions[1].kind ==
            W_SEED_PARALLEL_PLATFORM1_COMPLETION_ERROR &&
        parallel_completions[1].error_code == 2u);
  for (size_t index = 2u; index < 5u; index += 1u)
    CHECK(parallel_context.calls[index] == 0u &&
          parallel_completions[index].kind ==
              W_SEED_PARALLEL_PLATFORM1_COMPLETION_CANCELED &&
          parallel_completions[index].cancel_reason ==
              W_SEED_PARALLEL_PLATFORM1_CANCEL_FAIL_FAST);

  test_platform1_context serial_context = parallel_context;
  (void)memset(serial_context.calls, 0, sizeof(serial_context.calls));
  const w_seed_parallel_platform1_job serial_job = {
      test_platform1_invoke, &serial_context, sizeof(serial_context)};
  w_seed_parallel_platform1_completion serial_completions[5];
  w_seed_parallel_platform1_receipt serial_receipt;
  w_seed_parallel_provider0_kind serial_kind =
      W_SEED_PARALLEL_PROVIDER0_KIND_NONE;
  CHECK(w_seed_parallel_platform1_execute(
            &serial_job, 5u, 1u, serial_completions, &serial_receipt,
            &serial_kind) == W_SEED_PARALLEL_PROVIDER0_PLATFORM_OK &&
        serial_kind == parallel_kind &&
        serial_receipt.started_count == 2u &&
        serial_receipt.settled_count == 2u &&
        serial_receipt.canceled_before_start_count == 3u &&
        serial_receipt.maximum_active == 1u &&
        serial_receipt.cancellation_source_index == 1u &&
        memcmp(serial_completions, parallel_completions,
               sizeof(serial_completions)) == 0);

  test_platform1_context canceled_context = {0};
  canceled_context.requested[0] = (w_seed_parallel_platform1_completion){
      W_SEED_PARALLEL_PLATFORM1_COMPLETION_CANCELED, 0, 0u, 9u, 0u,
      W_SEED_PARALLEL_PLATFORM1_PANIC_NONE};
  canceled_context.requested[1] = (w_seed_parallel_platform1_completion){
      W_SEED_PARALLEL_PLATFORM1_COMPLETION_SUCCESS, 11, 0u, 0u, 0u,
      W_SEED_PARALLEL_PLATFORM1_PANIC_NONE};
  const w_seed_parallel_platform1_job canceled_job = {
      test_platform1_invoke, &canceled_context, sizeof(canceled_context)};
  w_seed_parallel_platform1_completion canceled_completions[5];
  w_seed_parallel_platform1_receipt canceled_receipt;
  w_seed_parallel_provider0_kind canceled_kind =
      W_SEED_PARALLEL_PROVIDER0_KIND_NONE;
  CHECK(w_seed_parallel_platform1_execute(
            &canceled_job, 5u, 2u, canceled_completions, &canceled_receipt,
            &canceled_kind) == W_SEED_PARALLEL_PROVIDER0_PLATFORM_OK &&
        canceled_receipt.cancellation_source_index == 0u &&
        canceled_completions[0].kind ==
            W_SEED_PARALLEL_PLATFORM1_COMPLETION_CANCELED &&
        canceled_completions[1].kind ==
            W_SEED_PARALLEL_PLATFORM1_COMPLETION_SUCCESS);
  for (size_t index = 2u; index < 5u; index += 1u)
    CHECK(canceled_completions[index].kind ==
              W_SEED_PARALLEL_PLATFORM1_COMPLETION_CANCELED &&
          canceled_completions[index].cancel_reason == 9u);

  test_platform1_context panic_context = parallel_context;
  (void)memset(panic_context.calls, 0, sizeof(panic_context.calls));
  panic_context.requested[0] = (w_seed_parallel_platform1_completion){
      .kind = W_SEED_PARALLEL_PLATFORM1_COMPLETION_ERROR,
      .error_code = 2u};
  panic_context.requested[1] = (w_seed_parallel_platform1_completion){
      .kind = W_SEED_PARALLEL_PLATFORM1_COMPLETION_PANIC,
      .panic_code = W_SEED_PARALLEL_PLATFORM1_PANIC_EXPLICIT};
  const w_seed_parallel_platform1_job panic_job = {
      test_platform1_invoke, &panic_context, sizeof(panic_context)};
  w_seed_parallel_platform1_completion panic_completions[5];
  w_seed_parallel_platform1_receipt panic_receipt;
  w_seed_parallel_provider0_kind panic_kind =
      W_SEED_PARALLEL_PROVIDER0_KIND_NONE;
  CHECK(w_seed_parallel_platform1_execute(
            &panic_job, 5u, 2u, panic_completions, &panic_receipt,
            &panic_kind) == W_SEED_PARALLEL_PROVIDER0_PLATFORM_OK &&
        panic_receipt.started_count == 2u &&
        panic_receipt.settled_count == 2u &&
        panic_receipt.canceled_before_start_count == 3u &&
        panic_receipt.cancellation_requested &&
        panic_receipt.cancellation_source_index == 1u &&
        panic_receipt.panic_requested &&
        panic_receipt.panic_source_index == 1u &&
        panic_receipt.panic_code ==
            W_SEED_PARALLEL_PLATFORM1_PANIC_EXPLICIT &&
        panic_completions[0].kind ==
            W_SEED_PARALLEL_PLATFORM1_COMPLETION_ERROR &&
        panic_completions[1].kind ==
            W_SEED_PARALLEL_PLATFORM1_COMPLETION_PANIC);
  for (size_t index = 2u; index < 5u; index += 1u)
    CHECK(panic_context.calls[index] == 0u &&
          panic_completions[index].kind ==
              W_SEED_PARALLEL_PLATFORM1_COMPLETION_CANCELED &&
          panic_completions[index].cancel_reason ==
              W_SEED_PARALLEL_PLATFORM1_CANCEL_PANIC_BOUNDARY);

  test_platform1_context invalid_context = parallel_context;
  (void)memset(invalid_context.calls, 0, sizeof(invalid_context.calls));
  invalid_context.requested[0].error_code = 1u;
  const w_seed_parallel_platform1_job invalid_job = {
      test_platform1_invoke, &invalid_context, sizeof(invalid_context)};
  CHECK(w_seed_parallel_platform1_execute(
            &invalid_job, 5u, 1u, serial_completions, &serial_receipt,
            &serial_kind) == W_SEED_PARALLEL_PROVIDER0_PLATFORM_TASK_FAILURE);
  invalid_context.requested[0] = (w_seed_parallel_platform1_completion){
      .kind = W_SEED_PARALLEL_PLATFORM1_COMPLETION_PANIC,
      .panic_code = W_SEED_PARALLEL_PLATFORM1_PANIC_NONE};
  CHECK(w_seed_parallel_platform1_execute(
            &invalid_job, 5u, 1u, serial_completions, &serial_receipt,
            &serial_kind) == W_SEED_PARALLEL_PROVIDER0_PLATFORM_TASK_FAILURE);
  const w_seed_parallel_platform1_job missing_context_extent = {
      test_platform1_invoke, &invalid_context, 0u};
  (void)memset(serial_completions, 0x4eu, sizeof(serial_completions));
  (void)memset(&serial_receipt, 0x9du, sizeof(serial_receipt));
  serial_kind = W_SEED_PARALLEL_PROVIDER0_KIND_NONE;
  const w_seed_parallel_platform1_completion
      missing_context_completions_before[5] = {
          serial_completions[0], serial_completions[1], serial_completions[2],
          serial_completions[3], serial_completions[4]};
  const w_seed_parallel_platform1_receipt missing_context_receipt_before =
      serial_receipt;
  const uint32_t missing_context_calls_before = invalid_context.calls[0];
  CHECK(w_seed_parallel_platform1_execute(
            &missing_context_extent, 5u, 1u, serial_completions,
            &serial_receipt, &serial_kind) ==
            W_SEED_PARALLEL_PROVIDER0_PLATFORM_PROVIDER_FAILURE &&
        invalid_context.calls[0] == missing_context_calls_before &&
        memcmp(serial_completions, missing_context_completions_before,
               sizeof(serial_completions)) == 0 &&
        memcmp(&serial_receipt, &missing_context_receipt_before,
               sizeof(serial_receipt)) == 0 &&
        serial_kind == W_SEED_PARALLEL_PROVIDER0_KIND_NONE);
  const w_seed_parallel_platform1_job null_context_extent = {
      test_platform1_invoke, NULL, sizeof(invalid_context)};
  CHECK(w_seed_parallel_platform1_execute(
            &null_context_extent, 5u, 1u, serial_completions, &serial_receipt,
            &serial_kind) ==
        W_SEED_PARALLEL_PROVIDER0_PLATFORM_PROVIDER_FAILURE);
  return true;
}

static bool test_parallel_provider_cardinality(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection) {
  CHECK(program != NULL && hir_result != NULL && selection != NULL &&
        selection->task_count >= 1u &&
        selection->task_count <= W_SEED_PARALLEL_PROVIDER0_MAX_TASKS);
  w_seed_parallel_invocation0_plan invocation;
  CHECK(w_seed_parallel_invocation0_select(program, hir_result, selection,
                                            &invocation) ==
            W_SEED_PARALLEL_INVOCATION0_OK &&
        w_seed_parallel_invocation0_verify(program, hir_result, selection,
                                            &invocation));
  int64_t direct_value = INT64_MIN;
  CHECK(w_seed_parallel_invocation0_evaluate_task(
            program, hir_result, selection, &invocation, 0u, &direct_value) ==
            W_SEED_PARALLEL_INVOCATION0_OK &&
        direct_value == 21);
  w_seed_parallel_provider0_input input = {
      program, hir_result, selection, &invocation, 1u};
  w_seed_parallel_provider0_outcomes sequential;
  w_seed_parallel_provider0_receipt sequential_receipt;
  CHECK(w_seed_parallel_provider0_execute(&input, &sequential,
                                          &sequential_receipt) ==
        W_SEED_PARALLEL_PROVIDER0_OK);
  for (size_t index = 0u; index < selection->task_count; index += 1u)
    CHECK(sequential.values[index] == 21 + (int64_t)(index * 2u));
  input.provider_capacity = 2u;
  w_seed_parallel_provider0_outcomes parallel;
  w_seed_parallel_provider0_receipt parallel_receipt;
  CHECK(w_seed_parallel_provider0_execute(&input, &parallel,
                                          &parallel_receipt) ==
            W_SEED_PARALLEL_PROVIDER0_OK &&
        memcmp(&parallel, &sequential, sizeof(parallel)) == 0 &&
        sequential_receipt.maximum_active_workers == 1u &&
        parallel_receipt.maximum_active_workers >= 1u &&
        parallel_receipt.maximum_active_workers <= 2u);
  return true;
}

static bool test_parallel_provider_windows(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection) {
  CHECK(program != NULL && hir_result != NULL && selection != NULL &&
        selection->task_count == 2u);
  w_seed_parallel_invocation0_plan invocation;
  CHECK(w_seed_parallel_invocation0_select(program, hir_result, selection,
                                            &invocation) ==
            W_SEED_PARALLEL_INVOCATION0_OK &&
        w_seed_parallel_invocation0_verify(program, hir_result, selection,
                                            &invocation));
  int64_t first_value = INT64_MIN;
  int64_t second_value = INT64_MIN;
  CHECK(w_seed_parallel_invocation0_evaluate_task(
            program, hir_result, selection, &invocation, 0u, &first_value) ==
            W_SEED_PARALLEL_INVOCATION0_OK &&
        w_seed_parallel_invocation0_evaluate_task(
            program, hir_result, selection, &invocation, 1u, &second_value) ==
            W_SEED_PARALLEL_INVOCATION0_OK &&
        first_value == 21 && second_value == 23);

  union {
    w_seed_parallel_selection0 selection;
    w_seed_parallel_invocation0_plan invocation;
  } invocation_alias;
  (void)memset(&invocation_alias, 0x3d, sizeof(invocation_alias));
  invocation_alias.selection = *selection;
  unsigned char invocation_alias_before[sizeof(invocation_alias)];
  (void)memcpy(invocation_alias_before, &invocation_alias,
               sizeof(invocation_alias));
  CHECK(w_seed_parallel_invocation0_select(
            program, hir_result, &invocation_alias.selection,
            &invocation_alias.invocation) ==
            W_SEED_PARALLEL_INVOCATION0_INVALID &&
        memcmp(&invocation_alias, invocation_alias_before,
               sizeof(invocation_alias)) == 0);

  w_seed_parallel_provider0_input input = {
      program, hir_result, selection, &invocation, 1u};
  w_seed_parallel_provider0_outcomes sequential;
  w_seed_parallel_provider0_receipt sequential_receipt;
  (void)memset(&sequential, 0x5a, sizeof(sequential));
  (void)memset(&sequential_receipt, 0x5a, sizeof(sequential_receipt));
  CHECK(w_seed_parallel_provider0_execute(&input, &sequential,
                                          &sequential_receipt) ==
            W_SEED_PARALLEL_PROVIDER0_OK &&
        sequential.task_count == 2u && sequential.values[0] == 21 &&
        sequential.values[1] == 23 &&
        w_seed_parallel_provider0_verify_outcomes(program, hir_result,
                                                   selection, &sequential) &&
        sequential_receipt.provider_kind ==
            W_SEED_PARALLEL_PROVIDER0_KIND_WINDOWS_KERNEL32 &&
        sequential_receipt.provider_capacity == 1u &&
        sequential_receipt.started_count == 2u &&
        sequential_receipt.completed_count == 2u &&
        sequential_receipt.maximum_active_workers == 1u &&
        !sequential_receipt.overlap_observed);
  w_seed_parallel_provider0_outcomes forged_outcomes = sequential;
  forged_outcomes.outcome_digest[0] ^= 1u;
  CHECK(!w_seed_parallel_provider0_verify_outcomes(
      program, hir_result, selection, &forged_outcomes));

  input.provider_capacity = 2u;
  w_seed_parallel_provider0_outcomes parallel;
  w_seed_parallel_provider0_receipt parallel_receipt;
  (void)memset(&parallel, 0x6c, sizeof(parallel));
  (void)memset(&parallel_receipt, 0x6c, sizeof(parallel_receipt));
  const w_seed_parallel_provider0_status parallel_status =
      w_seed_parallel_provider0_execute(&input, &parallel, &parallel_receipt);
  CHECK(parallel_status == W_SEED_PARALLEL_PROVIDER0_OK &&
        memcmp(&parallel, &sequential, sizeof(parallel)) == 0 &&
        parallel_receipt.provider_capacity == 2u &&
        parallel_receipt.started_count == 2u &&
        parallel_receipt.completed_count == 2u &&
        parallel_receipt.maximum_active_workers == 2u &&
        parallel_receipt.overlap_observed);

  w_seed_parallel_provider0_outcomes outcome_sentinel;
  w_seed_parallel_provider0_receipt receipt_sentinel;
  (void)memset(&outcome_sentinel, 0x71, sizeof(outcome_sentinel));
  (void)memset(&receipt_sentinel, 0x72, sizeof(receipt_sentinel));
  const w_seed_parallel_provider0_outcomes outcome_before = outcome_sentinel;
  const w_seed_parallel_provider0_receipt receipt_before = receipt_sentinel;
  input.provider_capacity = 3u;
  CHECK(w_seed_parallel_provider0_execute(&input, &outcome_sentinel,
                                          &receipt_sentinel) ==
            W_SEED_PARALLEL_PROVIDER0_INVALID &&
        memcmp(&outcome_sentinel, &outcome_before, sizeof(outcome_before)) ==
            0 &&
        memcmp(&receipt_sentinel, &receipt_before, sizeof(receipt_before)) ==
            0);

  input.provider_capacity = 1u;
  w_seed_parallel_invocation0_plan forged_invocation = invocation;
  forged_invocation.tasks[0].call_index ^= 1u;
  int64_t forged_value = 0x6262;
  CHECK(w_seed_parallel_invocation0_evaluate_task(
            program, hir_result, selection, &forged_invocation, 0u,
            &forged_value) == W_SEED_PARALLEL_INVOCATION0_INVALID &&
        forged_value == 0x6262);
  input.invocation = &forged_invocation;
  CHECK(w_seed_parallel_provider0_execute(&input, &outcome_sentinel,
                                          &receipt_sentinel) ==
            W_SEED_PARALLEL_PROVIDER0_INVALID &&
        memcmp(&outcome_sentinel, &outcome_before, sizeof(outcome_before)) ==
            0 &&
        memcmp(&receipt_sentinel, &receipt_before, sizeof(receipt_before)) ==
            0);

  union {
    w_seed_parallel_selection0 selection;
    w_seed_parallel_provider0_outcomes outcomes;
  } selection_alias;
  (void)memset(&selection_alias, 0x2d, sizeof(selection_alias));
  selection_alias.selection = *selection;
  unsigned char alias_snapshot[sizeof(selection_alias)];
  (void)memcpy(alias_snapshot, &selection_alias, sizeof(selection_alias));
  input.invocation = &invocation;
  input.selection = &selection_alias.selection;
  CHECK(w_seed_parallel_provider0_execute(&input, &selection_alias.outcomes,
                                          &receipt_sentinel) ==
            W_SEED_PARALLEL_PROVIDER0_INVALID &&
        memcmp(&selection_alias, alias_snapshot, sizeof(selection_alias)) ==
            0 &&
        memcmp(&receipt_sentinel, &receipt_before, sizeof(receipt_before)) ==
            0);
  return true;
}

static bool test_parallel_provider1_windows(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_program *selection,
    const w_seed_parallel_selection1_result *selection_result,
    const w_seed_parallel_invocation1_program *invocation,
    const w_seed_parallel_invocation1_result *invocation_result) {
  CHECK(program != NULL && hir_result != NULL && selection != NULL &&
        selection_result != NULL && invocation != NULL &&
        invocation_result != NULL && selection->task_count == 5u &&
        test_parallel_platform1_typed_completions());
  w_seed_parallel_provider1_input input = {
      program,       hir_result,        selection, selection_result,
      invocation,    invocation_result, 1u};
  w_seed_parallel_provider1_counts counts;
  w_seed_parallel_provider1_result measured;
  CHECK(w_seed_parallel_provider1_measure(&input, &counts, &measured) ==
            W_SEED_PARALLEL_PROVIDER1_OK &&
        counts.outcomes == 5u && counts.workspace_values == 5u &&
        measured.written.outcomes == 0u &&
        measured.written.workspace_values == 0u);

  w_seed_parallel_provider1_outcome sequential_outcomes[5];
  int64_t sequential_workspace[5];
  const w_seed_parallel_provider1_output sequential_output = {
      sequential_outcomes, 5u, sequential_workspace, 5u};
  w_seed_parallel_provider1_result sequential_result;
  w_seed_parallel_provider1_receipt sequential_receipt;
  CHECK(w_seed_parallel_provider1_execute(
            &input, &sequential_output, &sequential_result,
            &sequential_receipt) == W_SEED_PARALLEL_PROVIDER1_OK &&
        w_seed_parallel_provider1_verify_outcomes(
            &input, sequential_outcomes, 5u, &sequential_result) &&
        sequential_receipt.provider_kind ==
            W_SEED_PARALLEL_PROVIDER1_KIND_WINDOWS_KERNEL32 &&
        sequential_receipt.provider_capacity == 1u &&
        sequential_receipt.task_count == 5u &&
        sequential_receipt.started_count == 5u &&
        sequential_receipt.completed_count == 5u &&
        sequential_receipt.maximum_active_workers == 1u &&
        !sequential_receipt.overlap_observed);
  for (size_t index = 0u; index < 5u; index += 1u)
    CHECK(sequential_outcomes[index].function_index ==
              invocation->tasks[index].function_index &&
          sequential_outcomes[index].value == 21 + (int64_t)(index * 2u) &&
          sequential_workspace[index] == sequential_outcomes[index].value);

  input.provider_capacity = 2u;
  w_seed_parallel_provider1_counts parallel_counts;
  w_seed_parallel_provider1_result parallel_measured;
  CHECK(w_seed_parallel_provider1_measure(
            &input, &parallel_counts, &parallel_measured) ==
            W_SEED_PARALLEL_PROVIDER1_OK &&
        memcmp(&parallel_counts, &counts, sizeof(counts)) == 0 &&
        memcmp(&parallel_measured, &measured, sizeof(measured)) == 0);
  w_seed_parallel_provider1_outcome parallel_outcomes[5];
  int64_t parallel_workspace[5];
  const w_seed_parallel_provider1_output parallel_output = {
      parallel_outcomes, 5u, parallel_workspace, 5u};
  w_seed_parallel_provider1_result parallel_result;
  w_seed_parallel_provider1_receipt parallel_receipt;
  CHECK(w_seed_parallel_provider1_execute(
            &input, &parallel_output, &parallel_result, &parallel_receipt) ==
            W_SEED_PARALLEL_PROVIDER1_OK &&
        memcmp(&parallel_result, &sequential_result,
               sizeof(parallel_result)) == 0 &&
        w_seed_parallel_provider1_verify_outcomes(
            &input, parallel_outcomes, 5u, &parallel_result) &&
        parallel_receipt.provider_capacity == 2u &&
        parallel_receipt.task_count == 5u &&
        parallel_receipt.started_count == 5u &&
        parallel_receipt.completed_count == 5u &&
        parallel_receipt.maximum_active_workers == 2u &&
        parallel_receipt.overlap_observed);
  for (size_t index = 0u; index < 5u; index += 1u)
    CHECK(parallel_outcomes[index].function_index ==
              sequential_outcomes[index].function_index &&
          parallel_outcomes[index].value == sequential_outcomes[index].value);

  w_seed_parallel_lifecycle1_input lifecycle_input = {
      &input, parallel_outcomes, 5u, &parallel_result, 300u};
  w_seed_parallel_lifecycle1_counts lifecycle_counts;
  w_seed_parallel_lifecycle1_result lifecycle_measured;
  CHECK(w_seed_parallel_lifecycle1_measure(
            &lifecycle_input, &lifecycle_counts, &lifecycle_measured) ==
            W_SEED_PARALLEL_LIFECYCLE1_OK &&
        lifecycle_counts.task_specs == 5u &&
        lifecycle_counts.events == 45u &&
        lifecycle_counts.task_records == 5u &&
        lifecycle_counts.trace_events == 45u &&
        lifecycle_measured.written.task_specs == 0u &&
        lifecycle_measured.written.events == 0u &&
        lifecycle_measured.written.task_records == 0u &&
        lifecycle_measured.written.trace_events == 0u);
  union {
    w_seed_parallel_lifecycle1_counts counts;
    w_seed_parallel_lifecycle1_result result;
  } lifecycle_measure_alias;
  (void)memset(&lifecycle_measure_alias, 0x80,
               sizeof(lifecycle_measure_alias));
  unsigned char lifecycle_measure_alias_before[sizeof(lifecycle_measure_alias)];
  (void)memcpy(lifecycle_measure_alias_before, &lifecycle_measure_alias,
               sizeof(lifecycle_measure_alias_before));
  CHECK(w_seed_parallel_lifecycle1_measure(
            &lifecycle_input, &lifecycle_measure_alias.counts,
            &lifecycle_measure_alias.result) ==
            W_SEED_PARALLEL_LIFECYCLE1_ALIAS &&
        memcmp(&lifecycle_measure_alias, lifecycle_measure_alias_before,
               sizeof(lifecycle_measure_alias)) == 0);

  w_seed_task_lifecycle0_task_spec lifecycle_specs[5];
  w_seed_task_lifecycle0_event lifecycle_events[45];
  w_seed_task_lifecycle0_task_record lifecycle_reducer[5];
  w_seed_task_lifecycle0_task_record lifecycle_tasks[5];
  w_seed_task_lifecycle0_event lifecycle_trace[45];
  const w_seed_parallel_lifecycle1_workspace lifecycle_workspace = {
      lifecycle_specs, 5u, lifecycle_events, 45u, lifecycle_reducer, 5u};
  const w_seed_parallel_lifecycle1_output lifecycle_output = {
      lifecycle_tasks, 5u, lifecycle_trace, 45u};
  w_seed_parallel_lifecycle1_result lifecycle_result;
  CHECK(w_seed_parallel_lifecycle1_run(
            &lifecycle_input, &lifecycle_workspace, &lifecycle_output,
            &lifecycle_result) == W_SEED_PARALLEL_LIFECYCLE1_OK &&
        w_seed_parallel_lifecycle1_verify(
            &lifecycle_input, &lifecycle_workspace, &lifecycle_output,
            &lifecycle_result) &&
        lifecycle_result.lifecycle.scope.state ==
            W_SEED_TASK_LIFECYCLE0_SCOPE_JOINED &&
        lifecycle_result.lifecycle.scope.outcome.kind ==
            W_SEED_TASK_LIFECYCLE0_OUTCOME_SUCCESS &&
        lifecycle_result.lifecycle.scope.outcome.success_value == 125 &&
        memcmp(lifecycle_result.provider_outcome_digest,
               parallel_result.outcome_digest,
               sizeof(lifecycle_result.provider_outcome_digest)) == 0);
  for (size_t index = 0u; index < 5u; index += 1u)
    CHECK(lifecycle_tasks[index].state ==
              W_SEED_TASK_LIFECYCLE0_TASK_RELEASED &&
          lifecycle_tasks[index].outcome.kind ==
              W_SEED_TASK_LIFECYCLE0_OUTCOME_SUCCESS &&
          lifecycle_tasks[index].outcome.success_value ==
              parallel_outcomes[index].value);

  input.provider_capacity = 1u;
  lifecycle_input.provider_outcomes = sequential_outcomes;
  lifecycle_input.provider_result = &sequential_result;
  w_seed_task_lifecycle0_task_spec sequential_specs[5];
  w_seed_task_lifecycle0_event sequential_events[45];
  w_seed_task_lifecycle0_task_record sequential_reducer[5];
  w_seed_task_lifecycle0_task_record sequential_tasks[5];
  w_seed_task_lifecycle0_event sequential_trace[45];
  const w_seed_parallel_lifecycle1_workspace sequential_lifecycle_workspace = {
      sequential_specs, 5u, sequential_events, 45u, sequential_reducer, 5u};
  const w_seed_parallel_lifecycle1_output sequential_lifecycle_output = {
      sequential_tasks, 5u, sequential_trace, 45u};
  w_seed_parallel_lifecycle1_result sequential_lifecycle_result;
  CHECK(w_seed_parallel_lifecycle1_run(
            &lifecycle_input, &sequential_lifecycle_workspace,
            &sequential_lifecycle_output, &sequential_lifecycle_result) ==
            W_SEED_PARALLEL_LIFECYCLE1_OK &&
        w_seed_parallel_lifecycle1_verify(
            &lifecycle_input, &sequential_lifecycle_workspace,
            &sequential_lifecycle_output, &sequential_lifecycle_result) &&
        memcmp(&sequential_lifecycle_result, &lifecycle_result,
               sizeof(lifecycle_result)) == 0 &&
        memcmp(sequential_tasks, lifecycle_tasks,
               sizeof(lifecycle_tasks)) == 0 &&
        memcmp(sequential_trace, lifecycle_trace,
               sizeof(lifecycle_trace)) == 0);

  w_seed_task_lifecycle0_task_record short_lifecycle_tasks[5];
  w_seed_task_lifecycle0_event short_lifecycle_trace[45];
  (void)memset(short_lifecycle_tasks, 0x81, sizeof(short_lifecycle_tasks));
  (void)memset(short_lifecycle_trace, 0x82, sizeof(short_lifecycle_trace));
  w_seed_parallel_lifecycle1_result lifecycle_sentinel;
  (void)memset(&lifecycle_sentinel, 0x83, sizeof(lifecycle_sentinel));
  const w_seed_task_lifecycle0_task_record short_lifecycle_tasks_before[5] = {
      short_lifecycle_tasks[0], short_lifecycle_tasks[1],
      short_lifecycle_tasks[2], short_lifecycle_tasks[3],
      short_lifecycle_tasks[4]};
  w_seed_task_lifecycle0_event short_lifecycle_trace_before[45];
  (void)memcpy(short_lifecycle_trace_before, short_lifecycle_trace,
               sizeof(short_lifecycle_trace_before));
  const w_seed_parallel_lifecycle1_result lifecycle_sentinel_before =
      lifecycle_sentinel;
  const w_seed_parallel_lifecycle1_output short_lifecycle_output = {
      short_lifecycle_tasks, 5u, short_lifecycle_trace, 44u};
  CHECK(w_seed_parallel_lifecycle1_run(
            &lifecycle_input, &lifecycle_workspace, &short_lifecycle_output,
            &lifecycle_sentinel) == W_SEED_PARALLEL_LIFECYCLE1_CAPACITY &&
        memcmp(short_lifecycle_tasks, short_lifecycle_tasks_before,
               sizeof(short_lifecycle_tasks)) == 0 &&
        memcmp(short_lifecycle_trace, short_lifecycle_trace_before,
               sizeof(short_lifecycle_trace)) == 0 &&
        memcmp(&lifecycle_sentinel, &lifecycle_sentinel_before,
               sizeof(lifecycle_sentinel)) == 0);

  const w_seed_parallel_provider1_outcome parallel_outcomes_before[5] = {
      parallel_outcomes[0], parallel_outcomes[1], parallel_outcomes[2],
      parallel_outcomes[3], parallel_outcomes[4]};
  lifecycle_input.provider_outcomes = parallel_outcomes;
  lifecycle_input.provider_result = &parallel_result;
  input.provider_capacity = 2u;
  const w_seed_parallel_lifecycle1_workspace lifecycle_alias_workspace = {
      (w_seed_task_lifecycle0_task_spec *)(void *)parallel_outcomes, 5u,
      lifecycle_events, 45u, lifecycle_reducer, 5u};
  CHECK(w_seed_parallel_lifecycle1_run(
            &lifecycle_input, &lifecycle_alias_workspace, &lifecycle_output,
            &lifecycle_sentinel) == W_SEED_PARALLEL_LIFECYCLE1_ALIAS &&
        memcmp(parallel_outcomes, parallel_outcomes_before,
               sizeof(parallel_outcomes)) == 0 &&
        memcmp(&lifecycle_sentinel, &lifecycle_sentinel_before,
               sizeof(lifecycle_sentinel)) == 0);
  w_seed_task_lifecycle0_task_record lifecycle_tasks_before_alias[5];
  w_seed_task_lifecycle0_event lifecycle_trace_before_alias[45];
  (void)memcpy(lifecycle_tasks_before_alias, lifecycle_tasks,
               sizeof(lifecycle_tasks_before_alias));
  (void)memcpy(lifecycle_trace_before_alias, lifecycle_trace,
               sizeof(lifecycle_trace_before_alias));
  CHECK(w_seed_parallel_lifecycle1_run(
            &lifecycle_input, &lifecycle_workspace, &lifecycle_output,
            (w_seed_parallel_lifecycle1_result *)(void *)lifecycle_tasks) ==
            W_SEED_PARALLEL_LIFECYCLE1_ALIAS &&
        memcmp(lifecycle_tasks, lifecycle_tasks_before_alias,
               sizeof(lifecycle_tasks)) == 0 &&
        memcmp(lifecycle_trace, lifecycle_trace_before_alias,
               sizeof(lifecycle_trace)) == 0);

  w_seed_parallel_lifecycle1_result forged_lifecycle = lifecycle_result;
  forged_lifecycle.lifecycle.transaction_digest ^= UINT64_C(1);
  CHECK(!w_seed_parallel_lifecycle1_verify(
      &lifecycle_input, &lifecycle_workspace, &lifecycle_output,
      &forged_lifecycle));
  w_seed_parallel_provider1_result forged_provider = parallel_result;
  forged_provider.outcome_digest[0] ^= 1u;
  lifecycle_input.provider_result = &forged_provider;
  CHECK(w_seed_parallel_lifecycle1_run(
            &lifecycle_input, &lifecycle_workspace, &lifecycle_output,
            &lifecycle_sentinel) == W_SEED_PARALLEL_LIFECYCLE1_INVALID &&
        memcmp(&lifecycle_sentinel, &lifecycle_sentinel_before,
               sizeof(lifecycle_sentinel)) == 0 &&
        memcmp(lifecycle_tasks, sequential_tasks, sizeof(lifecycle_tasks)) ==
            0 &&
        memcmp(lifecycle_trace, sequential_trace, sizeof(lifecycle_trace)) ==
            0);
  lifecycle_input.provider_result = &parallel_result;

  w_seed_parallel_provider1_outcome short_outcomes[4];
  int64_t short_workspace[5];
  (void)memset(short_outcomes, 0x71, sizeof(short_outcomes));
  (void)memset(short_workspace, 0x72, sizeof(short_workspace));
  const w_seed_parallel_provider1_outcome short_outcomes_before[4] = {
      short_outcomes[0], short_outcomes[1], short_outcomes[2],
      short_outcomes[3]};
  const int64_t short_workspace_before[5] = {
      short_workspace[0], short_workspace[1], short_workspace[2],
      short_workspace[3], short_workspace[4]};
  w_seed_parallel_provider1_result result_sentinel;
  w_seed_parallel_provider1_receipt receipt_sentinel;
  (void)memset(&result_sentinel, 0x73, sizeof(result_sentinel));
  (void)memset(&receipt_sentinel, 0x74, sizeof(receipt_sentinel));
  const w_seed_parallel_provider1_result result_before = result_sentinel;
  const w_seed_parallel_provider1_receipt receipt_before = receipt_sentinel;
  const w_seed_parallel_provider1_output short_output = {
      short_outcomes, 4u, short_workspace, 5u};
  CHECK(w_seed_parallel_provider1_execute(
            &input, &short_output, &result_sentinel, &receipt_sentinel) ==
            W_SEED_PARALLEL_PROVIDER1_CAPACITY &&
        memcmp(short_outcomes, short_outcomes_before,
               sizeof(short_outcomes)) == 0 &&
        memcmp(short_workspace, short_workspace_before,
               sizeof(short_workspace)) == 0 &&
        memcmp(&result_sentinel, &result_before, sizeof(result_before)) == 0 &&
        memcmp(&receipt_sentinel, &receipt_before, sizeof(receipt_before)) ==
            0);

  w_seed_parallel_provider1_outcome complete_outcomes[5];
  int64_t short_workspace_values[4];
  (void)memset(complete_outcomes, 0x75, sizeof(complete_outcomes));
  (void)memset(short_workspace_values, 0x76,
               sizeof(short_workspace_values));
  const w_seed_parallel_provider1_outcome complete_outcomes_before[5] = {
      complete_outcomes[0], complete_outcomes[1], complete_outcomes[2],
      complete_outcomes[3], complete_outcomes[4]};
  const int64_t short_workspace_values_before[4] = {
      short_workspace_values[0], short_workspace_values[1],
      short_workspace_values[2], short_workspace_values[3]};
  const w_seed_parallel_provider1_output short_workspace_output = {
      complete_outcomes, 5u, short_workspace_values, 4u};
  CHECK(w_seed_parallel_provider1_execute(
            &input, &short_workspace_output, &result_sentinel,
            &receipt_sentinel) == W_SEED_PARALLEL_PROVIDER1_CAPACITY &&
        memcmp(complete_outcomes, complete_outcomes_before,
               sizeof(complete_outcomes)) == 0 &&
        memcmp(short_workspace_values, short_workspace_values_before,
               sizeof(short_workspace_values)) == 0 &&
        memcmp(&result_sentinel, &result_before, sizeof(result_before)) == 0 &&
        memcmp(&receipt_sentinel, &receipt_before, sizeof(receipt_before)) ==
            0);

  const w_seed_parallel_invocation1_task invocation_tasks_before[5] = {
      invocation->tasks[0], invocation->tasks[1], invocation->tasks[2],
      invocation->tasks[3], invocation->tasks[4]};
  int64_t alias_workspace[5];
  const w_seed_parallel_provider1_output alias_output = {
      (w_seed_parallel_provider1_outcome *)(void *)invocation->tasks, 5u,
      alias_workspace, 5u};
  CHECK(w_seed_parallel_provider1_execute(
            &input, &alias_output, &result_sentinel, &receipt_sentinel) ==
            W_SEED_PARALLEL_PROVIDER1_ALIAS &&
        memcmp(invocation->tasks, invocation_tasks_before,
               sizeof(invocation_tasks_before)) == 0 &&
        memcmp(&result_sentinel, &result_before, sizeof(result_before)) == 0 &&
        memcmp(&receipt_sentinel, &receipt_before, sizeof(receipt_before)) ==
            0);

  w_seed_parallel_provider1_result forged_result = sequential_result;
  forged_result.outcome_digest[0] ^= 1u;
  input.provider_capacity = 1u;
  CHECK(!w_seed_parallel_provider1_verify_outcomes(
      &input, sequential_outcomes, 5u, &forged_result));
  sequential_outcomes[0].value ^= 1;
  CHECK(!w_seed_parallel_provider1_verify_outcomes(
      &input, sequential_outcomes, 5u, &sequential_result));
  sequential_outcomes[0].value ^= 1;
  return true;
}
#endif

enum {
  TEST_SOURCE = 4096,
  TEST_LEXER_FRAMES = 256,
  TEST_TOKENS = 2048,
  TEST_NODES = 4096,
  TEST_PARSE_FRAMES = 2048,
  TEST_ISSUES = 128,
  TEST_MODULES = 8,
  TEST_IMPORTS = 8,
  TEST_IMPORT_ITEMS = 8,
  TEST_STRUCTS = 4,
  TEST_FIELDS = 8,
  TEST_ENUMS = 8,
  TEST_ENUM_CASES = 32,
  TEST_ENUM_CASE_PARAMETERS = 16,
  TEST_SWITCH_ARMS = 32,
  TEST_TYPES = 32,
  TEST_FUNCTIONS = 8,
  TEST_PARAMETERS = 32,
  TEST_ENTRIES = 4,
  TEST_STATEMENTS = 256,
  TEST_EXPRESSIONS = 256,
  TEST_ARGUMENTS = 32,
  TEST_INTERPOLATION_SEGMENTS = 16,
  TEST_SYMBOLS = 32,
  TEST_FACTS = 16,
  TEST_DIAGNOSTICS = 8,
  TEST_RECEIPT = 65536,
  TEST_HIR_IDENTITIES = 32,
  TEST_HIR_RECORDS = 512,
  TEST_HIR_TEXT = 4096,
  TEST_HIR_VALUES = 4096,
  TEST_HIR_RECEIPT = W_SEED_HIR0_MAX_RECEIPT_BYTES,
};

typedef struct {
  uint8_t source_bytes[TEST_SOURCE];
  size_t source_length;
  w_seed_source source;
  w_seed_lexer_frame lexer_frames[TEST_LEXER_FRAMES];
  w_seed_parse_token tokens[TEST_TOKENS];
  w_seed_cst_node nodes[TEST_NODES];
  w_seed_parse_frame parse_frames[TEST_PARSE_FRAMES];
  w_seed_parse_issue issues[TEST_ISSUES];
  w_seed_parser parser;
  w_seed_parse_result parse;
  w_seed_frontend_document document;
  w_seed_frontend_input input;
  w_seed_frontend_module modules[TEST_MODULES];
  w_seed_frontend_import imports[TEST_IMPORTS];
  w_seed_frontend_import_item import_items[TEST_IMPORT_ITEMS];
  w_seed_frontend_struct structs[TEST_STRUCTS];
  w_seed_frontend_field fields[TEST_FIELDS];
  w_seed_frontend_enum enums[TEST_ENUMS];
  w_seed_frontend_enum_case enum_cases[TEST_ENUM_CASES];
  w_seed_frontend_enum_case_parameter
      enum_case_parameters[TEST_ENUM_CASE_PARAMETERS];
  w_seed_frontend_enum_subset_member
      enum_subset_members[TEST_HIR_RECORDS];
  w_seed_frontend_type_declaration type_declarations[TEST_STRUCTS];
  w_seed_frontend_alias aliases[TEST_STRUCTS];
  w_seed_frontend_type types[TEST_TYPES];
  w_seed_frontend_function functions[TEST_FUNCTIONS];
  w_seed_frontend_parameter parameters[TEST_PARAMETERS];
  w_seed_frontend_entry entries[TEST_ENTRIES];
  w_seed_frontend_statement statements[TEST_STATEMENTS];
  w_seed_frontend_expression expressions[TEST_EXPRESSIONS];
  w_seed_frontend_argument arguments[TEST_ARGUMENTS];
  w_seed_frontend_switch_arm switch_arms[TEST_SWITCH_ARMS];
  w_seed_frontend_pattern_capture pattern_captures[TEST_SWITCH_ARMS];
  w_seed_frontend_interpolation_segment
      interpolation_segments[TEST_INTERPOLATION_SEGMENTS];
  w_seed_frontend_symbol symbols[TEST_SYMBOLS];
  w_seed_frontend_fact facts[TEST_FACTS];
  w_seed_frontend_diagnostic diagnostics[TEST_DIAGNOSTICS];
  w_seed_frontend_diagnostic_fact diagnostic_facts[TEST_DIAGNOSTICS * 5];
  w_seed_frontend_diagnostic_item diagnostic_items[TEST_DIAGNOSTICS * 4];
  w_seed_frontend_diagnostic_label diagnostic_labels[TEST_DIAGNOSTICS * 2];
  w_seed_frontend_host_requirement host_requirements[2];
  w_seed_frontend_external_parameter host_parameters[2];
  w_seed_frontend_host_prelude_symbol host_symbols[2];
  w_seed_frontend_host_prelude host_scope;
  w_seed_frontend_domain domains[2];
  w_seed_frontend_external_parameter external_parameters[8];
  w_seed_frontend_external_symbol external_symbols[8];
  w_seed_frontend_external_module external_modules[2];
  w_seed_frontend_resolved_import resolved_imports[4];
  w_seed_frontend_kernel_module kernel_modules[4];
  w_seed_frontend_kernel_binding kernel_bindings[8];
  uint8_t const_bytes[TEST_SOURCE];
  uint8_t frontend_receipt[TEST_RECEIPT];
  w_seed_frontend_output output;
  w_seed_frontend_result result;
  w_seed_hir0_module hir_modules[TEST_HIR_RECORDS];
  w_seed_hir0_identity hir_identities[TEST_HIR_IDENTITIES];
  w_seed_hir0_type hir_types[TEST_HIR_RECORDS];
  w_seed_hir0_enum hir_enums[TEST_HIR_RECORDS];
  w_seed_hir0_enum_case hir_enum_cases[TEST_HIR_RECORDS];
  w_seed_hir0_enum_case_parameter
      hir_enum_case_parameters[TEST_HIR_RECORDS];
  w_seed_hir0_enum_subset_member
      hir_enum_subset_members[TEST_HIR_RECORDS];
  w_seed_hir0_function hir_functions[TEST_HIR_RECORDS];
  w_seed_hir0_parameter hir_parameters[TEST_HIR_RECORDS];
  w_seed_hir0_block hir_blocks[TEST_HIR_RECORDS];
  w_seed_hir0_block_argument hir_block_arguments[TEST_HIR_RECORDS];
  w_seed_hir0_edge_argument hir_edge_arguments[TEST_HIR_RECORDS];
  w_seed_hir0_switch_edge hir_switch_edges[TEST_HIR_RECORDS];
  w_seed_hir0_switch_capture hir_switch_captures[TEST_HIR_RECORDS];
  w_seed_hir0_instruction hir_instructions[TEST_HIR_RECORDS];
  w_seed_hir0_binding hir_bindings[TEST_HIR_RECORDS];
  w_seed_hir0_call hir_calls[TEST_HIR_RECORDS];
  w_seed_hir0_host_parameter hir_host_parameters[TEST_HIR_RECORDS];
  w_seed_hir0_argument hir_arguments[TEST_HIR_RECORDS];
  w_seed_hir0_enum_payload hir_enum_payloads[TEST_HIR_RECORDS];
  w_seed_hir0_requirement hir_requirements[TEST_HIR_RECORDS];
  w_seed_hir0_value hir_values[TEST_HIR_RECORDS];
  w_seed_hir0_interpolation_segment
      hir_interpolation_segments[TEST_HIR_RECORDS];
  w_seed_hir0_terminator hir_terminators[TEST_HIR_RECORDS];
  w_seed_hir0_entry hir_entries[TEST_HIR_RECORDS];
  w_seed_hir0_external_module hir_external_modules[2];
  w_seed_hir0_external_symbol hir_external_symbols[8];
  w_seed_hir0_cleanup hir_cleanups[TEST_HIR_RECORDS];
  uint8_t hir_text[TEST_HIR_TEXT];
  uint8_t hir_value_bytes[TEST_HIR_VALUES];
  uint8_t hir_receipt[TEST_HIR_RECEIPT];
  w_seed_hir0_output hir_output;
  w_seed_hir0_result hir_result;
  w_seed_hir0_counts hir_counts;
  w_seed_hir0_program hir_program;
} hir_fixture;

static hir_fixture fixture;

static w_seed_hir0_input hir_input(void);
static void fill_hir_output(uint8_t value);
static bool hir_output_is_byte(uint8_t value);

static uint32_t edge_value_at(const w_seed_hir0_program *program,
                              size_t terminator_index) {
  const w_seed_hir0_terminator *terminator =
      &program->terminators[terminator_index];
  if (terminator->edge_argument_count == 0u ||
      terminator->first_edge_argument == W_SEED_HIR0_NONE)
    return W_SEED_HIR0_NONE;
  return program->edge_arguments[terminator->first_edge_argument].value_index;
}

static uint32_t edge_value_for(const w_seed_hir0_program *program,
                               const w_seed_hir0_terminator *terminator) {
  return edge_value_at(
      program, (size_t)(terminator - program->terminators));
}

#define EDGE_VALUE_SLOT(program, terminator_index)                            \
  ((program)->edge_arguments[(program)->terminators[(terminator_index)]       \
                                 .first_edge_argument]                        \
       .value_index)

#define FIXTURE_EDGE_VALUE_SLOT(terminator_index)                             \
  (fixture.hir_edge_arguments[fixture.hir_terminators[(terminator_index)]      \
                                  .first_edge_argument]                        \
       .value_index)

static const char CANONICAL_SOURCE[] =
    "fn main() { print(message: \"Hello, world!\", suffix: \"!\") }\n"
    "entry(main)\n";

static const char COMMENTED_SOURCE[] =
    "// harmless comment\n"
    "fn main() {   print(message: \"Hello, world!\", suffix: \"!\")   }\n"
    "\nentry(main)\n";

static bool fixture_parse(const char *text) {
  fixture.source_length = strlen(text);
  CHECK(fixture.source_length < sizeof(fixture.source_bytes));
  (void)memset(&fixture, 0, sizeof(fixture));
  fixture.source_length = strlen(text);
  (void)memcpy(fixture.source_bytes, text, fixture.source_length);
  w_seed_source_error source_error;
  CHECK(w_seed_source_init(
      (w_seed_byte_view){fixture.source_bytes, fixture.source_length},
      &fixture.source, &source_error));
  w_seed_lex_error lex_error;
  CHECK(w_seed_parser_init(
      &fixture.source, (w_seed_span){0u, fixture.source_length},
      (w_seed_foreign_limits){65536u, 256u}, fixture.lexer_frames,
      TEST_LEXER_FRAMES, fixture.tokens, TEST_TOKENS, fixture.nodes,
      TEST_NODES, fixture.parse_frames, TEST_PARSE_FRAMES, fixture.issues,
      TEST_ISSUES, &fixture.parser, &lex_error));
  CHECK(w_seed_parser_parse(&fixture.parser, &fixture.parse));
  fixture.document = (w_seed_frontend_document){
      .logical_source_id = (w_seed_frontend_text){"hir0-test", 9u},
      .module_id = (w_seed_frontend_text){"hir0-test", 9u},
      .local_module_name = (w_seed_frontend_text){"hir0-test", 9u},
      .source = &fixture.source,
      .nodes = fixture.nodes,
      .node_count = fixture.parse.node_count,
      .parse = fixture.parse};
  fixture.input = (w_seed_frontend_input){
      .documents = &fixture.document,
      .document_count = 1u,
      .external_modules = NULL,
      .external_module_count = 0u,
      .host_scope = NULL,
      .import_resolution_complete = false,
      .resolved_imports = NULL,
      .resolved_import_count = 0u};
  fixture.output = (w_seed_frontend_output){
      .modules = fixture.modules,
      .module_capacity = TEST_MODULES,
      .imports = fixture.imports,
      .import_capacity = TEST_IMPORTS,
      .import_items = fixture.import_items,
      .import_item_capacity = TEST_IMPORT_ITEMS,
      .structs = fixture.structs,
      .struct_capacity = TEST_STRUCTS,
      .fields = fixture.fields,
      .field_capacity = TEST_FIELDS,
      .enums = fixture.enums,
      .enum_capacity = TEST_ENUMS,
      .enum_cases = fixture.enum_cases,
      .enum_case_capacity = TEST_ENUM_CASES,
      .enum_case_parameters = fixture.enum_case_parameters,
      .enum_case_parameter_capacity = TEST_ENUM_CASE_PARAMETERS,
      .enum_subset_members = fixture.enum_subset_members,
      .enum_subset_member_capacity = TEST_HIR_RECORDS,
      .type_declarations = fixture.type_declarations,
      .type_declaration_capacity = TEST_STRUCTS,
      .aliases = fixture.aliases,
      .alias_capacity = TEST_STRUCTS,
      .types = fixture.types,
      .type_capacity = TEST_TYPES,
      .functions = fixture.functions,
      .function_capacity = TEST_FUNCTIONS,
      .parameters = fixture.parameters,
      .parameter_capacity = TEST_PARAMETERS,
      .kernel_modules = fixture.kernel_modules,
      .kernel_module_capacity =
          sizeof(fixture.kernel_modules) /
          sizeof(fixture.kernel_modules[0]),
      .kernel_bindings = fixture.kernel_bindings,
      .kernel_binding_capacity =
          sizeof(fixture.kernel_bindings) /
          sizeof(fixture.kernel_bindings[0]),
      .entries = fixture.entries,
      .entry_capacity = TEST_ENTRIES,
      .statements = fixture.statements,
      .statement_capacity = TEST_STATEMENTS,
      .expressions = fixture.expressions,
      .expression_capacity = TEST_EXPRESSIONS,
      .arguments = fixture.arguments,
      .argument_capacity = TEST_ARGUMENTS,
      .switch_arms = fixture.switch_arms,
      .switch_arm_capacity = TEST_SWITCH_ARMS,
      .pattern_captures = fixture.pattern_captures,
      .pattern_capture_capacity = TEST_SWITCH_ARMS,
      .interpolation_segments = fixture.interpolation_segments,
      .interpolation_segment_capacity = TEST_INTERPOLATION_SEGMENTS,
      .symbols = fixture.symbols,
      .symbol_capacity = TEST_SYMBOLS,
      .facts = fixture.facts,
      .fact_capacity = TEST_FACTS,
      .diagnostics = fixture.diagnostics,
      .diagnostic_capacity = TEST_DIAGNOSTICS,
      .diagnostic_facts = fixture.diagnostic_facts,
      .diagnostic_fact_capacity = TEST_DIAGNOSTICS * 5u,
      .diagnostic_items = fixture.diagnostic_items,
      .diagnostic_item_capacity = TEST_DIAGNOSTICS * 4u,
      .diagnostic_labels = fixture.diagnostic_labels,
      .diagnostic_label_capacity = TEST_DIAGNOSTICS * 2u,
      .receipt = fixture.frontend_receipt,
      .receipt_capacity = sizeof(fixture.frontend_receipt),
      .const_bytes = fixture.const_bytes,
      .const_bytes_capacity = sizeof(fixture.const_bytes)};
  return true;
}

static void configure_host(void) {
  fixture.host_requirements[0] = (w_seed_frontend_host_requirement){
      .name = (w_seed_frontend_text){"Console", 7u}};
  fixture.host_parameters[0] = (w_seed_frontend_external_parameter){
      .name = (w_seed_frontend_text){"message", 7u},
      .type = (w_seed_frontend_text){"String", 6u},
      .label_kind = W_SEED_FRONTEND_LABEL_REQUIRED};
  fixture.host_parameters[1] = (w_seed_frontend_external_parameter){
      .name = (w_seed_frontend_text){"suffix", 6u},
      .type = (w_seed_frontend_text){"String", 6u},
      .label_kind = W_SEED_FRONTEND_LABEL_REQUIRED};
  fixture.host_symbols[0] = (w_seed_frontend_host_prelude_symbol){
      .name = (w_seed_frontend_text){"noop", 4u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"()", 2u},
      .is_const = false,
      .requirements = NULL,
      .requirement_count = 0u};
  fixture.host_symbols[1] = (w_seed_frontend_host_prelude_symbol){
      .name = (w_seed_frontend_text){"print", 5u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .parameters = fixture.host_parameters,
      .parameter_count = 2u,
      .return_type = (w_seed_frontend_text){"()", 2u},
      .is_const = false,
      .requirements = fixture.host_requirements,
      .requirement_count = 1u};
  fixture.host_scope = (w_seed_frontend_host_prelude){
      .profile = (w_seed_frontend_text){"native-process@1", 16u},
      .symbols = fixture.host_symbols,
      .symbol_count = 2u};
  fixture.input.host_scope = &fixture.host_scope;
}

static bool fixture_frontend(const char *source) {
  CHECK(fixture_parse(source));
  configure_host();
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output,
                            &fixture.result) == W_SEED_FRONTEND_OK);
  return true;
}

static void configure_parallel_domain(w_seed_frontend_domain_mode mode,
                                      uint32_t capabilities) {
  fixture.domains[0] = (w_seed_frontend_domain){
      .name = (w_seed_frontend_text){W_SEED_FRONTEND_DOMAIN_IDENTITY,
                                     sizeof(W_SEED_FRONTEND_DOMAIN_IDENTITY) -
                                         1u},
      .mode = mode,
      .capabilities = capabilities};
  fixture.input.domains = fixture.domains;
  fixture.input.domain_count = 1u;
}

static bool fixture_parallel_domain_frontend(const char *source,
                                             uint32_t capabilities) {
  CHECK(fixture_parse(source));
  configure_host();
  configure_parallel_domain(W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT,
                            capabilities);
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output,
                            &fixture.result) == W_SEED_FRONTEND_OK);
  return true;
}

static bool test_frontend_inferred_call_interpolation(void) {
  static const char SOURCE[] =
      "enum Course { starter }\n"
      "fn localCall(label: Course): i64 { return 11 }\n"
      "entry {\n"
      "  let x = localCall(label: .starter)\n"
      "  print(message: \"Courses ${x}\", suffix: \"\")\n"
      "}\n";
  CHECK(fixture_parse(SOURCE));
  configure_host();
  w_seed_frontend_counts measured;
  w_seed_frontend_result measure_result;
  CHECK(w_seed_frontend_measure(&fixture.input, &measured, &measure_result) ==
        W_SEED_FRONTEND_OK);
  CHECK(measured.facts == 0u && measure_result.required.facts == 0u &&
        measured.interpolation_segments == 2u);
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output,
                            &fixture.result) == W_SEED_FRONTEND_OK);
  CHECK(fixture.result.status == W_SEED_FRONTEND_OK &&
        fixture.result.written.facts == 0u &&
        fixture.result.written.interpolation_segments ==
            measured.interpolation_segments &&
        fixture.result.written.receipt_bytes == measured.receipt_bytes &&
        fixture.result.required.receipt_bytes == measured.receipt_bytes &&
        fixture.result.receipt_bytes == measured.receipt_bytes);
  CHECK(fixture.result.written.expressions == measured.expressions &&
        fixture.result.written.statements == measured.statements &&
        fixture.result.written.arguments == measured.arguments);

  static const char *const REJECTED[] = {
      "enum Course { starter }\n"
      "fn localCall(label: Course): i64 { return 11 }\n"
      "entry { let x = localCall\n"
      "print(message: \"Courses ${x}\", suffix: \"\") }\n",
      "enum Course { starter }\n"
      "fn localCall(label: Course): i64 { return 11 }\n"
      "entry { let x = localCall.member\n"
      "print(message: \"Courses ${x}\", suffix: \"\") }\n",
      "enum Course { starter }\n"
      "fn localCall(label: Course): i64 { return 11 }\n"
      "entry { let x = localCall(wrong: .starter)\n"
      "print(message: \"Courses ${x}\", suffix: \"\") }\n"};
  for (size_t index = 0u; index < sizeof(REJECTED) / sizeof(REJECTED[0]);
       index += 1u) {
    CHECK(fixture_parse(REJECTED[index]));
    configure_host();
    CHECK(w_seed_frontend_run(&fixture.input, &fixture.output,
                              &fixture.result) != W_SEED_FRONTEND_OK);
  }
  return true;
}

static void configure_process_input_host(void) {
  configure_host();
  fixture.host_parameters[0].label_kind =
      W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY;
  fixture.host_symbols[1].parameter_count = 1u;
}

static void configure_process_external(void) {
  static const w_seed_frontend_text empty = {NULL, 0u};
  fixture.external_symbols[0] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"Arguments", 9u},
      .kind = W_SEED_FRONTEND_EXTERNAL_TYPE,
      .exported = true,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"Arguments", 9u},
      .is_const = false,
      .receiver_type = empty};
  fixture.external_symbols[1] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"Context", 7u},
      .kind = W_SEED_FRONTEND_EXTERNAL_TYPE,
      .exported = true,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"Context", 7u},
      .is_const = false,
      .receiver_type = empty};
  fixture.external_symbols[2] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"ExitCode", 8u},
      .kind = W_SEED_FRONTEND_EXTERNAL_TYPE,
      .exported = true,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"ExitCode", 8u},
      .is_const = false,
      .receiver_type = empty};
  fixture.external_symbols[3] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"success", 7u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .exported = true,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"ExitCode", 8u},
      .is_const = true,
      .receiver_type = (w_seed_frontend_text){"ExitCode", 8u}};
  fixture.external_modules[0] = (w_seed_frontend_external_module){
      .module_id = (w_seed_frontend_text){"std.process", 11u},
      .symbols = fixture.external_symbols,
      .symbol_count = 4u};
  fixture.input.external_modules = fixture.external_modules;
  fixture.input.external_module_count = 1u;
}

static void configure_process_input_external(void) {
  static const w_seed_frontend_text empty = {NULL, 0u};
  configure_process_external();
  fixture.external_parameters[0] = (w_seed_frontend_external_parameter){
      .name = (w_seed_frontend_text){"code", 4u},
      .type = (w_seed_frontend_text){"i64", 3u},
      .label_kind = W_SEED_FRONTEND_LABEL_POSITIONAL_ONLY};
  fixture.external_symbols[4] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"isEmpty", 7u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .exported = true,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"Bool", 4u},
      .is_const = true,
      .receiver_type = (w_seed_frontend_text){"Arguments", 9u}};
  fixture.external_symbols[5] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"failure", 7u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .exported = true,
      .parameters = fixture.external_parameters,
      .parameter_count = 1u,
      .return_type = (w_seed_frontend_text){"ExitCode", 8u},
      .is_const = true,
      .receiver_type = (w_seed_frontend_text){"ExitCode", 8u}};
  fixture.external_symbols[6] = (w_seed_frontend_external_symbol){
      .name = (w_seed_frontend_text){"count", 5u},
      .kind = W_SEED_FRONTEND_EXTERNAL_VALUE,
      .exported = true,
      .parameters = NULL,
      .parameter_count = 0u,
      .return_type = (w_seed_frontend_text){"usize", 5u},
      .is_const = true,
      .receiver_type = (w_seed_frontend_text){"Arguments", 9u}};
  fixture.external_modules[0].symbol_count = 7u;
  (void)empty;
}

static bool resolve_process_import(void) {
  w_seed_module_origin origins[4];
  w_seed_module_scan_result scan_result;
  CHECK(w_seed_module_scan(&fixture.source, fixture.nodes,
                          fixture.parse.node_count, &fixture.parse, origins,
                          4u, &scan_result) == W_SEED_MODULE_SCAN_OK);
  CHECK(scan_result.written == 1u);
  fixture.resolved_imports[0] = (w_seed_frontend_resolved_import){
      .source_document_index = 0u,
      .direct_import_ordinal = origins[0].direct_import_ordinal,
      .import_declaration_span = origins[0].declaration_span,
      .target_kind = W_SEED_FRONTEND_RESOLVED_IMPORT_EXTERNAL_MODULE,
      .target_index = 0u};
  fixture.input.import_resolution_complete = true;
  fixture.input.resolved_imports = fixture.resolved_imports;
  fixture.input.resolved_import_count = 1u;
  return true;
}

static bool fixture_process_frontend(const char *source) {
  CHECK(fixture_parse(source));
  configure_host();
  configure_process_external();
  CHECK(resolve_process_import());
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output,
                            &fixture.result) == W_SEED_FRONTEND_OK);
  return true;
}

static bool fixture_process_input0_frontend(const char *source) {
  CHECK(fixture_parse(source));
  configure_process_input_host();
  configure_process_input_external();
  CHECK(resolve_process_import());
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output,
                            &fixture.result) == W_SEED_FRONTEND_OK);
  return true;
}

static bool fixture_process_parallel_frontend(const char *source) {
  CHECK(fixture_parse(source));
  configure_process_input_host();
  configure_parallel_domain(W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT,
                            W_SEED_FRONTEND_DOMAIN_CAPABILITY_PARALLEL);
  configure_process_input_external();
  CHECK(resolve_process_import());
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output,
                            &fixture.result) == W_SEED_FRONTEND_OK);
  return true;
}

static void setup_hir_output(void) {
  fixture.hir_output = (w_seed_hir0_output){
      .modules = fixture.hir_modules,
      .module_capacity = TEST_HIR_RECORDS,
      .identities = fixture.hir_identities,
      .identity_capacity = TEST_HIR_IDENTITIES,
      .types = fixture.hir_types,
      .type_capacity = TEST_HIR_RECORDS,
      .enums = fixture.hir_enums,
      .enum_capacity = TEST_HIR_RECORDS,
      .enum_cases = fixture.hir_enum_cases,
      .enum_case_capacity = TEST_HIR_RECORDS,
      .enum_case_parameters = fixture.hir_enum_case_parameters,
      .enum_case_parameter_capacity = TEST_HIR_RECORDS,
      .enum_subset_members = fixture.hir_enum_subset_members,
      .enum_subset_member_capacity = TEST_HIR_RECORDS,
      .functions = fixture.hir_functions,
      .function_capacity = TEST_HIR_RECORDS,
      .parameters = fixture.hir_parameters,
      .parameter_capacity = TEST_HIR_RECORDS,
      .blocks = fixture.hir_blocks,
      .block_capacity = TEST_HIR_RECORDS,
      .block_arguments = fixture.hir_block_arguments,
      .block_argument_capacity = TEST_HIR_RECORDS,
      .edge_arguments = fixture.hir_edge_arguments,
      .edge_argument_capacity = TEST_HIR_RECORDS,
      .switch_edges = fixture.hir_switch_edges,
      .switch_edge_capacity = TEST_HIR_RECORDS,
      .switch_captures = fixture.hir_switch_captures,
      .switch_capture_capacity = TEST_HIR_RECORDS,
      .instructions = fixture.hir_instructions,
      .instruction_capacity = TEST_HIR_RECORDS,
      .bindings = fixture.hir_bindings,
      .binding_capacity = TEST_HIR_RECORDS,
      .calls = fixture.hir_calls,
      .call_capacity = TEST_HIR_RECORDS,
      .host_parameters = fixture.hir_host_parameters,
      .host_parameter_capacity = TEST_HIR_RECORDS,
      .arguments = fixture.hir_arguments,
      .argument_capacity = TEST_HIR_RECORDS,
      .enum_payloads = fixture.hir_enum_payloads,
      .enum_payload_capacity = TEST_HIR_RECORDS,
      .requirements = fixture.hir_requirements,
      .requirement_capacity = TEST_HIR_RECORDS,
      .values = fixture.hir_values,
      .value_capacity = TEST_HIR_RECORDS,
      .interpolation_segments = fixture.hir_interpolation_segments,
      .interpolation_segment_capacity = TEST_HIR_RECORDS,
      .terminators = fixture.hir_terminators,
      .terminator_capacity = TEST_HIR_RECORDS,
      .entries = fixture.hir_entries,
      .entry_capacity = TEST_HIR_RECORDS,
      .text_bytes = fixture.hir_text,
      .text_byte_capacity = sizeof(fixture.hir_text),
      .value_bytes = fixture.hir_value_bytes,
      .value_byte_capacity = sizeof(fixture.hir_value_bytes),
      .receipt = fixture.hir_receipt,
      .receipt_capacity = sizeof(fixture.hir_receipt),
      .external_modules = fixture.hir_external_modules,
      .external_module_capacity = 2u,
      .external_symbols = fixture.hir_external_symbols,
      .external_symbol_capacity = 8u,
      .cleanups = fixture.hir_cleanups,
      .cleanup_capacity = TEST_HIR_RECORDS};
}

static bool lower(const char *source) {
  CHECK(fixture_frontend(source));
  setup_hir_output();
  w_seed_hir0_input input = {
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  w_seed_hir0_counts measured;
  w_seed_hir0_result measure_result;
  CHECK(w_seed_hir0_measure(&input, &measured, &measure_result) ==
        W_SEED_HIR0_OK);
  CHECK(measure_result.required.modules == measured.modules);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_OK);
  CHECK(fixture.hir_result.required.modules == measured.modules);
  CHECK(fixture.hir_result.written.receipt_bytes == measured.receipt_bytes);
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output,
                                        &fixture.hir_result,
                                        &fixture.hir_program));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_counts = measured;
  return true;
}

static bool lower_single_print_host(const char *source) {
  CHECK(fixture_parse(source));
  configure_process_input_host();
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output, &fixture.result) ==
        W_SEED_FRONTEND_OK);
  setup_hir_output();
  const w_seed_hir0_input input = {
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  w_seed_hir0_counts measured;
  w_seed_hir0_result measure_result;
  CHECK(w_seed_hir0_measure(&input, &measured, &measure_result) ==
        W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output,
                                        &fixture.hir_result,
                                        &fixture.hir_program));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_counts = measured;
  return true;
}

static bool test_explicit_panic_hir(void) {
  static const char SOURCE[] =
      "fn f(): i64 { panic(\"bounded literal\") }\n"
      "fn main() { }\n"
      "entry(main)\n";
  CHECK(lower(SOURCE));
  CHECK(fixture.hir_program.type_count == 5u &&
        fixture.hir_program.function_count == 2u &&
        fixture.hir_program.block_count == 2u &&
        fixture.hir_program.instruction_count == 0u &&
        fixture.hir_program.call_count == 0u &&
        fixture.hir_program.argument_count == 0u &&
        fixture.hir_program.value_count == 1u &&
        fixture.hir_program.terminator_count == 2u);
  const w_seed_hir0_function *function = &fixture.hir_program.functions[0];
  CHECK(function->return_type == W_SEED_HIR0_TYPE_I64);
  const w_seed_hir0_terminator *terminator =
      &fixture.hir_program.terminators[0];
  CHECK(terminator->kind == W_SEED_HIR0_TERMINATOR_PANIC &&
        terminator->panic_code == W_SEED_HIR0_PANIC_CODE_EXPLICIT &&
        terminator->call_index == W_SEED_HIR0_NONE &&
        terminator->result_type == 4u &&
        fixture.hir_program.types[terminator->result_type].kind ==
            W_SEED_HIR0_TYPE_NEVER &&
        terminator->value_index == 0u);
  const w_seed_hir0_value *message =
      &fixture.hir_program.values[terminator->value_index];
  CHECK(message->kind == W_SEED_HIR0_VALUE_CONST_STRING &&
        message->owner_kind == W_SEED_HIR0_VALUE_OWNER_TERMINATOR &&
        message->owner_index == 0u && message->owner_ordinal == 0u &&
        message->type_index == W_SEED_HIR0_TYPE_STRING &&
        message->byte_offset == 0u && message->byte_count == 15u &&
        memcmp(fixture.hir_program.value_bytes, "bounded literal", 15u) == 0);
  const w_seed_hir0_panic_code saved_code = terminator->panic_code;
  fixture.hir_terminators[0].panic_code = W_SEED_HIR0_PANIC_CODE_INVALID;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_terminators[0].panic_code = saved_code;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_terminators[1].panic_code = W_SEED_HIR0_PANIC_CODE_EXPLICIT;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_terminators[1].panic_code = W_SEED_HIR0_PANIC_CODE_INVALID;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));

  uint8_t semantic_digest[32];
  (void)memcpy(semantic_digest, fixture.hir_result.semantic_digest,
               sizeof(semantic_digest));
  static const char DIFFERENT_MESSAGE[] =
      "fn f(): i64 { panic(\"different literal\") }\n"
      "fn main() { }\n"
      "entry(main)\n";
  CHECK(lower(DIFFERENT_MESSAGE));
  CHECK(memcmp(semantic_digest, fixture.hir_result.semantic_digest,
               sizeof(semantic_digest)) != 0);
  return true;
}

static bool expect_panic_source_rejected(const char *panic_expression) {
  char source[256];
  const int written = snprintf(source, sizeof(source),
                               "fn f(): i64 { %s }\n"
                               "fn main() { }\n"
                               "entry(main)\n",
                               panic_expression);
  CHECK(written > 0 && (size_t)written < sizeof(source));
  CHECK(fixture_parse(source));
  configure_host();
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output,
                            &fixture.result) != W_SEED_FRONTEND_OK);
  return true;
}

static bool test_explicit_panic_rejections(void) {
  CHECK(expect_panic_source_rejected("panic()"));
  CHECK(expect_panic_source_rejected("panic(\"a\", \"b\")"));
  CHECK(expect_panic_source_rejected("panic(message: \"a\")"));
  CHECK(expect_panic_source_rejected("panic(1)"));
  CHECK(expect_panic_source_rejected("panic(\"value ${1}\")"));
  return true;
}

static bool lower_parallel_domain(const char *source) {
  CHECK(fixture_parallel_domain_frontend(
      source, W_SEED_FRONTEND_DOMAIN_CAPABILITY_PARALLEL));
  setup_hir_output();
  const w_seed_hir0_input input = {
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  w_seed_hir0_counts measured;
  w_seed_hir0_result measure_result;
  CHECK(w_seed_hir0_measure(&input, &measured, &measure_result) ==
        W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output,
                                        &fixture.hir_result,
                                        &fixture.hir_program));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_counts = measured;
  return true;
}

static bool lower_process(const char *source) {
  CHECK(fixture_process_frontend(source));
  setup_hir_output();
  const w_seed_hir0_input input = {
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  w_seed_hir0_counts measured;
  w_seed_hir0_result measure_result;
  CHECK(w_seed_hir0_measure(&input, &measured, &measure_result) ==
        W_SEED_HIR0_OK);
  CHECK(measured.external_modules == 1u && measured.external_symbols == 4u &&
        measured.types == 7u);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output,
                                        &fixture.hir_result,
                                        &fixture.hir_program));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_counts = measured;
  return true;
}

static bool lower_process_input0(const char *source) {
  CHECK(fixture_process_input0_frontend(source));
  setup_hir_output();
  const w_seed_hir0_input input = {
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  w_seed_hir0_counts measured;
  w_seed_hir0_result measure_result;
  CHECK(w_seed_hir0_measure(&input, &measured, &measure_result) ==
        W_SEED_HIR0_OK);
  CHECK(measured.external_modules == 1u && measured.external_symbols == 7u &&
        measured.types == 8u && measured.functions == 1u &&
        measured.parameters == 2u && measured.blocks == 3u &&
        measured.instructions == 2u && measured.calls == 2u &&
        measured.arguments == 2u && measured.requirements == 1u &&
        measured.values == 7u && measured.terminators == 3u &&
        measured.entries == 1u && measured.value_bytes == 15u);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output,
                                        &fixture.hir_result,
                                        &fixture.hir_program));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_counts = measured;
  return true;
}

/* The public count witness is intentionally not the terminal-branch fixture
 * above.  Keep this path shape-independent so the scalar count contract is
 * tested independently of the isEmpty branch's fixed record counts. */
static bool lower_process_input0_generic(const char *source) {
  CHECK(fixture_process_input0_frontend(source));
  setup_hir_output();
  const w_seed_hir0_input input = {
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  w_seed_hir0_counts measured;
  w_seed_hir0_result measure_result;
  CHECK(w_seed_hir0_measure(&input, &measured, &measure_result) ==
        W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output,
                                        &fixture.hir_result,
                                        &fixture.hir_program));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_counts = measured;
  return true;
}

static bool test_explicit_panic_process_layout(void) {
  static const char SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { panic(\"process failed\") }\n"
      "entry(run)\n";
  CHECK(lower_process_input0_generic(SOURCE));
  CHECK(fixture.hir_program.type_count == 9u &&
        fixture.hir_program.types[7].kind == W_SEED_HIR0_TYPE_NEVER &&
        fixture.hir_program.types[8].kind == W_SEED_HIR0_TYPE_USIZE &&
        fixture.hir_program.terminator_count == 1u &&
        fixture.hir_program.terminators[0].kind ==
            W_SEED_HIR0_TERMINATOR_PANIC &&
        fixture.hir_program.call_count == 0u &&
        fixture.hir_program.instruction_count == 0u);
  CHECK(memcmp(fixture.hir_program.text_bytes +
                   fixture.hir_program.types[7].name.offset,
               "Never", 5u) == 0 &&
        memcmp(fixture.hir_program.text_bytes +
                   fixture.hir_program.types[8].name.offset,
               "usize", 5u) == 0);
  return true;
}

static bool lower_process_parallel(const char *source) {
  CHECK(fixture_process_parallel_frontend(source));
  setup_hir_output();
  const w_seed_hir0_input input = {
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  w_seed_hir0_counts measured;
  w_seed_hir0_result measure_result;
  CHECK(w_seed_hir0_measure(&input, &measured, &measure_result) ==
        W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output,
                                        &fixture.hir_result,
                                        &fixture.hir_program));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_counts = measured;
  return true;
}

static bool expect_process_count_comparison_operator(
    const char *source, w_seed_hir0_binary_operator expected_operator) {
  CHECK(lower_process_input0_generic(source));
  const w_seed_hir0_program *program = &fixture.hir_program;
  size_t comparison_count = 0u;
  for (size_t value_index = 0u; value_index < program->value_count;
       value_index += 1u) {
    const w_seed_hir0_value *value = &program->values[value_index];
    if (value->kind != W_SEED_HIR0_VALUE_USIZE_COUNT_COMPARISON) continue;
    CHECK(value->binary_operator == expected_operator);
    comparison_count += 1u;
  }
  CHECK(comparison_count == 1u);
  return true;
}

typedef enum {
  PROCESS_INPUT0_BAD_SOURCE_MEMBER,
  PROCESS_INPUT0_BAD_SOURCE_FAILURE_VALUE,
  PROCESS_INPUT0_BAD_SOURCE_FAILURE_LABEL,
  PROCESS_INPUT0_BAD_SOURCE_HANDLER_SIGNATURE,
  PROCESS_INPUT0_BAD_CATALOG_MEMBER,
  PROCESS_INPUT0_BAD_CATALOG_FAILURE_SIGNATURE,
} process_input0_bad_case;

static void reseal_hir_fixture(void);

static bool expect_process_input0_rejected(const char *source,
                                           process_input0_bad_case bad) {
  CHECK(fixture_parse(source));
  configure_process_input_host();
  configure_process_input_external();
  if (bad == PROCESS_INPUT0_BAD_CATALOG_MEMBER)
    fixture.external_symbols[4].name = (w_seed_frontend_text){"empty", 5u};
  if (bad == PROCESS_INPUT0_BAD_CATALOG_FAILURE_SIGNATURE) {
    fixture.external_parameters[0].type =
        (w_seed_frontend_text){"Bool", 4u};
  }
  CHECK(resolve_process_import());
  const w_seed_frontend_status frontend_status =
      w_seed_frontend_run(&fixture.input, &fixture.output, &fixture.result);
  if (frontend_status != W_SEED_FRONTEND_OK) return true;
  setup_hir_output();
  const w_seed_hir0_input input = {
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  w_seed_hir0_counts counts;
  w_seed_hir0_result result;
  CHECK(w_seed_hir0_measure(&input, &counts, &result) != W_SEED_HIR0_OK);
  return true;
}

/* A parser rejection is itself a valid source negative.  If the source gets
 * as far as HIR, require the HIR boundary to reject the raw external owner. */
static bool expect_process_input0_source_rejected(const char *source) {
  if (!fixture_parse(source)) return true;
  configure_process_input_host();
  configure_process_input_external();
  if (!resolve_process_import()) return true;
  if (w_seed_frontend_run(&fixture.input, &fixture.output, &fixture.result) !=
      W_SEED_FRONTEND_OK)
    return true;
  setup_hir_output();
  const w_seed_hir0_input input = {
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  w_seed_hir0_counts counts;
  w_seed_hir0_result result;
  return w_seed_hir0_measure(&input, &counts, &result) != W_SEED_HIR0_OK;
}

static bool test_process_input0_hir(void) {
  static const char SOURCE[] =
      "import {\n"
      "  Arguments as ProcessArguments,\n"
      "  Context as ProcessContext,\n"
      "  ExitCode as ProcessExitCode,\n"
      "} from std.process\n"
      "\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode {\n"
      "  if args.isEmpty {\n"
      "    print(\"missing\")\n"
      "    return .failure(2)\n"
      "  } else {\n"
      "    print(\"received\")\n"
      "    return .success\n"
      "  }\n"
      "}\n"
      "\n"
      "entry(run)\n";
  static const char BAD_MEMBER[] =
      "import {\n"
      "  Arguments as ProcessArguments,\n"
      "  Context as ProcessContext,\n"
      "  ExitCode as ProcessExitCode,\n"
      "} from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode {\n"
      "  if args.empty {\n"
      "    print(\"missing\")\n"
      "    return .failure(2)\n"
      "  } else {\n"
      "    print(\"received\")\n"
      "    return .success\n"
      "  }\n"
      "}\n"
      "entry(run)\n";
  static const char BAD_FAILURE_VALUE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { if args.isEmpty { print(\"missing\") "
      "return .failure(3) } else { print(\"received\") return .success } }\n"
      "entry(run)\n";
  static const char FAILURE_VALUE_SEVEN[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { if args.isEmpty { print(\"missing\") "
      "return .failure(7) } else { print(\"received\") return .success } }\n"
      "entry(run)\n";
  static const char FAILURE_VALUE_MAX[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { if args.isEmpty { print(\"missing\") "
      "return .failure(255) } else { print(\"received\") return .success } }\n"
      "entry(run)\n";
  static const char FAILURE_VALUE_ZERO[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { if args.isEmpty { print(\"missing\") "
      "return .failure(0) } else { print(\"received\") return .success } }\n"
      "entry(run)\n";
  static const char FAILURE_VALUE_OVERFLOW[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { if args.isEmpty { print(\"missing\") "
      "return .failure(256) } else { print(\"received\") return .success } }\n"
      "entry(run)\n";
  static const char FAILURE_VALUE_NEGATIVE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { if args.isEmpty { print(\"missing\") "
      "return .failure(-1) } else { print(\"received\") return .success } }\n"
      "entry(run)\n";
  static const char BAD_FAILURE_LABEL[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { if args.isEmpty { print(\"missing\") "
      "return .failure(code: 2) } else { print(\"received\") "
      "return .success } }\n"
      "entry(run)\n";
  static const char BAD_HANDLER_SIGNATURE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessExitCode): "
      "ProcessExitCode { if args.isEmpty { print(\"missing\") "
      "return .failure(2) } else { print(\"received\") return .success } }\n"
      "entry(run)\n";
  CHECK(lower_process_input0(SOURCE));
  CHECK(fixture.result.written.modules == 1u &&
        fixture.result.written.types == 5u &&
        fixture.result.written.symbols == 5u &&
        fixture.result.written.functions == 1u &&
        fixture.result.written.parameters == 2u &&
        fixture.result.written.statements == 5u &&
        fixture.result.written.expressions == 12u &&
        fixture.result.written.arguments == 3u);
  CHECK(fixture.input.external_modules[0].symbol_count == 7u &&
        fixture.result.written.imports == 1u &&
        fixture.result.written.import_items == 3u);
  CHECK(fixture.output.expressions[1].kind == W_SEED_FRONTEND_EXPR_MEMBER &&
        fixture.output.expressions[1].resolved_external_module_index == 0u &&
        fixture.output.expressions[1].resolved_external_symbol_index == 4u &&
        text_equal(fixture.output.expressions[1].member_name,
                   (w_seed_frontend_text){"isEmpty", 7u}) &&
        fixture.output.expressions[5].kind == W_SEED_FRONTEND_EXPR_ENUM_CASE &&
        fixture.output.expressions[5].resolved_external_module_index == 0u &&
        fixture.output.expressions[5].resolved_external_symbol_index == 5u &&
        fixture.output.expressions[7].kind == W_SEED_FRONTEND_EXPR_CALL &&
        fixture.output.expressions[7].resolved_external_module_index == 0u &&
        fixture.output.expressions[7].resolved_external_symbol_index == 5u &&
        fixture.output.expressions[6].kind == W_SEED_FRONTEND_EXPR_INTEGER &&
        text_equal(fixture.output.expressions[6].spelling,
                   (w_seed_frontend_text){"2", 1u}));
  CHECK(fixture.hir_counts.external_modules == 1u &&
        fixture.hir_counts.external_symbols == 7u &&
        fixture.hir_counts.blocks == 3u && fixture.hir_counts.instructions == 2u &&
        fixture.hir_counts.calls == 2u && fixture.hir_counts.arguments == 2u &&
        fixture.hir_counts.requirements == 1u &&
        fixture.hir_counts.values == 7u && fixture.hir_counts.terminators == 3u);
  CHECK(fixture.hir_program.external_modules[0].symbol_count == 7u &&
        fixture.hir_program.external_symbol_count == 7u &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE &&
        fixture.hir_program.entries[0].adapter_kind ==
            W_SEED_HIR0_ENTRY_ADAPTER_NATIVE_PROCESS &&
        fixture.hir_program.entries[0].cleanup_obligation ==
            W_SEED_HIR0_ENTRY_CLEANUP_RELEASE_HANDLER_OWNERS &&
        fixture.hir_program.terminators[0].kind ==
            W_SEED_HIR0_TERMINATOR_BRANCH &&
        fixture.hir_program.terminators[0].value_index == 3u &&
        fixture.hir_program.terminators[1].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        fixture.hir_program.terminators[1].value_index == 5u &&
        fixture.hir_program.terminators[2].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        fixture.hir_program.terminators[2].value_index == 6u &&
        fixture.hir_program.values[2].kind ==
            W_SEED_HIR0_VALUE_PARAMETER_READ &&
        fixture.hir_program.values[3].kind ==
            W_SEED_HIR0_VALUE_EXTERNAL_MEMBER &&
        fixture.hir_program.values[3].left_value == 2u &&
        fixture.hir_program.values[3].external_symbol_index == 4u &&
        fixture.hir_program.values[4].kind == W_SEED_HIR0_VALUE_CONST_I64 &&
        fixture.hir_program.values[4].integer_value == 2 &&
        fixture.hir_program.values[5].kind ==
            W_SEED_HIR0_VALUE_EXTERNAL_ENUM_CASE &&
        fixture.hir_program.values[5].left_value == 4u &&
        fixture.hir_program.values[5].external_symbol_index == 5u &&
        fixture.hir_program.values[6].kind ==
            W_SEED_HIR0_VALUE_EXTERNAL_ENUM_CASE &&
        fixture.hir_program.values[6].left_value == W_SEED_HIR0_NONE &&
        fixture.hir_program.values[6].external_symbol_index == 3u &&
        hir_text_is(&fixture.hir_program, fixture.hir_program.values[3].member_name,
                    "isEmpty") &&
        hir_text_is(&fixture.hir_program, fixture.hir_program.values[5].member_name,
                    "failure") &&
        hir_text_is(&fixture.hir_program, fixture.hir_program.values[6].member_name,
                    "success"));
  CHECK(expect_process_input0_rejected(
      BAD_MEMBER, PROCESS_INPUT0_BAD_SOURCE_MEMBER));
  CHECK(lower_process_input0(BAD_FAILURE_VALUE));
  CHECK(lower_process_input0(FAILURE_VALUE_SEVEN));
  CHECK(lower_process_input0(FAILURE_VALUE_MAX));
  CHECK(expect_process_input0_rejected(
      FAILURE_VALUE_ZERO, PROCESS_INPUT0_BAD_SOURCE_FAILURE_VALUE));
  CHECK(expect_process_input0_rejected(
      FAILURE_VALUE_OVERFLOW, PROCESS_INPUT0_BAD_SOURCE_FAILURE_VALUE));
  CHECK(expect_process_input0_rejected(
      FAILURE_VALUE_NEGATIVE, PROCESS_INPUT0_BAD_SOURCE_FAILURE_VALUE));
  CHECK(expect_process_input0_rejected(
      BAD_FAILURE_LABEL, PROCESS_INPUT0_BAD_SOURCE_FAILURE_LABEL));
  CHECK(expect_process_input0_rejected(
      BAD_HANDLER_SIGNATURE, PROCESS_INPUT0_BAD_SOURCE_HANDLER_SIGNATURE));
  CHECK(expect_process_input0_rejected(SOURCE, PROCESS_INPUT0_BAD_CATALOG_MEMBER));
  CHECK(expect_process_input0_rejected(
      SOURCE, PROCESS_INPUT0_BAD_CATALOG_FAILURE_SIGNATURE));

  /* The public MLIR adapter emits these verified HIR literals. Resealing a
   * different byte sequence must not preserve the public witness identity. */
  CHECK(lower_process_input0(SOURCE));
  const uint8_t saved_literal_byte = fixture.hir_value_bytes[0];
  fixture.hir_value_bytes[0] = 'x';
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_value_bytes[0] = saved_literal_byte;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  /* The special process route cannot silently drop orphan capture records. */
  fixture.result.required.pattern_captures = 1u;
  fixture.result.written.pattern_captures = 1u;
  const w_seed_hir0_input forged = {
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  w_seed_hir0_counts measured = fixture.hir_counts;
  w_seed_hir0_result measured_result = fixture.hir_result;
  CHECK(w_seed_hir0_measure(&forged, &measured, &measured_result) != W_SEED_HIR0_OK);
  CHECK(memcmp(&measured, &fixture.hir_counts, sizeof(measured)) == 0 &&
        memcmp(&measured_result, &fixture.hir_result, sizeof(measured_result)) == 0);
  return true;
}

static void reseal_hir_fixture(void) {
  uint8_t semantic_digest[32];
  uint8_t provenance_digest[32];
  digest_program(&fixture.hir_program, &fixture.hir_counts, semantic_digest);
  digest_provenance(&fixture.hir_program, &fixture.hir_counts,
                    provenance_digest);
  (void)memcpy(fixture.hir_result.semantic_digest, semantic_digest,
               sizeof(semantic_digest));
  (void)memcpy(fixture.hir_result.provenance_digest, provenance_digest,
               sizeof(provenance_digest));
  write_receipt_unchecked(fixture.hir_receipt, &fixture.hir_counts,
                          semantic_digest, provenance_digest);
}

static bool test_process_arguments_count_hir(void) {
  static const char COUNT_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { if args.count == 2 { print(\"count 2\") "
      "return .success } else { print(\"count ${args.count}\") "
      "return .success } }\n"
      "entry(run)\n";
  static const char COUNT_FLAT_IMPORT_SOURCE[] =
      "import std.process\n"
      "async fn run(args: Arguments, ctx: Context): ExitCode { "
      "if args.count == 2 { print(\"count 2\") return .success } "
      "else { print(\"count ${args.count}\") return .success } }\n"
      "entry(run)\n";
  static const char COUNT_BIND_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { let observed = args.count print(\"count\") "
      "return .success }\n"
      "entry(run)\n";
  static const char COUNT_COMPARISON_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { let matches = args.count == 0 "
      "print(\"matches ${matches}\") return .success }\n"
      "entry(run)\n";
  static const char COUNT_COMPARISON_REVERSED_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${0 != args.count}\") "
      "return .success }\n"
      "entry(run)\n";
  static const char COUNT_COMPARISON_LESS_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${args.count < 2}\") "
      "return .success }\n"
      "entry(run)\n";
  static const char COUNT_COMPARISON_LESS_EQUAL_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${args.count <= 2}\") "
      "return .success }\n"
      "entry(run)\n";
  static const char COUNT_COMPARISON_GREATER_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${args.count > 2}\") "
      "return .success }\n"
      "entry(run)\n";
  static const char COUNT_COMPARISON_GREATER_EQUAL_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${args.count >= 2}\") "
      "return .success }\n"
      "entry(run)\n";
  static const char COUNT_COMPARISON_REVERSED_LESS_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${2 < args.count}\") "
      "return .success }\n"
      "entry(run)\n";
  static const char COUNT_COMPARISON_REVERSED_LESS_EQUAL_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${2 <= args.count}\") "
      "return .success }\n"
      "entry(run)\n";
  static const char COUNT_COMPARISON_REVERSED_GREATER_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${2 > args.count}\") "
      "return .success }\n"
      "entry(run)\n";
  static const char COUNT_COMPARISON_REVERSED_GREATER_EQUAL_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${2 >= args.count}\") "
      "return .success }\n"
      "entry(run)\n";
  static const char COUNT_COMPARISON_IF_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { if args.count != 0 { return .failure(1) } "
      "else { return .success } }\n"
      "entry(run)\n";
  static const char OPTIONAL_MEMBER_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"count ${args?.count}\") "
      "return .success }\n"
      "entry(run)\n";

  CHECK(expect_process_count_comparison_operator(
      COUNT_COMPARISON_LESS_SOURCE, W_SEED_HIR0_BINARY_LESS));
  CHECK(expect_process_count_comparison_operator(
      COUNT_COMPARISON_LESS_EQUAL_SOURCE, W_SEED_HIR0_BINARY_LESS_EQUAL));
  CHECK(expect_process_count_comparison_operator(
      COUNT_COMPARISON_GREATER_SOURCE, W_SEED_HIR0_BINARY_GREATER));
  CHECK(expect_process_count_comparison_operator(
      COUNT_COMPARISON_GREATER_EQUAL_SOURCE,
      W_SEED_HIR0_BINARY_GREATER_EQUAL));
  CHECK(expect_process_count_comparison_operator(
      COUNT_COMPARISON_REVERSED_LESS_SOURCE, W_SEED_HIR0_BINARY_LESS));
  CHECK(expect_process_count_comparison_operator(
      COUNT_COMPARISON_REVERSED_LESS_EQUAL_SOURCE,
      W_SEED_HIR0_BINARY_LESS_EQUAL));
  CHECK(expect_process_count_comparison_operator(
      COUNT_COMPARISON_REVERSED_GREATER_SOURCE,
      W_SEED_HIR0_BINARY_GREATER));
  CHECK(expect_process_count_comparison_operator(
      COUNT_COMPARISON_REVERSED_GREATER_EQUAL_SOURCE,
      W_SEED_HIR0_BINARY_GREATER_EQUAL));

  CHECK(lower_process_input0_generic(COUNT_SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  size_t usize_type = SIZE_MAX;
  size_t arguments_type = SIZE_MAX;
  for (size_t type = 0u; type < program->type_count; type += 1u) {
    if (program->types[type].kind == W_SEED_HIR0_TYPE_USIZE)
      usize_type = type;
    if (program->types[type].kind == W_SEED_HIR0_TYPE_NOMINAL &&
        program->types[type].external_module_index == 0u &&
        program->types[type].external_symbol_index == 0u)
      arguments_type = type;
  }
  CHECK(usize_type != SIZE_MAX && arguments_type != SIZE_MAX &&
        usize_type == 7u + program->enum_count &&
        program->types[usize_type].lifecycle ==
            W_SEED_HIR0_LIFECYCLE_VALUE_COPY &&
        program->types[usize_type].release_contract ==
            W_SEED_HIR0_RELEASE_CONTRACT_NONE);
  CHECK(program->external_symbol_count == 7u &&
        program->external_symbols[6].parameter_count == 0u &&
        program->external_symbols[6].parameter_abi ==
            W_SEED_HIR0_EXTERNAL_PARAMETER_NONE &&
        hir_text_is(program, program->external_symbols[6].name, "count") &&
        hir_text_is(program, program->external_symbols[6].receiver_type,
                    "Arguments") &&
        hir_text_is(program, program->external_symbols[6].return_type,
                    "usize"));

  uint32_t count_member = W_SEED_HIR0_NONE;
  size_t count_member_count = 0u;
  for (size_t value_index = 0u; value_index < program->value_count;
       value_index += 1u) {
    const w_seed_hir0_value *value = &program->values[value_index];
    if (value->kind != W_SEED_HIR0_VALUE_EXTERNAL_MEMBER ||
        value->external_module_index != 0u ||
        value->external_symbol_index != 6u ||
        value->type_index != usize_type ||
        !hir_text_is(program, value->member_name, "count"))
      continue;
    CHECK(value->left_value < program->value_count);
    count_member = (uint32_t)value_index;
    count_member_count += 1u;
    const w_seed_hir0_value *receiver = &program->values[value->left_value];
    CHECK(receiver->kind == W_SEED_HIR0_VALUE_PARAMETER_READ &&
          receiver->type_index == arguments_type && receiver->parameter_index <
              program->parameter_count &&
          program->parameters[receiver->parameter_index].ordinal == 0u);
  }
  CHECK(count_member_count == 2u && count_member != W_SEED_HIR0_NONE);

  bool interpolated_count = false;
  for (size_t value_index = 0u; value_index < program->value_count;
       value_index += 1u) {
    const w_seed_hir0_value *value = &program->values[value_index];
    if (value->kind != W_SEED_HIR0_VALUE_INTERPOLATED_STRING ||
        value->first_interpolation_segment == W_SEED_HIR0_NONE)
      continue;
    CHECK((size_t)value->first_interpolation_segment +
              value->interpolation_segment_count <=
          program->interpolation_segment_count);
    for (size_t segment = 0u; segment < value->interpolation_segment_count;
         segment += 1u) {
      const w_seed_hir0_interpolation_segment *item =
          &program->interpolation_segments[
              (size_t)value->first_interpolation_segment + segment];
      if (item->kind == W_SEED_HIR0_INTERPOLATION_VALUE &&
          item->value_index < program->value_count) {
        const w_seed_hir0_value *embedded =
            &program->values[item->value_index];
        if (embedded->kind == W_SEED_HIR0_VALUE_EXTERNAL_MEMBER &&
            embedded->external_module_index == 0u &&
            embedded->external_symbol_index == 6u &&
            hir_text_is(program, embedded->member_name, "count"))
          interpolated_count = true;
      }
    }
  }
  CHECK(interpolated_count);

  uint8_t selective_semantic_digest[32];
  uint8_t selective_provenance_digest[32];
  (void)memcpy(selective_semantic_digest,
               fixture.hir_result.semantic_digest,
               sizeof(selective_semantic_digest));
  (void)memcpy(selective_provenance_digest,
               fixture.hir_result.provenance_digest,
               sizeof(selective_provenance_digest));
  CHECK(lower_process_input0_generic(COUNT_FLAT_IMPORT_SOURCE));
  program = &fixture.hir_program;
  CHECK(fixture.result.written.imports == 1u &&
        fixture.result.written.import_items == 1u &&
        program->external_module_count == 1u &&
        program->external_symbol_count == 7u &&
        memcmp(selective_semantic_digest,
               fixture.hir_result.semantic_digest,
               sizeof(selective_semantic_digest)) == 0 &&
        memcmp(selective_provenance_digest,
               fixture.hir_result.provenance_digest,
               sizeof(selective_provenance_digest)) != 0);

  /* A usize member is an ordinary scalar binding, independent of the
   * process adapter's fixed positive source shape. */
  CHECK(lower_process_input0_generic(COUNT_BIND_SOURCE));
  program = &fixture.hir_program;
  size_t observed_binding = SIZE_MAX;
  for (size_t binding = 0u; binding < program->binding_count; binding += 1u)
    if (hir_text_is(program, program->bindings[binding].name, "observed"))
      observed_binding = binding;
  CHECK(observed_binding != SIZE_MAX &&
        program->bindings[observed_binding].initializer_value <
            program->value_count);
  const w_seed_hir0_value *observed =
      &program->values[program->bindings[observed_binding].initializer_value];
  CHECK(observed->kind == W_SEED_HIR0_VALUE_EXTERNAL_MEMBER &&
        observed->type_index < program->type_count &&
        program->types[observed->type_index].kind == W_SEED_HIR0_TYPE_USIZE &&
        observed->external_module_index == 0u &&
        observed->external_symbol_index == 6u &&
        hir_text_is(program, observed->member_name, "count"));

  CHECK(lower_process_input0_generic(COUNT_COMPARISON_SOURCE));
  program = &fixture.hir_program;
  size_t count_comparisons = 0u;
  size_t count_comparison_literal = 0u;
  uint32_t comparison_index = W_SEED_HIR0_NONE;
  uint32_t comparison_literal_index = W_SEED_HIR0_NONE;
  for (size_t value_index = 0u; value_index < program->value_count;
       value_index += 1u) {
    const w_seed_hir0_value *value = &program->values[value_index];
    if (value->kind != W_SEED_HIR0_VALUE_USIZE_COUNT_COMPARISON) continue;
    CHECK(value->type_index < program->type_count &&
          program->types[value->type_index].kind == W_SEED_HIR0_TYPE_BOOL &&
          (value->binary_operator == W_SEED_HIR0_BINARY_EQUAL ||
           value->binary_operator == W_SEED_HIR0_BINARY_NOT_EQUAL) &&
          value->left_value < program->value_count &&
          value->right_value < program->value_count);
    count_comparisons += 1u;
    comparison_index = (uint32_t)value_index;
    const w_seed_hir0_value *left = &program->values[value->left_value];
    const w_seed_hir0_value *right = &program->values[value->right_value];
    const w_seed_hir0_value *member =
        left->kind == W_SEED_HIR0_VALUE_EXTERNAL_MEMBER ? left : right;
    const w_seed_hir0_value *literal =
        left->kind == W_SEED_HIR0_VALUE_CONST_USIZE ? left : right;
    if (member->kind == W_SEED_HIR0_VALUE_EXTERNAL_MEMBER &&
        literal->kind == W_SEED_HIR0_VALUE_CONST_USIZE &&
        member->type_index == literal->type_index &&
        literal->unsigned_integer_value == UINT64_C(0)) {
      count_comparison_literal += 1u;
      comparison_literal_index =
          (uint32_t)(literal - program->values);
    }
  }
  CHECK(count_comparisons == 1u && count_comparison_literal == 1u &&
        comparison_index != W_SEED_HIR0_NONE &&
        comparison_literal_index != W_SEED_HIR0_NONE);

  const w_seed_hir0_value saved_comparison =
      fixture.hir_values[comparison_index];
  const w_seed_hir0_value saved_comparison_literal =
      fixture.hir_values[comparison_literal_index];
  fixture.hir_values[comparison_index].binary_operator =
      W_SEED_HIR0_BINARY_ADD;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[comparison_index] = saved_comparison;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[comparison_literal_index].kind =
      W_SEED_HIR0_VALUE_CONST_I64;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[comparison_literal_index] = saved_comparison_literal;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[comparison_literal_index].type_index = 3u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[comparison_literal_index] = saved_comparison_literal;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[comparison_index].unsigned_integer_value = UINT64_C(1);
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[comparison_index] = saved_comparison;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  CHECK(lower_process_input0_generic(COUNT_COMPARISON_REVERSED_SOURCE));
  program = &fixture.hir_program;
  count_comparisons = 0u;
  for (size_t value_index = 0u; value_index < program->value_count;
       value_index += 1u)
    if (program->values[value_index].kind ==
        W_SEED_HIR0_VALUE_USIZE_COUNT_COMPARISON)
      count_comparisons += 1u;
  CHECK(count_comparisons == 1u);

  CHECK(lower_process_input0_generic(COUNT_COMPARISON_IF_SOURCE));
  program = &fixture.hir_program;
  CHECK(program->functions[0].block_count == 3u &&
        program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[0].value_index < program->value_count &&
        program->values[program->terminators[0].value_index].kind ==
            W_SEED_HIR0_VALUE_USIZE_COUNT_COMPARISON);

  static const char *const COUNT_COMPARISON_REJECTED[] = {
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"${args.count < (1 + 1)}\") "
      "return .success }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"${args.count + 1}\") "
      "return .success }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"${args.count == -1}\") "
      "return .success }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { let saved = args.count "
      "print(\"${saved == 0}\") return .success }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"${ctx.count == 0}\") "
      "return .success }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "fn helper(value: usize) { }\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return .success }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "fn helper(): usize { return 0 }\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return .success }\n"
      "entry(run)\n"};
  for (size_t index = 0u;
       index < sizeof(COUNT_COMPARISON_REJECTED) /
                   sizeof(COUNT_COMPARISON_REJECTED[0]);
       index += 1u)
    CHECK(expect_process_input0_source_rejected(
        COUNT_COMPARISON_REJECTED[index]));

  /* Raw external owners stay blocked in every ownership position. */
  static const char *const RAW_REJECTED[] = {
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { let saved = args return .success }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { let saved = ctx return .success }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { let copied = copy args return .success }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { let copied = copy ctx return .success }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "fn takeArgs(value: ProcessArguments) { }\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { takeArgs(value: args) return .success }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "fn takeContext(value: ProcessContext) { }\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { takeContext(value: ctx) return .success }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return args }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return ctx }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return args.length }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return args[0] }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return ctx.count }\n"
      "entry(run)\n"};
  for (size_t index = 0u;
       index < sizeof(RAW_REJECTED) / sizeof(RAW_REJECTED[0]); index += 1u)
    CHECK(expect_process_input0_source_rejected(RAW_REJECTED[index]));

  /* The parser must preserve `?.`; the non-Option Arguments receiver is then
   * rejected by the frontend/HIR boundary rather than lowered as `.`. */
  CHECK(fixture_parse(OPTIONAL_MEMBER_SOURCE));
  configure_process_input_host();
  configure_process_input_external();
  CHECK(resolve_process_import());
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output, &fixture.result) ==
        W_SEED_FRONTEND_OK);
  bool saw_optional_count = false;
  for (size_t expression = 0u; expression < fixture.result.written.expressions;
       expression += 1u) {
    const w_seed_frontend_expression *value = &fixture.output.expressions[expression];
    if (value->kind == W_SEED_FRONTEND_EXPR_MEMBER &&
        text_is(value->member_name, "count")) {
      CHECK(text_is(value->operator_text, "?."));
      saw_optional_count = true;
    }
  }
  CHECK(saw_optional_count);
  setup_hir_output();
  const w_seed_hir0_input optional_input = {
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  w_seed_hir0_counts optional_counts;
  w_seed_hir0_result optional_result;
  CHECK(w_seed_hir0_measure(&optional_input, &optional_counts,
                             &optional_result) != W_SEED_HIR0_OK);

  /* Each mutation is resealed so the verifier, not the digest guard, owns the
   * rejection.  Reload the same positive witness before every independent
   * mutation and restore it after the check. */
  CHECK(lower_process_input0_generic(COUNT_SOURCE));
  program = &fixture.hir_program;
  uint32_t count_value = W_SEED_HIR0_NONE;
  for (size_t value = 0u; value < program->value_count; value += 1u)
    if (program->values[value].kind == W_SEED_HIR0_VALUE_EXTERNAL_MEMBER &&
        program->values[value].external_symbol_index == 6u &&
        hir_text_is(program, program->values[value].member_name, "count")) {
      count_value = (uint32_t)value;
      break;
    }
  CHECK(count_value != W_SEED_HIR0_NONE);
  const w_seed_hir0_value saved_count_value = fixture.hir_values[count_value];
  const w_seed_hir0_external_symbol saved_count_symbol =
      fixture.hir_external_symbols[6];

  fixture.hir_values[count_value].member_name =
      fixture.hir_external_symbols[4].name;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[count_value] = saved_count_value;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_external_symbols[6].name = fixture.hir_external_symbols[4].name;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_external_symbols[6] = saved_count_symbol;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[count_value].external_symbol_index = 4u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[count_value] = saved_count_value;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_external_symbols[6].receiver_type =
      fixture.hir_external_symbols[1].name;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_external_symbols[6] = saved_count_symbol;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_external_symbols[6].return_type =
      fixture.hir_external_symbols[4].return_type;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_external_symbols[6] = saved_count_symbol;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[count_value].type_index = W_SEED_HIR0_TYPE_BOOL;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[count_value] = saved_count_value;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_external_symbols[6].parameter_abi =
      W_SEED_HIR0_EXTERNAL_PARAMETER_PROCESS_FAILURE_I64;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_external_symbols[6] = saved_count_symbol;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[count_value].owner_index = W_SEED_HIR0_NONE;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[count_value] = saved_count_value;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_process_hir(void) {
  static const char canonical[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return .success }\n"
      "entry(run)\n";
  static const char variant[] =
      "import { Arguments as ProcessArgs, Context as ProcessCtx, "
      "ExitCode as ProcessStatus } from std.process\n"
      "async fn run(args: ProcessArgs, ctx: ProcessCtx): ProcessStatus { "
      "return .success }\n"
      "entry(run)\n";
  CHECK(lower_process(canonical));
  CHECK(fixture.hir_counts.external_modules == 1u &&
        fixture.hir_counts.external_symbols == 4u &&
        fixture.hir_counts.types == 7u && fixture.hir_counts.functions == 1u &&
        fixture.hir_counts.parameters == 2u && fixture.hir_counts.values == 1u);
  CHECK(fixture.hir_program.external_modules[0].module_index == 0u &&
        fixture.hir_program.external_modules[0].first_symbol == 0u &&
        fixture.hir_program.external_modules[0].symbol_count == 4u &&
        fixture.hir_program.external_symbol_count == 4u);
  CHECK(fixture.hir_program.types[4].kind == W_SEED_HIR0_TYPE_NOMINAL &&
        fixture.hir_program.types[4].external_module_index == 0u &&
        fixture.hir_program.types[4].external_symbol_index == 0u &&
        fixture.hir_program.types[5].external_symbol_index == 1u &&
        fixture.hir_program.types[6].external_symbol_index == 2u);
  CHECK(fixture.hir_program.types[0].lifecycle ==
            W_SEED_HIR0_LIFECYCLE_VALUE_COPY &&
        fixture.hir_program.types[0].release_contract ==
            W_SEED_HIR0_RELEASE_CONTRACT_NONE &&
        fixture.hir_program.types[1].lifecycle ==
            W_SEED_HIR0_LIFECYCLE_UNKNOWN &&
        fixture.hir_program.types[1].release_contract ==
            W_SEED_HIR0_RELEASE_CONTRACT_UNKNOWN &&
        fixture.hir_program.types[2].lifecycle ==
            W_SEED_HIR0_LIFECYCLE_VALUE_COPY &&
        fixture.hir_program.types[2].release_contract ==
            W_SEED_HIR0_RELEASE_CONTRACT_NONE &&
        fixture.hir_program.types[3].lifecycle ==
            W_SEED_HIR0_LIFECYCLE_VALUE_COPY &&
        fixture.hir_program.types[3].release_contract ==
            W_SEED_HIR0_RELEASE_CONTRACT_NONE &&
        fixture.hir_program.types[4].lifecycle ==
            W_SEED_HIR0_LIFECYCLE_ENTRY_ROOT_OWNER &&
        fixture.hir_program.types[4].release_contract ==
            W_SEED_HIR0_RELEASE_CONTRACT_PROCESS_V1_WRAPPER_RELEASE &&
        fixture.hir_program.types[5].lifecycle ==
            W_SEED_HIR0_LIFECYCLE_ENTRY_ROOT_OWNER &&
        fixture.hir_program.types[5].release_contract ==
            W_SEED_HIR0_RELEASE_CONTRACT_PROCESS_V1_WRAPPER_RELEASE &&
        fixture.hir_program.types[6].lifecycle ==
            W_SEED_HIR0_LIFECYCLE_VALUE_COPY &&
        fixture.hir_program.types[6].release_contract ==
            W_SEED_HIR0_RELEASE_CONTRACT_NONE);
  CHECK(fixture.hir_program.functions[0].is_async &&
        fixture.hir_program.functions[0].return_type == 6u &&
        fixture.hir_program.functions[0].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE &&
        fixture.hir_program.parameters[0].type_index == 4u &&
        fixture.hir_program.parameters[1].type_index == 5u &&
        fixture.hir_program.entries[0].adapter_kind ==
            W_SEED_HIR0_ENTRY_ADAPTER_NATIVE_PROCESS &&
        fixture.hir_program.entries[0].cleanup_obligation ==
            W_SEED_HIR0_ENTRY_CLEANUP_RELEASE_HANDLER_OWNERS &&
        fixture.hir_program.entries[0].first_cleanup_owner_parameter == 0u &&
        fixture.hir_program.entries[0].cleanup_owner_parameter_count == 2u);
  CHECK(fixture.hir_program.values[0].kind ==
            W_SEED_HIR0_VALUE_EXTERNAL_ENUM_CASE &&
        fixture.hir_program.values[0].type_index == 6u &&
        fixture.hir_program.values[0].external_module_index == 0u &&
        fixture.hir_program.values[0].external_symbol_index == 3u &&
        fixture.hir_program.values[0].member_name.count == 7u &&
        memcmp(fixture.hir_text + fixture.hir_program.values[0].member_name.offset,
               "success", 7u) == 0);

  uint8_t semantic[32];
  uint8_t provenance[32];
  (void)memcpy(semantic, fixture.hir_result.semantic_digest, sizeof(semantic));
  (void)memcpy(provenance, fixture.hir_result.provenance_digest,
               sizeof(provenance));
  /* The HIR owns canonical external names and does not retain resolver
   * pointers or source aliases. */
  fixture.external_symbols[0].name = (w_seed_frontend_text){NULL, 9u};
  fixture.external_modules[0].module_id = (w_seed_frontend_text){NULL, 11u};
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));

  CHECK(lower_process(variant));
  CHECK(memcmp(semantic, fixture.hir_result.semantic_digest,
               sizeof(semantic)) == 0);
  CHECK(memcmp(provenance, fixture.hir_result.provenance_digest,
               sizeof(provenance)) != 0);

  /* HIR-consumer mutations are rejected without reparsing aliases. */
  w_seed_hir0_type saved_type = fixture.hir_types[4];
  fixture.hir_types[4].external_symbol_index = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_types[4] = saved_type;
  w_seed_hir0_external_symbol saved_symbol = fixture.hir_external_symbols[0];
  fixture.hir_external_symbols[0] = fixture.hir_external_symbols[1];
  fixture.hir_external_symbols[1] = saved_symbol;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  saved_symbol = fixture.hir_external_symbols[0];
  fixture.hir_external_symbols[0] = fixture.hir_external_symbols[1];
  fixture.hir_external_symbols[1] = saved_symbol;
  w_seed_hir0_entry saved_entry = fixture.hir_entries[0];
  fixture.hir_entries[0].adapter_kind =
      W_SEED_HIR0_ENTRY_ADAPTER_DEFAULT_UNIT;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_entries[0] = saved_entry;
  w_seed_hir0_value saved_value = fixture.hir_values[0];
  fixture.hir_values[0].external_symbol_index = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_values[0] = saved_value;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool test_direct_entry_facts(void) {
  static const char PURE_SOURCE[] =
      "async fn quote(count: i64): i64 { return count + 1 }\n"
      "entry { }\n";
  CHECK(lower(PURE_SOURCE));
  CHECK(fixture.hir_program.function_count == 2u &&
        fixture.hir_program.functions[0].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE);

  static const char EMPTY_ASYNC_SOURCE[] =
      "async fn empty() { }\n"
      "entry { }\n";
  CHECK(lower(EMPTY_ASYNC_SOURCE));
  CHECK(fixture.hir_program.function_count == 2u &&
        fixture.hir_program.functions[0].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE);

  /* A zero-count frontend family may omit its caller-owned storage. */
  CHECK(fixture_parse(EMPTY_ASYNC_SOURCE));
  configure_host();
  fixture.output.statements = NULL;
  fixture.output.statement_capacity = 0u;
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output,
                            &fixture.result) == W_SEED_FRONTEND_OK);
  CHECK(fixture.result.written.statements == 0u);
  setup_hir_output();
  const w_seed_hir0_input empty_input = hir_input();
  w_seed_hir0_counts empty_counts;
  w_seed_hir0_result empty_measure;
  CHECK(w_seed_hir0_measure(&empty_input, &empty_counts, &empty_measure) ==
        W_SEED_HIR0_OK);
  CHECK(empty_counts.blocks == 2u && empty_counts.terminators == 2u);
  CHECK(w_seed_hir0_run(&empty_input, &fixture.hir_output,
                        &fixture.hir_result) == W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output,
                                        &fixture.hir_result,
                                        &fixture.hir_program));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  CHECK(fixture.hir_program.functions[0].direct_entry ==
        W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE);

  static const char HELPER_SOURCE[] =
      "fn baseTotal(count: i64): i64 { return count + 1 }\n"
      "fn orderTotal(count: i64): i64 { "
      "let subtotal = baseTotal(count: count) return subtotal }\n"
      "async fn quote(count: i64): i64 { "
      "let total = orderTotal(count: count) return total }\n"
      "entry { }\n";
  CHECK(lower(HELPER_SOURCE));
  CHECK(fixture.hir_program.function_count == 4u &&
        fixture.hir_program.functions[0].suspension ==
            W_SEED_HIR0_SUSPENSION_NEVER &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT &&
        fixture.hir_program.functions[1].suspension ==
            W_SEED_HIR0_SUSPENSION_NEVER &&
        fixture.hir_program.functions[1].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT &&
        fixture.hir_program.functions[2].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[2].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE);

  static const char REVERSED_HELPER_SOURCE[] =
      "async fn reverseQuote(count: i64): i64 { "
      "let total = reverseTotal(count: count) return total }\n"
      "fn reverseTotal(count: i64): i64 { "
      "let subtotal = reverseBase(count: count) return subtotal }\n"
      "fn reverseBase(count: i64): i64 { return count + 1 }\n"
      "entry { }\n";
  CHECK(lower(REVERSED_HELPER_SOURCE));
  CHECK(fixture.hir_program.function_count == 4u &&
        fixture.hir_program.functions[0].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE &&
        fixture.hir_program.functions[1].suspension ==
            W_SEED_HIR0_SUSPENSION_NEVER &&
        fixture.hir_program.functions[1].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT &&
        fixture.hir_program.functions[2].suspension ==
            W_SEED_HIR0_SUSPENSION_NEVER &&
        fixture.hir_program.functions[2].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT);

  static const char POISONED_REVERSED_SOURCE[] =
      "fn unrelated(value: Bool): Bool { return value }\n"
      "async fn reverseQuote(count: i64): i64 { "
      "let total = reverseTotal(count: count) return total }\n"
      "fn reverseTotal(count: i64): i64 { "
      "let subtotal = reverseBase(count: count) return subtotal }\n"
      "fn reverseBase(count: i64): i64 { "
      "print(message: \"poison\", suffix: \"\") return count }\n"
      "fn cycleA(value: Bool): Bool { "
      "let next = cycleB(value: value) return next }\n"
      "fn cycleB(value: Bool): Bool { "
      "print(message: \"cycle\", suffix: \"\") "
      "let next = cycleA(value: value) return next }\n"
      "entry { }\n";
  CHECK(lower(POISONED_REVERSED_SOURCE));
  CHECK(fixture.hir_program.function_count == 7u &&
        fixture.hir_program.functions[0].suspension ==
            W_SEED_HIR0_SUSPENSION_NEVER &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT &&
        fixture.hir_program.functions[1].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[1].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT &&
        fixture.hir_program.functions[2].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[2].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT &&
        fixture.hir_program.functions[3].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[4].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[5].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY);

  static const char HOST_SOURCE[] =
      "async fn announce() { "
      "print(message: \"order\", suffix: \"\") }\n"
      "entry { }\n";
  CHECK(lower(HOST_SOURCE));
  CHECK(fixture.hir_program.function_count == 2u &&
        fixture.hir_program.functions[0].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT);

  static const char STRING_SOURCE[] =
      "async fn retain(value: String) { let echoed = value }\n"
      "entry { }\n";
  CHECK(lower(STRING_SOURCE));
  CHECK(fixture.hir_program.function_count == 2u &&
        fixture.hir_program.functions[0].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT);

  static const char ASYNC_BRANCH_SOURCE[] =
      "async fn maybe(): Bool { return true }\n"
      "async fn choose() { "
      "if false { let selected = maybe() } "
      "else { let fallback = true } }\n"
      "entry { }\n";
  CHECK(lower(ASYNC_BRANCH_SOURCE));
  CHECK(fixture.hir_program.function_count == 3u &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE &&
        fixture.hir_program.functions[1].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[1].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT);

  static const char RECURSIVE_SOURCE[] =
      "fn left(value: Bool): Bool { let next = right(value: value) return next }\n"
      "fn right(value: Bool): Bool { let next = left(value: value) return next }\n"
      "async fn choose(value: Bool): Bool { "
      "let next = left(value: value) return next }\n"
      "entry { }\n";
  CHECK(lower(RECURSIVE_SOURCE));
  CHECK(fixture.hir_program.function_count == 4u &&
        fixture.hir_program.functions[0].suspension ==
            W_SEED_HIR0_SUSPENSION_NEVER &&
        fixture.hir_program.functions[1].suspension ==
            W_SEED_HIR0_SUSPENSION_NEVER &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT &&
        fixture.hir_program.functions[1].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT &&
        fixture.hir_program.functions[2].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE);

  const w_seed_hir0_function saved = fixture.hir_functions[2];
  fixture.hir_functions[2].suspension = W_SEED_HIR0_SUSPENSION_NEVER;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_functions[2] = saved;
  fixture.hir_functions[2].direct_entry =
      W_SEED_HIR0_DIRECT_ENTRY_ABSENT;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_functions[2] = saved;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool check_direct_entry_effect_barrier(
    const char *source, w_seed_frontend_status expected_status) {
  CHECK(fixture_parse(source));
  configure_host();
  const w_seed_frontend_status status =
      w_seed_frontend_run(&fixture.input, &fixture.output, &fixture.result);
  CHECK(status == expected_status);
  for (size_t fact = 0u; fact < fixture.result.written.facts; fact += 1u)
    CHECK(fixture.facts[fact].kind !=
              W_SEED_FRONTEND_FACT_UNRESOLVED_IMPORTED_SYMBOL &&
          fixture.facts[fact].kind !=
              W_SEED_FRONTEND_FACT_UNRESOLVED_LOCAL_SYMBOL);

  setup_hir_output();
  fill_hir_output(0xa5u);
  const w_seed_hir0_input input = hir_input();
  (void)memset(&fixture.hir_counts, 0x6au, sizeof(fixture.hir_counts));
  const w_seed_hir0_counts counts_before = fixture.hir_counts;
  (void)memset(&fixture.hir_result, 0x5au, sizeof(fixture.hir_result));
  const w_seed_hir0_result result_before = fixture.hir_result;
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) ==
        W_SEED_HIR0_FRONTEND);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_FRONTEND);
  CHECK(hir_output_is_byte(0xa5u) &&
        memcmp(&fixture.hir_counts, &counts_before, sizeof(counts_before)) ==
            0 &&
        memcmp(&fixture.hir_result, &result_before, sizeof(result_before)) ==
            0);
  return true;
}

static bool test_direct_entry_effect_barrier(void) {
  static const char UNSUPPORTED_EFFECT_SOURCE[] =
      "async fn value(): i64 { return 1 }\n"
      "async fn wait(): i64 { return await value() }\n"
      "entry { }\n";
  CHECK(check_direct_entry_effect_barrier(UNSUPPORTED_EFFECT_SOURCE,
                                          W_SEED_FRONTEND_UNSUPPORTED));

  static const char DEFER_EFFECT_SOURCE[] =
      "async fn cleanup() { }\n"
      "async fn deferWork() { defer async { await cleanup() } }\n"
      "entry { }\n";
  CHECK(check_direct_entry_effect_barrier(DEFER_EFFECT_SOURCE,
                                          W_SEED_FRONTEND_UNSUPPORTED));
  return true;
}

static bool test_canonical_and_copy_boundary(void) {
  CHECK(lower(CANONICAL_SOURCE));
  CHECK(fixture.hir_counts.modules == 1u);
  CHECK(fixture.hir_counts.identities == 5u);
  CHECK(fixture.hir_counts.types == 4u);
  CHECK(fixture.hir_counts.functions == 1u);
  CHECK(fixture.hir_counts.parameters == 0u);
  CHECK(fixture.hir_counts.blocks == 1u);
  CHECK(fixture.hir_counts.instructions == 1u);
  CHECK(fixture.hir_counts.bindings == 0u);
  CHECK(fixture.hir_counts.calls == 1u);
  CHECK(fixture.hir_counts.host_parameters == 2u);
  CHECK(fixture.hir_counts.arguments == 2u);
  CHECK(fixture.hir_counts.requirements == 1u);
  CHECK(fixture.hir_counts.values == 2u);
  CHECK(fixture.hir_counts.terminators == 1u);
  CHECK(fixture.hir_counts.entries == 1u);
  CHECK(fixture.hir_program.identities[3].profile.count == 16u);
  CHECK(memcmp(fixture.hir_text + fixture.hir_program.identities[3].profile.offset,
               "native-process@1", 16u) == 0);
  CHECK(fixture.hir_program.identities[4].first_parameter == 0u &&
        fixture.hir_program.identities[4].parameter_count == 2u &&
        fixture.hir_program.host_parameters[0].owner_identity == 4u &&
        fixture.hir_program.host_parameters[1].owner_identity == 4u &&
        fixture.hir_program.host_parameters[0].ordinal == 0u &&
        fixture.hir_program.host_parameters[1].ordinal == 1u &&
        fixture.hir_program.host_parameters[0].type_index == 1u &&
        fixture.hir_program.host_parameters[1].type_index == 1u);
  CHECK(fixture.hir_program.host_parameters[0].label.count == 7u &&
        fixture.hir_program.host_parameters[1].label.count == 6u &&
        memcmp(fixture.hir_text + fixture.hir_program.host_parameters[0].label.offset,
               "message", 7u) == 0 &&
        memcmp(fixture.hir_text + fixture.hir_program.host_parameters[1].label.offset,
               "suffix", 6u) == 0);
  CHECK(fixture.hir_program.entries[0].slot.count == 8u);
  CHECK(memcmp(fixture.hir_text + fixture.hir_program.entries[0].slot.offset,
               ".default", 8u) == 0);
  CHECK(fixture.hir_program.host_parameters[0].ordinal == 0u &&
        fixture.hir_program.host_parameters[1].ordinal == 1u &&
        fixture.hir_program.host_parameters[0].label_kind ==
            W_SEED_HIR0_LABEL_REQUIRED &&
        fixture.hir_program.host_parameters[1].label_kind ==
            W_SEED_HIR0_LABEL_REQUIRED);
  CHECK(fixture.hir_program.arguments[0].label_kind ==
            W_SEED_HIR0_LABEL_REQUIRED &&
        fixture.hir_program.arguments[1].label_kind ==
            W_SEED_HIR0_LABEL_REQUIRED &&
        fixture.hir_program.arguments[0].label.count == 7u &&
        fixture.hir_program.arguments[1].label.count == 6u);
  CHECK(fixture.hir_program.arguments[0].owner_call == 0u &&
        fixture.hir_program.arguments[1].owner_call == 0u &&
        fixture.hir_program.arguments[0].ordinal == 0u &&
        fixture.hir_program.arguments[1].ordinal == 1u &&
        fixture.hir_program.arguments[0].value_index == 0u &&
        fixture.hir_program.arguments[1].value_index == 1u);
  CHECK(fixture.hir_program.calls[0].callee_identity == 4u);
  CHECK(fixture.hir_program.values[0].byte_count == 13u);
  CHECK(memcmp(fixture.hir_value_bytes + fixture.hir_program.values[0].byte_offset,
               "Hello, world!", 13u) == 0);
  CHECK(fixture.hir_program.values[1].byte_count == 1u &&
        memcmp(fixture.hir_value_bytes + fixture.hir_program.values[1].byte_offset,
               "!", 1u) == 0);
  (void)memset(&fixture.document, 0xa5, sizeof(fixture.document));
  (void)memset(&fixture.input, 0xa5, sizeof(fixture.input));
  (void)memset(&fixture.output, 0xa5, sizeof(fixture.output));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool test_semantic_and_provenance_digests(void) {
  uint8_t semantic[32];
  uint8_t provenance[32];
  CHECK(lower(CANONICAL_SOURCE));
  (void)memcpy(semantic, fixture.hir_result.semantic_digest, sizeof(semantic));
  (void)memcpy(provenance, fixture.hir_result.provenance_digest,
               sizeof(provenance));
  CHECK(lower(COMMENTED_SOURCE));
  CHECK(memcmp(semantic, fixture.hir_result.semantic_digest, sizeof(semantic)) ==
        0);
  CHECK(memcmp(provenance, fixture.hir_result.provenance_digest,
               sizeof(provenance)) != 0);
  return true;
}

static bool test_function_parameter_records(void) {
  static const char SOURCE[] =
      "fn main(value: String) { print(message: \"Hello, world!\", suffix: \"!\") }\n"
      "entry(main)\n";
  CHECK(lower(SOURCE));
  CHECK(fixture.hir_counts.parameters == 1u);
  const w_seed_hir0_parameter *parameter = &fixture.hir_program.parameters[0];
  CHECK(parameter->owner_function == 0u && parameter->ordinal == 0u &&
        parameter->type_index == W_SEED_HIR0_TYPE_STRING &&
        parameter->label_kind == W_SEED_HIR0_LABEL_REQUIRED &&
        parameter->name.count == 5u && parameter->label.count == 5u);
  CHECK(memcmp(fixture.hir_text + parameter->name.offset, "value", 5u) == 0);
  CHECK(memcmp(fixture.hir_text + parameter->label.offset, "value", 5u) == 0);
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool test_structured_async_elision_hir(void) {
  static const char SOURCE[] =
      "fn prepare(value: i64): i64 { return value }\n"
      "entry {\n"
      "  let left = async prepare(value: 20)\n"
      "  let right = async prepare(value: 22)\n"
      "  let first = await left\n"
      "  let second = await right\n"
      "}\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 2u && program->call_count == 2u &&
        program->binding_count == 4u && program->instruction_count == 6u &&
        program->argument_count == 2u && program->value_count == 7u);
  CHECK(program->calls[0].execution_kind ==
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_ELIDED &&
        program->calls[1].execution_kind ==
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_ELIDED);
  CHECK(program->bindings[0].task_peer_binding == 2u &&
        program->bindings[1].task_peer_binding == 3u &&
        program->bindings[2].task_peer_binding == 0u &&
        program->bindings[3].task_peer_binding == 1u);
  CHECK(program->bindings[0].task_role == W_SEED_HIR0_TASK_ROLE_LAUNCH &&
        program->bindings[1].task_role == W_SEED_HIR0_TASK_ROLE_LAUNCH &&
        program->bindings[2].task_role ==
            W_SEED_HIR0_TASK_ROLE_AWAIT_RESULT &&
        program->bindings[3].task_role ==
            W_SEED_HIR0_TASK_ROLE_AWAIT_RESULT);
  CHECK(program->values[program->bindings[0].initializer_value].kind ==
            W_SEED_HIR0_VALUE_CALL_RESULT &&
        program->values[program->bindings[1].initializer_value].kind ==
            W_SEED_HIR0_VALUE_CALL_RESULT &&
        program->values[program->bindings[2].initializer_value].kind ==
            W_SEED_HIR0_VALUE_BINDING_READ &&
        program->values[program->bindings[3].initializer_value].kind ==
            W_SEED_HIR0_VALUE_BINDING_READ);

  const w_seed_hir0_function saved_prepare = fixture.hir_functions[0];
  fixture.hir_functions[0].is_throws = true;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_functions[0] = saved_prepare;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_call saved_call = fixture.hir_calls[0];
  fixture.hir_calls[0].execution_kind = W_SEED_HIR0_CALL_DIRECT;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_calls[0] = saved_call;

  fixture.hir_calls[0].execution_kind =
      (w_seed_hir0_call_execution_kind)-1;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_calls[0] = saved_call;

  const w_seed_hir0_binding saved_launch = fixture.hir_bindings[0];
  fixture.hir_bindings[0].task_peer_binding = W_SEED_HIR0_NONE;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_bindings[0] = saved_launch;

  fixture.hir_bindings[0].task_role = W_SEED_HIR0_TASK_ROLE_NONE;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_bindings[0] = saved_launch;

  const w_seed_hir0_binding saved_join = fixture.hir_bindings[2];
  fixture.hir_bindings[2].task_peer_binding = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_bindings[2] = saved_join;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  static const char ASYNC_STATIC_YIELD_SOURCE[] =
      "async fn prepare(value: i64): i64 { "
      "let staged = value + 1 await execution#yield() "
      "await execution#yield() "
      "return staged * 2 }\n"
      "entry { let pending = async prepare(value: 20) "
      "let result = await pending }\n";
  CHECK(lower(ASYNC_STATIC_YIELD_SOURCE));
  program = &fixture.hir_program;
  CHECK(program->function_count == 2u && program->call_count == 1u &&
        program->binding_count == 3u && program->instruction_count == 6u &&
        program->functions[0].is_async &&
        program->functions[0].suspension == W_SEED_HIR0_SUSPENSION_MAY &&
        program->functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT &&
        program->calls[0].execution_kind ==
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_STATIC_YIELDS_ELIDED);
  size_t yield_instruction = W_SEED_HIR0_NONE;
  size_t yield_count = 0u;
  for (size_t index = 0u; index < program->instruction_count; index += 1u)
    if (program->instructions[index].kind ==
        W_SEED_HIR0_INSTRUCTION_EXECUTION_YIELD) {
      yield_instruction = index;
      yield_count += 1u;
      CHECK(program->instructions[index].call_index == W_SEED_HIR0_NONE &&
            program->instructions[index].binding_index == W_SEED_HIR0_NONE &&
            program->instructions[index].result_type == 0u);
    }
  CHECK(yield_count == 2u && yield_instruction != W_SEED_HIR0_NONE &&
        program->instructions[yield_instruction].call_index ==
            W_SEED_HIR0_NONE &&
        program->instructions[yield_instruction].binding_index ==
            W_SEED_HIR0_NONE &&
        program->instructions[yield_instruction].result_type == 0u &&
        w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_instruction saved_yield =
      fixture.hir_instructions[yield_instruction];
  fixture.hir_instructions[yield_instruction].kind =
      W_SEED_HIR0_INSTRUCTION_BINDING;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_instructions[yield_instruction] = saved_yield;
  const w_seed_hir0_call saved_static_yield_call = fixture.hir_calls[0];
  fixture.hir_calls[0].execution_kind =
      W_SEED_HIR0_CALL_STRUCTURED_ASYNC_ELIDED;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_calls[0] = saved_static_yield_call;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_functions[0].is_async = false;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_functions[0].is_async = true;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  static const char SPAWN_MAIN_SOURCE[] =
      "async fn prepare(value: i64): i64 { "
      "let staged = value + 1 await execution#yield() "
      "await execution#yield() return staged * 2 }\n"
      "entry { let left = spawn<.main> prepare(value: 20) "
      "let right = spawn<.main> prepare(value: 22) "
      "let first = await left let second = await right "
      "print(\"Dispatched ${first + second}\") }\n";
  CHECK(lower_single_print_host(SPAWN_MAIN_SOURCE));
  program = &fixture.hir_program;
  size_t main_dispatches = 0u;
  size_t first_main_dispatch = W_SEED_HIR0_NONE;
  for (size_t call = 0u; call < program->call_count; call += 1u) {
    if (program->calls[call].execution_kind !=
        W_SEED_HIR0_CALL_STRUCTURED_ASYNC_MAIN_DISPATCH)
      continue;
    if (first_main_dispatch == W_SEED_HIR0_NONE) first_main_dispatch = call;
    main_dispatches += 1u;
  }
  CHECK(main_dispatches == 2u &&
        first_main_dispatch != W_SEED_HIR0_NONE &&
        w_seed_hir0_verify(program, &fixture.hir_result));
  const w_seed_hir0_call saved_main_dispatch =
      fixture.hir_calls[first_main_dispatch];
  fixture.hir_calls[first_main_dispatch].execution_kind =
      W_SEED_HIR0_CALL_STRUCTURED_ASYNC_STATIC_YIELDS_ELIDED;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_calls[first_main_dispatch] = saved_main_dispatch;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  static const char SPAWN_MAIN_FIVE_SOURCE[] =
      "async fn prepare(value: i64): i64 { "
      "await execution#yield() return value + 1 }\n"
      "entry { let a = spawn<.main> prepare(value: 20) "
      "let b = spawn<.main> prepare(value: 22) "
      "let c = spawn<.main> prepare(value: 24) "
      "let d = spawn<.main> prepare(value: 26) "
      "let e = spawn<.main> prepare(value: 28) "
      "let av = await a let bv = await b let cv = await c let dv = await d "
      "let ev = await e }\n";
  CHECK(lower(SPAWN_MAIN_FIVE_SOURCE));
  program = &fixture.hir_program;
  main_dispatches = 0u;
  for (size_t call = 0u; call < program->call_count; call += 1u)
    if (program->calls[call].execution_kind ==
        W_SEED_HIR0_CALL_STRUCTURED_ASYNC_MAIN_DISPATCH)
      main_dispatches += 1u;
  CHECK(main_dispatches == 5u &&
        w_seed_hir0_verify(program, &fixture.hir_result));

  uint32_t task_type = W_SEED_FRONTEND_NONE;
  uint32_t scalar_expression = W_SEED_FRONTEND_NONE;
  for (size_t type = 0u; type < fixture.result.written.types; type += 1u)
    if (fixture.types[type].kind == W_SEED_FRONTEND_TYPE_TASK) {
      task_type = (uint32_t)type;
      break;
    }
  for (size_t expression = 0u; expression < fixture.result.written.expressions;
       expression += 1u)
    if (fixture.expressions[expression].kind == W_SEED_FRONTEND_EXPR_INTEGER) {
      scalar_expression = (uint32_t)expression;
      break;
    }
  CHECK(task_type != W_SEED_FRONTEND_NONE &&
        scalar_expression != W_SEED_FRONTEND_NONE);
  fixture.expressions[scalar_expression].inferred_type = task_type;
  w_seed_hir0_input forged_input = {
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  w_seed_hir0_counts forged_counts;
  w_seed_hir0_result forged_result;
  CHECK(w_seed_hir0_measure(&forged_input, &forged_counts, &forged_result) !=
        W_SEED_HIR0_OK);

  static const char BOOL_SOURCE[] =
      "fn confirm(value: Bool): Bool { return value }\n"
      "entry { let pending = async confirm(value: true) "
      "let accepted = await pending }\n";
  CHECK(lower(BOOL_SOURCE));
  program = &fixture.hir_program;
  CHECK(program->call_count == 1u && program->binding_count == 2u &&
        program->calls[0].execution_kind ==
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_ELIDED &&
        program->bindings[0].task_role == W_SEED_HIR0_TASK_ROLE_LAUNCH &&
        program->bindings[1].task_role ==
            W_SEED_HIR0_TASK_ROLE_AWAIT_RESULT &&
        program->bindings[0].task_peer_binding == 1u &&
        program->bindings[1].task_peer_binding == 0u);

  static const char ASYNC_I64_SOURCE[] =
      "async fn pause(value: i64): i64 { return value }\n"
      "entry { let pending = async pause(value: 42) "
      "let result = await pending }\n";
  CHECK(lower(ASYNC_I64_SOURCE));
  program = &fixture.hir_program;
  CHECK(program->function_count == 2u && program->call_count == 1u &&
        program->functions[0].is_async &&
        program->functions[0].suspension == W_SEED_HIR0_SUSPENSION_MAY &&
        program->functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE &&
        program->calls[0].execution_kind ==
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_ELIDED);
  const w_seed_hir0_function saved_async_function =
      fixture.hir_functions[0];
  const w_seed_hir0_call saved_async_call = fixture.hir_calls[0];
  fixture.hir_functions[0].direct_entry =
      W_SEED_HIR0_DIRECT_ENTRY_ABSENT;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_functions[0] = saved_async_function;
  fixture.hir_calls[0].execution_kind = W_SEED_HIR0_CALL_DIRECT;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_calls[0].execution_kind =
      W_SEED_HIR0_CALL_STRUCTURED_ASYNC_STATIC_YIELDS_ELIDED;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_calls[0] = saved_async_call;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  static const char ASYNC_BOOL_SOURCE[] =
      "async fn confirm(value: Bool): Bool { return value }\n"
      "entry { let pending = async confirm(value: true) "
      "let result = await pending }\n";
  CHECK(lower(ASYNC_BOOL_SOURCE));
  program = &fixture.hir_program;
  CHECK(program->function_count == 2u && program->call_count == 1u &&
        program->functions[0].is_async &&
        program->functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE &&
        program->calls[0].execution_kind ==
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_ELIDED);

  static const char ASYNC_EFFECT_SOURCE[] =
      "async fn pause(value: i64): i64 { "
      "print(message: \"yielded\", suffix: \"\") return value }\n"
      "entry { let pending = async pause(value: 42) "
      "let result = await pending }\n";
  CHECK(fixture_frontend(ASYNC_EFFECT_SOURCE));
  setup_hir_output();
  fill_hir_output(0xa5u);
  w_seed_hir0_input effect_input = {
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  w_seed_hir0_counts effect_counts;
  w_seed_hir0_result effect_result;
  CHECK(w_seed_hir0_measure(&effect_input, &effect_counts, &effect_result) ==
        W_SEED_HIR0_UNSUPPORTED);
  CHECK(hir_output_is_byte(0xa5u));
  (void)memset(&effect_result, 0x5au, sizeof(effect_result));
  const w_seed_hir0_result effect_result_before = effect_result;
  CHECK(w_seed_hir0_run(&effect_input, &fixture.hir_output, &effect_result) ==
        W_SEED_HIR0_UNSUPPORTED);
  CHECK(hir_output_is_byte(0xa5u) &&
        memcmp(&effect_result, &effect_result_before,
               sizeof(effect_result)) == 0);

  static const char UNIT_SOURCE[] =
      "fn finish() {}\nentry { let pending = async finish() "
      "let done = await pending }\n";
  CHECK(fixture_frontend(UNIT_SOURCE));
  setup_hir_output();
  w_seed_hir0_input input = {
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  w_seed_hir0_counts counts;
  w_seed_hir0_result result;
  CHECK(w_seed_hir0_measure(&input, &counts, &result) ==
        W_SEED_HIR0_UNSUPPORTED);

  static const char STATIC_YIELD_UNIT_SOURCE[] =
      "async fn finish() { await execution#yield() }\n"
      "entry { let pending = async finish() let done = await pending }\n";
  CHECK(fixture_frontend(STATIC_YIELD_UNIT_SOURCE));
  setup_hir_output();
  input = (w_seed_hir0_input){
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  CHECK(w_seed_hir0_measure(&input, &counts, &result) ==
        W_SEED_HIR0_UNSUPPORTED);

  static const char ONE_YIELD_SOURCE[] =
      "async fn pause(value: i64): i64 { await execution#yield() "
      "return value }\n"
      "entry { let pending = async pause(value: 1) "
      "let value = await pending }\n";
  CHECK(lower(ONE_YIELD_SOURCE));
  program = &fixture.hir_program;
  yield_count = 0u;
  for (size_t index = 0u; index < program->instruction_count; index += 1u)
    if (program->instructions[index].kind ==
        W_SEED_HIR0_INSTRUCTION_EXECUTION_YIELD)
      yield_count += 1u;
  CHECK(yield_count == 1u && program->call_count == 1u &&
        program->calls[0].execution_kind ==
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_STATIC_YIELDS_ELIDED &&
        w_seed_hir0_verify(program, &fixture.hir_result));

  static const char THREE_YIELDS_SOURCE[] =
      "async fn pause(value: i64): i64 { await execution#yield() "
      "await execution#yield() await execution#yield() return value }\n"
      "entry { let pending = async pause(value: 1) "
      "let value = await pending }\n";
  CHECK(lower(THREE_YIELDS_SOURCE));
  program = &fixture.hir_program;
  yield_count = 0u;
  for (size_t index = 0u; index < program->instruction_count; index += 1u)
    if (program->instructions[index].kind ==
        W_SEED_HIR0_INSTRUCTION_EXECUTION_YIELD)
      yield_count += 1u;
  CHECK(yield_count == 3u && program->call_count == 1u &&
        program->calls[0].execution_kind ==
            W_SEED_HIR0_CALL_STRUCTURED_ASYNC_STATIC_YIELDS_ELIDED &&
        w_seed_hir0_verify(program, &fixture.hir_result));

  static const char YIELD_CALL_SOURCE[] =
      "fn helper(value: i64): i64 { return value + 1 }\n"
      "async fn pause(value: i64): i64 { await execution#yield() "
      "return helper(value: value) }\n"
      "entry { let pending = async pause(value: 1) "
      "let value = await pending }\n";
  CHECK(lower(YIELD_CALL_SOURCE));
  program = &fixture.hir_program;
  CHECK(program->function_count == 3u && program->call_count == 2u &&
        program->functions[0].suspension == W_SEED_HIR0_SUSPENSION_NEVER &&
        program->functions[1].suspension == W_SEED_HIR0_SUSPENSION_MAY &&
        program->functions[1].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT);
  size_t helper_call = W_SEED_HIR0_NONE;
  size_t yielding_call = W_SEED_HIR0_NONE;
  for (size_t call = 0u; call < program->call_count; call += 1u) {
    if (program->calls[call].execution_kind == W_SEED_HIR0_CALL_DIRECT)
      helper_call = call;
    else if (program->calls[call].execution_kind ==
             W_SEED_HIR0_CALL_STRUCTURED_ASYNC_STATIC_YIELDS_ELIDED)
      yielding_call = call;
  }
  CHECK(helper_call != W_SEED_HIR0_NONE &&
        yielding_call != W_SEED_HIR0_NONE &&
        program->identities[program->calls[helper_call].callee_identity]
                .target_index == 0u &&
        program->identities[program->calls[yielding_call].callee_identity]
                .target_index == 1u &&
        w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_call saved_helper_call = fixture.hir_calls[helper_call];
  fixture.hir_calls[helper_call].execution_kind =
      W_SEED_HIR0_CALL_STRUCTURED_ASYNC_ELIDED;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_calls[helper_call] = saved_helper_call;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_function saved_helper_function = fixture.hir_functions[0];
  fixture.hir_functions[0].is_async = true;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_functions[0] = saved_helper_function;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  static const char YIELD_HELPER_DAG_SOURCE[] =
      "fn base(value: i64): i64 { return value + 1 }\n"
      "fn helper(value: i64): i64 { return base(value: value) * 2 }\n"
      "async fn pause(value: i64): i64 { await execution#yield() "
      "return helper(value: value) }\n"
      "entry { let pending = async pause(value: 1) "
      "let value = await pending }\n";
  CHECK(lower(YIELD_HELPER_DAG_SOURCE));
  program = &fixture.hir_program;
  CHECK(program->function_count == 4u && program->call_count == 3u &&
        w_seed_hir0_verify(program, &fixture.hir_result));
  size_t nested_helper_call = W_SEED_HIR0_NONE;
  for (size_t call = 0u; call < program->call_count; call += 1u) {
    const w_seed_hir0_call *candidate = &program->calls[call];
    if (candidate->execution_kind != W_SEED_HIR0_CALL_DIRECT ||
        candidate->owner_block >= program->block_count)
      continue;
    if (program->blocks[candidate->owner_block].owner_function == 1u) {
      nested_helper_call = call;
      break;
    }
  }
  CHECK(nested_helper_call != W_SEED_HIR0_NONE);
  const w_seed_hir0_call saved_nested_helper_call =
      fixture.hir_calls[nested_helper_call];
  fixture.hir_calls[nested_helper_call].callee_identity =
      (uint32_t)program->module_count + 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_calls[nested_helper_call] = saved_nested_helper_call;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  static const char YIELD_RECURSIVE_HELPER_SOURCE[] =
      "fn helper(value: i64): i64 { return helper(value: value) }\n"
      "async fn pause(value: i64): i64 { await execution#yield() "
      "return helper(value: value) }\n"
      "entry { let pending = async pause(value: 1) "
      "let value = await pending }\n";
  CHECK(fixture_frontend(YIELD_RECURSIVE_HELPER_SOURCE));
  setup_hir_output();
  input = (w_seed_hir0_input){
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  CHECK(w_seed_hir0_measure(&input, &counts, &result) ==
        W_SEED_HIR0_UNSUPPORTED);

  static const char YIELD_MUTUAL_HELPER_SOURCE[] =
      "fn left(value: i64): i64 { return right(value: value) }\n"
      "fn right(value: i64): i64 { return left(value: value) }\n"
      "async fn pause(value: i64): i64 { await execution#yield() "
      "return left(value: value) }\n"
      "entry { let pending = async pause(value: 1) "
      "let value = await pending }\n";
  CHECK(fixture_frontend(YIELD_MUTUAL_HELPER_SOURCE));
  setup_hir_output();
  input = (w_seed_hir0_input){
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  CHECK(w_seed_hir0_measure(&input, &counts, &result) ==
        W_SEED_HIR0_UNSUPPORTED);

  static const char YIELD_EFFECT_HELPER_SOURCE[] =
      "fn helper(value: i64): i64 { "
      "print(message: \"effect\", suffix: \"\") return value }\n"
      "async fn pause(value: i64): i64 { await execution#yield() "
      "return helper(value: value) }\n"
      "entry { let pending = async pause(value: 1) "
      "let value = await pending }\n";
  CHECK(fixture_frontend(YIELD_EFFECT_HELPER_SOURCE));
  setup_hir_output();
  input = (w_seed_hir0_input){
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  CHECK(w_seed_hir0_measure(&input, &counts, &result) ==
        W_SEED_HIR0_UNSUPPORTED);

  static const char STRING_SOURCE[] =
      "fn echo(value: String): String { return value }\n"
      "entry { let pending = async echo(value: \"ready\") "
      "let text = await pending }\n";
  CHECK(fixture_frontend(STRING_SOURCE));
  setup_hir_output();
  input = (w_seed_hir0_input){
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  CHECK(w_seed_hir0_measure(&input, &counts, &result) ==
        W_SEED_HIR0_UNSUPPORTED);

  static const char THROWING_SOURCE[] =
      "fn danger(value: i64): i64 throws Error { return value }\n"
      "entry { let pending = async danger(value: 1) "
      "let value = await pending }\n";
  CHECK(fixture_frontend(THROWING_SOURCE));
  setup_hir_output();
  input = (w_seed_hir0_input){
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  CHECK(w_seed_hir0_measure(&input, &counts, &result) != W_SEED_HIR0_OK);
  return true;
}

static bool test_parallel_domain_placement_hir(void) {
  static const char SOURCE[] =
      "fn increment(value: i64): i64 { return value + 1 }\n"
      "fn prepare(value: i64): i64 { return increment(value: value) }\n"
      "fn combine(left: i64, right: i64): i64 { return left + right + 1 }\n"
      "fn unused(value: i64): i64 { return value + 99 }\n"
      "entry { let left = spawn<.domain> prepare(value: 20) "
      "let right = spawn<.domain> combine(right: 2, left: 20) "
      "let first = await left let second = await right }\n";

  /* Frontend32 added accelerated-domain and kernel-binding identities. HIR38
   * does not represent them yet, so its boundary must reject every forged or
   * unrepresented field instead of treating appended schema bytes as zero-cost
   * metadata. */
  CHECK(fixture_parallel_domain_frontend(
      SOURCE, W_SEED_FRONTEND_DOMAIN_CAPABILITY_PARALLEL));
  setup_hir_output();
  const w_seed_hir0_input frontend32_input = {
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  w_seed_hir0_counts frontend32_counts;
  w_seed_hir0_result frontend32_result;
  fixture.domains[0].kind = W_SEED_FRONTEND_DOMAIN_ACCELERATED;
  fixture.domains[0].capabilities = W_SEED_FRONTEND_DOMAIN_CAPABILITY_DEVICE;
  fixture.domains[0].maximum = 1u;
  CHECK(w_seed_hir0_measure(&frontend32_input, &frontend32_counts,
                            &frontend32_result) != W_SEED_HIR0_OK);
  fixture.domains[0].kind = W_SEED_FRONTEND_DOMAIN_HOST;
  fixture.domains[0].capabilities = W_SEED_FRONTEND_DOMAIN_CAPABILITY_PARALLEL;
  fixture.domains[0].maximum = 0u;
  size_t frontend_launch = W_SEED_HIR0_NONE;
  size_t frontend_call = W_SEED_HIR0_NONE;
  for (size_t index = 0u; index < fixture.result.written.expressions;
       index += 1u) {
    if (fixture.expressions[index].kind ==
            W_SEED_FRONTEND_EXPR_SPAWN_PARALLEL_DOMAIN_LAUNCH &&
        frontend_launch == W_SEED_HIR0_NONE)
      frontend_launch = index;
    if (fixture.expressions[index].kind == W_SEED_FRONTEND_EXPR_CALL &&
        frontend_call == W_SEED_HIR0_NONE)
      frontend_call = index;
  }
  CHECK(frontend_launch != W_SEED_HIR0_NONE &&
        frontend_call != W_SEED_HIR0_NONE);
  fixture.expressions[frontend_launch].domain_kind =
      W_SEED_FRONTEND_DOMAIN_ACCELERATED;
  fixture.expressions[frontend_launch].domain_maximum = 1u;
  CHECK(w_seed_hir0_measure(&frontend32_input, &frontend32_counts,
                            &frontend32_result) != W_SEED_HIR0_OK);
  fixture.expressions[frontend_launch].domain_kind =
      W_SEED_FRONTEND_DOMAIN_HOST;
  fixture.expressions[frontend_launch].domain_maximum = 0u;
  fixture.expressions[frontend_call].resolved_kernel_module_index = 0u;
  fixture.expressions[frontend_call].resolved_kernel_binding_index = 0u;
  CHECK(w_seed_hir0_measure(&frontend32_input, &frontend32_counts,
                            &frontend32_result) != W_SEED_HIR0_OK);

  CHECK(lower_parallel_domain(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  size_t parallel_calls = 0u;
  size_t first_parallel = W_SEED_HIR0_NONE;
  for (size_t index = 0u; index < program->call_count; index += 1u) {
    const w_seed_hir0_call *call = &program->calls[index];
    if (call->execution_kind !=
        W_SEED_HIR0_CALL_STRUCTURED_ASYNC_PARALLEL_DOMAIN_DISPATCH)
      continue;
    if (first_parallel == W_SEED_HIR0_NONE) first_parallel = index;
    CHECK(call->placement == W_SEED_HIR0_CALL_PLACEMENT_PARALLEL_DOMAIN &&
          call->domain_mode == W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT &&
          call->domain_capabilities ==
              W_SEED_FRONTEND_DOMAIN_CAPABILITY_PARALLEL &&
          hir_text_is(program, call->domain_identity,
                      W_SEED_FRONTEND_DOMAIN_IDENTITY));
    parallel_calls += 1u;
  }
  CHECK(parallel_calls == 2u && first_parallel != W_SEED_HIR0_NONE &&
        w_seed_hir0_verify(program, &fixture.hir_result));

  w_seed_parallel_selection0 selection;
  (void)memset(&selection, 0x6b, sizeof(selection));
  CHECK(w_seed_parallel_selection0_select(program, &fixture.hir_result,
                                          &selection) ==
            W_SEED_PARALLEL_SELECTION0_OK &&
        selection.task_count == 2u &&
        selection.placement ==
            W_SEED_HIR0_CALL_PLACEMENT_PARALLEL_DOMAIN &&
        selection.domain_mode == W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT &&
        selection.domain_capabilities ==
            W_SEED_FRONTEND_DOMAIN_CAPABILITY_PARALLEL &&
        strcmp(selection.domain_identity,
               W_SEED_FRONTEND_DOMAIN_IDENTITY) == 0 &&
        w_seed_parallel_selection0_verify(program, &fixture.hir_result,
                                          &selection));

  w_seed_parallel_selection1_counts ordered_selection_counts;
  w_seed_parallel_selection1_result ordered_selection_result;
  CHECK(w_seed_parallel_selection1_measure(
            program, &fixture.hir_result, &ordered_selection_counts,
            &ordered_selection_result) == W_SEED_PARALLEL_SELECTION1_OK &&
        ordered_selection_counts.tasks == 2u);
  w_seed_parallel_selection1_task ordered_selection_tasks[2];
  const w_seed_parallel_selection1_output ordered_selection_output = {
      ordered_selection_tasks,
      sizeof(ordered_selection_tasks) / sizeof(ordered_selection_tasks[0])};
  CHECK(w_seed_parallel_selection1_run(
            program, &fixture.hir_result, &ordered_selection_output,
            &ordered_selection_result) == W_SEED_PARALLEL_SELECTION1_OK);
  w_seed_parallel_selection1_program ordered_selection;
  CHECK(w_seed_parallel_selection1_program_from_output(
            &ordered_selection_output, &ordered_selection_result,
            &ordered_selection) &&
        w_seed_parallel_selection1_verify(
            program, &fixture.hir_result, &ordered_selection,
            &ordered_selection_result));

  w_seed_parallel_invocation1_counts ordered_invocation_counts;
  w_seed_parallel_invocation1_result ordered_invocation_result;
  CHECK(w_seed_parallel_invocation1_measure(
            program, &fixture.hir_result, &ordered_selection,
            &ordered_selection_result, &ordered_invocation_counts,
            &ordered_invocation_result) == W_SEED_PARALLEL_INVOCATION1_OK &&
        ordered_invocation_counts.tasks == 2u &&
        ordered_invocation_counts.arguments == 3u);
  w_seed_parallel_invocation1_task ordered_invocation_tasks[2];
  w_seed_parallel_invocation1_argument ordered_invocation_arguments[3];
  const w_seed_parallel_invocation1_output ordered_invocation_output = {
      ordered_invocation_tasks,
      sizeof(ordered_invocation_tasks) / sizeof(ordered_invocation_tasks[0]),
      ordered_invocation_arguments,
      sizeof(ordered_invocation_arguments) /
          sizeof(ordered_invocation_arguments[0])};
  CHECK(w_seed_parallel_invocation1_run(
            program, &fixture.hir_result, &ordered_selection,
            &ordered_selection_result, &ordered_invocation_output,
            &ordered_invocation_result) == W_SEED_PARALLEL_INVOCATION1_OK);
  w_seed_parallel_invocation1_program ordered_invocation;
  CHECK(w_seed_parallel_invocation1_program_from_output(
            &ordered_invocation_output, &ordered_invocation_result,
            &ordered_invocation) &&
        w_seed_parallel_invocation1_verify(
            program, &fixture.hir_result, &ordered_selection,
            &ordered_selection_result, &ordered_invocation,
            &ordered_invocation_result) &&
        ordered_invocation.tasks[1].first_argument == 1u &&
        ordered_invocation.tasks[1].argument_count == 2u &&
        ordered_invocation.arguments[1].parameter_ordinal == 0u &&
        ordered_invocation.arguments[2].parameter_ordinal == 1u &&
        program->values[ordered_invocation.arguments[1].value_index]
                .integer_value == 20 &&
        program->values[ordered_invocation.arguments[2].value_index]
                .integer_value == 2);
  int64_t ordered_value = 0;
  CHECK(w_seed_parallel_invocation1_evaluate_task(
            program, &fixture.hir_result, &ordered_selection,
            &ordered_selection_result, &ordered_invocation,
            &ordered_invocation_result, 1u, &ordered_value) ==
            W_SEED_PARALLEL_INVOCATION1_OK &&
        ordered_value == 23);
  CHECK(test_parallel_mlir_entries1(
      program, &fixture.hir_result, &ordered_selection,
      &ordered_selection_result, &ordered_invocation,
      &ordered_invocation_result, &selection, 2u, 3u,
      "func.func @w_seed_parallel_task_1"));
  w_seed_parallel_elision0_certificate elision_sentinel;
  (void)memset(&elision_sentinel, 0x9d, sizeof(elision_sentinel));
  const w_seed_parallel_elision0_certificate elision_before =
      elision_sentinel;
  CHECK(w_seed_parallel_elision0_certify(
            program, &fixture.hir_result, &selection, &elision_sentinel) ==
            W_SEED_PARALLEL_ELISION0_UNSUPPORTED &&
        memcmp(&elision_sentinel, &elision_before,
               sizeof(elision_sentinel)) == 0);
  CHECK(test_parallel_mlir_entries(program, &fixture.hir_result, &selection));
  for (size_t task = selection.task_count;
       task < W_SEED_PARALLEL_SELECTION0_MAX_TASKS; task += 1u)
    CHECK(selection.task_call_indices[task] == 0u &&
          selection.task_function_indices[task] == 0u &&
          selection.launch_binding_indices[task] == 0u &&
          selection.join_binding_indices[task] == 0u);
#if defined(_WIN32) && defined(_WIN64)
  CHECK(test_parallel_provider_windows(program, &fixture.hir_result,
                                       &selection));
#endif

  w_seed_parallel_selection0 forged_selection = selection;
  forged_selection.task_call_indices[0] ^= 1u;
  CHECK(!w_seed_parallel_selection0_verify(program, &fixture.hir_result,
                                           &forged_selection));
  forged_selection = selection;
  forged_selection.domain_capabilities = 0u;
  CHECK(!w_seed_parallel_selection0_verify(program, &fixture.hir_result,
                                           &forged_selection));

  w_seed_parallel_selection0 selection_sentinel;
  (void)memset(&selection_sentinel, 0x7c, sizeof(selection_sentinel));
  const w_seed_parallel_selection0 selection_before = selection_sentinel;
  const w_seed_hir0_call first_call_before = fixture.hir_calls[0];
  CHECK(w_seed_parallel_selection0_select(
            program, &fixture.hir_result,
            (w_seed_parallel_selection0 *)(void *)fixture.hir_calls) ==
            W_SEED_PARALLEL_SELECTION0_INVALID &&
        memcmp(&fixture.hir_calls[0], &first_call_before,
               sizeof(first_call_before)) == 0);
  const w_seed_hir0_program program_before = fixture.hir_program;
  CHECK(w_seed_parallel_selection0_select(
            program, &fixture.hir_result,
            (w_seed_parallel_selection0 *)(void *)&fixture.hir_program) ==
            W_SEED_PARALLEL_SELECTION0_INVALID &&
        memcmp(&fixture.hir_program, &program_before,
               sizeof(program_before)) == 0);
  const w_seed_hir0_result result_before = fixture.hir_result;
  CHECK(w_seed_parallel_selection0_select(
            program, &fixture.hir_result,
            (w_seed_parallel_selection0 *)(void *)&fixture.hir_result) ==
            W_SEED_PARALLEL_SELECTION0_INVALID &&
        memcmp(&fixture.hir_result, &result_before,
               sizeof(result_before)) == 0);
  CHECK(w_seed_parallel_selection0_select(NULL, &fixture.hir_result,
                                          &selection_sentinel) ==
            W_SEED_PARALLEL_SELECTION0_INVALID &&
        memcmp(&selection_sentinel, &selection_before,
               sizeof(selection_sentinel)) == 0);

  /* The verified program owns the identity bytes; frontend/profile storage
   * can disappear before a later pass re-verifies the placement proof. */
  (void)memset(fixture.domains, 0xa5, sizeof(fixture.domains));
  (void)memset(&fixture.input, 0xa5, sizeof(fixture.input));
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result) &&
        w_seed_parallel_selection0_verify(program, &fixture.hir_result,
                                          &selection));

  const w_seed_hir0_call saved = fixture.hir_calls[first_parallel];
  fixture.hir_calls[first_parallel].placement =
      W_SEED_HIR0_CALL_PLACEMENT_NONE;
  reseal_hir_fixture();
  selection_sentinel = selection_before;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result) &&
        w_seed_parallel_selection0_select(program, &fixture.hir_result,
                                          &selection_sentinel) ==
            W_SEED_PARALLEL_SELECTION0_INVALID &&
        memcmp(&selection_sentinel, &selection_before,
               sizeof(selection_sentinel)) == 0);
  fixture.hir_calls[first_parallel] = saved;

  fixture.hir_calls[first_parallel].domain_mode =
      W_SEED_FRONTEND_DOMAIN_MODE_SERIAL;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_calls[first_parallel] = saved;

  fixture.hir_calls[first_parallel].domain_identity.count = 0u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_calls[first_parallel] = saved;

  fixture.hir_calls[first_parallel].execution_kind =
      W_SEED_HIR0_CALL_STRUCTURED_ASYNC_MAIN_DISPATCH;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_calls[first_parallel] = saved;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  static const char ONE_TASK[] =
      "fn prepare(value: i64): i64 { return value + 1 }\n"
      "entry { let only = spawn<.domain> prepare(value: 20) "
      "let value = await only }\n";
  CHECK(lower_parallel_domain(ONE_TASK));
  CHECK(w_seed_parallel_selection0_select(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
            W_SEED_PARALLEL_SELECTION0_OK &&
        selection.task_count == 1u &&
        w_seed_parallel_selection0_verify(
            &fixture.hir_program, &fixture.hir_result, &selection));
#if defined(_WIN32) && defined(_WIN64)
  CHECK(test_parallel_provider_cardinality(
      &fixture.hir_program, &fixture.hir_result, &selection));
#endif

  static const char FOUR_TASKS[] =
      "fn prepare(value: i64): i64 { return value + 1 }\n"
      "entry { let a = spawn<.domain> prepare(value: 20) "
      "let b = spawn<.domain> prepare(value: 22) "
      "let c = spawn<.domain> prepare(value: 24) "
      "let d = spawn<.domain> prepare(value: 26) "
      "let av = await a let bv = await b let cv = await c let dv = await d }\n";
  CHECK(lower_parallel_domain(FOUR_TASKS));
  CHECK(w_seed_parallel_selection0_select(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
            W_SEED_PARALLEL_SELECTION0_OK &&
        selection.task_count == W_SEED_PARALLEL_SELECTION0_MAX_TASKS &&
        w_seed_parallel_selection0_verify(
            &fixture.hir_program, &fixture.hir_result, &selection));
#if defined(_WIN32) && defined(_WIN64)
  CHECK(test_parallel_provider_cardinality(
      &fixture.hir_program, &fixture.hir_result, &selection));
#endif

  static const char FIVE_TASKS[] =
      "fn prepare(value: i64): i64 { return value + 1 }\n"
      "entry { let a = spawn<.domain> prepare(value: 20) "
      "let b = spawn<.domain> prepare(value: 22) "
      "let c = spawn<.domain> prepare(value: 24) "
      "let d = spawn<.domain> prepare(value: 26) "
      "let e = spawn<.domain> prepare(value: 28) "
      "let av = await a let bv = await b let cv = await c let dv = await d "
      "let ev = await e }\n";
  CHECK(lower_parallel_domain(FIVE_TASKS));
  w_seed_parallel_selection1_counts measured_selection;
  w_seed_parallel_selection1_result selection1_result;
  CHECK(w_seed_parallel_selection1_measure(
            &fixture.hir_program, &fixture.hir_result, &measured_selection,
            &selection1_result) == W_SEED_PARALLEL_SELECTION1_OK &&
        measured_selection.tasks == 5u &&
        selection1_result.required.tasks == 5u &&
        selection1_result.written.tasks == 0u);
  w_seed_parallel_selection1_task measured_tasks[5];
  (void)memset(measured_tasks, 0xa5, sizeof(measured_tasks));
  const w_seed_parallel_selection1_output measured_output = {
      measured_tasks, sizeof(measured_tasks) / sizeof(measured_tasks[0])};
  CHECK(w_seed_parallel_selection1_run(
            &fixture.hir_program, &fixture.hir_result, &measured_output,
            &selection1_result) == W_SEED_PARALLEL_SELECTION1_OK &&
        selection1_result.written.tasks == 5u);
  w_seed_parallel_selection1_program measured_program;
  CHECK(w_seed_parallel_selection1_program_from_output(
            &measured_output, &selection1_result, &measured_program) &&
        measured_program.task_count == 5u &&
        w_seed_parallel_selection1_verify(
            &fixture.hir_program, &fixture.hir_result, &measured_program,
            &selection1_result));
  for (size_t ordinal = 0u; ordinal < measured_program.task_count;
       ordinal += 1u)
    CHECK(measured_program.tasks[ordinal].call_index <
              fixture.hir_program.call_count &&
          measured_program.tasks[ordinal].function_index <
              fixture.hir_program.function_count &&
          (ordinal == 0u ||
           measured_program.tasks[ordinal - 1u].call_instruction <
               measured_program.tasks[ordinal].call_instruction));

  w_seed_parallel_invocation1_counts measured_invocation;
  w_seed_parallel_invocation1_result invocation1_result;
  CHECK(w_seed_parallel_invocation1_measure(
            &fixture.hir_program, &fixture.hir_result, &measured_program,
            &selection1_result, &measured_invocation,
            &invocation1_result) == W_SEED_PARALLEL_INVOCATION1_OK &&
        measured_invocation.tasks == 5u &&
        measured_invocation.arguments == 5u &&
        invocation1_result.required.tasks == 5u &&
        invocation1_result.required.arguments == 5u &&
        invocation1_result.written.tasks == 0u &&
        invocation1_result.written.arguments == 0u);
  w_seed_parallel_invocation1_task invocation_tasks[5];
  w_seed_parallel_invocation1_argument invocation_arguments[5];
  (void)memset(invocation_tasks, 0x4a, sizeof(invocation_tasks));
  (void)memset(invocation_arguments, 0x4b, sizeof(invocation_arguments));
  const w_seed_parallel_invocation1_output invocation_output = {
      invocation_tasks,
      sizeof(invocation_tasks) / sizeof(invocation_tasks[0]),
      invocation_arguments,
      sizeof(invocation_arguments) / sizeof(invocation_arguments[0])};
  CHECK(w_seed_parallel_invocation1_run(
            &fixture.hir_program, &fixture.hir_result, &measured_program,
            &selection1_result, &invocation_output, &invocation1_result) ==
            W_SEED_PARALLEL_INVOCATION1_OK &&
        invocation1_result.written.tasks == 5u &&
        invocation1_result.written.arguments == 5u);
  w_seed_parallel_invocation1_program invocation_program;
  CHECK(w_seed_parallel_invocation1_program_from_output(
            &invocation_output, &invocation1_result, &invocation_program) &&
        invocation_program.task_count == 5u &&
        invocation_program.argument_count == 5u &&
        w_seed_parallel_invocation1_verify(
            &fixture.hir_program, &fixture.hir_result, &measured_program,
            &selection1_result, &invocation_program, &invocation1_result));
  CHECK(test_parallel_mlir_entries1(
      &fixture.hir_program, &fixture.hir_result, &measured_program,
      &selection1_result, &invocation_program, &invocation1_result, NULL, 5u,
      5u, "func.func @w_seed_parallel_task_4"));
  for (uint32_t task = 0u; task < 5u; task += 1u) {
    CHECK(invocation_tasks[task].first_argument == task &&
          invocation_tasks[task].argument_count == 1u &&
          invocation_arguments[task].owner_task == task &&
          invocation_arguments[task].parameter_ordinal == 0u &&
          invocation_arguments[task].parameter_index <
              fixture.hir_program.parameter_count &&
          invocation_arguments[task].value_index <
              fixture.hir_program.value_count);
    int64_t evaluated = 0;
    CHECK(w_seed_parallel_invocation1_evaluate_task(
              &fixture.hir_program, &fixture.hir_result, &measured_program,
              &selection1_result, &invocation_program, &invocation1_result,
              task, &evaluated) == W_SEED_PARALLEL_INVOCATION1_OK &&
          evaluated == (int64_t)(21u + task * 2u));
  }

  w_seed_parallel_invocation1_argument short_invocation_arguments[4];
  (void)memset(short_invocation_arguments, 0x5c,
               sizeof(short_invocation_arguments));
  const w_seed_parallel_invocation1_argument
      short_invocation_arguments_before[4] = {
          short_invocation_arguments[0], short_invocation_arguments[1],
          short_invocation_arguments[2], short_invocation_arguments[3]};
  w_seed_parallel_invocation1_task short_invocation_tasks[5];
  (void)memset(short_invocation_tasks, 0x5d,
               sizeof(short_invocation_tasks));
  const w_seed_parallel_invocation1_task short_invocation_tasks_before[5] = {
      short_invocation_tasks[0], short_invocation_tasks[1],
      short_invocation_tasks[2], short_invocation_tasks[3],
      short_invocation_tasks[4]};
  w_seed_parallel_invocation1_result short_invocation_result;
  (void)memset(&short_invocation_result, 0x5e,
               sizeof(short_invocation_result));
  const w_seed_parallel_invocation1_result short_invocation_result_before =
      short_invocation_result;
  const w_seed_parallel_invocation1_output short_invocation_output = {
      short_invocation_tasks,
      sizeof(short_invocation_tasks) / sizeof(short_invocation_tasks[0]),
      short_invocation_arguments,
      sizeof(short_invocation_arguments) /
          sizeof(short_invocation_arguments[0])};
  CHECK(w_seed_parallel_invocation1_run(
            &fixture.hir_program, &fixture.hir_result, &measured_program,
            &selection1_result, &short_invocation_output,
            &short_invocation_result) == W_SEED_PARALLEL_INVOCATION1_CAPACITY &&
        memcmp(short_invocation_tasks, short_invocation_tasks_before,
               sizeof(short_invocation_tasks)) == 0 &&
        memcmp(short_invocation_arguments, short_invocation_arguments_before,
               sizeof(short_invocation_arguments)) == 0 &&
        memcmp(&short_invocation_result, &short_invocation_result_before,
               sizeof(short_invocation_result)) == 0);

  w_seed_parallel_invocation1_task short_task_storage[4];
  w_seed_parallel_invocation1_argument complete_argument_storage[5];
  (void)memset(short_task_storage, 0x5f, sizeof(short_task_storage));
  (void)memset(complete_argument_storage, 0x60,
               sizeof(complete_argument_storage));
  const w_seed_parallel_invocation1_task short_task_storage_before[4] = {
      short_task_storage[0], short_task_storage[1], short_task_storage[2],
      short_task_storage[3]};
  const w_seed_parallel_invocation1_argument
      complete_argument_storage_before[5] = {
          complete_argument_storage[0], complete_argument_storage[1],
          complete_argument_storage[2], complete_argument_storage[3],
          complete_argument_storage[4]};
  short_invocation_result = short_invocation_result_before;
  const w_seed_parallel_invocation1_output short_task_output = {
      short_task_storage, 4u, complete_argument_storage, 5u};
  CHECK(w_seed_parallel_invocation1_run(
            &fixture.hir_program, &fixture.hir_result, &measured_program,
            &selection1_result, &short_task_output,
            &short_invocation_result) == W_SEED_PARALLEL_INVOCATION1_CAPACITY &&
        memcmp(short_task_storage, short_task_storage_before,
               sizeof(short_task_storage)) == 0 &&
        memcmp(complete_argument_storage, complete_argument_storage_before,
               sizeof(complete_argument_storage)) == 0 &&
        memcmp(&short_invocation_result, &short_invocation_result_before,
               sizeof(short_invocation_result)) == 0);

  union {
    w_seed_parallel_invocation1_counts counts;
    w_seed_parallel_invocation1_result result;
  } invocation_measure_alias;
  (void)memset(&invocation_measure_alias, 0x63,
               sizeof(invocation_measure_alias));
  unsigned char invocation_measure_alias_before
      [sizeof(invocation_measure_alias)];
  (void)memcpy(invocation_measure_alias_before, &invocation_measure_alias,
               sizeof(invocation_measure_alias));
  CHECK(w_seed_parallel_invocation1_measure(
            &fixture.hir_program, &fixture.hir_result, &measured_program,
            &selection1_result, &invocation_measure_alias.counts,
            &invocation_measure_alias.result) ==
            W_SEED_PARALLEL_INVOCATION1_ALIAS &&
        memcmp(&invocation_measure_alias, invocation_measure_alias_before,
               sizeof(invocation_measure_alias)) == 0);

  union {
    w_seed_parallel_invocation1_task tasks[5];
    w_seed_parallel_invocation1_result result;
  } invocation_run_alias;
  w_seed_parallel_invocation1_argument invocation_run_alias_arguments[5];
  (void)memset(&invocation_run_alias, 0x64, sizeof(invocation_run_alias));
  (void)memset(invocation_run_alias_arguments, 0x65,
               sizeof(invocation_run_alias_arguments));
  unsigned char invocation_run_alias_before[sizeof(invocation_run_alias)];
  const w_seed_parallel_invocation1_argument
      invocation_run_alias_arguments_before[5] = {
          invocation_run_alias_arguments[0], invocation_run_alias_arguments[1],
          invocation_run_alias_arguments[2], invocation_run_alias_arguments[3],
          invocation_run_alias_arguments[4]};
  (void)memcpy(invocation_run_alias_before, &invocation_run_alias,
               sizeof(invocation_run_alias));
  const w_seed_parallel_invocation1_output invocation_alias_output = {
      invocation_run_alias.tasks,
      sizeof(invocation_run_alias.tasks) /
          sizeof(invocation_run_alias.tasks[0]),
      invocation_run_alias_arguments,
      sizeof(invocation_run_alias_arguments) /
          sizeof(invocation_run_alias_arguments[0])};
  CHECK(w_seed_parallel_invocation1_run(
            &fixture.hir_program, &fixture.hir_result, &measured_program,
            &selection1_result, &invocation_alias_output,
            &invocation_run_alias.result) ==
            W_SEED_PARALLEL_INVOCATION1_ALIAS &&
        memcmp(&invocation_run_alias, invocation_run_alias_before,
               sizeof(invocation_run_alias)) == 0 &&
        memcmp(invocation_run_alias_arguments,
               invocation_run_alias_arguments_before,
               sizeof(invocation_run_alias_arguments)) == 0);

  w_seed_parallel_invocation1_argument producer_alias_arguments[5];
  (void)memset(producer_alias_arguments, 0x66,
               sizeof(producer_alias_arguments));
  const w_seed_parallel_invocation1_argument
      producer_alias_arguments_before[5] = {
          producer_alias_arguments[0], producer_alias_arguments[1],
          producer_alias_arguments[2], producer_alias_arguments[3],
          producer_alias_arguments[4]};
  const w_seed_parallel_selection1_task measured_tasks_before[5] = {
      measured_tasks[0], measured_tasks[1], measured_tasks[2],
      measured_tasks[3], measured_tasks[4]};
  w_seed_parallel_invocation1_result producer_alias_result;
  (void)memset(&producer_alias_result, 0x67,
               sizeof(producer_alias_result));
  const w_seed_parallel_invocation1_result producer_alias_result_before =
      producer_alias_result;
  const w_seed_parallel_invocation1_output producer_alias_output = {
      (w_seed_parallel_invocation1_task *)(void *)measured_tasks, 5u,
      producer_alias_arguments, 5u};
  CHECK(w_seed_parallel_invocation1_run(
            &fixture.hir_program, &fixture.hir_result, &measured_program,
            &selection1_result, &producer_alias_output,
            &producer_alias_result) == W_SEED_PARALLEL_INVOCATION1_ALIAS &&
        memcmp(measured_tasks, measured_tasks_before, sizeof(measured_tasks)) ==
            0 &&
        memcmp(producer_alias_arguments, producer_alias_arguments_before,
               sizeof(producer_alias_arguments)) == 0 &&
        memcmp(&producer_alias_result, &producer_alias_result_before,
               sizeof(producer_alias_result)) == 0);

  union {
    w_seed_parallel_invocation1_result result;
    w_seed_parallel_invocation1_program program;
  } invocation_bridge_alias;
  (void)memset(&invocation_bridge_alias, 0, sizeof(invocation_bridge_alias));
  invocation_bridge_alias.result = invocation1_result;
  unsigned char invocation_bridge_alias_before
      [sizeof(invocation_bridge_alias)];
  (void)memcpy(invocation_bridge_alias_before, &invocation_bridge_alias,
               sizeof(invocation_bridge_alias));
  CHECK(!w_seed_parallel_invocation1_program_from_output(
            &invocation_output, &invocation_bridge_alias.result,
            &invocation_bridge_alias.program) &&
        memcmp(&invocation_bridge_alias, invocation_bridge_alias_before,
               sizeof(invocation_bridge_alias)) == 0);

  const w_seed_parallel_invocation1_result verified_invocation_result =
      invocation1_result;
  invocation1_result.semantic_digest[0] ^= 1u;
  CHECK(!w_seed_parallel_invocation1_verify(
      &fixture.hir_program, &fixture.hir_result, &measured_program,
      &selection1_result, &invocation_program, &invocation1_result));
  invocation1_result = verified_invocation_result;

  union {
    w_seed_parallel_invocation1_result result;
    int64_t value;
  } invocation_value_alias;
  (void)memset(&invocation_value_alias, 0, sizeof(invocation_value_alias));
  invocation_value_alias.result = invocation1_result;
  unsigned char invocation_value_alias_before[sizeof(invocation_value_alias)];
  (void)memcpy(invocation_value_alias_before, &invocation_value_alias,
               sizeof(invocation_value_alias));
  CHECK(w_seed_parallel_invocation1_evaluate_task(
            &fixture.hir_program, &fixture.hir_result, &measured_program,
            &selection1_result, &invocation_program,
            &invocation_value_alias.result, 0u,
            &invocation_value_alias.value) ==
            W_SEED_PARALLEL_INVOCATION1_INVALID &&
        memcmp(&invocation_value_alias, invocation_value_alias_before,
               sizeof(invocation_value_alias)) == 0);

  const w_seed_parallel_invocation1_argument first_invocation_argument =
      invocation_arguments[0];
  invocation_arguments[0].value_index = W_SEED_HIR0_NONE;
  CHECK(!w_seed_parallel_invocation1_verify(
      &fixture.hir_program, &fixture.hir_result, &measured_program,
      &selection1_result, &invocation_program, &invocation1_result));
  invocation_arguments[0] = first_invocation_argument;
  CHECK(w_seed_parallel_invocation1_verify(
      &fixture.hir_program, &fixture.hir_result, &measured_program,
      &selection1_result, &invocation_program, &invocation1_result));
#if defined(_WIN32) && defined(_WIN64)
  CHECK(test_parallel_provider1_windows(
      &fixture.hir_program, &fixture.hir_result, &measured_program,
      &selection1_result, &invocation_program, &invocation1_result));
#endif

  w_seed_parallel_invocation1_counts invalid_invocation_counts;
  (void)memset(&invalid_invocation_counts, 0x68,
               sizeof(invalid_invocation_counts));
  const w_seed_parallel_invocation1_counts invalid_invocation_counts_before =
      invalid_invocation_counts;
  w_seed_parallel_invocation1_result invalid_invocation_result;
  (void)memset(&invalid_invocation_result, 0x69,
               sizeof(invalid_invocation_result));
  const w_seed_parallel_invocation1_result invalid_invocation_result_before =
      invalid_invocation_result;
  selection1_result.semantic_digest[0] ^= 1u;
  CHECK(w_seed_parallel_invocation1_measure(
            &fixture.hir_program, &fixture.hir_result, &measured_program,
            &selection1_result, &invalid_invocation_counts,
            &invalid_invocation_result) == W_SEED_PARALLEL_INVOCATION1_INVALID &&
        memcmp(&invalid_invocation_counts, &invalid_invocation_counts_before,
               sizeof(invalid_invocation_counts)) == 0 &&
        memcmp(&invalid_invocation_result, &invalid_invocation_result_before,
               sizeof(invalid_invocation_result)) == 0);
  selection1_result.semantic_digest[0] ^= 1u;

  w_seed_parallel_selection1_task short_tasks[4];
  (void)memset(short_tasks, 0x6d, sizeof(short_tasks));
  const w_seed_parallel_selection1_task short_before[4] = {
      short_tasks[0], short_tasks[1], short_tasks[2], short_tasks[3]};
  w_seed_parallel_selection1_result short_result;
  (void)memset(&short_result, 0x7e, sizeof(short_result));
  const w_seed_parallel_selection1_result short_result_before = short_result;
  const w_seed_parallel_selection1_output short_output = {
      short_tasks, sizeof(short_tasks) / sizeof(short_tasks[0])};
  CHECK(w_seed_parallel_selection1_run(
            &fixture.hir_program, &fixture.hir_result, &short_output,
            &short_result) == W_SEED_PARALLEL_SELECTION1_CAPACITY &&
        memcmp(short_tasks, short_before, sizeof(short_tasks)) == 0 &&
        memcmp(&short_result, &short_result_before, sizeof(short_result)) == 0);

  union {
    w_seed_parallel_selection1_counts counts;
    w_seed_parallel_selection1_result result;
  } measure_alias;
  (void)memset(&measure_alias, 0x35, sizeof(measure_alias));
  unsigned char measure_alias_before[sizeof(measure_alias)];
  (void)memcpy(measure_alias_before, &measure_alias, sizeof(measure_alias));
  CHECK(w_seed_parallel_selection1_measure(
            &fixture.hir_program, &fixture.hir_result, &measure_alias.counts,
            &measure_alias.result) == W_SEED_PARALLEL_SELECTION1_ALIAS &&
        memcmp(&measure_alias, measure_alias_before, sizeof(measure_alias)) ==
            0);

  union {
    w_seed_parallel_selection1_task tasks[5];
    w_seed_parallel_selection1_result result;
  } run_alias;
  (void)memset(&run_alias, 0x46, sizeof(run_alias));
  unsigned char run_alias_before[sizeof(run_alias)];
  (void)memcpy(run_alias_before, &run_alias, sizeof(run_alias));
  const w_seed_parallel_selection1_output alias_output = {
      run_alias.tasks, sizeof(run_alias.tasks) / sizeof(run_alias.tasks[0])};
  CHECK(w_seed_parallel_selection1_run(
            &fixture.hir_program, &fixture.hir_result, &alias_output,
            &run_alias.result) == W_SEED_PARALLEL_SELECTION1_ALIAS &&
        memcmp(&run_alias, run_alias_before, sizeof(run_alias)) == 0);

  union {
    w_seed_parallel_selection1_result result;
    w_seed_parallel_selection1_program program;
  } bridge_alias;
  (void)memset(&bridge_alias, 0, sizeof(bridge_alias));
  bridge_alias.result = selection1_result;
  unsigned char bridge_alias_before[sizeof(bridge_alias)];
  (void)memcpy(bridge_alias_before, &bridge_alias, sizeof(bridge_alias));
  CHECK(!w_seed_parallel_selection1_program_from_output(
            &measured_output, &bridge_alias.result, &bridge_alias.program) &&
        memcmp(&bridge_alias, bridge_alias_before, sizeof(bridge_alias)) == 0);

  const w_seed_parallel_selection1_result verified_result = selection1_result;
  selection1_result.schema[0] ^= 1;
  CHECK(!w_seed_parallel_selection1_verify(
      &fixture.hir_program, &fixture.hir_result, &measured_program,
      &selection1_result));
  selection1_result = verified_result;
  selection1_result.domain_mode = W_SEED_FRONTEND_DOMAIN_MODE_SERIAL;
  CHECK(!w_seed_parallel_selection1_verify(
      &fixture.hir_program, &fixture.hir_result, &measured_program,
      &selection1_result));
  selection1_result = verified_result;
  selection1_result.semantic_digest[0] ^= 1u;
  CHECK(!w_seed_parallel_selection1_verify(
      &fixture.hir_program, &fixture.hir_result, &measured_program,
      &selection1_result));
  selection1_result = verified_result;

  const w_seed_parallel_selection1_task fifth_task = measured_tasks[4];
  measured_tasks[4].call_index = W_SEED_HIR0_NONE;
  CHECK(!w_seed_parallel_selection1_verify(
      &fixture.hir_program, &fixture.hir_result, &measured_program,
      &selection1_result));
  measured_tasks[4] = fifth_task;
  CHECK(w_seed_parallel_selection1_verify(
      &fixture.hir_program, &fixture.hir_result, &measured_program,
      &selection1_result));
  selection_sentinel = selection_before;
  CHECK(w_seed_parallel_selection0_select(
            &fixture.hir_program, &fixture.hir_result, &selection_sentinel) ==
            W_SEED_PARALLEL_SELECTION0_UNSUPPORTED &&
        memcmp(&selection_sentinel, &selection_before,
               sizeof(selection_sentinel)) == 0);

  static const char WIDE_TASK[] =
      "fn wide(a0: i64, a1: i64, a2: i64, a3: i64, a4: i64, "
      "a5: i64, a6: i64, a7: i64, a8: i64, a9: i64, a10: i64, "
      "a11: i64, a12: i64, a13: i64, a14: i64, a15: i64, "
      "a16: i64): i64 { return a0 + a16 }\n"
      "entry { let pending = spawn<.domain> wide(a0: 0, a1: 1, a2: 2, "
      "a3: 3, a4: 4, a5: 5, a6: 6, a7: 7, a8: 8, a9: 9, a10: 10, "
      "a11: 11, a12: 12, a13: 13, a14: 14, a15: 15, a16: 16) "
      "let value = await pending }\n";
  CHECK(lower_parallel_domain(WIDE_TASK));
  w_seed_parallel_selection1_counts wide_selection_counts;
  w_seed_parallel_selection1_result wide_selection_result;
  CHECK(w_seed_parallel_selection1_measure(
            &fixture.hir_program, &fixture.hir_result, &wide_selection_counts,
            &wide_selection_result) == W_SEED_PARALLEL_SELECTION1_OK &&
        wide_selection_counts.tasks == 1u);
  w_seed_parallel_selection1_task wide_selection_task[1];
  const w_seed_parallel_selection1_output wide_selection_output = {
      wide_selection_task, 1u};
  CHECK(w_seed_parallel_selection1_run(
            &fixture.hir_program, &fixture.hir_result, &wide_selection_output,
            &wide_selection_result) == W_SEED_PARALLEL_SELECTION1_OK);
  w_seed_parallel_selection1_program wide_selection;
  CHECK(w_seed_parallel_selection1_program_from_output(
            &wide_selection_output, &wide_selection_result, &wide_selection) &&
        w_seed_parallel_selection1_verify(
            &fixture.hir_program, &fixture.hir_result, &wide_selection,
            &wide_selection_result));
  w_seed_parallel_invocation1_counts wide_invocation_counts;
  w_seed_parallel_invocation1_result wide_invocation_result;
  CHECK(w_seed_parallel_invocation1_measure(
            &fixture.hir_program, &fixture.hir_result, &wide_selection,
            &wide_selection_result, &wide_invocation_counts,
            &wide_invocation_result) == W_SEED_PARALLEL_INVOCATION1_OK &&
        wide_invocation_counts.tasks == 1u &&
        wide_invocation_counts.arguments == 17u);
  w_seed_parallel_invocation1_task wide_invocation_task[1];
  w_seed_parallel_invocation1_argument wide_invocation_arguments[17];
  const w_seed_parallel_invocation1_output wide_invocation_output = {
      wide_invocation_task, 1u, wide_invocation_arguments, 17u};
  CHECK(w_seed_parallel_invocation1_run(
            &fixture.hir_program, &fixture.hir_result, &wide_selection,
            &wide_selection_result, &wide_invocation_output,
            &wide_invocation_result) == W_SEED_PARALLEL_INVOCATION1_OK);
  w_seed_parallel_invocation1_program wide_invocation;
  CHECK(w_seed_parallel_invocation1_program_from_output(
            &wide_invocation_output, &wide_invocation_result,
            &wide_invocation) &&
        wide_invocation.task_count == 1u &&
        wide_invocation.argument_count == 17u &&
        wide_invocation.arguments[16].parameter_ordinal == 16u &&
        w_seed_parallel_invocation1_verify(
            &fixture.hir_program, &fixture.hir_result, &wide_selection,
            &wide_selection_result, &wide_invocation,
            &wide_invocation_result));
  CHECK(test_parallel_mlir_entries1(
      &fixture.hir_program, &fixture.hir_result, &wide_selection,
      &wide_selection_result, &wide_invocation, &wide_invocation_result, NULL,
      1u, 17u, "%arg16: i64"));
  int64_t wide_value = 0;
  CHECK(w_seed_parallel_invocation1_evaluate_task(
            &fixture.hir_program, &fixture.hir_result, &wide_selection,
            &wide_selection_result, &wide_invocation,
            &wide_invocation_result, 0u, &wide_value) ==
            W_SEED_PARALLEL_INVOCATION1_OK &&
        wide_value == 16);
#if defined(_WIN32) && defined(_WIN64)
  w_seed_parallel_provider1_input wide_provider_input = {
      &fixture.hir_program, &fixture.hir_result, &wide_selection,
      &wide_selection_result, &wide_invocation, &wide_invocation_result, 1u};
  w_seed_parallel_provider1_outcome wide_outcome[1];
  int64_t wide_workspace[1];
  const w_seed_parallel_provider1_output wide_provider_output = {
      wide_outcome, 1u, wide_workspace, 1u};
  w_seed_parallel_provider1_result wide_provider_result;
  w_seed_parallel_provider1_receipt wide_provider_receipt;
  CHECK(w_seed_parallel_provider1_execute(
            &wide_provider_input, &wide_provider_output, &wide_provider_result,
            &wide_provider_receipt) == W_SEED_PARALLEL_PROVIDER1_OK &&
        wide_outcome[0].value == 16 && wide_workspace[0] == 16 &&
        w_seed_parallel_provider1_verify_outcomes(
            &wide_provider_input, wide_outcome, 1u,
            &wide_provider_result));
#endif
  CHECK(w_seed_parallel_selection0_select(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
            W_SEED_PARALLEL_SELECTION0_OK);
  w_seed_parallel_invocation0_plan legacy_wide_plan;
  (void)memset(&legacy_wide_plan, 0x77, sizeof(legacy_wide_plan));
  const w_seed_parallel_invocation0_plan legacy_wide_before = legacy_wide_plan;
  CHECK(w_seed_parallel_invocation0_select(
            &fixture.hir_program, &fixture.hir_result, &selection,
            &legacy_wide_plan) == W_SEED_PARALLEL_INVOCATION0_UNSUPPORTED &&
        memcmp(&legacy_wide_plan, &legacy_wide_before,
               sizeof(legacy_wide_plan)) == 0);

  static const char NO_PARALLEL_TASK[] = "entry { }\n";
  CHECK(lower_parallel_domain(NO_PARALLEL_TASK));
  selection_sentinel = selection_before;
  CHECK(w_seed_parallel_selection0_select(
            &fixture.hir_program, &fixture.hir_result, &selection_sentinel) ==
            W_SEED_PARALLEL_SELECTION0_UNSUPPORTED &&
        memcmp(&selection_sentinel, &selection_before,
               sizeof(selection_sentinel)) == 0);

  static const char OVERFLOWING_TASK[] =
      "fn prepare(value: i64): i64 { return value + 1 }\n"
      "entry { let pending = spawn<.domain> "
      "prepare(value: 9223372036854775807) let value = await pending }\n";
  CHECK(lower_parallel_domain(OVERFLOWING_TASK));
  CHECK(w_seed_parallel_selection0_select(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_PARALLEL_SELECTION0_OK);
  w_seed_parallel_invocation0_plan invocation_sentinel;
  (void)memset(&invocation_sentinel, 0x4e, sizeof(invocation_sentinel));
  const w_seed_parallel_invocation0_plan invocation_before =
      invocation_sentinel;
  CHECK(w_seed_parallel_invocation0_select(
            &fixture.hir_program, &fixture.hir_result, &selection,
            &invocation_sentinel) == W_SEED_PARALLEL_INVOCATION0_UNSUPPORTED &&
        memcmp(&invocation_sentinel, &invocation_before,
               sizeof(invocation_before)) == 0);
  return true;
}

static bool test_parallel_panic_invocation1(void) {
  static const char SOURCE[] =
      "fn prepare(value: i64): i64 { return value + 1 }\n"
      "fn fail(): i64 { panic(\"parallel invariant\") }\n"
      "entry { let firstTask = spawn<.domain> prepare(value: 20) "
      "let secondTask = spawn<.domain> fail() "
      "let first = await firstTask let second = await secondTask }\n";
  CHECK(lower_parallel_domain(SOURCE));

  w_seed_parallel_selection1_counts selection_counts;
  w_seed_parallel_selection1_result selection_result;
  CHECK(w_seed_parallel_selection1_measure(
            &fixture.hir_program, &fixture.hir_result, &selection_counts,
            &selection_result) == W_SEED_PARALLEL_SELECTION1_OK &&
        selection_counts.tasks == 2u);
  w_seed_parallel_selection1_task selection_tasks[2];
  const w_seed_parallel_selection1_output selection_output = {
      selection_tasks, 2u};
  CHECK(w_seed_parallel_selection1_run(
            &fixture.hir_program, &fixture.hir_result, &selection_output,
            &selection_result) == W_SEED_PARALLEL_SELECTION1_OK);
  w_seed_parallel_selection1_program selection;
  CHECK(w_seed_parallel_selection1_program_from_output(
            &selection_output, &selection_result, &selection) &&
        w_seed_parallel_selection1_verify(
            &fixture.hir_program, &fixture.hir_result, &selection,
            &selection_result));
  w_seed_parallel_invocation1_counts invocation_counts;
  w_seed_parallel_invocation1_result invocation_result;
  CHECK(w_seed_parallel_invocation1_measure(
            &fixture.hir_program, &fixture.hir_result, &selection,
            &selection_result, &invocation_counts, &invocation_result) ==
            W_SEED_PARALLEL_INVOCATION1_OK &&
        invocation_counts.tasks == 2u && invocation_counts.arguments == 1u);
  w_seed_parallel_invocation1_task invocation_tasks[2];
  w_seed_parallel_invocation1_argument invocation_arguments[1];
  const w_seed_parallel_invocation1_output invocation_output = {
      invocation_tasks, 2u, invocation_arguments, 1u};
  CHECK(w_seed_parallel_invocation1_run(
            &fixture.hir_program, &fixture.hir_result, &selection,
            &selection_result, &invocation_output, &invocation_result) ==
        W_SEED_PARALLEL_INVOCATION1_OK);
  w_seed_parallel_invocation1_program invocation;
  CHECK(w_seed_parallel_invocation1_program_from_output(
            &invocation_output, &invocation_result, &invocation) &&
        w_seed_parallel_invocation1_verify(
            &fixture.hir_program, &fixture.hir_result, &selection,
            &selection_result, &invocation, &invocation_result));
  CHECK(invocation.tasks[0].kind ==
            W_SEED_PARALLEL_INVOCATION1_TASK_VALUE_I64 &&
        invocation.tasks[0].panic_terminator == W_SEED_HIR0_NONE &&
        invocation.tasks[0].panic_message_value == W_SEED_HIR0_NONE &&
        invocation.tasks[0].panic_code == W_SEED_HIR0_PANIC_CODE_INVALID &&
        invocation.tasks[1].kind ==
            W_SEED_PARALLEL_INVOCATION1_TASK_PANIC &&
        invocation.tasks[1].panic_terminator <
            fixture.hir_program.terminator_count &&
        invocation.tasks[1].panic_message_value <
            fixture.hir_program.value_count &&
        invocation.tasks[1].panic_code == W_SEED_HIR0_PANIC_CODE_EXPLICIT);
  const w_seed_hir0_value *message =
      &fixture.hir_program.values[invocation.tasks[1].panic_message_value];
  CHECK(message->byte_count == sizeof("parallel invariant") - 1u &&
        memcmp(fixture.hir_program.value_bytes + message->byte_offset,
               "parallel invariant", sizeof("parallel invariant") - 1u) ==
            0);

  int64_t value = 0;
  CHECK(w_seed_parallel_invocation1_evaluate_task(
            &fixture.hir_program, &fixture.hir_result, &selection,
            &selection_result, &invocation, &invocation_result, 0u, &value) ==
            W_SEED_PARALLEL_INVOCATION1_OK &&
        value == 21);
  value = INT64_C(0x123456789abcdef);
  const int64_t value_before = value;
  CHECK(w_seed_parallel_invocation1_evaluate_task(
            &fixture.hir_program, &fixture.hir_result, &selection,
            &selection_result, &invocation, &invocation_result, 1u, &value) ==
            W_SEED_PARALLEL_INVOCATION1_EVALUATION_FAILURE &&
        value == value_before);

  const w_seed_parallel_invocation1_task panic_task = invocation_tasks[1];
  invocation_tasks[1].kind = W_SEED_PARALLEL_INVOCATION1_TASK_VALUE_I64;
  CHECK(!w_seed_parallel_invocation1_verify(
      &fixture.hir_program, &fixture.hir_result, &selection,
      &selection_result, &invocation, &invocation_result));
  invocation_tasks[1] = panic_task;
  invocation_tasks[1].panic_terminator ^= 1u;
  CHECK(!w_seed_parallel_invocation1_verify(
      &fixture.hir_program, &fixture.hir_result, &selection,
      &selection_result, &invocation, &invocation_result));
  invocation_tasks[1] = panic_task;
  invocation_tasks[1].panic_message_value = W_SEED_HIR0_NONE;
  CHECK(!w_seed_parallel_invocation1_verify(
      &fixture.hir_program, &fixture.hir_result, &selection,
      &selection_result, &invocation, &invocation_result));
  invocation_tasks[1] = panic_task;
  invocation_tasks[1].panic_code = W_SEED_HIR0_PANIC_CODE_INVALID;
  CHECK(!w_seed_parallel_invocation1_verify(
      &fixture.hir_program, &fixture.hir_result, &selection,
      &selection_result, &invocation, &invocation_result));
  invocation_tasks[1] = panic_task;
  CHECK(w_seed_parallel_invocation1_verify(
      &fixture.hir_program, &fixture.hir_result, &selection,
      &selection_result, &invocation, &invocation_result));
  return true;
}

static bool test_parallel_panic_binding1(void) {
#if !defined(_WIN32) || !defined(_WIN64)
  /* The current local PLATFORM1 authority is deliberately Windows-only. */
  return true;
#else
  static const char SOURCE[] =
      "fn prepare(value: i64): i64 { return value + 1 }\n"
      "fn fail(): i64 { panic(\"parallel invariant\") }\n"
      "entry { let firstTask = spawn<.domain> prepare(value: 20) "
      "let secondTask = spawn<.domain> fail() "
      "let first = await firstTask let second = await secondTask }\n";
  CHECK(lower_parallel_domain(SOURCE));

  w_seed_parallel_selection1_counts selection_counts;
  w_seed_parallel_selection1_result selection_result;
  CHECK(w_seed_parallel_selection1_measure(
            &fixture.hir_program, &fixture.hir_result, &selection_counts,
            &selection_result) == W_SEED_PARALLEL_SELECTION1_OK &&
        selection_counts.tasks == 2u);
  w_seed_parallel_selection1_task selection_tasks[2];
  const w_seed_parallel_selection1_output selection_output = {
      selection_tasks, 2u};
  CHECK(w_seed_parallel_selection1_run(
            &fixture.hir_program, &fixture.hir_result, &selection_output,
            &selection_result) == W_SEED_PARALLEL_SELECTION1_OK);
  w_seed_parallel_selection1_program selection;
  CHECK(w_seed_parallel_selection1_program_from_output(
            &selection_output, &selection_result, &selection) &&
        w_seed_parallel_selection1_verify(
            &fixture.hir_program, &fixture.hir_result, &selection,
            &selection_result));

  w_seed_parallel_invocation1_counts invocation_counts;
  w_seed_parallel_invocation1_result invocation_result;
  CHECK(w_seed_parallel_invocation1_measure(
            &fixture.hir_program, &fixture.hir_result, &selection,
            &selection_result, &invocation_counts, &invocation_result) ==
            W_SEED_PARALLEL_INVOCATION1_OK &&
        invocation_counts.tasks == 2u && invocation_counts.arguments == 1u);
  w_seed_parallel_invocation1_task invocation_tasks[2];
  w_seed_parallel_invocation1_argument invocation_arguments[1];
  const w_seed_parallel_invocation1_output invocation_output = {
      invocation_tasks, 2u, invocation_arguments, 1u};
  CHECK(w_seed_parallel_invocation1_run(
            &fixture.hir_program, &fixture.hir_result, &selection,
            &selection_result, &invocation_output, &invocation_result) ==
        W_SEED_PARALLEL_INVOCATION1_OK);
  w_seed_parallel_invocation1_program invocation;
  CHECK(w_seed_parallel_invocation1_program_from_output(
            &invocation_output, &invocation_result, &invocation) &&
        w_seed_parallel_invocation1_verify(
            &fixture.hir_program, &fixture.hir_result, &selection,
            &selection_result, &invocation, &invocation_result));

  w_seed_parallel_local_provider1_authority authority;
  CHECK(w_seed_parallel_local_provider1_open(
            W_SEED_PARALLEL_LOCAL_PROVIDER1_WINDOWS_AMD64, &authority) &&
        w_seed_parallel_local_provider1_verify(&authority));
  const w_seed_parallel_panic_binding1_input input = {
      &fixture.hir_program, &fixture.hir_result, &selection,
      &selection_result, &invocation, &invocation_result, &authority, 1u,
      7u};

  w_seed_parallel_panic_binding1_counts required;
  w_seed_parallel_panic_binding1_result measured;
  CHECK(w_seed_parallel_panic_binding1_measure(&input, &required, &measured) ==
            W_SEED_PARALLEL_PANIC_BINDING1_OK &&
        required.source_id_bytes != 0u && required.module_id_bytes != 0u &&
        required.message_bytes == sizeof("parallel invariant") - 1u &&
        measured.required.source_id_bytes == required.source_id_bytes &&
        measured.required.module_id_bytes == required.module_id_bytes &&
        measured.required.message_bytes == required.message_bytes &&
        measured.written.source_id_bytes == 0u &&
        measured.written.module_id_bytes == 0u &&
        measured.written.message_bytes == 0u && measured.task_count == 2u &&
        measured.panic_count == 1u && measured.source_task_index == 1u &&
        measured.generation == 7u);

  union measure_alias_storage {
    w_seed_parallel_panic_binding1_counts counts;
    w_seed_parallel_panic_binding1_result result;
  } measure_alias;
  (void)memset(&measure_alias, 0x2au, sizeof(measure_alias));
  const union measure_alias_storage measure_alias_before = measure_alias;
  CHECK(w_seed_parallel_panic_binding1_measure(
            &input, &measure_alias.counts, &measure_alias.result) ==
            W_SEED_PARALLEL_PANIC_BINDING1_ALIAS &&
        memcmp(&measure_alias, &measure_alias_before,
               sizeof(measure_alias)) == 0);

  uint8_t source_id[128];
  uint8_t module_id[128];
  uint8_t message[128];
  w_seed_parallel_panic_binding1_signal signal;
  w_seed_parallel_platform1_completion completions[2];
  w_seed_parallel_platform1_receipt receipt;
  w_seed_parallel_provider0_kind provider_kind;
  const w_seed_parallel_panic_binding1_workspace workspace = {
      completions, 2u, &receipt, &provider_kind};
  const w_seed_parallel_panic_binding1_output output = {
      source_id, sizeof(source_id), module_id, sizeof(module_id), message,
      sizeof(message), &signal};
  w_seed_parallel_panic_binding1_result result;
  (void)memset(source_id, 0xa0, sizeof(source_id));
  (void)memset(module_id, 0xa1, sizeof(module_id));
  (void)memset(message, 0xa2, sizeof(message));
  CHECK(w_seed_parallel_panic_binding1_run(&input, &workspace, &output,
                                           &result) ==
            W_SEED_PARALLEL_PANIC_BINDING1_OK &&
        result.required.source_id_bytes == required.source_id_bytes &&
        result.required.module_id_bytes == required.module_id_bytes &&
        result.required.message_bytes == required.message_bytes &&
        result.written.source_id_bytes == required.source_id_bytes &&
        result.written.module_id_bytes == required.module_id_bytes &&
        result.written.message_bytes == required.message_bytes &&
        result.task_count == 2u && result.panic_count == 1u &&
        result.source_task_index == 1u && result.generation == 7u);

  const w_seed_parallel_invocation1_task *panic_task = &invocation.tasks[1];
  const w_seed_hir0_module *panic_module =
      &fixture.hir_program.modules[
          fixture.hir_program.functions[panic_task->function_index]
              .module_index];
  const w_seed_hir0_value *panic_message =
      &fixture.hir_program.values[panic_task->panic_message_value];
  const w_seed_hir0_call *panic_call =
      &fixture.hir_program.calls[panic_task->call_index];
  const w_seed_hir0_function *panic_function =
      &fixture.hir_program.functions[panic_task->function_index];
  const w_seed_hir0_terminator *panic_terminator =
      &fixture.hir_program.terminators[panic_task->panic_terminator];
  CHECK(signal.source_task_index == 1u && signal.lexical_index == 1u &&
        signal.call_index == panic_task->call_index &&
        signal.function_index == panic_task->function_index &&
        signal.module_index == panic_function->module_index &&
        signal.source_expression == panic_call->source_expression &&
        signal.terminator_index == panic_task->panic_terminator &&
        signal.message_value_index == panic_task->panic_message_value &&
        signal.panic_code == W_SEED_HIR0_PANIC_CODE_EXPLICIT &&
        !signal.semantic_value_published &&
        memcmp(&signal.function_span, &panic_function->source_span,
               sizeof(signal.function_span)) == 0 &&
        memcmp(&signal.call_span, &panic_call->source_span,
               sizeof(signal.call_span)) == 0 &&
        memcmp(&signal.terminator_span, &panic_terminator->source_span,
               sizeof(signal.terminator_span)) == 0 &&
        signal.source_length == panic_module->source_length &&
        memcmp(signal.source_sha256, panic_module->source_sha256,
               sizeof(signal.source_sha256)) == 0 &&
        signal.source_id_bytes == source_id &&
        signal.source_id_byte_count == panic_module->source_id.count &&
        signal.module_id_bytes == module_id &&
        signal.module_id_byte_count == panic_module->module_id.count &&
        signal.message_bytes == message &&
        signal.message_byte_count == panic_message->byte_count &&
        memcmp(source_id,
               fixture.hir_text + panic_module->source_id.offset,
               required.source_id_bytes) == 0 &&
        memcmp(module_id,
               fixture.hir_text + panic_module->module_id.offset,
               required.module_id_bytes) == 0 &&
        memcmp(message, fixture.hir_value_bytes + panic_message->byte_offset,
               required.message_bytes) == 0 &&
        signal.provider_kind == W_SEED_PARALLEL_PROVIDER0_KIND_WINDOWS_KERNEL32 &&
        signal.provider_capacity == 1u && signal.generation == 7u &&
        signal.platform_receipt.started_count == 2u &&
        signal.platform_receipt.settled_count == 2u &&
        signal.platform_receipt.canceled_before_start_count == 0u &&
        signal.platform_receipt.maximum_active == 1u &&
        signal.platform_receipt.cancellation_requested &&
        signal.platform_receipt.cancellation_source_index == 1u &&
        signal.platform_receipt.panic_requested &&
        signal.platform_receipt.panic_source_index == 1u &&
        signal.platform_receipt.panic_code ==
            W_SEED_PARALLEL_PLATFORM1_PANIC_EXPLICIT &&
        w_seed_parallel_panic_binding1_verify(
            &input, &workspace, &output, &result));

  uint8_t source_before[sizeof(source_id)];
  uint8_t module_before[sizeof(module_id)];
  uint8_t message_before[sizeof(message)];
  (void)memcpy(source_before, source_id, sizeof(source_id));
  (void)memcpy(module_before, module_id, sizeof(module_id));
  (void)memcpy(message_before, message, sizeof(message));
  const w_seed_parallel_panic_binding1_signal signal_before = signal;
  const w_seed_parallel_panic_binding1_result result_before = result;

  w_seed_parallel_panic_binding1_output short_output = output;
  short_output.message_capacity = required.message_bytes - 1u;
  uint8_t short_source[128];
  uint8_t short_module[128];
  uint8_t short_message[128];
  w_seed_parallel_panic_binding1_signal short_signal;
  w_seed_parallel_panic_binding1_result short_result;
  w_seed_parallel_platform1_completion short_completions[2];
  w_seed_parallel_platform1_receipt short_receipt;
  w_seed_parallel_provider0_kind short_provider_kind;
  (void)memset(short_source, 0xa1, sizeof(short_source));
  (void)memset(short_module, 0xa2, sizeof(short_module));
  (void)memset(short_message, 0xa3, sizeof(short_message));
  (void)memset(&short_signal, 0xa4, sizeof(short_signal));
  (void)memset(&short_result, 0xa5, sizeof(short_result));
  (void)memset(short_completions, 0xa6, sizeof(short_completions));
  (void)memset(&short_receipt, 0xa7, sizeof(short_receipt));
  (void)memset(&short_provider_kind, 0xa8, sizeof(short_provider_kind));
  short_output.source_id_bytes = short_source;
  short_output.module_id_bytes = short_module;
  short_output.message_bytes = short_message;
  short_output.signal = &short_signal;
  const w_seed_parallel_panic_binding1_workspace short_workspace = {
      short_completions, 2u, &short_receipt, &short_provider_kind};
  uint8_t short_source_before[sizeof(short_source)];
  uint8_t short_module_before[sizeof(short_module)];
  uint8_t short_message_before[sizeof(short_message)];
  (void)memcpy(short_source_before, short_source, sizeof(short_source));
  (void)memcpy(short_module_before, short_module, sizeof(short_module));
  (void)memcpy(short_message_before, short_message, sizeof(short_message));
  const w_seed_parallel_panic_binding1_signal short_signal_before =
      short_signal;
  const w_seed_parallel_panic_binding1_result short_result_before =
      short_result;
  CHECK(w_seed_parallel_panic_binding1_run(&input, &short_workspace,
                                           &short_output, &short_result) ==
            W_SEED_PARALLEL_PANIC_BINDING1_CAPACITY &&
        memcmp(short_source, short_source_before, sizeof(short_source)) == 0 &&
        memcmp(short_module, short_module_before, sizeof(short_module)) == 0 &&
        memcmp(short_message, short_message_before, sizeof(short_message)) == 0 &&
        memcmp(&short_signal, &short_signal_before,
               sizeof(short_signal)) == 0 &&
        memcmp(&short_result, &short_result_before,
               sizeof(short_result)) == 0);

  uint8_t alias_source[128];
  uint8_t alias_module[128];
  uint8_t alias_message[128];
  w_seed_parallel_panic_binding1_signal alias_signal;
  w_seed_parallel_panic_binding1_result alias_result;
  (void)memset(alias_source, 0xb1, sizeof(alias_source));
  (void)memset(alias_module, 0xb2, sizeof(alias_module));
  (void)memset(alias_message, 0xb3, sizeof(alias_message));
  (void)memset(&alias_signal, 0xb4, sizeof(alias_signal));
  (void)memset(&alias_result, 0xb5, sizeof(alias_result));
  const w_seed_parallel_panic_binding1_output alias_output = {
      alias_source, sizeof(alias_source), alias_source, sizeof(alias_source),
      alias_message, sizeof(alias_message), &alias_signal};
  uint8_t alias_source_before[sizeof(alias_source)];
  (void)memcpy(alias_source_before, alias_source, sizeof(alias_source));
  const w_seed_parallel_panic_binding1_result alias_result_before =
      alias_result;
  CHECK(w_seed_parallel_panic_binding1_run(
            &input, &workspace, &alias_output, &alias_result) ==
            W_SEED_PARALLEL_PANIC_BINDING1_ALIAS &&
        memcmp(alias_source, alias_source_before, sizeof(alias_source)) == 0 &&
        memcmp(&alias_result, &alias_result_before,
               sizeof(alias_result)) == 0);

  /* PARPROV1 remains success-only: its scalar adapter reaches task failure
   * on the verified source panic and does not publish a partial result. */
  const w_seed_parallel_provider1_input success_only_input = {
      &fixture.hir_program, &fixture.hir_result, &selection,
      &selection_result, &invocation, &invocation_result, 1u};
  w_seed_parallel_provider1_outcome success_only_outcomes[2];
  int64_t success_only_workspace[2];
  w_seed_parallel_provider1_result success_only_result;
  w_seed_parallel_provider1_receipt success_only_receipt;
  (void)memset(success_only_outcomes, 0xc1, sizeof(success_only_outcomes));
  (void)memset(success_only_workspace, 0xc2, sizeof(success_only_workspace));
  (void)memset(&success_only_result, 0xc3, sizeof(success_only_result));
  (void)memset(&success_only_receipt, 0xc4, sizeof(success_only_receipt));
  const w_seed_parallel_provider1_outcome success_only_outcomes_before[2] = {
      success_only_outcomes[0], success_only_outcomes[1]};
  const w_seed_parallel_provider1_result success_only_result_before =
      success_only_result;
  const w_seed_parallel_provider1_receipt success_only_receipt_before =
      success_only_receipt;
  CHECK(w_seed_parallel_provider1_execute(
            &success_only_input,
            &(w_seed_parallel_provider1_output){
                success_only_outcomes, 2u, success_only_workspace, 2u},
            &success_only_result, &success_only_receipt) ==
            W_SEED_PARALLEL_PROVIDER1_TASK_FAILURE &&
        memcmp(success_only_outcomes, success_only_outcomes_before,
               sizeof(success_only_outcomes)) == 0 &&
        memcmp(&success_only_result, &success_only_result_before,
               sizeof(success_only_result)) == 0 &&
        memcmp(&success_only_receipt, &success_only_receipt_before,
               sizeof(success_only_receipt)) == 0);

  w_seed_parallel_panic_binding1_input forged_input = input;
  w_seed_parallel_panic_binding1_counts forged_counts;
  w_seed_parallel_panic_binding1_result forged_result;
  (void)memset(&forged_counts, 0xd1, sizeof(forged_counts));
  (void)memset(&forged_result, 0xd2, sizeof(forged_result));
  const w_seed_parallel_panic_binding1_counts forged_counts_before =
      forged_counts;
  const w_seed_parallel_panic_binding1_result forged_result_before =
      forged_result;
  w_seed_parallel_invocation1_task saved_task = invocation_tasks[1];
  invocation_tasks[1].kind = W_SEED_PARALLEL_INVOCATION1_TASK_VALUE_I64;
  CHECK(w_seed_parallel_panic_binding1_measure(
            &input, &forged_counts, &forged_result) ==
            W_SEED_PARALLEL_PANIC_BINDING1_HIR &&
        memcmp(&forged_counts, &forged_counts_before,
               sizeof(forged_counts)) == 0 &&
        memcmp(&forged_result, &forged_result_before,
               sizeof(forged_result)) == 0);
  invocation_tasks[1] = saved_task;
  invocation_tasks[1].call_index = W_SEED_HIR0_NONE;
  CHECK(w_seed_parallel_panic_binding1_measure(
            &input, &forged_counts, &forged_result) ==
            W_SEED_PARALLEL_PANIC_BINDING1_HIR);
  invocation_tasks[1] = saved_task;
  invocation_tasks[1].panic_terminator ^= 1u;
  CHECK(w_seed_parallel_panic_binding1_measure(
            &input, &forged_counts, &forged_result) ==
            W_SEED_PARALLEL_PANIC_BINDING1_HIR);
  invocation_tasks[1] = saved_task;
  invocation_tasks[1].panic_message_value = W_SEED_HIR0_NONE;
  CHECK(w_seed_parallel_panic_binding1_measure(
            &input, &forged_counts, &forged_result) ==
            W_SEED_PARALLEL_PANIC_BINDING1_HIR);
  invocation_tasks[1] = saved_task;
  const w_seed_parallel_invocation1_result saved_invocation_result =
      invocation_result;
  invocation_result.semantic_digest[0] ^= 1u;
  CHECK(w_seed_parallel_panic_binding1_measure(
            &input, &forged_counts, &forged_result) ==
            W_SEED_PARALLEL_PANIC_BINDING1_HIR);
  invocation_result = saved_invocation_result;
  CHECK(w_seed_parallel_panic_binding1_measure(
            &input, &forged_counts, &forged_result) ==
            W_SEED_PARALLEL_PANIC_BINDING1_OK);

  w_seed_parallel_local_provider1_authority forged_authority = authority;
  forged_authority.receipt.generation ^= 1u;
  forged_input.provider_authority = &forged_authority;
  CHECK(w_seed_parallel_panic_binding1_run(&forged_input, &workspace, &output,
                                           &result) ==
            W_SEED_PARALLEL_PANIC_BINDING1_AUTHORITY &&
        memcmp(source_id, source_before, sizeof(source_id)) == 0 &&
        memcmp(module_id, module_before, sizeof(module_id)) == 0 &&
        memcmp(message, message_before, sizeof(message)) == 0 &&
        memcmp(&signal, &signal_before, sizeof(signal)) == 0 &&
        memcmp(&result, &result_before, sizeof(result)) == 0);
  forged_input.provider_authority = &authority;
  forged_input.generation = 0u;
  CHECK(w_seed_parallel_panic_binding1_run(&forged_input, &workspace, &output,
                                           &result) ==
            W_SEED_PARALLEL_PANIC_BINDING1_INVALID &&
        memcmp(source_id, source_before, sizeof(source_id)) == 0 &&
        memcmp(&signal, &signal_before, sizeof(signal)) == 0 &&
        memcmp(&result, &result_before, sizeof(result)) == 0);
  forged_input.generation = input.generation;

  const w_seed_parallel_platform1_receipt saved_receipt = receipt;
  receipt.cancellation_source_index ^= 1u;
  CHECK(!w_seed_parallel_panic_binding1_verify(&input, &workspace, &output,
                                               &result));
  receipt = saved_receipt;
  const w_seed_parallel_platform1_completion saved_completion = completions[0];
  completions[0].kind = W_SEED_PARALLEL_PLATFORM1_COMPLETION_PANIC;
  CHECK(!w_seed_parallel_panic_binding1_verify(&input, &workspace, &output,
                                               &result));
  completions[0] = saved_completion;
  const w_seed_parallel_provider0_kind saved_kind = provider_kind;
  provider_kind = W_SEED_PARALLEL_PROVIDER0_KIND_NONE;
  CHECK(!w_seed_parallel_panic_binding1_verify(&input, &workspace, &output,
                                               &result));
  provider_kind = saved_kind;
  const uint8_t *saved_message_pointer = signal.message_bytes;
  signal.message_bytes = message + 1u;
  CHECK(!w_seed_parallel_panic_binding1_verify(&input, &workspace, &output,
                                               &result));
  signal.message_bytes = saved_message_pointer;
  const size_t saved_message_count = signal.message_byte_count;
  signal.message_byte_count += 1u;
  CHECK(!w_seed_parallel_panic_binding1_verify(&input, &workspace, &output,
                                               &result));
  signal.message_byte_count = saved_message_count;
  const uint8_t saved_result_digest = result.semantic_digest[0];
  result.semantic_digest[0] ^= 1u;
  CHECK(!w_seed_parallel_panic_binding1_verify(&input, &workspace, &output,
                                               &result));
  result.semantic_digest[0] = saved_result_digest;
  CHECK(w_seed_parallel_panic_binding1_verify(&input, &workspace, &output,
                                              &result));

  uint8_t source_id_two[128];
  uint8_t module_id_two[128];
  uint8_t message_two[128];
  w_seed_parallel_panic_binding1_signal signal_two;
  w_seed_parallel_platform1_completion completions_two[2];
  w_seed_parallel_platform1_receipt receipt_two;
  w_seed_parallel_provider0_kind provider_kind_two;
  const w_seed_parallel_panic_binding1_workspace workspace_two = {
      completions_two, 2u, &receipt_two, &provider_kind_two};
  const w_seed_parallel_panic_binding1_output output_two = {
      source_id_two, sizeof(source_id_two), module_id_two, sizeof(module_id_two),
      message_two, sizeof(message_two), &signal_two};
  w_seed_parallel_panic_binding1_result result_two;
  w_seed_parallel_panic_binding1_input input_two = input;
  input_two.provider_capacity = 2u;
  CHECK(w_seed_parallel_panic_binding1_run(
            &input_two, &workspace_two, &output_two, &result_two) ==
            W_SEED_PARALLEL_PANIC_BINDING1_OK &&
        w_seed_parallel_panic_binding1_verify(
            &input_two, &workspace_two, &output_two, &result_two) &&
        memcmp(result.semantic_digest, result_two.semantic_digest,
               sizeof(result.semantic_digest)) == 0 &&
        memcmp(result.provenance_digest, result_two.provenance_digest,
               sizeof(result.provenance_digest)) != 0 &&
        memcmp(message, message_two, required.message_bytes) == 0);

  enum {
    PANIC_ALIAS_WORKSPACE = 0u,
    PANIC_ALIAS_COMPLETIONS,
    PANIC_ALIAS_RECEIPT,
    PANIC_ALIAS_PROVIDER_KIND,
    PANIC_ALIAS_OUTPUT,
    PANIC_ALIAS_SOURCE,
    PANIC_ALIAS_MODULE,
    PANIC_ALIAS_MESSAGE,
    PANIC_ALIAS_SIGNAL,
    PANIC_ALIAS_RESULT,
  };
  typedef struct {
    unsigned pointer_slot;
    unsigned target_slot;
  } panic_alias_pair;
  /* The ten writable ranges have 45 unordered pairs. Three descriptor-to-
   * descriptor argument reinterpretations are not representable in C; the
   * remaining 42 pointer/range cases are exercised below. */
  static const panic_alias_pair PANIC_ALIAS_PAIRS[] = {
      {PANIC_ALIAS_COMPLETIONS, PANIC_ALIAS_WORKSPACE},
      {PANIC_ALIAS_RECEIPT, PANIC_ALIAS_WORKSPACE},
      {PANIC_ALIAS_PROVIDER_KIND, PANIC_ALIAS_WORKSPACE},
      {PANIC_ALIAS_SOURCE, PANIC_ALIAS_WORKSPACE},
      {PANIC_ALIAS_MODULE, PANIC_ALIAS_WORKSPACE},
      {PANIC_ALIAS_MESSAGE, PANIC_ALIAS_WORKSPACE},
      {PANIC_ALIAS_SIGNAL, PANIC_ALIAS_WORKSPACE},
      {PANIC_ALIAS_RECEIPT, PANIC_ALIAS_COMPLETIONS},
      {PANIC_ALIAS_PROVIDER_KIND, PANIC_ALIAS_COMPLETIONS},
      {PANIC_ALIAS_COMPLETIONS, PANIC_ALIAS_OUTPUT},
      {PANIC_ALIAS_SOURCE, PANIC_ALIAS_COMPLETIONS},
      {PANIC_ALIAS_MODULE, PANIC_ALIAS_COMPLETIONS},
      {PANIC_ALIAS_MESSAGE, PANIC_ALIAS_COMPLETIONS},
      {PANIC_ALIAS_SIGNAL, PANIC_ALIAS_COMPLETIONS},
      {PANIC_ALIAS_COMPLETIONS, PANIC_ALIAS_RESULT},
      {PANIC_ALIAS_PROVIDER_KIND, PANIC_ALIAS_RECEIPT},
      {PANIC_ALIAS_RECEIPT, PANIC_ALIAS_OUTPUT},
      {PANIC_ALIAS_SOURCE, PANIC_ALIAS_RECEIPT},
      {PANIC_ALIAS_MODULE, PANIC_ALIAS_RECEIPT},
      {PANIC_ALIAS_MESSAGE, PANIC_ALIAS_RECEIPT},
      {PANIC_ALIAS_SIGNAL, PANIC_ALIAS_RECEIPT},
      {PANIC_ALIAS_RECEIPT, PANIC_ALIAS_RESULT},
      {PANIC_ALIAS_PROVIDER_KIND, PANIC_ALIAS_OUTPUT},
      {PANIC_ALIAS_SOURCE, PANIC_ALIAS_PROVIDER_KIND},
      {PANIC_ALIAS_MODULE, PANIC_ALIAS_PROVIDER_KIND},
      {PANIC_ALIAS_MESSAGE, PANIC_ALIAS_PROVIDER_KIND},
      {PANIC_ALIAS_SIGNAL, PANIC_ALIAS_PROVIDER_KIND},
      {PANIC_ALIAS_PROVIDER_KIND, PANIC_ALIAS_RESULT},
      {PANIC_ALIAS_SOURCE, PANIC_ALIAS_OUTPUT},
      {PANIC_ALIAS_MODULE, PANIC_ALIAS_OUTPUT},
      {PANIC_ALIAS_MESSAGE, PANIC_ALIAS_OUTPUT},
      {PANIC_ALIAS_SIGNAL, PANIC_ALIAS_OUTPUT},
      {PANIC_ALIAS_MODULE, PANIC_ALIAS_SOURCE},
      {PANIC_ALIAS_MESSAGE, PANIC_ALIAS_SOURCE},
      {PANIC_ALIAS_SIGNAL, PANIC_ALIAS_SOURCE},
      {PANIC_ALIAS_SOURCE, PANIC_ALIAS_RESULT},
      {PANIC_ALIAS_MESSAGE, PANIC_ALIAS_MODULE},
      {PANIC_ALIAS_SIGNAL, PANIC_ALIAS_MODULE},
      {PANIC_ALIAS_MODULE, PANIC_ALIAS_RESULT},
      {PANIC_ALIAS_SIGNAL, PANIC_ALIAS_MESSAGE},
      {PANIC_ALIAS_MESSAGE, PANIC_ALIAS_RESULT},
      {PANIC_ALIAS_SIGNAL, PANIC_ALIAS_RESULT},
  };
  for (size_t pair_index = 0u;
       pair_index < sizeof(PANIC_ALIAS_PAIRS) / sizeof(PANIC_ALIAS_PAIRS[0]);
       pair_index += 1u) {
    uint8_t alias_source_bytes[128];
    uint8_t alias_module_bytes[128];
    uint8_t alias_message_bytes[128];
    w_seed_parallel_platform1_completion alias_completions[2];
    w_seed_parallel_platform1_receipt alias_receipt;
    w_seed_parallel_provider0_kind alias_provider_kind;
    w_seed_parallel_panic_binding1_signal pair_alias_signal;
    w_seed_parallel_panic_binding1_result pair_alias_result;
    (void)memset(alias_source_bytes, 0x31, sizeof(alias_source_bytes));
    (void)memset(alias_module_bytes, 0x32, sizeof(alias_module_bytes));
    (void)memset(alias_message_bytes, 0x33, sizeof(alias_message_bytes));
    (void)memset(alias_completions, 0x34, sizeof(alias_completions));
    (void)memset(&alias_receipt, 0x35, sizeof(alias_receipt));
    (void)memset(&alias_provider_kind, 0x36, sizeof(alias_provider_kind));
    (void)memset(&pair_alias_signal, 0x37, sizeof(pair_alias_signal));
    (void)memset(&pair_alias_result, 0x38, sizeof(pair_alias_result));
    w_seed_parallel_panic_binding1_workspace alias_workspace = {
        alias_completions, 2u, &alias_receipt, &alias_provider_kind};
    w_seed_parallel_panic_binding1_output pair_alias_output = {
        alias_source_bytes, sizeof(alias_source_bytes), alias_module_bytes,
        sizeof(alias_module_bytes), alias_message_bytes,
        sizeof(alias_message_bytes), &pair_alias_signal};
    uint8_t source_snapshot[sizeof(alias_source_bytes)];
    uint8_t module_snapshot[sizeof(alias_module_bytes)];
    uint8_t message_snapshot[sizeof(alias_message_bytes)];
    w_seed_parallel_platform1_completion completion_snapshot[2];
    w_seed_parallel_platform1_receipt receipt_snapshot;
    w_seed_parallel_provider0_kind provider_kind_snapshot;
    w_seed_parallel_panic_binding1_signal signal_snapshot;
    w_seed_parallel_panic_binding1_result result_snapshot;
    (void)memcpy(source_snapshot, alias_source_bytes,
                 sizeof(source_snapshot));
    (void)memcpy(module_snapshot, alias_module_bytes,
                 sizeof(module_snapshot));
    (void)memcpy(message_snapshot, alias_message_bytes,
                 sizeof(message_snapshot));
    (void)memcpy(completion_snapshot, alias_completions,
                 sizeof(completion_snapshot));
    receipt_snapshot = alias_receipt;
    provider_kind_snapshot = alias_provider_kind;
    signal_snapshot = pair_alias_signal;
    result_snapshot = pair_alias_result;

    const panic_alias_pair pair = PANIC_ALIAS_PAIRS[pair_index];
    w_seed_parallel_panic_binding1_workspace *workspace_argument =
        &alias_workspace;
    const w_seed_parallel_panic_binding1_output *output_argument =
        &pair_alias_output;
    w_seed_parallel_panic_binding1_result *result_argument =
        &pair_alias_result;
    void *target = NULL;
    switch (pair.target_slot) {
      case PANIC_ALIAS_WORKSPACE:
        target = (void *)&alias_workspace;
        break;
      case PANIC_ALIAS_COMPLETIONS:
        target = (void *)alias_workspace.completions;
        break;
      case PANIC_ALIAS_RECEIPT:
        target = (void *)alias_workspace.receipt;
        break;
      case PANIC_ALIAS_PROVIDER_KIND:
        target = (void *)alias_workspace.provider_kind;
        break;
      case PANIC_ALIAS_OUTPUT:
        target = (void *)&pair_alias_output;
        break;
      case PANIC_ALIAS_SOURCE:
        target = (void *)pair_alias_output.source_id_bytes;
        break;
      case PANIC_ALIAS_MODULE:
        target = (void *)pair_alias_output.module_id_bytes;
        break;
      case PANIC_ALIAS_MESSAGE:
        target = (void *)pair_alias_output.message_bytes;
        break;
      case PANIC_ALIAS_SIGNAL:
        target = (void *)pair_alias_output.signal;
        break;
      case PANIC_ALIAS_RESULT:
        target = (void *)&pair_alias_result;
        break;
      default:
        CHECK(false);
    }
    switch (pair.pointer_slot) {
      case PANIC_ALIAS_COMPLETIONS:
        alias_workspace.completions =
            (w_seed_parallel_platform1_completion *)target;
        break;
      case PANIC_ALIAS_RECEIPT:
        alias_workspace.receipt =
            (w_seed_parallel_platform1_receipt *)target;
        break;
      case PANIC_ALIAS_PROVIDER_KIND:
        alias_workspace.provider_kind =
            (w_seed_parallel_provider0_kind *)target;
        break;
      case PANIC_ALIAS_SOURCE:
        pair_alias_output.source_id_bytes = (uint8_t *)target;
        break;
      case PANIC_ALIAS_MODULE:
        pair_alias_output.module_id_bytes = (uint8_t *)target;
        break;
      case PANIC_ALIAS_MESSAGE:
        pair_alias_output.message_bytes = (uint8_t *)target;
        break;
      case PANIC_ALIAS_SIGNAL:
        pair_alias_output.signal =
            (w_seed_parallel_panic_binding1_signal *)target;
        break;
      default:
        CHECK(false);
    }
    CHECK(w_seed_parallel_panic_binding1_run(
              &input, workspace_argument, output_argument, result_argument) ==
              W_SEED_PARALLEL_PANIC_BINDING1_ALIAS &&
          memcmp(alias_source_bytes, source_snapshot,
                 sizeof(alias_source_bytes)) == 0 &&
          memcmp(alias_module_bytes, module_snapshot,
                 sizeof(alias_module_bytes)) == 0 &&
          memcmp(alias_message_bytes, message_snapshot,
                 sizeof(alias_message_bytes)) == 0 &&
          memcmp(alias_completions, completion_snapshot,
                 sizeof(alias_completions)) == 0 &&
          memcmp(&alias_receipt, &receipt_snapshot, sizeof(alias_receipt)) ==
              0 &&
          memcmp(&alias_provider_kind, &provider_kind_snapshot,
                 sizeof(alias_provider_kind)) == 0 &&
          memcmp(&pair_alias_signal, &signal_snapshot,
                 sizeof(pair_alias_signal)) == 0 &&
          memcmp(&pair_alias_result, &result_snapshot,
                 sizeof(pair_alias_result)) == 0);
  }

  typedef struct {
    const void *pointer;
    size_t bytes;
  } panic_input_alias_target;
  const panic_input_alias_target PANIC_INPUT_ALIAS_TARGETS[] = {
      {&input, sizeof(input)},
      {&fixture.hir_program, sizeof(fixture.hir_program)},
      {fixture.hir_modules, sizeof(fixture.hir_modules)},
      {&fixture.hir_result, sizeof(fixture.hir_result)},
      {&selection, sizeof(selection)},
      {selection_tasks, sizeof(selection_tasks)},
      {&selection_result, sizeof(selection_result)},
      {&invocation, sizeof(invocation)},
      {invocation_tasks, sizeof(invocation_tasks)},
      {invocation_arguments, sizeof(invocation_arguments)},
      {&invocation_result, sizeof(invocation_result)},
      {&authority, sizeof(authority)},
  };
  for (size_t target_index = 0u;
       target_index < sizeof(PANIC_INPUT_ALIAS_TARGETS) /
                          sizeof(PANIC_INPUT_ALIAS_TARGETS[0]);
       target_index += 1u) {
    uint8_t alias_source_bytes[128];
    uint8_t alias_module_bytes[128];
    uint8_t alias_message_bytes[128];
    w_seed_parallel_platform1_completion alias_completions[2];
    w_seed_parallel_platform1_receipt alias_receipt;
    w_seed_parallel_provider0_kind alias_provider_kind;
    w_seed_parallel_panic_binding1_signal input_alias_signal;
    w_seed_parallel_panic_binding1_result input_alias_result;
    (void)memset(alias_source_bytes, 0x41, sizeof(alias_source_bytes));
    (void)memset(alias_module_bytes, 0x42, sizeof(alias_module_bytes));
    (void)memset(alias_message_bytes, 0x43, sizeof(alias_message_bytes));
    (void)memset(alias_completions, 0x44, sizeof(alias_completions));
    (void)memset(&alias_receipt, 0x45, sizeof(alias_receipt));
    (void)memset(&alias_provider_kind, 0x46, sizeof(alias_provider_kind));
    (void)memset(&input_alias_signal, 0x47, sizeof(input_alias_signal));
    (void)memset(&input_alias_result, 0x48, sizeof(input_alias_result));
    w_seed_parallel_panic_binding1_workspace alias_workspace = {
        alias_completions, 2u, &alias_receipt, &alias_provider_kind};
    w_seed_parallel_panic_binding1_output input_alias_output = {
        alias_source_bytes, sizeof(alias_source_bytes), alias_module_bytes,
        sizeof(alias_module_bytes), alias_message_bytes,
        sizeof(alias_message_bytes), &input_alias_signal};
    uint8_t source_snapshot[sizeof(alias_source_bytes)];
    uint8_t module_snapshot[sizeof(alias_module_bytes)];
    uint8_t message_snapshot[sizeof(alias_message_bytes)];
    (void)memcpy(source_snapshot, alias_source_bytes,
                 sizeof(source_snapshot));
    (void)memcpy(module_snapshot, alias_module_bytes,
                 sizeof(module_snapshot));
    (void)memcpy(message_snapshot, alias_message_bytes,
                 sizeof(message_snapshot));
    const w_seed_parallel_panic_binding1_signal signal_snapshot =
        input_alias_signal;
    const w_seed_parallel_panic_binding1_result result_snapshot =
        input_alias_result;
    input_alias_output.message_bytes = (uint8_t *)(void *)PANIC_INPUT_ALIAS_TARGETS[
        target_index]
                                                   .pointer;
    input_alias_output.message_capacity = required.message_bytes;
    CHECK(PANIC_INPUT_ALIAS_TARGETS[target_index].bytes >=
              required.message_bytes &&
          w_seed_parallel_panic_binding1_run(
              &input, &alias_workspace, &input_alias_output,
              &input_alias_result) ==
              W_SEED_PARALLEL_PANIC_BINDING1_ALIAS &&
          memcmp(alias_source_bytes, source_snapshot,
                 sizeof(alias_source_bytes)) == 0 &&
          memcmp(alias_module_bytes, module_snapshot,
                 sizeof(alias_module_bytes)) == 0 &&
          memcmp(alias_message_bytes, message_snapshot,
                 sizeof(alias_message_bytes)) == 0 &&
          memcmp(&input_alias_signal, &signal_snapshot,
                 sizeof(input_alias_signal)) == 0 &&
          memcmp(&input_alias_result, &result_snapshot,
                 sizeof(input_alias_result)) == 0);
  }

  static const char TWO_PANICS[] =
      "fn firstPanic(): i64 { panic(\"first panic\") }\n"
      "fn secondPanic(): i64 { panic(\"second panic\") }\n"
      "entry { let firstTask = spawn<.domain> firstPanic() "
      "let secondTask = spawn<.domain> secondPanic() "
      "let first = await firstTask let second = await secondTask }\n";
  CHECK(lower_parallel_domain(TWO_PANICS));
  w_seed_parallel_selection1_counts two_selection_counts;
  w_seed_parallel_selection1_result two_selection_result;
  CHECK(w_seed_parallel_selection1_measure(
            &fixture.hir_program, &fixture.hir_result, &two_selection_counts,
            &two_selection_result) == W_SEED_PARALLEL_SELECTION1_OK &&
        two_selection_counts.tasks == 2u);
  w_seed_parallel_selection1_task two_selection_tasks[2];
  const w_seed_parallel_selection1_output two_selection_output = {
      two_selection_tasks, 2u};
  CHECK(w_seed_parallel_selection1_run(
            &fixture.hir_program, &fixture.hir_result, &two_selection_output,
            &two_selection_result) == W_SEED_PARALLEL_SELECTION1_OK);
  w_seed_parallel_selection1_program two_selection;
  CHECK(w_seed_parallel_selection1_program_from_output(
            &two_selection_output, &two_selection_result, &two_selection) &&
        w_seed_parallel_selection1_verify(
            &fixture.hir_program, &fixture.hir_result, &two_selection,
            &two_selection_result));
  w_seed_parallel_invocation1_counts two_invocation_counts;
  w_seed_parallel_invocation1_result two_invocation_result;
  CHECK(w_seed_parallel_invocation1_measure(
            &fixture.hir_program, &fixture.hir_result, &two_selection,
            &two_selection_result, &two_invocation_counts,
            &two_invocation_result) == W_SEED_PARALLEL_INVOCATION1_OK &&
        two_invocation_counts.tasks == 2u &&
        two_invocation_counts.arguments == 0u);
  w_seed_parallel_invocation1_task two_invocation_tasks[2];
  const w_seed_parallel_invocation1_output two_invocation_output = {
      two_invocation_tasks, 2u, NULL, 0u};
  CHECK(w_seed_parallel_invocation1_run(
            &fixture.hir_program, &fixture.hir_result, &two_selection,
            &two_selection_result, &two_invocation_output,
            &two_invocation_result) == W_SEED_PARALLEL_INVOCATION1_OK);
  w_seed_parallel_invocation1_program two_invocation;
  CHECK(w_seed_parallel_invocation1_program_from_output(
            &two_invocation_output, &two_invocation_result, &two_invocation) &&
        w_seed_parallel_invocation1_verify(
            &fixture.hir_program, &fixture.hir_result, &two_selection,
            &two_selection_result, &two_invocation,
            &two_invocation_result) &&
        two_invocation.tasks[0].kind ==
            W_SEED_PARALLEL_INVOCATION1_TASK_PANIC &&
        two_invocation.tasks[1].kind ==
            W_SEED_PARALLEL_INVOCATION1_TASK_PANIC);
  const w_seed_parallel_panic_binding1_input two_input = {
      &fixture.hir_program, &fixture.hir_result, &two_selection,
      &two_selection_result, &two_invocation, &two_invocation_result,
      &authority, 1u, 11u};
  w_seed_parallel_panic_binding1_counts two_required;
  w_seed_parallel_panic_binding1_result two_measured;
  CHECK(w_seed_parallel_panic_binding1_measure(
            &two_input, &two_required, &two_measured) ==
            W_SEED_PARALLEL_PANIC_BINDING1_OK &&
        two_measured.task_count == 2u && two_measured.panic_count == 2u &&
        two_measured.source_task_index == 0u);

  uint8_t two_source_one[128];
  uint8_t two_module_one[128];
  uint8_t two_message_one[128];
  w_seed_parallel_panic_binding1_signal two_signal_one;
  w_seed_parallel_platform1_completion two_completions_one[2];
  w_seed_parallel_platform1_receipt two_receipt_one;
  w_seed_parallel_provider0_kind two_kind_one;
  const w_seed_parallel_panic_binding1_workspace two_workspace_one = {
      two_completions_one, 2u, &two_receipt_one, &two_kind_one};
  const w_seed_parallel_panic_binding1_output two_output_one = {
      two_source_one, sizeof(two_source_one), two_module_one,
      sizeof(two_module_one), two_message_one, sizeof(two_message_one),
      &two_signal_one};
  w_seed_parallel_panic_binding1_result two_result_one;
  CHECK(w_seed_parallel_panic_binding1_run(
            &two_input, &two_workspace_one, &two_output_one,
            &two_result_one) == W_SEED_PARALLEL_PANIC_BINDING1_OK &&
        two_result_one.panic_count == 2u &&
        two_result_one.source_task_index == 0u &&
        two_signal_one.source_task_index == 0u &&
        two_signal_one.lexical_index == 0u &&
        two_signal_one.message_byte_count == sizeof("first panic") - 1u &&
        memcmp(two_message_one, "first panic", sizeof("first panic") - 1u) ==
            0 &&
        two_signal_one.platform_receipt.panic_source_index == 0u &&
        two_signal_one.platform_receipt.cancellation_source_index == 0u &&
        two_signal_one.platform_receipt.started_count == 1u &&
        two_signal_one.platform_receipt.settled_count == 1u &&
        two_signal_one.platform_receipt.canceled_before_start_count == 1u &&
        two_signal_one.platform_receipt.maximum_active == 1u &&
        w_seed_parallel_panic_binding1_verify(
            &two_input, &two_workspace_one, &two_output_one,
            &two_result_one));

  uint8_t two_source_two[128];
  uint8_t two_module_two[128];
  uint8_t two_message_two[128];
  w_seed_parallel_panic_binding1_signal two_signal_two;
  w_seed_parallel_platform1_completion two_completions_two[2];
  w_seed_parallel_platform1_receipt two_receipt_two;
  w_seed_parallel_provider0_kind two_kind_two;
  const w_seed_parallel_panic_binding1_workspace two_workspace_two = {
      two_completions_two, 2u, &two_receipt_two, &two_kind_two};
  const w_seed_parallel_panic_binding1_output two_output_two = {
      two_source_two, sizeof(two_source_two), two_module_two,
      sizeof(two_module_two), two_message_two, sizeof(two_message_two),
      &two_signal_two};
  w_seed_parallel_panic_binding1_result two_result_two;
  w_seed_parallel_panic_binding1_input two_input_capacity_two = two_input;
  two_input_capacity_two.provider_capacity = 2u;
  CHECK(w_seed_parallel_panic_binding1_run(
            &two_input_capacity_two, &two_workspace_two, &two_output_two,
            &two_result_two) == W_SEED_PARALLEL_PANIC_BINDING1_OK &&
        two_result_two.panic_count == 2u &&
        two_result_two.source_task_index == 0u &&
        two_signal_two.source_task_index == 0u &&
        two_signal_two.lexical_index == 0u &&
        two_signal_two.platform_receipt.panic_source_index == 0u &&
        two_signal_two.platform_receipt.cancellation_source_index == 0u &&
        two_signal_two.platform_receipt.started_count == 2u &&
        two_signal_two.platform_receipt.settled_count == 2u &&
        two_signal_two.platform_receipt.canceled_before_start_count == 0u &&
        two_signal_two.platform_receipt.maximum_active == 2u &&
        memcmp(two_result_one.semantic_digest, two_result_two.semantic_digest,
               sizeof(two_result_one.semantic_digest)) == 0 &&
        memcmp(two_result_one.provenance_digest,
               two_result_two.provenance_digest,
               sizeof(two_result_one.provenance_digest)) != 0 &&
        memcmp(two_message_one, two_message_two,
               sizeof("first panic") - 1u) == 0 &&
        w_seed_parallel_panic_binding1_verify(
            &two_input_capacity_two, &two_workspace_two, &two_output_two,
            &two_result_two));

  static const char VALUE_ONLY[] =
      "fn prepare(value: i64): i64 { return value + 1 }\n"
      "entry { let pending = spawn<.domain> prepare(value: 20) "
      "let value = await pending }\n";
  CHECK(lower_parallel_domain(VALUE_ONLY));
  w_seed_parallel_selection1_counts no_panic_selection_counts;
  w_seed_parallel_selection1_result no_panic_selection_result;
  CHECK(w_seed_parallel_selection1_measure(
            &fixture.hir_program, &fixture.hir_result,
            &no_panic_selection_counts, &no_panic_selection_result) ==
            W_SEED_PARALLEL_SELECTION1_OK &&
        no_panic_selection_counts.tasks == 1u);
  w_seed_parallel_selection1_task no_panic_selection_tasks[1];
  const w_seed_parallel_selection1_output no_panic_selection_output = {
      no_panic_selection_tasks, 1u};
  CHECK(w_seed_parallel_selection1_run(
            &fixture.hir_program, &fixture.hir_result,
            &no_panic_selection_output, &no_panic_selection_result) ==
        W_SEED_PARALLEL_SELECTION1_OK);
  w_seed_parallel_selection1_program no_panic_selection;
  CHECK(w_seed_parallel_selection1_program_from_output(
            &no_panic_selection_output, &no_panic_selection_result,
            &no_panic_selection) &&
        w_seed_parallel_selection1_verify(
            &fixture.hir_program, &fixture.hir_result, &no_panic_selection,
            &no_panic_selection_result));
  w_seed_parallel_invocation1_counts no_panic_invocation_counts;
  w_seed_parallel_invocation1_result no_panic_invocation_result;
  CHECK(w_seed_parallel_invocation1_measure(
            &fixture.hir_program, &fixture.hir_result, &no_panic_selection,
            &no_panic_selection_result, &no_panic_invocation_counts,
            &no_panic_invocation_result) ==
            W_SEED_PARALLEL_INVOCATION1_OK &&
        no_panic_invocation_counts.tasks == 1u);
  w_seed_parallel_invocation1_task no_panic_invocation_tasks[1];
  w_seed_parallel_invocation1_argument no_panic_invocation_arguments[1];
  const w_seed_parallel_invocation1_output no_panic_invocation_output = {
      no_panic_invocation_tasks, 1u, no_panic_invocation_arguments, 1u};
  CHECK(w_seed_parallel_invocation1_run(
            &fixture.hir_program, &fixture.hir_result, &no_panic_selection,
            &no_panic_selection_result, &no_panic_invocation_output,
            &no_panic_invocation_result) ==
        W_SEED_PARALLEL_INVOCATION1_OK);
  w_seed_parallel_invocation1_program no_panic_invocation;
  CHECK(w_seed_parallel_invocation1_program_from_output(
            &no_panic_invocation_output, &no_panic_invocation_result,
            &no_panic_invocation) &&
        w_seed_parallel_invocation1_verify(
            &fixture.hir_program, &fixture.hir_result, &no_panic_selection,
            &no_panic_selection_result, &no_panic_invocation,
            &no_panic_invocation_result));
  const w_seed_parallel_panic_binding1_input no_panic_input = {
      &fixture.hir_program, &fixture.hir_result, &no_panic_selection,
      &no_panic_selection_result, &no_panic_invocation,
      &no_panic_invocation_result, &authority, 1u, 13u};
  w_seed_parallel_panic_binding1_counts no_panic_counts;
  w_seed_parallel_panic_binding1_result no_panic_result;
  (void)memset(&no_panic_counts, 0x61, sizeof(no_panic_counts));
  (void)memset(&no_panic_result, 0x62, sizeof(no_panic_result));
  const w_seed_parallel_panic_binding1_counts no_panic_counts_before =
      no_panic_counts;
  const w_seed_parallel_panic_binding1_result no_panic_result_before =
      no_panic_result;
  CHECK(w_seed_parallel_panic_binding1_measure(
            &no_panic_input, &no_panic_counts, &no_panic_result) ==
            W_SEED_PARALLEL_PANIC_BINDING1_NO_PANIC &&
        memcmp(&no_panic_counts, &no_panic_counts_before,
               sizeof(no_panic_counts)) == 0 &&
        memcmp(&no_panic_result, &no_panic_result_before,
               sizeof(no_panic_result)) == 0);
  uint8_t no_panic_source[128];
  uint8_t no_panic_module[128];
  uint8_t no_panic_message[128];
  w_seed_parallel_platform1_completion no_panic_completions[1];
  w_seed_parallel_platform1_receipt no_panic_receipt;
  w_seed_parallel_provider0_kind no_panic_kind;
  w_seed_parallel_panic_binding1_signal no_panic_signal;
  (void)memset(no_panic_source, 0x63, sizeof(no_panic_source));
  (void)memset(no_panic_module, 0x64, sizeof(no_panic_module));
  (void)memset(no_panic_message, 0x65, sizeof(no_panic_message));
  (void)memset(no_panic_completions, 0x66, sizeof(no_panic_completions));
  (void)memset(&no_panic_receipt, 0x67, sizeof(no_panic_receipt));
  (void)memset(&no_panic_kind, 0x68, sizeof(no_panic_kind));
  (void)memset(&no_panic_signal, 0x69, sizeof(no_panic_signal));
  w_seed_parallel_panic_binding1_workspace no_panic_workspace = {
      no_panic_completions, 1u, &no_panic_receipt, &no_panic_kind};
  const w_seed_parallel_panic_binding1_output no_panic_output = {
      no_panic_source, sizeof(no_panic_source), no_panic_module,
      sizeof(no_panic_module), no_panic_message, sizeof(no_panic_message),
      &no_panic_signal};
  w_seed_parallel_panic_binding1_result no_panic_run_result;
  (void)memset(&no_panic_run_result, 0x6a, sizeof(no_panic_run_result));
  uint8_t no_panic_source_before[sizeof(no_panic_source)];
  uint8_t no_panic_module_before[sizeof(no_panic_module)];
  uint8_t no_panic_message_before[sizeof(no_panic_message)];
  (void)memcpy(no_panic_source_before, no_panic_source,
               sizeof(no_panic_source));
  (void)memcpy(no_panic_module_before, no_panic_module,
               sizeof(no_panic_module));
  (void)memcpy(no_panic_message_before, no_panic_message,
               sizeof(no_panic_message));
  const w_seed_parallel_panic_binding1_signal no_panic_signal_before =
      no_panic_signal;
  const w_seed_parallel_panic_binding1_result no_panic_run_before =
      no_panic_run_result;
  const w_seed_parallel_platform1_completion no_panic_completion_before =
      no_panic_completions[0];
  const w_seed_parallel_platform1_receipt no_panic_receipt_before =
      no_panic_receipt;
  const w_seed_parallel_provider0_kind no_panic_kind_before = no_panic_kind;
  CHECK(w_seed_parallel_panic_binding1_run(
            &no_panic_input, &no_panic_workspace, &no_panic_output,
            &no_panic_run_result) == W_SEED_PARALLEL_PANIC_BINDING1_NO_PANIC &&
        memcmp(no_panic_source, no_panic_source_before,
               sizeof(no_panic_source)) == 0 &&
        memcmp(no_panic_module, no_panic_module_before,
               sizeof(no_panic_module)) == 0 &&
        memcmp(no_panic_message, no_panic_message_before,
               sizeof(no_panic_message)) == 0 &&
        memcmp(&no_panic_signal, &no_panic_signal_before,
               sizeof(no_panic_signal)) == 0 &&
        memcmp(&no_panic_run_result, &no_panic_run_before,
               sizeof(no_panic_run_result)) == 0 &&
        memcmp(&no_panic_completions[0], &no_panic_completion_before,
               sizeof(no_panic_completion_before)) == 0 &&
        memcmp(&no_panic_receipt, &no_panic_receipt_before,
               sizeof(no_panic_receipt)) == 0 &&
        memcmp(&no_panic_kind, &no_panic_kind_before,
               sizeof(no_panic_kind)) == 0);

  uint8_t teardown_source[128];
  uint8_t teardown_module[128];
  uint8_t teardown_message[128];
  (void)memcpy(teardown_source, source_id, sizeof(teardown_source));
  (void)memcpy(teardown_module, module_id, sizeof(teardown_module));
  (void)memcpy(teardown_message, message, sizeof(teardown_message));
  (void)memset(fixture.hir_text, 0, fixture.hir_program.text_byte_count);
  (void)memset(fixture.hir_value_bytes, 0,
               fixture.hir_program.value_byte_count);
  CHECK(signal.source_id_bytes == source_id &&
        signal.module_id_bytes == module_id && signal.message_bytes == message &&
        memcmp(source_id, teardown_source, sizeof(teardown_source)) == 0 &&
        memcmp(module_id, teardown_module, sizeof(teardown_module)) == 0 &&
        memcmp(message, teardown_message, sizeof(teardown_message)) == 0);
  return true;
#endif
}

static bool test_process_parallel_composition_hir(void) {
  static const char SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "fn select(missing: Bool): i64 { return if missing { 0 } else { 40 } }\n"
      "fn addOne(value: i64): i64 { return value + 1 }\n"
      "fn increment(value: i64): i64 { return addOne(value: value) }\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { let seed = select(missing: args.isEmpty) "
      "let pending = spawn<.domain> increment(value: seed) "
      "let value = await pending "
      "return .success }\nentry(run)\n";
  CHECK(lower_process_parallel(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  size_t dispatches = 0u;
  size_t root_direct_calls = 0u;
  size_t launch_bindings = 0u;
  size_t join_bindings = 0u;
  const w_seed_hir0_entry *entry = &program->entries[0];
  const w_seed_hir0_function *root = &program->functions[entry->target_function];
  for (size_t index = 0u; index < program->call_count; index += 1u)
    if (program->calls[index].execution_kind ==
        W_SEED_HIR0_CALL_STRUCTURED_ASYNC_PARALLEL_DOMAIN_DISPATCH) {
      const w_seed_hir0_call *call = &program->calls[index];
      CHECK(call->owner_block == root->first_block &&
            call->argument_count == 1u &&
            call->first_argument < program->argument_count);
      const w_seed_hir0_argument *argument =
          &program->arguments[call->first_argument];
      CHECK(argument->value_index < program->value_count &&
            program->values[argument->value_index].kind ==
                W_SEED_HIR0_VALUE_BINDING_READ);
      dispatches += 1u;
    } else if (program->calls[index].execution_kind == W_SEED_HIR0_CALL_DIRECT &&
               program->calls[index].owner_block == root->first_block) {
      const w_seed_hir0_call *call = &program->calls[index];
      CHECK(call->argument_count == 1u &&
            call->first_argument < program->argument_count);
      const w_seed_hir0_argument *argument =
          &program->arguments[call->first_argument];
      CHECK(argument->value_index < program->value_count &&
            program->values[argument->value_index].kind ==
                W_SEED_HIR0_VALUE_EXTERNAL_MEMBER &&
            program->values[argument->value_index].external_symbol_index == 4u);
      root_direct_calls += 1u;
    }
  for (size_t index = 0u; index < program->binding_count; index += 1u) {
    if (program->bindings[index].owner_block != root->first_block) continue;
    if (program->bindings[index].task_role == W_SEED_HIR0_TASK_ROLE_LAUNCH)
      launch_bindings += 1u;
    if (program->bindings[index].task_role ==
        W_SEED_HIR0_TASK_ROLE_AWAIT_RESULT)
      join_bindings += 1u;
  }
  CHECK(dispatches == 1u && program->entry_count == 1u &&
        root_direct_calls == 1u && launch_bindings == 1u &&
        join_bindings == 1u && entry->adapter_kind ==
            W_SEED_HIR0_ENTRY_ADAPTER_NATIVE_PROCESS);
  w_seed_parallel_selection0 selection;
  CHECK(w_seed_parallel_selection0_select(program, &fixture.hir_result,
                                          &selection) ==
            W_SEED_PARALLEL_SELECTION0_OK &&
        selection.root_function_index == entry->target_function &&
        selection.task_count == 1u &&
        w_seed_parallel_selection0_verify(program, &fixture.hir_result,
                                          &selection));

  /* This certificate proves only target-neutral transformation legality.
   * Physical launch/join remains the reference route; a target optimizer must
   * separately prove domain observability and profitability before eliding it.
   */
  w_seed_parallel_elision0_certificate elision;
  (void)memset(&elision, 0xa6, sizeof(elision));
  CHECK(w_seed_parallel_elision0_certify(program, &fixture.hir_result,
                                         &selection, &elision) ==
            W_SEED_PARALLEL_ELISION0_OK &&
        elision.root_function_index == entry->target_function &&
        elision.task_call_index == selection.task_call_indices[0] &&
        elision.task_function_index == selection.task_function_indices[0] &&
        elision.launch_binding_index == selection.launch_binding_indices[0] &&
        elision.join_binding_index == selection.join_binding_indices[0] &&
        elision.join_instruction_index ==
            elision.launch_instruction_index + 1u &&
        elision.reachable_function_count == 2u &&
        elision.proof_facts == W_SEED_PARALLEL_ELISION0_REQUIRED_FACTS &&
        w_seed_parallel_elision0_verify(program, &fixture.hir_result,
                                         &selection, &elision));
  w_seed_parallel_elision0_certificate forged_elision = elision;
  forged_elision.proof_facts &=
      ~(uint32_t)W_SEED_PARALLEL_ELISION0_FACT_IMMEDIATE_JOIN;
  CHECK(!w_seed_parallel_elision0_verify(program, &fixture.hir_result,
                                          &selection, &forged_elision));
  forged_elision = elision;
  forged_elision.task_call_index ^= 1u;
  CHECK(!w_seed_parallel_elision0_verify(program, &fixture.hir_result,
                                          &selection, &forged_elision));

  w_seed_parallel_elision0_certificate elision_sentinel;
  (void)memset(&elision_sentinel, 0x7d, sizeof(elision_sentinel));
  const w_seed_parallel_elision0_certificate elision_before =
      elision_sentinel;
  const w_seed_parallel_selection0 selection_before = selection;
  CHECK(w_seed_parallel_elision0_certify(
            program, &fixture.hir_result, &selection,
            (w_seed_parallel_elision0_certificate *)(void *)&selection) ==
            W_SEED_PARALLEL_ELISION0_INVALID &&
        memcmp(&selection, &selection_before, sizeof(selection)) == 0 &&
        w_seed_parallel_elision0_certify(
            NULL, &fixture.hir_result, &selection, &elision_sentinel) ==
            W_SEED_PARALLEL_ELISION0_INVALID &&
        memcmp(&elision_sentinel, &elision_before,
               sizeof(elision_sentinel)) == 0);

  static const char TWO_PRELUDES[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "fn select(missing: Bool): i64 { return if missing { 0 } else { 40 } }\n"
      "fn increment(value: i64): i64 { return value + 1 }\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { let first = select(missing: args.isEmpty) "
      "let second = select(missing: args.isEmpty) "
      "let pending = spawn<.domain> increment(value: second) "
      "let value = await pending return .success }\nentry(run)\n";
  static const char EFFECTFUL_PRELUDE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "fn select(missing: Bool): i64 { print(\"effect\") return 40 }\n"
      "fn increment(value: i64): i64 { return value + 1 }\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { let seed = select(missing: args.isEmpty) "
      "let pending = spawn<.domain> increment(value: seed) "
      "let value = await pending return .success }\nentry(run)\n";
  static const char ROOT_EFFECT[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "fn select(missing: Bool): i64 { return if missing { 0 } else { 40 } }\n"
      "fn increment(value: i64): i64 { return value + 1 }\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { let seed = select(missing: args.isEmpty) "
      "let pending = spawn<.domain> increment(value: seed) "
      "let value = await pending print(\"effect\") return .success }\n"
      "entry(run)\n";
  static const char *const REJECTED[] = {TWO_PRELUDES, EFFECTFUL_PRELUDE,
                                         ROOT_EFFECT};
  for (size_t rejected = 0u;
       rejected < sizeof(REJECTED) / sizeof(REJECTED[0]); rejected += 1u) {
    CHECK(fixture_process_parallel_frontend(REJECTED[rejected]));
    setup_hir_output();
    const w_seed_hir0_input input = {
        .frontend_input = &fixture.input,
        .frontend_output = &fixture.output,
        .frontend_result = &fixture.result,
        .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
    w_seed_hir0_counts measured;
    w_seed_hir0_result measured_result;
    CHECK(w_seed_hir0_measure(&input, &measured, &measured_result) ==
          W_SEED_HIR0_UNSUPPORTED);
  }
  return true;
}

static const char PROCESS_PARALLEL_MLIR_SOURCE[] =
    "import { Arguments as ProcessArguments, Context as ProcessContext, "
    "ExitCode as ProcessExitCode } from std.process\n"
    "fn select(missing: Bool): i64 { return if missing { 0 } else { 40 } }\n"
    "fn addOne(value: i64): i64 { return value + 1 }\n"
    "fn increment(value: i64): i64 { return addOne(value: value) }\n"
    "async fn run(args: ProcessArguments, ctx: ProcessContext): "
    "ProcessExitCode { let seed = select(missing: args.isEmpty) "
    "let pending = spawn<.domain> increment(value: seed) "
    "let value = await pending "
    "return .success }\nentry(run)\n";

static bool test_process_parallel_mlir(void) {
  CHECK(lower_process_parallel(PROCESS_PARALLEL_MLIR_SOURCE));
  w_seed_parallel_selection0 selection;
  CHECK(w_seed_parallel_selection0_select(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
            W_SEED_PARALLEL_SELECTION0_OK &&
        selection.task_count == 1u &&
        w_seed_parallel_selection0_verify(&fixture.hir_program,
                                          &fixture.hir_result, &selection));

  const w_seed_mlir0_target linux_target = {
      W_SEED_MLIR0_TARGET_X86_64_UNKNOWN_LINUX_GNU};
  w_seed_mlir0_process_parallel_counts counts;
  w_seed_mlir0_process_parallel_result measured;
  (void)memset(&counts, 0x3a, sizeof(counts));
  (void)memset(&measured, 0x3b, sizeof(measured));
  const w_seed_mlir0_status process_parallel_status =
      w_seed_mlir0_measure_process_parallel(
            &fixture.hir_program, &fixture.hir_result, &selection,
            &linux_target, &counts, &measured);
  CHECK(process_parallel_status == W_SEED_MLIR0_OK &&
        counts.mlir_bytes != 0u && counts.mlir_bytes < W_SEED_MLIR0_MAX_BYTES &&
        counts.task_count == 1u && counts.root_function_index ==
            selection.root_function_index &&
        counts.runtime_argument_count == 1u && counts.reachable_function_count ==
            4u && counts.launch_count == 1u && counts.join_count == 1u &&
        measured.required.mlir_bytes == counts.mlir_bytes &&
        measured.written.mlir_bytes == 0u);

  static uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  (void)memset(artifact, 0xa7, sizeof(artifact));
  const w_seed_mlir0_process_parallel_result before = measured;
  w_seed_mlir0_process_parallel_output output = {artifact, sizeof(artifact)};
  CHECK(w_seed_mlir0_emit_process_parallel(
            &fixture.hir_program, &fixture.hir_result, &selection,
            &linux_target, &output, &measured) == W_SEED_MLIR0_OK &&
        measured.written.mlir_bytes == counts.mlir_bytes &&
        bytes_contain(artifact, counts.mlir_bytes,
                      W_SEED_MLIR0_PROCESS_PARALLEL_SCHEMA_VERSION) &&
        bytes_contain(artifact, counts.mlir_bytes,
                      "llvm.call @w_seed_process_arguments_is_empty(%p0") &&
        bytes_contain(artifact, counts.mlir_bytes,
                      "llvm.call @w_seed_parallel_launch_task_0(%parallel_frame") &&
        bytes_contain(artifact, counts.mlir_bytes,
                      "llvm.call @w_seed_parallel_join_task_0(%parallel_frame") &&
        bytes_contain(artifact, counts.mlir_bytes,
                      "%parallel_frame_slots = llvm.mlir.constant(4 : i64)") &&
        bytes_contain(artifact, counts.mlir_bytes,
                      "%parallel_frame = llvm.alloca %parallel_frame_slots x i64") &&
        bytes_contain(artifact, counts.mlir_bytes, "@w_seed_parallel_task_0") &&
        !bytes_contain(artifact, counts.mlir_bytes,
                       "@w_seed_parallel_launch_task_0(%parallel_frame, 40") &&
        w_seed_mlir0_verify_process_parallel(
            &fixture.hir_program, &fixture.hir_result, &selection,
            &linux_target, artifact, counts.mlir_bytes, &measured));

  uint8_t forged_byte = artifact[0];
  artifact[0] ^= 1u;
  CHECK(!w_seed_mlir0_verify_process_parallel(
      &fixture.hir_program, &fixture.hir_result, &selection, &linux_target,
      artifact, counts.mlir_bytes, &measured));
  artifact[0] = forged_byte;
  w_seed_mlir0_process_parallel_result forged = measured;
  forged.mlir_sha256[0] ^= 1u;
  CHECK(!w_seed_mlir0_verify_process_parallel(
      &fixture.hir_program, &fixture.hir_result, &selection, &linux_target,
      artifact, counts.mlir_bytes, &forged));

  w_seed_mlir0_process_parallel_result sentinel = measured;
  const w_seed_mlir0_process_parallel_result sentinel_before = sentinel;
  static uint8_t short_artifact_before[W_SEED_MLIR0_MAX_BYTES];
  (void)memcpy(short_artifact_before, artifact, sizeof(short_artifact_before));
  w_seed_mlir0_process_parallel_output short_output = {artifact,
                                                       counts.mlir_bytes - 1u};
  CHECK(w_seed_mlir0_emit_process_parallel(
            &fixture.hir_program, &fixture.hir_result, &selection,
            &linux_target, &short_output, &sentinel) ==
            W_SEED_MLIR0_CAPACITY &&
        memcmp(&sentinel, &sentinel_before, sizeof(sentinel)) == 0 &&
        memcmp(artifact, short_artifact_before,
               sizeof(short_artifact_before)) == 0);
  CHECK(memcmp(&measured, &before, sizeof(before)) != 0);

  enum process_parallel_alias_case {
    PROCESS_PARALLEL_BYTES_RESULT,
    PROCESS_PARALLEL_BYTES_SELECTION,
    PROCESS_PARALLEL_BYTES_HIR_RESULT,
    PROCESS_PARALLEL_BYTES_HIR_TEXT,
    PROCESS_PARALLEL_DESCRIPTOR_RESULT,
    PROCESS_PARALLEL_DESCRIPTOR_SELECTION,
    PROCESS_PARALLEL_DESCRIPTOR_HIR_RESULT,
    PROCESS_PARALLEL_DESCRIPTOR_HIR_TEXT,
    PROCESS_PARALLEL_RESULT_SELECTION,
    PROCESS_PARALLEL_RESULT_HIR_RESULT,
    PROCESS_PARALLEL_RESULT_HIR_TEXT,
    PROCESS_PARALLEL_RESULT_DESCRIPTOR,
  };
  static const enum process_parallel_alias_case ALIAS_CASES[] = {
      PROCESS_PARALLEL_BYTES_RESULT,
      PROCESS_PARALLEL_BYTES_SELECTION,
      PROCESS_PARALLEL_BYTES_HIR_RESULT,
      PROCESS_PARALLEL_BYTES_HIR_TEXT,
      PROCESS_PARALLEL_DESCRIPTOR_RESULT,
      PROCESS_PARALLEL_DESCRIPTOR_SELECTION,
      PROCESS_PARALLEL_DESCRIPTOR_HIR_RESULT,
      PROCESS_PARALLEL_DESCRIPTOR_HIR_TEXT,
      PROCESS_PARALLEL_RESULT_SELECTION,
      PROCESS_PARALLEL_RESULT_HIR_RESULT,
      PROCESS_PARALLEL_RESULT_HIR_TEXT,
      PROCESS_PARALLEL_RESULT_DESCRIPTOR};
  for (size_t alias_case = 0u;
       alias_case < sizeof(ALIAS_CASES) / sizeof(ALIAS_CASES[0]);
       alias_case += 1u) {
    static uint8_t alias_artifact[W_SEED_MLIR0_MAX_BYTES];
    static uint8_t alias_artifact_before[W_SEED_MLIR0_MAX_BYTES];
    static uint8_t alias_hir_text_before[TEST_HIR_TEXT];
    (void)memset(alias_artifact, 0xc1, sizeof(alias_artifact));
    (void)memcpy(alias_artifact_before, alias_artifact,
                 sizeof(alias_artifact_before));
    (void)memcpy(alias_hir_text_before, fixture.hir_text,
                 sizeof(alias_hir_text_before));
    const w_seed_parallel_selection0 selection_before = selection;
    const w_seed_hir0_result hir_result_before = fixture.hir_result;
    w_seed_mlir0_process_parallel_result alias_result;
    (void)memset(&alias_result, 0xc2, sizeof(alias_result));
    const w_seed_mlir0_process_parallel_result alias_result_before =
        alias_result;
    w_seed_mlir0_process_parallel_output alias_output = {
        alias_artifact, sizeof(alias_artifact)};
    const w_seed_mlir0_process_parallel_output *output_pointer =
        &alias_output;
    w_seed_mlir0_process_parallel_result *result_pointer = &alias_result;
    switch (ALIAS_CASES[alias_case]) {
      case PROCESS_PARALLEL_BYTES_RESULT:
        alias_output.bytes = (uint8_t *)(void *)&alias_result;
        break;
      case PROCESS_PARALLEL_BYTES_SELECTION:
        alias_output.bytes = (uint8_t *)(void *)&selection;
        break;
      case PROCESS_PARALLEL_BYTES_HIR_RESULT:
        alias_output.bytes = (uint8_t *)(void *)&fixture.hir_result;
        break;
      case PROCESS_PARALLEL_BYTES_HIR_TEXT:
        alias_output.bytes = fixture.hir_text;
        break;
      case PROCESS_PARALLEL_DESCRIPTOR_RESULT:
        output_pointer =
            (const w_seed_mlir0_process_parallel_output *)(void *)&alias_result;
        break;
      case PROCESS_PARALLEL_DESCRIPTOR_SELECTION:
        output_pointer =
            (const w_seed_mlir0_process_parallel_output *)(void *)&selection;
        break;
      case PROCESS_PARALLEL_DESCRIPTOR_HIR_RESULT:
        output_pointer =
            (const w_seed_mlir0_process_parallel_output *)(void *)&fixture.hir_result;
        break;
      case PROCESS_PARALLEL_DESCRIPTOR_HIR_TEXT:
        output_pointer =
            (const w_seed_mlir0_process_parallel_output *)(void *)fixture.hir_text;
        break;
      case PROCESS_PARALLEL_RESULT_SELECTION:
        result_pointer =
            (w_seed_mlir0_process_parallel_result *)(void *)&selection;
        break;
      case PROCESS_PARALLEL_RESULT_HIR_RESULT:
        result_pointer =
            (w_seed_mlir0_process_parallel_result *)(void *)&fixture.hir_result;
        break;
      case PROCESS_PARALLEL_RESULT_HIR_TEXT:
        result_pointer =
            (w_seed_mlir0_process_parallel_result *)(void *)fixture.hir_text;
        break;
      case PROCESS_PARALLEL_RESULT_DESCRIPTOR:
        result_pointer =
            (w_seed_mlir0_process_parallel_result *)(void *)&alias_output;
        break;
      default:
        return false;
    }
    const w_seed_mlir0_process_parallel_output alias_output_before =
        alias_output;
    CHECK(w_seed_mlir0_emit_process_parallel(
              &fixture.hir_program, &fixture.hir_result, &selection,
              &linux_target, output_pointer, result_pointer) ==
              W_SEED_MLIR0_ALIAS &&
          memcmp(alias_artifact, alias_artifact_before,
                 sizeof(alias_artifact)) == 0 &&
          memcmp(&alias_result, &alias_result_before,
                 sizeof(alias_result)) == 0 &&
          memcmp(&alias_output, &alias_output_before,
                 sizeof(alias_output)) == 0 &&
          memcmp(&selection, &selection_before, sizeof(selection)) == 0 &&
          memcmp(&fixture.hir_result, &hir_result_before,
                 sizeof(fixture.hir_result)) == 0 &&
          memcmp(fixture.hir_text, alias_hir_text_before,
                 sizeof(alias_hir_text_before)) == 0);
  }

  union process_parallel_measure_alias_storage {
    w_seed_mlir0_process_parallel_counts counts;
    w_seed_mlir0_process_parallel_result result;
  } measure_alias;
  (void)memset(&measure_alias, 0xc3, sizeof(measure_alias));
  const union process_parallel_measure_alias_storage measure_alias_before =
      measure_alias;
  CHECK(w_seed_mlir0_measure_process_parallel(
            &fixture.hir_program, &fixture.hir_result, &selection,
            &linux_target, &measure_alias.counts, &measure_alias.result) ==
            W_SEED_MLIR0_ALIAS &&
        memcmp(&measure_alias, &measure_alias_before,
               sizeof(measure_alias)) == 0);

  w_seed_native_subset0_process ordinary_process;
  CHECK(w_seed_native_subset0_select_process_executable(
            &fixture.hir_program, &fixture.hir_result, &ordinary_process) ==
        W_SEED_NATIVE_SUBSET0_UNSUPPORTED);
  return true;
}

static bool test_lowering_is_not_hello_hardcoded(void) {
  static const char OTHER_SOURCE[] =
      "fn main() { print(message: \"north\", suffix: \"!\") }\nentry(main)\n";
  CHECK(lower(OTHER_SOURCE));
  CHECK(fixture.hir_program.values[0].byte_count == 5u);
  CHECK(memcmp(fixture.hir_value_bytes + fixture.hir_program.values[0].byte_offset,
               "north", 5u) == 0);
  return true;
}

static bool test_local_binding_lowering(void) {
  static const char SOURCE[] =
      "fn main() { let message = \"Table 42 remains open\" "
      "print(message: message, suffix: \"!\") }\nentry(main)\n";
  CHECK(lower(SOURCE));
  CHECK(fixture.hir_counts.bindings == 1u);
  CHECK(fixture.hir_counts.instructions == 2u);
  CHECK(fixture.hir_counts.calls == 1u);
  CHECK(fixture.hir_counts.arguments == 2u && fixture.hir_counts.values == 3u);
  CHECK(fixture.hir_program.instructions[0].kind ==
            W_SEED_HIR0_INSTRUCTION_BINDING &&
        fixture.hir_program.instructions[0].call_index == W_SEED_HIR0_NONE &&
        fixture.hir_program.instructions[0].binding_index == 0u &&
        fixture.hir_program.instructions[1].kind ==
            W_SEED_HIR0_INSTRUCTION_CALL &&
        fixture.hir_program.instructions[1].call_index == 0u &&
        fixture.hir_program.instructions[1].binding_index == W_SEED_HIR0_NONE);
  const w_seed_hir0_binding *binding = &fixture.hir_program.bindings[0];
  CHECK(binding->owner_instruction == 0u && binding->owner_block == 0u &&
        binding->ordinal == 0u && binding->type_index == W_SEED_HIR0_TYPE_STRING &&
        !binding->is_mutable && binding->name.count == 7u &&
        memcmp(fixture.hir_text + binding->name.offset, "message", 7u) == 0 &&
        binding->initializer_value == 0u);
  const w_seed_hir0_value *initializer = &fixture.hir_program.values[0];
  CHECK(initializer->kind == W_SEED_HIR0_VALUE_CONST_STRING &&
        initializer->owner_kind == W_SEED_HIR0_VALUE_OWNER_BINDING &&
        initializer->owner_index == 0u && initializer->owner_ordinal == 0u &&
        initializer->byte_offset == 0u &&
        initializer->byte_count == strlen("Table 42 remains open"));
  CHECK(memcmp(fixture.hir_value_bytes + initializer->byte_offset,
               "Table 42 remains open", initializer->byte_count) == 0);
  CHECK(fixture.hir_program.calls[0].owner_instruction == 1u &&
        fixture.hir_program.values[1].kind == W_SEED_HIR0_VALUE_BINDING_READ &&
        fixture.hir_program.values[1].binding_index == 0u &&
        fixture.hir_program.values[1].byte_offset == 0u &&
        fixture.hir_program.values[1].byte_count == 0u &&
        fixture.hir_program.values[2].kind == W_SEED_HIR0_VALUE_CONST_STRING &&
        fixture.hir_program.values[2].binding_index == W_SEED_HIR0_NONE &&
        fixture.hir_program.values[2].byte_offset == initializer->byte_count &&
        fixture.hir_program.values[2].byte_count == 1u);
  (void)memset(&fixture.document, 0, sizeof(fixture.document));
  (void)memset(&fixture.input, 0, sizeof(fixture.input));
  (void)memset(&fixture.output, 0, sizeof(fixture.output));
  (void)memset(&fixture.result, 0, sizeof(fixture.result));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool test_straight_line_mutation_ssa(void) {
  static const char SOURCE[] =
      "entry {\n"
      "  var seats = 5\n"
      "  seats = seats + 1\n"
      "  print(message: \"Open ${seats}\", suffix: \"!\")\n"
      "}\n";
  CHECK(lower(SOURCE));
  CHECK(fixture.hir_program.binding_count == 2u &&
        fixture.hir_program.instruction_count == 3u &&
        fixture.hir_program.call_count == 1u);
  const w_seed_hir0_binding declaration = fixture.hir_bindings[0];
  const w_seed_hir0_binding update = fixture.hir_bindings[1];
  CHECK(declaration.is_mutable && declaration.source_binding == 0u &&
        declaration.previous_version == W_SEED_HIR0_NONE &&
        declaration.next_version == 1u &&
        declaration.type_index == W_SEED_HIR0_TYPE_I64 &&
        declaration.initializer_value < fixture.hir_program.value_count &&
        update.is_mutable && update.source_binding == 0u &&
        update.previous_version == 0u &&
        update.next_version == W_SEED_HIR0_NONE &&
        update.type_index == declaration.type_index &&
        update.initializer_value < fixture.hir_program.value_count &&
        update.owner_instruction == 1u &&
        declaration.name.count == update.name.count &&
        memcmp(fixture.hir_text + declaration.name.offset,
               fixture.hir_text + update.name.offset,
               declaration.name.count) == 0);
  CHECK(fixture.hir_values[declaration.initializer_value].kind ==
            W_SEED_HIR0_VALUE_CONST_I64 &&
        fixture.hir_values[declaration.initializer_value].integer_value == 5);
  const w_seed_hir0_value *replacement =
      &fixture.hir_values[update.initializer_value];
  CHECK(replacement->kind == W_SEED_HIR0_VALUE_BINARY_I64 &&
        fixture.hir_values[replacement->left_value].kind ==
            W_SEED_HIR0_VALUE_BINDING_READ &&
        fixture.hir_values[replacement->left_value].binding_index == 0u &&
        fixture.hir_values[replacement->right_value].kind ==
            W_SEED_HIR0_VALUE_CONST_I64 &&
        fixture.hir_values[replacement->right_value].integer_value == 1);
  bool saw_latest_read = false;
  for (size_t index = 0u; index < fixture.hir_program.value_count; index += 1u)
    if (fixture.hir_values[index].kind == W_SEED_HIR0_VALUE_BINDING_READ &&
        fixture.hir_values[index].binding_index == 1u)
      saw_latest_read = true;
  CHECK(saw_latest_read);

  fixture.hir_bindings[1].source_binding = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_bindings[1] = update;
  reseal_hir_fixture();
  fixture.hir_bindings[0].next_version = W_SEED_HIR0_NONE;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_bindings[0] = declaration;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool test_interleaved_mutation_versions(void) {
  static const char SOURCE[] =
      "entry {\n"
      "  var seats = 5\n"
      "  var tables = 2\n"
      "  seats = seats + 1\n"
      "  tables = tables + 3\n"
      "  seats = seats + tables\n"
      "  print(message: \"Capacity ${seats}\", suffix: \"!\")\n"
      "}\n";
  CHECK(lower(SOURCE));
  CHECK(fixture.hir_program.binding_count == 5u &&
        fixture.hir_program.instruction_count == 6u &&
        fixture.hir_program.call_count == 1u);

  const w_seed_hir0_binding *seats0 = &fixture.hir_bindings[0];
  const w_seed_hir0_binding *tables0 = &fixture.hir_bindings[1];
  const w_seed_hir0_binding *seats1 = &fixture.hir_bindings[2];
  const w_seed_hir0_binding *tables1 = &fixture.hir_bindings[3];
  const w_seed_hir0_binding *seats2 = &fixture.hir_bindings[4];
  CHECK(seats0->source_binding == 0u &&
        seats0->previous_version == W_SEED_HIR0_NONE &&
        seats0->next_version == 2u && tables0->source_binding == 1u &&
        tables0->previous_version == W_SEED_HIR0_NONE &&
        tables0->next_version == 3u && seats1->source_binding == 0u &&
        seats1->previous_version == 0u && seats1->next_version == 4u &&
        tables1->source_binding == 1u && tables1->previous_version == 1u &&
        tables1->next_version == W_SEED_HIR0_NONE &&
        seats2->source_binding == 0u && seats2->previous_version == 2u &&
        seats2->next_version == W_SEED_HIR0_NONE);

  const w_seed_hir0_value *seats_update1 =
      &fixture.hir_values[seats1->initializer_value];
  const w_seed_hir0_value *tables_update1 =
      &fixture.hir_values[tables1->initializer_value];
  const w_seed_hir0_value *seats_update2 =
      &fixture.hir_values[seats2->initializer_value];
  CHECK(seats_update1->kind == W_SEED_HIR0_VALUE_BINARY_I64 &&
        fixture.hir_values[seats_update1->left_value].binding_index == 0u &&
        tables_update1->kind == W_SEED_HIR0_VALUE_BINARY_I64 &&
        fixture.hir_values[tables_update1->left_value].binding_index == 1u &&
        seats_update2->kind == W_SEED_HIR0_VALUE_BINARY_I64 &&
        fixture.hir_values[seats_update2->left_value].binding_index == 2u &&
        fixture.hir_values[seats_update2->right_value].binding_index == 3u);

  bool saw_final_seats = false;
  for (size_t index = 0u; index < fixture.hir_program.value_count; index += 1u)
    if (fixture.hir_values[index].kind == W_SEED_HIR0_VALUE_BINDING_READ &&
        fixture.hir_values[index].binding_index == 4u)
      saw_final_seats = true;
  CHECK(saw_final_seats &&
        w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool test_conditional_mutation_merge(void) {
  static const char SOURCE[] =
      "fn nextSeats(isOpen: Bool): i64 {\n"
      "  var seats = 5\n"
      "  let selected = if isOpen { seats + 1 } else { seats - 1 }\n"
      "  seats = selected\n"
      "  return seats\n"
      "}\n"
      "entry(nextSeats)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 1u && program->binding_count == 3u &&
        program->block_count == 4u && program->block_argument_count == 1u);
  const w_seed_hir0_binding *declaration = &program->bindings[0];
  const w_seed_hir0_binding *selected = &program->bindings[1];
  const w_seed_hir0_binding *merge = &program->bindings[2];
  CHECK(declaration->source_binding == 0u && declaration->next_version == 2u &&
        selected->source_binding == 1u &&
        selected->previous_version == W_SEED_HIR0_NONE &&
        selected->next_version == W_SEED_HIR0_NONE &&
        merge->source_binding == 0u && merge->previous_version == 0u &&
        merge->next_version == W_SEED_HIR0_NONE &&
        merge->owner_block == 3u &&
        merge->initializer_value < program->value_count);
  const w_seed_hir0_value *selected_value =
      &program->values[selected->initializer_value];
  const w_seed_hir0_value *merged = &program->values[merge->initializer_value];
  CHECK(selected_value->kind == W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        selected_value->block_argument_index == 0u &&
        merged->kind == W_SEED_HIR0_VALUE_BINDING_READ &&
        merged->binding_index == 1u &&
        program->blocks[3].block_argument_count == 1u &&
        program->block_arguments[0].owner_block == 3u &&
        program->block_arguments[0].type_index == W_SEED_HIR0_TYPE_I64 &&
        edge_value_at(program, 1u) < program->value_count &&
        edge_value_at(program, 2u) < program->value_count);
  const w_seed_hir0_value *then_value =
      &program->values[edge_value_at(program, 1u)];
  const w_seed_hir0_value *else_value =
      &program->values[edge_value_at(program, 2u)];
  CHECK(then_value->kind == W_SEED_HIR0_VALUE_BINARY_I64 &&
        else_value->kind == W_SEED_HIR0_VALUE_BINARY_I64 &&
        program->values[then_value->left_value].binding_index == 0u &&
        program->values[else_value->left_value].binding_index == 0u);
  bool saw_merged_read = false;
  for (size_t index = 0u; index < program->value_count; index += 1u)
    if (program->values[index].kind == W_SEED_HIR0_VALUE_BINDING_READ &&
        program->values[index].binding_index == 2u)
      saw_merged_read = true;
  CHECK(saw_merged_read && w_seed_hir0_verify(program, &fixture.hir_result));
  const uint32_t then_read_index = then_value->left_value;
  const w_seed_hir0_value saved_then_read =
      fixture.hir_values[then_read_index];
  fixture.hir_values[then_read_index].binding_index = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[then_read_index] = saved_then_read;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_bool_mutation_ssa(void) {
  static const char SOURCE[] =
      "fn availability(requested: Bool): Bool {\n"
      "  var open = false\n"
      "  open = requested\n"
      "  return open\n"
      "}\n"
      "entry(availability)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 1u && program->binding_count == 2u &&
        program->bindings[0].type_index == W_SEED_HIR0_TYPE_BOOL &&
        program->bindings[1].type_index == W_SEED_HIR0_TYPE_BOOL &&
        program->bindings[0].next_version == 1u &&
        program->bindings[1].source_binding == 0u &&
        program->bindings[1].previous_version == 0u);
  const w_seed_hir0_value *replacement =
      &program->values[program->bindings[1].initializer_value];
  CHECK(replacement->kind == W_SEED_HIR0_VALUE_PARAMETER_READ &&
        replacement->type_index == W_SEED_HIR0_TYPE_BOOL);
  const w_seed_hir0_terminator *terminator = &program->terminators[0];
  CHECK(terminator->kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        terminator->value_index < program->value_count &&
        program->values[terminator->value_index].kind ==
            W_SEED_HIR0_VALUE_BINDING_READ &&
        program->values[terminator->value_index].binding_index == 1u &&
        w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_branch_local_mutation_merge(void) {
  static const char SOURCE[] =
      "fn nextSeats(isOpen: Bool): i64 {\n"
      "  var seats = 5\n"
      "  if isOpen {\n"
      "    seats = seats + 1\n"
      "  } else {\n"
      "    seats = seats - 1\n"
      "  }\n"
      "  return seats\n"
      "}\n"
      "entry(nextSeats)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 1u && program->block_count == 4u &&
        program->block_argument_count == 1u &&
        program->binding_count == 2u);
  const w_seed_hir0_binding *declaration = &program->bindings[0];
  const w_seed_hir0_binding *merge = &program->bindings[1];
  CHECK(declaration->owner_block == 0u && declaration->next_version == 1u &&
        merge->owner_block == 3u && merge->source_binding == 0u &&
        merge->previous_version == 0u &&
        merge->next_version == W_SEED_HIR0_NONE &&
        program->blocks[3].block_argument_count == 1u &&
        program->block_arguments[0].owner_block == 3u &&
        program->block_arguments[0].type_index == W_SEED_HIR0_TYPE_I64);
  const w_seed_hir0_value *initializer =
      &program->values[merge->initializer_value];
  CHECK(initializer->kind == W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        initializer->block_argument_index == 0u &&
        initializer->owner_kind == W_SEED_HIR0_VALUE_OWNER_BINDING &&
        initializer->owner_index == 1u);
  CHECK(program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[0].result_type == 0u &&
        edge_value_at(program, 1u) != W_SEED_HIR0_NONE &&
        edge_value_at(program, 2u) != W_SEED_HIR0_NONE &&
        program->terminators[1].target_block == 3u &&
        program->terminators[2].target_block == 3u);
  const w_seed_hir0_terminator *return_term = &program->terminators[3];
  CHECK(return_term->kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        return_term->value_index < program->value_count &&
        program->values[return_term->value_index].kind ==
            W_SEED_HIR0_VALUE_BINDING_READ &&
        program->values[return_term->value_index].binding_index == 1u &&
        w_seed_hir0_verify(program, &fixture.hir_result));
  const w_seed_hir0_terminator saved_branch = fixture.hir_terminators[0];
  fixture.hir_terminators[0].result_type = W_SEED_HIR0_TYPE_I64;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[0] = saved_branch;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  const w_seed_hir0_terminator saved_then = fixture.hir_terminators[1];
  const uint32_t saved_then_edge_value = FIXTURE_EDGE_VALUE_SLOT(1u);
  FIXTURE_EDGE_VALUE_SLOT(1u) = W_SEED_HIR0_NONE;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[1] = saved_then;
  FIXTURE_EDGE_VALUE_SLOT(1u) = saved_then_edge_value;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_multi_branch_mutation_merge(void) {
  static const char SOURCE[] =
      "fn nextState(isOpen: Bool): i64 {\n"
      "  var seats = 5\n"
      "  var tables = 2\n"
      "  if isOpen {\n"
      "    tables = tables + 10\n"
      "    seats = seats + 1\n"
      "  } else {\n"
      "    seats = seats - 1\n"
      "    tables = tables - 10\n"
      "  }\n"
      "  return seats + tables\n"
      "}\n"
      "entry(nextState)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 1u && program->block_count == 4u &&
        program->block_argument_count == 2u && program->binding_count == 4u &&
        program->instruction_count == 4u);
  const w_seed_hir0_binding *seats = &program->bindings[0];
  const w_seed_hir0_binding *tables = &program->bindings[1];
  const w_seed_hir0_binding *seats_merge = &program->bindings[2];
  const w_seed_hir0_binding *tables_merge = &program->bindings[3];
  CHECK(seats->source_binding == 0u && seats->next_version == 2u &&
        tables->source_binding == 1u && tables->next_version == 3u &&
        seats_merge->source_binding == 0u && seats_merge->previous_version == 0u &&
        seats_merge->next_version == W_SEED_HIR0_NONE &&
        tables_merge->source_binding == 1u &&
        tables_merge->previous_version == 1u &&
        tables_merge->next_version == W_SEED_HIR0_NONE);
  CHECK(seats_merge->owner_block == 3u && tables_merge->owner_block == 3u &&
        seats_merge->type_index == W_SEED_HIR0_TYPE_I64 &&
        tables_merge->type_index == W_SEED_HIR0_TYPE_I64 &&
        seats_merge->name.count == 5u && tables_merge->name.count == 6u &&
        memcmp(fixture.hir_text + seats_merge->name.offset, "seats", 5u) == 0 &&
        memcmp(fixture.hir_text + tables_merge->name.offset, "tables", 6u) == 0);
  CHECK(program->blocks[3].first_block_argument == 0u &&
        program->blocks[3].block_argument_count == 2u &&
        program->block_arguments[0].owner_block == 3u &&
        program->block_arguments[0].ordinal == 0u &&
        program->block_arguments[0].type_index == W_SEED_HIR0_TYPE_I64 &&
        program->block_arguments[1].owner_block == 3u &&
        program->block_arguments[1].ordinal == 1u &&
        program->block_arguments[1].type_index == W_SEED_HIR0_TYPE_I64);
  CHECK(program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[0].result_type == 0u &&
        program->terminators[1].target_block == 3u &&
        program->terminators[2].target_block == 3u &&
        program->terminators[1].edge_argument_count == 2u &&
        program->terminators[2].edge_argument_count == 2u &&
        program->terminators[1].first_edge_argument == 0u &&
        program->terminators[2].first_edge_argument == 2u);
  for (size_t predecessor = 1u; predecessor <= 2u; predecessor += 1u) {
    const w_seed_hir0_terminator *terminator =
        &program->terminators[predecessor];
    for (size_t ordinal = 0u; ordinal < 2u; ordinal += 1u) {
      const w_seed_hir0_edge_argument *edge =
          &program->edge_arguments[(size_t)terminator->first_edge_argument +
                                   ordinal];
      CHECK(edge->owner_terminator == predecessor &&
            edge->owner_block == predecessor && edge->ordinal == ordinal &&
            edge->type_index == W_SEED_HIR0_TYPE_I64 &&
            edge->value_index < program->value_count);
    }
  }
  CHECK(program->values[program->edge_arguments[0].value_index].type_index ==
            W_SEED_HIR0_TYPE_I64 &&
        program->values[program->edge_arguments[2].value_index].type_index ==
            W_SEED_HIR0_TYPE_I64 &&
        w_seed_hir0_verify(program, &fixture.hir_result));
  const w_seed_hir0_edge_argument saved_edge = fixture.hir_edge_arguments[1];
  fixture.hir_edge_arguments[1].ordinal = 0u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_edge_arguments[1] = saved_edge;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_edge_arguments[1].type_index = W_SEED_HIR0_TYPE_BOOL;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_edge_arguments[1] = saved_edge;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  const w_seed_hir0_terminator saved_then = fixture.hir_terminators[1];
  fixture.hir_terminators[1].edge_argument_count = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[1] = saved_then;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_while_mutation_ssa(void) {
  static const char SOURCE[] =
      "fn countTo(limit: i64): i64 {\n"
      "  var count = 0\n"
      "  while count < limit { count = count + 1 }\n"
      "  return count\n"
      "}\n"
      "entry(countTo)\n";
  CHECK(lower(SOURCE));
  w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 1u && program->block_count == 4u &&
        program->block_argument_count == 1u &&
        program->edge_argument_count == 2u &&
        program->binding_count == 2u && program->instruction_count == 2u &&
        program->value_count == 10u && program->terminator_count == 4u);
  CHECK(program->blocks[0].instruction_count == 1u &&
        program->blocks[1].instruction_count == 0u &&
        program->blocks[1].first_block_argument == 0u &&
        program->blocks[1].block_argument_count == 1u &&
        program->blocks[2].instruction_count == 1u &&
        program->blocks[3].instruction_count == 0u &&
        program->block_arguments[0].owner_block == 1u &&
        program->block_arguments[0].ordinal == 0u &&
        program->block_arguments[0].type_index == W_SEED_HIR0_TYPE_I64);
  CHECK(program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[0].target_block == 1u &&
        program->terminators[0].edge_argument_count == 1u &&
        program->terminators[1].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[1].target_block == 2u &&
        program->terminators[1].else_block == 3u &&
        program->terminators[2].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[2].target_block == 1u &&
        program->terminators[2].edge_argument_count == 1u &&
        program->terminators[3].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_VALUE);
  const w_seed_hir0_binding declaration = fixture.hir_bindings[0];
  const w_seed_hir0_binding update = fixture.hir_bindings[1];
  CHECK(declaration.owner_block == 0u && declaration.is_mutable &&
        declaration.source_binding == 0u && declaration.next_version == 1u &&
        update.owner_block == 2u && update.is_mutable &&
        update.source_binding == 0u && update.previous_version == 0u &&
        update.next_version == W_SEED_HIR0_NONE &&
        program->values[update.initializer_value].kind ==
            W_SEED_HIR0_VALUE_BINARY_I64 &&
        program->values[program->values[update.initializer_value].left_value]
                .kind == W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        program->values[program->values[update.initializer_value].left_value]
                .block_argument_index == 0u);
  CHECK(program->values[program->terminators[1].value_index].type_index ==
            W_SEED_HIR0_TYPE_BOOL &&
        program->values[program->terminators[3].value_index].kind ==
            W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        program->values[program->terminators[3].value_index]
                .block_argument_index == 0u &&
        program->values[edge_value_at(program, 0u)].binding_index == 0u &&
        program->values[edge_value_at(program, 2u)].binding_index == 1u &&
        w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_block_argument saved_argument =
      fixture.hir_block_arguments[0];
  fixture.hir_block_arguments[0].type_index = W_SEED_HIR0_TYPE_BOOL;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_block_arguments[0] = saved_argument;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_terminator saved_preheader = fixture.hir_terminators[0];
  fixture.hir_terminators[0].edge_argument_count = 0u;
  fixture.hir_terminators[0].first_edge_argument = W_SEED_HIR0_NONE;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[0] = saved_preheader;
  reseal_hir_fixture();

  const w_seed_hir0_edge_argument saved_back_edge =
      fixture.hir_edge_arguments[1];
  fixture.hir_edge_arguments[1].value_index =
      fixture.hir_edge_arguments[0].value_index;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_edge_arguments[1] = saved_back_edge;
  reseal_hir_fixture();

  fixture.hir_block_arguments[0].owner_block = 2u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_block_arguments[0] = saved_argument;
  reseal_hir_fixture();

  const w_seed_hir0_terminator saved_body = fixture.hir_terminators[2];
  fixture.hir_terminators[2].target_block = 3u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[2] = saved_body;
  reseal_hir_fixture();

  const w_seed_hir0_terminator saved_exit = fixture.hir_terminators[3];
  fixture.hir_terminators[3] = saved_body;
  fixture.hir_terminators[3].owner_block = 3u;
  fixture.hir_terminators[3].target_block = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[3] = saved_exit;
  reseal_hir_fixture();

  fixture.hir_bindings[1].previous_version = W_SEED_HIR0_NONE;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_bindings[1] = update;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_repeat_mutation_ssa(void) {
  static const char SOURCE[] =
      "fn countTo(limit: i64): i64 {\n"
      "  var count = 0\n"
      "  repeat { count = count + 1 } while count < limit\n"
      "  return count\n"
      "}\n"
      "entry(countTo)\n";
  CHECK(lower(SOURCE));
  w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 1u && program->block_count == 5u &&
        program->block_argument_count == 1u &&
        program->edge_argument_count == 2u && program->binding_count == 2u &&
        program->instruction_count == 2u && program->terminator_count == 5u);
  CHECK(program->blocks[0].instruction_count == 1u &&
        program->blocks[1].first_block_argument == 0u &&
        program->blocks[1].block_argument_count == 1u &&
        program->blocks[1].instruction_count == 1u &&
        program->blocks[2].instruction_count == 0u &&
        program->blocks[3].instruction_count == 0u &&
        program->blocks[4].instruction_count == 0u &&
        program->block_arguments[0].owner_block == 1u &&
        program->block_arguments[0].ordinal == 0u &&
        program->block_arguments[0].type_index == W_SEED_HIR0_TYPE_I64);
  CHECK(program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[0].target_block == 1u &&
        program->terminators[0].edge_argument_count == 1u &&
        program->terminators[1].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[1].target_block == 2u &&
        program->terminators[1].edge_argument_count == 0u &&
        program->terminators[2].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[2].target_block == 3u &&
        program->terminators[2].else_block == 4u &&
        program->terminators[2].edge_argument_count == 0u &&
        program->terminators[3].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[3].target_block == 1u &&
        program->terminators[3].edge_argument_count == 1u &&
        program->terminators[4].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_VALUE);
  const w_seed_hir0_binding declaration = fixture.hir_bindings[0];
  const w_seed_hir0_binding update = fixture.hir_bindings[1];
  CHECK(declaration.owner_block == 0u && declaration.is_mutable &&
        declaration.source_binding == 0u && declaration.next_version == 1u &&
        update.owner_block == 1u && update.is_mutable &&
        update.source_binding == 0u && update.previous_version == 0u &&
        update.next_version == W_SEED_HIR0_NONE &&
        program->values[update.initializer_value].kind ==
            W_SEED_HIR0_VALUE_BINARY_I64 &&
        program->values[program->values[update.initializer_value].left_value]
                .kind == W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        program->values[program->values[update.initializer_value].left_value]
                .block_argument_index == 0u);
  CHECK(program->values[program->terminators[2].value_index].type_index ==
        W_SEED_HIR0_TYPE_BOOL);
  CHECK(program->values[program->terminators[4].value_index].kind ==
            W_SEED_HIR0_VALUE_BINDING_READ &&
        program->values[program->terminators[4].value_index].binding_index ==
            1u);
  CHECK(program->values[edge_value_at(program, 0u)].binding_index == 0u);
  CHECK(program->values[edge_value_at(program, 3u)].binding_index == 1u);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_terminator saved_condition = fixture.hir_terminators[2];
  fixture.hir_terminators[2].target_block = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[2] = saved_condition;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_repeat_multi_carrier_ssa(void) {
  static const char SOURCE[] =
      "fn receiptDigits(value: i64): i64 {\n"
      "  var remaining = value\n"
      "  var digits = 0\n"
      "  repeat {\n"
      "    digits = digits + 1\n"
      "    remaining = remaining / 10\n"
      "  } while remaining > 0\n"
      "  return digits\n"
      "}\n"
      "entry(receiptDigits)\n";
  CHECK(lower(SOURCE));
  w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 1u && program->block_count == 5u &&
        program->block_argument_count == 2u &&
        program->edge_argument_count == 4u && program->binding_count == 4u &&
        program->instruction_count == 4u && program->terminator_count == 5u);
  CHECK(program->blocks[1].block_argument_count == 2u &&
        program->blocks[1].instruction_count == 2u &&
        program->blocks[2].instruction_count == 0u &&
        program->blocks[3].instruction_count == 0u &&
        program->blocks[4].instruction_count == 0u &&
        program->terminators[2].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[2].target_block == 3u &&
        program->terminators[2].else_block == 4u &&
        program->terminators[3].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[3].target_block == 1u &&
        program->terminators[3].edge_argument_count == 2u);
  CHECK(program->bindings[0].source_binding == 0u &&
        program->bindings[0].next_version == 3u &&
        program->bindings[1].source_binding == 1u &&
        program->bindings[1].next_version == 2u &&
        program->bindings[2].source_binding == 1u &&
        program->bindings[2].previous_version == 1u &&
        program->bindings[3].source_binding == 0u &&
        program->bindings[3].previous_version == 0u &&
        value_tree_contains_binding_read(
            program, program->terminators[2].value_index, 3u, 0u) &&
        program->values[edge_value_at(program, 3u)].binding_index == 3u &&
        program->values[program->edge_arguments[3].value_index].binding_index ==
            2u &&
        program->values[program->terminators[4].value_index].kind ==
            W_SEED_HIR0_VALUE_BINDING_READ &&
        program->values[program->terminators[4].value_index].binding_index ==
            2u &&
        w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_while_multi_carrier_ssa(void) {
  static const char SOURCE[] =
      "fn serve(limit: i64): i64 {\n"
      "  var served = 0\n"
      "  var total = 0\n"
      "  while served < limit {\n"
      "    total = total + 2\n"
      "    served = served + 1\n"
      "  }\n"
      "  return served + total\n"
      "}\n"
      "entry(serve)\n";
  CHECK(lower(SOURCE));
  w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 1u && program->block_count == 4u &&
        program->block_argument_count == 2u &&
        program->edge_argument_count == 4u &&
        program->binding_count == 4u && program->instruction_count == 4u &&
        program->value_count == 18u && program->terminator_count == 4u);
  CHECK(program->blocks[0].instruction_count == 2u &&
        program->blocks[1].block_argument_count == 2u &&
        program->blocks[2].instruction_count == 2u &&
        program->block_arguments[0].ordinal == 0u &&
        program->block_arguments[1].ordinal == 1u);

  /* Carrier order follows root declarations even though the independent body
   * assignments are deliberately written in the opposite order. */
  CHECK(program->bindings[0].source_binding == 0u &&
        program->bindings[0].next_version == 3u &&
        program->bindings[1].source_binding == 1u &&
        program->bindings[1].next_version == 2u &&
        program->bindings[2].source_binding == 1u &&
        program->bindings[2].previous_version == 1u &&
        program->bindings[3].source_binding == 0u &&
        program->bindings[3].previous_version == 0u &&
        program->values[program->edge_arguments[0].value_index]
                .binding_index == 0u &&
        program->values[program->edge_arguments[1].value_index]
                .binding_index == 1u &&
        program->values[program->edge_arguments[2].value_index]
                .binding_index == 3u &&
        program->values[program->edge_arguments[3].value_index]
                .binding_index == 2u &&
        w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_block_argument saved_argument =
      fixture.hir_block_arguments[1];
  fixture.hir_block_arguments[1].ordinal = 0u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_block_arguments[1] = saved_argument;
  reseal_hir_fixture();

  const w_seed_hir0_edge_argument saved_edge = fixture.hir_edge_arguments[2];
  fixture.hir_edge_arguments[2].value_index =
      fixture.hir_edge_arguments[3].value_index;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_edge_arguments[2] = saved_edge;
  reseal_hir_fixture();

  const w_seed_hir0_binding saved_update = fixture.hir_bindings[3];
  fixture.hir_bindings[3].source_binding = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_bindings[3] = saved_update;
  reseal_hir_fixture();

  fixture.hir_bindings[3].previous_version = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_bindings[3] = saved_update;
  reseal_hir_fixture();

  fixture.hir_bindings[3].owner_block = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_bindings[3] = saved_update;
  reseal_hir_fixture();

  fixture.hir_bindings[3].type_index = W_SEED_HIR0_TYPE_BOOL;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_bindings[3] = saved_update;
  reseal_hir_fixture();

  const w_seed_hir0_terminator saved_backedge = fixture.hir_terminators[2];
  fixture.hir_terminators[2].edge_argument_count = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[2] = saved_backedge;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_while_multi_carrier_source_order(void) {
  static const char SOURCE[] =
      "fn accumulate(limit: i64): i64 {\n"
      "  var count = 0\n"
      "  var total = 10\n"
      "  while count < limit {\n"
      "    count = count + 1\n"
      "    total = total + count\n"
      "  }\n"
      "  return total\n"
      "}\n"
      "entry(accumulate)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->binding_count == 4u && program->bindings[2].source_binding == 0u &&
        program->bindings[3].source_binding == 1u);
  const w_seed_hir0_value *total =
      &program->values[program->bindings[3].initializer_value];
  CHECK(total->kind == W_SEED_HIR0_VALUE_BINARY_I64 &&
        total->right_value < program->value_count &&
        program->values[total->right_value].kind ==
            W_SEED_HIR0_VALUE_BINDING_READ &&
        program->values[total->right_value].binding_index == 2u &&
        w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_while_post_loop_continuation_ssa(void) {
  static const char SOURCE[] =
      "fn settle(limit: i64): i64 {\n"
      "  var served = 0\n"
      "  var total = 0\n"
      "  while served < limit {\n"
      "    total = total + 2\n"
      "    served = served + 1\n"
      "  }\n"
      "  total = total + served\n"
      "  return total\n"
      "}\n"
      "entry(settle)\n";
  CHECK(lower(SOURCE));
  w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 1u && program->block_count == 4u &&
        program->block_argument_count == 2u &&
        program->edge_argument_count == 4u && program->binding_count == 5u &&
        program->instruction_count == 5u && program->blocks[3].instruction_count ==
                                                     1u &&
        program->blocks[3].first_instruction == 4u &&
        program->terminators[3].ordinal == 1u);
  CHECK(program->bindings[0].source_binding == 0u &&
        program->bindings[0].next_version == 3u &&
        program->bindings[1].source_binding == 1u &&
        program->bindings[1].next_version == 2u &&
        program->bindings[2].source_binding == 1u &&
        program->bindings[2].previous_version == 1u &&
        program->bindings[2].next_version == 4u &&
        program->bindings[3].source_binding == 0u &&
        program->bindings[3].previous_version == 0u &&
        program->bindings[3].next_version == W_SEED_HIR0_NONE &&
        program->bindings[4].source_binding == 1u &&
        program->bindings[4].previous_version == 2u &&
        program->bindings[4].next_version == W_SEED_HIR0_NONE &&
        program->bindings[4].owner_block == 3u &&
        program->bindings[4].ordinal == 0u);
  const w_seed_hir0_value *continuation =
      &program->values[program->bindings[4].initializer_value];
  CHECK(continuation->kind == W_SEED_HIR0_VALUE_BINARY_I64 &&
        continuation->left_value < program->value_count &&
        continuation->right_value < program->value_count &&
        program->values[continuation->left_value].kind ==
            W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        program->values[continuation->right_value].kind ==
            W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        program->values[continuation->left_value].block_argument_index == 1u &&
        program->values[continuation->right_value].block_argument_index == 0u);
  const w_seed_hir0_terminator *exit_term = &program->terminators[3];
  CHECK(exit_term->value_index < program->value_count &&
        program->values[exit_term->value_index].kind ==
            W_SEED_HIR0_VALUE_BINDING_READ &&
        program->values[exit_term->value_index].binding_index == 4u &&
        w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_instruction saved_instruction = fixture.hir_instructions[4];
  fixture.hir_instructions[4].owner_block = 2u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_instructions[4] = saved_instruction;
  reseal_hir_fixture();

  const w_seed_hir0_binding saved_continuation = fixture.hir_bindings[4];
  fixture.hir_bindings[4].source_binding = 0u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_bindings[4] = saved_continuation;
  reseal_hir_fixture();

  const w_seed_hir0_block saved_exit = fixture.hir_blocks[3];
  fixture.hir_blocks[3].instruction_count = 2u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_blocks[3] = saved_exit;
  reseal_hir_fixture();

  const w_seed_hir0_binding saved_type = fixture.hir_bindings[4];
  fixture.hir_bindings[4].type_index = W_SEED_HIR0_TYPE_BOOL;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_bindings[4] = saved_type;
  reseal_hir_fixture();

  const w_seed_hir0_binding saved_update = fixture.hir_bindings[2];
  fixture.hir_bindings[2].next_version = W_SEED_HIR0_NONE;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_bindings[2] = saved_update;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_while_multi_carrier_general_values(void) {
  CHECK(lower(
      "fn exchange(limit: i64): i64 {\n"
      "  var left = 0\n"
      "  var right = 10\n"
      "  while left < limit {\n"
      "    left = right + 1\n"
      "    right = left + 1\n"
      "  }\n"
      "  return right\n"
      "}\nentry(exchange)\n"));
  CHECK(fixture.hir_program.block_argument_count == 2u &&
        fixture.hir_program.binding_count == 4u &&
        w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));

  CHECK(lower(
      "fn countTo(limit: i64): i64 {\n"
      "  let seed = 1\n"
      "  var count = seed\n"
      "  while count < limit { count = count + 1 }\n"
      "  return seed\n"
      "}\nentry(countTo)\n"));
  CHECK(fixture.hir_program.blocks[0].instruction_count == 2u &&
        fixture.hir_program.blocks[1].block_argument_count == 1u &&
        w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));

  CHECK(lower(
      "fn countTo(limit: i64): i64 {\n"
      "  var count = 0\n"
      "  while count < limit { count = count + 1 }\n"
      "  return 0\n"
      "}\nentry(countTo)\n"));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));

  return true;
}

static bool test_while_multi_carrier_native_subset(void) {
  static const char SOURCE[] =
      "fn serve(limit: i64): i64 {\n"
      "  let step = 1\n"
      "  var count = 0\n"
      "  var total = 10\n"
      "  while count < limit {\n"
      "    count = count + 1\n"
      "    total = total + count\n"
      "  }\n"
      "  return 0\n"
      "}\n"
      "fn main() {\n"
      "  let result = serve(limit: 3)\n"
      "  print(\"${result}\")\n"
      "}\n"
      "entry(main)\n";
  CHECK(lower_single_print_host(SOURCE));
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.natural_loop_functions[0] && selection.has_cfg);
  return true;
}

static bool expect_branch_mutation_unsupported(const char *source) {
  CHECK(fixture_frontend(source));
  setup_hir_output();
  const w_seed_hir0_input input = hir_input();
  w_seed_hir0_counts counts;
  w_seed_hir0_result result;
  CHECK(w_seed_hir0_measure(&input, &counts, &result) ==
        W_SEED_HIR0_UNSUPPORTED);
  return true;
}

static bool test_while_mutation_barriers(void) {
  CHECK(expect_branch_mutation_unsupported(
      "fn countTo(limit: i64): i64 {\n"
      "  var count = 0\n"
      "  while limit > 0 { count = count + 1 }\n"
      "  return count\n"
      "}\nentry(countTo)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn countTo(limit: i64): i64 {\n"
      "  var count = 0\n"
      "  while count < limit { count = limit }\n"
      "  return count\n"
      "}\nentry(countTo)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn countTo(limit: i64): i64 {\n"
      "  var count = 0\n"
      "  while count < limit { count = count + 1 count = count + 1 }\n"
      "  return count\n"
      "}\nentry(countTo)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn adjust(value: i64): i64 { return value + 1 }\n"
      "fn countTo(limit: i64): i64 {\n"
      "  var count = 0\n"
      "  while count < limit { count = adjust(value: count) }\n"
      "  return count\n"
      "}\nentry(countTo)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn countTo(limit: i64): i64 {\n"
      "  var count = 0\n"
      "  while count < limit { while count < limit { count = count + 1 } }\n"
      "  return count\n"
      "}\nentry(countTo)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn flip(limit: i64): Bool {\n"
      "  var open = false\n"
      "  while limit > 0 { open = !open }\n"
      "  return open\n"
      "}\nentry(flip)\n"));
  return true;
}

static bool test_branch_local_mutation_barriers(void) {
  CHECK(expect_branch_mutation_unsupported(
      "fn choose(flag: Bool): i64 {\n"
      "  var value = 1\n"
      "  if flag { value = 2 }\n"
      "  return value\n"
      "}\nentry(choose)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn choose(flag: Bool): i64 {\n"
      "  var left = 1\n"
      "  var right = 2\n"
      "  if flag { left = 3 } else { right = 4 }\n"
      "  return left\n"
      "}\nentry(choose)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn choose(flag: Bool): i64 {\n"
      "  var value = 1\n"
      "  if flag { value = 2 let extra = 3 } else { value = 3 }\n"
      "  return value\n"
      "}\nentry(choose)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn choose(flag: Bool): i64 {\n"
      "  var value = 1\n"
      "  if flag { value = 2 value = 3 } else { value = 4 }\n"
      "  return value\n"
      "}\nentry(choose)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn choose(flag: Bool): i64 {\n"
      "  var left = 1\n"
      "  var right = 2\n"
      "  if flag { left = 3 right = left + 1 } else { left = 4 right = 5 }\n"
      "  return left + right\n"
      "}\nentry(choose)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn choose(flag: Bool): i64 {\n"
      "  var left = 1\n"
      "  var right = 2\n"
      "  if flag { left = right + 1 right = 3 } else { left = 4 right = 5 }\n"
      "  return left + right\n"
      "}\nentry(choose)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn adjust(): i64 { return 4 }\n"
      "fn choose(flag: Bool): i64 {\n"
      "  var value = 1\n"
      "  if flag { value = adjust() } else { value = 2 }\n"
      "  return value\n"
      "}\nentry(choose)\n"));
  CHECK(expect_branch_mutation_unsupported(
      "fn choose(flag: Bool): i64 {\n"
      "  var value = 1\n"
      "  if flag { if true { value = 2 } else { value = 3 } }\n"
      "  else { value = 4 }\n"
      "  return value\n"
      "}\nentry(choose)\n"));
  return true;
}

static bool test_bindings_across_functions(void) {
  static const char SOURCE[] =
      "fn first() { let first = true }\n"
      "fn second() { let second = false }\n"
      "entry(second)\n";
  CHECK(lower(SOURCE));
  CHECK(fixture.hir_program.function_count == 2u &&
        fixture.hir_program.binding_count == 2u &&
        fixture.hir_program.instruction_count == 2u &&
        fixture.hir_program.value_count == 2u);
  CHECK(fixture.hir_program.bindings[0].owner_block == 0u &&
        fixture.hir_program.bindings[0].initializer_value == 0u &&
        fixture.hir_program.bindings[1].owner_block == 1u &&
        fixture.hir_program.bindings[1].initializer_value == 1u);
  CHECK(fixture.hir_program.values[0].kind == W_SEED_HIR0_VALUE_CONST_BOOL &&
        fixture.hir_program.values[0].bool_value &&
        fixture.hir_program.values[1].kind == W_SEED_HIR0_VALUE_CONST_BOOL &&
        !fixture.hir_program.values[1].bool_value);
  return true;
}

static bool test_local_binding_verify_mutations(void) {
  static const char SOURCE[] =
      "fn main() { let message = \"Table 42 remains open\" "
      "print(message: message, suffix: \"!\") }\nentry(main)\n";
  CHECK(lower(SOURCE));
  w_seed_hir0_program *program = &fixture.hir_program;
  w_seed_hir0_binding saved_binding = fixture.hir_bindings[0];
  w_seed_hir0_instruction saved_instruction0 = fixture.hir_instructions[0];
  w_seed_hir0_instruction saved_instruction1 = fixture.hir_instructions[1];
  w_seed_hir0_value saved_value1 = fixture.hir_values[1];
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_bindings[0].owner_instruction = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_bindings[0] = saved_binding;
  fixture.hir_bindings[0].type_index = W_SEED_HIR0_TYPE_UNIT;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_bindings[0] = saved_binding;
  fixture.hir_bindings[0].is_mutable = true;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_bindings[0] = saved_binding;
  fixture.hir_instructions[1].binding_index = 0u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_instructions[1] = saved_instruction1;
  fixture.hir_values[1].binding_index = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[1] = saved_value1;
  fixture.hir_bindings[0].initializer_value = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_bindings[0] = saved_binding;
  fixture.hir_program.bindings =
      (const w_seed_hir0_binding *)(const void *)fixture.hir_instructions;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_program.bindings = fixture.hir_bindings;
  fixture.hir_instructions[0] = saved_instruction0;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_local_enum_hir(void) {
  static const char SOURCE[] =
      "enum Stage { cold ready done }\n"
      "enum OtherStage { cold ready }\n"
      "fn choose(): Stage { return .ready }\n"
      "entry { }\n";
  CHECK(lower(SOURCE));
  CHECK(fixture.result.written.enums == 2u &&
        fixture.result.written.enum_cases == 5u &&
        fixture.result.written.enum_case_parameters == 0u &&
        fixture.result.written.switch_arms == 0u &&
        fixture.result.written.types == 4u &&
        fixture.result.written.functions == 2u &&
        fixture.result.written.entries == 1u &&
        fixture.result.written.expressions == 1u &&
        fixture.result.written.statements == 1u &&
        fixture.result.written.symbols == 11u);

  const w_seed_frontend_enum *stage = &fixture.output.enums[0];
  const w_seed_frontend_enum *other = &fixture.output.enums[1];
  CHECK(stage->module_index == 0u && stage->type_index == 0u &&
        stage->first_case == 0u && stage->case_count == 3u &&
        text_is(stage->name, "Stage"));
  CHECK(other->module_index == 0u && other->type_index == 1u &&
        other->first_case == 3u && other->case_count == 2u &&
        text_is(other->name, "OtherStage"));
  CHECK(fixture.output.types[0].kind == W_SEED_FRONTEND_TYPE_ENUM &&
        fixture.output.types[0].enum_base_index == 0u &&
        text_is(fixture.output.types[0].spelling, "Stage") &&
        fixture.output.types[1].kind == W_SEED_FRONTEND_TYPE_ENUM &&
        fixture.output.types[1].enum_base_index == 1u &&
        text_is(fixture.output.types[1].spelling, "OtherStage") &&
        fixture.output.types[2].kind == W_SEED_FRONTEND_TYPE_ENUM &&
        fixture.output.types[2].enum_base_index == 0u &&
        text_is(fixture.output.types[2].spelling, "Stage"));

  static const char *const STAGE_CASES[] = {"cold", "ready", "done"};
  static const char *const OTHER_CASES[] = {"cold", "ready"};
  for (size_t ordinal = 0u; ordinal < 3u; ordinal += 1u) {
    const w_seed_frontend_enum_case *value =
        &fixture.output.enum_cases[ordinal];
    CHECK(value->module_index == 0u && value->owner_enum == 0u &&
          value->first_payload == 0u &&
          value->payload_count == 0u &&
          text_is(value->name, STAGE_CASES[ordinal]));
  }
  for (size_t ordinal = 0u; ordinal < 2u; ordinal += 1u) {
    const w_seed_frontend_enum_case *value =
        &fixture.output.enum_cases[3u + ordinal];
    CHECK(value->module_index == 0u && value->owner_enum == 1u &&
          value->first_payload == 0u &&
          value->payload_count == 0u &&
          text_is(value->name, OTHER_CASES[ordinal]));
  }
  const w_seed_frontend_expression *frontend_value =
      &fixture.output.expressions[0];
  CHECK(frontend_value->kind == W_SEED_FRONTEND_EXPR_ENUM_CASE);
  CHECK(frontend_value->supported);
  CHECK(frontend_value->enum_index == 0u);
  CHECK(frontend_value->enum_case_index == 1u);
  CHECK(frontend_value->inferred_type < fixture.result.written.types);
  CHECK(fixture.output.types[frontend_value->inferred_type].kind ==
        W_SEED_FRONTEND_TYPE_ENUM);
  CHECK(fixture.output.types[frontend_value->inferred_type].enum_base_index ==
        0u);
  CHECK(fixture.output.functions[0].return_type < fixture.result.written.types);
  CHECK(fixture.output.types[fixture.output.functions[0].return_type].kind ==
        W_SEED_FRONTEND_TYPE_ENUM);
  CHECK(fixture.output.types[fixture.output.functions[0].return_type]
            .enum_base_index == 0u);

  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(fixture.hir_counts.enums == 2u &&
        fixture.hir_counts.enum_cases == 5u && fixture.hir_counts.types == 6u &&
        fixture.hir_counts.functions == 2u && fixture.hir_counts.entries == 1u &&
        fixture.hir_counts.values == 1u && fixture.hir_counts.blocks == 2u &&
        fixture.hir_counts.terminators == 2u && program->enum_count == 2u &&
        program->enum_case_count == 5u);
  CHECK(program->enums[0].module_index == 0u &&
        program->enums[0].type_index == 4u &&
        program->enums[0].first_case == 0u &&
        program->enums[0].case_count == 3u &&
        hir_text_is(program, program->enums[0].name, "Stage") &&
        program->enums[1].module_index == 0u &&
        program->enums[1].type_index == 5u &&
        program->enums[1].first_case == 3u &&
        program->enums[1].case_count == 2u &&
        hir_text_is(program, program->enums[1].name, "OtherStage"));
  CHECK(program->types[4].kind == W_SEED_HIR0_TYPE_ENUM &&
        program->types[4].owner_module == 0u &&
        program->types[4].enum_index == 0u &&
        program->types[4].lifecycle == W_SEED_HIR0_LIFECYCLE_VALUE_COPY &&
        program->types[4].release_contract ==
            W_SEED_HIR0_RELEASE_CONTRACT_NONE &&
        hir_text_equal(program, program->types[4].name, program->enums[0].name) &&
        program->types[5].kind == W_SEED_HIR0_TYPE_ENUM &&
        program->types[5].owner_module == 0u &&
        program->types[5].enum_index == 1u &&
        program->types[5].lifecycle == W_SEED_HIR0_LIFECYCLE_VALUE_COPY &&
        program->types[5].release_contract ==
            W_SEED_HIR0_RELEASE_CONTRACT_NONE &&
        hir_text_equal(program, program->types[5].name, program->enums[1].name));
  for (size_t ordinal = 0u; ordinal < 3u; ordinal += 1u) {
    const w_seed_hir0_enum_case *value = &program->enum_cases[ordinal];
    CHECK(value->owner_enum == 0u && value->ordinal == ordinal &&
          value->tag == ordinal && value->payload_count == 0u &&
          hir_text_is(program, value->name, STAGE_CASES[ordinal]));
  }
  for (size_t ordinal = 0u; ordinal < 2u; ordinal += 1u) {
    const w_seed_hir0_enum_case *value = &program->enum_cases[3u + ordinal];
    CHECK(value->owner_enum == 1u && value->ordinal == ordinal &&
          value->tag == ordinal && value->payload_count == 0u &&
          hir_text_is(program, value->name, OTHER_CASES[ordinal]));
  }
  CHECK(program->functions[0].return_type == 4u &&
        program->identities[1].return_type == 4u &&
        program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        program->terminators[0].value_index == 0u &&
        program->terminators[0].result_type == 4u);
  const w_seed_hir0_value saved_value = program->values[0];
  CHECK(saved_value.kind == W_SEED_HIR0_VALUE_ENUM_CASE &&
        saved_value.owner_kind == W_SEED_HIR0_VALUE_OWNER_TERMINATOR &&
        saved_value.owner_index == 0u && saved_value.owner_ordinal == 0u &&
        saved_value.type_index == 4u && saved_value.enum_index == 0u &&
        saved_value.enum_case_index == 1u &&
        saved_value.external_module_index == W_SEED_HIR0_NONE &&
        saved_value.external_symbol_index == W_SEED_HIR0_NONE &&
        saved_value.member_name.count == 0u);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_enum saved_enum0 = fixture.hir_enums[0];
  fixture.hir_enums[0].type_index = 5u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enums[0] = saved_enum0;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_enums[0].first_case = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enums[0] = saved_enum0;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_enum_case saved_case1 = fixture.hir_enum_cases[1];
  fixture.hir_enum_cases[1].owner_enum = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_cases[1] = saved_case1;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_enum_cases[1].ordinal = 2u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_cases[1] = saved_case1;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_enum_cases[1].tag = 9u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_cases[1] = saved_case1;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_enum_cases[1].payload_count = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_cases[1] = saved_case1;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_enum_cases[1].name = fixture.hir_enum_cases[0].name;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_cases[1] = saved_case1;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[0].enum_index = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[0] = saved_value;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[0].enum_case_index = 3u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[0] = saved_value;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[0].type_index = 5u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[0] = saved_value;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_values[0].kind = W_SEED_HIR0_VALUE_CONST_BOOL;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[0] = saved_value;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_input input = hir_input();
  const uint8_t sentinel = 0xa5u;
  w_seed_hir0_result rejected;
  (void)memset(&rejected, 0x42, sizeof(rejected));
  const w_seed_hir0_result rejected_before = rejected;

  setup_hir_output();
  fill_hir_output(sentinel);
  fixture.hir_output.enum_capacity = fixture.hir_counts.enums - 1u;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(sentinel) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  setup_hir_output();
  fill_hir_output(sentinel);
  fixture.hir_output.enum_capacity = 0u;
  fixture.hir_output.enums = NULL;
  rejected = rejected_before;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(sentinel) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  setup_hir_output();
  fill_hir_output(sentinel);
  fixture.hir_output.enum_case_capacity = fixture.hir_counts.enum_cases - 1u;
  rejected = rejected_before;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(sentinel) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  setup_hir_output();
  fill_hir_output(sentinel);
  fixture.hir_output.enum_case_capacity = 0u;
  fixture.hir_output.enum_cases = NULL;
  rejected = rejected_before;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(sentinel) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  setup_hir_output();
  fill_hir_output(sentinel);
  w_seed_hir0_output alias = fixture.hir_output;
  alias.enum_cases = (w_seed_hir0_enum_case *)(void *)alias.enums;
  rejected = rejected_before;
  CHECK(w_seed_hir0_run(&input, &alias, &rejected) == W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(sentinel) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  return true;
}

static bool test_typed_throw_hir(void) {
  static const char SOURCE[] =
      "enum Failure: Error { denied }\n"
      "fn fail(): i64 throws Failure { throw .denied }\n"
      "entry { }\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->enum_count == 1u && program->function_count == 2u &&
        program->block_count == 2u && program->terminator_count == 2u &&
        program->value_count == 1u);
  CHECK(program->enums[0].error_conformance &&
        program->enums[0].type_index == 4u &&
        program->functions[0].is_throws &&
        program->functions[0].return_type == W_SEED_HIR0_TYPE_I64 &&
        program->functions[0].error_type == 4u &&
        program->functions[1].is_anonymous_entry &&
        !program->functions[1].is_throws &&
        program->functions[1].error_type == W_SEED_HIR0_NONE);
  CHECK(program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_THROW &&
        program->terminators[0].value_index == 0u &&
        program->terminators[0].result_type == 4u &&
        program->values[0].kind == W_SEED_HIR0_VALUE_ENUM_CASE &&
        program->values[0].type_index == 4u &&
        program->values[0].enum_index == 0u &&
        program->values[0].enum_case_index == 0u);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_function saved_function = fixture.hir_functions[0];
  const w_seed_hir0_enum saved_enum = fixture.hir_enums[0];
  const w_seed_hir0_terminator saved_terminator = fixture.hir_terminators[0];

  fixture.hir_functions[0].error_type = W_SEED_HIR0_NONE;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_functions[0] = saved_function;

  fixture.hir_enums[0].error_conformance = false;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enums[0] = saved_enum;

  fixture.hir_terminators[0].result_type = W_SEED_HIR0_TYPE_UNIT;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[0] = saved_terminator;

  fixture.hir_terminators[0].kind = W_SEED_HIR0_TERMINATOR_RETURN_VALUE;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[0] = saved_terminator;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  static const char MIXED_EXIT_SOURCE[] =
      "enum Failure: Error { denied }\n"
      "fn fail(flag: Bool): i64 throws Failure { "
      "if flag { throw .denied } else { return 7 } }\n"
      "entry { }\n";
  CHECK(lower(MIXED_EXIT_SOURCE));
  program = &fixture.hir_program;
  CHECK(program->functions[0].is_throws &&
        program->functions[0].return_type == W_SEED_HIR0_TYPE_I64 &&
        program->functions[0].error_type == 4u &&
        program->functions[0].block_count == 3u &&
        program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[0].target_block == 1u &&
        program->terminators[0].else_block == 2u &&
        program->terminators[1].kind == W_SEED_HIR0_TERMINATOR_THROW &&
        program->terminators[1].result_type == 4u &&
        program->terminators[2].kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        program->terminators[2].result_type == W_SEED_HIR0_TYPE_I64);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  const w_seed_hir0_terminator saved_mixed_throw = fixture.hir_terminators[1];
  fixture.hir_terminators[1].kind = W_SEED_HIR0_TERMINATOR_JUMP;
  fixture.hir_terminators[1].value_index = W_SEED_HIR0_NONE;
  fixture.hir_terminators[1].result_type = 0u;
  fixture.hir_terminators[1].target_block = 2u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[1] = saved_mixed_throw;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  static const char NON_ERROR_SOURCE[] =
      "enum Failure { denied }\n"
      "fn fail(): () throws Failure { throw .denied }\n"
      "entry { }\n";
  CHECK(fixture_frontend(NON_ERROR_SOURCE));
  setup_hir_output();
  fill_hir_output(0xa5u);
  const w_seed_hir0_input input = hir_input();
  w_seed_hir0_counts counts = fixture.hir_counts;
  w_seed_hir0_result result = fixture.hir_result;
  CHECK(w_seed_hir0_measure(&input, &counts, &result) != W_SEED_HIR0_OK);
  CHECK(hir_output_is_byte(0xa5u));

  static const char BRANCH_THROW_SOURCE[] =
      "enum Failure: Error { denied }\n"
      "fn fail(flag: Bool): i64 throws Failure { "
      "if flag { throw .denied } return 1 }\n"
      "entry { }\n";
  CHECK(fixture_frontend(BRANCH_THROW_SOURCE));
  setup_hir_output();
  counts = fixture.hir_counts;
  result = fixture.hir_result;
  const w_seed_hir0_input branch_input = hir_input();
  CHECK(w_seed_hir0_measure(&branch_input, &counts, &result) !=
        W_SEED_HIR0_OK);

  static const char EARLY_BRANCH_THROW_SOURCE[] =
      "enum Failure: Error { denied }\n"
      "fn fail(first: Bool, second: Bool): i64 throws Failure { "
      "if first { throw .denied } "
      "if second { throw .denied } else { return 7 } }\n"
      "entry { }\n";
  CHECK(fixture_frontend(EARLY_BRANCH_THROW_SOURCE));
  setup_hir_output();
  counts = fixture.hir_counts;
  result = fixture.hir_result;
  const w_seed_hir0_input early_branch_input = hir_input();
  CHECK(w_seed_hir0_measure(&early_branch_input, &counts, &result) !=
        W_SEED_HIR0_OK);
  return true;
}

static bool test_typed_invoke_hir(void) {
  static const char SOURCE[] =
      "enum Failure: Error { denied }\n"
      "fn leaf(): i64 throws Failure { throw .denied }\n"
      "fn relay(): i64 throws Failure { return try leaf() }\n"
      "entry { }\n";
  CHECK(lower(SOURCE));
  w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 3u && program->block_count == 5u &&
        program->instruction_count == 0u && program->call_count == 1u &&
        program->block_argument_count == 2u &&
        program->edge_argument_count == 0u &&
        program->terminator_count == 5u && program->value_count == 3u);
  const w_seed_hir0_function *relay = &program->functions[1];
  CHECK(relay->is_throws && relay->return_type == W_SEED_HIR0_TYPE_I64 &&
        relay->error_type == 4u && relay->block_count == 3u);
  const uint32_t invoke_block = relay->first_block;
  const uint32_t normal_block = invoke_block + 1u;
  const uint32_t error_block = invoke_block + 2u;
  const w_seed_hir0_terminator *invoke =
      &program->terminators[invoke_block];
  CHECK(invoke->kind == W_SEED_HIR0_TERMINATOR_INVOKE &&
        invoke->call_index == 0u && invoke->value_index == W_SEED_HIR0_NONE &&
        invoke->result_type == W_SEED_HIR0_TYPE_I64 &&
        invoke->error_type == relay->error_type &&
        invoke->target_block == normal_block &&
        invoke->else_block == error_block &&
        program->blocks[normal_block].block_argument_count == 1u &&
        program->blocks[error_block].block_argument_count == 1u &&
        program->block_arguments[program->blocks[normal_block]
                                     .first_block_argument]
                .type_index == W_SEED_HIR0_TYPE_I64 &&
        program->block_arguments[program->blocks[error_block]
                                     .first_block_argument]
                .type_index == relay->error_type);
  const w_seed_hir0_call *call = &program->calls[invoke->call_index];
  CHECK(call->owner_instruction == W_SEED_HIR0_NONE &&
        call->owner_terminator == invoke_block &&
        call->owner_block == invoke_block &&
        call->execution_kind == W_SEED_HIR0_CALL_DIRECT &&
        program->identities[call->callee_identity].target_index == 0u &&
        program->terminators[normal_block].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        program->terminators[error_block].kind ==
            W_SEED_HIR0_TERMINATOR_THROW &&
        program->values[program->terminators[normal_block].value_index].kind ==
            W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        program->values[program->terminators[error_block].value_index].kind ==
            W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        w_seed_hir0_verify(program, &fixture.hir_result));

  w_seed_product_closure0_counts closure_counts;
  w_seed_product_closure0_result closure_result;
  (void)memset(&closure_counts, 0xa5, sizeof(closure_counts));
  (void)memset(&closure_result, 0x5a, sizeof(closure_result));
  const w_seed_product_closure0_counts closure_counts_before = closure_counts;
  const w_seed_product_closure0_result closure_result_before = closure_result;
  const w_seed_product_closure0_input closure_input = {
      .program = program, .hir_result = &fixture.hir_result};
  CHECK(w_seed_product_closure0_measure(
            &closure_input, &closure_counts, &closure_result) ==
        W_SEED_PRODUCT_CLOSURE0_UNSUPPORTED);
  CHECK(memcmp(&closure_counts, &closure_counts_before,
               sizeof(closure_counts)) == 0 &&
        memcmp(&closure_result, &closure_result_before,
               sizeof(closure_result)) == 0);

  (void)memset(&fixture.document, 0, sizeof(fixture.document));
  (void)memset(&fixture.input, 0, sizeof(fixture.input));
  (void)memset(&fixture.output, 0, sizeof(fixture.output));
  (void)memset(&fixture.result, 0, sizeof(fixture.result));
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_terminator saved_invoke = fixture.hir_terminators[invoke_block];
  const w_seed_hir0_call saved_call = fixture.hir_calls[0];
  const w_seed_hir0_block_argument saved_normal_argument =
      fixture.hir_block_arguments[program->blocks[normal_block]
                                      .first_block_argument];
  const w_seed_hir0_block_argument saved_error_argument =
      fixture.hir_block_arguments[program->blocks[error_block]
                                      .first_block_argument];
  const w_seed_hir0_value saved_normal_value =
      fixture.hir_values[fixture.hir_terminators[normal_block].value_index];
  const w_seed_hir0_value saved_error_value =
      fixture.hir_values[fixture.hir_terminators[error_block].value_index];
  fixture.hir_terminators[invoke_block].target_block = error_block;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[invoke_block] = saved_invoke;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_terminators[invoke_block].call_index = W_SEED_HIR0_NONE;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[invoke_block] = saved_invoke;
  fixture.hir_terminators[invoke_block].error_type = W_SEED_HIR0_TYPE_I64;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[invoke_block] = saved_invoke;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_calls[0].owner_instruction = 0u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_calls[0] = saved_call;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_calls[0].owner_terminator = error_block;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_calls[0] = saved_call;
  fixture.hir_calls[0].callee_identity = program->entries[0].identity_index;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_calls[0] = saved_call;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_block_arguments[program->blocks[normal_block]
                                  .first_block_argument]
      .type_index = relay->error_type;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_block_arguments[program->blocks[normal_block]
                                  .first_block_argument] = saved_normal_argument;

  fixture.hir_block_arguments[program->blocks[error_block]
                                  .first_block_argument]
      .type_index = W_SEED_HIR0_TYPE_I64;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_block_arguments[program->blocks[error_block]
                                  .first_block_argument] = saved_error_argument;
  fixture.hir_values[fixture.hir_terminators[normal_block].value_index]
      .block_argument_index = program->blocks[error_block].first_block_argument;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[fixture.hir_terminators[normal_block].value_index] =
      saved_normal_value;
  fixture.hir_values[fixture.hir_terminators[error_block].value_index]
      .block_argument_index = program->blocks[normal_block].first_block_argument;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[fixture.hir_terminators[error_block].value_index] =
      saved_error_value;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_typed_invoke_cleanup_hir(void) {
  static const char SOURCE[] =
      "enum Failure: Error { denied }\n"
      "fn clean() { }\n"
      "fn leaf(): i64 throws Failure { throw .denied }\n"
      "fn relay(): i64 throws Failure { defer { clean() } return try leaf() }\n"
      "entry { }\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->cleanup_count == 1u && program->call_count == 3u &&
        program->instruction_count == 2u && program->block_argument_count == 2u);
  size_t relay_index = SIZE_MAX;
  for (size_t index = 0u; index < program->function_count; index += 1u)
    if (hir_text_is(program, program->functions[index].name, "relay"))
      relay_index = index;
  CHECK(relay_index != SIZE_MAX);
  const w_seed_hir0_function *relay = &program->functions[relay_index];
  CHECK(relay->block_count == 3u);
  const uint32_t invoke_block = relay->first_block;
  const uint32_t normal_block = invoke_block + 1u;
  const uint32_t error_block = invoke_block + 2u;
  const w_seed_hir0_cleanup *cleanup = &program->cleanups[0];
  CHECK(cleanup->owner_function == relay_index &&
        cleanup->invoke_terminator == invoke_block &&
        cleanup->normal_block == normal_block &&
        cleanup->error_block == error_block &&
        cleanup->normal_instruction ==
            program->blocks[normal_block].first_instruction &&
        cleanup->error_instruction ==
            program->blocks[error_block].first_instruction &&
        cleanup->normal_call != cleanup->error_call &&
        program->calls[cleanup->normal_call].callee_identity ==
            cleanup->cleanup_identity &&
        program->calls[cleanup->error_call].callee_identity ==
            cleanup->cleanup_identity &&
        program->instructions[cleanup->normal_instruction].kind ==
            W_SEED_HIR0_INSTRUCTION_CALL &&
        program->instructions[cleanup->error_instruction].kind ==
            W_SEED_HIR0_INSTRUCTION_CALL &&
        program->terminators[invoke_block].kind ==
            W_SEED_HIR0_TERMINATOR_INVOKE &&
        program->terminators[normal_block].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        program->terminators[error_block].kind ==
            W_SEED_HIR0_TERMINATOR_THROW &&
        program->values[program->terminators[normal_block].value_index].kind ==
            W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        program->values[program->terminators[error_block].value_index].kind ==
            W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_cleanup saved_cleanup = fixture.hir_cleanups[0];
  const w_seed_hir0_block saved_error_block = fixture.hir_blocks[error_block];
  const w_seed_hir0_instruction saved_normal_instruction =
      fixture.hir_instructions[cleanup->normal_instruction];
  const w_seed_hir0_call saved_normal_call =
      fixture.hir_calls[cleanup->normal_call];
  const w_seed_hir0_terminator saved_normal_terminator =
      fixture.hir_terminators[normal_block];
  const uint32_t invoke_call = fixture.hir_terminators[invoke_block].call_index;
  CHECK(invoke_call < program->call_count);

  fixture.hir_cleanups[0].owner_function = 0u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_cleanups[0] = saved_cleanup;

  fixture.hir_cleanups[0].error_block = normal_block;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_cleanups[0] = saved_cleanup;

  fixture.hir_cleanups[0].cleanup_identity =
      fixture.hir_calls[invoke_call].callee_identity;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_cleanups[0] = saved_cleanup;

  fixture.hir_blocks[error_block].instruction_count = 0u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_blocks[error_block] = saved_error_block;

  fixture.hir_instructions[cleanup->normal_instruction].ordinal = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_instructions[cleanup->normal_instruction] =
      saved_normal_instruction;

  fixture.hir_calls[cleanup->normal_call].owner_block = error_block;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_calls[cleanup->normal_call] = saved_normal_call;

  fixture.hir_calls[cleanup->normal_call].argument_count = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_calls[cleanup->normal_call] = saved_normal_call;

  fixture.hir_terminators[normal_block].value_index = W_SEED_HIR0_NONE;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[normal_block] = saved_normal_terminator;

  fixture.hir_cleanups[0].source_span.end_byte =
      program->modules[relay->module_index].source_length + 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_cleanups[0] = saved_cleanup;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  w_seed_product_closure0_counts closure_counts;
  w_seed_product_closure0_result closure_result;
  (void)memset(&closure_counts, 0xa5, sizeof(closure_counts));
  (void)memset(&closure_result, 0x5a, sizeof(closure_result));
  const w_seed_product_closure0_counts closure_counts_before = closure_counts;
  const w_seed_product_closure0_result closure_result_before = closure_result;
  const w_seed_product_closure0_input closure_input = {
      .program = program, .hir_result = &fixture.hir_result};
  CHECK(w_seed_product_closure0_measure(
            &closure_input, &closure_counts, &closure_result) ==
        W_SEED_PRODUCT_CLOSURE0_UNSUPPORTED &&
        memcmp(&closure_counts, &closure_counts_before,
               sizeof(closure_counts)) == 0 &&
        memcmp(&closure_result, &closure_result_before,
               sizeof(closure_result)) == 0);

  const w_seed_hir0_input input = hir_input();
  const uint8_t sentinel = 0xa5u;
  w_seed_hir0_counts measured_counts;
  w_seed_hir0_result measured_result;
  CHECK(w_seed_hir0_measure(&input, &measured_counts, &measured_result) ==
        W_SEED_HIR0_OK);
  CHECK(measured_counts.cleanups == 1u);

  setup_hir_output();
  fill_hir_output(sentinel);
  fixture.hir_output.cleanups = NULL;
  fixture.hir_output.cleanup_capacity = 0u;
  w_seed_hir0_result rejected_result;
  (void)memset(&rejected_result, 0x5a, sizeof(rejected_result));
  const w_seed_hir0_result rejected_before = rejected_result;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected_result) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(sentinel) &&
        memcmp(&rejected_result, &rejected_before,
               sizeof(rejected_result)) == 0);

  setup_hir_output();
  fill_hir_output(sentinel);
  w_seed_hir0_output alias = fixture.hir_output;
  alias.cleanups = (w_seed_hir0_cleanup *)(void *)alias.calls;
  rejected_result = rejected_before;
  CHECK(w_seed_hir0_run(&input, &alias, &rejected_result) ==
        W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(sentinel) &&
        memcmp(&rejected_result, &rejected_before,
               sizeof(rejected_result)) == 0);

  CHECK(lower(SOURCE));
  (void)memset(&fixture.document, 0, sizeof(fixture.document));
  (void)memset(&fixture.input, 0, sizeof(fixture.input));
  (void)memset(&fixture.output, 0, sizeof(fixture.output));
  (void)memset(&fixture.result, 0, sizeof(fixture.result));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

#if defined(_WIN32) && defined(_WIN64)
typedef struct {
  w_seed_parallel_platform1_completion requested[2];
  uint32_t calls[2];
  bool fail;
} typed_binding1_provider_context;

static bool typed_binding1_provider_invoke(
    void *raw, size_t task_index,
    w_seed_parallel_platform1_completion *completion) {
  typed_binding1_provider_context *context =
      (typed_binding1_provider_context *)raw;
  if (context == NULL || completion == NULL || task_index >= 2u ||
      context->fail)
    return false;
  context->calls[task_index] += 1u;
  *completion = context->requested[task_index];
  return true;
}

static uint32_t typed_binding1_function_named(
    const w_seed_hir0_program *program, const char *name) {
  if (program == NULL || name == NULL) return W_SEED_HIR0_NONE;
  for (size_t index = 0u; index < program->function_count; index += 1u)
    if (hir_text_is(program, program->functions[index].name, name))
      return (uint32_t)index;
  return W_SEED_HIR0_NONE;
}

static bool test_parallel_typed_binding1(void) {
  static const char SOURCE[] =
      "enum Failure: Error { denied }\n"
      "fn clean() { }\n"
      "fn succeed(): i64 { return 42 }\n"
      "fn successCaller(): i64 { return succeed() }\n"
      "fn leaf(): i64 throws Failure { throw .denied }\n"
      "fn relay(): i64 throws Failure { defer { clean() } return try leaf() }\n"
      "entry { }\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  const uint32_t success_caller =
      typed_binding1_function_named(program, "successCaller");
  const uint32_t relay = typed_binding1_function_named(program, "relay");
  CHECK(success_caller != W_SEED_HIR0_NONE && relay != W_SEED_HIR0_NONE);
  const w_seed_hir0_function *success_function =
      &program->functions[success_caller];
  const w_seed_hir0_function *relay_function = &program->functions[relay];
  CHECK(success_function->block_count == 1u &&
        success_function->first_block < program->block_count &&
        relay_function->block_count == 3u &&
        relay_function->first_block < program->block_count);
  const w_seed_hir0_block *success_block =
      &program->blocks[success_function->first_block];
  CHECK(success_block->instruction_count == 1u &&
        success_block->first_instruction < program->instruction_count);
  const w_seed_hir0_instruction *success_instruction =
      &program->instructions[success_block->first_instruction];
  const uint32_t success_call = success_instruction->call_index;
  const uint32_t invoke_terminator =
      program->blocks[relay_function->first_block].terminator_index;
  CHECK(invoke_terminator < program->terminator_count);
  const uint32_t error_call = program->terminators[invoke_terminator].call_index;
  CHECK(success_call < program->call_count && error_call < program->call_count &&
        success_call != error_call);

  const w_seed_parallel_typed_binding1_task tasks[2] = {
      {.lexical_index = 0u, .call_index = success_call},
      {.lexical_index = 1u, .call_index = error_call}};
  typed_binding1_provider_context context = {0};
  context.requested[0] = (w_seed_parallel_platform1_completion){
      .kind = W_SEED_PARALLEL_PLATFORM1_COMPLETION_SUCCESS,
      .success_value = 42};
  context.requested[1] = (w_seed_parallel_platform1_completion){
      .kind = W_SEED_PARALLEL_PLATFORM1_COMPLETION_ERROR,
      .error_code = 17u};
  w_seed_parallel_typed_binding1_provider provider = {
      .target = W_SEED_PARALLEL_TYPED_BINDING1_TARGET_WINDOWS_AMD64};
  (void)memcpy(provider.profile,
               W_SEED_PARALLEL_TYPED_BINDING1_WINDOWS_PROFILE,
               sizeof(W_SEED_PARALLEL_TYPED_BINDING1_WINDOWS_PROFILE));
  (void)memcpy(provider.identity,
               W_SEED_PARALLEL_TYPED_BINDING1_WINDOWS_IDENTITY,
               sizeof(W_SEED_PARALLEL_TYPED_BINDING1_WINDOWS_IDENTITY));
  w_seed_parallel_local_provider1_authority provider_authority;
  CHECK(w_seed_parallel_local_provider1_open(
            W_SEED_PARALLEL_LOCAL_PROVIDER1_WINDOWS_AMD64,
            &provider_authority) &&
        w_seed_parallel_local_provider1_verify(&provider_authority));
  w_seed_parallel_typed_binding1_input input = {
      .hir_program = program,
      .hir_result = &fixture.hir_result,
      .invoke_terminator = invoke_terminator,
      .tasks = tasks,
      .task_count = 2u,
      .task_capacity = 2u,
      .provider_job = {typed_binding1_provider_invoke, &context,
                       sizeof(context)},
      .provider_authority = &provider_authority,
      .provider_capacity = 1u,
      .generation = 41u,
      .provider = provider};

  w_seed_parallel_typed_binding1_counts counts;
  w_seed_parallel_typed_binding1_result measured;
  CHECK(w_seed_parallel_typed_binding1_measure(&input, &counts, &measured) ==
            W_SEED_PARALLEL_TYPED_BINDING1_OK &&
        counts.completions == 2u && counts.records == 2u &&
        measured.written.completions == 0u && measured.written.records == 0u);
  w_seed_parallel_local_provider1_authority unopened_authority;
  (void)memset(&unopened_authority, 0x51, sizeof(unopened_authority));
  const w_seed_parallel_local_provider1_authority unopened_before =
      unopened_authority;
  CHECK(!w_seed_parallel_local_provider1_open(0u, &unopened_authority) &&
        memcmp(&unopened_authority, &unopened_before,
               sizeof(unopened_authority)) == 0);
  w_seed_parallel_local_provider1_authority forged_authority =
      provider_authority;
  forged_authority.receipt.contract_digest[0] ^= 1u;
  CHECK(!w_seed_parallel_local_provider1_verify(&forged_authority));
  forged_authority = provider_authority;
  forged_authority.private_seal ^= (uintptr_t)1u;
  CHECK(!w_seed_parallel_local_provider1_verify(&forged_authority));
  forged_authority = provider_authority;
  forged_authority.receipt.contract_digest[0] ^= 1u;
  w_seed_parallel_typed_binding1_input forged_authority_input = input;
  forged_authority_input.provider_authority = &forged_authority;
  w_seed_parallel_typed_binding1_counts authority_counts;
  w_seed_parallel_typed_binding1_result authority_result;
  (void)memset(&authority_counts, 0x2e, sizeof(authority_counts));
  (void)memset(&authority_result, 0xe2, sizeof(authority_result));
  const w_seed_parallel_typed_binding1_counts authority_counts_before =
      authority_counts;
  const w_seed_parallel_typed_binding1_result authority_result_before =
      authority_result;
  CHECK(w_seed_parallel_typed_binding1_measure(
            &forged_authority_input, &authority_counts, &authority_result) ==
            W_SEED_PARALLEL_TYPED_BINDING1_AUTHORITY &&
        memcmp(&authority_counts, &authority_counts_before,
               sizeof(authority_counts)) == 0 &&
        memcmp(&authority_result, &authority_result_before,
               sizeof(authority_result)) == 0);
  w_seed_parallel_platform1_completion local_sentinel_completions[2];
  w_seed_parallel_platform1_receipt local_sentinel_receipt;
  w_seed_parallel_provider0_kind local_sentinel_kind =
      W_SEED_PARALLEL_PROVIDER0_KIND_NONE;
  (void)memset(local_sentinel_completions, 0x4c,
               sizeof(local_sentinel_completions));
  (void)memset(&local_sentinel_receipt, 0xc4,
               sizeof(local_sentinel_receipt));
  const w_seed_parallel_platform1_completion
      local_sentinel_completions_before[2] = {
          local_sentinel_completions[0], local_sentinel_completions[1]};
  const w_seed_parallel_platform1_receipt local_sentinel_receipt_before =
      local_sentinel_receipt;
  CHECK(w_seed_parallel_local_provider1_execute(
            &forged_authority, &input.provider_job, input.task_count,
            input.provider_capacity, local_sentinel_completions,
            &local_sentinel_receipt, &local_sentinel_kind) ==
            W_SEED_PARALLEL_PROVIDER0_PLATFORM_PROVIDER_FAILURE &&
        memcmp(local_sentinel_completions,
               local_sentinel_completions_before,
               sizeof(local_sentinel_completions)) == 0 &&
        memcmp(&local_sentinel_receipt, &local_sentinel_receipt_before,
               sizeof(local_sentinel_receipt)) == 0 &&
        local_sentinel_kind == W_SEED_PARALLEL_PROVIDER0_KIND_NONE);

  w_seed_parallel_typed_binding1_input mismatched_provider_input = input;
  mismatched_provider_input.provider.profile[0] ^= 1;
  CHECK(w_seed_parallel_typed_binding1_measure(
            &mismatched_provider_input, &authority_counts,
            &authority_result) == W_SEED_PARALLEL_TYPED_BINDING1_AUTHORITY &&
        memcmp(&authority_counts, &authority_counts_before,
               sizeof(authority_counts)) == 0 &&
        memcmp(&authority_result, &authority_result_before,
               sizeof(authority_result)) == 0);
  context.requested[1].error_case_ordinal =
      measured.error_identity.case_ordinal;

  w_seed_parallel_platform1_completion serial_completions[2];
  w_seed_parallel_platform1_receipt serial_receipt;
  w_seed_parallel_provider0_kind serial_provider_kind =
      W_SEED_PARALLEL_PROVIDER0_KIND_NONE;
  w_seed_parallel_typed_binding1_record serial_records[2];
  w_seed_parallel_typed_binding1_result serial_result;
  const w_seed_parallel_typed_binding1_workspace serial_workspace = {
      serial_completions, 2u, &serial_receipt, &serial_provider_kind};
  const w_seed_parallel_typed_binding1_output serial_output = {serial_records,
                                                               2u};
  CHECK(w_seed_parallel_typed_binding1_run(
            &input, &serial_workspace, &serial_output, &serial_result) ==
            W_SEED_PARALLEL_TYPED_BINDING1_OK &&
        w_seed_parallel_typed_binding1_verify(
            &input, &serial_workspace, &serial_output, &serial_result) &&
        serial_records[0].outcome ==
            W_SEED_PARALLEL_TYPED_BINDING1_OUTCOME_SUCCESS &&
        serial_records[0].success_value == 42 &&
        serial_records[0].call_index == success_call &&
        serial_records[1].outcome ==
            W_SEED_PARALLEL_TYPED_BINDING1_OUTCOME_ERROR &&
        serial_records[1].call_index == error_call &&
        serial_records[0].callee_function != serial_records[1].callee_function &&
        serial_result.primary_error_task == 1u &&
        serial_result.provenance.assurance ==
            W_SEED_PARALLEL_TYPED_BINDING1_ASSURANCE_STATIC_LOCAL_PROVIDER &&
        w_seed_parallel_local_provider1_receipt_equal(
            &serial_result.provenance.local_authority,
            &provider_authority.receipt) &&
        serial_receipt.started_count == 2u &&
        serial_receipt.settled_count == 2u &&
        serial_receipt.cancellation_source_index == 1u &&
        serial_receipt.maximum_active == 1u);

  input.provider_capacity = 2u;
  (void)memset(context.calls, 0, sizeof(context.calls));
  w_seed_parallel_platform1_completion parallel_completions[2];
  w_seed_parallel_platform1_receipt parallel_receipt;
  w_seed_parallel_provider0_kind parallel_provider_kind =
      W_SEED_PARALLEL_PROVIDER0_KIND_NONE;
  w_seed_parallel_typed_binding1_record parallel_records[2];
  w_seed_parallel_typed_binding1_result parallel_result;
  const w_seed_parallel_typed_binding1_workspace parallel_workspace = {
      parallel_completions, 2u, &parallel_receipt, &parallel_provider_kind};
  const w_seed_parallel_typed_binding1_output parallel_output = {
      parallel_records, 2u};
  CHECK(w_seed_parallel_typed_binding1_run(
            &input, &parallel_workspace, &parallel_output, &parallel_result) ==
            W_SEED_PARALLEL_TYPED_BINDING1_OK &&
        w_seed_parallel_typed_binding1_verify(
            &input, &parallel_workspace, &parallel_output, &parallel_result) &&
        memcmp(serial_records, parallel_records, sizeof(serial_records)) == 0 &&
        memcmp(serial_result.semantic_digest, parallel_result.semantic_digest,
               sizeof(serial_result.semantic_digest)) == 0 &&
        memcmp(serial_result.provenance.provenance_digest,
               parallel_result.provenance.provenance_digest,
               sizeof(serial_result.provenance.provenance_digest)) != 0 &&
        parallel_receipt.maximum_active == 2u && context.calls[0] == 1u &&
        context.calls[1] == 1u);

  w_seed_parallel_typed_binding1_input serial_binding_input = input;
  serial_binding_input.provider_capacity = 1u;
  const w_seed_parallel_typed_lifecycle1_input serial_lifecycle_input = {
      &serial_binding_input, &serial_workspace, &serial_output, &serial_result,
      73u};
  const w_seed_parallel_typed_lifecycle1_input parallel_lifecycle_input = {
      &input, &parallel_workspace, &parallel_output, &parallel_result, 73u};
  w_seed_parallel_typed_lifecycle1_counts lifecycle_counts;
  w_seed_parallel_typed_lifecycle1_result lifecycle_measured;
  CHECK(w_seed_parallel_typed_lifecycle1_measure(
            &parallel_lifecycle_input, &lifecycle_counts,
            &lifecycle_measured) == W_SEED_PARALLEL_TYPED_LIFECYCLE1_OK &&
        lifecycle_counts.task_specs == 2u &&
        lifecycle_counts.events == 22u && lifecycle_counts.tasks == 2u &&
        lifecycle_counts.trace_events == 22u &&
        lifecycle_counts.typed_records == 2u &&
        lifecycle_measured.written.events == 0u);
  union {
    w_seed_parallel_typed_lifecycle1_counts counts;
    w_seed_parallel_typed_lifecycle1_result result;
  } lifecycle_measure_alias;
  (void)memset(&lifecycle_measure_alias, 0x91,
               sizeof(lifecycle_measure_alias));
  unsigned char lifecycle_measure_alias_before[
      sizeof(lifecycle_measure_alias)];
  (void)memcpy(lifecycle_measure_alias_before, &lifecycle_measure_alias,
               sizeof(lifecycle_measure_alias));
  CHECK(w_seed_parallel_typed_lifecycle1_measure(
            &parallel_lifecycle_input, &lifecycle_measure_alias.counts,
            &lifecycle_measure_alias.result) ==
            W_SEED_PARALLEL_TYPED_LIFECYCLE1_ALIAS &&
        memcmp(&lifecycle_measure_alias, lifecycle_measure_alias_before,
               sizeof(lifecycle_measure_alias)) == 0);

  w_seed_task_lifecycle0_task_spec serial_lifecycle_specs[2];
  w_seed_task_lifecycle0_event serial_lifecycle_events[32];
  w_seed_task_lifecycle0_task_record serial_lifecycle_reducer[2];
  w_seed_task_lifecycle0_task_record serial_lifecycle_tasks[2];
  w_seed_task_lifecycle0_event serial_lifecycle_trace[32];
  w_seed_parallel_typed_lifecycle1_record serial_typed_records[2];
  const w_seed_parallel_typed_lifecycle1_workspace serial_lifecycle_workspace = {
      serial_lifecycle_specs, 2u, serial_lifecycle_events, 32u,
      serial_lifecycle_reducer, 2u};
  const w_seed_parallel_typed_lifecycle1_output serial_lifecycle_output = {
      serial_lifecycle_tasks, 2u, serial_lifecycle_trace, 32u,
      serial_typed_records, 2u};
  w_seed_parallel_typed_lifecycle1_result serial_lifecycle_result;
  CHECK(w_seed_parallel_typed_lifecycle1_run(
            &serial_lifecycle_input, &serial_lifecycle_workspace,
            &serial_lifecycle_output, &serial_lifecycle_result) ==
            W_SEED_PARALLEL_TYPED_LIFECYCLE1_OK &&
        w_seed_parallel_typed_lifecycle1_verify(
            &serial_lifecycle_input, &serial_lifecycle_workspace,
            &serial_lifecycle_output, &serial_lifecycle_result) &&
        serial_lifecycle_result.primary_error_task == 1u &&
        serial_lifecycle_result.lifecycle.scope.state ==
            W_SEED_TASK_LIFECYCLE0_SCOPE_JOINED &&
        serial_lifecycle_result.lifecycle.scope.outcome.kind ==
            W_SEED_TASK_LIFECYCLE0_OUTCOME_ERROR &&
        serial_lifecycle_tasks[0].outcome.kind ==
            W_SEED_TASK_LIFECYCLE0_OUTCOME_SUCCESS &&
        serial_lifecycle_tasks[0].settled_before_cancellation != 0u &&
        serial_lifecycle_tasks[1].outcome.error_code ==
            W_SEED_TASK_LIFECYCLE0_ERROR_BODY &&
        serial_lifecycle_tasks[1].settled_before_cancellation != 0u &&
        (uint32_t)serial_lifecycle_tasks[1].outcome.error_code !=
            context.requested[1].error_code &&
        serial_lifecycle_trace[7].kind ==
            W_SEED_TASK_LIFECYCLE0_EVENT_TASK_CLEANUP &&
        serial_lifecycle_trace[8].kind ==
            W_SEED_TASK_LIFECYCLE0_EVENT_TASK_OUTCOME_COMMITTED &&
        serial_lifecycle_trace[11].kind ==
            W_SEED_TASK_LIFECYCLE0_EVENT_TASK_CLEANUP &&
        serial_lifecycle_trace[12].kind ==
            W_SEED_TASK_LIFECYCLE0_EVENT_TASK_OUTCOME_COMMITTED &&
        serial_lifecycle_trace[13].kind ==
            W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_CANCELLATION_REQUESTED &&
        serial_lifecycle_trace[13].source_index == 1u &&
        serial_lifecycle_trace[13].reason ==
            W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_ERROR_FAIL_FAST &&
        serial_lifecycle_trace[13].snapshot.generation == 73u &&
        serial_lifecycle_trace[13].snapshot.request_sequence == 13u &&
        serial_lifecycle_trace[13].snapshot.source_index == 1u &&
        serial_lifecycle_trace[13].snapshot.reason ==
            W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_ERROR_FAIL_FAST &&
        serial_typed_records[1].semantic.error_case_ordinal ==
            serial_result.error_identity.case_ordinal &&
        memcmp(serial_typed_records[1].semantic.error_identity_digest,
               serial_result.error_identity.digest,
               sizeof(serial_result.error_identity.digest)) == 0);

  w_seed_task_lifecycle0_task_spec parallel_lifecycle_specs[2];
  w_seed_task_lifecycle0_event parallel_lifecycle_events[32];
  w_seed_task_lifecycle0_task_record parallel_lifecycle_reducer[2];
  w_seed_task_lifecycle0_task_record parallel_lifecycle_tasks[2];
  w_seed_task_lifecycle0_event parallel_lifecycle_trace[32];
  w_seed_parallel_typed_lifecycle1_record parallel_typed_records[2];
  const w_seed_parallel_typed_lifecycle1_workspace
      parallel_lifecycle_workspace = {
          parallel_lifecycle_specs, 2u, parallel_lifecycle_events, 32u,
          parallel_lifecycle_reducer, 2u};
  const w_seed_parallel_typed_lifecycle1_output parallel_lifecycle_output = {
      parallel_lifecycle_tasks, 2u, parallel_lifecycle_trace, 32u,
      parallel_typed_records, 2u};
  w_seed_parallel_typed_lifecycle1_result parallel_lifecycle_result;
  CHECK(w_seed_parallel_typed_lifecycle1_run(
            &parallel_lifecycle_input, &parallel_lifecycle_workspace,
            &parallel_lifecycle_output, &parallel_lifecycle_result) ==
            W_SEED_PARALLEL_TYPED_LIFECYCLE1_OK &&
        w_seed_parallel_typed_lifecycle1_verify(
            &parallel_lifecycle_input, &parallel_lifecycle_workspace,
            &parallel_lifecycle_output, &parallel_lifecycle_result) &&
        memcmp(serial_lifecycle_tasks, parallel_lifecycle_tasks,
               sizeof(serial_lifecycle_tasks)) == 0 &&
        memcmp(serial_lifecycle_trace, parallel_lifecycle_trace,
               lifecycle_counts.trace_events *
                   sizeof(*serial_lifecycle_trace)) == 0 &&
        memcmp(serial_typed_records, parallel_typed_records,
               sizeof(serial_typed_records)) == 0 &&
        memcmp(serial_lifecycle_result.semantic_digest,
               parallel_lifecycle_result.semantic_digest,
               sizeof(serial_lifecycle_result.semantic_digest)) == 0 &&
        memcmp(serial_lifecycle_result.provenance_digest,
               parallel_lifecycle_result.provenance_digest,
               sizeof(serial_lifecycle_result.provenance_digest)) != 0);

  w_seed_task_lifecycle0_task_record lifecycle_sentinel_tasks[2];
  w_seed_task_lifecycle0_event lifecycle_sentinel_trace[32];
  w_seed_parallel_typed_lifecycle1_record lifecycle_sentinel_typed[2];
  w_seed_parallel_typed_lifecycle1_result lifecycle_sentinel_result;
  (void)memset(lifecycle_sentinel_tasks, 0x27,
               sizeof(lifecycle_sentinel_tasks));
  (void)memset(lifecycle_sentinel_trace, 0x72,
               sizeof(lifecycle_sentinel_trace));
  (void)memset(lifecycle_sentinel_typed, 0x4d,
               sizeof(lifecycle_sentinel_typed));
  (void)memset(&lifecycle_sentinel_result, 0xd4,
               sizeof(lifecycle_sentinel_result));
  w_seed_task_lifecycle0_task_record lifecycle_sentinel_tasks_before[2];
  w_seed_task_lifecycle0_event lifecycle_sentinel_trace_before[32];
  w_seed_parallel_typed_lifecycle1_record lifecycle_sentinel_typed_before[2];
  (void)memcpy(lifecycle_sentinel_tasks_before, lifecycle_sentinel_tasks,
               sizeof(lifecycle_sentinel_tasks));
  (void)memcpy(lifecycle_sentinel_trace_before, lifecycle_sentinel_trace,
               sizeof(lifecycle_sentinel_trace));
  (void)memcpy(lifecycle_sentinel_typed_before, lifecycle_sentinel_typed,
               sizeof(lifecycle_sentinel_typed));
  const w_seed_parallel_typed_lifecycle1_result
      lifecycle_sentinel_result_before = lifecycle_sentinel_result;
  const w_seed_parallel_typed_lifecycle1_output short_lifecycle_output = {
      lifecycle_sentinel_tasks, 2u, lifecycle_sentinel_trace, 21u,
      lifecycle_sentinel_typed, 2u};
  CHECK(w_seed_parallel_typed_lifecycle1_run(
            &parallel_lifecycle_input, &parallel_lifecycle_workspace,
            &short_lifecycle_output, &lifecycle_sentinel_result) ==
            W_SEED_PARALLEL_TYPED_LIFECYCLE1_CAPACITY &&
        memcmp(lifecycle_sentinel_tasks, lifecycle_sentinel_tasks_before,
               sizeof(lifecycle_sentinel_tasks)) == 0 &&
        memcmp(lifecycle_sentinel_trace, lifecycle_sentinel_trace_before,
               sizeof(lifecycle_sentinel_trace)) == 0 &&
        memcmp(lifecycle_sentinel_typed, lifecycle_sentinel_typed_before,
               sizeof(lifecycle_sentinel_typed)) == 0 &&
        memcmp(&lifecycle_sentinel_result, &lifecycle_sentinel_result_before,
               sizeof(lifecycle_sentinel_result)) == 0);

  for (size_t short_index = 0u; short_index < 6u; short_index += 1u) {
    typedef struct {
      w_seed_task_lifecycle0_task_spec specs[2];
      w_seed_task_lifecycle0_event events[22];
      w_seed_task_lifecycle0_task_record reducer[2];
      w_seed_task_lifecycle0_task_record tasks[2];
      w_seed_task_lifecycle0_event trace[22];
      w_seed_parallel_typed_lifecycle1_record typed[2];
      w_seed_parallel_typed_lifecycle1_result result;
    } lifecycle_short_state;
    lifecycle_short_state short_state;
    (void)memset(&short_state, 0x6b, sizeof(short_state));
    w_seed_parallel_typed_lifecycle1_workspace short_workspace = {
        short_state.specs, 2u, short_state.events, 22u,
        short_state.reducer, 2u};
    w_seed_parallel_typed_lifecycle1_output short_output = {
        short_state.tasks, 2u, short_state.trace, 22u, short_state.typed, 2u};
    switch (short_index) {
      case 0u:
        short_workspace.task_spec_capacity = 1u;
        break;
      case 1u:
        short_workspace.event_capacity = 21u;
        break;
      case 2u:
        short_workspace.reducer_task_capacity = 1u;
        break;
      case 3u:
        short_output.task_capacity = 1u;
        break;
      case 4u:
        short_output.trace_capacity = 21u;
        break;
      default:
        short_output.typed_record_capacity = 1u;
        break;
    }
    const lifecycle_short_state short_state_before = short_state;
    CHECK(w_seed_parallel_typed_lifecycle1_run(
              &parallel_lifecycle_input, &short_workspace, &short_output,
              &short_state.result) ==
              W_SEED_PARALLEL_TYPED_LIFECYCLE1_CAPACITY &&
          memcmp(&short_state, &short_state_before, sizeof(short_state)) == 0);
  }

  w_seed_parallel_typed_lifecycle1_output alias_lifecycle_output = {
      lifecycle_sentinel_tasks, 2u, lifecycle_sentinel_trace, 32u,
      (w_seed_parallel_typed_lifecycle1_record *)(void *)
          &lifecycle_sentinel_result,
      2u};
  CHECK(w_seed_parallel_typed_lifecycle1_run(
            &parallel_lifecycle_input, &parallel_lifecycle_workspace,
            &alias_lifecycle_output, &lifecycle_sentinel_result) ==
        W_SEED_PARALLEL_TYPED_LIFECYCLE1_ALIAS);

  w_seed_parallel_typed_binding1_record parallel_records_before[2];
  (void)memcpy(parallel_records_before, parallel_records,
               sizeof(parallel_records));
  alias_lifecycle_output = (w_seed_parallel_typed_lifecycle1_output){
      (w_seed_task_lifecycle0_task_record *)(void *)parallel_records, 2u,
      lifecycle_sentinel_trace, 32u, lifecycle_sentinel_typed, 2u};
  lifecycle_sentinel_result = lifecycle_sentinel_result_before;
  CHECK(w_seed_parallel_typed_lifecycle1_run(
            &parallel_lifecycle_input, &parallel_lifecycle_workspace,
            &alias_lifecycle_output, &lifecycle_sentinel_result) ==
            W_SEED_PARALLEL_TYPED_LIFECYCLE1_ALIAS &&
        memcmp(parallel_records, parallel_records_before,
               sizeof(parallel_records)) == 0 &&
        memcmp(lifecycle_sentinel_trace, lifecycle_sentinel_trace_before,
               sizeof(lifecycle_sentinel_trace)) == 0 &&
        memcmp(lifecycle_sentinel_typed, lifecycle_sentinel_typed_before,
               sizeof(lifecycle_sentinel_typed)) == 0 &&
        memcmp(&lifecycle_sentinel_result, &lifecycle_sentinel_result_before,
               sizeof(lifecycle_sentinel_result)) == 0);

  parallel_typed_records[1].semantic.error_case_ordinal += 1u;
  CHECK(!w_seed_parallel_typed_lifecycle1_verify(
      &parallel_lifecycle_input, &parallel_lifecycle_workspace,
      &parallel_lifecycle_output, &parallel_lifecycle_result));
  parallel_typed_records[1].semantic.error_case_ordinal -= 1u;
  parallel_lifecycle_result.error_identity.digest[0] ^= 1u;
  CHECK(!w_seed_parallel_typed_lifecycle1_verify(
      &parallel_lifecycle_input, &parallel_lifecycle_workspace,
      &parallel_lifecycle_output, &parallel_lifecycle_result));
  parallel_lifecycle_result.error_identity.digest[0] ^= 1u;

  w_seed_parallel_typed_lifecycle1_counts rejected_lifecycle_counts;
  w_seed_parallel_typed_lifecycle1_result rejected_lifecycle_result;
  (void)memset(&rejected_lifecycle_counts, 0x38,
               sizeof(rejected_lifecycle_counts));
  (void)memset(&rejected_lifecycle_result, 0x83,
               sizeof(rejected_lifecycle_result));
  const w_seed_parallel_typed_lifecycle1_counts
      rejected_lifecycle_counts_before = rejected_lifecycle_counts;
  const w_seed_parallel_typed_lifecycle1_result
      rejected_lifecycle_result_before = rejected_lifecycle_result;
  parallel_result.semantic_digest[0] ^= 1u;
  CHECK(w_seed_parallel_typed_lifecycle1_measure(
            &parallel_lifecycle_input, &rejected_lifecycle_counts,
            &rejected_lifecycle_result) ==
            W_SEED_PARALLEL_TYPED_LIFECYCLE1_UPSTREAM &&
        memcmp(&rejected_lifecycle_counts,
               &rejected_lifecycle_counts_before,
               sizeof(rejected_lifecycle_counts)) == 0 &&
        memcmp(&rejected_lifecycle_result,
               &rejected_lifecycle_result_before,
               sizeof(rejected_lifecycle_result)) == 0);
  parallel_result.semantic_digest[0] ^= 1u;

  w_seed_parallel_typed_lifecycle1_input overflow_lifecycle_input =
      parallel_lifecycle_input;
  overflow_lifecycle_input.scope_generation = UINT32_MAX - 1u;
  CHECK(w_seed_parallel_typed_lifecycle1_measure(
            &overflow_lifecycle_input, &rejected_lifecycle_counts,
            &rejected_lifecycle_result) ==
            W_SEED_PARALLEL_TYPED_LIFECYCLE1_UPSTREAM &&
        memcmp(&rejected_lifecycle_counts,
               &rejected_lifecycle_counts_before,
               sizeof(rejected_lifecycle_counts)) == 0 &&
        memcmp(&rejected_lifecycle_result,
               &rejected_lifecycle_result_before,
               sizeof(rejected_lifecycle_result)) == 0);

  w_seed_parallel_typed_binding1_result sentinel_result;
  w_seed_parallel_typed_binding1_record sentinel_records[2];
  w_seed_parallel_platform1_completion sentinel_completions[2];
  w_seed_parallel_platform1_receipt sentinel_receipt;
  w_seed_parallel_provider0_kind sentinel_kind =
      W_SEED_PARALLEL_PROVIDER0_KIND_NONE;
  (void)memset(&sentinel_result, 0xa5, sizeof(sentinel_result));
  (void)memset(sentinel_records, 0x5a, sizeof(sentinel_records));
  (void)memset(sentinel_completions, 0x3c, sizeof(sentinel_completions));
  (void)memset(&sentinel_receipt, 0xc3, sizeof(sentinel_receipt));
  const w_seed_parallel_typed_binding1_result sentinel_result_before =
      sentinel_result;
  w_seed_parallel_typed_binding1_record sentinel_records_before[2];
  w_seed_parallel_platform1_completion sentinel_completions_before[2];
  (void)memcpy(sentinel_records_before, sentinel_records,
               sizeof(sentinel_records));
  (void)memcpy(sentinel_completions_before, sentinel_completions,
               sizeof(sentinel_completions));
  const w_seed_parallel_platform1_receipt sentinel_receipt_before =
      sentinel_receipt;
  const w_seed_parallel_typed_binding1_workspace sentinel_workspace = {
      sentinel_completions, 2u, &sentinel_receipt, &sentinel_kind};
  const w_seed_parallel_typed_binding1_output short_output = {sentinel_records,
                                                              1u};
  const w_seed_parallel_typed_binding1_output sentinel_output = {
      sentinel_records, 2u};
  CHECK(w_seed_parallel_typed_binding1_run(
            &input, &sentinel_workspace, &short_output, &sentinel_result) ==
            W_SEED_PARALLEL_TYPED_BINDING1_CAPACITY &&
        memcmp(&sentinel_result, &sentinel_result_before,
               sizeof(sentinel_result)) == 0 &&
        memcmp(sentinel_records, sentinel_records_before,
               sizeof(sentinel_records)) == 0 &&
        memcmp(sentinel_completions, sentinel_completions_before,
               sizeof(sentinel_completions)) == 0 &&
        memcmp(&sentinel_receipt, &sentinel_receipt_before,
               sizeof(sentinel_receipt)) == 0);

  w_seed_parallel_typed_binding1_output alias_output = {
      (w_seed_parallel_typed_binding1_record *)(void *)&sentinel_result, 2u};
  CHECK(w_seed_parallel_typed_binding1_run(
            &input, &sentinel_workspace, &alias_output, &sentinel_result) ==
        W_SEED_PARALLEL_TYPED_BINDING1_ALIAS);

  struct {
    w_seed_parallel_local_provider1_authority authority;
    unsigned char padding[
        sizeof(w_seed_parallel_typed_binding1_result) +
        2u * sizeof(w_seed_parallel_typed_binding1_record)];
  } authority_alias_state;
  (void)memset(&authority_alias_state, 0x6a,
               sizeof(authority_alias_state));
  authority_alias_state.authority = provider_authority;
  unsigned char authority_alias_before[sizeof(authority_alias_state)];
  (void)memcpy(authority_alias_before, &authority_alias_state,
               sizeof(authority_alias_state));
  w_seed_parallel_typed_binding1_input authority_alias_input = input;
  authority_alias_input.provider_authority = &authority_alias_state.authority;
  alias_output.records =
      (w_seed_parallel_typed_binding1_record *)(void *)&authority_alias_state;
  CHECK(w_seed_parallel_typed_binding1_run(
            &authority_alias_input, &sentinel_workspace, &alias_output,
            &sentinel_result) == W_SEED_PARALLEL_TYPED_BINDING1_ALIAS &&
        memcmp(&authority_alias_state, authority_alias_before,
               sizeof(authority_alias_state)) == 0 &&
        memcmp(&sentinel_result, &sentinel_result_before,
               sizeof(sentinel_result)) == 0);
  alias_output = sentinel_output;
  CHECK(w_seed_parallel_typed_binding1_run(
            &authority_alias_input, &sentinel_workspace, &alias_output,
            (w_seed_parallel_typed_binding1_result *)(void *)
                &authority_alias_state.authority) ==
            W_SEED_PARALLEL_TYPED_BINDING1_ALIAS &&
        memcmp(&authority_alias_state, authority_alias_before,
               sizeof(authority_alias_state)) == 0 &&
        memcmp(sentinel_records, sentinel_records_before,
               sizeof(sentinel_records)) == 0);

  context.fail = true;
  CHECK(w_seed_parallel_typed_binding1_run(
            &input, &sentinel_workspace, &sentinel_output, &sentinel_result) ==
            W_SEED_PARALLEL_TYPED_BINDING1_TASK_FAILURE &&
        memcmp(&sentinel_result, &sentinel_result_before,
               sizeof(sentinel_result)) == 0 &&
        memcmp(sentinel_records, sentinel_records_before,
               sizeof(sentinel_records)) == 0 &&
        memcmp(sentinel_completions, sentinel_completions_before,
               sizeof(sentinel_completions)) == 0);
  context.fail = false;

  context.requested[0].success_value = 43;
  CHECK(w_seed_parallel_typed_binding1_run(
            &input, &sentinel_workspace, &sentinel_output, &sentinel_result) ==
            W_SEED_PARALLEL_TYPED_BINDING1_PROVIDER_FAILURE &&
        memcmp(&sentinel_result, &sentinel_result_before,
               sizeof(sentinel_result)) == 0 &&
        memcmp(sentinel_records, sentinel_records_before,
               sizeof(sentinel_records)) == 0 &&
        memcmp(sentinel_completions, sentinel_completions_before,
               sizeof(sentinel_completions)) == 0);
  context.requested[0].success_value = 42;

  context.requested[1].error_case_ordinal += 1u;
  CHECK(w_seed_parallel_typed_binding1_run(
            &input, &sentinel_workspace, &sentinel_output, &sentinel_result) ==
            W_SEED_PARALLEL_TYPED_BINDING1_PROVIDER_FAILURE &&
        memcmp(&sentinel_result, &sentinel_result_before,
               sizeof(sentinel_result)) == 0 &&
        memcmp(sentinel_records, sentinel_records_before,
               sizeof(sentinel_records)) == 0);
  context.requested[1].error_case_ordinal -= 1u;

  context.requested[1] = (w_seed_parallel_platform1_completion){
      .kind = W_SEED_PARALLEL_PLATFORM1_COMPLETION_CANCELED,
      .cancel_reason = 9u};
  CHECK(w_seed_parallel_typed_binding1_run(
            &input, &sentinel_workspace, &sentinel_output, &sentinel_result) ==
            W_SEED_PARALLEL_TYPED_BINDING1_CANCELED &&
        memcmp(&sentinel_result, &sentinel_result_before,
               sizeof(sentinel_result)) == 0 &&
        memcmp(sentinel_records, sentinel_records_before,
               sizeof(sentinel_records)) == 0);

  context.requested[1] = (w_seed_parallel_platform1_completion){
      .kind = W_SEED_PARALLEL_PLATFORM1_COMPLETION_PANIC,
      .panic_code = W_SEED_PARALLEL_PLATFORM1_PANIC_EXPLICIT};
  CHECK(w_seed_parallel_typed_binding1_run(
            &input, &sentinel_workspace, &sentinel_output, &sentinel_result) ==
            W_SEED_PARALLEL_TYPED_BINDING1_PANIC &&
        memcmp(&sentinel_result, &sentinel_result_before,
               sizeof(sentinel_result)) == 0 &&
        memcmp(sentinel_records, sentinel_records_before,
               sizeof(sentinel_records)) == 0 &&
        memcmp(sentinel_completions, sentinel_completions_before,
               sizeof(sentinel_completions)) == 0 &&
        memcmp(&sentinel_receipt, &sentinel_receipt_before,
               sizeof(sentinel_receipt)) == 0 &&
        sentinel_kind == W_SEED_PARALLEL_PROVIDER0_KIND_NONE);
  context.requested[1] = (w_seed_parallel_platform1_completion){
      .kind = W_SEED_PARALLEL_PLATFORM1_COMPLETION_ERROR,
      .error_code = 17u,
      .error_case_ordinal = measured.error_identity.case_ordinal};

  w_seed_parallel_typed_binding1_record forged_records[2];
  (void)memcpy(forged_records, parallel_records, sizeof(forged_records));
  forged_records[0].call_index = error_call;
  const w_seed_parallel_typed_binding1_output forged_output = {forged_records,
                                                               2u};
  CHECK(!w_seed_parallel_typed_binding1_verify(
      &input, &parallel_workspace, &forged_output, &parallel_result));
  parallel_completions[1].error_code += 1u;
  CHECK(!w_seed_parallel_typed_binding1_verify(
      &input, &parallel_workspace, &parallel_output, &parallel_result));
  parallel_completions[1].error_code -= 1u;
  parallel_result.semantic_digest[0] ^= 1u;
  CHECK(!w_seed_parallel_typed_binding1_verify(
      &input, &parallel_workspace, &parallel_output, &parallel_result));
  parallel_result.semantic_digest[0] ^= 1u;
  parallel_result.provenance.local_authority.contract_digest[0] ^= 1u;
  CHECK(!w_seed_parallel_typed_binding1_verify(
      &input, &parallel_workspace, &parallel_output, &parallel_result));
  parallel_result.provenance.local_authority.contract_digest[0] ^= 1u;
  parallel_result.provenance.assurance =
      W_SEED_PARALLEL_TYPED_BINDING1_ASSURANCE_EXECUTION_INTEGRITY;
  CHECK(!w_seed_parallel_typed_binding1_verify(
      &input, &parallel_workspace, &parallel_output, &parallel_result));
  parallel_result.provenance.assurance =
      W_SEED_PARALLEL_TYPED_BINDING1_ASSURANCE_STATIC_LOCAL_PROVIDER;

  /* The typed physical binding is deliberately a HIR41 witness: a typed
   * invoke without its exact dual-path cleanup must fail before publication. */
  static const char NO_CLEANUP_SOURCE[] =
      "enum Failure: Error { denied }\n"
      "fn succeed(): i64 { return 42 }\n"
      "fn successCaller(): i64 { return succeed() }\n"
      "fn leaf(): i64 throws Failure { throw .denied }\n"
      "fn relay(): i64 throws Failure { return try leaf() }\n"
      "entry { }\n";
  CHECK(lower(NO_CLEANUP_SOURCE));
  program = &fixture.hir_program;
  const uint32_t no_cleanup_success_caller =
      typed_binding1_function_named(program, "successCaller");
  const uint32_t no_cleanup_relay =
      typed_binding1_function_named(program, "relay");
  CHECK(no_cleanup_success_caller != W_SEED_HIR0_NONE &&
        no_cleanup_relay != W_SEED_HIR0_NONE);
  const w_seed_hir0_function *no_cleanup_success_function =
      &program->functions[no_cleanup_success_caller];
  const w_seed_hir0_function *no_cleanup_relay_function =
      &program->functions[no_cleanup_relay];
  CHECK(no_cleanup_success_function->first_block < program->block_count &&
        no_cleanup_relay_function->first_block < program->block_count);
  const w_seed_hir0_block *no_cleanup_success_block =
      &program->blocks[no_cleanup_success_function->first_block];
  CHECK(no_cleanup_success_block->first_instruction <
        program->instruction_count);
  const uint32_t no_cleanup_success_call =
      program->instructions[no_cleanup_success_block->first_instruction]
          .call_index;
  const uint32_t no_cleanup_invoke =
      program->blocks[no_cleanup_relay_function->first_block]
          .terminator_index;
  CHECK(no_cleanup_success_call < program->call_count &&
        no_cleanup_invoke < program->terminator_count &&
        program->terminators[no_cleanup_invoke].call_index <
            program->call_count);
  const w_seed_parallel_typed_binding1_task no_cleanup_tasks[2] = {
      {.lexical_index = 0u, .call_index = no_cleanup_success_call},
      {.lexical_index = 1u,
       .call_index = program->terminators[no_cleanup_invoke].call_index}};
  input.hir_program = program;
  input.hir_result = &fixture.hir_result;
  input.invoke_terminator = no_cleanup_invoke;
  input.tasks = no_cleanup_tasks;
  w_seed_parallel_typed_binding1_counts rejected_counts;
  w_seed_parallel_typed_binding1_result rejected_result;
  (void)memset(&rejected_counts, 0x6b, sizeof(rejected_counts));
  (void)memset(&rejected_result, 0xb6, sizeof(rejected_result));
  const w_seed_parallel_typed_binding1_counts rejected_counts_before =
      rejected_counts;
  const w_seed_parallel_typed_binding1_result rejected_result_before =
      rejected_result;
  CHECK(w_seed_parallel_typed_binding1_measure(
            &input, &rejected_counts, &rejected_result) ==
            W_SEED_PARALLEL_TYPED_BINDING1_HIR &&
        memcmp(&rejected_counts, &rejected_counts_before,
               sizeof(rejected_counts)) == 0 &&
        memcmp(&rejected_result, &rejected_result_before,
               sizeof(rejected_result)) == 0);
  return true;
}
#else
static bool test_parallel_typed_binding1(void) { return true; }
#endif

static bool test_local_enum_payload_declarations_hir(void) {
  static const char SOURCE[] =
      "enum Course { starter main(price: i64) shared(i64, i64) }\n"
      "entry { }\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(fixture.result.written.enums == 1u &&
        fixture.result.written.enum_cases == 3u &&
        fixture.result.written.enum_case_parameters == 3u &&
        fixture.hir_counts.enums == 1u && fixture.hir_counts.enum_cases == 3u &&
        fixture.hir_counts.enum_case_parameters == 3u &&
        program->enum_count == 1u && program->enum_case_count == 3u &&
        program->enum_case_parameter_count == 3u);

  CHECK(program->enum_cases[0].first_payload == 0u &&
        program->enum_cases[0].payload_count == 0u &&
        program->enum_cases[1].first_payload == 0u &&
        program->enum_cases[1].payload_count == 1u &&
        program->enum_cases[2].first_payload == 1u &&
        program->enum_cases[2].payload_count == 2u);
  CHECK(program->enum_case_parameters[0].owner_case == 1u &&
        program->enum_case_parameters[0].ordinal == 0u &&
        program->enum_case_parameters[0].type_index == 2u &&
        program->enum_case_parameters[0].has_label &&
        hir_text_is(program, program->enum_case_parameters[0].label, "price"));
  for (size_t ordinal = 0u; ordinal < 2u; ordinal += 1u) {
    const w_seed_hir0_enum_case_parameter *payload =
        &program->enum_case_parameters[1u + ordinal];
    CHECK(payload->owner_case == 2u && payload->ordinal == ordinal &&
          payload->type_index == 2u && !payload->has_label &&
          payload->label.count == 0u);
  }
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_enum_case saved_case = fixture.hir_enum_cases[1];
  fixture.hir_enum_cases[1].first_payload = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_cases[1] = saved_case;

  const w_seed_hir0_enum_case_parameter saved_payload =
      fixture.hir_enum_case_parameters[0];
  fixture.hir_enum_case_parameters[0].owner_case = 2u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_case_parameters[0] = saved_payload;
  fixture.hir_enum_case_parameters[0].ordinal = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_case_parameters[0] = saved_payload;
  fixture.hir_enum_case_parameters[0].type_index = 3u;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_case_parameters[0] = saved_payload;
  fixture.hir_enum_case_parameters[0].type_index = 5u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_case_parameters[0] = saved_payload;
  fixture.hir_enum_case_parameters[0].has_label = false;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_case_parameters[0] = saved_payload;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_input input = hir_input();
  const uint8_t sentinel = 0x6bu;
  w_seed_hir0_result rejected;
  (void)memset(&rejected, 0x42, sizeof(rejected));
  const w_seed_hir0_result rejected_before = rejected;
  setup_hir_output();
  fill_hir_output(sentinel);
  fixture.hir_output.enum_case_parameter_capacity =
      fixture.hir_counts.enum_case_parameters - 1u;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(sentinel) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  setup_hir_output();
  fill_hir_output(sentinel);
  w_seed_hir0_output alias = fixture.hir_output;
  alias.enum_case_parameters =
      (w_seed_hir0_enum_case_parameter *)(void *)alias.enum_cases;
  rejected = rejected_before;
  CHECK(w_seed_hir0_run(&input, &alias, &rejected) == W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(sentinel) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  CHECK(fixture_frontend(
      "enum Message { text(value: String) }\nentry { }\n"));
  setup_hir_output();
  const w_seed_hir0_input unsupported = hir_input();
  w_seed_hir0_counts unsupported_counts;
  w_seed_hir0_result unsupported_result;
  CHECK(w_seed_hir0_measure(&unsupported, &unsupported_counts,
                            &unsupported_result) == W_SEED_HIR0_UNSUPPORTED);
  return true;
}

static bool test_local_enum_payload_constructor_hir(void) {
  static const char SOURCE[] =
      "enum Outcome { idle pair(left: i64, right: i64) }\n"
      "fn make(left: i64, right: i64): Outcome { "
      "return .pair(right: right, left: left) }\n"
      "entry { }\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->enum_count == 1u && program->enum_case_count == 2u &&
        program->enum_case_parameter_count == 2u &&
        program->call_count == 0u && program->argument_count == 0u &&
        program->enum_payload_count == 2u && program->value_count == 3u);
  const w_seed_hir0_terminator *terminator = &program->terminators[0];
  CHECK(terminator->kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        terminator->value_index == 2u);
  const w_seed_hir0_value *constructed =
      &program->values[terminator->value_index];
  CHECK(constructed->kind == W_SEED_HIR0_VALUE_ENUM_CASE &&
        constructed->enum_index == 0u && constructed->enum_case_index == 1u &&
        constructed->first_enum_payload == 0u &&
        constructed->enum_payload_count == 2u);
  CHECK(program->enum_payloads[0].owner_value == 2u &&
        program->enum_payloads[0].ordinal == 0u &&
        program->enum_payloads[0].parameter_ordinal == 1u &&
        program->enum_payloads[0].value_index == 0u &&
        program->values[0].kind == W_SEED_HIR0_VALUE_PARAMETER_READ &&
        program->values[0].parameter_index == 1u &&
        program->enum_payloads[1].owner_value == 2u &&
        program->enum_payloads[1].ordinal == 1u &&
        program->enum_payloads[1].parameter_ordinal == 0u &&
        program->enum_payloads[1].value_index == 1u &&
        program->values[1].kind == W_SEED_HIR0_VALUE_PARAMETER_READ &&
        program->values[1].parameter_index == 0u);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_enum_payload saved_payload = fixture.hir_enum_payloads[0];
  fixture.hir_enum_payloads[0].owner_value = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_payloads[0] = saved_payload;
  fixture.hir_enum_payloads[0].parameter_ordinal = 0u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_payloads[0] = saved_payload;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_input input = hir_input();
  const uint8_t sentinel = 0x6cu;
  w_seed_hir0_result rejected;
  (void)memset(&rejected, 0x43, sizeof(rejected));
  const w_seed_hir0_result rejected_before = rejected;
  setup_hir_output();
  fill_hir_output(sentinel);
  fixture.hir_output.enum_payload_capacity = 1u;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(sentinel) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  setup_hir_output();
  fill_hir_output(sentinel);
  w_seed_hir0_output alias = fixture.hir_output;
  alias.enum_payloads =
      (w_seed_hir0_enum_payload *)(void *)alias.values;
  rejected = rejected_before;
  CHECK(w_seed_hir0_run(&input, &alias, &rejected) == W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(sentinel) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);
  return true;
}

static w_seed_hir0_input hir_input(void) {
  return (w_seed_hir0_input){
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
}

static void fill_hir_output(uint8_t value) {
  (void)memset(fixture.hir_modules, value, sizeof(fixture.hir_modules));
  (void)memset(fixture.hir_identities, value, sizeof(fixture.hir_identities));
  (void)memset(fixture.hir_types, value, sizeof(fixture.hir_types));
  (void)memset(fixture.hir_enums, value, sizeof(fixture.hir_enums));
  (void)memset(fixture.hir_enum_cases, value, sizeof(fixture.hir_enum_cases));
  (void)memset(fixture.hir_enum_case_parameters, value,
               sizeof(fixture.hir_enum_case_parameters));
  (void)memset(fixture.hir_enum_subset_members, value,
               sizeof(fixture.hir_enum_subset_members));
  (void)memset(fixture.hir_functions, value, sizeof(fixture.hir_functions));
  (void)memset(fixture.hir_parameters, value, sizeof(fixture.hir_parameters));
  (void)memset(fixture.hir_blocks, value, sizeof(fixture.hir_blocks));
  (void)memset(fixture.hir_block_arguments, value,
               sizeof(fixture.hir_block_arguments));
  (void)memset(fixture.hir_edge_arguments, value,
               sizeof(fixture.hir_edge_arguments));
  (void)memset(fixture.hir_switch_edges, value,
               sizeof(fixture.hir_switch_edges));
  (void)memset(fixture.hir_switch_captures, value,
               sizeof(fixture.hir_switch_captures));
  (void)memset(fixture.hir_instructions, value,
               sizeof(fixture.hir_instructions));
  (void)memset(fixture.hir_bindings, value, sizeof(fixture.hir_bindings));
  (void)memset(fixture.hir_calls, value, sizeof(fixture.hir_calls));
  (void)memset(fixture.hir_host_parameters, value,
               sizeof(fixture.hir_host_parameters));
  (void)memset(fixture.hir_arguments, value, sizeof(fixture.hir_arguments));
  (void)memset(fixture.hir_enum_payloads, value,
               sizeof(fixture.hir_enum_payloads));
  (void)memset(fixture.hir_requirements, value,
               sizeof(fixture.hir_requirements));
  (void)memset(fixture.hir_values, value, sizeof(fixture.hir_values));
  (void)memset(fixture.hir_interpolation_segments, value,
               sizeof(fixture.hir_interpolation_segments));
  (void)memset(fixture.hir_terminators, value,
               sizeof(fixture.hir_terminators));
  (void)memset(fixture.hir_entries, value, sizeof(fixture.hir_entries));
  (void)memset(fixture.hir_external_modules, value,
               sizeof(fixture.hir_external_modules));
  (void)memset(fixture.hir_external_symbols, value,
               sizeof(fixture.hir_external_symbols));
  (void)memset(fixture.hir_cleanups, value, sizeof(fixture.hir_cleanups));
  (void)memset(fixture.hir_text, value, sizeof(fixture.hir_text));
  (void)memset(fixture.hir_value_bytes, value,
               sizeof(fixture.hir_value_bytes));
  (void)memset(fixture.hir_receipt, value, sizeof(fixture.hir_receipt));
}

static bool hir_output_is_byte(uint8_t value) {
#define CHECK_BYTES(field)                                                     \
  CHECK(memchr(fixture.field, (int)value, sizeof(fixture.field)) != NULL ||    \
        sizeof(fixture.field) == 0u)
  /* A capacity rejection must preserve a uniform sentinel in every region. */
#undef CHECK_BYTES
  const uint8_t *regions[] = {
      (const uint8_t *)fixture.hir_modules,
      (const uint8_t *)fixture.hir_identities,
      (const uint8_t *)fixture.hir_types,
      (const uint8_t *)fixture.hir_enums,
      (const uint8_t *)fixture.hir_enum_cases,
      (const uint8_t *)fixture.hir_enum_case_parameters,
      (const uint8_t *)fixture.hir_enum_subset_members,
      (const uint8_t *)fixture.hir_functions,
      (const uint8_t *)fixture.hir_parameters,
      (const uint8_t *)fixture.hir_blocks,
      (const uint8_t *)fixture.hir_block_arguments,
      (const uint8_t *)fixture.hir_edge_arguments,
      (const uint8_t *)fixture.hir_switch_edges,
      (const uint8_t *)fixture.hir_switch_captures,
      (const uint8_t *)fixture.hir_instructions,
      (const uint8_t *)fixture.hir_bindings,
      (const uint8_t *)fixture.hir_calls,
      (const uint8_t *)fixture.hir_host_parameters,
      (const uint8_t *)fixture.hir_arguments,
      (const uint8_t *)fixture.hir_enum_payloads,
      (const uint8_t *)fixture.hir_requirements,
      (const uint8_t *)fixture.hir_values,
      (const uint8_t *)fixture.hir_interpolation_segments,
      (const uint8_t *)fixture.hir_terminators,
      (const uint8_t *)fixture.hir_entries,
      (const uint8_t *)fixture.hir_external_modules,
      (const uint8_t *)fixture.hir_external_symbols,
      (const uint8_t *)fixture.hir_cleanups,
      fixture.hir_text,
      fixture.hir_value_bytes,
      fixture.hir_receipt};
  const size_t sizes[] = {
      sizeof(fixture.hir_modules), sizeof(fixture.hir_identities),
      sizeof(fixture.hir_types), sizeof(fixture.hir_enums),
      sizeof(fixture.hir_enum_cases),
      sizeof(fixture.hir_enum_case_parameters),
      sizeof(fixture.hir_enum_subset_members),
      sizeof(fixture.hir_functions),
      sizeof(fixture.hir_parameters), sizeof(fixture.hir_blocks),
      sizeof(fixture.hir_block_arguments),
      sizeof(fixture.hir_edge_arguments),
      sizeof(fixture.hir_switch_edges),
      sizeof(fixture.hir_switch_captures),
      sizeof(fixture.hir_instructions), sizeof(fixture.hir_bindings),
      sizeof(fixture.hir_calls),
      sizeof(fixture.hir_host_parameters), sizeof(fixture.hir_arguments),
      sizeof(fixture.hir_enum_payloads),
      sizeof(fixture.hir_requirements), sizeof(fixture.hir_values),
      sizeof(fixture.hir_interpolation_segments),
      sizeof(fixture.hir_terminators), sizeof(fixture.hir_entries),
      sizeof(fixture.hir_external_modules),
      sizeof(fixture.hir_external_symbols),
      sizeof(fixture.hir_cleanups),
      sizeof(fixture.hir_text), sizeof(fixture.hir_value_bytes),
      sizeof(fixture.hir_receipt)};
  for (size_t region = 0u; region < sizeof(sizes) / sizeof(sizes[0]);
       region += 1u) {
    for (size_t byte = 0u; byte < sizes[region]; byte += 1u)
      if (regions[region][byte] != value) return false;
  }
  return true;
}

typedef enum {
  PROCESS_BAD_EXTERNAL_MODULE_COUNT,
  PROCESS_BAD_EXTERNAL_SYMBOL_COUNT,
  PROCESS_BAD_EXTERNAL_MODULE_ID,
  PROCESS_BAD_EXTERNAL_SYMBOL_NAME,
  PROCESS_BAD_EXTERNAL_SYMBOL_KIND,
  PROCESS_BAD_EXTERNAL_SYMBOL_EXPORT,
  PROCESS_BAD_EXTERNAL_SYMBOL_CONST,
  PROCESS_BAD_EXTERNAL_SYMBOL_RECEIVER,
  PROCESS_BAD_EXTERNAL_SYMBOL_RETURN,
  PROCESS_BAD_EXTERNAL_IMPORT_ITEM,
  PROCESS_BAD_EXTERNAL_TYPE_IDENTITY,
  PROCESS_BAD_EXTERNAL_CASE_IDENTITY,
  PROCESS_BAD_EXTERNAL_CASE_MEMBER,
  PROCESS_BAD_EXTERNAL_RETURN_EXPRESSION_MAX,
  PROCESS_BAD_EXTERNAL_RETURN_EXPRESSION_COUNT,
  PROCESS_BAD_PROCESS_PROFILE,
  PROCESS_BAD_HANDLER_ARITY,
  PROCESS_BAD_HANDLER_ORDER,
  PROCESS_BAD_HANDLER_ASYNC,
  PROCESS_BAD_HANDLER_RETURN,
  PROCESS_BAD_HANDLER_CONST,
  PROCESS_BAD_HANDLER_THROWS,
  PROCESS_BAD_HANDLER_UNSAFE,
  PROCESS_BAD_HANDLER_BORROW,
  PROCESS_BAD_HANDLER_ANONYMOUS,
} process_bad_frontend_case;

static w_seed_frontend_expression *process_return_expression(void) {
  const w_seed_frontend_function *function = &fixture.functions[0];
  const w_seed_frontend_statement *statement =
      &fixture.statements[function->first_statement];
  return &fixture.expressions[statement->expression_index];
}

static bool expect_process_frontend_rejected(process_bad_frontend_case bad) {
  static const char SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return .success }\n"
      "entry(run)\n";
  CHECK(fixture_process_frontend(SOURCE));
  switch (bad) {
    case PROCESS_BAD_EXTERNAL_MODULE_COUNT:
      fixture.input.external_module_count = 2u;
      break;
    case PROCESS_BAD_EXTERNAL_SYMBOL_COUNT:
      fixture.external_modules[0].symbol_count = 5u;
      break;
    case PROCESS_BAD_EXTERNAL_MODULE_ID:
      fixture.external_modules[0].module_id =
          (w_seed_frontend_text){"std.other", 9u};
      break;
    case PROCESS_BAD_EXTERNAL_SYMBOL_NAME:
      fixture.external_symbols[0].name =
          (w_seed_frontend_text){"Wrong", 5u};
      break;
    case PROCESS_BAD_EXTERNAL_SYMBOL_KIND:
      fixture.external_symbols[0].kind = W_SEED_FRONTEND_EXTERNAL_VALUE;
      break;
    case PROCESS_BAD_EXTERNAL_SYMBOL_EXPORT:
      fixture.external_symbols[2].exported = false;
      break;
    case PROCESS_BAD_EXTERNAL_SYMBOL_CONST:
      fixture.external_symbols[3].is_const = false;
      break;
    case PROCESS_BAD_EXTERNAL_SYMBOL_RECEIVER:
      fixture.external_symbols[3].receiver_type =
          (w_seed_frontend_text){"Context", 7u};
      break;
    case PROCESS_BAD_EXTERNAL_SYMBOL_RETURN:
      fixture.external_symbols[3].return_type =
          (w_seed_frontend_text){"Context", 7u};
      break;
    case PROCESS_BAD_EXTERNAL_IMPORT_ITEM:
      fixture.import_items[0].name = (w_seed_frontend_text){"Wrong", 5u};
      break;
    case PROCESS_BAD_EXTERNAL_TYPE_IDENTITY:
      fixture.types[0].external_symbol_index = 1u;
      break;
    case PROCESS_BAD_EXTERNAL_CASE_IDENTITY:
      process_return_expression()->resolved_external_symbol_index = 2u;
      break;
    case PROCESS_BAD_EXTERNAL_CASE_MEMBER:
      process_return_expression()->member_name =
          (w_seed_frontend_text){"failure", 7u};
      break;
    case PROCESS_BAD_EXTERNAL_RETURN_EXPRESSION_MAX:
      fixture.statements[0].expression_index = UINT32_MAX;
      break;
    case PROCESS_BAD_EXTERNAL_RETURN_EXPRESSION_COUNT:
      fixture.statements[0].expression_index =
          (uint32_t)fixture.result.written.expressions;
      break;
    case PROCESS_BAD_PROCESS_PROFILE:
      fixture.host_scope.profile = (w_seed_frontend_text){"bogus", 5u};
      break;
    case PROCESS_BAD_HANDLER_ARITY:
      fixture.functions[0].parameter_count = 1u;
      break;
    case PROCESS_BAD_HANDLER_ORDER:
      fixture.parameters[0].type_index = 5u;
      break;
    case PROCESS_BAD_HANDLER_ASYNC:
      fixture.functions[0].is_async = false;
      break;
    case PROCESS_BAD_HANDLER_RETURN:
      fixture.functions[0].return_type = 4u;
      break;
    case PROCESS_BAD_HANDLER_CONST:
      fixture.functions[0].is_const = true;
      break;
    case PROCESS_BAD_HANDLER_THROWS:
      fixture.functions[0].is_throws = true;
      break;
    case PROCESS_BAD_HANDLER_UNSAFE:
      fixture.functions[0].is_unsafe = true;
      break;
    case PROCESS_BAD_HANDLER_BORROW:
      fixture.functions[0].has_borrow_clause = true;
      break;
    case PROCESS_BAD_HANDLER_ANONYMOUS:
      fixture.functions[0].is_anonymous_entry = true;
      fixture.entries[0].is_body = true;
      break;
  }
  setup_hir_output();
  fill_hir_output(0xa5u);
  const w_seed_hir0_input input = hir_input();
  const w_seed_hir0_result result_before = fixture.hir_result;
  const w_seed_hir0_status status =
      w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result);
  if (status == W_SEED_HIR0_OK)
    (void)fprintf(stderr, "process bad case unexpectedly accepted: %d\n",
                  (int)bad);
  CHECK(status != W_SEED_HIR0_OK);
  CHECK(hir_output_is_byte(0xa5u));
  CHECK(memcmp(&fixture.hir_result, &result_before, sizeof(result_before)) ==
        0);
  return true;
}

static bool test_process_hir_adversarial(void) {
  static const char SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return .success }\n"
      "entry(run)\n";
  for (int bad = PROCESS_BAD_EXTERNAL_MODULE_COUNT;
       bad <= PROCESS_BAD_HANDLER_ANONYMOUS; bad += 1)
    CHECK(expect_process_frontend_rejected((process_bad_frontend_case)bad));

  CHECK(lower_process(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  const w_seed_hir0_result result_before = fixture.hir_result;
  const w_seed_hir0_external_module saved_module =
      fixture.hir_external_modules[0];
  const w_seed_hir0_external_symbol saved_symbol =
      fixture.hir_external_symbols[3];
  const w_seed_hir0_type saved_type = fixture.hir_types[4];
  const w_seed_hir0_value saved_value = fixture.hir_values[0];
  const w_seed_hir0_function saved_function = fixture.hir_functions[0];
  const w_seed_hir0_parameter saved_parameter = fixture.hir_parameters[0];
  const w_seed_hir0_entry saved_entry = fixture.hir_entries[0];
  const w_seed_hir0_terminator saved_terminator = fixture.hir_terminators[0];

  /* Lifecycle and cleanup facts are not trusted merely because the canonical
   * names, profile, and success case remain intact. */
  fixture.hir_types[4].lifecycle = W_SEED_HIR0_LIFECYCLE_VALUE_COPY;
  fixture.hir_functions[0].direct_entry =
      W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_types[4] = saved_type;
  fixture.hir_functions[0] = saved_function;
  fixture.hir_types[4].release_contract =
      W_SEED_HIR0_RELEASE_CONTRACT_UNKNOWN;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_types[4] = saved_type;
  fixture.hir_entries[0].cleanup_obligation =
      W_SEED_HIR0_ENTRY_CLEANUP_NONE;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_entries[0] = saved_entry;
  fixture.hir_entries[0].first_cleanup_owner_parameter = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_entries[0] = saved_entry;
  fixture.hir_entries[0].cleanup_owner_parameter_count = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_entries[0] = saved_entry;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  /* Reseal each mutant with the production-private digest helpers. This
   * makes the following failures exercise lifecycle verification itself,
   * rather than only the unchanged-digest barrier. */
  fixture.hir_types[4].release_contract = W_SEED_HIR0_RELEASE_CONTRACT_NONE;
  fixture.hir_functions[0].direct_entry =
      W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_types[4] = saved_type;
  fixture.hir_functions[0] = saved_function;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  w_seed_hir0_type saved_string_type = fixture.hir_types[1];
  fixture.hir_types[1].lifecycle = W_SEED_HIR0_LIFECYCLE_VALUE_COPY;
  fixture.hir_types[1].release_contract = W_SEED_HIR0_RELEASE_CONTRACT_NONE;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_types[1] = saved_string_type;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_entries[0].first_cleanup_owner_parameter = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_entries[0] = saved_entry;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_entries[0].cleanup_owner_parameter_count = 1u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_entries[0] = saved_entry;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_external_modules[0].module_id.count = 10u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_external_modules[0] = saved_module;
  fixture.hir_external_symbols[3].kind = W_SEED_HIR0_EXTERNAL_TYPE;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_external_symbols[3] = saved_symbol;
  fixture.hir_external_symbols[3].exported = false;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_external_symbols[3] = saved_symbol;
  fixture.hir_external_symbols[3].is_const = false;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_external_symbols[3] = saved_symbol;
  fixture.hir_external_symbols[3].receiver_type =
      fixture.hir_external_symbols[1].name;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_external_symbols[3] = saved_symbol;
  fixture.hir_external_symbols[3].return_type =
      fixture.hir_external_symbols[1].name;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_external_symbols[3] = saved_symbol;
  fixture.hir_external_symbols[3].parameter_count = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_external_symbols[3] = saved_symbol;
  fixture.hir_external_modules[0].first_symbol = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_external_modules[0] = saved_module;

  fixture.hir_types[4].external_module_index = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_types[4] = saved_type;
  fixture.hir_types[4].external_symbol_index = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_types[4] = saved_type;
  fixture.hir_values[0].kind = W_SEED_HIR0_VALUE_CONST_STRING;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[0] = saved_value;
  fixture.hir_values[0].member_name.count = 6u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[0] = saved_value;
  fixture.hir_values[0].external_symbol_index = 2u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[0] = saved_value;

  fixture.hir_functions[0].is_async = false;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_functions[0] = saved_function;
  fixture.hir_functions[0].is_const = true;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_functions[0] = saved_function;
  fixture.hir_functions[0].is_throws = true;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_functions[0] = saved_function;
  fixture.hir_functions[0].is_unsafe = true;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_functions[0] = saved_function;
  fixture.hir_functions[0].has_borrow_clause = true;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_functions[0] = saved_function;
  fixture.hir_functions[0].return_type = 4u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_functions[0] = saved_function;
  fixture.hir_parameters[0].type_index = 5u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_parameters[0] = saved_parameter;
  fixture.hir_parameters[0].ordinal = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_parameters[0] = saved_parameter;
  fixture.hir_entries[0].adapter_kind =
      W_SEED_HIR0_ENTRY_ADAPTER_DEFAULT_UNIT;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_entries[0] = saved_entry;
  fixture.hir_terminators[0].result_type = 4u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[0] = saved_terminator;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const uint8_t saved_receipt_byte = fixture.hir_receipt[0];
  fixture.hir_receipt[0] ^= 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_receipt[0] = saved_receipt_byte;
  const uint8_t saved_digest_byte = fixture.hir_result.semantic_digest[0];
  fixture.hir_result.semantic_digest[0] ^= 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_result.semantic_digest[0] = saved_digest_byte;
  const size_t saved_external_symbols =
      fixture.hir_result.required.external_symbols;
  fixture.hir_result.required.external_symbols = 3u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_result.required.external_symbols = saved_external_symbols;
  CHECK(memcmp(&fixture.hir_result, &result_before, sizeof(result_before)) ==
        0);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  /* Caller-owned external output arrays are transactional and non-aliasing. */
  CHECK(lower_process(SOURCE));
  const w_seed_hir0_input input = hir_input();
  setup_hir_output();
  fill_hir_output(0xa5u);
  fixture.hir_output.external_module_capacity = 0u;
  const w_seed_hir0_result capacity_result_before = fixture.hir_result;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(0xa5u));
  CHECK(memcmp(&fixture.hir_result, &capacity_result_before,
               sizeof(capacity_result_before)) == 0);
  setup_hir_output();
  fill_hir_output(0xa5u);
  fixture.hir_output.external_symbol_capacity = 3u;
  const w_seed_hir0_result symbol_capacity_result_before = fixture.hir_result;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(0xa5u));
  CHECK(memcmp(&fixture.hir_result, &symbol_capacity_result_before,
               sizeof(symbol_capacity_result_before)) == 0);
  setup_hir_output();
  fill_hir_output(0xa5u);
  w_seed_hir0_output alias = fixture.hir_output;
  alias.external_modules =
      (w_seed_hir0_external_module *)(void *)fixture.hir_external_symbols;
  CHECK(w_seed_hir0_run(&input, &alias, &fixture.hir_result) ==
        W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u));
  setup_hir_output();
  fill_hir_output(0xa5u);
  alias = fixture.hir_output;
  alias.external_symbols =
      (w_seed_hir0_external_symbol *)(void *)fixture.hir_external_modules;
  CHECK(w_seed_hir0_run(&input, &alias, &fixture.hir_result) ==
        W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u));

  /* Every nested import text slice is input-owned and must stay outside the
   * caller-owned HIR text buffer. */
  CHECK(lower_process(SOURCE));
  const w_seed_hir0_input import_input = hir_input();
  uint8_t path_before[11];
  (void)memcpy(path_before, fixture.imports[0].path.data,
               sizeof(path_before));
  setup_hir_output();
  fill_hir_output(0xa5u);
  alias = fixture.hir_output;
  alias.text_bytes = (uint8_t *)(void *)fixture.imports[0].path.data;
  alias.text_byte_capacity = fixture.hir_counts.text_bytes;
  const w_seed_hir0_result path_result_before = fixture.hir_result;
  CHECK(w_seed_hir0_run(&import_input, &alias, &fixture.hir_result) ==
        W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u) &&
        memcmp(path_before, fixture.imports[0].path.data,
               sizeof(path_before)) == 0 &&
        memcmp(&fixture.hir_result, &path_result_before,
               sizeof(path_result_before)) == 0);

  uint8_t local_name_before[sizeof("ProcessArguments") - 1u];
  (void)memcpy(local_name_before, fixture.import_items[0].local_name.data,
               sizeof(local_name_before));
  setup_hir_output();
  fill_hir_output(0xa5u);
  alias = fixture.hir_output;
  alias.text_bytes =
      (uint8_t *)(void *)fixture.import_items[0].local_name.data;
  alias.text_byte_capacity = fixture.hir_counts.text_bytes;
  const w_seed_hir0_result local_name_result_before = fixture.hir_result;
  CHECK(w_seed_hir0_run(&import_input, &alias, &fixture.hir_result) ==
        W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u) &&
        memcmp(local_name_before, fixture.import_items[0].local_name.data,
               sizeof(local_name_before)) == 0 &&
        memcmp(&fixture.hir_result, &local_name_result_before,
               sizeof(local_name_result_before)) == 0);

  static const char MODULE_ALIAS[] = "module";
  fixture.imports[0].alias =
      (w_seed_frontend_text){MODULE_ALIAS, sizeof(MODULE_ALIAS) - 1u};
  setup_hir_output();
  fill_hir_output(0xa5u);
  alias = fixture.hir_output;
  alias.text_bytes = (uint8_t *)(void *)fixture.imports[0].alias.data;
  alias.text_byte_capacity = fixture.hir_counts.text_bytes;
  const w_seed_hir0_result alias_result_before = fixture.hir_result;
  CHECK(w_seed_hir0_run(&import_input, &alias, &fixture.hir_result) ==
        W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u) &&
        memcmp(MODULE_ALIAS, fixture.imports[0].alias.data,
               sizeof(MODULE_ALIAS) - 1u) == 0 &&
        memcmp(&fixture.hir_result, &alias_result_before,
               sizeof(alias_result_before)) == 0);
  return true;
}

static bool test_capacity_and_alias_barriers(void) {
  CHECK(lower(CANONICAL_SOURCE));
  const w_seed_hir0_input input = hir_input();
  w_seed_hir0_counts counts;
  w_seed_hir0_result measured;
  CHECK(w_seed_hir0_measure(&input, &counts, &measured) == W_SEED_HIR0_OK);
  CHECK(counts.modules == fixture.hir_counts.modules &&
        counts.receipt_bytes == fixture.hir_counts.receipt_bytes);
  w_seed_hir0_result invalid_result;
  CHECK(w_seed_hir0_measure(&input, NULL, &invalid_result) ==
        W_SEED_HIR0_INVALID);
  CHECK(w_seed_hir0_measure(&input, &counts, NULL) == W_SEED_HIR0_INVALID);
  CHECK(w_seed_hir0_run(&input, NULL, &fixture.hir_result) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, NULL) ==
        W_SEED_HIR0_INVALID);
  typedef struct {
    size_t *capacity;
    size_t required;
  } capacity_case;
  capacity_case cases[] = {
      {&fixture.hir_output.module_capacity, 1u},
      {&fixture.hir_output.identity_capacity, 5u},
      {&fixture.hir_output.type_capacity, 4u},
      {&fixture.hir_output.function_capacity, 1u},
      {&fixture.hir_output.parameter_capacity, 0u},
      {&fixture.hir_output.block_capacity, 1u},
      {&fixture.hir_output.instruction_capacity, 1u},
      {&fixture.hir_output.call_capacity, 1u},
      {&fixture.hir_output.host_parameter_capacity, 1u},
      {&fixture.hir_output.argument_capacity, 1u},
      {&fixture.hir_output.requirement_capacity, 1u},
      {&fixture.hir_output.value_capacity, 1u},
      {&fixture.hir_output.terminator_capacity, 1u},
      {&fixture.hir_output.entry_capacity, 1u},
      {&fixture.hir_output.text_byte_capacity, 0u},
      {&fixture.hir_output.value_byte_capacity, 0u},
      {&fixture.hir_output.receipt_capacity, 0u}};
  for (size_t index = 0u; index < sizeof(cases) / sizeof(cases[0]);
       index += 1u) {
    if (index == 14u) cases[index].required = fixture.hir_counts.text_bytes;
    if (index == 15u) cases[index].required = fixture.hir_counts.value_bytes;
    if (index == 16u) cases[index].required = fixture.hir_counts.receipt_bytes;
    if (cases[index].required == 0u) continue;
    setup_hir_output();
    fill_hir_output(0xa5u);
    const w_seed_hir0_result result_before = fixture.hir_result;
    *cases[index].capacity = cases[index].required - 1u;
    CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
          W_SEED_HIR0_CAPACITY);
    CHECK(hir_output_is_byte(0xa5u));
    CHECK(memcmp(&fixture.hir_result, &result_before,
                 sizeof(result_before)) == 0);
  }
  setup_hir_output();
  w_seed_hir0_output alias = fixture.hir_output;
  alias.identities = (w_seed_hir0_identity *)alias.modules;
  fill_hir_output(0xa5u);
  CHECK(w_seed_hir0_run(&input, &alias, &fixture.hir_result) ==
        W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u));
  setup_hir_output();
  alias = fixture.hir_output;
  alias.modules = (w_seed_hir0_module *)alias.text_bytes;
  fill_hir_output(0xa5u);
  CHECK(w_seed_hir0_run(&input, &alias, &fixture.hir_result) ==
        W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u));
  setup_hir_output();
  const w_seed_frontend_module frontend_module_before = fixture.modules[0];
  alias = fixture.hir_output;
  alias.modules = (w_seed_hir0_module *)fixture.modules;
  CHECK(w_seed_hir0_run(&input, &alias, &fixture.hir_result) ==
        W_SEED_HIR0_INVALID);
  CHECK(memcmp(&fixture.modules[0], &frontend_module_before,
               sizeof(frontend_module_before)) == 0);
  setup_hir_output();
  fill_hir_output(0xa5u);
  CHECK(w_seed_hir0_run(
            &input, &fixture.hir_output,
            (w_seed_hir0_result *)(void *)fixture.hir_modules) ==
        W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u));
  setup_hir_output();
  fill_hir_output(0xa5u);
  const w_seed_frontend_result frontend_result_before = fixture.result;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output,
                        (w_seed_hir0_result *)(void *)&fixture.result) ==
        W_SEED_HIR0_INVALID);
  CHECK(memcmp(&fixture.result, &frontend_result_before,
               sizeof(frontend_result_before)) == 0 &&
        hir_output_is_byte(0xa5u));
  setup_hir_output();
  fill_hir_output(0xa5u);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output,
                        (w_seed_hir0_result *)(void *)fixture.hir_text) ==
        W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u));
  union {
    w_seed_hir0_counts counts;
    w_seed_hir0_result result;
  } measure_alias;
  (void)memset(&measure_alias, 0xa5, sizeof(measure_alias));
  CHECK(w_seed_hir0_measure(&input,
                            (w_seed_hir0_counts *)(void *)&measure_alias,
                            (w_seed_hir0_result *)(void *)&measure_alias) ==
        W_SEED_HIR0_INVALID);
  const uint8_t *measure_alias_bytes = (const uint8_t *)(const void *)&measure_alias;
  for (size_t byte = 0u; byte < sizeof(measure_alias); byte += 1u)
    CHECK(measure_alias_bytes[byte] == 0xa5u);
  setup_hir_output();
  const w_seed_hir0_program bridge_before = fixture.hir_program;
  const w_seed_hir0_output output_before = fixture.hir_output;
  const w_seed_hir0_result bridge_result_before = fixture.hir_result;
  CHECK(!w_seed_hir0_program_from_output(
      &fixture.hir_output, &fixture.hir_result,
      (w_seed_hir0_program *)(void *)&fixture.hir_output));
  CHECK(memcmp(&fixture.hir_program, &bridge_before, sizeof(bridge_before)) ==
            0 &&
        memcmp(&fixture.hir_output, &output_before, sizeof(output_before)) ==
            0 &&
        memcmp(&fixture.hir_result, &bridge_result_before,
               sizeof(bridge_result_before)) ==
            0);
  const w_seed_hir0_module module_before = fixture.hir_modules[0];
  CHECK(!w_seed_hir0_program_from_output(
      &fixture.hir_output, &fixture.hir_result,
      (w_seed_hir0_program *)(void *)fixture.hir_modules));
  CHECK(memcmp(&fixture.hir_modules[0], &module_before, sizeof(module_before)) ==
        0);
  const w_seed_hir0_result result_destination_before = fixture.hir_result;
  CHECK(!w_seed_hir0_program_from_output(
      &fixture.hir_output, &fixture.hir_result,
      (w_seed_hir0_program *)(void *)&fixture.hir_result));
  CHECK(memcmp(&fixture.hir_result, &result_destination_before,
               sizeof(result_destination_before)) == 0);
  uint8_t text_before[sizeof(fixture.hir_text)];
  (void)memcpy(text_before, fixture.hir_text, sizeof(text_before));
  CHECK(!w_seed_hir0_program_from_output(
      &fixture.hir_output, &fixture.hir_result,
      (w_seed_hir0_program *)(void *)fixture.hir_text));
  CHECK(memcmp(fixture.hir_text, text_before, sizeof(text_before)) == 0);
  w_seed_hir0_counts counts_before = counts;
  w_seed_hir0_result result_before = fixture.hir_result;
  fixture.input.host_scope = NULL;
  CHECK(w_seed_hir0_measure(&input, &counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  CHECK(memcmp(&counts, &counts_before, sizeof(counts)) == 0 &&
        memcmp(&fixture.hir_result, &result_before, sizeof(result_before)) ==
            0);
  fixture.input.host_scope = &fixture.host_scope;
  return true;
}

static bool test_verify_mutations(void) {
  CHECK(lower(CANONICAL_SOURCE));
  w_seed_hir0_program *program = &fixture.hir_program;
  w_seed_hir0_result *result = &fixture.hir_result;
  CHECK(w_seed_hir0_verify(program, result));
  w_seed_hir0_program saved_program = *program;
  w_seed_hir0_result saved_result = *result;
  const w_seed_hir0_module saved_module = fixture.hir_modules[0];
  const w_seed_hir0_identity saved_identity = fixture.hir_identities[4];
  const w_seed_hir0_function saved_function = fixture.hir_functions[0];
  const w_seed_hir0_block saved_block = fixture.hir_blocks[0];
  const w_seed_hir0_instruction saved_instruction = fixture.hir_instructions[0];
  const w_seed_hir0_call saved_call = fixture.hir_calls[0];
  const w_seed_hir0_argument saved_argument = fixture.hir_arguments[0];
  const w_seed_hir0_argument saved_argument1 = fixture.hir_arguments[1];
  const w_seed_hir0_value saved_value = fixture.hir_values[0];
  const w_seed_hir0_terminator saved_terminator = fixture.hir_terminators[0];
  const w_seed_hir0_entry saved_entry = fixture.hir_entries[0];
  const w_seed_hir0_requirement saved_requirement = fixture.hir_requirements[0];
  const w_seed_hir0_host_parameter saved_host_parameter0 =
      fixture.hir_host_parameters[0];
  const w_seed_hir0_host_parameter saved_host_parameter1 =
      fixture.hir_host_parameters[1];
#define RESTORE_RECORDS()                                                       \
  do {                                                                         \
    fixture.hir_modules[0] = saved_module;                                     \
    fixture.hir_identities[4] = saved_identity;                               \
    fixture.hir_functions[0] = saved_function;                                 \
    fixture.hir_blocks[0] = saved_block;                                       \
    fixture.hir_instructions[0] = saved_instruction;                           \
    fixture.hir_calls[0] = saved_call;                                         \
    fixture.hir_arguments[0] = saved_argument;                                 \
    fixture.hir_arguments[1] = saved_argument1;                                \
    fixture.hir_values[0] = saved_value;                                       \
    fixture.hir_terminators[0] = saved_terminator;                             \
    fixture.hir_entries[0] = saved_entry;                                      \
    fixture.hir_requirements[0] = saved_requirement;                           \
    fixture.hir_host_parameters[0] = saved_host_parameter0;                     \
    fixture.hir_host_parameters[1] = saved_host_parameter1;                     \
  } while (0)

  result->schema[0] = 'X';
  CHECK(!w_seed_hir0_verify(program, result));
  *result = saved_result;
  program->module_capacity = 0u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  program->text_byte_count -= 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_text[0] = 'X';
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_text[0] = '(';
  fixture.hir_identities[4].name.count = UINT32_MAX;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_identities[4].name.offset = UINT32_MAX;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_identities[4].profile.offset = UINT32_MAX;
  fixture.hir_identities[4].profile.count = 0u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_value_bytes[0] = 'X';
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_value_bytes[0] = 'H';
  fixture.hir_receipt[0] ^= 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_receipt[0] ^= 1u;
  result->semantic_digest[0] ^= 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *result = saved_result;
  result->provenance_digest[0] ^= 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *result = saved_result;

  fixture.hir_modules[0].module_index = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_functions[0].identity_index = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  program->parameters = NULL;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_blocks[0].first_instruction = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_blocks[0].ordinal = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_instructions[0].owner_block = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_calls[0].callee_identity = 3u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_calls[0].first_argument = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_host_parameters[0].label = (w_seed_hir0_text){0u, 0u};
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_arguments[0].label = fixture.hir_host_parameters[1].label;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_arguments[0].ordinal = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_arguments[0].type_index = 0u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_values[0].owner_index = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_terminators[0].kind =
      (w_seed_hir0_terminator_kind)(W_SEED_HIR0_TERMINATOR_RETURN_UNIT + 1);
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_entries[0].target_function = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  const uint32_t profile_offset = program->identities[4].profile.offset;
  fixture.hir_text[profile_offset] = 'X';
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_text[profile_offset] = 'n';
  fixture.hir_identities[4].parameter_count = 0u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_identities[4].first_parameter = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_identities[4].parameter_count = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_identities[4].first_requirement = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_calls[0].requirement_count = 0u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_requirements[0].owner_index = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_calls[0].argument_count = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  fixture.hir_arguments[1].value_index = 0u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  w_seed_hir0_program bridge_before = *program;
  w_seed_hir0_result bad_result = *result;
  bad_result.status = W_SEED_HIR0_INVALID;
  CHECK(!w_seed_hir0_program_from_output(&fixture.hir_output, &bad_result,
                                         program));
  CHECK(program->modules == bridge_before.modules &&
        program->module_count == bridge_before.module_count &&
        program->module_capacity == bridge_before.module_capacity &&
        program->receipt == bridge_before.receipt &&
        program->receipt_count == bridge_before.receipt_count);
  w_seed_hir0_output truncated_output = fixture.hir_output;
  truncated_output.module_capacity = 0u;
  CHECK(!w_seed_hir0_program_from_output(&truncated_output, result, program));
  CHECK(program->modules == bridge_before.modules &&
        program->module_capacity == bridge_before.module_capacity);
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output, result,
                                        program));
  program->module_capacity = 0u;
  CHECK(!w_seed_hir0_verify(program, result));
  *program = saved_program;
  RESTORE_RECORDS();
  CHECK(w_seed_hir0_verify(program, result));
#undef RESTORE_RECORDS
  return true;
}

static bool test_closed_frontend_barriers(void) {
  CHECK(lower(CANONICAL_SOURCE));
  const w_seed_hir0_input input = hir_input();
  const w_seed_frontend_result saved_result = fixture.result;
  typedef struct {
    size_t *required;
    size_t *written;
  } family_case;
  family_case unsupported[] = {
      {&fixture.result.required.imports, &fixture.result.written.imports},
      {&fixture.result.required.import_items,
       &fixture.result.written.import_items},
      {&fixture.result.required.structs, &fixture.result.written.structs},
      {&fixture.result.required.fields, &fixture.result.written.fields},
      {&fixture.result.required.type_declarations,
       &fixture.result.written.type_declarations},
      {&fixture.result.required.aliases, &fixture.result.written.aliases},
      {&fixture.result.required.facts, &fixture.result.written.facts},
      {&fixture.result.required.diagnostics,
       &fixture.result.written.diagnostics},
      {&fixture.result.required.diagnostic_facts,
       &fixture.result.written.diagnostic_facts},
      {&fixture.result.required.diagnostic_items,
       &fixture.result.written.diagnostic_items},
      {&fixture.result.required.diagnostic_labels,
       &fixture.result.written.diagnostic_labels},
      {&fixture.result.required.switch_arms,
       &fixture.result.written.switch_arms},
      {&fixture.result.required.enum_subset_members,
       &fixture.result.written.enum_subset_members},
      {&fixture.result.required.enum_membership_cases,
       &fixture.result.written.enum_membership_cases},
      {&fixture.result.required.generic_parameters,
       &fixture.result.written.generic_parameters},
      {&fixture.result.required.generic_applications,
       &fixture.result.written.generic_applications},
      {&fixture.result.required.generic_arguments,
       &fixture.result.written.generic_arguments},
      {&fixture.result.required.typed_const_expressions,
       &fixture.result.written.typed_const_expressions},
      {&fixture.result.required.const_values,
       &fixture.result.written.const_values},
      {&fixture.result.required.const_elements,
       &fixture.result.written.const_elements},
      {&fixture.result.required.const_declarations,
       &fixture.result.written.const_declarations},
      {&fixture.result.required.kernel_modules,
       &fixture.result.written.kernel_modules},
      {&fixture.result.required.kernel_bindings,
       &fixture.result.written.kernel_bindings},
  };
  for (size_t index = 0u; index < sizeof(unsupported) / sizeof(unsupported[0]);
       index += 1u) {
    *unsupported[index].required = 1u;
    *unsupported[index].written = 1u;
    CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) ==
          W_SEED_HIR0_UNSUPPORTED);
    fixture.result = saved_result;
  }
  const w_seed_frontend_symbol saved_symbol = fixture.symbols[1];
  fixture.symbols[1].name = (w_seed_frontend_text){"forged", 6u};
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  fixture.symbols[1] = saved_symbol;
  const w_seed_frontend_module saved_module = fixture.modules[0];
  fixture.modules[0].first_function = 1u;
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  fixture.modules[0] = saved_module;
  const w_seed_frontend_function saved_function = fixture.functions[0];
  fixture.functions[0].first_statement = 1u;
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  fixture.functions[0] = saved_function;
  const size_t call_expression_index = fixture.statements[0].expression_index;
  const w_seed_frontend_expression saved_call =
      fixture.expressions[call_expression_index];
  fixture.expressions[call_expression_index].first_argument = 1u;
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  fixture.expressions[call_expression_index] = saved_call;
  const w_seed_frontend_expression saved_callee = fixture.expressions[0];
  fixture.expressions[call_expression_index].left = 1u;
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  fixture.expressions[call_expression_index] = saved_call;
  const w_seed_frontend_argument saved_frontend_argument = fixture.arguments[0];
  fixture.arguments[0].expression_index = 0u;
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  fixture.arguments[0] = saved_frontend_argument;
  const w_seed_frontend_expression saved_first_value = fixture.expressions[1];
  fixture.expressions[1].const_byte_offset = 1u;
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  fixture.expressions[1] = saved_first_value;
  const w_seed_frontend_expression saved_second_value = fixture.expressions[2];
  fixture.expressions[2].const_byte_offset = 12u;
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  fixture.expressions[2] = saved_second_value;
  const w_seed_frontend_statement saved_statement = fixture.statements[0];
  fixture.statements[0].expression_index = 2u;
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  fixture.statements[0] = saved_statement;
  fixture.expressions[0] = saved_callee;
  const size_t saved_document_count = fixture.input.document_count;
  fixture.input.document_count = 2u;
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  fixture.input.document_count = saved_document_count;
  const w_seed_frontend_result saved_entry_result = fixture.result;
  fixture.result.required.entries = 2u;
  fixture.result.written.entries = 2u;
  CHECK(w_seed_hir0_measure(&input, &fixture.hir_counts, &fixture.hir_result) !=
        W_SEED_HIR0_OK);
  fixture.result = saved_entry_result;
  return true;
}

static bool test_typed_interpolation_value_tree(void) {
  static const char SOURCE[] =
      "fn main() { print(message: \"The answer is ${6 * 7}\", suffix: \"!\") }\n"
      "entry(main)\n";
  CHECK(fixture_frontend(SOURCE));
  CHECK(fixture.result.written.interpolation_segments == 2u);
  setup_hir_output();
  const w_seed_hir0_input input = hir_input();
  w_seed_hir0_counts counts;
  w_seed_hir0_result result;
  (void)memset(&counts, 0xa4, sizeof(counts));
  (void)memset(&result, 0xa4, sizeof(result));
  const w_seed_hir0_counts rejected_counts = counts;
  const w_seed_hir0_result rejected_measure = result;
  const uint32_t saved_owner =
      fixture.interpolation_segments[0].owner_expression;
  fixture.interpolation_segments[0].owner_expression = W_SEED_FRONTEND_NONE;
  CHECK(w_seed_hir0_measure(&input, &counts, &result) ==
        W_SEED_HIR0_UNSUPPORTED);
  CHECK(memcmp(&counts, &rejected_counts, sizeof(counts)) == 0);
  CHECK(memcmp(&result, &rejected_measure, sizeof(result)) == 0);
  fixture.interpolation_segments[0].owner_expression = saved_owner;

  CHECK(w_seed_hir0_measure(&input, &counts, &result) == W_SEED_HIR0_OK);
  CHECK(counts.types == 4u && counts.values == 5u &&
        counts.interpolation_segments == 2u && counts.value_bytes == 15u);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &result) ==
        W_SEED_HIR0_OK);
  w_seed_hir0_program program;
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output, &result,
                                        &program));
  CHECK(w_seed_hir0_verify(&program, &result));
  CHECK(program.arguments[0].value_index == 3u &&
        program.arguments[1].value_index == 4u);
  CHECK(program.values[0].kind == W_SEED_HIR0_VALUE_CONST_I64 &&
        program.values[0].integer_value == 6 &&
        program.values[1].kind == W_SEED_HIR0_VALUE_CONST_I64 &&
        program.values[1].integer_value == 7);
  CHECK(program.values[2].kind == W_SEED_HIR0_VALUE_BINARY_I64 &&
        program.values[2].binary_operator == W_SEED_HIR0_BINARY_MULTIPLY &&
        program.values[2].left_value == 0u &&
        program.values[2].right_value == 1u);
  CHECK(program.values[3].kind ==
            W_SEED_HIR0_VALUE_INTERPOLATED_STRING &&
        program.values[3].first_interpolation_segment == 0u &&
        program.values[3].interpolation_segment_count == 2u);
  CHECK(program.interpolation_segments[0].kind ==
            W_SEED_HIR0_INTERPOLATION_TEXT &&
        program.interpolation_segments[0].byte_count == 14u &&
        program.interpolation_segments[1].kind ==
            W_SEED_HIR0_INTERPOLATION_VALUE &&
        program.interpolation_segments[1].value_index == 2u);
  CHECK(memcmp(program.value_bytes, "The answer is !", 15u) == 0);

  const w_seed_hir0_value saved_binary = fixture.hir_values[2];
  const w_seed_hir0_value saved_interpolation = fixture.hir_values[3];
  const w_seed_hir0_interpolation_segment saved_segment =
      fixture.hir_interpolation_segments[1];
  fixture.hir_values[2].left_value = 1u;
  CHECK(!w_seed_hir0_verify(&program, &result));
  fixture.hir_values[2] = saved_binary;
  fixture.hir_values[3].interpolation_segment_count = 1u;
  CHECK(!w_seed_hir0_verify(&program, &result));
  fixture.hir_values[3] = saved_interpolation;
  fixture.hir_interpolation_segments[1].owner_value = 2u;
  CHECK(!w_seed_hir0_verify(&program, &result));
  fixture.hir_interpolation_segments[1] = saved_segment;
  CHECK(w_seed_hir0_verify(&program, &result));

  setup_hir_output();
  fill_hir_output(0xa5u);
  w_seed_hir0_result rejected_result;
  (void)memset(&rejected_result, 0xa5, sizeof(rejected_result));
  const w_seed_hir0_result rejected_before = rejected_result;
  fixture.hir_output.interpolation_segment_capacity = 1u;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected_result) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(0xa5u));
  CHECK(memcmp(&rejected_result, &rejected_before,
               sizeof(rejected_result)) == 0);

  setup_hir_output();
  fill_hir_output(0xa5u);
  w_seed_hir0_output aliased = fixture.hir_output;
  aliased.interpolation_segments =
      (w_seed_hir0_interpolation_segment *)(void *)fixture.hir_values;
  CHECK(w_seed_hir0_run(&input, &aliased, &rejected_result) ==
        W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u));
  CHECK(memcmp(&rejected_result, &rejected_before,
               sizeof(rejected_result)) == 0);
  return true;
}

static bool test_builtin_display_value_tree(void) {
  static const char SOURCE[] =
      "fn main() { let state = \"open\" "
      "print(message: \"${true}/${false}/${state}\", suffix: \"!\") }\n"
      "entry(main)\n";
  CHECK(fixture_frontend(SOURCE));
  setup_hir_output();
  const w_seed_hir0_input input = hir_input();
  w_seed_hir0_counts counts;
  w_seed_hir0_result result;
  CHECK(w_seed_hir0_measure(&input, &counts, &result) == W_SEED_HIR0_OK);
  CHECK(counts.bindings == 1u && counts.values == 6u &&
        counts.interpolation_segments == 5u && counts.value_bytes == 7u);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &result) ==
        W_SEED_HIR0_OK);
  w_seed_hir0_program program;
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output, &result,
                                        &program));
  CHECK(w_seed_hir0_verify(&program, &result));
  CHECK(program.bindings[0].initializer_value == 0u &&
        program.values[0].kind == W_SEED_HIR0_VALUE_CONST_STRING &&
        program.values[0].owner_kind == W_SEED_HIR0_VALUE_OWNER_BINDING &&
        program.values[1].kind == W_SEED_HIR0_VALUE_CONST_BOOL &&
        program.values[1].type_index == 3u && program.values[1].bool_value &&
        program.values[2].kind == W_SEED_HIR0_VALUE_CONST_BOOL &&
        program.values[2].type_index == 3u && !program.values[2].bool_value &&
        program.values[3].kind == W_SEED_HIR0_VALUE_BINDING_READ &&
        program.values[3].type_index == 1u &&
        program.values[3].binding_index == 0u &&
        program.values[4].kind == W_SEED_HIR0_VALUE_INTERPOLATED_STRING &&
        program.values[4].type_index == 1u &&
        program.values[5].kind == W_SEED_HIR0_VALUE_CONST_STRING &&
        program.values[5].type_index == 1u);
  CHECK(program.interpolation_segments[0].kind ==
            W_SEED_HIR0_INTERPOLATION_VALUE &&
        program.interpolation_segments[0].value_index == 1u &&
        program.interpolation_segments[1].kind ==
            W_SEED_HIR0_INTERPOLATION_TEXT &&
        program.interpolation_segments[2].kind ==
            W_SEED_HIR0_INTERPOLATION_VALUE &&
        program.interpolation_segments[2].value_index == 2u &&
        program.interpolation_segments[3].kind ==
            W_SEED_HIR0_INTERPOLATION_TEXT &&
        program.interpolation_segments[4].kind ==
            W_SEED_HIR0_INTERPOLATION_VALUE &&
        program.interpolation_segments[4].value_index == 3u);
  CHECK(memcmp(program.value_bytes, "open//!", 7u) == 0);
  return true;
}

static bool test_typed_immutable_binding_values(void) {
  static const char SOURCE[] =
      "fn serve() { let table = 6 * 7 let isOpen = true let state = \"open\" "
      "print(message: \"Table ${table}; open: ${isOpen}; state: ${state}\", "
      "suffix: \"!\") }\nentry(serve)\n";
  CHECK(lower(SOURCE));
  CHECK(fixture.hir_program.binding_count == 3u &&
        fixture.hir_program.instruction_count == 4u &&
        fixture.hir_program.value_count == 10u &&
        fixture.hir_program.interpolation_segment_count == 6u);
  CHECK(fixture.hir_program.bindings[0].type_index == 2u &&
        fixture.hir_program.bindings[0].initializer_value == 2u &&
        fixture.hir_program.values[2].kind ==
            W_SEED_HIR0_VALUE_BINARY_I64 &&
        fixture.hir_program.values[2].owner_kind ==
            W_SEED_HIR0_VALUE_OWNER_BINDING &&
        fixture.hir_program.values[2].owner_index == 0u);
  CHECK(fixture.hir_program.bindings[1].type_index == 3u &&
        fixture.hir_program.bindings[1].initializer_value == 3u &&
        fixture.hir_program.values[3].kind == W_SEED_HIR0_VALUE_CONST_BOOL &&
        fixture.hir_program.values[3].owner_kind ==
            W_SEED_HIR0_VALUE_OWNER_BINDING &&
        fixture.hir_program.values[3].bool_value);
  CHECK(fixture.hir_program.bindings[2].type_index == 1u &&
        fixture.hir_program.bindings[2].initializer_value == 4u &&
        fixture.hir_program.values[4].kind ==
            W_SEED_HIR0_VALUE_CONST_STRING);
  CHECK(fixture.hir_program.values[5].kind ==
            W_SEED_HIR0_VALUE_BINDING_READ &&
        fixture.hir_program.values[5].type_index == 2u &&
        fixture.hir_program.values[5].binding_index == 0u &&
        fixture.hir_program.values[6].kind ==
            W_SEED_HIR0_VALUE_BINDING_READ &&
        fixture.hir_program.values[6].type_index == 3u &&
        fixture.hir_program.values[6].binding_index == 1u &&
        fixture.hir_program.values[7].kind ==
            W_SEED_HIR0_VALUE_BINDING_READ &&
        fixture.hir_program.values[7].type_index == 1u &&
        fixture.hir_program.values[7].binding_index == 2u &&
        fixture.hir_program.values[8].kind ==
            W_SEED_HIR0_VALUE_INTERPOLATED_STRING &&
        fixture.hir_program.values[9].kind ==
            W_SEED_HIR0_VALUE_CONST_STRING);
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));

  const w_seed_hir0_binding saved_binding = fixture.hir_bindings[0];
  fixture.hir_bindings[0].initializer_value = 3u;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_bindings[0] = saved_binding;

  const w_seed_hir0_value saved_initializer = fixture.hir_values[2];
  fixture.hir_values[2].owner_index = 1u;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_values[2] = saved_initializer;

  const w_seed_hir0_value saved_read = fixture.hir_values[5];
  fixture.hir_values[5].type_index = 1u;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_values[5] = saved_read;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool test_local_unit_call_and_parameter_reads(void) {
  static const char SOURCE[] =
      "fn announce(table: i64, isOpen: Bool) { "
      "print(message: \"Table ${table}; open: ${isOpen}\", suffix: \"\") }\n"
      "fn main() { announce(isOpen: true, table: 6 * 7) }\n"
      "entry(main)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 2u && program->parameter_count == 2u &&
        program->call_count == 2u && program->argument_count == 4u &&
        program->value_count == 8u && program->entry_count == 1u);
  CHECK(program->functions[0].first_parameter == 0u &&
        program->functions[0].parameter_count == 2u &&
        program->parameters[0].owner_function == 0u &&
        program->parameters[0].ordinal == 0u &&
        program->parameters[0].type_index == 2u &&
        program->parameters[1].owner_function == 0u &&
        program->parameters[1].ordinal == 1u &&
        program->parameters[1].type_index == 3u);
  CHECK(program->identities[1].kind == W_SEED_HIR0_IDENTITY_FUNCTION &&
        program->identities[1].first_parameter == 0u &&
        program->identities[1].parameter_count == 2u &&
        program->identities[1].return_type == 0u);
  CHECK(program->calls[0].callee_identity >=
            program->module_count + program->function_count +
                program->entry_count &&
        program->calls[1].callee_identity == 1u &&
        program->calls[1].first_requirement == W_SEED_HIR0_NONE &&
        program->calls[1].requirement_count == 0u);
  CHECK(program->values[0].kind == W_SEED_HIR0_VALUE_PARAMETER_READ &&
        program->values[0].parameter_index == 0u &&
        program->values[0].type_index == 2u &&
        program->values[1].kind == W_SEED_HIR0_VALUE_PARAMETER_READ &&
        program->values[1].parameter_index == 1u &&
        program->values[1].type_index == 3u);
  CHECK(program->arguments[2].ordinal == 0u &&
        program->arguments[2].parameter_ordinal == 1u &&
        program->arguments[2].type_index == 3u &&
        program->values[program->arguments[2].value_index].kind ==
            W_SEED_HIR0_VALUE_CONST_BOOL &&
        program->arguments[3].ordinal == 1u &&
        program->arguments[3].parameter_ordinal == 0u &&
        program->arguments[3].type_index == 2u &&
        program->values[program->arguments[3].value_index].kind ==
            W_SEED_HIR0_VALUE_BINARY_I64);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_argument saved_argument = fixture.hir_arguments[2];
  fixture.hir_arguments[2].parameter_ordinal = 0u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_arguments[2] = saved_argument;
  const w_seed_hir0_value saved_parameter = fixture.hir_values[0];
  fixture.hir_values[0].parameter_index = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[0] = saved_parameter;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_scalar_return_and_call_result(void) {
  static const char SOURCE[] =
      "fn tableNumber(): i64 { return 6 * 7 }\n"
      "fn main() { let table = tableNumber() "
      "print(message: \"Table ${table}\", suffix: \"\") }\n"
      "entry(main)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 2u && program->call_count == 2u &&
        program->binding_count == 1u && program->terminator_count == 2u &&
        program->functions[0].return_type == 2u &&
        program->functions[1].return_type == 0u);
  CHECK(program->calls[0].result_type == 2u &&
        program->identities[program->calls[0].callee_identity].target_index ==
            0u &&
        program->instructions[program->calls[0].owner_instruction]
                .result_type == 2u);
  const uint32_t initializer = program->bindings[0].initializer_value;
  CHECK(initializer < program->value_count &&
        program->values[initializer].kind == W_SEED_HIR0_VALUE_CALL_RESULT &&
        program->values[initializer].type_index == 2u &&
        program->values[initializer].call_index == 0u);
  CHECK(program->terminators[0].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        program->terminators[0].result_type == 2u &&
        program->terminators[0].value_index < program->value_count &&
        program->values[program->terminators[0].value_index].kind ==
            W_SEED_HIR0_VALUE_BINARY_I64 &&
        program->terminators[1].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_UNIT);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_value saved_result = fixture.hir_values[initializer];
  fixture.hir_values[initializer].call_index = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[initializer] = saved_result;
  const w_seed_hir0_terminator saved_terminator = fixture.hir_terminators[0];
  fixture.hir_terminators[0].result_type = 3u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[0] = saved_terminator;
  const w_seed_hir0_call saved_call = fixture.hir_calls[0];
  fixture.hir_calls[0].result_type = 3u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_calls[0] = saved_call;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_scalar_if_value_diamond(void) {
  static const char SOURCE[] =
      "fn serve(isOpen: Bool, seats: i64): i64 { "
      "let next = if isOpen { seats + 1 } else { seats - 1 } "
      "return next }\n"
      "fn flag(isOpen: Bool): Bool { return if isOpen { true } else { false } }\n"
      "entry(serve)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 2u && program->block_count == 8u &&
        program->block_arguments == fixture.hir_block_arguments &&
        program->block_argument_count == 2u &&
        program->functions[0].block_count == 4u &&
        program->functions[1].block_count == 4u);
  CHECK(program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[0].logical_operator ==
            W_SEED_HIR0_LOGICAL_NONE &&
        program->terminators[0].result_type == W_SEED_HIR0_TYPE_I64 &&
        program->terminators[0].target_block == 1u &&
        program->terminators[0].else_block == 2u &&
        edge_value_at(program, 1u) != W_SEED_HIR0_NONE &&
        edge_value_at(program, 2u) != W_SEED_HIR0_NONE &&
        program->terminators[3].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_VALUE);
  CHECK(program->blocks[3].block_argument_count == 1u &&
        program->block_arguments[0].owner_block == 3u &&
        program->block_arguments[0].ordinal == 0u &&
        program->block_arguments[0].type_index == W_SEED_HIR0_TYPE_I64 &&
        program->blocks[7].block_argument_count == 1u &&
        program->block_arguments[1].owner_block == 7u &&
        program->block_arguments[1].type_index == W_SEED_HIR0_TYPE_BOOL);
  CHECK(program->terminators[4].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[4].result_type == W_SEED_HIR0_TYPE_BOOL &&
        edge_value_at(program, 5u) != W_SEED_HIR0_NONE &&
        edge_value_at(program, 6u) != W_SEED_HIR0_NONE &&
        program->terminators[7].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_VALUE);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_terminator saved_branch = fixture.hir_terminators[0];
  fixture.hir_terminators[0].result_type = W_SEED_HIR0_TYPE_BOOL;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[0] = saved_branch;
  fixture.hir_terminators[0].logical_operator = W_SEED_HIR0_LOGICAL_AND;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[0] = saved_branch;

  const uint32_t saved_incoming = edge_value_at(program, 1u);
  FIXTURE_EDGE_VALUE_SLOT(1u) = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  FIXTURE_EDGE_VALUE_SLOT(1u) = saved_incoming;
  const w_seed_hir0_value saved_incoming_value =
      fixture.hir_values[saved_incoming];
  fixture.hir_values[saved_incoming].owner_index = 2u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[saved_incoming] = saved_incoming_value;
  fixture.hir_values[saved_incoming].owner_ordinal = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[saved_incoming] = saved_incoming_value;
  fixture.hir_values[saved_incoming].type_index = W_SEED_HIR0_TYPE_BOOL;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[saved_incoming] = saved_incoming_value;

  const uint32_t saved_target = fixture.hir_terminators[1].target_block;
  fixture.hir_terminators[1].target_block = 2u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[1].target_block = saved_target;
  const w_seed_hir0_block saved_join = fixture.hir_blocks[3];
  const w_seed_hir0_block_argument saved_argument = fixture.hir_block_arguments[0];
  fixture.hir_block_arguments[0].type_index = W_SEED_HIR0_TYPE_BOOL;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_block_arguments[0] = saved_argument;
  fixture.hir_blocks[3].first_block_argument =
      (uint32_t)program->block_argument_count;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_blocks[3] = saved_join;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const uint32_t saved_read = program->values[program->bindings[0].initializer_value]
                                  .block_argument_index;
  fixture.hir_values[program->bindings[0].initializer_value].block_argument_index =
      1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[program->bindings[0].initializer_value].block_argument_index =
      saved_read;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_nested_scalar_if_value_diamond(void) {
  static const char SOURCE[] =
      "fn choose(outer: Bool, inner: Bool, open: i64, middle: i64, "
      "closed: i64): i64 { return if outer { if inner { open } else { "
      "middle } } else { closed } }\n"
      "fn main() { let first = choose(outer: true, inner: true, open: 1, "
      "middle: 2, closed: 3) let second = choose(outer: true, inner: false, "
      "open: 1, middle: 2, closed: 3) let third = choose(outer: false, "
      "inner: false, open: 1, middle: 2, closed: 3) print(message: \"${first},${second},${third}\", "
      "suffix: \"\") }\n"
      "entry(main)\n";
  CHECK(lower(SOURCE));
  w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 2u && program->block_count == 8u &&
        program->block_argument_count == 2u &&
        program->functions[0].first_block == 0u &&
        program->functions[0].block_count == 7u &&
        program->functions[1].first_block == 7u &&
        program->functions[1].block_count == 1u);

  const w_seed_hir0_terminator *outer_branch = &program->terminators[0];
  const w_seed_hir0_terminator *inner_branch = &program->terminators[1];
  CHECK(outer_branch->kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        outer_branch->result_type == W_SEED_HIR0_TYPE_I64 &&
        outer_branch->target_block == 1u && outer_branch->else_block == 5u &&
        inner_branch->kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        inner_branch->result_type == W_SEED_HIR0_TYPE_I64 &&
        inner_branch->target_block == 2u && inner_branch->else_block == 3u);
  CHECK(program->blocks[4].block_argument_count == 1u &&
        program->blocks[4].first_block_argument == 0u &&
        program->block_arguments[0].owner_block == 4u &&
        program->block_arguments[0].ordinal == 0u &&
        program->block_arguments[0].type_index == W_SEED_HIR0_TYPE_I64 &&
        program->blocks[6].block_argument_count == 1u &&
        program->blocks[6].first_block_argument == 1u &&
        program->block_arguments[1].owner_block == 6u &&
        program->block_arguments[1].ordinal == 0u &&
        program->block_arguments[1].type_index == W_SEED_HIR0_TYPE_I64);
  CHECK(program->terminators[2].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[3].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[4].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[5].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[2].target_block == 4u &&
        program->terminators[3].target_block == 4u &&
        program->terminators[4].target_block == 6u &&
        program->terminators[5].target_block == 6u &&
        edge_value_at(program, 2u) != W_SEED_HIR0_NONE &&
        edge_value_at(program, 3u) != W_SEED_HIR0_NONE &&
        edge_value_at(program, 4u) != W_SEED_HIR0_NONE &&
        edge_value_at(program, 5u) != W_SEED_HIR0_NONE);
  CHECK(program->values[edge_value_at(program, 2u)].kind ==
            W_SEED_HIR0_VALUE_PARAMETER_READ &&
        program->values[edge_value_at(program, 3u)].kind ==
            W_SEED_HIR0_VALUE_PARAMETER_READ &&
        program->values[edge_value_at(program, 4u)].kind ==
            W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        program->values[edge_value_at(program, 4u)]
                .block_argument_index == 0u &&
        program->values[edge_value_at(program, 5u)].kind ==
            W_SEED_HIR0_VALUE_PARAMETER_READ);
  CHECK(program->terminators[6].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        program->values[program->terminators[6].value_index].kind ==
            W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        program->values[program->terminators[6].value_index]
                .block_argument_index == 1u &&
        program->terminators[7].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_UNIT &&
        w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_block saved_inner_join = fixture.hir_blocks[4];
  fixture.hir_blocks[4].owner_function = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_blocks[4] = saved_inner_join;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_terminator saved_outer_branch = fixture.hir_terminators[0];
  fixture.hir_terminators[0].target_block = 2u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[0] = saved_outer_branch;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_value saved_outer_incoming =
      fixture.hir_values[edge_value_at(program, 4u)];
  const uint32_t outer_incoming_index = edge_value_at(program, 4u);
  fixture.hir_values[outer_incoming_index].block_argument_index = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[outer_incoming_index] = saved_outer_incoming;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const uint32_t inner_incoming_index = edge_value_at(program, 2u);
  const w_seed_hir0_value saved_inner_incoming =
      fixture.hir_values[inner_incoming_index];
  fixture.hir_values[inner_incoming_index].parameter_index = 5u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[inner_incoming_index] = saved_inner_incoming;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  size_t outer_expression = SIZE_MAX;
  size_t outer_span = 0u;
  for (size_t index = 0u; index < fixture.result.written.expressions;
       index += 1u) {
    const w_seed_frontend_expression *candidate = &fixture.expressions[index];
    if (candidate->kind != W_SEED_FRONTEND_EXPR_IF ||
        candidate->span.end_byte < candidate->span.start_byte)
      continue;
    const size_t span = candidate->span.end_byte - candidate->span.start_byte;
    if (span > outer_span) {
      outer_expression = index;
      outer_span = span;
    }
  }
  CHECK(outer_expression != SIZE_MAX);
  const w_seed_frontend_expression saved_outer_expression =
      fixture.expressions[outer_expression];
  CHECK(saved_outer_expression.left != W_SEED_FRONTEND_NONE &&
        fixture.expressions[saved_outer_expression.left].inferred_type !=
            saved_outer_expression.inferred_type);
  fixture.expressions[outer_expression].inferred_type =
      fixture.expressions[saved_outer_expression.left].inferred_type;
  setup_hir_output();
  fill_hir_output(0xa5u);
  w_seed_hir0_result rejected;
  (void)memset(&rejected, 0x42, sizeof(rejected));
  const w_seed_hir0_result rejected_snapshot = rejected;
  const w_seed_hir0_input input = hir_input();
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_UNSUPPORTED);
  CHECK(hir_output_is_byte(0xa5u));
  CHECK(memcmp(&rejected, &rejected_snapshot, sizeof(rejected)) == 0);
  fixture.expressions[outer_expression] = saved_outer_expression;
  return true;
}

static bool test_if_diamond_cfg(void) {
  static const char SOURCE[] =
      "fn serve(isOpen: Bool) { "
      "if isOpen { print(message: \"Kitchen open\", suffix: \"\") } "
      "else { print(message: \"Kitchen closed\", suffix: \"\") } "
      "print(message: \"After service\", suffix: \"\") }\n"
      "fn main() { serve(isOpen: true) serve(isOpen: false) }\n"
      "entry(main)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 2u && program->block_count == 5u &&
        program->terminator_count == 5u && program->call_count == 5u &&
        program->entry_count == 1u);
  CHECK(program->functions[0].first_block == 0u &&
        program->functions[0].block_count == 4u &&
        program->functions[1].first_block == 4u &&
        program->functions[1].block_count == 1u);
  CHECK(program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[0].target_block == 1u &&
        program->terminators[0].else_block == 2u &&
        program->terminators[1].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[1].target_block == 3u &&
        program->terminators[2].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[2].target_block == 3u &&
        program->terminators[3].kind == W_SEED_HIR0_TERMINATOR_RETURN_UNIT &&
        program->terminators[4].kind == W_SEED_HIR0_TERMINATOR_RETURN_UNIT);
  CHECK(program->values[program->terminators[0].value_index].type_index == 3u &&
        program->values[program->terminators[0].value_index].owner_kind ==
            W_SEED_HIR0_VALUE_OWNER_TERMINATOR);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_terminator saved_branch = fixture.hir_terminators[0];
  fixture.hir_terminators[0].target_block = 4u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[0] = saved_branch;
  fixture.hir_terminators[0].else_block = 3u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[0] = saved_branch;
  const w_seed_hir0_terminator saved_then = fixture.hir_terminators[1];
  fixture.hir_terminators[1].target_block = 2u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[1].target_block = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[1] = saved_then;

  const uint32_t branch_value = saved_branch.value_index;
  const w_seed_hir0_value saved_condition = fixture.hir_values[branch_value];
  fixture.hir_values[branch_value].type_index = 2u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[branch_value] = saved_condition;
  fixture.hir_values[branch_value].owner_kind =
      W_SEED_HIR0_VALUE_OWNER_ARGUMENT;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[branch_value] = saved_condition;

  const w_seed_hir0_block saved_entry = fixture.hir_blocks[0];
  fixture.hir_blocks[0].next_block = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_blocks[0] = saved_entry;
  const w_seed_hir0_block saved_then_block = fixture.hir_blocks[1];
  fixture.hir_blocks[1].first_instruction += 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_blocks[1] = saved_then_block;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_if_without_else_cfg(void) {
  static const char SOURCE[] =
      "fn main() { if true { let label = \"Open\" "
      "print(message: label, suffix: \"\") } "
      "print(message: \"After\", suffix: \"\") }\n"
      "entry(main)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 1u && program->block_count == 4u &&
        program->call_count == 2u && program->binding_count == 1u &&
        program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[1].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[2].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->blocks[2].instruction_count == 0u &&
        program->terminators[3].kind == W_SEED_HIR0_TERMINATOR_RETURN_UNIT);
  CHECK(program->blocks[1].instruction_count == 2u &&
        program->bindings[0].owner_block == 1u &&
        program->values[1].kind == W_SEED_HIR0_VALUE_BINDING_READ);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_sequential_if_diamonds(void) {
  static const char SOURCE[] =
      "fn main() { if true { print(message: \"first\", suffix: \"\") } "
      "print(message: \"middle\", suffix: \"\") "
      "if false { print(message: \"second\", suffix: \"\") } "
      "else { print(message: \"third\", suffix: \"\") } "
      "print(message: \"after\", suffix: \"\") }\n"
      "entry(main)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 1u && program->block_count == 7u &&
        program->terminator_count == 7u);
  CHECK(program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[0].target_block == 1u &&
        program->terminators[0].else_block == 2u &&
        program->terminators[1].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[1].target_block == 3u &&
        program->terminators[2].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[2].target_block == 3u &&
        program->terminators[3].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[3].target_block == 4u &&
        program->terminators[3].else_block == 5u &&
        program->terminators[4].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[4].target_block == 6u &&
        program->terminators[5].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[5].target_block == 6u &&
        program->terminators[6].kind == W_SEED_HIR0_TERMINATOR_RETURN_UNIT);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_nested_if_diamonds(void) {
  static const char SOURCE[] =
      "fn serve(isOpen: Bool, isKitchen: Bool) { "
      "if isOpen { print(message: \"outer open\", suffix: \"\") "
      "if isKitchen { print(message: \"inner open\", suffix: \"\") } "
      "else { print(message: \"inner closed\", suffix: \"\") } "
      "print(message: \"outer open after\", suffix: \"\") } "
      "else { print(message: \"outer closed\", suffix: \"\") "
      "if isKitchen { print(message: \"inner open closed\", suffix: \"\") } "
      "else { print(message: \"inner closed closed\", suffix: \"\") } "
      "print(message: \"outer closed after\", suffix: \"\") } "
      "print(message: \"post join\", suffix: \"\") }\n"
      "fn main() { serve(isOpen: true, isKitchen: true) "
      "serve(isOpen: true, isKitchen: false) "
      "serve(isOpen: false, isKitchen: true) "
      "serve(isOpen: false, isKitchen: false) }\n"
      "entry(main)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->functions[0].block_count == 10u &&
        program->functions[1].block_count == 1u &&
        program->block_count == 11u);
  CHECK(program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[0].target_block == 1u &&
        program->terminators[0].else_block == 5u);
  CHECK(program->terminators[1].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[1].target_block == 2u &&
        program->terminators[1].else_block == 3u);
  CHECK(program->terminators[2].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[2].target_block == 4u &&
        program->terminators[3].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[3].target_block == 4u &&
        program->terminators[4].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[4].target_block == 9u);
  CHECK(program->terminators[5].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[5].target_block == 6u &&
        program->terminators[5].else_block == 7u);
  CHECK(program->terminators[6].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[6].target_block == 8u &&
        program->terminators[7].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[7].target_block == 8u &&
        program->terminators[8].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[8].target_block == 9u &&
        program->terminators[9].kind == W_SEED_HIR0_TERMINATOR_RETURN_UNIT);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_terminator saved = fixture.hir_terminators[1];
  fixture.hir_terminators[1].else_block = 4u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[1] = saved;
  fixture.hir_terminators[4].target_block = 5u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[4].target_block = 9u;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_enum_switch_hir(void) {
  static const char SOURCE[] =
      "enum Course { starter main dessert }\n"
      "fn price(course: Course): i64 { return switch course { "
      "case .starter: 10 case .main: 30 case .dessert: 20 } }\n"
      "entry { }\n";
  CHECK(fixture_parse(SOURCE));
  configure_host();
  const w_seed_frontend_status frontend_status =
      w_seed_frontend_run(&fixture.input, &fixture.output, &fixture.result);
  CHECK(frontend_status == W_SEED_FRONTEND_OK);
  setup_hir_output();
  const w_seed_hir0_input input = {
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  w_seed_hir0_counts measured;
  w_seed_hir0_result measure_result;
  CHECK(w_seed_hir0_measure(&input, &measured, &measure_result) ==
        W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output,
                                        &fixture.hir_result,
                                        &fixture.hir_program));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_counts = measured;
  CHECK(fixture.hir_counts.enums == 1u &&
        fixture.hir_counts.enum_cases == 3u &&
        fixture.hir_counts.switch_edges == 3u &&
        fixture.hir_program.switch_edge_count == 3u);
  const w_seed_hir0_program *program = &fixture.hir_program;
  size_t dispatch = SIZE_MAX;
  for (size_t block = 0u; block < program->block_count; block += 1u)
    if (program->terminators[block].kind ==
        W_SEED_HIR0_TERMINATOR_SWITCH_ENUM) {
      CHECK(dispatch == SIZE_MAX);
      dispatch = block;
    }
  CHECK(dispatch != SIZE_MAX);
  const w_seed_hir0_terminator *term = &program->terminators[dispatch];
  CHECK(term->value_index != W_SEED_HIR0_NONE &&
        term->switch_enum_index == 0u && term->first_switch_edge == 0u &&
        term->switch_edge_count == 3u && term->switch_carrier_width == 2u);
  for (size_t ordinal = 0u; ordinal < 3u; ordinal += 1u) {
    const w_seed_hir0_switch_edge *edge =
        &program->switch_edges[ordinal];
    CHECK(edge->owner_terminator == dispatch && edge->ordinal == ordinal &&
          edge->enum_index == 0u && edge->enum_case_index == ordinal &&
          edge->target_block == dispatch + 1u + ordinal &&
          program->terminators[edge->target_block].kind ==
              W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
          program->terminators[edge->target_block].value_index !=
              W_SEED_HIR0_NONE);
  }
  const w_seed_hir0_terminator saved_dispatch =
      fixture.hir_terminators[dispatch];
  const w_seed_hir0_switch_edge saved_edges[3] = {
      fixture.hir_switch_edges[0], fixture.hir_switch_edges[1],
      fixture.hir_switch_edges[2]};
  const w_seed_hir0_terminator saved_arms[3] = {
      fixture.hir_terminators[dispatch + 1u],
      fixture.hir_terminators[dispatch + 2u],
      fixture.hir_terminators[dispatch + 3u]};

  fixture.hir_terminators[dispatch].value_index = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[dispatch] = saved_dispatch;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_terminators[dispatch].switch_carrier_width = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[dispatch] = saved_dispatch;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_terminators[dispatch].switch_edge_count = 2u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[dispatch] = saved_dispatch;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_switch_edges[1].ordinal = 0u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_switch_edges[1] = saved_edges[1];
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_switch_edges[1].enum_index = UINT32_MAX;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_switch_edges[1] = saved_edges[1];
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_switch_edges[1].enum_case_index = 0u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_switch_edges[1] = saved_edges[1];
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_switch_edges[1].target_block = (uint32_t)dispatch;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_switch_edges[1] = saved_edges[1];
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_switch_edges[1].target_block =
      (uint32_t)program->functions[1].first_block;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_switch_edges[1] = saved_edges[1];
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_terminators[dispatch + 2u].value_index =
      W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[dispatch + 2u] = saved_arms[1];
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  setup_hir_output();
  fill_hir_output(0xa5u);
  fixture.hir_output.switch_edge_capacity = measured.switch_edges - 1u;
  w_seed_hir0_result rejected;
  (void)memset(&rejected, 0x42, sizeof(rejected));
  const w_seed_hir0_result rejected_before = rejected;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(0xa5u) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);
  return true;
}

static bool test_enum_payload_captures(void) {
  static const char SOURCE[] =
      "enum Course { starter main(price: i64, tax: i64) dessert(value: i64) }\n"
      "fn total(amount: i64, fee: i64): i64 { return amount + fee }\n"
      "fn bill(course: Course): i64 { return switch course { "
      "case .dessert(value: let sweet): sweet "
      "case .main(tax: let fee, price: let amount): total(amount: amount, fee: fee) "
      "case .starter: 10 } }\n"
      "fn rebate(course: Course): i64 { return switch course { "
      "case .starter: 0 case .main(price: let amount, ...): amount "
      "case .dessert(value: _): 0 } }\n"
      "entry { let order: Course = .main(tax: 2, price: 30) "
      "let total = bill(course: order) }\n";
  CHECK(lower(SOURCE));
  w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->switch_capture_count == 4u &&
        program->switch_edges[0].capture_count == 0u &&
        program->switch_edges[1].first_capture == 0u &&
        program->switch_edges[1].capture_count == 2u &&
        program->switch_edges[2].first_capture == 2u);
  CHECK(program->switch_captures[0].owner_switch_edge == 1u &&
        program->switch_captures[0].parameter_ordinal == 1u &&
        program->switch_captures[1].parameter_ordinal == 0u &&
        hir_text_is(program, program->switch_captures[0].name, "fee") &&
        hir_text_is(program, program->switch_captures[1].name, "amount"));
  uint32_t amount_read = W_SEED_HIR0_NONE;
  size_t reads = 0u;
  for (size_t index = 0u; index < program->value_count; index += 1u) {
    if (program->values[index].kind == W_SEED_HIR0_VALUE_PATTERN_CAPTURE_READ) {
      reads += 1u;
      if (program->values[index].pattern_capture_index == 1u)
        amount_read = (uint32_t)index;
    }
  }
  CHECK(reads == 4u && amount_read != W_SEED_HIR0_NONE &&
        program->switch_captures[3].owner_switch_edge == 4u);
  /* Producer validation must reject a capture reference outside its arm. */
  bool forged_read_checked = false;
  for (size_t index = 0u; index < fixture.result.written.expressions; index += 1u) {
    w_seed_frontend_expression *expression = &fixture.expressions[index];
    if (expression->resolved_pattern_capture == 2u) {
      const w_seed_frontend_expression saved_expression = *expression;
      expression->resolved_pattern_capture = 0u;
      expression->spelling = fixture.pattern_captures[0].name;
      w_seed_hir0_counts counts = fixture.hir_counts;
      w_seed_hir0_result result = fixture.hir_result;
      const w_seed_hir0_input forged_input = hir_input();
      CHECK(w_seed_hir0_measure(&forged_input, &counts, &result) != W_SEED_HIR0_OK);
      CHECK(memcmp(&counts, &fixture.hir_counts, sizeof(counts)) == 0 &&
            memcmp(&result, &fixture.hir_result, sizeof(result)) == 0);
      *expression = saved_expression;
      forged_read_checked = true;
      break;
    }
  }
  CHECK(forged_read_checked);
  const w_seed_hir0_switch_capture saved = fixture.hir_switch_captures[0];
  for (unsigned mutation = 0u; mutation < 5u; mutation += 1u) {
    fixture.hir_switch_captures[0] = saved;
    switch (mutation) {
      case 0u: fixture.hir_switch_captures[0].owner_switch_edge = UINT32_MAX; break;
      case 1u: fixture.hir_switch_captures[0].parameter_ordinal = 0u; break;
      case 2u: fixture.hir_switch_captures[0].type_index = 3u; break;
      case 3u: fixture.hir_switch_captures[0].ordinal = 1u; break;
      default: fixture.hir_switch_captures[0].name.count = 0u; break;
    }
    reseal_hir_fixture();
    CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  }
  fixture.hir_switch_captures[0] = saved;
  /* A same-typed capture from another arm is invalid even with fresh digests. */
  fixture.hir_values[amount_read].pattern_capture_index = 2u;
  reseal_hir_fixture();
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[amount_read].pattern_capture_index = 1u;
  reseal_hir_fixture();
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_input input = hir_input();
  w_seed_hir0_result rejected;
  (void)memset(&rejected, 0x42, sizeof(rejected));
  const w_seed_hir0_result before = rejected;
  setup_hir_output();
  fill_hir_output(0xa5);
  fixture.hir_output.switch_capture_capacity =
      fixture.hir_counts.switch_captures - 1u;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(0xa5) && memcmp(&rejected, &before, sizeof(before)) == 0);
  setup_hir_output();
  fixture.hir_output.switch_captures = NULL;
  fixture.hir_output.switch_capture_capacity = 0u;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(0xa5) && memcmp(&rejected, &before, sizeof(before)) == 0);
  setup_hir_output();
  fixture.hir_output.switch_captures =
      (w_seed_hir0_switch_capture *)(void *)fixture.hir_switch_edges;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) != W_SEED_HIR0_OK);
  CHECK(hir_output_is_byte(0xa5) && memcmp(&rejected, &before, sizeof(before)) == 0);

  CHECK(lower("enum Course { main(price: i64, tax: i64) }\n"
              "fn bill(course: Course): i64 { return switch course { "
              "case .main(price: let amount, ...): amount } }\nentry { }\n"));
  CHECK(fixture.hir_program.switch_capture_count == 1u);
  CHECK(lower("enum Course { main(i64, i64) }\n"
              "fn bill(course: Course): i64 { return switch course { "
              "case .main(_, let amount): amount } }\nentry { }\n"));
  CHECK(fixture.hir_program.switch_capture_count == 1u &&
        fixture.hir_switch_captures[0].parameter_ordinal == 1u);
  (void)memset(fixture.pattern_captures, 0, sizeof(fixture.pattern_captures));
  (void)memset(fixture.source_bytes, 0, sizeof(fixture.source_bytes));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool test_enum_switch_local_calls(void) {
  static const char SOURCE[] =
      "enum Course { starter main dessert }\n"
      "fn ten(): i64 { return 10 }\n"
      "fn thirty(value: i64): i64 { return value }\n"
      "fn price(course: Course): i64 { return switch course { "
      "case .dessert: ten() case .starter: thirty(value: 10) "
      "case .main: 30 } }\n"
      "entry { }\n";
  CHECK(fixture_parse(SOURCE));
  configure_host();
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output, &fixture.result) ==
        W_SEED_FRONTEND_OK);
  setup_hir_output();
  const w_seed_hir0_input input = {
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  w_seed_hir0_counts measured;
  w_seed_hir0_result measure_result;
  CHECK(w_seed_hir0_measure(&input, &measured, &measure_result) ==
        W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &fixture.hir_result) ==
        W_SEED_HIR0_OK);
  CHECK(w_seed_hir0_program_from_output(&fixture.hir_output,
                                        &fixture.hir_result,
                                        &fixture.hir_program));
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_counts = measured;
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(measured.functions == 4u && measured.blocks == 7u &&
        measured.instructions == 2u && measured.calls == 2u &&
        measured.arguments == 1u && measured.switch_edges == 3u);
  const size_t dispatch = program->functions[2].first_block;
  CHECK(program->terminators[dispatch].kind ==
            W_SEED_HIR0_TERMINATOR_SWITCH_ENUM &&
        program->terminators[dispatch].switch_edge_count == 3u);
  const w_seed_hir0_switch_edge *starter = &program->switch_edges[0];
  const w_seed_hir0_switch_edge *main_course = &program->switch_edges[1];
  const w_seed_hir0_switch_edge *dessert = &program->switch_edges[2];
  CHECK(starter->enum_case_index == 0u && starter->target_block == dispatch + 1u &&
        main_course->enum_case_index == 1u &&
        main_course->target_block == dispatch + 2u &&
        dessert->enum_case_index == 2u &&
        dessert->target_block == dispatch + 3u);
  CHECK(program->blocks[starter->target_block].instruction_count == 1u &&
        program->blocks[dessert->target_block].instruction_count == 1u &&
        program->blocks[main_course->target_block].instruction_count == 0u);
  CHECK(program->calls[0].owner_block == starter->target_block &&
        program->calls[0].argument_count == 1u &&
        program->calls[1].owner_block == dessert->target_block &&
        program->calls[1].argument_count == 0u);
  CHECK(program->values[program->arguments[0].value_index].kind ==
            W_SEED_HIR0_VALUE_CONST_I64);
  CHECK(program->values[program->terminators[starter->target_block].value_index]
                .kind == W_SEED_HIR0_VALUE_CALL_RESULT &&
        program->values[program->terminators[dessert->target_block].value_index]
                .kind == W_SEED_HIR0_VALUE_CALL_RESULT);
  return true;
}

static bool test_enum_subset_hir(void) {
  static const char SOURCE[] =
      "enum Stage { accepted reserving preparing serving completed }\n"
      "alias WorkStage = Stage<[.serving, .preparing]>\n"
      "alias ReorderedStage = Stage<[.preparing, .serving]>\n"
      "fn label(stage: WorkStage): i64 { return switch stage { "
      "case .serving: 2 case .preparing: 1 } }\n"
      "fn reordered(stage: ReorderedStage): i64 { "
      "return label(stage: stage) }\n"
      "entry { let preparing = reordered(stage: .preparing) "
      "let serving = label(stage: .serving) }\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(fixture.hir_counts.enums == 1u &&
        fixture.hir_counts.enum_cases == 5u &&
        fixture.hir_counts.enum_subsets == 1u &&
        fixture.hir_counts.enum_subset_members == 2u &&
        program->enum_subset_member_count == 2u &&
        fixture.hir_counts.receipt_bytes == W_SEED_HIR0_MAX_RECEIPT_BYTES &&
        fixture.hir_result.required.receipt_bytes ==
            W_SEED_HIR0_MAX_RECEIPT_BYTES &&
        fixture.hir_result.written.receipt_bytes ==
            W_SEED_HIR0_MAX_RECEIPT_BYTES);
  CHECK(program->types[4].kind == W_SEED_HIR0_TYPE_ENUM &&
        program->types[4].enum_index == 0u &&
        program->types[5].kind == W_SEED_HIR0_TYPE_ENUM_SUBSET &&
        program->types[5].enum_index == 0u &&
        program->types[5].first_subset_member == 0u &&
        program->types[5].subset_member_count == 2u &&
        program->types[5].lifecycle == W_SEED_HIR0_LIFECYCLE_VALUE_COPY &&
        program->types[5].release_contract ==
            W_SEED_HIR0_RELEASE_CONTRACT_NONE);
  CHECK(program->enum_subset_members[0].owner_type == 5u &&
        program->enum_subset_members[0].ordinal == 0u &&
        program->enum_subset_members[0].enum_index == 0u &&
        program->enum_subset_members[0].enum_case_index == 2u &&
        program->enum_subset_members[1].owner_type == 5u &&
        program->enum_subset_members[1].ordinal == 1u &&
        program->enum_subset_members[1].enum_index == 0u &&
        program->enum_subset_members[1].enum_case_index == 3u &&
        program->enum_cases[2].tag == 2u && program->enum_cases[3].tag == 3u);

  size_t subset_type_count = 0u;
  uint32_t first_subset_type = W_SEED_FRONTEND_NONE;
  for (size_t type = 0u; type < fixture.result.written.types; type += 1u) {
    if (fixture.types[type].kind != W_SEED_FRONTEND_TYPE_ENUM_SUBSET) continue;
    if (first_subset_type == W_SEED_FRONTEND_NONE)
      first_subset_type = (uint32_t)type;
    CHECK(hir_type_from_frontend(&fixture.output, &fixture.result,
                                 (uint32_t)type) == 5u);
    subset_type_count += 1u;
  }
  CHECK(subset_type_count >= 2u && first_subset_type != W_SEED_FRONTEND_NONE);

  uint8_t semantic_digest[32];
  (void)memcpy(semantic_digest, fixture.hir_result.semantic_digest,
               sizeof(semantic_digest));
  static const char REORDERED_SOURCE[] =
      "enum Stage { accepted reserving preparing serving completed }\n"
      "alias ReorderedStage = Stage<[.preparing, .serving]>\n"
      "alias WorkStage = Stage<[.serving, .preparing]>\n"
      "fn label(stage: WorkStage): i64 { return switch stage { "
      "case .serving: 2 case .preparing: 1 } }\n"
      "fn reordered(stage: ReorderedStage): i64 { "
      "return label(stage: stage) }\n"
      "entry { let preparing = reordered(stage: .preparing) "
      "let serving = label(stage: .serving) }\n";
  CHECK(lower(REORDERED_SOURCE));
  program = &fixture.hir_program;
  CHECK(memcmp(semantic_digest, fixture.hir_result.semantic_digest,
               sizeof(semantic_digest)) == 0 &&
        fixture.hir_counts.enum_subsets == 1u &&
        fixture.hir_counts.enum_subset_members == 2u);

  size_t dispatch = SIZE_MAX;
  for (size_t block = 0u; block < program->block_count; block += 1u)
    if (program->terminators[block].kind ==
        W_SEED_HIR0_TERMINATOR_SWITCH_ENUM) {
      CHECK(dispatch == SIZE_MAX);
      dispatch = block;
    }
  CHECK(dispatch != SIZE_MAX);
  const w_seed_hir0_terminator *term = &program->terminators[dispatch];
  CHECK(term->value_index != W_SEED_HIR0_NONE &&
        program->values[term->value_index].type_index == 5u &&
        term->switch_enum_index == 0u && term->switch_edge_count == 2u &&
        term->switch_carrier_width == 3u);
  CHECK(program->switch_edges[0].enum_case_index == 2u &&
        program->switch_edges[1].enum_case_index == 3u &&
        program->switch_edges[0].ordinal == 0u &&
        program->switch_edges[1].ordinal == 1u &&
        program->switch_edges[0].target_block == dispatch + 1u &&
        program->switch_edges[1].target_block == dispatch + 2u);
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_type saved_type = fixture.hir_types[5];
  const w_seed_hir0_enum_subset_member saved_member0 =
      fixture.hir_enum_subset_members[0];
  const w_seed_hir0_enum_subset_member saved_member1 =
      fixture.hir_enum_subset_members[1];
  const w_seed_hir0_terminator saved_dispatch =
      fixture.hir_terminators[dispatch];
  const w_seed_hir0_switch_edge saved_edge0 = fixture.hir_switch_edges[0];
  const w_seed_hir0_switch_edge saved_edge1 = fixture.hir_switch_edges[1];
  const w_seed_hir0_value saved_subject =
      fixture.hir_values[saved_dispatch.value_index];
  const w_seed_hir0_enum saved_enum = fixture.hir_enums[0];
  const w_seed_hir0_enum_case saved_case0 = fixture.hir_enum_cases[0];
  const w_seed_hir0_enum_case saved_case2 = fixture.hir_enum_cases[2];
  const w_seed_hir0_enum_case saved_case3 = fixture.hir_enum_cases[3];

  uint32_t excluded_value = W_SEED_HIR0_NONE;
  for (size_t value = 0u; value < program->value_count; value += 1u) {
    if (program->values[value].kind == W_SEED_HIR0_VALUE_ENUM_CASE &&
        program->values[value].type_index == 5u) {
      excluded_value = (uint32_t)value;
      break;
    }
  }
  CHECK(excluded_value != W_SEED_HIR0_NONE);
  const w_seed_hir0_value saved_excluded =
      fixture.hir_values[excluded_value];
  fixture.hir_values[excluded_value].enum_case_index = 0u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[excluded_value] = saved_excluded;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_types[5].subset_member_count = 0u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_types[5] = saved_type;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_types[5].subset_member_count = 5u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_types[5] = saved_type;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_program.enum_subset_member_count = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_program.enum_subset_member_count = 2u;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_enum_subset_members[0].owner_type = 4u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_subset_members[0] = saved_member0;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_subset_members[1].ordinal = 0u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_subset_members[1] = saved_member1;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_subset_members[1].enum_case_index = 2u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_subset_members[1] = saved_member1;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_subset_members[0].enum_index = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_subset_members[0] = saved_member0;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_subset_members[0].enum_case_index = 99u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_subset_members[0] = saved_member0;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_subset_members[0].enum_case_index = 3u;
  fixture.hir_enum_subset_members[1].enum_case_index = 2u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_subset_members[0] = saved_member0;
  fixture.hir_enum_subset_members[1] = saved_member1;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  fixture.hir_enum_cases[0].payload_count = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_cases[0] = saved_case0;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[dispatch].switch_edge_count = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[dispatch] = saved_dispatch;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[dispatch].switch_edge_count = 3u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[dispatch] = saved_dispatch;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[dispatch].switch_carrier_width = 1u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_terminators[dispatch] = saved_dispatch;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[saved_dispatch.value_index].type_index = 4u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[saved_dispatch.value_index] = saved_subject;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_switch_edges[0].enum_case_index = 3u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_switch_edges[0] = saved_edge0;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_switch_edges[1].enum_case_index = 2u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_switch_edges[1] = saved_edge1;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_cases[2].tag = 0u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_cases[2] = saved_case2;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_cases[3].tag = 2u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enum_cases[3] = saved_case3;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enums[0].case_count = 4u;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_enums[0] = saved_enum;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));

  const w_seed_hir0_input input = hir_input();
  const uint8_t sentinel = 0xa5u;
  w_seed_hir0_result rejected;
  (void)memset(&rejected, 0x42, sizeof(rejected));
  const w_seed_hir0_result rejected_before = rejected;

  setup_hir_output();
  fill_hir_output(sentinel);
  fixture.hir_output.enum_subset_member_capacity =
      fixture.hir_counts.enum_subset_members - 1u;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(sentinel) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  setup_hir_output();
  fill_hir_output(sentinel);
  fixture.hir_output.enum_subset_members = NULL;
  rejected = rejected_before;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(sentinel) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  setup_hir_output();
  fill_hir_output(sentinel);
  w_seed_hir0_output alias = fixture.hir_output;
  alias.enum_subset_members =
      (w_seed_hir0_enum_subset_member *)(void *)alias.types;
  rejected = rejected_before;
  CHECK(w_seed_hir0_run(&input, &alias, &rejected) == W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(sentinel) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);
  return true;
}

static bool test_enum_switch_cfg_composition_barrier(void) {
  static const char SOURCE[] =
      "enum Course { starter main dessert }\n"
      "fn price(course: Course): i64 { "
      "let seed = if true { 1 } else { 2 } "
      "return switch course { case .starter: 10 case .main: 30 "
      "case .dessert: 20 } }\n"
      "entry { }\n";
  CHECK(fixture_parse(SOURCE));
  configure_host();
  CHECK(w_seed_frontend_run(&fixture.input, &fixture.output, &fixture.result) ==
        W_SEED_FRONTEND_OK);
  setup_hir_output();
  const w_seed_hir0_input input = {
      .frontend_input = &fixture.input,
      .frontend_output = &fixture.output,
      .frontend_result = &fixture.result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  w_seed_hir0_counts measured;
  w_seed_hir0_result measure_result;
  CHECK(w_seed_hir0_measure(&input, &measured, &measure_result) ==
        W_SEED_HIR0_UNSUPPORTED);
  return true;
}

static bool append_source(char *buffer, size_t capacity, size_t *offset,
                          const char *text) {
  if (buffer == NULL || offset == NULL || text == NULL || *offset > capacity)
    return false;
  const size_t count = strlen(text);
  if (count > capacity - *offset) return false;
  (void)memcpy(buffer + *offset, text, count);
  *offset += count;
  buffer[*offset] = '\0';
  return true;
}

static bool make_nested_if_source(char *buffer, size_t capacity, size_t depth) {
  size_t offset = 0u;
  if (!append_source(buffer, capacity, &offset, "fn main() { ")) return false;
  for (size_t level = 0u; level < depth; level += 1u)
    if (!append_source(buffer, capacity, &offset, "if true { ")) return false;
  if (!append_source(buffer, capacity, &offset,
                     "print(message: \"nested\", suffix: \"\") "))
    return false;
  for (size_t level = 0u; level < depth; level += 1u)
    if (!append_source(buffer, capacity, &offset, "} ")) return false;
  return append_source(buffer, capacity, &offset, "}\nentry(main)\n");
}

static bool make_nested_scalar_if_source(char *buffer, size_t capacity,
                                         size_t depth) {
  size_t offset = 0u;
  if (!append_source(buffer, capacity, &offset,
                     "fn choose(flag: Bool, value: i64): i64 { return "))
    return false;
  for (size_t level = 0u; level < depth; level += 1u)
    if (!append_source(buffer, capacity, &offset, "if flag { ")) return false;
  if (!append_source(buffer, capacity, &offset, "value")) return false;
  for (size_t level = 0u; level < depth; level += 1u)
    if (!append_source(buffer, capacity, &offset, " } else { value }"))
      return false;
  return append_source(buffer, capacity, &offset,
                       " }\nentry(choose)\n");
}

static bool test_nested_if_depth_boundary(void) {
  char source[TEST_SOURCE];
  CHECK(make_nested_if_source(source, sizeof(source) - 1u,
                              W_SEED_HIR0_MAX_NESTING));
  CHECK(fixture_frontend(source));
  setup_hir_output();
  const w_seed_hir0_input input = hir_input();
  w_seed_hir0_counts counts;
  w_seed_hir0_result result;
  CHECK(w_seed_hir0_measure(&input, &counts, &result) == W_SEED_HIR0_OK);
  CHECK(counts.blocks == 1u + W_SEED_HIR0_MAX_NESTING * 3u);

  CHECK(make_nested_if_source(source, sizeof(source) - 1u,
                              W_SEED_HIR0_MAX_NESTING + 1u));
  CHECK(fixture_frontend(source));
  setup_hir_output();
  CHECK(w_seed_hir0_measure(&input, &counts, &result) ==
        W_SEED_HIR0_UNSUPPORTED);
  return true;
}

static bool test_nested_scalar_if_depth_boundary(void) {
  char source[TEST_SOURCE];
  CHECK(make_nested_scalar_if_source(source, sizeof(source) - 1u,
                                     W_SEED_HIR0_MAX_NESTING));
  CHECK(fixture_frontend(source));
  setup_hir_output();
  const w_seed_hir0_input input = hir_input();
  w_seed_hir0_counts counts;
  w_seed_hir0_result result;
  CHECK(w_seed_hir0_measure(&input, &counts, &result) == W_SEED_HIR0_OK);
  CHECK(counts.blocks == 1u + W_SEED_HIR0_MAX_NESTING * 3u);

  CHECK(make_nested_scalar_if_source(source, sizeof(source) - 1u,
                                     W_SEED_HIR0_MAX_NESTING + 1u));
  CHECK(fixture_frontend(source));
  setup_hir_output();
  fill_hir_output(0xa5u);
  (void)memset(&result, 0x42, sizeof(result));
  const w_seed_hir0_result snapshot = result;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &result) ==
        W_SEED_HIR0_UNSUPPORTED);
  CHECK(hir_output_is_byte(0xa5u));
  CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
  return true;
}

static bool test_logical_and_diamond_positive(void) {
  static const char SOURCE[] =
      "fn rhs(): Bool { return true }\n"
      "fn allowed(left: Bool): Bool { return left && rhs() }\n"
      "entry(allowed)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 2u && program->block_count == 5u &&
        program->block_argument_count == 1u && program->call_count == 1u &&
        program->value_count == 5u);
  const w_seed_hir0_terminator *branch = &program->terminators[1];
  CHECK(branch->kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        branch->logical_operator == W_SEED_HIR0_LOGICAL_AND &&
        branch->target_block == 2u && branch->else_block == 3u &&
        edge_value_for(program, branch) == W_SEED_HIR0_NONE);
  CHECK(program->blocks[4].block_argument_count == 1u &&
        program->blocks[4].first_block_argument == 0u &&
        program->block_arguments[0].owner_block == 4u &&
        program->block_arguments[0].ordinal == 0u &&
        program->block_arguments[0].type_index == W_SEED_HIR0_TYPE_BOOL);
  CHECK(program->terminators[2].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[2].target_block == 4u &&
        edge_value_at(program, 2u) == 2u &&
        program->values[2].kind == W_SEED_HIR0_VALUE_CALL_RESULT &&
        program->values[2].type_index == W_SEED_HIR0_TYPE_BOOL &&
        program->calls[0].owner_block == 2u);
  CHECK(program->terminators[3].kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        program->terminators[3].target_block == 4u &&
        edge_value_at(program, 3u) == 3u &&
        program->values[3].kind == W_SEED_HIR0_VALUE_CONST_BOOL &&
        !program->values[3].bool_value);
  CHECK(program->values[4].kind == W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        program->values[4].block_argument_index == 0u &&
        program->terminators[4].value_index == 4u);
  return true;
}

static bool test_logical_unary_not_positive(void) {
  static const char SOURCE[] =
      "fn allowed(left: Bool): Bool { return !left }\n"
      "entry(allowed)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 1u && program->block_count == 1u &&
        program->block_argument_count == 0u && program->value_count == 2u);
  const w_seed_hir0_terminator *return_term = &program->terminators[0];
  CHECK(return_term->kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        return_term->value_index == 1u);
  CHECK(program->values[0].kind == W_SEED_HIR0_VALUE_PARAMETER_READ &&
        program->values[0].type_index == W_SEED_HIR0_TYPE_BOOL &&
        program->values[0].owner_kind == W_SEED_HIR0_VALUE_OWNER_UNARY &&
        program->values[0].owner_index == 1u &&
        program->values[1].kind == W_SEED_HIR0_VALUE_UNARY_BOOL &&
        program->values[1].unary_operator == W_SEED_HIR0_UNARY_NOT &&
        program->values[1].type_index == W_SEED_HIR0_TYPE_BOOL &&
        program->values[1].left_value == 0u);
  return true;
}

static bool test_i64_unary_negate_positive(void) {
  static const char SOURCE[] =
      "fn negate(value: i64): i64 { return -value }\n"
      "entry(negate)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 1u && program->block_count == 1u &&
        program->value_count == 2u &&
        program->terminators[0].kind ==
            W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        program->terminators[0].value_index == 1u);
  CHECK(program->values[0].kind == W_SEED_HIR0_VALUE_PARAMETER_READ &&
        program->values[0].type_index == W_SEED_HIR0_TYPE_I64 &&
        program->values[0].owner_kind == W_SEED_HIR0_VALUE_OWNER_UNARY &&
        program->values[0].owner_index == 1u &&
        program->values[1].kind == W_SEED_HIR0_VALUE_UNARY_I64 &&
        program->values[1].unary_operator == W_SEED_HIR0_UNARY_NEGATE &&
        program->values[1].type_index == W_SEED_HIR0_TYPE_I64 &&
        program->values[1].left_value == 0u);

  const w_seed_hir0_value saved = fixture.hir_values[1];
  fixture.hir_values[1].unary_operator = W_SEED_HIR0_UNARY_NOT;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[1] = saved;
  fixture.hir_values[1].type_index = W_SEED_HIR0_TYPE_BOOL;
  CHECK(!w_seed_hir0_verify(program, &fixture.hir_result));
  fixture.hir_values[1] = saved;
  CHECK(w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_direct_i64_unary_interpolation(void) {
  static const char SOURCE[] =
      "entry { print(message: \"Balance ${-7}\", suffix: \"\") }\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  uint32_t literal = W_SEED_HIR0_NONE;
  uint32_t unary = W_SEED_HIR0_NONE;
  uint32_t interpolation = W_SEED_HIR0_NONE;
  for (size_t index = 0u; index < program->value_count; index += 1u) {
    if (program->values[index].kind == W_SEED_HIR0_VALUE_CONST_I64)
      literal = (uint32_t)index;
    else if (program->values[index].kind == W_SEED_HIR0_VALUE_UNARY_I64)
      unary = (uint32_t)index;
    else if (program->values[index].kind ==
             W_SEED_HIR0_VALUE_INTERPOLATED_STRING)
      interpolation = (uint32_t)index;
  }
  CHECK(literal != W_SEED_HIR0_NONE && unary != W_SEED_HIR0_NONE &&
        interpolation != W_SEED_HIR0_NONE &&
        program->values[literal].type_index == 2u &&
        program->values[literal].integer_value == 7 &&
        program->values[unary].type_index == 2u &&
        program->values[unary].unary_operator == W_SEED_HIR0_UNARY_NEGATE &&
        program->values[unary].left_value == literal &&
        program->values[interpolation].first_interpolation_segment == 0u &&
        program->values[interpolation].interpolation_segment_count == 2u &&
        program->interpolation_segments[1].kind ==
            W_SEED_HIR0_INTERPOLATION_VALUE &&
        program->interpolation_segments[1].value_index == unary &&
        w_seed_hir0_verify(program, &fixture.hir_result));
  return true;
}

static bool test_direct_print_interpolation(void) {
  static const char POSITIVE_SOURCE[] =
      "async fn main() { print(\"Balance ${-7}\") }\n"
      "entry(main)\n";
  CHECK(lower_single_print_host(POSITIVE_SOURCE));
  CHECK(fixture.hir_program.function_count == 1u &&
        fixture.hir_program.functions[0].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE);

  static const char BINDING_SOURCE[] =
      "async fn main() { let message = \"Balance ${-7}\" "
      "print(message) }\n"
      "entry(main)\n";
  CHECK(lower_single_print_host(BINDING_SOURCE));
  CHECK(fixture.hir_program.function_count == 1u &&
        fixture.hir_program.functions[0].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        fixture.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT);
  return true;
}

static bool test_logical_or_diamond_positive(void) {
  static const char SOURCE[] =
      "fn rhs(): Bool { return true }\n"
      "fn allowed(left: Bool): Bool { return left || rhs() }\n"
      "entry(allowed)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 2u && program->block_count == 5u &&
        program->block_argument_count == 1u && program->call_count == 1u);
  const w_seed_hir0_terminator *branch = &program->terminators[1];
  const w_seed_hir0_terminator *skip = &program->terminators[2];
  const w_seed_hir0_terminator *rhs = &program->terminators[3];
  CHECK(branch->kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        branch->logical_operator == W_SEED_HIR0_LOGICAL_OR &&
        branch->target_block == 2u && branch->else_block == 3u &&
        skip->kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        skip->target_block == 4u &&
        rhs->kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        rhs->target_block == 4u &&
        edge_value_for(program, skip) != W_SEED_HIR0_NONE &&
        edge_value_for(program, rhs) != W_SEED_HIR0_NONE);
  CHECK(program->values[edge_value_for(program, skip)].kind ==
            W_SEED_HIR0_VALUE_CONST_BOOL &&
        program->values[edge_value_for(program, skip)].bool_value &&
        program->values[edge_value_for(program, rhs)].kind ==
            W_SEED_HIR0_VALUE_CALL_RESULT &&
        program->values[edge_value_for(program, rhs)].type_index ==
            W_SEED_HIR0_TYPE_BOOL &&
        program->calls[0].owner_block == 3u);
  CHECK(program->blocks[4].block_argument_count == 1u &&
        program->block_arguments[0].owner_block == 4u &&
        program->terminators[4].value_index != W_SEED_HIR0_NONE &&
        program->values[program->terminators[4].value_index].kind ==
            W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ);
  return true;
}

static bool test_nested_logical_positive(void) {
  static const char SOURCE[] =
      "fn rhs(): Bool { return true }\n"
      "fn allowed(left: Bool): Bool { return left && (false || rhs()) }\n"
      "entry(allowed)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 2u && program->block_count == 8u &&
        program->block_argument_count == 2u && program->call_count == 1u);
  CHECK(program->terminators[1].logical_operator ==
            W_SEED_HIR0_LOGICAL_AND &&
        program->terminators[1].target_block == 2u &&
        program->terminators[1].else_block == 6u &&
        program->terminators[2].logical_operator == W_SEED_HIR0_LOGICAL_OR &&
        program->terminators[2].target_block == 3u &&
        program->terminators[2].else_block == 4u);
  CHECK(program->terminators[3].target_block == 5u &&
        edge_value_at(program, 3u) != W_SEED_HIR0_NONE &&
        program->values[edge_value_at(program, 3u)].kind ==
            W_SEED_HIR0_VALUE_CONST_BOOL &&
        program->values[edge_value_at(program, 3u)].bool_value &&
        program->terminators[4].target_block == 5u &&
        edge_value_at(program, 4u) != W_SEED_HIR0_NONE &&
        program->values[edge_value_at(program, 4u)].kind ==
            W_SEED_HIR0_VALUE_CALL_RESULT);
  CHECK(program->terminators[5].target_block == 7u &&
        edge_value_at(program, 5u) != W_SEED_HIR0_NONE &&
        program->values[edge_value_at(program, 5u)].kind ==
            W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        program->terminators[6].target_block == 7u &&
        edge_value_at(program, 6u) != W_SEED_HIR0_NONE &&
        !program->values[edge_value_at(program, 6u)].bool_value &&
        program->blocks[5].block_argument_count == 1u &&
        program->blocks[7].block_argument_count == 1u &&
        program->block_arguments[0].owner_block == 5u &&
        program->block_arguments[1].owner_block == 7u);
  return true;
}

static bool test_logical_rhs_call_argument_positive(void) {
  static const char SOURCE[] =
      "fn rhs(flag: Bool): Bool { return flag }\n"
      "fn allowed(): Bool { return false || rhs(flag: true) }\n"
      "entry(allowed)\n";
  CHECK(lower(SOURCE));
  const w_seed_hir0_program *program = &fixture.hir_program;
  CHECK(program->function_count == 2u && program->block_count == 5u &&
        program->block_argument_count == 1u && program->call_count == 1u &&
        program->argument_count == 1u);
  const w_seed_hir0_terminator *branch = &program->terminators[1];
  const w_seed_hir0_terminator *skip = &program->terminators[2];
  const w_seed_hir0_terminator *rhs = &program->terminators[3];
  CHECK(branch->logical_operator == W_SEED_HIR0_LOGICAL_OR &&
        branch->target_block == 2u && branch->else_block == 3u &&
        edge_value_for(program, skip) != W_SEED_HIR0_NONE &&
        program->values[edge_value_for(program, skip)].kind ==
            W_SEED_HIR0_VALUE_CONST_BOOL &&
        program->values[edge_value_for(program, skip)].bool_value &&
        edge_value_for(program, rhs) != W_SEED_HIR0_NONE &&
        program->values[edge_value_for(program, rhs)].kind ==
            W_SEED_HIR0_VALUE_CALL_RESULT &&
        program->calls[0].owner_block == 3u &&
        program->calls[0].argument_count == 1u);
  const w_seed_hir0_argument *argument = &program->arguments[0];
  CHECK(argument->owner_call == 0u && argument->ordinal == 0u &&
        argument->type_index == W_SEED_HIR0_TYPE_BOOL &&
        program->values[argument->value_index].kind ==
            W_SEED_HIR0_VALUE_CONST_BOOL &&
        program->values[argument->value_index].bool_value);
  CHECK(program->blocks[4].block_argument_count == 1u &&
        program->terminators[4].value_index != W_SEED_HIR0_NONE &&
        program->values[program->terminators[4].value_index].kind ==
            W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ);
  return true;
}

static bool test_logical_adversarial_barriers(void) {
  static const char LOGICAL_SOURCE[] =
      "fn rhs(): Bool { return true }\n"
      "fn allowed(left: Bool): Bool { return !((left && rhs()) || rhs()) }\n"
      "entry(allowed)\n";
  CHECK(lower(LOGICAL_SOURCE));
  w_seed_hir0_program *program = &fixture.hir_program;
  w_seed_hir0_result *result = &fixture.hir_result;
  CHECK(program->function_count == 2u && program->block_count == 8u &&
        program->block_argument_count == 2u && program->value_count == 9u);
  const size_t inner_branch = 1u;
  const size_t inner_rhs_jump = 2u;
  const size_t inner_skip_jump = 3u;
  const size_t inner_join = 4u;
  const size_t outer_branch = 4u;
  const size_t outer_join = 7u;
  size_t unary = SIZE_MAX;
  size_t inner_read = SIZE_MAX;
  size_t outer_read = SIZE_MAX;
  for (size_t index = 0u; index < program->value_count; index += 1u) {
    if (program->values[index].kind == W_SEED_HIR0_VALUE_UNARY_BOOL)
      unary = index;
    if (program->values[index].kind ==
        W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ) {
      if (program->values[index].block_argument_index == 0u)
        inner_read = index;
      if (program->values[index].block_argument_index == 1u)
        outer_read = index;
    }
  }
  CHECK(unary != SIZE_MAX && inner_read != SIZE_MAX &&
        outer_read != SIZE_MAX &&
        program->terminators[inner_branch].logical_operator ==
            W_SEED_HIR0_LOGICAL_AND &&
        program->terminators[outer_branch].logical_operator ==
            W_SEED_HIR0_LOGICAL_OR &&
        program->blocks[inner_join].block_argument_count == 1u &&
        program->blocks[outer_join].block_argument_count == 1u);
  const w_seed_hir0_value saved_unary = fixture.hir_values[unary];
  const w_seed_hir0_value saved_inner_read = fixture.hir_values[inner_read];
  const w_seed_hir0_value saved_outer_read = fixture.hir_values[outer_read];
  const w_seed_hir0_value saved_inner_incoming =
      fixture.hir_values[edge_value_at(program, inner_rhs_jump)];
  const w_seed_hir0_value saved_inner_skip =
      fixture.hir_values[edge_value_at(program, inner_skip_jump)];
  const w_seed_hir0_terminator saved_inner_branch =
      fixture.hir_terminators[inner_branch];
  const w_seed_hir0_terminator saved_inner_rhs =
      fixture.hir_terminators[inner_rhs_jump];
  const uint32_t inner_edge_index =
      fixture.hir_terminators[inner_rhs_jump].first_edge_argument;
  const w_seed_hir0_edge_argument saved_inner_edge =
      fixture.hir_edge_arguments[inner_edge_index];
  const w_seed_hir0_block saved_inner_join = fixture.hir_blocks[inner_join];
  const w_seed_hir0_block_argument saved_inner_argument =
      fixture.hir_block_arguments[0];

  fixture.hir_values[unary].unary_operator =
      (w_seed_hir0_unary_operator)(W_SEED_HIR0_UNARY_NOT + 1);
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_values[unary] = saved_unary;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_values[unary].type_index = W_SEED_HIR0_TYPE_I64;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_values[unary] = saved_unary;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_terminators[inner_branch].logical_operator =
      W_SEED_HIR0_LOGICAL_OR;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_terminators[inner_branch] = saved_inner_branch;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_terminators[inner_branch].target_block = (uint32_t)inner_skip_jump;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_terminators[inner_branch] = saved_inner_branch;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_terminators[inner_rhs_jump].target_block = (uint32_t)outer_join;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_terminators[inner_rhs_jump] = saved_inner_rhs;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_edge_arguments[inner_edge_index].owner_terminator =
      (uint32_t)inner_skip_jump;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_edge_arguments[inner_edge_index] = saved_inner_edge;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_edge_arguments[inner_edge_index].owner_block =
      (uint32_t)outer_join;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_edge_arguments[inner_edge_index] = saved_inner_edge;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_edge_arguments[inner_edge_index].ordinal = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_edge_arguments[inner_edge_index] = saved_inner_edge;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_edge_arguments[inner_edge_index].type_index =
      W_SEED_HIR0_TYPE_I64;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_edge_arguments[inner_edge_index] = saved_inner_edge;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_edge_arguments[inner_edge_index].value_index =
      W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_edge_arguments[inner_edge_index] = saved_inner_edge;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_terminators[inner_rhs_jump].first_edge_argument =
      (uint32_t)program->edge_argument_count;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_terminators[inner_rhs_jump] = saved_inner_rhs;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_terminators[inner_rhs_jump].edge_argument_count = 2u;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_terminators[inner_rhs_jump] = saved_inner_rhs;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_terminators[inner_rhs_jump].first_edge_argument =
      W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_terminators[inner_rhs_jump] = saved_inner_rhs;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_terminators[inner_rhs_jump].edge_argument_count = 0u;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_terminators[inner_rhs_jump] = saved_inner_rhs;
  CHECK(w_seed_hir0_verify(program, result));

  const uint32_t inner_incoming_index = edge_value_at(program, inner_rhs_jump);
  FIXTURE_EDGE_VALUE_SLOT(inner_rhs_jump) =
      W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, result));
  FIXTURE_EDGE_VALUE_SLOT(inner_rhs_jump) =
      inner_incoming_index;
  fixture.hir_terminators[inner_rhs_jump] = saved_inner_rhs;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_values[inner_incoming_index].owner_index = (uint32_t)inner_skip_jump;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_values[inner_incoming_index] = saved_inner_incoming;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_values[inner_incoming_index].owner_ordinal = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_values[inner_incoming_index] = saved_inner_incoming;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_values[inner_incoming_index].type_index =
      W_SEED_HIR0_TYPE_I64;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_values[inner_incoming_index] = saved_inner_incoming;
  CHECK(w_seed_hir0_verify(program, result));

  const uint32_t inner_skip_index =
      edge_value_at(program, inner_skip_jump);
  fixture.hir_values[inner_skip_index].bool_value = true;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_values[inner_skip_index] = saved_inner_skip;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_block_arguments[0].owner_block = (uint32_t)outer_join;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_block_arguments[0] = saved_inner_argument;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_block_arguments[0].type_index = W_SEED_HIR0_TYPE_I64;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_block_arguments[0] = saved_inner_argument;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_blocks[inner_join].first_block_argument =
      (uint32_t)program->block_argument_count;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_blocks[inner_join] = saved_inner_join;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_values[outer_read].block_argument_index = 0u;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_values[outer_read] = saved_outer_read;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_values[outer_read].block_argument_index =
      (uint32_t)program->block_argument_count;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_values[outer_read] = saved_outer_read;
  CHECK(w_seed_hir0_verify(program, result));

  fixture.hir_values[outer_read].block_argument_index = W_SEED_HIR0_NONE;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_values[outer_read] = saved_outer_read;
  CHECK(w_seed_hir0_verify(program, result));
  fixture.hir_values[inner_read] = saved_inner_read;

  const w_seed_hir0_input input = hir_input();
  setup_hir_output();
  fill_hir_output(0xa5u);
  w_seed_hir0_result rejected;
  (void)memset(&rejected, 0x42, sizeof(rejected));
  const w_seed_hir0_result rejected_before = rejected;
  fixture.hir_output.block_argument_capacity =
      fixture.hir_counts.block_arguments - 1u;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(0xa5u) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  setup_hir_output();
  fill_hir_output(0xa5u);
  fixture.hir_output.edge_argument_capacity =
      fixture.hir_counts.edge_arguments - 1u;
  (void)memset(&rejected, 0x42, sizeof(rejected));
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(0xa5u) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  setup_hir_output();
  fill_hir_output(0xa5u);
  w_seed_hir0_output alias = fixture.hir_output;
  alias.block_arguments =
      (w_seed_hir0_block_argument *)(void *)alias.blocks;
  (void)memset(&rejected, 0x42, sizeof(rejected));
  CHECK(w_seed_hir0_run(&input, &alias, &rejected) == W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  setup_hir_output();
  fill_hir_output(0xa5u);
  alias = fixture.hir_output;
  alias.edge_arguments =
      (w_seed_hir0_edge_argument *)(void *)alias.blocks;
  (void)memset(&rejected, 0x42, sizeof(rejected));
  CHECK(w_seed_hir0_run(&input, &alias, &rejected) == W_SEED_HIR0_INVALID);
  CHECK(hir_output_is_byte(0xa5u) &&
        memcmp(&rejected, &rejected_before, sizeof(rejected)) == 0);

  static const char NORMAL_SOURCE[] =
      "fn main() { if true { print(message: \"yes\", suffix: \"\") } "
      "else { print(message: \"no\", suffix: \"\") } }\n"
      "entry(main)\n";
  CHECK(lower(NORMAL_SOURCE));
  program = &fixture.hir_program;
  result = &fixture.hir_result;
  CHECK(program->block_argument_count == 0u &&
        program->terminators[0].logical_operator ==
            W_SEED_HIR0_LOGICAL_NONE &&
        edge_value_at(program, 1u) == W_SEED_HIR0_NONE);
  const w_seed_hir0_terminator saved_normal_branch =
      fixture.hir_terminators[0];
  const w_seed_hir0_terminator saved_normal_jump = fixture.hir_terminators[1];
  fixture.hir_terminators[0].logical_operator = W_SEED_HIR0_LOGICAL_AND;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_terminators[0] = saved_normal_branch;
  CHECK(w_seed_hir0_verify(program, result));
  fixture.hir_terminators[1].first_edge_argument = 0u;
  fixture.hir_terminators[1].edge_argument_count = 1u;
  CHECK(!w_seed_hir0_verify(program, result));
  fixture.hir_terminators[1] = saved_normal_jump;
  CHECK(w_seed_hir0_verify(program, result));
  return true;
}

static bool test_signed_comparison_values(void) {
  static const char *const operators[] = {"==", "!=", "<", "<=", ">", ">="};
  static const w_seed_hir0_binary_operator opcodes[] = {
      W_SEED_HIR0_BINARY_EQUAL, W_SEED_HIR0_BINARY_NOT_EQUAL,
      W_SEED_HIR0_BINARY_LESS, W_SEED_HIR0_BINARY_LESS_EQUAL,
      W_SEED_HIR0_BINARY_GREATER, W_SEED_HIR0_BINARY_GREATER_EQUAL};
  for (size_t operation = 0u; operation < 6u; operation += 1u) {
    char source[1024];
    const int written = snprintf(source, sizeof(source),
        "fn fits(left: i64, right: i64): Bool { return left %s right }\n"
        "fn main() { let fits = fits(left: 0 - 9223372036854775807 - 1, "
        "right: 9223372036854775807) "
        "print(message: \"${fits}\", suffix: \"\") }\nentry(main)\n",
        operators[operation]);
    CHECK(written > 0 && (size_t)written < sizeof(source));
    CHECK(lower(source));
    size_t comparison = SIZE_MAX;
    for (size_t index = 0u; index < fixture.hir_program.value_count; index += 1u)
      if (fixture.hir_values[index].kind == W_SEED_HIR0_VALUE_BINARY_I64 &&
          fixture.hir_values[index].binary_operator == opcodes[operation])
        comparison = index;
    CHECK(comparison != SIZE_MAX);
    const w_seed_hir0_value saved = fixture.hir_values[comparison];
    CHECK(saved.type_index == 3u &&
          saved.owner_kind == W_SEED_HIR0_VALUE_OWNER_TERMINATOR &&
          fixture.hir_values[saved.left_value].type_index == 2u &&
          fixture.hir_values[saved.right_value].type_index == 2u &&
          fixture.hir_values[saved.left_value].kind ==
              W_SEED_HIR0_VALUE_PARAMETER_READ &&
          fixture.hir_values[saved.right_value].kind ==
              W_SEED_HIR0_VALUE_PARAMETER_READ);
    /* Original digests remain unchanged: these exercise the combined verifier. */
    for (size_t mutation = 0u; mutation < 7u; mutation += 1u) {
      const w_seed_hir0_value saved_left = fixture.hir_values[saved.left_value];
      switch (mutation) {
        case 0u: fixture.hir_values[comparison].type_index = 2u; break;
        case 1u: fixture.hir_values[saved.left_value].type_index = 3u; break;
        case 2u:
          fixture.hir_values[comparison].binary_operator =
              (w_seed_hir0_binary_operator)UINT32_MAX;
          break;
        case 3u:
          fixture.hir_values[comparison].binary_operator =
              (w_seed_hir0_binary_operator)(W_SEED_HIR0_BINARY_GREATER_EQUAL + 1);
          break;
        case 4u: fixture.hir_values[comparison].left_value = (uint32_t)comparison; break;
        case 5u: fixture.hir_values[saved.left_value].owner_index = 0u; break;
        default: fixture.hir_values[comparison].right_value = saved.left_value; break;
      }
      CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
      fixture.hir_values[comparison] = saved;
      fixture.hir_values[saved.left_value] = saved_left;
      CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
    }
  }

  const w_seed_hir0_input input = hir_input();
  setup_hir_output();
  fill_hir_output(0xa5u);
  fixture.hir_output.value_capacity = 0u;
  w_seed_hir0_result rejected;
  (void)memset(&rejected, 0x42, sizeof(rejected));
  const w_seed_hir0_result snapshot = rejected;
  CHECK(w_seed_hir0_run(&input, &fixture.hir_output, &rejected) ==
        W_SEED_HIR0_CAPACITY);
  CHECK(hir_output_is_byte(0xa5u));
  CHECK(memcmp(&rejected, &snapshot, sizeof(rejected)) == 0);
  return true;
}

static bool test_short_entry_hir(void) {
  static const char SOURCE[] =
      "entry { print(message: \"Hello, world!\", suffix: \"!\") }\n";
  static const char COMMENTED[] =
      "// trivia\nentry {   print(message: \"Hello, world!\", suffix: \"!\")   }\n";
  CHECK(lower(SOURCE));
  CHECK(fixture.hir_program.function_count == 1u &&
        fixture.hir_program.entry_count == 1u);
  const w_seed_hir0_function function = fixture.hir_functions[0];
  const w_seed_hir0_entry entry = fixture.hir_entries[0];
  CHECK(function.is_anonymous_entry && function.parameter_count == 0u &&
        function.return_type == 0u && entry.is_body &&
        entry.target_function == 0u && entry.target_identity == 1u);
  CHECK(function.name.count == strlen("<entry.default>") &&
        entry.target_name.count == function.name.count &&
        memcmp(fixture.hir_text + function.name.offset, "<entry.default>",
               function.name.count) == 0 &&
        memcmp(fixture.hir_text + entry.target_name.offset,
               fixture.hir_text + function.name.offset, function.name.count) ==
            0);
  uint8_t semantic[32];
  uint8_t provenance[32];
  (void)memcpy(semantic, fixture.hir_result.semantic_digest, sizeof(semantic));
  (void)memcpy(provenance, fixture.hir_result.provenance_digest,
               sizeof(provenance));

  fixture.hir_functions[0].is_anonymous_entry = false;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_functions[0] = function;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_entries[0].is_body = false;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_entries[0] = entry;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));

  CHECK(lower(COMMENTED));
  CHECK(memcmp(semantic, fixture.hir_result.semantic_digest, sizeof(semantic)) ==
        0);
  CHECK(memcmp(provenance, fixture.hir_result.provenance_digest,
               sizeof(provenance)) != 0);
  return true;
}

static bool test_function_export_facts(void) {
  static const char SOURCE[] =
      "export fn public() { }\n"
      "fn private() { }\n"
      "entry { }\n";
  CHECK(lower(SOURCE));
  CHECK(fixture.hir_program.function_count == 3u &&
        fixture.hir_program.entry_count == 1u &&
        fixture.hir_program.functions[0].exported &&
        !fixture.hir_program.functions[1].exported &&
        !fixture.hir_program.functions[2].exported &&
        !fixture.hir_program.functions[0].is_anonymous_entry &&
        !fixture.hir_program.functions[1].is_anonymous_entry &&
        fixture.hir_program.functions[2].is_anonymous_entry);

  const w_seed_hir0_function saved_public = fixture.hir_functions[0];
  fixture.hir_functions[0].exported = false;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_functions[0] = saved_public;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));

  const w_seed_hir0_function saved_private = fixture.hir_functions[1];
  fixture.hir_functions[1].exported = true;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_functions[1] = saved_private;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));

  const w_seed_hir0_function saved_entry = fixture.hir_functions[2];
  fixture.hir_functions[2].exported = true;
  CHECK(!w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  fixture.hir_functions[2] = saved_entry;
  CHECK(w_seed_hir0_verify(&fixture.hir_program, &fixture.hir_result));
  return true;
}

static bool emit_parallel_entry_mlir(void) {
  static const char SOURCE[] =
      "fn increment(value: i64): i64 { return value + 1 }\n"
      "fn prepare(value: i64): i64 { return increment(value: value) }\n"
      "fn combine(left: i64, right: i64): i64 { return left + right + 1 }\n"
      "fn unused(value: i64): i64 { return value + 99 }\n"
      "entry { let left = spawn<.domain> prepare(value: 20) "
      "let right = spawn<.domain> combine(right: 2, left: 20) "
      "let first = await left let second = await right }\n";
  CHECK(lower_parallel_domain(SOURCE));
  w_seed_parallel_selection0 selection;
  CHECK(w_seed_parallel_selection0_select(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_PARALLEL_SELECTION0_OK);
  w_seed_parallel_invocation0_plan invocation;
  CHECK(w_seed_parallel_invocation0_select(
            &fixture.hir_program, &fixture.hir_result, &selection,
            &invocation) == W_SEED_PARALLEL_INVOCATION0_OK);
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_parallel_entry_result emitted;
  CHECK(w_seed_mlir0_emit_parallel_entries(
            &fixture.hir_program, &fixture.hir_result, &selection, &invocation,
            &(w_seed_mlir0_parallel_entry_output){artifact, sizeof(artifact)},
            &emitted) == W_SEED_MLIR0_OK &&
        fwrite(artifact, 1u, emitted.written.mlir_bytes, stdout) ==
            emitted.written.mlir_bytes &&
        fflush(stdout) == 0);
  return true;
}

static bool emit_parallel_entry1_mlir(void) {
  static const char SOURCE[] =
      "fn prepare(value: i64): i64 { return value + 1 }\n"
      "entry { let a = spawn<.domain> prepare(value: 20) "
      "let b = spawn<.domain> prepare(value: 22) "
      "let c = spawn<.domain> prepare(value: 24) "
      "let d = spawn<.domain> prepare(value: 26) "
      "let e = spawn<.domain> prepare(value: 28) "
      "let av = await a let bv = await b let cv = await c let dv = await d "
      "let ev = await e }\n";
  CHECK(lower_parallel_domain(SOURCE));
  w_seed_parallel_selection1_counts selection_counts;
  w_seed_parallel_selection1_result selection_result;
  CHECK(w_seed_parallel_selection1_measure(
            &fixture.hir_program, &fixture.hir_result, &selection_counts,
            &selection_result) == W_SEED_PARALLEL_SELECTION1_OK &&
        selection_counts.tasks == 5u);
  w_seed_parallel_selection1_task selection_tasks[5];
  const w_seed_parallel_selection1_output selection_output = {
      selection_tasks, 5u};
  CHECK(w_seed_parallel_selection1_run(
            &fixture.hir_program, &fixture.hir_result, &selection_output,
            &selection_result) == W_SEED_PARALLEL_SELECTION1_OK);
  w_seed_parallel_selection1_program selection;
  CHECK(w_seed_parallel_selection1_program_from_output(
      &selection_output, &selection_result, &selection));

  w_seed_parallel_invocation1_counts invocation_counts;
  w_seed_parallel_invocation1_result invocation_result;
  CHECK(w_seed_parallel_invocation1_measure(
            &fixture.hir_program, &fixture.hir_result, &selection,
            &selection_result, &invocation_counts, &invocation_result) ==
            W_SEED_PARALLEL_INVOCATION1_OK &&
        invocation_counts.tasks == 5u && invocation_counts.arguments == 5u);
  w_seed_parallel_invocation1_task invocation_tasks[5];
  w_seed_parallel_invocation1_argument invocation_arguments[5];
  const w_seed_parallel_invocation1_output invocation_output = {
      invocation_tasks, 5u, invocation_arguments, 5u};
  CHECK(w_seed_parallel_invocation1_run(
            &fixture.hir_program, &fixture.hir_result, &selection,
            &selection_result, &invocation_output, &invocation_result) ==
        W_SEED_PARALLEL_INVOCATION1_OK);
  w_seed_parallel_invocation1_program invocation;
  CHECK(w_seed_parallel_invocation1_program_from_output(
      &invocation_output, &invocation_result, &invocation));

  static uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir1_parallel_entry_result emitted;
  CHECK(w_seed_mlir1_emit_parallel_entries(
            &fixture.hir_program, &fixture.hir_result, &selection,
            &selection_result, &invocation, &invocation_result,
            &(w_seed_mlir0_parallel_entry_output){artifact, sizeof(artifact)},
            &emitted) == W_SEED_MLIR0_OK &&
        fwrite(artifact, 1u, emitted.written.mlir_bytes, stdout) ==
            emitted.written.mlir_bytes &&
        fflush(stdout) == 0);
  return true;
}

static bool emit_process_parallel_mlir(bool windows) {
  CHECK(lower_process_parallel(PROCESS_PARALLEL_MLIR_SOURCE));
  w_seed_parallel_selection0 selection;
  CHECK(w_seed_parallel_selection0_select(
            &fixture.hir_program, &fixture.hir_result, &selection) ==
        W_SEED_PARALLEL_SELECTION0_OK);
  static uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_mlir0_process_parallel_result emitted;
  const w_seed_mlir0_target target = {
      windows ? W_SEED_MLIR0_TARGET_X86_64_PC_WINDOWS_MSVC
              : W_SEED_MLIR0_TARGET_X86_64_UNKNOWN_LINUX_GNU};
  CHECK(w_seed_mlir0_emit_process_parallel(
            &fixture.hir_program, &fixture.hir_result, &selection, &target,
            &(w_seed_mlir0_process_parallel_output){artifact, sizeof(artifact)},
            &emitted) == W_SEED_MLIR0_OK &&
        fwrite(artifact, 1u, emitted.written.mlir_bytes, stdout) ==
            emitted.written.mlir_bytes &&
        fflush(stdout) == 0);
  return true;
}

int main(int argc, char **argv) {
  if (argc == 2 && argv != NULL &&
      strcmp(argv[1], "--emit-parallel-entry-mlir") == 0) {
    if (!emit_parallel_entry_mlir()) return 1;
    return 0;
  }
  if (argc == 2 && argv != NULL &&
      strcmp(argv[1], "--emit-parallel-entry1-mlir") == 0) {
    if (!emit_parallel_entry1_mlir()) return 1;
    return 0;
  }
  if (argc == 2 && argv != NULL &&
      strcmp(argv[1], "--emit-process-parallel-mlir") == 0) {
    if (!emit_process_parallel_mlir(false)) return 1;
    return 0;
  }
  if (argc == 2 && argv != NULL &&
      strcmp(argv[1], "--emit-process-parallel-windows-mlir") == 0) {
    if (!emit_process_parallel_mlir(true)) return 1;
    return 0;
  }
  if (argc != 1) return 2;
  if (!test_scalar_evaluator_edges()) return 1;
  if (!test_frontend_inferred_call_interpolation()) return 1;
  if (!test_explicit_panic_hir()) return 1;
  if (!test_explicit_panic_rejections()) return 1;
  if (!test_process_hir()) return 1;
  if (!test_process_input0_hir()) return 1;
  if (!test_explicit_panic_process_layout()) return 1;
  if (!test_process_arguments_count_hir()) return 1;
  if (!test_process_hir_adversarial()) return 1;
  if (!test_direct_entry_facts()) return 1;
  if (!test_typed_invoke_cleanup_hir()) return 1;
  if (!test_parallel_typed_binding1()) return 1;
  if (!test_direct_entry_effect_barrier()) return 1;
  if (!test_short_entry_hir()) return 1;
  if (!test_signed_comparison_values()) return 1;
  if (!test_canonical_and_copy_boundary()) return 1;
  if (!test_semantic_and_provenance_digests()) return 1;
  if (!test_function_parameter_records()) return 1;
  if (!test_structured_async_elision_hir()) return 1;
  if (!test_parallel_domain_placement_hir()) return 1;
  if (!test_parallel_panic_invocation1()) return 1;
  if (!test_parallel_panic_binding1()) return 1;
  if (!test_process_parallel_composition_hir()) return 1;
  if (!test_process_parallel_mlir()) return 1;
  if (!test_lowering_is_not_hello_hardcoded()) return 1;
  if (!test_local_binding_lowering()) return 1;
  if (!test_straight_line_mutation_ssa()) return 1;
  if (!test_interleaved_mutation_versions()) return 1;
  if (!test_conditional_mutation_merge()) return 1;
  if (!test_bool_mutation_ssa()) return 1;
  if (!test_branch_local_mutation_merge()) return 1;
  if (!test_multi_branch_mutation_merge()) return 1;
  if (!test_while_mutation_ssa()) return 1;
  if (!test_repeat_mutation_ssa()) return 1;
  if (!test_repeat_multi_carrier_ssa()) return 1;
  if (!test_while_multi_carrier_ssa()) return 1;
  if (!test_while_multi_carrier_source_order()) return 1;
  if (!test_while_post_loop_continuation_ssa()) return 1;
  if (!test_while_multi_carrier_general_values()) return 1;
  if (!test_while_multi_carrier_native_subset()) return 1;
  if (!test_while_mutation_barriers()) return 1;
  if (!test_branch_local_mutation_barriers()) return 1;
  if (!test_bindings_across_functions()) return 1;
  if (!test_local_binding_verify_mutations()) return 1;
  if (!test_local_enum_hir()) return 1;
  if (!test_typed_throw_hir()) return 1;
  if (!test_typed_invoke_hir()) return 1;
  if (!test_local_enum_payload_declarations_hir()) return 1;
  if (!test_local_enum_payload_constructor_hir()) return 1;
  if (!test_enum_switch_hir()) return 1;
  if (!test_enum_subset_hir()) return 1;
  if (!test_enum_payload_captures()) return 1;
  if (!test_enum_switch_local_calls()) return 1;
  if (!test_enum_switch_cfg_composition_barrier()) return 1;
  if (!test_capacity_and_alias_barriers()) return 1;
  if (!test_verify_mutations()) return 1;
  if (!test_closed_frontend_barriers()) return 1;
  if (!test_typed_interpolation_value_tree()) return 1;
  if (!test_builtin_display_value_tree()) return 1;
  if (!test_typed_immutable_binding_values()) return 1;
  if (!test_local_unit_call_and_parameter_reads()) return 1;
  if (!test_scalar_return_and_call_result()) return 1;
  if (!test_scalar_if_value_diamond()) return 1;
  if (!test_nested_scalar_if_value_diamond()) return 1;
  if (!test_if_diamond_cfg()) return 1;
  if (!test_if_without_else_cfg()) return 1;
  if (!test_sequential_if_diamonds()) return 1;
  if (!test_nested_if_diamonds()) return 1;
  if (!test_nested_if_depth_boundary()) return 1;
  if (!test_nested_scalar_if_depth_boundary()) return 1;
  if (!test_logical_and_diamond_positive()) return 1;
  if (!test_logical_unary_not_positive()) return 1;
  if (!test_i64_unary_negate_positive()) return 1;
  if (!test_direct_i64_unary_interpolation()) return 1;
  if (!test_direct_print_interpolation()) return 1;
  if (!test_logical_or_diamond_positive()) return 1;
  if (!test_nested_logical_positive()) return 1;
  if (!test_logical_rhs_call_argument_positive()) return 1;
  if (!test_logical_adversarial_barriers()) return 1;
  if (!test_function_export_facts()) return 1;
  (void)puts("hir0 tests: ok");
  return 0;
}
