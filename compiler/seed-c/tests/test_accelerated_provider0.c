#include "test_accelerated_provider0_fixture.h"

#include "w_seed_accelerated_provider0.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "accelerated provider check failed: %s (%s:%d)\n", \
                    #condition, __FILE__, __LINE__);                          \
      return false;                                                            \
    }                                                                          \
  } while (0)

typedef struct {
  uint32_t fail_phase;
  uint32_t generation;
  uint32_t stage_calls;
  uint32_t submit_calls;
  uint32_t join_calls;
  uint32_t cleanup_calls;
  int32_t result_value;
  bool callback_returns_false;
  bool malformed_event;
  const char *device;
  const char *queue;
} provider0_fake;

enum {
  PROVIDER0_FAKE_FAIL_NONE = 0u,
  PROVIDER0_FAKE_FAIL_STAGE = 1u,
  PROVIDER0_FAKE_FAIL_SUBMIT = 2u,
  PROVIDER0_FAKE_FAIL_JOIN = 3u,
  PROVIDER0_FAKE_FAIL_CLEANUP = 4u,
  PROVIDER0_FAKE_DEVICE_LOST = 5u,
  PROVIDER0_FAKE_CALLBACK_FALSE_STAGE = 6u,
  PROVIDER0_FAKE_CALLBACK_FALSE_SUBMIT = 7u,
  PROVIDER0_FAKE_CALLBACK_FALSE_JOIN = 8u,
  PROVIDER0_FAKE_CALLBACK_FALSE_CLEANUP = 9u,
  PROVIDER0_FAKE_MALFORMED_STAGE = 10u,
  PROVIDER0_FAKE_MALFORMED_SUBMIT = 11u,
  PROVIDER0_FAKE_MALFORMED_JOIN = 12u,
};

static void fake_copy(char destination[], size_t capacity, const char *source) {
  if (destination == NULL || capacity == 0u || source == NULL) return;
  const size_t bytes = strlen(source);
  if (bytes >= capacity) return;
  (void)memcpy(destination, source, bytes);
}

static bool fake_stage(
    void *raw, const w_seed_accelerated_request0_program *request_program,
    const w_seed_accelerated_request0_result *request_result,
    const w_seed_accelerated_provider0_native_artifact *artifact,
    w_seed_accelerated_provider0_callback_event *event) {
  provider0_fake *fake = (provider0_fake *)raw;
  if (fake == NULL || request_program == NULL || request_result == NULL ||
      artifact == NULL || event == NULL)
    return false;
  fake->stage_calls += 1u;
  event->generation = fake->generation;
  event->effect_started = true;
  fake_copy(event->device, sizeof(event->device), fake->device);
  fake_copy(event->queue, sizeof(event->queue), fake->queue);
  fake_copy(event->generation_text, sizeof(event->generation_text),
            "generation:1");
  if (fake->malformed_event ||
      fake->fail_phase == PROVIDER0_FAKE_MALFORMED_STAGE) {
    event->effect_started = false;
    (void)memset(event->device, 'x', sizeof(event->device));
    return !fake->callback_returns_false;
  }
  if (fake->callback_returns_false ||
      fake->fail_phase == PROVIDER0_FAKE_CALLBACK_FALSE_STAGE)
    event->effect_started = false;
  if (fake->fail_phase == PROVIDER0_FAKE_DEVICE_LOST) {
    event->status = W_SEED_ACCELERATED_PROVIDER0_CALLBACK_DEVICE_LOST;
    event->raw_status = 105u;
  } else if (fake->fail_phase == PROVIDER0_FAKE_FAIL_STAGE) {
    event->status = W_SEED_ACCELERATED_PROVIDER0_CALLBACK_FAILURE;
    event->raw_status = 101u;
  }
  return !fake->callback_returns_false &&
         fake->fail_phase != PROVIDER0_FAKE_CALLBACK_FALSE_STAGE;
}

