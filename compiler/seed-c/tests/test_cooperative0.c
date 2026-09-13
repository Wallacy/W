#include "w_seed_native0.h"
#include "w_seed_sha256.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef W_SEED_COOPERATIVE0_FIXTURE_PATH
#define W_SEED_COOPERATIVE0_FIXTURE_PATH "fixtures/restaurant-cooperative0.w"
#endif

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "cooperative0 check failed: %s (%s:%d)\n",       \
                    #condition, __FILE__, __LINE__);                         \
      (void)remove(NEGATIVE_PATH);                                             \
      return false;                                                            \
    }                                                                          \
  } while (0)

static const char NEGATIVE_PATH[] = "w_seed_cooperative0_negative.w";
static const char SOURCE_ID[] = "fixture/restaurant-cooperative0";
static const w_seed_mlir0_target TARGET = {
    W_SEED_MLIR0_TARGET_X86_64_PC_WINDOWS_MSVC};

_Static_assert(W_SEED_COOPERATIVE0_MAX_FUNCTIONS ==
                   W_SEED_HIR0_COOPERATIVE_MAX_FUNCTIONS,
               "frontend/HIR and COOP0 use one helper-state bound");

static w_seed_native0_storage storage;
static uint8_t normal_artifact[W_SEED_MLIR0_MAX_BYTES];
static uint8_t artifact[W_SEED_COOPERATIVE0_MAX_ARTIFACT_BYTES];
static uint8_t stdout_bytes[W_SEED_COOPERATIVE0_MAX_STDOUT_BYTES];
static w_seed_cooperative0_trace_event trace[W_SEED_COOPERATIVE0_MAX_TRACE_EVENTS];

static bool contains_bytes(const uint8_t *bytes, size_t length,
                           const char *needle) {
  if (bytes == NULL || needle == NULL) return false;
  const size_t needle_length = strlen(needle);
  if (needle_length == 0u || needle_length > length) return false;
  for (size_t offset = 0u; offset + needle_length <= length; offset += 1u)
    if (memcmp(bytes + offset, needle, needle_length) == 0) return true;
  return false;
}

static void digest_bytes(const uint8_t *bytes, size_t length,
                         uint8_t digest[32]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, bytes, length);
  w_seed_sha256_final(&state, digest);
}

static bool write_source(const char *source) {
  if (source == NULL) return false;
  FILE *file = fopen(NEGATIVE_PATH, "wb");
  if (file == NULL) return false;
  const size_t length = strlen(source);
  const size_t written = fwrite(source, sizeof(char), length, file);
  const int close_status = fclose(file);
  return written == length && close_status == 0;
}

static w_seed_native0_input native_input_for_path(const char *path) {
  return (w_seed_native0_input){
      .path = path,
      .path_length = strlen(path),
      .logical_source_id = {SOURCE_ID, sizeof(SOURCE_ID) - 1u},
      .target = TARGET,
      .artifact_kind = W_SEED_MLIR0_ARTIFACT_EXECUTABLE};
}

static w_seed_cooperative0_input input_for_path(const char *path) {
  return (w_seed_cooperative0_input){
      .path = path,
      .path_length = strlen(path),
      .logical_source_id = {SOURCE_ID, sizeof(SOURCE_ID) - 1u}};
}

static w_seed_cooperative0_output full_output(void) {
  return (w_seed_cooperative0_output){
      .artifact = artifact,
      .artifact_capacity = sizeof(artifact),
      .stdout_bytes = stdout_bytes,
      .stdout_capacity = sizeof(stdout_bytes),
      .trace = trace,
      .trace_capacity = sizeof(trace) / sizeof(trace[0])};
}

