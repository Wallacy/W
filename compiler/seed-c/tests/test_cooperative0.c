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
#ifndef W_SEED_MAIN_DISPATCH0_FIXTURE_PATH
#define W_SEED_MAIN_DISPATCH0_FIXTURE_PATH "fixtures/restaurant-main-dispatch0.w"
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
static uint8_t cooperative_mlir[W_SEED_MLIR0_MAX_BYTES];
static uint8_t short_cooperative_mlir[W_SEED_MLIR0_MAX_BYTES];
static size_t cooperative_mlir_length;
static uint8_t cooperative_windows_mlir[W_SEED_MLIR0_MAX_BYTES];
static uint8_t cooperative_linux_mlir[W_SEED_MLIR0_MAX_BYTES];
static size_t cooperative_windows_mlir_length;
static size_t cooperative_linux_mlir_length;
static uint8_t main_dispatch_windows_mlir[W_SEED_MLIR0_MAX_BYTES];
static uint8_t main_dispatch_linux_mlir[W_SEED_MLIR0_MAX_BYTES];
static size_t main_dispatch_windows_mlir_length;
static size_t main_dispatch_linux_mlir_length;
static uint8_t main_dispatch_k1_windows_mlir[W_SEED_MLIR0_MAX_BYTES];
static uint8_t main_dispatch_k1_linux_mlir[W_SEED_MLIR0_MAX_BYTES];
static size_t main_dispatch_k1_windows_mlir_length;
static size_t main_dispatch_k1_linux_mlir_length;
static uint8_t main_dispatch_k3_windows_mlir[W_SEED_MLIR0_MAX_BYTES];
static uint8_t main_dispatch_k3_linux_mlir[W_SEED_MLIR0_MAX_BYTES];
static size_t main_dispatch_k3_windows_mlir_length;
static size_t main_dispatch_k3_linux_mlir_length;
static uint8_t main_dispatch_k4_windows_mlir[W_SEED_MLIR0_MAX_BYTES];
static uint8_t main_dispatch_k4_linux_mlir[W_SEED_MLIR0_MAX_BYTES];
static size_t main_dispatch_k4_windows_mlir_length;
static size_t main_dispatch_k4_linux_mlir_length;
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

static bool expect_native_negative_source(const char *source) {
  CHECK(write_source(source));
  const w_seed_native0_input input = native_input_for_path(NEGATIVE_PATH);
  w_seed_native0_result result;
  (void)memset(&result, 0x65, sizeof(result));
  const w_seed_native0_result result_before = result;
  (void)memset(artifact, 0x66, sizeof(artifact));
  CHECK(w_seed_native0_run(
            &input, &storage,
            &(w_seed_native0_output){artifact, sizeof(artifact)}, &result) !=
        W_SEED_NATIVE0_OK);
  CHECK(memcmp(&result, &result_before, sizeof(result)) == 0);
  for (size_t index = 0u; index < sizeof(artifact); index += 1u)
    CHECK(artifact[index] == 0x66u);
  (void)remove(NEGATIVE_PATH);
  return true;
}

static bool main_selection_unused_slots_zero(
    const w_seed_cooperative_selection0 *selection, size_t task_count) {
  if (selection == NULL || task_count > W_SEED_HIR0_COOPERATIVE_MAX_TASKS)
    return false;
  for (size_t task = task_count;
       task < W_SEED_HIR0_COOPERATIVE_MAX_TASKS; task += 1u)
    if (selection->task_call_indices[task] != 0u ||
        selection->task_function_indices[task] != 0u ||
        selection->launch_binding_indices[task] != 0u ||
        selection->join_binding_indices[task] != 0u ||
        selection->task_yield_counts[task] != 0u)
      return false;
  return true;
}