static bool fake_submit(void *raw,
                        w_seed_accelerated_provider0_callback_event *event) {
  provider0_fake *fake = (provider0_fake *)raw;
  if (fake == NULL || event == NULL) return false;
  fake->submit_calls += 1u;
  event->generation = fake->generation;
  event->effect_started = true;
  fake_copy(event->device, sizeof(event->device), fake->device);
  fake_copy(event->queue, sizeof(event->queue), fake->queue);
  fake_copy(event->generation_text, sizeof(event->generation_text),
            "generation:1");
  if (fake->malformed_event ||
      fake->fail_phase == PROVIDER0_FAKE_MALFORMED_SUBMIT) {
    (void)memset(event->queue, 'x', sizeof(event->queue));
    return !fake->callback_returns_false &&
           fake->fail_phase != PROVIDER0_FAKE_CALLBACK_FALSE_SUBMIT;
  }
  if (fake->fail_phase == PROVIDER0_FAKE_DEVICE_LOST) {
    event->status = W_SEED_ACCELERATED_PROVIDER0_CALLBACK_DEVICE_LOST;
    event->raw_status = 106u;
  } else if (fake->fail_phase == PROVIDER0_FAKE_FAIL_SUBMIT) {
    event->status = W_SEED_ACCELERATED_PROVIDER0_CALLBACK_FAILURE;
    event->raw_status = 102u;
  }
  return !fake->callback_returns_false &&
         fake->fail_phase != PROVIDER0_FAKE_CALLBACK_FALSE_SUBMIT;
}

static bool fake_join(void *raw,
                      w_seed_accelerated_provider0_callback_event *event) {
  provider0_fake *fake = (provider0_fake *)raw;
  if (fake == NULL || event == NULL) return false;
  fake->join_calls += 1u;
  event->generation = fake->generation;
  event->effect_started = true;
  event->body_settled = true;
  event->provider_drained = true;
  event->result_value = fake->result_value;
  fake_copy(event->device, sizeof(event->device), fake->device);
  fake_copy(event->queue, sizeof(event->queue), fake->queue);
  fake_copy(event->generation_text, sizeof(event->generation_text),
            "generation:1");
  if (fake->malformed_event ||
      fake->fail_phase == PROVIDER0_FAKE_MALFORMED_JOIN) {
    (void)memset(event->generation_text, 'x', sizeof(event->generation_text));
    return !fake->callback_returns_false &&
           fake->fail_phase != PROVIDER0_FAKE_CALLBACK_FALSE_JOIN;
  }
  if (fake->fail_phase == PROVIDER0_FAKE_DEVICE_LOST) {
    event->status = W_SEED_ACCELERATED_PROVIDER0_CALLBACK_DEVICE_LOST;
    event->raw_status = 107u;
  } else if (fake->fail_phase == PROVIDER0_FAKE_FAIL_JOIN) {
    event->status = W_SEED_ACCELERATED_PROVIDER0_CALLBACK_FAILURE;
    event->raw_status = 103u;
    event->body_settled = false;
    event->provider_drained = false;
  }
  return !fake->callback_returns_false &&
         fake->fail_phase != PROVIDER0_FAKE_CALLBACK_FALSE_JOIN;
}

static bool fake_cleanup(
    void *raw, w_seed_accelerated_provider0_callback_event *event) {
  provider0_fake *fake = (provider0_fake *)raw;
  if (fake == NULL || event == NULL) return false;
  fake->cleanup_calls += 1u;
  event->generation = 1u;
  event->cleanup_succeeded = fake->fail_phase != PROVIDER0_FAKE_FAIL_CLEANUP;
  fake_copy(event->device, sizeof(event->device), fake->device);
  fake_copy(event->queue, sizeof(event->queue), fake->queue);
  fake_copy(event->generation_text, sizeof(event->generation_text),
            "generation:1");
  if (fake->fail_phase == PROVIDER0_FAKE_FAIL_CLEANUP) {
    event->status = W_SEED_ACCELERATED_PROVIDER0_CALLBACK_FAILURE;
    event->raw_status = 104u;
  }
  return fake->fail_phase != PROVIDER0_FAKE_CALLBACK_FALSE_CLEANUP;
}