static bool expect_negative_source(const char *source) {
  CHECK(write_source(source));
  const w_seed_cooperative0_input input = input_for_path(NEGATIVE_PATH);
  w_seed_cooperative0_result result;
  (void)memset(&result, 0x61, sizeof(result));
  const w_seed_cooperative0_result result_before = result;
  (void)memset(artifact, 0x62, sizeof(artifact));
  (void)memset(stdout_bytes, 0x63, sizeof(stdout_bytes));
  (void)memset(trace, 0x64, sizeof(trace));
  const w_seed_cooperative0_output output = full_output();
  CHECK(w_seed_native0_run_cooperative_oracle(&input, &storage, &output,
                                              &result) !=
        W_SEED_COOPERATIVE0_OK);
  CHECK(memcmp(&result, &result_before, sizeof(result)) == 0);
  for (size_t index = 0u; index < sizeof(artifact); index += 1u)
    CHECK(artifact[index] == 0x62u);
  for (size_t index = 0u; index < sizeof(stdout_bytes); index += 1u)
    CHECK(stdout_bytes[index] == 0x63u);
  for (size_t index = 0u; index < sizeof(trace) / sizeof(trace[0]); index += 1u)
    CHECK(trace[index].sequence == 0x64646464u);
  (void)remove(NEGATIVE_PATH);
  return true;
}