static bool expect_main_cardinality_source(const char *source,
                                           size_t task_count,
                                           int64_t expected_sum,
                                           size_t yields_per_task) {
  CHECK(source != NULL && task_count >= 1u &&
        task_count <= W_SEED_HIR0_COOPERATIVE_MAX_TASKS &&
        yields_per_task >= 1u &&
        yields_per_task <= W_SEED_HIR0_COOPERATIVE_MAX_YIELDS_PER_TASK);
  CHECK(write_source(source));
  static const w_seed_mlir0_target TARGETS[] = {
      {W_SEED_MLIR0_TARGET_X86_64_UNKNOWN_LINUX_GNU},
      {W_SEED_MLIR0_TARGET_X86_64_PC_WINDOWS_MSVC}};
  uint8_t *artifacts[2] = {main_dispatch_linux_mlir,
                           main_dispatch_windows_mlir};
  size_t *lengths[2] = {&main_dispatch_linux_mlir_length,
                        &main_dispatch_windows_mlir_length};
  if (task_count == 1u) {
    artifacts[0] = main_dispatch_k1_linux_mlir;
    artifacts[1] = main_dispatch_k1_windows_mlir;
    lengths[0] = &main_dispatch_k1_linux_mlir_length;
    lengths[1] = &main_dispatch_k1_windows_mlir_length;
  } else if (task_count == 3u) {
    artifacts[0] = main_dispatch_k3_linux_mlir;
    artifacts[1] = main_dispatch_k3_windows_mlir;
    lengths[0] = &main_dispatch_k3_linux_mlir_length;
    lengths[1] = &main_dispatch_k3_windows_mlir_length;
  } else if (task_count == W_SEED_HIR0_COOPERATIVE_MAX_TASKS) {
    artifacts[0] = main_dispatch_k4_linux_mlir;
    artifacts[1] = main_dispatch_k4_windows_mlir;
    lengths[0] = &main_dispatch_k4_linux_mlir_length;
    lengths[1] = &main_dispatch_k4_windows_mlir_length;
  }
  const uint32_t expected_yields = (uint32_t)(task_count * yields_per_task);
  for (size_t target_index = 0u;
       target_index < sizeof(TARGETS) / sizeof(TARGETS[0]); target_index += 1u) {
    w_seed_native0_input input = native_input_for_path(NEGATIVE_PATH);
    input.target = TARGETS[target_index];
    (void)memset(artifacts[target_index], 0xa8, W_SEED_MLIR0_MAX_BYTES);
    w_seed_native0_result native_result;
    CHECK(w_seed_native0_run(
              &input, &storage,
              &(w_seed_native0_output){artifacts[target_index],
                                       W_SEED_MLIR0_MAX_BYTES},
              &native_result) == W_SEED_NATIVE0_OK);
    *lengths[target_index] = native_result.mlir.written.mlir_bytes;
    CHECK(native_result.status == W_SEED_NATIVE0_OK &&
          *lengths[target_index] > 0u &&
          contains_bytes(artifacts[target_index], *lengths[target_index],
                         W_SEED_MLIR0_COOPERATIVE_EXECUTABLE_SCHEMA_VERSION) &&
          contains_bytes(artifacts[target_index], *lengths[target_index],
                         "func.call @w_seed_cooperative_core()") &&
          contains_bytes(artifacts[target_index], *lengths[target_index],
                         target_index == 0u
                             ? W_SEED_MLIR0_TARGET_TRIPLE_LINUX
                             : W_SEED_MLIR0_TARGET_TRIPLE_WINDOWS) &&
          contains_bytes(artifacts[target_index], *lengths[target_index],
                         "%final_done"));
    w_seed_cooperative_selection0 selection;
    CHECK(w_seed_mlir0_select_cooperative(&storage.hir_program,
                                          &storage.hir_result,
                                          &selection) == W_SEED_MLIR0_OK &&
          selection.execution_profile ==
              W_SEED_HIR0_EXECUTION_PROFILE_MAIN_SERIAL &&
          selection.task_count == task_count &&
          selection.yield_count == expected_yields &&
          main_selection_unused_slots_zero(&selection, task_count) &&
          w_seed_mlir0_verify_cooperative_selection(
              &storage.hir_program, &storage.hir_result, &selection));
    size_t physical_calls = 0u;
    for (size_t call = 0u; call < storage.hir_program.call_count; call += 1u)
      if (storage.hir_program.calls[call].execution_kind ==
          W_SEED_HIR0_CALL_STRUCTURED_ASYNC_MAIN_DISPATCH)
        physical_calls += 1u;
    CHECK(physical_calls == task_count &&
          w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));

    w_seed_mlir0_cooperative_counts counts = {0};
    w_seed_mlir0_cooperative_result measured;
    CHECK(w_seed_mlir0_measure_cooperative(
              &storage.hir_program, &storage.hir_result, &selection, &counts,
              &measured) == W_SEED_MLIR0_OK &&
          counts.frame_count == task_count &&
          counts.queue_capacity == task_count &&
          counts.yield_count == expected_yields &&
          counts.result_value == expected_sum &&
          measured.required.result_value == expected_sum);
    w_seed_mlir0_cooperative_result emitted;
    CHECK(w_seed_mlir0_emit_cooperative(
              &storage.hir_program, &storage.hir_result, &selection,
              &(w_seed_mlir0_cooperative_output){cooperative_mlir,
                                                 sizeof(cooperative_mlir)},
              &emitted) == W_SEED_MLIR0_OK &&
          emitted.required.result_value == expected_sum &&
          emitted.written.mlir_bytes == counts.mlir_bytes &&
          w_seed_mlir0_verify_cooperative_emission(
              &storage.hir_program, &storage.hir_result, &selection,
              cooperative_mlir, emitted.written.mlir_bytes, &emitted));
    CHECK(contains_bytes(cooperative_mlir, emitted.written.mlir_bytes,
                         "%final_done"));
    if (task_count == 1u)
      CHECK(contains_bytes(cooperative_mlir, emitted.written.mlir_bytes,
                           "%final_result0") &&
            !contains_bytes(cooperative_mlir, emitted.written.mlir_bytes,
                            "%final_result1"));
    if (task_count == W_SEED_HIR0_COOPERATIVE_MAX_TASKS)
      CHECK(contains_bytes(cooperative_mlir, emitted.written.mlir_bytes,
                           "%final_done3") &&
            contains_bytes(cooperative_mlir, emitted.written.mlir_bytes,
                           "%result_fold2"));

    for (size_t slot = 0u; slot < W_SEED_HIR0_COOPERATIVE_MAX_TASKS;
         slot += 1u) {
      w_seed_cooperative_selection0 forged = selection;
      forged.task_call_indices[slot] ^= 1u;
      CHECK(!w_seed_mlir0_verify_cooperative_selection(
          &storage.hir_program, &storage.hir_result, &forged));
      forged = selection;
      forged.task_function_indices[slot] ^= 1u;
      CHECK(!w_seed_mlir0_verify_cooperative_selection(
          &storage.hir_program, &storage.hir_result, &forged));
      forged = selection;
      forged.launch_binding_indices[slot] ^= 1u;
      CHECK(!w_seed_mlir0_verify_cooperative_selection(
          &storage.hir_program, &storage.hir_result, &forged));
      forged = selection;
      forged.join_binding_indices[slot] ^= 1u;
      CHECK(!w_seed_mlir0_verify_cooperative_selection(
          &storage.hir_program, &storage.hir_result, &forged));
      forged = selection;
      forged.task_yield_counts[slot] ^= 1u;
      CHECK(!w_seed_mlir0_verify_cooperative_selection(
          &storage.hir_program, &storage.hir_result, &forged));
    }
  }
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

  /* Product-policy callers may explicitly request the independently verified
   * cooperative profile without making it the default for an elidable source.
   * Native0 owns source-to-HIR selection; MLIR0 still owns target projection. */
  w_seed_native0_input cooperative_native_input = native_input;
  cooperative_native_input.artifact_kind =
      W_SEED_MLIR0_ARTIFACT_COOPERATIVE_EXECUTABLE;
  w_seed_native0_result cooperative_native_result;
  CHECK(w_seed_native0_run(
            &cooperative_native_input, &storage,
            &(w_seed_native0_output){cooperative_windows_mlir,
                                     sizeof(cooperative_windows_mlir)},
            &cooperative_native_result) == W_SEED_NATIVE0_OK);
  CHECK(cooperative_native_result.mlir.written.mlir_bytes >
            normal_result.mlir.written.mlir_bytes &&
        contains_bytes(cooperative_windows_mlir,
                       cooperative_native_result.mlir.written.mlir_bytes,
                       W_SEED_MLIR0_COOPERATIVE_EXECUTABLE_SCHEMA_VERSION) &&
        contains_bytes(cooperative_windows_mlir,
                       cooperative_native_result.mlir.written.mlir_bytes,
                       "func.call @w_seed_cooperative_core()") &&
        contains_bytes(cooperative_windows_mlir,
                       cooperative_native_result.mlir.written.mlir_bytes,
                       W_SEED_MLIR0_TARGET_TRIPLE_WINDOWS));

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