static const w_seed_accelerated_provider0_vtable PROVIDER0_VTABLE = {
    fake_stage, fake_submit, fake_join, fake_cleanup};

static bool open_authority(provider0_fake *fake,
                           w_seed_accelerated_provider0_authority *authority) {
  return w_seed_accelerated_provider0_authority_open(
      &PROVIDER0_VTABLE, fake, sizeof(*fake),
      (w_seed_accelerated_provider0_text){"fake-provider-implementation", 28u},
      1u, authority);
}

static bool run_case(const w_seed_accelerated_provider0_input *input,
                     provider0_fake *fake,
                     w_seed_accelerated_provider0_status expected_status,
                     bool expect_semantic_output) {
  w_seed_accelerated_provider0_authority authority;
  CHECK(open_authority(fake, &authority));
  w_seed_accelerated_provider0_input bound = *input;
  bound.authority = &authority;
  w_seed_accelerated_provider0_state state;
  w_seed_accelerated_provider0_outcome outcome;
  w_seed_accelerated_provider0_receipt receipt;
  (void)memset(&state, 0, sizeof(state));
  (void)memset(&outcome, 0x5a, sizeof(outcome));
  (void)memset(&receipt, 0x5a, sizeof(receipt));
  const w_seed_accelerated_provider0_outcome before = outcome;
  const w_seed_accelerated_provider0_status status =
      w_seed_accelerated_provider0_run(&bound, &state, &outcome, &receipt);
  CHECK(status == expected_status);
  CHECK(fake->stage_calls == 1u && fake->cleanup_calls == 1u);
  if (expect_semantic_output) {
    CHECK(fake->submit_calls == 1u && fake->join_calls == 1u);
    CHECK(w_seed_accelerated_provider0_verify_outcome(&bound, &outcome));
    CHECK(w_seed_accelerated_provider0_verify_receipt(&bound, &receipt));
    CHECK(outcome.value == 42 && state.phase ==
          W_SEED_ACCELERATED_PROVIDER0_PHASE_JOINED);
  } else {
    CHECK(memcmp(&before, &outcome, sizeof(outcome)) == 0);
    CHECK(state.terminal);
  }
  const w_seed_accelerated_provider0_status rerun =
      w_seed_accelerated_provider0_run(&bound, &state, &outcome, &receipt);
  CHECK(rerun == W_SEED_ACCELERATED_PROVIDER0_INVALID_ARGUMENT ||
        rerun == W_SEED_ACCELERATED_PROVIDER0_INVALID_STATE);
  CHECK(fake->stage_calls == 1u && fake->submit_calls <= 1u &&
        fake->join_calls <= 1u && fake->cleanup_calls == 1u);
  CHECK(w_seed_accelerated_provider0_destroy(&state, &outcome, &receipt) ==
        W_SEED_ACCELERATED_PROVIDER0_INVALID_STATE);
  return true;
}

static bool success_case(const w_seed_accelerated_provider0_input *input,
                         provider0_fake *fake,
                         w_seed_accelerated_provider0_outcome *outcome,
                         w_seed_accelerated_provider0_receipt *receipt) {
  w_seed_accelerated_provider0_authority authority;
  CHECK(open_authority(fake, &authority));
  const w_seed_accelerated_provider0_input bound = {
      input->request_program, input->request_result, input->artifact,
      &authority};
  w_seed_accelerated_provider0_state state;
  (void)memset(&state, 0, sizeof(state));
  (void)memset(outcome, 0, sizeof(*outcome));
  (void)memset(receipt, 0, sizeof(*receipt));
  CHECK(w_seed_accelerated_provider0_run(&bound, &state, outcome, receipt) ==
        W_SEED_ACCELERATED_PROVIDER0_OK);
  CHECK(w_seed_accelerated_provider0_verify_outcome(&bound, outcome) &&
        w_seed_accelerated_provider0_verify_receipt(&bound, receipt) &&
        fake->stage_calls == 1u && fake->submit_calls == 1u &&
        fake->join_calls == 1u && fake->cleanup_calls == 1u);
  return true;
}