static bool test_cooperative_fixture(void) {
  const w_seed_native0_input native_input =
      native_input_for_path(W_SEED_COOPERATIVE0_FIXTURE_PATH);
  const w_seed_cooperative0_input input =
      input_for_path(W_SEED_COOPERATIVE0_FIXTURE_PATH);

  /* The same pure source remains on the ordinary W-1582 virtual-elision path. */
  w_seed_native0_result normal_result;
  const w_seed_native0_output normal_output = {normal_artifact,
                                               sizeof(normal_artifact)};
  CHECK(w_seed_native0_run(&native_input, &storage, &normal_output,
                           &normal_result) ==
        W_SEED_NATIVE0_OK);
  CHECK(!contains_bytes(normal_artifact, normal_result.mlir.written.mlir_bytes,
                        "@w_task") &&
        !contains_bytes(normal_artifact, normal_result.mlir.written.mlir_bytes,
                        "@w_async_") &&
        !contains_bytes(normal_artifact, normal_result.mlir.written.mlir_bytes,
                        "yield"));

  (void)memset(artifact, 0xa1, sizeof(artifact));
  (void)memset(stdout_bytes, 0xa2, sizeof(stdout_bytes));
  (void)memset(trace, 0xa3, sizeof(trace));
  w_seed_cooperative0_result result;
  const w_seed_cooperative0_output output = full_output();
  const w_seed_cooperative0_status cooperative_status =
      w_seed_native0_run_cooperative_oracle(&input, &storage, &output, &result);
  CHECK(cooperative_status == W_SEED_COOPERATIVE0_OK);
  CHECK(result.status == W_SEED_COOPERATIVE0_OK &&
        result.plan.phase == W_SEED_COOPERATIVE0_PLAN_INITIAL &&
        result.final_state.phase == W_SEED_COOPERATIVE0_STATE_EXECUTED &&
        result.plan.task_count == 2u && result.plan.frame_count == 2u &&
        result.plan.yield_count == 4u &&
        result.plan.tasks[0].yield_count ==
            W_SEED_HIR0_COOPERATIVE_MAX_YIELDS_PER_TASK &&
        result.plan.tasks[1].yield_count ==
            W_SEED_HIR0_COOPERATIVE_MAX_YIELDS_PER_TASK &&
        result.plan.trace_event_count == 30u &&
        result.required.trace_events == 30u &&
        result.written.trace_events == 30u);
  static const char EXPECTED_STDOUT[] = "Cooperative 88\n";
  CHECK(result.required.stdout_bytes == sizeof(EXPECTED_STDOUT) - 1u &&
        memcmp(stdout_bytes, EXPECTED_STDOUT, sizeof(EXPECTED_STDOUT) - 1u) ==
            0);
  CHECK(contains_bytes(artifact, result.required.artifact_bytes,
                       W_SEED_COOPERATIVE0_SCHEMA_VERSION) &&
        contains_bytes(artifact, result.required.artifact_bytes,
                       "execution=compiler-oracle") &&
        contains_bytes(artifact, result.required.artifact_bytes,
                       "heap=forbidden os_threads=forbidden crt=forbidden"));
  static const w_seed_cooperative0_event_kind EVENTS[] = {
      W_SEED_COOPERATIVE0_EVENT_RESERVE,
      W_SEED_COOPERATIVE0_EVENT_RESERVE,
      W_SEED_COOPERATIVE0_EVENT_PUBLISH,
      W_SEED_COOPERATIVE0_EVENT_PUBLISH,
      W_SEED_COOPERATIVE0_EVENT_DISPATCH,
      W_SEED_COOPERATIVE0_EVENT_RESUME,
      W_SEED_COOPERATIVE0_EVENT_YIELD,
      W_SEED_COOPERATIVE0_EVENT_DISPATCH,
      W_SEED_COOPERATIVE0_EVENT_RESUME,
      W_SEED_COOPERATIVE0_EVENT_YIELD,
      W_SEED_COOPERATIVE0_EVENT_DISPATCH,
      W_SEED_COOPERATIVE0_EVENT_RESUME,
      W_SEED_COOPERATIVE0_EVENT_YIELD,
      W_SEED_COOPERATIVE0_EVENT_DISPATCH,
      W_SEED_COOPERATIVE0_EVENT_RESUME,
      W_SEED_COOPERATIVE0_EVENT_YIELD,
      W_SEED_COOPERATIVE0_EVENT_DISPATCH,
      W_SEED_COOPERATIVE0_EVENT_RESUME,
      W_SEED_COOPERATIVE0_EVENT_SETTLE,
      W_SEED_COOPERATIVE0_EVENT_CLEANUP,
      W_SEED_COOPERATIVE0_EVENT_OUTCOME_COMMIT,
      W_SEED_COOPERATIVE0_EVENT_JOIN,
      W_SEED_COOPERATIVE0_EVENT_RELEASE,
      W_SEED_COOPERATIVE0_EVENT_DISPATCH,
      W_SEED_COOPERATIVE0_EVENT_RESUME,
      W_SEED_COOPERATIVE0_EVENT_SETTLE,
      W_SEED_COOPERATIVE0_EVENT_CLEANUP,
      W_SEED_COOPERATIVE0_EVENT_OUTCOME_COMMIT,
      W_SEED_COOPERATIVE0_EVENT_JOIN,
      W_SEED_COOPERATIVE0_EVENT_RELEASE};
  CHECK(sizeof(EVENTS) / sizeof(EVENTS[0]) == result.required.trace_events);
  for (size_t index = 0u; index < result.required.trace_events; index += 1u)
    CHECK(trace[index].sequence == index + 1u &&
          trace[index].kind == EVENTS[index] &&
          trace[index].frame_slot == trace[index].task_id &&
          trace[index].queue_before_count <= W_SEED_COOPERATIVE0_MAX_QUEUE &&
          trace[index].queue_after_count <= W_SEED_COOPERATIVE0_MAX_QUEUE);
  CHECK(w_seed_cooperative0_verify_plan(&storage.hir_program,
                                        &storage.hir_result, &result.plan) &&
        w_seed_cooperative0_verify_trace(
            &storage.hir_program, &storage.hir_result, &result.plan, trace,
            result.required.trace_events) &&
        w_seed_cooperative0_verify_execution(
            &storage.hir_program, &storage.hir_result, &result.plan, trace,
            result.required.trace_events, &result.final_state));
  CHECK(w_seed_cooperative0_verify_output(&storage.hir_program,
                                          &storage.hir_result, &output,
                                          &result));
  uint8_t digest[32];
  digest_bytes(artifact, result.required.artifact_bytes, digest);
  CHECK(memcmp(digest, result.artifact_digest, sizeof(digest)) == 0);
  digest_bytes(stdout_bytes, result.required.stdout_bytes, digest);
  CHECK(memcmp(digest, result.stdout_digest, sizeof(digest)) == 0);
  w_seed_cooperative0_result forged_output_result = result;
  forged_output_result.artifact_digest[0] ^= 1u;
  CHECK(!w_seed_cooperative0_verify_output(
      &storage.hir_program, &storage.hir_result, &output,
      &forged_output_result));
  forged_output_result = result;
  forged_output_result.stdout_digest[0] ^= 1u;
  CHECK(!w_seed_cooperative0_verify_output(
      &storage.hir_program, &storage.hir_result, &output,
      &forged_output_result));
  const uint8_t saved_artifact_byte = artifact[0];
  artifact[0] ^= 1u;
  CHECK(!w_seed_cooperative0_verify_output(&storage.hir_program,
                                           &storage.hir_result, &output,
                                           &result));
  artifact[0] = saved_artifact_byte;
  const uint8_t saved_stdout_byte = stdout_bytes[0];
  stdout_bytes[0] ^= 1u;
  CHECK(!w_seed_cooperative0_verify_output(&storage.hir_program,
                                           &storage.hir_result, &output,
                                           &result));
  stdout_bytes[0] = saved_stdout_byte;
  size_t physical_calls = 0u;
  for (size_t index = 0u; index < storage.hir_program.call_count; index += 1u)
    if (storage.hir_program.calls[index].execution_kind ==
        W_SEED_HIR0_CALL_STRUCTURED_ASYNC_COOPERATIVE_TRACE)
      physical_calls += 1u;
  CHECK(physical_calls == 2u);

  /* Every independently visible plan relation is fail-closed. */
  w_seed_cooperative0_plan forged_plan = result.plan;
  forged_plan.phase = 9u;
  CHECK(!w_seed_cooperative0_verify_plan(&storage.hir_program,
                                         &storage.hir_result, &forged_plan));
  forged_plan = result.plan;
  forged_plan.schema[0] = 'X';
  CHECK(!w_seed_cooperative0_verify_plan(&storage.hir_program,
                                         &storage.hir_result, &forged_plan));
  forged_plan = result.plan;
  forged_plan.hir_semantic_digest[0] ^= 1u;
  CHECK(!w_seed_cooperative0_verify_plan(&storage.hir_program,
                                         &storage.hir_result, &forged_plan));
  forged_plan = result.plan;
  forged_plan.execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL;
  CHECK(!w_seed_cooperative0_verify_plan(&storage.hir_program,
                                         &storage.hir_result, &forged_plan));
  forged_plan = result.plan;
  forged_plan.task_count = 3u;
  CHECK(!w_seed_cooperative0_verify_plan(&storage.hir_program,
                                         &storage.hir_result, &forged_plan));
  forged_plan = result.plan;
  forged_plan.frame_count = 1u;
  CHECK(!w_seed_cooperative0_verify_plan(&storage.hir_program,
                                         &storage.hir_result, &forged_plan));
  forged_plan = result.plan;
  forged_plan.yield_count = 0u;
  CHECK(!w_seed_cooperative0_verify_plan(&storage.hir_program,
                                         &storage.hir_result, &forged_plan));
  forged_plan = result.plan;
  forged_plan.tasks[0].task_id = 1u;
  CHECK(!w_seed_cooperative0_verify_plan(&storage.hir_program,
                                         &storage.hir_result, &forged_plan));
  forged_plan = result.plan;
  forged_plan.tasks[0].launch_binding = forged_plan.tasks[0].join_binding;
  CHECK(!w_seed_cooperative0_verify_plan(&storage.hir_program,
                                         &storage.hir_result, &forged_plan));
  forged_plan = result.plan;
  forged_plan.tasks[0].join_binding = forged_plan.tasks[1].join_binding;
  CHECK(!w_seed_cooperative0_verify_plan(&storage.hir_program,
                                         &storage.hir_result, &forged_plan));
  forged_plan = result.plan;
  forged_plan.tasks[0].yield_count = 0u;
  CHECK(!w_seed_cooperative0_verify_plan(&storage.hir_program,
                                         &storage.hir_result, &forged_plan));
  forged_plan = result.plan;
  forged_plan.tasks[0].yield_count = 3u;
  CHECK(!w_seed_cooperative0_verify_plan(&storage.hir_program,
                                         &storage.hir_result, &forged_plan));
  forged_plan = result.plan;
  forged_plan.frames[0].first_instruction = UINT32_MAX;
  forged_plan.frames[0].next_instruction = UINT32_MAX;
  CHECK(!w_seed_cooperative0_verify_plan(&storage.hir_program,
                                         &storage.hir_result, &forged_plan));
  forged_plan = result.plan;
  forged_plan.frames[0].first_instruction = UINT32_MAX - 1u;
  forged_plan.frames[0].next_instruction = UINT32_MAX - 1u;
  forged_plan.frames[0].instruction_count = 2u;
  CHECK(!w_seed_cooperative0_verify_plan(&storage.hir_program,
                                         &storage.hir_result, &forged_plan));
  forged_plan = result.plan;
  forged_plan.frames[0].next_instruction += 1u;
  CHECK(!w_seed_cooperative0_verify_plan(&storage.hir_program,
                                         &storage.hir_result, &forged_plan));
  forged_plan = result.plan;
  forged_plan.frames[0].bindings[0].integer ^= 1;
  CHECK(!w_seed_cooperative0_verify_plan(&storage.hir_program,
                                         &storage.hir_result, &forged_plan));

  /* A HIR relation forged behind the same physical profile is rejected by the
   * independent HIR verifier before Cooperative0 can admit it. */
  const uint32_t root_block_index =
      storage.hir_program.functions[storage.hir_program.entries[0]
                                       .target_function]
          .first_block;
  const uint32_t saved_root_instruction_count =
      storage.hir_blocks[root_block_index].instruction_count;
  storage.hir_blocks[root_block_index].instruction_count += 1u;
  CHECK(!w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));
  storage.hir_blocks[root_block_index].instruction_count =
      saved_root_instruction_count;
  CHECK(w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));

  /* Each event field and lifecycle record is independently checked. */
  for (size_t index = 0u; index < result.required.trace_events; index += 1u) {
    w_seed_cooperative0_trace_event forged_trace[W_SEED_COOPERATIVE0_MAX_TRACE_EVENTS];
    (void)memcpy(forged_trace, trace, sizeof(forged_trace));
    forged_trace[index].kind =
        (w_seed_cooperative0_event_kind)((forged_trace[index].kind + 1u) % 10u);
    CHECK(!w_seed_cooperative0_verify_trace(
        &storage.hir_program, &storage.hir_result, &result.plan, forged_trace,
        result.required.trace_events));
    (void)memcpy(forged_trace, trace, sizeof(forged_trace));
    forged_trace[index].frame_digest[0] ^= 1u;
    CHECK(!w_seed_cooperative0_verify_trace(
        &storage.hir_program, &storage.hir_result, &result.plan, forged_trace,
        result.required.trace_events));
  }
  w_seed_cooperative0_trace_event forged_trace[W_SEED_COOPERATIVE0_MAX_TRACE_EVENTS];
  (void)memcpy(forged_trace, trace, sizeof(forged_trace));
  forged_trace[2].queue_after[0] = 1u;
  CHECK(!w_seed_cooperative0_verify_trace(
      &storage.hir_program, &storage.hir_result, &result.plan, forged_trace,
      result.required.trace_events));
  (void)memcpy(forged_trace, trace, sizeof(forged_trace));
  forged_trace[6].pc += 1u;
  CHECK(!w_seed_cooperative0_verify_trace(
      &storage.hir_program, &storage.hir_result, &result.plan, forged_trace,
      result.required.trace_events));
  (void)memcpy(forged_trace, trace, sizeof(forged_trace));
  forged_trace[6].next_pc += 1u;
  CHECK(!w_seed_cooperative0_verify_trace(
      &storage.hir_program, &storage.hir_result, &result.plan, forged_trace,
      result.required.trace_events));
  (void)memcpy(forged_trace, trace, sizeof(forged_trace));
  forged_trace[6].yield_ordinal = 0u;
  CHECK(!w_seed_cooperative0_verify_trace(
      &storage.hir_program, &storage.hir_result, &result.plan, forged_trace,
      result.required.trace_events));
  for (size_t index = 0u; index < result.required.trace_events; index += 1u)
    if (trace[index].kind == W_SEED_COOPERATIVE0_EVENT_SETTLE) {
      (void)memcpy(forged_trace, trace, sizeof(forged_trace));
      forged_trace[index].outcome_digest[0] ^= 1u;
      CHECK(!w_seed_cooperative0_verify_trace(
          &storage.hir_program, &storage.hir_result, &result.plan, forged_trace,
          result.required.trace_events));
      break;
    }
  (void)memcpy(forged_trace, trace, sizeof(forged_trace));
  const w_seed_cooperative0_trace_event swapped = forged_trace[4];
  forged_trace[4] = forged_trace[5];
  forged_trace[5] = swapped;
  CHECK(!w_seed_cooperative0_verify_trace(
      &storage.hir_program, &storage.hir_result, &result.plan, forged_trace,
      result.required.trace_events));

  w_seed_cooperative0_execution_state forged_state = result.final_state;
  forged_state.frames[0].next_instruction += 1u;
  CHECK(!w_seed_cooperative0_verify_execution(
      &storage.hir_program, &storage.hir_result, &result.plan, trace,
      result.required.trace_events, &forged_state));
  forged_state = result.final_state;
  forged_state.frames[0].bindings[0].integer ^= 1;
  CHECK(!w_seed_cooperative0_verify_execution(
      &storage.hir_program, &storage.hir_result, &result.plan, trace,
      result.required.trace_events, &forged_state));
  forged_state = result.final_state;
  forged_state.frames[0].result.integer ^= 1;
  CHECK(!w_seed_cooperative0_verify_execution(
      &storage.hir_program, &storage.hir_result, &result.plan, trace,
      result.required.trace_events, &forged_state));

  return true;
}