static bool test_source_selected_main_dispatch(void) {
  const w_seed_native0_input input =
      native_input_for_path(W_SEED_MAIN_DISPATCH0_FIXTURE_PATH);
  static const w_seed_mlir0_target TARGETS[] = {
      {W_SEED_MLIR0_TARGET_X86_64_UNKNOWN_LINUX_GNU},
      {W_SEED_MLIR0_TARGET_X86_64_PC_WINDOWS_MSVC}};
  uint8_t *const artifacts[] = {main_dispatch_linux_mlir,
                                main_dispatch_windows_mlir};
  size_t *const lengths[] = {&main_dispatch_linux_mlir_length,
                             &main_dispatch_windows_mlir_length};
  static const char *const TRIPLES[] = {W_SEED_MLIR0_TARGET_TRIPLE_LINUX,
                                        W_SEED_MLIR0_TARGET_TRIPLE_WINDOWS};
  for (size_t target_index = 0u;
       target_index < sizeof(TARGETS) / sizeof(TARGETS[0]);
       target_index += 1u) {
    w_seed_native0_input target_input = input;
    target_input.target = TARGETS[target_index];
    w_seed_native0_result result;
    (void)memset(artifacts[target_index], 0xa7, W_SEED_MLIR0_MAX_BYTES);
    CHECK(w_seed_native0_run(
              &target_input, &storage,
              &(w_seed_native0_output){artifacts[target_index],
                                       W_SEED_MLIR0_MAX_BYTES},
              &result) == W_SEED_NATIVE0_OK);
    *lengths[target_index] = result.mlir.written.mlir_bytes;
    CHECK(result.status == W_SEED_NATIVE0_OK &&
          *lengths[target_index] > 0u &&
          contains_bytes(artifacts[target_index], *lengths[target_index],
                         W_SEED_MLIR0_COOPERATIVE_EXECUTABLE_SCHEMA_VERSION) &&
          contains_bytes(artifacts[target_index], *lengths[target_index],
                         "func.call @w_seed_cooperative_core()") &&
          contains_bytes(artifacts[target_index], *lengths[target_index],
                         TRIPLES[target_index]));
  }
  size_t main_dispatches = 0u;
  size_t cooperative_traces = 0u;
  for (size_t call = 0u; call < storage.hir_program.call_count; call += 1u) {
    if (storage.hir_program.calls[call].execution_kind ==
        W_SEED_HIR0_CALL_STRUCTURED_ASYNC_MAIN_DISPATCH)
      main_dispatches += 1u;
    if (storage.hir_program.calls[call].execution_kind ==
        W_SEED_HIR0_CALL_STRUCTURED_ASYNC_COOPERATIVE_TRACE)
      cooperative_traces += 1u;
  }
  CHECK(main_dispatches == 2u && cooperative_traces == 0u &&
        w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));

  w_seed_cooperative_selection0 selection;
  CHECK(w_seed_mlir0_select_cooperative(&storage.hir_program,
                                        &storage.hir_result,
                                        &selection) == W_SEED_MLIR0_OK &&
        selection.execution_profile ==
            W_SEED_HIR0_EXECUTION_PROFILE_MAIN_SERIAL &&
        w_seed_mlir0_verify_cooperative_selection(
            &storage.hir_program, &storage.hir_result, &selection));
  return true;
}

static bool test_main_cardinalities(void) {
  static const char MAIN_K1[] =
      "async fn prepare(value: i64): i64 { await execution#yield() "
      "await execution#yield() return value }\n"
      "entry { let a = spawn<.main> prepare(value: 20) "
      "let av = await a print(\"Dispatched ${av}\") }\n";
  static const char MAIN_K3[] =
      "async fn prepare(value: i64): i64 { await execution#yield() "
      "await execution#yield() return value }\n"
      "entry { let a = spawn<.main> prepare(value: 20) "
      "let b = spawn<.main> prepare(value: 22) "
      "let c = spawn<.main> prepare(value: 24) let av = await a "
      "let bv = await b let cv = await c "
      "print(\"Dispatched ${av + bv + cv}\") }\n";
  static const char MAIN_K4[] =
      "async fn prepare(value: i64): i64 { await execution#yield() "
      "await execution#yield() return value }\n"
      "entry { let a = spawn<.main> prepare(value: 20) "
      "let b = spawn<.main> prepare(value: 22) "
      "let c = spawn<.main> prepare(value: 24) "
      "let d = spawn<.main> prepare(value: 26) let av = await a "
      "let bv = await b let cv = await c let dv = await d "
      "print(\"Dispatched ${av + bv + cv + dv}\") }\n";
  CHECK(expect_main_cardinality_source(MAIN_K1, 1u, 20, 2u));
  CHECK(expect_main_cardinality_source(MAIN_K3, 3u, 66, 2u));
  CHECK(expect_main_cardinality_source(MAIN_K4, 4u, 92, 2u));
  return true;
}