static bool stepwise_success_case(
    const w_seed_accelerated_provider0_input *input,
    provider0_fake *fake) {
  w_seed_accelerated_provider0_authority authority;
  CHECK(open_authority(fake, &authority));
  const w_seed_accelerated_provider0_input bound = {
      input->request_program, input->request_result, input->artifact,
      &authority};
  w_seed_accelerated_provider0_state state;
  w_seed_accelerated_provider0_outcome outcome;
  w_seed_accelerated_provider0_receipt receipt;
  (void)memset(&state, 0, sizeof(state));
  (void)memset(&outcome, 0x5a, sizeof(outcome));
  (void)memset(&receipt, 0x5a, sizeof(receipt));
  CHECK(w_seed_accelerated_provider0_begin(&bound, &state, &outcome,
                                           &receipt) ==
        W_SEED_ACCELERATED_PROVIDER0_OK);
  CHECK(w_seed_accelerated_provider0_submit(&state, &receipt) ==
        W_SEED_ACCELERATED_PROVIDER0_OK);
  CHECK(w_seed_accelerated_provider0_join(&state, &receipt) ==
        W_SEED_ACCELERATED_PROVIDER0_OK);
  CHECK(w_seed_accelerated_provider0_destroy(&state, &outcome, &receipt) ==
        W_SEED_ACCELERATED_PROVIDER0_OK);
  CHECK(w_seed_accelerated_provider0_verify_outcome(&bound, &outcome) &&
        w_seed_accelerated_provider0_verify_receipt(&bound, &receipt) &&
        fake->stage_calls == 1u && fake->submit_calls == 1u &&
        fake->join_calls == 1u && fake->cleanup_calls == 1u);
  return true;
}

static bool make_alias_case(const w_seed_accelerated_provider0_input *input,
                            w_seed_accelerated_provider0_state *state,
                            void *outcome_pointer, void *receipt_pointer,
                            w_seed_accelerated_provider0_status expected) {
  if (state == NULL) return false;
  (void)memset(state, 0, sizeof(*state));
  return w_seed_accelerated_provider0_run(
             input, state,
             (w_seed_accelerated_provider0_outcome *)outcome_pointer,
             (w_seed_accelerated_provider0_receipt *)receipt_pointer) ==
             expected &&
         state->phase == W_SEED_ACCELERATED_PROVIDER0_PHASE_NONE;
}