static bool test_negative_shapes(void) {
  static const char ZERO_YIELDS[] =
      "async fn prepare(value: i64): i64 { return value }\n"
      "entry { let left = async prepare(value: 20) "
      "let right = async prepare(value: 22) let first = await left "
      "let second = await right print(\"${first + second}\") }\n";
  static const char THREE_YIELDS[] =
      "async fn prepare(value: i64): i64 { await execution#yield() "
      "await execution#yield() await execution#yield() return value }\n"
      "entry { let left = async prepare(value: 20) "
      "let right = async prepare(value: 22) let first = await left "
      "let second = await right print(\"${first + second}\") }\n";
  static const char CHILD_EFFECT[] =
      "async fn prepare(value: i64): i64 { "
      "print(message: \"effect\", suffix: \"\") "
      "await execution#yield() return value }\n"
      "entry { let left = async prepare(value: 20) "
      "let right = async prepare(value: 22) let first = await left "
      "let second = await right print(\"${first + second}\") }\n";
  static const char THROWING_CHILD[] =
      "fn prepare(value: i64): i64 throws Error { return value }\n"
      "entry { let left = async prepare(value: 20) "
      "let right = async prepare(value: 22) let first = await left "
      "let second = await right print(\"${first + second}\") }\n";
  static const char RECURSIVE_HELPER[] =
      "fn helper(value: i64): i64 { return helper(value: value) }\n"
      "async fn prepare(value: i64): i64 { await execution#yield() "
      "return helper(value: value) }\n"
      "entry { let left = async prepare(value: 20) "
      "let right = async prepare(value: 22) let first = await left "
      "let second = await right print(\"${first + second}\") }\n";
  CHECK(expect_negative_source(ZERO_YIELDS));
  CHECK(expect_negative_source(THREE_YIELDS));
  CHECK(expect_negative_source(CHILD_EFFECT));
  CHECK(expect_negative_source(THROWING_CHILD));
  CHECK(expect_negative_source(RECURSIVE_HELPER));
  return true;
}

