#include "w_seed_run0.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "w_seed_sha256.h"

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "run0 check failed: %s (%s:%d)\n", #condition, \
                    __FILE__, __LINE__);                                      \
      return false;                                                            \
    }                                                                          \
  } while (0)

typedef struct {
  size_t calls;
  size_t accepted_bytes;
  w_seed_run0_flush_status flush_status;
  uint8_t bytes[W_SEED_RUN0_MAX_OUTPUT_BYTES];
  size_t byte_count;
} sink_context;

static void set_text(char *destination, size_t capacity, const char *value) {
  const size_t length = strlen(value);
  (void)memcpy(destination, value, length + 1u);
  (void)memset(destination + length + 1u, 0, capacity - length - 1u);
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

static w_seed_run0_sink_result collect_sink(void *context,
                                            const uint8_t *bytes,
                                            size_t byte_count) {
  sink_context *sink = (sink_context *)context;
  if (sink == NULL || bytes == NULL || byte_count > sizeof(sink->bytes))
    return (w_seed_run0_sink_result){0u,
                                     W_SEED_RUN0_FLUSH_NOT_ATTEMPTED};
  sink->calls += 1u;
  sink->byte_count = byte_count;
  (void)memcpy(sink->bytes, bytes, byte_count);
  return (w_seed_run0_sink_result){sink->accepted_bytes,
                                   sink->flush_status};
}

static bool test_canonical_execution(void) {
  const w_seed_hlo0_plan plan = canonical_plan();
  CHECK(w_seed_hlo0_verify_plan(&plan));
  sink_context sink = {
      .accepted_bytes = 14u,
      .flush_status = W_SEED_RUN0_FLUSH_SUCCEEDED,
  };
  w_seed_run0_result result = {W_SEED_RUN0_INVALID_PLAN, 77u, 78u,
                               W_SEED_RUN0_FLUSH_FAILED, 79u};
  CHECK(w_seed_run0_execute(&plan, collect_sink, &sink, &result) ==
        W_SEED_RUN0_OK);
  CHECK(sink.calls == 1u);
  CHECK(sink.byte_count == 14u);
  CHECK(memcmp(sink.bytes, "Hello, world!\n", 14u) == 0);
  CHECK(result.status == W_SEED_RUN0_OK);
  CHECK(result.attempted_bytes == 14u);
  CHECK(result.accepted_bytes == 14u);
  CHECK(result.flush_status == W_SEED_RUN0_FLUSH_SUCCEEDED);
  CHECK(result.sink_calls == 1u);
  return true;
}

static bool expect_sink_fault(size_t accepted_bytes,
                              w_seed_run0_flush_status flush_status) {
  const w_seed_hlo0_plan plan = canonical_plan();
  sink_context sink = {
      .accepted_bytes = accepted_bytes,
      .flush_status = flush_status,
  };
  w_seed_run0_result result = {W_SEED_RUN0_OK, 91u, 92u,
                               W_SEED_RUN0_FLUSH_SUCCEEDED, 93u};
  CHECK(w_seed_run0_execute(&plan, collect_sink, &sink, &result) ==
        W_SEED_RUN0_IO);
  CHECK(sink.calls == 1u);
  CHECK(sink.byte_count == 14u);
  CHECK(result.status == W_SEED_RUN0_IO);
  CHECK(result.attempted_bytes == 14u);
  CHECK(result.accepted_bytes == accepted_bytes);
  CHECK(result.flush_status == flush_status);
  CHECK(result.sink_calls == 1u);
  return true;
}

static bool test_sink_failures_are_faithful(void) {
  CHECK(expect_sink_fault(5u, W_SEED_RUN0_FLUSH_NOT_ATTEMPTED));
  CHECK(expect_sink_fault(14u, W_SEED_RUN0_FLUSH_FAILED));
  CHECK(expect_sink_fault(0u, W_SEED_RUN0_FLUSH_NOT_ATTEMPTED));
  CHECK(expect_sink_fault(0u, W_SEED_RUN0_FLUSH_FAILED));
  CHECK(expect_sink_fault(15u, W_SEED_RUN0_FLUSH_NOT_ATTEMPTED));
  CHECK(expect_sink_fault(13u, W_SEED_RUN0_FLUSH_SUCCEEDED));
  CHECK(expect_sink_fault(14u, (w_seed_run0_flush_status)99));
  return true;
}

static bool test_preflight_failures_preserve_result(void) {
  const w_seed_hlo0_plan canonical = canonical_plan();
  const struct {
    size_t offset;
    uint8_t value;
  } byte_mutations[] = {
      {offsetof(w_seed_hlo0_plan, payload), 0u},
      {offsetof(w_seed_hlo0_plan, stdout_sha256), 0u},
  };
  for (size_t index = 0u;
       index < sizeof(byte_mutations) / sizeof(byte_mutations[0]); index += 1u) {
    w_seed_hlo0_plan plan = canonical;
    ((uint8_t *)&plan)[byte_mutations[index].offset] = byte_mutations[index].value;
    sink_context sink = {0};
    const w_seed_run0_result before = {W_SEED_RUN0_OK, 11u, 12u,
                                       W_SEED_RUN0_FLUSH_FAILED, 13u};
    w_seed_run0_result result = before;
    CHECK(w_seed_run0_execute(&plan, collect_sink, &sink, &result) ==
          W_SEED_RUN0_INVALID_PLAN);
    CHECK(memcmp(&result, &before, sizeof(result)) == 0);
    CHECK(sink.calls == 0u);
  }

  struct forged_case {
    size_t offset;
    bool value;
  } flag_mutations[] = {
      {offsetof(w_seed_hlo0_plan, is_async), true},
      {offsetof(w_seed_hlo0_plan, is_throws), true},
      {offsetof(w_seed_hlo0_plan, is_unsafe), true},
      {offsetof(w_seed_hlo0_plan, has_borrow_clause), true},
      {offsetof(w_seed_hlo0_plan, zero_parameters), false},
      {offsetof(w_seed_hlo0_plan, unit_return), false},
      {offsetof(w_seed_hlo0_plan, exit_success), false},
  };
  for (size_t index = 0u;
       index < sizeof(flag_mutations) / sizeof(flag_mutations[0]); index += 1u) {
    w_seed_hlo0_plan plan = canonical;
    *(bool *)((uint8_t *)&plan + flag_mutations[index].offset) =
        flag_mutations[index].value;
    sink_context sink = {0};
    const w_seed_run0_result before = {W_SEED_RUN0_OK, 21u, 22u,
                                       W_SEED_RUN0_FLUSH_FAILED, 23u};
    w_seed_run0_result result = before;
    CHECK(w_seed_run0_execute(&plan, collect_sink, &sink, &result) ==
          W_SEED_RUN0_INVALID_PLAN);
    CHECK(memcmp(&result, &before, sizeof(result)) == 0);
    CHECK(sink.calls == 0u);
  }
  return true;
}

static bool test_text_and_scalar_mutations(void) {
  const w_seed_hlo0_plan canonical = canonical_plan();
  struct text_case {
    size_t offset;
    const char *value;
  } text_cases[] = {
      {offsetof(w_seed_hlo0_plan, schema), "wrong"},
      {offsetof(w_seed_hlo0_plan, profile), "wrong"},
      {offsetof(w_seed_hlo0_plan, slot), "wrong"},
      {offsetof(w_seed_hlo0_plan, entry_target), "wrong"},
      {offsetof(w_seed_hlo0_plan, handler), "wrong"},
      {offsetof(w_seed_hlo0_plan, callee), "wrong"},
      {offsetof(w_seed_hlo0_plan, requirement), "wrong"},
  };
  for (size_t index = 0u;
       index < sizeof(text_cases) / sizeof(text_cases[0]); index += 1u) {
    w_seed_hlo0_plan plan = canonical;
    set_text((char *)&plan + text_cases[index].offset, sizeof(plan.schema),
             text_cases[index].value);
    CHECK(!w_seed_hlo0_verify_plan(&plan));
  }

  w_seed_hlo0_plan plan = canonical;
  plan.newline_policy = (w_seed_hlo0_newline_policy)99;
  CHECK(!w_seed_hlo0_verify_plan(&plan));
  plan = canonical;
  plan.payload_bytes = 12u;
  CHECK(!w_seed_hlo0_verify_plan(&plan));
  plan = canonical;
  plan.stdout_bytes = 13u;
  CHECK(!w_seed_hlo0_verify_plan(&plan));
  return true;
}

static bool test_alias_and_null_preflight(void) {
  w_seed_hlo0_plan plan = canonical_plan();
  sink_context sink = {0};
  w_seed_run0_result result = {W_SEED_RUN0_OK, 31u, 32u,
                               W_SEED_RUN0_FLUSH_FAILED, 33u};
  const w_seed_run0_result before = result;
  CHECK(w_seed_run0_execute(NULL, collect_sink, &sink, &result) ==
        W_SEED_RUN0_INVALID_PLAN);
  CHECK(memcmp(&result, &before, sizeof(result)) == 0);
  CHECK(w_seed_run0_execute(&plan, collect_sink, &sink, NULL) ==
        W_SEED_RUN0_INVALID_PLAN);
  CHECK(w_seed_run0_execute(&plan, NULL, &sink, &result) ==
        W_SEED_RUN0_INVALID_PLAN);
  CHECK(memcmp(&result, &before, sizeof(result)) == 0);
  CHECK(w_seed_run0_execute(&plan, collect_sink, &sink,
                            (w_seed_run0_result *)(void *)&plan) ==
        W_SEED_RUN0_ALIAS);
  CHECK(sink.calls == 0u);
  return true;
}

static bool test_overlap_boundaries(void) {
  union {
    w_seed_hlo0_plan plan;
    struct {
      uint64_t padding;
      w_seed_run0_result result;
    } shifted_result;
  } plan_first;
  (void)memset(&plan_first, 0, sizeof(plan_first));
  plan_first.plan = canonical_plan();
  uint8_t plan_first_before[sizeof(plan_first)];
  (void)memcpy(plan_first_before, &plan_first, sizeof(plan_first));
  const uintptr_t plan_first_start = (uintptr_t)&plan_first.plan;
  const uintptr_t shifted_result_start =
      (uintptr_t)&plan_first.shifted_result.result;
  CHECK(shifted_result_start > plan_first_start);
  CHECK(shifted_result_start < plan_first_start + sizeof(plan_first.plan));
  sink_context sink = {0};
  CHECK(w_seed_run0_execute(&plan_first.plan, collect_sink, &sink,
                            &plan_first.shifted_result.result) ==
        W_SEED_RUN0_ALIAS);
  CHECK(memcmp(plan_first_before, &plan_first, sizeof(plan_first)) == 0);
  CHECK(sink.calls == 0u);

  union {
    w_seed_run0_result result;
    struct {
      uint64_t padding;
      w_seed_hlo0_plan plan;
    } shifted_plan;
  } result_first;
  (void)memset(&result_first, 0, sizeof(result_first));
  result_first.shifted_plan.plan = canonical_plan();
  uint8_t result_first_before[sizeof(result_first)];
  (void)memcpy(result_first_before, &result_first, sizeof(result_first));
  const uintptr_t result_first_start = (uintptr_t)&result_first.result;
  const uintptr_t shifted_plan_start =
      (uintptr_t)&result_first.shifted_plan.plan;
  CHECK(shifted_plan_start > result_first_start);
  CHECK(shifted_plan_start <
        result_first_start + sizeof(result_first.result));
  CHECK(w_seed_run0_execute(&result_first.shifted_plan.plan, collect_sink,
                            &sink, &result_first.result) ==
        W_SEED_RUN0_ALIAS);
  CHECK(memcmp(result_first_before, &result_first, sizeof(result_first)) == 0);
  CHECK(sink.calls == 0u);

  struct {
    w_seed_hlo0_plan plan;
    w_seed_run0_result result;
  } adjacent = {.plan = canonical_plan()};
  CHECK((uintptr_t)&adjacent.result ==
        (uintptr_t)&adjacent.plan + sizeof(adjacent.plan));
  sink = (sink_context){
      .accepted_bytes = 14u,
      .flush_status = W_SEED_RUN0_FLUSH_SUCCEEDED,
  };
  CHECK(w_seed_run0_execute(&adjacent.plan, collect_sink, &sink,
                            &adjacent.result) == W_SEED_RUN0_OK);
  CHECK(sink.calls == 1u);
  CHECK(adjacent.result.status == W_SEED_RUN0_OK);
  return true;
}

int main(void) {
  if (!test_canonical_execution() || !test_sink_failures_are_faithful() ||
      !test_preflight_failures_preserve_result() ||
      !test_text_and_scalar_mutations() || !test_alias_and_null_preflight() ||
      !test_overlap_boundaries())
    return 1;
  (void)puts("run0 unit: verifier, faithful sink result, aliases, and preflight transaction passed");
  return 0;
}