static bool expect_main_selection_unsupported_source(const char *source) {
  CHECK(write_source(source));
  const w_seed_native0_input input = native_input_for_path(NEGATIVE_PATH);
  w_seed_native0_result native_result;
  (void)memset(normal_artifact, 0x69, sizeof(normal_artifact));
  CHECK(w_seed_native0_run(
            &input, &storage,
            &(w_seed_native0_output){normal_artifact,
                                     sizeof(normal_artifact)},
            &native_result) == W_SEED_NATIVE0_OK);
  w_seed_cooperative_selection0 selection;
  (void)memset(&selection, 0x6a, sizeof(selection));
  CHECK(w_seed_mlir0_select_cooperative(&storage.hir_program,
                                        &storage.hir_result, &selection) ==
        W_SEED_MLIR0_UNSUPPORTED);
  (void)remove(NEGATIVE_PATH);
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
  static const char MAIN_FIVE_LAUNCHES[] =
      "async fn prepare(value: i64): i64 { await execution#yield() "
      "return value }\n"
      "entry { let a = spawn<.main> prepare(value: 20) "
      "let b = spawn<.main> prepare(value: 22) "
      "let c = spawn<.main> prepare(value: 24) "
      "let d = spawn<.main> prepare(value: 26) "
      "let e = spawn<.main> prepare(value: 28) let av = await a "
      "let bv = await b let cv = await c let dv = await d let ev = await e "
      "print(\"${av + bv + cv + dv + ev}\") }\n";
  static const char MAIN_JOIN_REORDER[] =
      "async fn prepare(value: i64): i64 { await execution#yield() "
      "return value }\n"
      "entry { let a = spawn<.main> prepare(value: 20) "
      "let b = spawn<.main> prepare(value: 22) let bv = await b "
      "let av = await a print(\"${bv + av}\") }\n";
  static const char MAIN_ORPHAN_LAUNCH[] =
      "async fn prepare(value: i64): i64 { await execution#yield() "
      "return value }\n"
      "entry { let a = spawn<.main> prepare(value: 20) "
      "let b = spawn<.main> prepare(value: 22) let av = await a "
      "let duplicate = await a print(\"${av + duplicate}\") }\n";
  static const char MAIN_NO_SPAWN[] =
      "async fn prepare(value: i64): i64 { await execution#yield() "
      "return value }\n"
      "entry { let a = async prepare(value: 20) let av = await a "
      "print(\"Dispatched ${av}\") }\n";
  static const char MAIN_MIXED_LAUNCH[] =
      "async fn prepare(value: i64): i64 { await execution#yield() "
      "return value }\n"
      "entry { let left = spawn<.main> prepare(value: 20) "
      "let right = async prepare(value: 22) let first = await left "
      "let second = await right print(\"${first + second}\") }\n";
  static const char MAIN_ZERO_YIELDS[] =
      "async fn prepare(value: i64): i64 { return value }\n"
      "entry { let left = spawn<.main> prepare(value: 20) "
      "let right = spawn<.main> prepare(value: 22) let first = await left "
      "let second = await right print(\"${first + second}\") }\n";
  static const char MAIN_SYNC_CHILD[] =
      "fn prepare(value: i64): i64 { return value }\n"
      "entry { let left = spawn<.main> prepare(value: 20) "
      "let right = spawn<.main> prepare(value: 22) let first = await left "
      "let second = await right print(\"${first + second}\") }\n";
  CHECK(expect_negative_source(ZERO_YIELDS));
  CHECK(expect_negative_source(THREE_YIELDS));
  CHECK(expect_negative_source(CHILD_EFFECT));
  CHECK(expect_negative_source(THROWING_CHILD));
  CHECK(expect_negative_source(RECURSIVE_HELPER));
  CHECK(expect_native_negative_source(MAIN_FIVE_LAUNCHES));
  CHECK(expect_native_negative_source(MAIN_JOIN_REORDER));
  CHECK(expect_native_negative_source(MAIN_ORPHAN_LAUNCH));
  CHECK(expect_native_negative_source(MAIN_MIXED_LAUNCH));
  CHECK(expect_native_negative_source(MAIN_ZERO_YIELDS));
  CHECK(expect_native_negative_source(MAIN_SYNC_CHILD));
  CHECK(expect_main_selection_unsupported_source(MAIN_NO_SPAWN));
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

static bool test_cooperative_product_selection(void) {
  const w_seed_cooperative_selection0 selection_sentinel = {
      .schema = {0x51u},
      .root_function_index = 91u,
      .task_count = 92u,
      .function_count = 93u,
      .instruction_count = 94u,
      .binding_count = 95u,
      .call_count = 96u,
      .yield_count = 97u,
      .task_call_indices = {98u, 99u},
      .task_function_indices = {100u, 101u},
      .launch_binding_indices = {102u, 103u},
      .join_binding_indices = {104u, 105u},
      .task_yield_counts = {1u, 2u},
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL,
      .reserved = {0x52u, 0x53u, 0x54u},
      .hir_semantic_digest = {0xa5u}};
  const w_seed_cooperative0_input input =
      input_for_path(W_SEED_COOPERATIVE0_FIXTURE_PATH);
  w_seed_cooperative0_result oracle_result;
  const w_seed_cooperative0_output output = full_output();
  CHECK(w_seed_native0_run_cooperative_oracle(&input, &storage, &output,
                                              &oracle_result) ==
        W_SEED_COOPERATIVE0_OK);
  w_seed_cooperative_selection0 selection;
  CHECK(w_seed_mlir0_select_cooperative(&storage.hir_program,
                                        &storage.hir_result, &selection) ==
        W_SEED_MLIR0_OK);
  static const uint8_t EXPECTED_DIGEST[32] = {
      0xf5u, 0x7du, 0x1eu, 0xb4u, 0x38u, 0xacu, 0x48u, 0x5du,
      0x5du, 0xbeu, 0xdfu, 0x52u, 0x1bu, 0x80u, 0x79u, 0x10u,
      0x16u, 0xb8u, 0x9fu, 0xddu, 0xfcu, 0xc7u, 0x24u, 0x25u,
      0x96u, 0x80u, 0x17u, 0xf6u, 0xc0u, 0xd1u, 0x3fu, 0x3bu};
  static const uint32_t EXPECTED_CALLS[2] = {1u, 2u};
  static const uint32_t EXPECTED_FUNCTIONS[2] = {1u, 1u};
  static const uint32_t EXPECTED_LAUNCHES[2] = {2u, 3u};
  static const uint32_t EXPECTED_JOINS[2] = {4u, 5u};
  static const uint32_t EXPECTED_YIELDS[2] = {2u, 2u};
  static const uint8_t ZERO_RESERVED[3] = {0u, 0u, 0u};
  CHECK(memcmp(selection.schema, W_SEED_COOPERATIVE_SELECTION0_SCHEMA_VERSION,
              sizeof(selection.schema)) == 0 &&
        selection.root_function_index == 2u &&
        selection.task_count == W_SEED_HIR0_COOPERATIVE_ORACLE_MAX_TASKS &&
        selection.function_count == 3u && selection.instruction_count == 12u &&
        selection.binding_count == 6u && selection.call_count == 4u &&
        selection.yield_count == 4u &&
        memcmp(selection.task_call_indices, EXPECTED_CALLS,
               sizeof(EXPECTED_CALLS)) == 0 &&
        memcmp(selection.task_function_indices, EXPECTED_FUNCTIONS,
               sizeof(EXPECTED_FUNCTIONS)) == 0 &&
        memcmp(selection.launch_binding_indices, EXPECTED_LAUNCHES,
               sizeof(EXPECTED_LAUNCHES)) == 0 &&
        memcmp(selection.join_binding_indices, EXPECTED_JOINS,
               sizeof(EXPECTED_JOINS)) == 0 &&
        memcmp(selection.task_yield_counts, EXPECTED_YIELDS,
               sizeof(EXPECTED_YIELDS)) == 0 &&
        main_selection_unused_slots_zero(
            &selection, W_SEED_HIR0_COOPERATIVE_ORACLE_MAX_TASKS) &&
        selection.execution_profile ==
            W_SEED_HIR0_EXECUTION_PROFILE_COOPERATIVE_TRACE &&
        memcmp(selection.reserved, ZERO_RESERVED, sizeof(ZERO_RESERVED)) == 0 &&
        memcmp(selection.hir_semantic_digest, EXPECTED_DIGEST,
               sizeof(EXPECTED_DIGEST)) == 0);
  CHECK(w_seed_mlir0_verify_cooperative_selection(
      &storage.hir_program, &storage.hir_result, &selection));

  const w_seed_mlir0_cooperative_counts counts_sentinel = {
      91u, 92u, 93u, 94u, 95};
  w_seed_mlir0_cooperative_counts counts = counts_sentinel;
  w_seed_mlir0_cooperative_result measured;
  (void)memset(&measured, 0x5au, sizeof(measured));
  CHECK(w_seed_mlir0_measure_cooperative(
            &storage.hir_program, &storage.hir_result, &selection, &counts,
            &measured) == W_SEED_MLIR0_OK);
  CHECK(counts.mlir_bytes > 0u &&
        counts.mlir_bytes < sizeof(cooperative_mlir) &&
        counts.frame_count == 2u && counts.queue_capacity == 2u &&
        counts.yield_count == 4u && counts.result_value == 88 &&
        measured.status == W_SEED_MLIR0_OK &&
        measured.required.mlir_bytes == counts.mlir_bytes &&
        measured.required.frame_count == counts.frame_count &&
        measured.required.queue_capacity == counts.queue_capacity &&
        measured.required.yield_count == counts.yield_count &&
        measured.required.result_value == counts.result_value);
  static const w_seed_mlir0_cooperative_counts ZERO_COUNTS = {0};
  CHECK(memcmp(&measured.written, &ZERO_COUNTS, sizeof(ZERO_COUNTS)) == 0);

  (void)memset(cooperative_mlir, 0xa5, sizeof(cooperative_mlir));
  w_seed_mlir0_cooperative_result emitted;
  (void)memset(&emitted, 0x6bu, sizeof(emitted));
  CHECK(w_seed_mlir0_emit_cooperative(
            &storage.hir_program, &storage.hir_result, &selection,
            &(w_seed_mlir0_cooperative_output){cooperative_mlir,
                                               sizeof(cooperative_mlir)},
            &emitted) == W_SEED_MLIR0_OK);
  cooperative_mlir_length = counts.mlir_bytes;
  CHECK(emitted.required.mlir_bytes == counts.mlir_bytes &&
        emitted.required.frame_count == counts.frame_count &&
        emitted.required.queue_capacity == counts.queue_capacity &&
        emitted.required.yield_count == counts.yield_count &&
        emitted.required.result_value == counts.result_value &&
        emitted.written.mlir_bytes == counts.mlir_bytes &&
        emitted.written.frame_count == counts.frame_count &&
        emitted.written.queue_capacity == counts.queue_capacity &&
        emitted.written.yield_count == counts.yield_count &&
        emitted.written.result_value == counts.result_value &&
        memcmp(emitted.mlir_sha256, measured.mlir_sha256,
               sizeof(emitted.mlir_sha256)) == 0 &&
        w_seed_mlir0_verify_cooperative_emission(
            &storage.hir_program, &storage.hir_result, &selection,
            cooperative_mlir, counts.mlir_bytes, &emitted));
  CHECK(contains_bytes(cooperative_mlir, counts.mlir_bytes,
                       W_SEED_MLIR0_COOPERATIVE_SCHEMA_VERSION) &&
        contains_bytes(cooperative_mlir, counts.mlir_bytes,
                       "func.func @w_seed_cooperative_core() -> i64") &&
        contains_bytes(cooperative_mlir, counts.mlir_bytes, "scf.while") &&
        contains_bytes(cooperative_mlir, counts.mlir_bytes,
                       "%pc_end0 = arith.constant 3 : i32") &&
        contains_bytes(cooperative_mlir, counts.mlir_bytes,
                       "%pc_end1 = arith.constant 3 : i32") &&
        contains_bytes(cooperative_mlir, counts.mlir_bytes,
                       "arith.addi %final_result0, %final_result1") &&
        !contains_bytes(cooperative_mlir, counts.mlir_bytes, "llvm.") &&
        !contains_bytes(cooperative_mlir, counts.mlir_bytes,
                        "llvm.target_triple") &&
        !contains_bytes(cooperative_mlir, counts.mlir_bytes, "!llvm.ptr") &&
        !contains_bytes(cooperative_mlir, counts.mlir_bytes, "WriteFile") &&
        !contains_bytes(cooperative_mlir, counts.mlir_bytes, "syscall") &&
        !contains_bytes(cooperative_mlir, counts.mlir_bytes, "mainCRTStartup"));

  static const w_seed_mlir0_target COOPERATIVE_TARGETS[] = {
      {W_SEED_MLIR0_TARGET_X86_64_UNKNOWN_LINUX_GNU},
      {W_SEED_MLIR0_TARGET_X86_64_PC_WINDOWS_MSVC}};
  uint8_t *const target_artifacts[] = {cooperative_linux_mlir,
                                      cooperative_windows_mlir};
  size_t *const target_lengths[] = {&cooperative_linux_mlir_length,
                                    &cooperative_windows_mlir_length};
  const w_seed_mlir0_input executable_input = {
      &storage.hir_program, &storage.hir_result,
      W_SEED_MLIR0_ARTIFACT_COOPERATIVE_EXECUTABLE};
  const w_seed_mlir0_target unsupported_target = {
      W_SEED_MLIR0_TARGET_UNSUPPORTED};
  w_seed_mlir0_counts unsupported_counts = {0};
  w_seed_mlir0_result unsupported_result;
  (void)memset(&unsupported_result, 0x39, sizeof(unsupported_result));
  const w_seed_mlir0_result unsupported_result_before = unsupported_result;
  (void)memset(short_cooperative_mlir, 0x3a,
               sizeof(short_cooperative_mlir));
  CHECK(w_seed_mlir0_measure(&executable_input, &unsupported_target,
                             &unsupported_counts, &unsupported_result) ==
        W_SEED_MLIR0_UNSUPPORTED);
  CHECK(w_seed_mlir0_emit(
            &executable_input, &unsupported_target,
            &(w_seed_mlir0_output){short_cooperative_mlir,
                                   sizeof(short_cooperative_mlir)},
            &unsupported_result) == W_SEED_MLIR0_UNSUPPORTED);
  CHECK(memcmp(&unsupported_result, &unsupported_result_before,
               sizeof(unsupported_result)) == 0);
  for (size_t byte = 0u; byte < sizeof(short_cooperative_mlir); byte += 1u)
    CHECK(short_cooperative_mlir[byte] == 0x3au);
  for (size_t target_index = 0u;
       target_index < sizeof(COOPERATIVE_TARGETS) /
                          sizeof(COOPERATIVE_TARGETS[0]);
       target_index += 1u) {
    w_seed_mlir0_counts target_counts = {0};
    w_seed_mlir0_result target_measured;
    w_seed_mlir0_result target_emitted;
    CHECK(w_seed_mlir0_measure(&executable_input,
                               &COOPERATIVE_TARGETS[target_index],
                               &target_counts, &target_measured) ==
          W_SEED_MLIR0_OK);
    CHECK(w_seed_mlir0_emit(
              &executable_input, &COOPERATIVE_TARGETS[target_index],
              &(w_seed_mlir0_output){target_artifacts[target_index],
                                     W_SEED_MLIR0_MAX_BYTES},
              &target_emitted) == W_SEED_MLIR0_OK);
    *target_lengths[target_index] = target_counts.mlir_bytes;
    CHECK(target_counts.mlir_bytes > counts.mlir_bytes &&
          target_emitted.required.mlir_bytes == target_counts.mlir_bytes &&
          target_emitted.written.mlir_bytes == target_counts.mlir_bytes &&
          memcmp(target_measured.mlir_sha256, target_emitted.mlir_sha256,
                 sizeof(target_emitted.mlir_sha256)) == 0 &&
          contains_bytes(target_artifacts[target_index],
                         target_counts.mlir_bytes,
                         W_SEED_MLIR0_COOPERATIVE_EXECUTABLE_SCHEMA_VERSION) &&
          contains_bytes(target_artifacts[target_index],
                         target_counts.mlir_bytes,
                         "func.call @w_seed_cooperative_core()") &&
          contains_bytes(target_artifacts[target_index],
                         target_counts.mlir_bytes,
                         "llvm.call @w_seed_append_i64") &&
          contains_bytes(target_artifacts[target_index],
                         target_counts.mlir_bytes,
                         "@w_seed_cooperative_text") &&
          contains_bytes(target_artifacts[target_index],
                         target_counts.mlir_bytes, "scf.while"));
    (void)memset(short_cooperative_mlir, 0x3b,
                 sizeof(short_cooperative_mlir));
    w_seed_mlir0_result short_result;
    (void)memset(&short_result, 0x3c, sizeof(short_result));
    const w_seed_mlir0_result short_result_before = short_result;
    CHECK(w_seed_mlir0_emit(
              &executable_input, &COOPERATIVE_TARGETS[target_index],
              &(w_seed_mlir0_output){short_cooperative_mlir,
                                     target_counts.mlir_bytes - 1u},
              &short_result) == W_SEED_MLIR0_CAPACITY);
    CHECK(memcmp(&short_result, &short_result_before, sizeof(short_result)) ==
          0);
    for (size_t byte = 0u; byte < sizeof(short_cooperative_mlir); byte += 1u)
      CHECK(short_cooperative_mlir[byte] == 0x3bu);
    if (target_index == 0u) {
      CHECK(contains_bytes(target_artifacts[target_index],
                           target_counts.mlir_bytes,
                           W_SEED_MLIR0_TARGET_TRIPLE_LINUX) &&
            contains_bytes(target_artifacts[target_index],
                           target_counts.mlir_bytes, "llvm.func @write") &&
            !contains_bytes(target_artifacts[target_index],
                            target_counts.mlir_bytes, "WriteFile") &&
            !contains_bytes(target_artifacts[target_index],
                            target_counts.mlir_bytes, "mainCRTStartup"));
    } else {
      CHECK(contains_bytes(target_artifacts[target_index],
                           target_counts.mlir_bytes,
                           W_SEED_MLIR0_TARGET_TRIPLE_WINDOWS) &&
            contains_bytes(target_artifacts[target_index],
                           target_counts.mlir_bytes, "WriteFile") &&
            contains_bytes(target_artifacts[target_index],
                           target_counts.mlir_bytes, "mainCRTStartup") &&
            !contains_bytes(target_artifacts[target_index],
                            target_counts.mlir_bytes,
                            W_SEED_MLIR0_TARGET_TRIPLE_LINUX));
    }
  }

  (void)memset(short_cooperative_mlir, 0x31,
               sizeof(short_cooperative_mlir));
  uint8_t short_before[32];
  uint8_t short_after[32];
  digest_bytes(short_cooperative_mlir, sizeof(short_cooperative_mlir),
               short_before);
  w_seed_mlir0_cooperative_result unchanged_emission;
  (void)memset(&unchanged_emission, 0x42, sizeof(unchanged_emission));
  const w_seed_mlir0_cooperative_result unchanged_before = unchanged_emission;
  CHECK(w_seed_mlir0_emit_cooperative(
            &storage.hir_program, &storage.hir_result, &selection,
            &(w_seed_mlir0_cooperative_output){short_cooperative_mlir,
                                               counts.mlir_bytes - 1u},
            &unchanged_emission) == W_SEED_MLIR0_CAPACITY);
  digest_bytes(short_cooperative_mlir, sizeof(short_cooperative_mlir),
               short_after);
  CHECK(memcmp(short_after, short_before, sizeof(short_before)) == 0 &&
        memcmp(&unchanged_emission, &unchanged_before,
               sizeof(unchanged_emission)) == 0);

  w_seed_mlir0_cooperative_result alias_result;
  (void)memset(&alias_result, 0x44, sizeof(alias_result));
  const w_seed_mlir0_cooperative_result alias_before = alias_result;
  CHECK(w_seed_mlir0_measure_cooperative(
            &storage.hir_program, &storage.hir_result, &selection,
            (w_seed_mlir0_cooperative_counts *)(void *)&alias_result,
            &alias_result) == W_SEED_MLIR0_ALIAS);
  CHECK(memcmp(&alias_result, &alias_before, sizeof(alias_result)) == 0);

  const w_seed_cooperative_selection0 selection_before_alias = selection;
  w_seed_mlir0_cooperative_result selection_alias_result;
  (void)memset(&selection_alias_result, 0x45,
               sizeof(selection_alias_result));
  const w_seed_mlir0_cooperative_result selection_alias_result_before =
      selection_alias_result;
  CHECK(w_seed_mlir0_emit_cooperative(
            &storage.hir_program, &storage.hir_result, &selection,
            &(w_seed_mlir0_cooperative_output){(uint8_t *)(void *)&selection,
                                               sizeof(selection)},
            &selection_alias_result) == W_SEED_MLIR0_ALIAS);
  CHECK(memcmp(&selection, &selection_before_alias, sizeof(selection)) == 0 &&
        memcmp(&selection_alias_result, &selection_alias_result_before,
               sizeof(selection_alias_result)) == 0);

  cooperative_mlir[0] ^= 1u;
  CHECK(!w_seed_mlir0_verify_cooperative_emission(
      &storage.hir_program, &storage.hir_result, &selection, cooperative_mlir,
      counts.mlir_bytes, &emitted));
  cooperative_mlir[0] ^= 1u;
  w_seed_mlir0_cooperative_result forged_emission = emitted;
  forged_emission.required.yield_count ^= 1u;
  CHECK(!w_seed_mlir0_verify_cooperative_emission(
      &storage.hir_program, &storage.hir_result, &selection, cooperative_mlir,
      counts.mlir_bytes, &forged_emission));
  forged_emission = emitted;
  forged_emission.written.result_value ^= 1;
  CHECK(!w_seed_mlir0_verify_cooperative_emission(
      &storage.hir_program, &storage.hir_result, &selection, cooperative_mlir,
      counts.mlir_bytes, &forged_emission));
  forged_emission = emitted;
  forged_emission.mlir_sha256[0] ^= 1u;
  CHECK(!w_seed_mlir0_verify_cooperative_emission(
      &storage.hir_program, &storage.hir_result, &selection, cooperative_mlir,
      counts.mlir_bytes, &forged_emission));

  w_seed_cooperative_selection0 forged = selection;
#define REJECT_SELECTION_MUTATION(statement)                                  \
  do {                                                                         \
    forged = selection;                                                        \
    statement;                                                                 \
    CHECK(!w_seed_mlir0_verify_cooperative_selection(                          \
        &storage.hir_program, &storage.hir_result, &forged));                 \
  } while (0)
  REJECT_SELECTION_MUTATION(forged.root_function_index ^= 1u);
  REJECT_SELECTION_MUTATION(forged.task_count ^= 1u);
  REJECT_SELECTION_MUTATION(forged.function_count ^= 1u);
  REJECT_SELECTION_MUTATION(forged.instruction_count ^= 1u);
  REJECT_SELECTION_MUTATION(forged.binding_count ^= 1u);
  REJECT_SELECTION_MUTATION(forged.call_count ^= 1u);
  REJECT_SELECTION_MUTATION(forged.yield_count ^= 1u);
  REJECT_SELECTION_MUTATION(forged.execution_profile =
                            W_SEED_HIR0_EXECUTION_PROFILE_NORMAL);
  for (size_t index = 0u; index < sizeof(forged.task_call_indices) /
                                  sizeof(forged.task_call_indices[0]);
       index += 1u)
    REJECT_SELECTION_MUTATION(forged.task_call_indices[index] ^= 1u);
  for (size_t index = 0u; index < sizeof(forged.task_function_indices) /
                                  sizeof(forged.task_function_indices[0]);
       index += 1u)
    REJECT_SELECTION_MUTATION(forged.task_function_indices[index] ^= 1u);
  for (size_t index = 0u; index < sizeof(forged.launch_binding_indices) /
                                  sizeof(forged.launch_binding_indices[0]);
       index += 1u)
    REJECT_SELECTION_MUTATION(forged.launch_binding_indices[index] ^= 1u);
  for (size_t index = 0u; index < sizeof(forged.join_binding_indices) /
                                  sizeof(forged.join_binding_indices[0]);
       index += 1u)
    REJECT_SELECTION_MUTATION(forged.join_binding_indices[index] ^= 1u);
  for (size_t index = 0u; index < sizeof(forged.task_yield_counts) /
                                  sizeof(forged.task_yield_counts[0]);
       index += 1u)
    REJECT_SELECTION_MUTATION(forged.task_yield_counts[index] ^= 1u);
  for (size_t index = 0u; index < sizeof(forged.schema); index += 1u)
    REJECT_SELECTION_MUTATION(forged.schema[index] ^= 1u);
  for (size_t index = 0u; index < sizeof(forged.reserved); index += 1u)
    REJECT_SELECTION_MUTATION(forged.reserved[index] ^= 1u);
  for (size_t index = 0u; index < sizeof(forged.hir_semantic_digest);
       index += 1u)
    REJECT_SELECTION_MUTATION(forged.hir_semantic_digest[index] ^= 1u);
#undef REJECT_SELECTION_MUTATION

  w_seed_hir0_result forged_hir_result = storage.hir_result;
  forged_hir_result.semantic_digest[0] ^= 1u;
  w_seed_cooperative_selection0 unchanged = selection_sentinel;
  CHECK(w_seed_mlir0_select_cooperative(&storage.hir_program,
                                        &forged_hir_result, &unchanged) ==
        W_SEED_MLIR0_INVALID_HIR);
  CHECK(memcmp(&unchanged, &selection_sentinel, sizeof(unchanged)) == 0);
  unchanged = selection_sentinel;
  CHECK(w_seed_mlir0_select_cooperative(NULL, &storage.hir_result, &unchanged) ==
        W_SEED_MLIR0_INVALID_HIR);
  CHECK(memcmp(&unchanged, &selection_sentinel, sizeof(unchanged)) == 0);

  const w_seed_native0_input normal_input =
      native_input_for_path(W_SEED_COOPERATIVE0_FIXTURE_PATH);
  w_seed_native0_result normal_result;
  CHECK(w_seed_native0_run(
            &normal_input, &storage,
            &(w_seed_native0_output){normal_artifact, sizeof(normal_artifact)},
            &normal_result) == W_SEED_NATIVE0_OK);
  unchanged = selection_sentinel;
  CHECK(w_seed_mlir0_select_cooperative(&storage.hir_program,
                                        &storage.hir_result, &unchanged) ==
        W_SEED_MLIR0_UNSUPPORTED);
  CHECK(memcmp(&unchanged, &selection_sentinel, sizeof(unchanged)) == 0);
  const w_seed_mlir0_input unsupported_executable_input = {
      &storage.hir_program, &storage.hir_result,
      W_SEED_MLIR0_ARTIFACT_COOPERATIVE_EXECUTABLE};
  w_seed_mlir0_counts executable_counts = {0};
  w_seed_mlir0_result executable_result;
  (void)memset(&executable_result, 0x5d, sizeof(executable_result));
  const w_seed_mlir0_result executable_result_before = executable_result;
  (void)memset(short_cooperative_mlir, 0x5e,
               sizeof(short_cooperative_mlir));
  CHECK(w_seed_mlir0_measure(&unsupported_executable_input,
                             &(w_seed_mlir0_target){
                                 W_SEED_MLIR0_TARGET_X86_64_UNKNOWN_LINUX_GNU},
                             &executable_counts, &executable_result) ==
        W_SEED_MLIR0_UNSUPPORTED);
  CHECK(w_seed_mlir0_emit(
            &unsupported_executable_input,
            &(w_seed_mlir0_target){
                W_SEED_MLIR0_TARGET_X86_64_UNKNOWN_LINUX_GNU},
            &(w_seed_mlir0_output){short_cooperative_mlir,
                                   sizeof(short_cooperative_mlir)},
            &executable_result) == W_SEED_MLIR0_UNSUPPORTED);
  CHECK(memcmp(&executable_result, &executable_result_before,
               sizeof(executable_result)) == 0);
  for (size_t byte = 0u; byte < sizeof(short_cooperative_mlir); byte += 1u)
    CHECK(short_cooperative_mlir[byte] == 0x5eu);
  return true;
}

int main(int argc, char **argv) {
  const bool ok = test_cooperative_fixture() &&
                  test_source_selected_main_dispatch() &&
                  test_main_cardinalities() &&
                  test_negative_shapes() &&
                   test_transactional_boundaries() &&
                   test_cooperative_product_selection();
  (void)remove(NEGATIVE_PATH);
  if (ok && argc == 2 && argv != NULL &&
      strcmp(argv[1], "--emit-target-neutral-mlir") == 0) {
    const size_t written =
        fwrite(cooperative_mlir, sizeof(uint8_t), cooperative_mlir_length,
               stdout);
    return written == cooperative_mlir_length && fflush(stdout) == 0 ? 0 : 1;
  }
  if (ok && argc == 2 && argv != NULL &&
      strcmp(argv[1], "--emit-cooperative-linux-mlir") == 0) {
    const size_t written = fwrite(cooperative_linux_mlir, sizeof(uint8_t),
                                  cooperative_linux_mlir_length, stdout);
    return written == cooperative_linux_mlir_length && fflush(stdout) == 0 ? 0
                                                                           : 1;
  }
  if (ok && argc == 2 && argv != NULL &&
      strcmp(argv[1], "--emit-cooperative-windows-mlir") == 0) {
    const size_t written = fwrite(cooperative_windows_mlir, sizeof(uint8_t),
                                  cooperative_windows_mlir_length, stdout);
    return written == cooperative_windows_mlir_length && fflush(stdout) == 0
               ? 0
               : 1;
  }
  if (ok && argc == 2 && argv != NULL &&
      strcmp(argv[1], "--emit-main-dispatch-linux-mlir") == 0) {
    const size_t written = fwrite(main_dispatch_linux_mlir, sizeof(uint8_t),
                                  main_dispatch_linux_mlir_length, stdout);
    return written == main_dispatch_linux_mlir_length && fflush(stdout) == 0
               ? 0
               : 1;
  }
  if (ok && argc == 2 && argv != NULL &&
      strcmp(argv[1], "--emit-main-dispatch-windows-mlir") == 0) {
    const size_t written = fwrite(main_dispatch_windows_mlir, sizeof(uint8_t),
                                  main_dispatch_windows_mlir_length, stdout);
    return written == main_dispatch_windows_mlir_length && fflush(stdout) == 0
               ? 0
               : 1;
  }
  if (ok && argc == 2 && argv != NULL &&
      strcmp(argv[1], "--emit-main-dispatch-k1-linux-mlir") == 0) {
    const size_t written =
        fwrite(main_dispatch_k1_linux_mlir, sizeof(uint8_t),
               main_dispatch_k1_linux_mlir_length, stdout);
    return written == main_dispatch_k1_linux_mlir_length && fflush(stdout) == 0
               ? 0
               : 1;
  }
  if (ok && argc == 2 && argv != NULL &&
      strcmp(argv[1], "--emit-main-dispatch-k1-windows-mlir") == 0) {
    const size_t written =
        fwrite(main_dispatch_k1_windows_mlir, sizeof(uint8_t),
               main_dispatch_k1_windows_mlir_length, stdout);
    return written == main_dispatch_k1_windows_mlir_length &&
                   fflush(stdout) == 0
               ? 0
               : 1;
  }
  if (ok && argc == 2 && argv != NULL &&
      strcmp(argv[1], "--emit-main-dispatch-k4-linux-mlir") == 0) {
    const size_t written =
        fwrite(main_dispatch_k4_linux_mlir, sizeof(uint8_t),
               main_dispatch_k4_linux_mlir_length, stdout);
    return written == main_dispatch_k4_linux_mlir_length && fflush(stdout) == 0
               ? 0
               : 1;
  }
  if (ok && argc == 2 && argv != NULL &&
      strcmp(argv[1], "--emit-main-dispatch-k4-windows-mlir") == 0) {
    const size_t written =
        fwrite(main_dispatch_k4_windows_mlir, sizeof(uint8_t),
               main_dispatch_k4_windows_mlir_length, stdout);
    return written == main_dispatch_k4_windows_mlir_length &&
                   fflush(stdout) == 0
               ? 0
               : 1;
  }
  if (argc != 1) return 2;
  return ok ? 0 : 1;
}