static bool test_transactional_boundaries(void) {
  const w_seed_cooperative0_input input =
      input_for_path(W_SEED_COOPERATIVE0_FIXTURE_PATH);
  w_seed_cooperative0_result result;
  (void)memset(&result, 0x71, sizeof(result));
  const w_seed_cooperative0_result result_before = result;
  (void)memset(artifact, 0x72, sizeof(artifact));
  (void)memset(stdout_bytes, 0x73, sizeof(stdout_bytes));
  (void)memset(trace, 0x74, sizeof(trace));
  const w_seed_cooperative0_output too_small = {
      .artifact = NULL,
      .artifact_capacity = 0u,
      .stdout_bytes = NULL,
      .stdout_capacity = 0u,
      .trace = NULL,
      .trace_capacity = 0u};
  CHECK(w_seed_native0_run_cooperative_oracle(
            &input, &storage, &too_small, &result) ==
        W_SEED_COOPERATIVE0_CAPACITY);
  CHECK(memcmp(&result, &result_before, sizeof(result)) == 0 &&
        artifact[0] == 0x72u && stdout_bytes[0] == 0x73u &&
        trace[0].sequence == 0x74747474u);

  w_seed_cooperative0_output output = full_output();
  w_seed_cooperative0_output oversized = output;
  oversized.artifact_capacity = W_SEED_COOPERATIVE0_MAX_ARTIFACT_BYTES + 1u;
  (void)memset(&result, 0x73, sizeof(result));
  const w_seed_cooperative0_result oversized_result_before = result;
  (void)memset(artifact, 0x74, sizeof(artifact));
  CHECK(w_seed_native0_run_cooperative_oracle(
            &input, &storage, &oversized, &result) ==
        W_SEED_COOPERATIVE0_CAPACITY);
  CHECK(memcmp(&result, &oversized_result_before, sizeof(result)) == 0 &&
        artifact[0] == 0x74u);

  w_seed_cooperative0_output alias = full_output();
  alias.artifact = stdout_bytes;
  alias.artifact_capacity = sizeof(stdout_bytes);
  (void)memset(&result, 0x75, sizeof(result));
  const w_seed_cooperative0_result alias_result_before = result;
  CHECK(w_seed_native0_run_cooperative_oracle(&input, &storage, &alias,
                                              &result) ==
        W_SEED_COOPERATIVE0_ALIAS);
  CHECK(memcmp(&result, &alias_result_before, sizeof(result)) == 0);

  w_seed_cooperative0_result *result_alias =
      (w_seed_cooperative0_result *)(void *)artifact;
  (void)memset(artifact, 0x76, sizeof(artifact));
  CHECK(w_seed_native0_run_cooperative_oracle(&input, &storage, &output,
                                              result_alias) ==
        W_SEED_COOPERATIVE0_ALIAS);
  CHECK(artifact[0] == 0x76u);

  /* Restore a valid HIR/result after the failed calls, then prove measure is
   * exact and transactional on independently forged output destinations. */
  CHECK(w_seed_native0_run_cooperative_oracle(&input, &storage, &output,
                                              &result) ==
        W_SEED_COOPERATIVE0_OK);

  /* The output verifier and the run preflight reject output buffers and
   * result records that point into any written HIR backing range. */
  w_seed_cooperative0_output hir_output_alias = output;
  hir_output_alias.artifact = (uint8_t *)(void *)storage.hir_program.text_bytes;
  hir_output_alias.artifact_capacity = 1u;
  (void)memset(&result, 0x7a, sizeof(result));
  const w_seed_cooperative0_result hir_output_alias_before = result;
  CHECK(w_seed_cooperative0_run(&storage.hir_program, &storage.hir_result,
                                &hir_output_alias, &result) ==
        W_SEED_COOPERATIVE0_ALIAS);
  CHECK(memcmp(&result, &hir_output_alias_before, sizeof(result)) == 0);
  w_seed_cooperative0_result *hir_result_alias =
      (w_seed_cooperative0_result *)(void *)storage.hir_program.instructions;
  (void)memset(&result, 0x7b, sizeof(result));
  const w_seed_cooperative0_result hir_result_alias_before = result;
  CHECK(w_seed_cooperative0_run(&storage.hir_program, &storage.hir_result,
                                &output, hir_result_alias) ==
        W_SEED_COOPERATIVE0_ALIAS);
  CHECK(memcmp(&result, &hir_result_alias_before, sizeof(result)) == 0);

  /* The reused bridge binds path and source-id views before clearing storage
   * or publishing any output; both views must be disjoint from destinations. */
  w_seed_cooperative0_input path_alias_input = input;
  const size_t fixture_path_bytes = strlen(W_SEED_COOPERATIVE0_FIXTURE_PATH);
  (void)memcpy(artifact, W_SEED_COOPERATIVE0_FIXTURE_PATH,
               fixture_path_bytes + 1u);
  path_alias_input.path = (const char *)(const void *)artifact;
  path_alias_input.path_length = fixture_path_bytes;
  (void)memset(&result, 0x7c, sizeof(result));
  const w_seed_cooperative0_result path_alias_result_before = result;
  CHECK(w_seed_native0_run_cooperative_oracle(
            &path_alias_input, &storage, &output, &result) ==
        W_SEED_COOPERATIVE0_ALIAS);
  CHECK(memcmp(&result, &path_alias_result_before, sizeof(result)) == 0 &&
        memcmp(artifact, W_SEED_COOPERATIVE0_FIXTURE_PATH,
               fixture_path_bytes) == 0);
  w_seed_cooperative0_input source_id_alias_input = input;
  const size_t source_id_bytes = sizeof(SOURCE_ID) - 1u;
  (void)memcpy(artifact, SOURCE_ID, source_id_bytes);
  source_id_alias_input.logical_source_id.data =
      (const char *)(const void *)artifact;
  source_id_alias_input.logical_source_id.length = source_id_bytes;
  (void)memset(&result, 0x7d, sizeof(result));
  const w_seed_cooperative0_result source_id_alias_result_before = result;
  CHECK(w_seed_native0_run_cooperative_oracle(
            &source_id_alias_input, &storage, &output, &result) ==
        W_SEED_COOPERATIVE0_ALIAS);
  CHECK(memcmp(&result, &source_id_alias_result_before, sizeof(result)) == 0 &&
        memcmp(artifact, SOURCE_ID, source_id_bytes) == 0);

  w_seed_cooperative0_counts counts;
  w_seed_cooperative0_result measure_result;
  (void)memset(&counts, 0x77, sizeof(counts));
  (void)memset(&measure_result, 0x78, sizeof(measure_result));
  CHECK(w_seed_cooperative0_measure(&storage.hir_program,
                                    &storage.hir_result, &counts,
                                    &measure_result) ==
        W_SEED_COOPERATIVE0_OK);
  CHECK(counts.artifact_bytes != 0u && counts.stdout_bytes == 15u &&
        counts.trace_events == 30u &&
        measure_result.required.artifact_bytes == counts.artifact_bytes &&
        measure_result.required.stdout_bytes == counts.stdout_bytes &&
        measure_result.required.trace_events == counts.trace_events &&
        measure_result.written.artifact_bytes == 0u &&
        measure_result.written.stdout_bytes == 0u &&
        measure_result.written.trace_events == 0u);
  const w_seed_cooperative0_counts counts_before = counts;
  const w_seed_cooperative0_result measure_result_before = measure_result;
  w_seed_cooperative0_counts *counts_alias =
      (w_seed_cooperative0_counts *)(void *)storage.hir_program.instructions;
  CHECK(w_seed_cooperative0_measure(&storage.hir_program,
                                    &storage.hir_result, counts_alias,
                                    &measure_result) ==
        W_SEED_COOPERATIVE0_ALIAS);
  CHECK(memcmp(&measure_result, &measure_result_before,
               sizeof(measure_result)) == 0);
  w_seed_cooperative0_result *result_counts_alias =
      (w_seed_cooperative0_result *)(void *)&counts;
  CHECK(w_seed_cooperative0_measure(&storage.hir_program,
                                    &storage.hir_result, &counts,
                                    result_counts_alias) ==
        W_SEED_COOPERATIVE0_ALIAS);
  CHECK(memcmp(&counts, &counts_before, sizeof(counts)) == 0);
  w_seed_cooperative0_result *measure_hir_result_alias =
      (w_seed_cooperative0_result *)(void *)storage.hir_program.instructions;
  CHECK(w_seed_cooperative0_measure(&storage.hir_program,
                                    &storage.hir_result, &counts,
                                    measure_hir_result_alias) ==
        W_SEED_COOPERATIVE0_ALIAS);
  CHECK(memcmp(&counts, &counts_before, sizeof(counts)) == 0);
  const size_t saved_instruction_count = storage.hir_program.instruction_count;
  storage.hir_program.instruction_count = SIZE_MAX;
  CHECK(w_seed_cooperative0_measure(&storage.hir_program,
                                    &storage.hir_result, &counts,
                                    &measure_result) ==
        W_SEED_COOPERATIVE0_ALIAS);
  storage.hir_program.instruction_count = saved_instruction_count;
  return true;
}

int main(void) {
  const bool ok = test_cooperative_fixture() && test_negative_shapes() &&
                  test_transactional_boundaries();
  (void)remove(NEGATIVE_PATH);
  return ok ? 0 : 1;
}