static bool test_provider0_boundary(void) {
  w_seed_accelerated_request0_program request_program;
  w_seed_accelerated_request0_result request_result;
  (void)memset(&request_program, 0, sizeof(request_program));
  (void)memset(&request_result, 0, sizeof(request_result));
  CHECK(w_seed_test_accelerated_provider0_make_request(&request_program,
                                                        &request_result));
  static const uint8_t native_bytes[] = "fake-native-artifact";
  w_seed_accelerated_provider0_artifact_receipt artifact_receipt;
  CHECK(w_seed_test_accelerated_provider0_make_native_receipt(
      &request_program, native_bytes, sizeof(native_bytes) - 1u,
      &artifact_receipt));
  const w_seed_accelerated_request0_record *request =
      &request_program.requests[0];
  const w_seed_accelerated_provider0_native_artifact artifact = {
      native_bytes,
      sizeof(native_bytes) - 1u,
      {(const char *)request_program.text + request->target_offset,
       request->target_bytes},
      {(const char *)request_program.text + request->provider_class_offset,
       request->provider_class_bytes},
      &artifact_receipt};
  const w_seed_accelerated_provider0_input input = {
      &request_program, &request_result, &artifact, NULL};

  provider0_fake success = {0u, 1u, 0u, 0u, 0u, 0u, 42, false, false,
                            "device:fake0", "queue:fake0"};
  CHECK(run_case(&input, &success, W_SEED_ACCELERATED_PROVIDER0_OK, true));

  provider0_fake stepwise = {0u, 1u, 0u, 0u, 0u, 0u, 42, false, false,
                             "device:fake0", "queue:fake0"};
  CHECK(stepwise_success_case(&input, &stepwise));

  provider0_fake first_identity = {0u, 1u, 0u, 0u, 0u, 0u, 42, false,
                                   false, "device:fake0", "queue:fake0"};
  provider0_fake second_identity = {0u, 1u, 0u, 0u, 0u, 0u, 42, false,
                                    false, "device:other0", "queue:other0"};
  w_seed_accelerated_provider0_outcome first_outcome;
  w_seed_accelerated_provider0_outcome second_outcome;
  w_seed_accelerated_provider0_receipt first_receipt;
  w_seed_accelerated_provider0_receipt second_receipt;
  CHECK(success_case(&input, &first_identity, &first_outcome, &first_receipt));
  CHECK(success_case(&input, &second_identity, &second_outcome,
                     &second_receipt));
  CHECK(memcmp(first_outcome.semantic_digest, second_outcome.semantic_digest,
               sizeof(first_outcome.semantic_digest)) == 0 &&
        memcmp(first_outcome.phases, second_outcome.phases,
               sizeof(first_outcome.phases)) == 0 &&
        memcmp(first_receipt.provenance_digest,
               second_receipt.provenance_digest,
               sizeof(first_receipt.provenance_digest)) != 0);

  const uint32_t failures[] = {PROVIDER0_FAKE_FAIL_STAGE,
                               PROVIDER0_FAKE_FAIL_SUBMIT,
                               PROVIDER0_FAKE_FAIL_JOIN,
                               PROVIDER0_FAKE_DEVICE_LOST,
                               PROVIDER0_FAKE_FAIL_CLEANUP};
  const w_seed_accelerated_provider0_status failure_statuses[] = {
      W_SEED_ACCELERATED_PROVIDER0_PROVIDER_FAILURE,
      W_SEED_ACCELERATED_PROVIDER0_PROVIDER_FAILURE,
      W_SEED_ACCELERATED_PROVIDER0_PROVIDER_FAILURE,
      W_SEED_ACCELERATED_PROVIDER0_DEVICE_LOST,
      W_SEED_ACCELERATED_PROVIDER0_CLEANUP_UNCERTAIN};
  for (size_t index = 0u; index < sizeof(failures) / sizeof(failures[0]);
       index += 1u) {
    provider0_fake failure = {failures[index], 1u, 0u, 0u, 0u, 0u, 42,
                              false, false, "device:fake0", "queue:fake0"};
    CHECK(run_case(&input, &failure, failure_statuses[index], false));
  }

  provider0_fake wrong_result = {0u, 1u, 0u, 0u, 0u, 0u, 41, false, false,
                                 "device:fake0", "queue:fake0"};
  CHECK(run_case(&input, &wrong_result,
                 W_SEED_ACCELERATED_PROVIDER0_RESULT_MISMATCH, false));

  provider0_fake stale = {0u, 2u, 0u, 0u, 0u, 0u, 42, false, false,
                          "device:fake0", "queue:fake0"};
  CHECK(run_case(&input, &stale,
                 W_SEED_ACCELERATED_PROVIDER0_STALE_GENERATION, false));

  provider0_fake callback_false = {0u, 1u, 0u, 0u, 0u, 0u, 42, true, false,
                                   "device:fake0", "queue:fake0"};
  CHECK(run_case(&input, &callback_false,
                 W_SEED_ACCELERATED_PROVIDER0_PROVIDER_FAILURE, false));

  provider0_fake callback_false_submit = {
      PROVIDER0_FAKE_CALLBACK_FALSE_SUBMIT, 1u, 0u, 0u, 0u, 0u, 42,
      false, false, "device:fake0", "queue:fake0"};
  CHECK(run_case(&input, &callback_false_submit,
                 W_SEED_ACCELERATED_PROVIDER0_PROVIDER_FAILURE, false));

  provider0_fake callback_false_join = {
      PROVIDER0_FAKE_CALLBACK_FALSE_JOIN, 1u, 0u, 0u, 0u, 0u, 42,
      false, false, "device:fake0", "queue:fake0"};
  CHECK(run_case(&input, &callback_false_join,
                 W_SEED_ACCELERATED_PROVIDER0_PROVIDER_FAILURE, false));

  provider0_fake callback_false_cleanup = {
      PROVIDER0_FAKE_CALLBACK_FALSE_CLEANUP, 1u, 0u, 0u, 0u, 0u, 42,
      false, false, "device:fake0", "queue:fake0"};
  CHECK(run_case(&input, &callback_false_cleanup,
                 W_SEED_ACCELERATED_PROVIDER0_CLEANUP_UNCERTAIN, false));

  provider0_fake malformed = {0u, 1u, 0u, 0u, 0u, 0u, 42, false, true,
                              "device:fake0", "queue:fake0"};
  CHECK(run_case(&input, &malformed,
                 W_SEED_ACCELERATED_PROVIDER0_PROTOCOL_MISMATCH, false));

  provider0_fake malformed_submit = {
      PROVIDER0_FAKE_MALFORMED_SUBMIT, 1u, 0u, 0u, 0u, 0u, 42, false, false,
      "device:fake0", "queue:fake0"};
  CHECK(run_case(&input, &malformed_submit,
                 W_SEED_ACCELERATED_PROVIDER0_PROTOCOL_MISMATCH, false));

  provider0_fake malformed_join = {
      PROVIDER0_FAKE_MALFORMED_JOIN, 1u, 0u, 0u, 0u, 0u, 42, false, false,
      "device:fake0", "queue:fake0"};
  CHECK(run_case(&input, &malformed_join,
                 W_SEED_ACCELERATED_PROVIDER0_PROTOCOL_MISMATCH, false));

  w_seed_accelerated_provider0_artifact_receipt wrong_receipt = artifact_receipt;
  wrong_receipt.native_artifact_digest[0] ^= UINT8_C(1);
  const w_seed_accelerated_provider0_native_artifact wrong_artifact = {
      native_bytes, sizeof(native_bytes) - 1u, artifact.target,
      artifact.provider_abi_class, &wrong_receipt};
  const w_seed_accelerated_provider0_input wrong_input = {
      &request_program, &request_result, &wrong_artifact, NULL};
  provider0_fake untouched = {0u, 1u, 0u, 0u, 0u, 0u, 42, false, false,
                              "device:fake0", "queue:fake0"};
  w_seed_accelerated_provider0_authority authority;
  CHECK(open_authority(&untouched, &authority));
  const w_seed_accelerated_provider0_input wrong_bound = {
      wrong_input.request_program, wrong_input.request_result,
      wrong_input.artifact, &authority};
  w_seed_accelerated_provider0_state wrong_state;
  w_seed_accelerated_provider0_outcome wrong_outcome;
  w_seed_accelerated_provider0_receipt wrong_output;
  (void)memset(&wrong_state, 0, sizeof(wrong_state));
  (void)memset(&wrong_outcome, 0x5a, sizeof(wrong_outcome));
  (void)memset(&wrong_output, 0x5a, sizeof(wrong_output));
  const w_seed_accelerated_provider0_outcome wrong_before = wrong_outcome;
  CHECK(w_seed_accelerated_provider0_run(&wrong_bound, &wrong_state,
                                         &wrong_outcome, &wrong_output) ==
        W_SEED_ACCELERATED_PROVIDER0_INVALID_ARTIFACT);
  CHECK(memcmp(&wrong_before, &wrong_outcome, sizeof(wrong_outcome)) == 0 &&
        untouched.stage_calls == 0u);

  w_seed_accelerated_provider0_artifact_receipt wrong_request_receipt =
      artifact_receipt;
  wrong_request_receipt.request_device_artifact_digest[0] ^= UINT8_C(1);
  const w_seed_accelerated_provider0_native_artifact wrong_request_artifact = {
      native_bytes, sizeof(native_bytes) - 1u, artifact.target,
      artifact.provider_abi_class, &wrong_request_receipt};
  const w_seed_accelerated_provider0_input wrong_request_input = {
      &request_program, &request_result, &wrong_request_artifact, &authority};
  (void)memset(&wrong_state, 0, sizeof(wrong_state));
  (void)memset(&wrong_outcome, 0x5a, sizeof(wrong_outcome));
  (void)memset(&wrong_output, 0x5a, sizeof(wrong_output));
  CHECK(w_seed_accelerated_provider0_run(
            &wrong_request_input, &wrong_state, &wrong_outcome,
            &wrong_output) == W_SEED_ACCELERATED_PROVIDER0_INVALID_ARTIFACT &&
        untouched.stage_calls == 0u);

  w_seed_accelerated_provider0_native_artifact oversized_artifact = artifact;
  oversized_artifact.byte_count =
      W_SEED_ACCELERATED_PROVIDER0_MAX_NATIVE_ARTIFACT_BYTES + 1u;
  w_seed_accelerated_provider0_state oversized_state;
  w_seed_accelerated_provider0_outcome oversized_outcome;
  w_seed_accelerated_provider0_receipt oversized_receipt;
  provider0_fake capacity_fake = {0u, 1u, 0u, 0u, 0u, 0u, 42, false, false,
                                  "device:fake0", "queue:fake0"};
  w_seed_accelerated_provider0_authority capacity_authority;
  CHECK(open_authority(&capacity_fake, &capacity_authority));
  const w_seed_accelerated_provider0_input capacity_bound = {
      &request_program, &request_result, &oversized_artifact,
      &capacity_authority};
  (void)memset(&oversized_state, 0, sizeof(oversized_state));
  (void)memset(&oversized_outcome, 0x5a, sizeof(oversized_outcome));
  (void)memset(&oversized_receipt, 0x5a, sizeof(oversized_receipt));
  const w_seed_accelerated_provider0_outcome oversized_before =
      oversized_outcome;
  CHECK(w_seed_accelerated_provider0_run(
            &capacity_bound, &oversized_state, &oversized_outcome,
            &oversized_receipt) == W_SEED_ACCELERATED_PROVIDER0_CAPACITY &&
        memcmp(&oversized_before, &oversized_outcome,
               sizeof(oversized_outcome)) == 0 &&
        capacity_fake.stage_calls == 0u);

  /* Every output pair and every output/input overlap is rejected before the
   * stage callback. */
  provider0_fake alias_fake = {0u, 1u, 0u, 0u, 0u, 0u, 42, false, false,
                               "device:fake0", "queue:fake0"};
  w_seed_accelerated_provider0_authority alias_authority;
  CHECK(open_authority(&alias_fake, &alias_authority));
  w_seed_accelerated_provider0_input alias_input = input;
  alias_input.authority = &alias_authority;
  w_seed_accelerated_provider0_state alias_state;
  w_seed_accelerated_provider0_outcome alias_outcome;
  w_seed_accelerated_provider0_receipt alias_receipt;
  (void)memset(&alias_state, 0, sizeof(alias_state));
  (void)memset(&alias_outcome, 0x5a, sizeof(alias_outcome));
  (void)memset(&alias_receipt, 0x5a, sizeof(alias_receipt));
  CHECK(make_alias_case(&alias_input, &alias_state, &alias_outcome,
                        &alias_outcome,
                        W_SEED_ACCELERATED_PROVIDER0_ALIAS));
  CHECK(make_alias_case(&alias_input, &alias_state, &alias_state, &alias_receipt,
                        W_SEED_ACCELERATED_PROVIDER0_ALIAS));
  CHECK(make_alias_case(&alias_input, &alias_state, &alias_outcome, &alias_state,
                        W_SEED_ACCELERATED_PROVIDER0_ALIAS));
  CHECK(make_alias_case(&alias_input, &alias_state, &request_program,
                        &alias_receipt,
                        W_SEED_ACCELERATED_PROVIDER0_ALIAS));
  CHECK(make_alias_case(&alias_input, &alias_state, &alias_outcome,
                        &request_program,
                        W_SEED_ACCELERATED_PROVIDER0_ALIAS));
  CHECK(alias_fake.stage_calls == 0u);

  provider0_fake stepwise_alias_fake = {
      0u, 1u, 0u, 0u, 0u, 0u, 42, false, false, "device:fake0",
      "queue:fake0"};
  w_seed_accelerated_provider0_authority stepwise_alias_authority;
  CHECK(open_authority(&stepwise_alias_fake, &stepwise_alias_authority));
  const w_seed_accelerated_provider0_input stepwise_alias_input = {
      &request_program, &request_result, &artifact,
      &stepwise_alias_authority};
  w_seed_accelerated_provider0_state stepwise_alias_state;
  w_seed_accelerated_provider0_outcome stepwise_alias_outcome;
  w_seed_accelerated_provider0_receipt stepwise_alias_receipt;
  (void)memset(&stepwise_alias_state, 0, sizeof(stepwise_alias_state));
  (void)memset(&stepwise_alias_outcome, 0x5a, sizeof(stepwise_alias_outcome));
  (void)memset(&stepwise_alias_receipt, 0x5a, sizeof(stepwise_alias_receipt));
  CHECK(w_seed_accelerated_provider0_begin(
            &stepwise_alias_input, &stepwise_alias_state,
            (w_seed_accelerated_provider0_outcome *)(void *)&stepwise_alias_state,
            &stepwise_alias_receipt) == W_SEED_ACCELERATED_PROVIDER0_ALIAS &&
        stepwise_alias_fake.stage_calls == 0u);

  /* A stepwise caller cannot substitute a new receipt or outcome after the
   * preflight. The failed call must not invoke another provider callback. */
  provider0_fake exact_fake = {0u, 1u, 0u, 0u, 0u, 0u, 42, false, false,
                               "device:fake0", "queue:fake0"};
  w_seed_accelerated_provider0_authority exact_authority;
  CHECK(open_authority(&exact_fake, &exact_authority));
  const w_seed_accelerated_provider0_input exact_input = {
      &request_program, &request_result, &artifact, &exact_authority};
  w_seed_accelerated_provider0_state exact_state;
  w_seed_accelerated_provider0_outcome exact_outcome;
  w_seed_accelerated_provider0_receipt exact_receipt;
  w_seed_accelerated_provider0_receipt alternate_receipt;
  (void)memset(&exact_state, 0, sizeof(exact_state));
  (void)memset(&exact_outcome, 0x5a, sizeof(exact_outcome));
  (void)memset(&exact_receipt, 0x5a, sizeof(exact_receipt));
  (void)memset(&alternate_receipt, 0x5a, sizeof(alternate_receipt));
  CHECK(w_seed_accelerated_provider0_begin(
            &exact_input, &exact_state, &exact_outcome, &exact_receipt) ==
        W_SEED_ACCELERATED_PROVIDER0_OK);
  CHECK(w_seed_accelerated_provider0_submit(&exact_state, &alternate_receipt) ==
        W_SEED_ACCELERATED_PROVIDER0_INVALID_STATE &&
        exact_fake.submit_calls == 0u);
  CHECK(w_seed_accelerated_provider0_submit(&exact_state, &exact_receipt) ==
        W_SEED_ACCELERATED_PROVIDER0_OK);
  CHECK(w_seed_accelerated_provider0_join(&exact_state, &exact_receipt) ==
        W_SEED_ACCELERATED_PROVIDER0_OK);
  CHECK(w_seed_accelerated_provider0_destroy(&exact_state, &exact_outcome,
                                             &alternate_receipt) ==
        W_SEED_ACCELERATED_PROVIDER0_INVALID_STATE &&
        exact_fake.cleanup_calls == 0u);
  CHECK(w_seed_accelerated_provider0_destroy(&exact_state, &exact_outcome,
                                             &exact_receipt) ==
        W_SEED_ACCELERATED_PROVIDER0_OK && exact_fake.cleanup_calls == 1u);
  return true;
}

int main(void) {
  if (!test_provider0_boundary()) return 1;
  (void)printf("ACCPROV0 private launch/join/result: PASS\n");
  (void)printf("ACCPROV0 lifecycle/fault/alias/provenance barriers: PASS\n");
  return 0;
}
