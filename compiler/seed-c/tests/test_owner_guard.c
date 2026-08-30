#include "w_seed_owner_guard.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_LEVEL_CAPACITY 8u
#define TEST_HANDLE_CAPACITY 24u

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "owner guard test failed: %s (%s:%d)\n",         \
                    #condition, __FILE__, __LINE__);                           \
      return false;                                                            \
    }                                                                          \
  } while (0)

typedef enum {
  FAKE_CORRUPTION_NONE = 0,
  FAKE_CORRUPTION_ZERO_GENERATION,
  FAKE_CORRUPTION_STALE_GENERATION_AND_BAD_LEVEL_COUNT,
  FAKE_CORRUPTION_BAD_LEVEL_COUNT,
  FAKE_CORRUPTION_BAD_DIRECTORY_ORDINAL,
  FAKE_CORRUPTION_BAD_CANDIDATE_INDEX,
  FAKE_CORRUPTION_BAD_ROOT_TERMINAL,
  FAKE_CORRUPTION_BAD_PHASE,
} fake_corruption;

typedef struct {
  uint64_t next_generation;
  uint64_t active_generation;
  bool session_live;
  size_t begin_calls;
  size_t revalidate_calls;
  size_t abort_begin_calls;
  size_t abort_without_session;
  size_t destroy_calls;
  size_t destroy_without_session;
  size_t stale_destroy_generation;
  uint64_t destroy_generations[4];
  size_t partial_cleanup_calls;
  size_t acquired[TEST_HANDLE_CAPACITY];
  size_t acquired_count;
  size_t closed[TEST_HANDLE_CAPACITY];
  size_t closed_count;
  const uint8_t *original_path;
  bool received_path_is_copy;
  bool received_path_content_ok;
  size_t received_path_length;
  bool partial_begin_failure;
  w_seed_owner_guard_backend_status begin_status;
  w_seed_owner_guard_backend_phase begin_failure_phase;
  size_t begin_failure_level;
  size_t begin_required_capacity;
  w_seed_owner_guard_backend_status revalidate_status;
  w_seed_owner_guard_backend_phase revalidate_failure_phase;
  size_t revalidate_failure_level;
  size_t revalidate_required_capacity;
  fake_corruption begin_corruption;
  fake_corruption revalidate_corruption;
  w_seed_owner_guard_observation begin_observations[TEST_LEVEL_CAPACITY];
  size_t begin_level_count;
  size_t begin_candidate_count;
  w_seed_owner_guard_observation
      revalidation_observations[TEST_LEVEL_CAPACITY];
  size_t revalidation_level_count;
  size_t revalidation_candidate_count;
} fake_context;

typedef struct {
  fake_context context;
  uint8_t path[sizeof("src/main.w") - 1u];
  w_seed_owner_guard_observation staged[TEST_LEVEL_CAPACITY];
  w_seed_owner_guard_observation revalidation[TEST_LEVEL_CAPACITY];
  w_seed_owner_guard_candidate_ref published[TEST_LEVEL_CAPACITY];
  w_seed_owner_guard_input input;
  w_seed_owner_guard guard;
  w_seed_owner_guard_result result;
} fixture;

static w_seed_owner_guard_backend_result fake_failure(
    w_seed_owner_guard_backend_status status,
    w_seed_owner_guard_backend_phase phase, size_t level,
    size_t required_capacity, uint64_t generation) {
  const w_seed_owner_guard_backend_result result = {
      status, phase, level, required_capacity, generation, 0u, 0u,
  };
  return result;
}

static void fake_acquire_handles(fake_context *context, size_t level_count,
                                 size_t candidate_count) {
  context->acquired_count = 2u + level_count + candidate_count;
  if (context->acquired_count > TEST_HANDLE_CAPACITY)
    context->acquired_count = TEST_HANDLE_CAPACITY;
  for (size_t index = 0u; index < context->acquired_count; index += 1u)
    context->acquired[index] = index + 1u;
  context->closed_count = 0u;
}

static w_seed_owner_guard_backend_result fake_success_result(
    uint64_t generation, size_t level_count, size_t candidate_count) {
  const w_seed_owner_guard_backend_result result = {
      W_SEED_OWNER_GUARD_BACKEND_OK,
      W_SEED_OWNER_GUARD_BACKEND_PHASE_COMMIT,
      level_count - 1u,
      0u,
      generation,
      level_count,
      candidate_count,
  };
  return result;
}

static void corrupt_success(fake_corruption corruption,
                            w_seed_owner_guard_observation *observations,
                            w_seed_owner_guard_backend_result *result) {
  switch (corruption) {
    case FAKE_CORRUPTION_NONE:
      return;
    case FAKE_CORRUPTION_ZERO_GENERATION:
      result->generation = 0u;
      return;
    case FAKE_CORRUPTION_STALE_GENERATION_AND_BAD_LEVEL_COUNT:
      result->generation -= 1u;
      result->level_count = TEST_LEVEL_CAPACITY + 1u;
      result->level_index = result->level_count - 1u;
      return;
    case FAKE_CORRUPTION_BAD_LEVEL_COUNT:
      result->level_count = TEST_LEVEL_CAPACITY + 1u;
      result->level_index = result->level_count - 1u;
      return;
    case FAKE_CORRUPTION_BAD_DIRECTORY_ORDINAL:
      observations[0].directory_ordinal = 1u;
      return;
    case FAKE_CORRUPTION_BAD_CANDIDATE_INDEX:
      observations[0].candidate_index = 2u;
      return;
    case FAKE_CORRUPTION_BAD_ROOT_TERMINAL:
      observations[result->level_count - 1u].root_terminal = false;
      return;
    case FAKE_CORRUPTION_BAD_PHASE:
      result->phase = W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_PARENT;
      return;
  }
}

static w_seed_owner_guard_backend_result fake_begin(
    void *context_value, w_seed_byte_view source_path,
    w_seed_owner_guard_observation *observations,
    size_t observation_capacity) {
  fake_context *context = (fake_context *)context_value;
  context->begin_calls += 1u;
  context->received_path_is_copy =
      source_path.data != NULL && source_path.data != context->original_path;
  context->received_path_content_ok =
      source_path.data != NULL &&
      source_path.length == sizeof("src/main.w") - 1u &&
      memcmp(source_path.data, "src/main.w", source_path.length) == 0;
  context->received_path_length = source_path.length;
  if (context->session_live)
    return fake_failure(W_SEED_OWNER_GUARD_BACKEND_INVALID,
                        W_SEED_OWNER_GUARD_BACKEND_PHASE_VALIDATE,
                        W_SEED_OWNER_GUARD_NO_LEVEL, 0u, 0u);
  if (source_path.data == NULL || source_path.length != sizeof("src/main.w") - 1u ||
      memcmp(source_path.data, "src/main.w", source_path.length) != 0)
    return fake_failure(W_SEED_OWNER_GUARD_BACKEND_INVALID,
                        W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_START,
                        W_SEED_OWNER_GUARD_NO_LEVEL, 0u, 0u);
  if (context->begin_status != W_SEED_OWNER_GUARD_BACKEND_OK) {
    if (context->partial_begin_failure) {
      fake_acquire_handles(context, 2u, 1u);
      for (size_t index = context->acquired_count; index > 0u; index -= 1u)
        context->closed[context->closed_count++] =
            context->acquired[index - 1u];
      context->partial_cleanup_calls += 1u;
    }
    return fake_failure(context->begin_status, context->begin_failure_phase,
                        context->begin_failure_level,
                        context->begin_required_capacity, 0u);
  }
  if (context->begin_level_count > observation_capacity) {
    return fake_failure(W_SEED_OWNER_GUARD_BACKEND_CAPACITY,
                        W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_PARENT,
                        observation_capacity,
                        context->begin_level_count, 0u);
  }
  if (context->next_generation == UINT64_MAX) {
    return fake_failure(W_SEED_OWNER_GUARD_BACKEND_FAULT,
                        W_SEED_OWNER_GUARD_BACKEND_PHASE_VALIDATE,
                        W_SEED_OWNER_GUARD_NO_LEVEL, 0u, 0u);
  }
  context->next_generation += 1u;
  context->active_generation = context->next_generation;
  context->session_live = true;
  fake_acquire_handles(context, context->begin_level_count,
                       context->begin_candidate_count);
  (void)memcpy(observations, context->begin_observations,
               context->begin_level_count * sizeof(observations[0]));
  w_seed_owner_guard_backend_result result = fake_success_result(
      context->active_generation, context->begin_level_count,
      context->begin_candidate_count);
  corrupt_success(context->begin_corruption, observations, &result);
  return result;
}

static w_seed_owner_guard_backend_result fake_revalidate(
    void *context_value, uint64_t generation,
    w_seed_owner_guard_observation *observations,
    size_t observation_capacity) {
  fake_context *context = (fake_context *)context_value;
  context->revalidate_calls += 1u;
  if (!context->session_live || generation == 0u ||
      generation != context->active_generation)
    return fake_failure(W_SEED_OWNER_GUARD_BACKEND_INVALID,
                        W_SEED_OWNER_GUARD_BACKEND_PHASE_VALIDATE,
                        W_SEED_OWNER_GUARD_NO_LEVEL, 0u, generation);
  if (context->revalidate_status != W_SEED_OWNER_GUARD_BACKEND_OK) {
    return fake_failure(
        context->revalidate_status, context->revalidate_failure_phase,
        context->revalidate_failure_level,
        context->revalidate_required_capacity, generation);
  }
  if (context->revalidation_level_count > observation_capacity) {
    return fake_failure(W_SEED_OWNER_GUARD_BACKEND_CAPACITY,
                        W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_PARENT,
                        observation_capacity,
                        context->revalidation_level_count, generation);
  }
  (void)memcpy(observations, context->revalidation_observations,
               context->revalidation_level_count * sizeof(observations[0]));
  w_seed_owner_guard_backend_result result = fake_success_result(
      generation, context->revalidation_level_count,
      context->revalidation_candidate_count);
  corrupt_success(context->revalidate_corruption, observations, &result);
  return result;
}

static void fake_destroy(void *context_value, uint64_t generation) {
  fake_context *context = (fake_context *)context_value;
  if (context->destroy_calls <
      sizeof(context->destroy_generations) /
          sizeof(context->destroy_generations[0]))
    context->destroy_generations[context->destroy_calls] = generation;
  context->destroy_calls += 1u;
  if (!context->session_live) {
    context->destroy_without_session += 1u;
    return;
  }
  if (generation == 0u || generation != context->active_generation) {
    context->stale_destroy_generation += 1u;
    return;
  }
  for (size_t index = context->acquired_count; index > 0u; index -= 1u)
    context->closed[context->closed_count++] = context->acquired[index - 1u];
  context->session_live = false;
  context->active_generation = 0u;
}

static void fake_abort_begin(void *context_value) {
  fake_context *context = (fake_context *)context_value;
  context->abort_begin_calls += 1u;
  if (!context->session_live) {
    context->abort_without_session += 1u;
    return;
  }
  for (size_t index = context->acquired_count; index > 0u; index -= 1u)
    context->closed[context->closed_count++] = context->acquired[index - 1u];
  context->session_live = false;
  context->active_generation = 0u;
}

static void observations_with_candidates(
    w_seed_owner_guard_observation *observations) {
  observations[0] = (w_seed_owner_guard_observation){0u, 0u, false};
  observations[1] = (w_seed_owner_guard_observation){
      1u, W_SEED_OWNER_GUARD_NO_CANDIDATE, false};
  observations[2] = (w_seed_owner_guard_observation){2u, 1u, false};
  observations[3] = (w_seed_owner_guard_observation){
      3u, W_SEED_OWNER_GUARD_NO_CANDIDATE, true};
}

static void observations_without_candidates(
    w_seed_owner_guard_observation *observations) {
  observations[0] = (w_seed_owner_guard_observation){
      0u, W_SEED_OWNER_GUARD_NO_CANDIDATE, false};
  observations[1] = (w_seed_owner_guard_observation){
      1u, W_SEED_OWNER_GUARD_NO_CANDIDATE, false};
  observations[2] = (w_seed_owner_guard_observation){
      2u, W_SEED_OWNER_GUARD_NO_CANDIDATE, true};
}

static void fixture_init(fixture *fixture_value, bool candidates) {
  (void)memset(fixture_value, 0, sizeof(*fixture_value));
  (void)memcpy(fixture_value->path, "src/main.w", sizeof(fixture_value->path));
  fixture_value->context.original_path = fixture_value->path;
  fixture_value->context.begin_status = W_SEED_OWNER_GUARD_BACKEND_OK;
  fixture_value->context.begin_failure_phase =
      W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_START;
  fixture_value->context.begin_failure_level = W_SEED_OWNER_GUARD_NO_LEVEL;
  fixture_value->context.revalidate_status = W_SEED_OWNER_GUARD_BACKEND_OK;
  fixture_value->context.revalidate_failure_phase =
      W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_DIRECTORY;
  fixture_value->context.revalidate_failure_level =
      W_SEED_OWNER_GUARD_NO_LEVEL;
  if (candidates) {
    observations_with_candidates(fixture_value->context.begin_observations);
    observations_with_candidates(
        fixture_value->context.revalidation_observations);
    fixture_value->context.begin_level_count = 4u;
    fixture_value->context.begin_candidate_count = 2u;
    fixture_value->context.revalidation_level_count = 4u;
    fixture_value->context.revalidation_candidate_count = 2u;
  } else {
    observations_without_candidates(fixture_value->context.begin_observations);
    observations_without_candidates(
        fixture_value->context.revalidation_observations);
    fixture_value->context.begin_level_count = 3u;
    fixture_value->context.begin_candidate_count = 0u;
    fixture_value->context.revalidation_level_count = 3u;
    fixture_value->context.revalidation_candidate_count = 0u;
  }
  (void)memset(fixture_value->published, 0xA5,
               sizeof(fixture_value->published));
  fixture_value->input.source_path =
      (w_seed_byte_view){fixture_value->path, sizeof(fixture_value->path)};
  fixture_value->input.max_levels = TEST_LEVEL_CAPACITY;
  fixture_value->input.storage = (w_seed_owner_guard_storage){
      fixture_value->staged,
      TEST_LEVEL_CAPACITY,
      fixture_value->revalidation,
      TEST_LEVEL_CAPACITY,
      fixture_value->published,
      TEST_LEVEL_CAPACITY,
  };
  fixture_value->input.backend = (w_seed_owner_guard_backend){
      .context = &fixture_value->context,
      .begin = fake_begin,
      .revalidate = fake_revalidate,
      .abort_begin = fake_abort_begin,
      .destroy = fake_destroy,
  };
  fixture_value->input.backend_context_size = sizeof(fixture_value->context);
  (void)memset(&fixture_value->result, 0x5A,
               sizeof(fixture_value->result));
}

static bool object_is_zero(const void *object, size_t size) {
  const uint8_t *bytes = (const uint8_t *)object;
  for (size_t index = 0u; index < size; index += 1u) {
    if (bytes[index] != 0u) return false;
  }
  return true;
}

static bool closed_in_reverse(const fake_context *context) {
  if (context->closed_count != context->acquired_count) return false;
  for (size_t index = 0u; index < context->closed_count; index += 1u) {
    if (context->closed[index] !=
        context->acquired[context->acquired_count - index - 1u])
      return false;
  }
  return true;
}

static bool test_candidate_lifecycle(void) {
  fixture state;
  fixture_init(&state, true);
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_OK);
  CHECK(state.context.begin_calls == 1u && state.context.session_live);
  CHECK(state.context.received_path_is_copy);
  CHECK(state.context.received_path_content_ok);
  CHECK(state.context.received_path_length == sizeof(state.path));
  CHECK(state.result.status == W_SEED_OWNER_GUARD_OK &&
        state.result.phase == W_SEED_OWNER_GUARD_PHASE_COMMIT);
  const uint64_t first_generation = state.guard.generation;
  CHECK(first_generation == 1u);

  w_seed_owner_guard_view view;
  (void)memset(&view, 0, sizeof(view));
  CHECK(w_seed_owner_guard_get_view(&state.guard, &view));
  CHECK(view.lifecycle == W_SEED_OWNER_GUARD_LIVE_OBSERVED);
  CHECK(view.disposition == W_SEED_OWNER_GUARD_CANDIDATES_OBSERVED);
  CHECK(view.root_terminal && view.directory_count == 4u &&
        view.candidate_count == 2u);
  CHECK(view.candidates == state.published);
  CHECK(view.candidates[0].generation == first_generation &&
        view.candidates[0].directory_ordinal == 0u &&
        view.candidates[0].candidate_index == 0u);
  CHECK(view.candidates[1].generation == first_generation &&
        view.candidates[1].directory_ordinal == 2u &&
        view.candidates[1].candidate_index == 1u);
  w_seed_owner_guard_candidate_ref candidate;
  CHECK(w_seed_owner_guard_get_candidate(&state.guard, 1u, &candidate));
  CHECK(candidate.generation == first_generation &&
        candidate.directory_ordinal == 2u && candidate.candidate_index == 1u);
  const w_seed_owner_guard_candidate_ref candidate_snapshot = candidate;
  CHECK(!w_seed_owner_guard_get_candidate(&state.guard, 2u, &candidate));
  CHECK(memcmp(&candidate, &candidate_snapshot, sizeof(candidate)) == 0);

  w_seed_owner_guard copied = state.guard;
  w_seed_owner_guard_destroy(&copied);
  CHECK(state.context.destroy_calls == 0u && state.context.session_live);
  CHECK(w_seed_owner_guard_get_view(&state.guard, &view));

  CHECK(w_seed_owner_guard_revalidate(&state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_OK);
  CHECK(state.context.revalidate_calls == 1u);
  CHECK(w_seed_owner_guard_get_view(&state.guard, &view));
  CHECK(view.lifecycle == W_SEED_OWNER_GUARD_LIVE_RECONFIRMED);
  CHECK(view.disposition == W_SEED_OWNER_GUARD_CANDIDATES_RECONFIRMED);
  CHECK(view.generation == first_generation && view.candidate_count == 2u);
  CHECK(w_seed_owner_guard_revalidate(&state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_INVALID);
  CHECK(state.context.revalidate_calls == 1u && state.guard.session_live &&
        state.guard.lifecycle == W_SEED_OWNER_GUARD_FAILED &&
        state.guard.disposition == W_SEED_OWNER_GUARD_DISPOSITION_NONE);
  const w_seed_owner_guard_view poisoned_view = view;
  CHECK(!w_seed_owner_guard_get_view(&state.guard, &view));
  CHECK(memcmp(&view, &poisoned_view, sizeof(view)) == 0);
  const w_seed_owner_guard_candidate_ref poisoned_candidate = candidate;
  CHECK(!w_seed_owner_guard_get_candidate(&state.guard, 0u, &candidate));
  CHECK(memcmp(&candidate, &poisoned_candidate, sizeof(candidate)) == 0);

  w_seed_owner_guard_destroy(&state.guard);
  CHECK(object_is_zero(&state.guard, sizeof(state.guard)));
  CHECK(state.context.destroy_calls == 1u &&
        state.context.destroy_without_session == 0u &&
        state.context.destroy_generations[0] == first_generation &&
        closed_in_reverse(&state.context));
  w_seed_owner_guard_destroy(&state.guard);
  CHECK(state.context.destroy_calls == 1u);

  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_OK);
  CHECK(state.guard.generation == first_generation + 1u);
  w_seed_owner_guard_destroy(&state.guard);
  CHECK(state.context.destroy_calls == 2u);
  return true;
}

static bool test_absence_lifecycle(void) {
  fixture state;
  fixture_init(&state, false);
  uint8_t published_snapshot[sizeof(state.published)];
  (void)memcpy(published_snapshot, state.published, sizeof(published_snapshot));
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_OK);
  CHECK(memcmp(published_snapshot, state.published,
               sizeof(published_snapshot)) == 0);
  w_seed_owner_guard_view view;
  CHECK(w_seed_owner_guard_get_view(&state.guard, &view));
  CHECK(view.disposition == W_SEED_OWNER_GUARD_NO_CANDIDATE_OBSERVED &&
        view.candidates == state.published && view.candidate_count == 0u);
  CHECK(w_seed_owner_guard_revalidate(&state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_OK);
  CHECK(w_seed_owner_guard_get_view(&state.guard, &view));
  CHECK(view.disposition ==
        W_SEED_OWNER_GUARD_NO_CANDIDATE_RECONFIRMED);
  CHECK(view.lifecycle == W_SEED_OWNER_GUARD_LIVE_RECONFIRMED);
  w_seed_owner_guard_destroy(&state.guard);
  return true;
}

static bool test_begin_capacity_and_malformed_cleanup(void) {
  fixture state;
  fixture_init(&state, true);
  state.input.storage.published_candidate_capacity = 1u;
  uint8_t published_snapshot[sizeof(state.published)];
  (void)memcpy(published_snapshot, state.published, sizeof(published_snapshot));
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_CAPACITY);
  CHECK(state.result.required_candidate_capacity == 2u);
  CHECK(object_is_zero(&state.guard, sizeof(state.guard)));
  CHECK(memcmp(published_snapshot, state.published,
               sizeof(published_snapshot)) == 0);
  CHECK(state.context.abort_begin_calls == 1u &&
        state.context.abort_without_session == 0u &&
        state.context.destroy_calls == 0u && !state.context.session_live &&
        closed_in_reverse(&state.context));

  fixture_init(&state, true);
  uint8_t malformed_snapshot[sizeof(state.published)];
  (void)memcpy(malformed_snapshot, state.published,
               sizeof(malformed_snapshot));
  state.context.begin_corruption = FAKE_CORRUPTION_ZERO_GENERATION;
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_FAULT);
  CHECK(object_is_zero(&state.guard, sizeof(state.guard)));
  CHECK(state.context.abort_begin_calls == 1u &&
        state.context.abort_without_session == 0u &&
        state.context.destroy_calls == 0u && !state.context.session_live &&
        closed_in_reverse(&state.context));
  CHECK(memcmp(malformed_snapshot, state.published,
               sizeof(malformed_snapshot)) == 0);

  fixture_init(&state, true);
  state.context.next_generation = 1u;
  state.context.begin_corruption =
      FAKE_CORRUPTION_STALE_GENERATION_AND_BAD_LEVEL_COUNT;
  (void)memcpy(malformed_snapshot, state.published,
               sizeof(malformed_snapshot));
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_FAULT);
  CHECK(state.result.generation == 0u &&
        object_is_zero(&state.guard, sizeof(state.guard)));
  CHECK(state.context.abort_begin_calls == 1u &&
        state.context.abort_without_session == 0u &&
        state.context.destroy_calls == 0u && !state.context.session_live &&
        state.context.active_generation == 0u &&
        closed_in_reverse(&state.context));
  CHECK(memcmp(malformed_snapshot, state.published,
               sizeof(malformed_snapshot)) == 0);

  const fake_corruption corruptions[] = {
      FAKE_CORRUPTION_BAD_LEVEL_COUNT,
      FAKE_CORRUPTION_BAD_DIRECTORY_ORDINAL,
      FAKE_CORRUPTION_BAD_CANDIDATE_INDEX,
      FAKE_CORRUPTION_BAD_ROOT_TERMINAL,
      FAKE_CORRUPTION_BAD_PHASE,
  };
  for (size_t index = 0u;
       index < sizeof(corruptions) / sizeof(corruptions[0]); index += 1u) {
    fixture_init(&state, true);
    (void)memcpy(malformed_snapshot, state.published,
                 sizeof(malformed_snapshot));
    state.context.begin_corruption = corruptions[index];
    CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
          W_SEED_OWNER_GUARD_FAULT);
    CHECK(object_is_zero(&state.guard, sizeof(state.guard)));
    CHECK(state.context.abort_begin_calls == 1u &&
          state.context.abort_without_session == 0u &&
          state.context.destroy_calls == 0u && !state.context.session_live &&
          closed_in_reverse(&state.context));
    CHECK(memcmp(malformed_snapshot, state.published,
                 sizeof(malformed_snapshot)) == 0);
  }

  fixture_init(&state, true);
  state.context.begin_status = W_SEED_OWNER_GUARD_BACKEND_CAPACITY;
  state.context.begin_failure_phase =
      W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_PARENT;
  state.context.begin_failure_level = TEST_LEVEL_CAPACITY;
  state.context.begin_required_capacity = 9u;
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_CAPACITY);
  CHECK(state.context.abort_begin_calls == 0u &&
        state.context.destroy_calls == 0u && !state.context.session_live);
  CHECK(object_is_zero(&state.guard, sizeof(state.guard)));

  fixture_init(&state, false);
  state.input.max_levels = 2u;
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_CAPACITY);
  CHECK(state.result.level_index == 2u &&
        state.result.required_level_capacity == 3u &&
        state.context.abort_begin_calls == 0u &&
        state.context.destroy_calls == 0u);

  fixture_init(&state, true);
  state.context.begin_status = W_SEED_OWNER_GUARD_BACKEND_IO;
  state.context.begin_failure_phase =
      W_SEED_OWNER_GUARD_BACKEND_PHASE_LOOKUP_CANDIDATE;
  state.context.begin_failure_level = 1u;
  state.context.partial_begin_failure = true;
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_IO);
  CHECK(state.context.abort_begin_calls == 0u &&
        state.context.destroy_calls == 0u &&
        state.context.partial_cleanup_calls == 1u &&
        closed_in_reverse(&state.context));
  return true;
}

static bool test_revalidation_failure_poisoning(void) {
  fixture state;
  fixture_init(&state, true);
  state.context.revalidation_observations[2].candidate_index =
      W_SEED_OWNER_GUARD_NO_CANDIDATE;
  state.context.revalidation_candidate_count = 1u;
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_OK);
  CHECK(w_seed_owner_guard_revalidate(&state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_MUTATED);
  CHECK(state.guard.lifecycle == W_SEED_OWNER_GUARD_FAILED &&
        state.guard.disposition == W_SEED_OWNER_GUARD_DISPOSITION_NONE &&
        state.guard.session_live);
  w_seed_owner_guard_view view;
  (void)memset(&view, 0x3C, sizeof(view));
  const w_seed_owner_guard_view view_snapshot = view;
  CHECK(!w_seed_owner_guard_get_view(&state.guard, &view));
  CHECK(memcmp(&view, &view_snapshot, sizeof(view)) == 0);
  w_seed_owner_guard_destroy(&state.guard);
  CHECK(state.context.destroy_calls == 1u && closed_in_reverse(&state.context));

  fixture_init(&state, false);
  state.context.revalidate_status = W_SEED_OWNER_GUARD_BACKEND_BOUNDARY;
  state.context.revalidate_failure_phase =
      W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_PARENT;
  state.context.revalidate_failure_level = 1u;
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_OK);
  CHECK(w_seed_owner_guard_revalidate(&state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_BOUNDARY);
  CHECK(state.guard.lifecycle == W_SEED_OWNER_GUARD_FAILED &&
        state.guard.session_live);
  w_seed_owner_guard_destroy(&state.guard);
  CHECK(state.context.destroy_calls == 1u);

  fixture_init(&state, true);
  state.context.revalidate_corruption = FAKE_CORRUPTION_BAD_PHASE;
  uint8_t published_snapshot[sizeof(state.published)];
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_OK);
  const uint64_t bad_phase_generation = state.guard.generation;
  (void)memcpy(published_snapshot, state.published,
               sizeof(published_snapshot));
  CHECK(w_seed_owner_guard_revalidate(&state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_FAULT);
  CHECK(state.guard.lifecycle == W_SEED_OWNER_GUARD_FAILED &&
        state.guard.session_live && state.context.destroy_calls == 0u &&
        state.context.session_live &&
        memcmp(published_snapshot, state.published,
               sizeof(published_snapshot)) == 0);
  w_seed_owner_guard_destroy(&state.guard);
  CHECK(state.context.destroy_calls == 1u &&
        state.context.destroy_generations[0] == bad_phase_generation &&
        !state.context.session_live && closed_in_reverse(&state.context));

  fixture_init(&state, true);
  state.context.revalidate_corruption = FAKE_CORRUPTION_ZERO_GENERATION;
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_OK);
  const uint64_t active_generation = state.context.active_generation;
  (void)memcpy(published_snapshot, state.published,
               sizeof(published_snapshot));
  CHECK(w_seed_owner_guard_revalidate(&state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_FAULT);
  CHECK(state.context.destroy_calls == 0u &&
        state.context.stale_destroy_generation == 0u &&
        state.context.session_live && state.guard.session_live &&
        memcmp(published_snapshot, state.published,
               sizeof(published_snapshot)) == 0);
  w_seed_owner_guard_destroy(&state.guard);
  CHECK(state.context.destroy_calls == 1u &&
        state.context.destroy_generations[0] == active_generation &&
        !state.context.session_live && object_is_zero(&state.guard,
                                                      sizeof(state.guard)));
  return true;
}

static bool test_backend_status_mapping(void) {
  static const struct {
    w_seed_owner_guard_backend_status backend;
    w_seed_owner_guard_status expected;
  } cases[] = {
      {W_SEED_OWNER_GUARD_BACKEND_MUTATED, W_SEED_OWNER_GUARD_MUTATED},
      {W_SEED_OWNER_GUARD_BACKEND_BOUNDARY, W_SEED_OWNER_GUARD_BOUNDARY},
      {W_SEED_OWNER_GUARD_BACKEND_REPARSE, W_SEED_OWNER_GUARD_REPARSE},
      {W_SEED_OWNER_GUARD_BACKEND_UNSUPPORTED, W_SEED_OWNER_GUARD_UNSUPPORTED},
      {W_SEED_OWNER_GUARD_BACKEND_IO, W_SEED_OWNER_GUARD_IO},
      {W_SEED_OWNER_GUARD_BACKEND_INVALID, W_SEED_OWNER_GUARD_INVALID},
      {W_SEED_OWNER_GUARD_BACKEND_FAULT, W_SEED_OWNER_GUARD_FAULT},
  };
  for (size_t index = 0u; index < sizeof(cases) / sizeof(cases[0]);
       index += 1u) {
    fixture state;
    fixture_init(&state, false);
    state.context.begin_status = cases[index].backend;
    state.context.begin_failure_phase =
        W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_START;
    CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
          cases[index].expected);
    CHECK(state.result.backend_status == cases[index].backend);
    CHECK(object_is_zero(&state.guard, sizeof(state.guard)));
    CHECK(state.context.destroy_calls == 0u);
  }
  return true;
}

static bool test_impossible_backend_envelopes(void) {
  fixture state;
  fixture_init(&state, false);
  state.context.begin_status = W_SEED_OWNER_GUARD_BACKEND_CAPACITY;
  state.context.begin_failure_phase =
      W_SEED_OWNER_GUARD_BACKEND_PHASE_LOOKUP_CANDIDATE;
  state.context.begin_failure_level = TEST_LEVEL_CAPACITY;
  state.context.begin_required_capacity = TEST_LEVEL_CAPACITY + 1u;
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_FAULT);
  CHECK(state.context.destroy_calls == 0u &&
        object_is_zero(&state.guard, sizeof(state.guard)));

  fixture_init(&state, false);
  state.context.begin_status = W_SEED_OWNER_GUARD_BACKEND_IO;
  state.context.begin_failure_phase =
      W_SEED_OWNER_GUARD_BACKEND_PHASE_VALIDATE;
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_FAULT);

  fixture_init(&state, false);
  state.context.begin_status = (w_seed_owner_guard_backend_status)-1;
  state.context.begin_failure_phase =
      W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_START;
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_FAULT);

  fixture_init(&state, false);
  state.context.begin_status = (w_seed_owner_guard_backend_status)99;
  state.context.begin_failure_phase =
      (w_seed_owner_guard_backend_phase)99;
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_FAULT);

  fixture_init(&state, false);
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_OK);
  state.context.revalidate_status = W_SEED_OWNER_GUARD_BACKEND_CAPACITY;
  state.context.revalidate_failure_phase =
      W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_PARENT;
  state.context.revalidate_failure_level = TEST_LEVEL_CAPACITY;
  state.context.revalidate_required_capacity = TEST_LEVEL_CAPACITY + 1u;
  CHECK(w_seed_owner_guard_revalidate(&state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_FAULT);
  CHECK(state.guard.lifecycle == W_SEED_OWNER_GUARD_FAILED);
  w_seed_owner_guard_destroy(&state.guard);

  fixture_init(&state, false);
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_OK);
  state.context.revalidate_status = W_SEED_OWNER_GUARD_BACKEND_INVALID;
  state.context.revalidate_failure_phase =
      W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_CANDIDATE;
  state.context.revalidate_failure_level = 0u;
  CHECK(w_seed_owner_guard_revalidate(&state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_FAULT);
  CHECK(state.guard.lifecycle == W_SEED_OWNER_GUARD_FAILED);
  w_seed_owner_guard_destroy(&state.guard);
  return true;
}

static bool test_view_preflight_before_record_access(void) {
  fixture state;
  fixture_init(&state, true);
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_OK);
  const w_seed_owner_guard valid_guard = state.guard;
  w_seed_owner_guard_view view;
  (void)memset(&view, 0x6B, sizeof(view));
  const w_seed_owner_guard_view view_snapshot = view;

  state.guard.storage.staged =
      (w_seed_owner_guard_observation *)(void *)&state.guard;
  CHECK(!w_seed_owner_guard_get_view(&state.guard, &view));
  CHECK(memcmp(&view, &view_snapshot, sizeof(view)) == 0);
  state.guard = valid_guard;

  state.guard.storage.published_candidates =
      (w_seed_owner_guard_candidate_ref *)(void *)&state.guard;
  CHECK(!w_seed_owner_guard_get_view(&state.guard, &view));
  CHECK(memcmp(&view, &view_snapshot, sizeof(view)) == 0);
  state.guard = valid_guard;

  state.guard.storage.revalidation =
      (w_seed_owner_guard_observation *)(void *)&view;
  CHECK(!w_seed_owner_guard_get_view(&state.guard, &view));
  CHECK(memcmp(&view, &view_snapshot, sizeof(view)) == 0);
  state.guard = valid_guard;

  state.guard.backend.context = &view;
  state.guard.backend_context_size = sizeof(view);
  CHECK(!w_seed_owner_guard_get_view(&state.guard, &view));
  CHECK(memcmp(&view, &view_snapshot, sizeof(view)) == 0);
  state.guard = valid_guard;

  state.guard.storage.staged =
      (w_seed_owner_guard_observation *)(void *)
          ((uint8_t *)(void *)state.staged + 1u);
  CHECK(!w_seed_owner_guard_get_view(&state.guard, &view));
  CHECK(memcmp(&view, &view_snapshot, sizeof(view)) == 0);
  state.guard = valid_guard;

  const uintptr_t alignment =
      (uintptr_t)_Alignof(w_seed_owner_guard_observation);
  const uintptr_t near_end = UINTPTR_MAX - (UINTPTR_MAX % alignment);
  state.guard.storage.staged =
      (w_seed_owner_guard_observation *)(void *)near_end;
  CHECK(!w_seed_owner_guard_get_view(&state.guard, &view));
  CHECK(memcmp(&view, &view_snapshot, sizeof(view)) == 0);
  state.guard = valid_guard;

  state.guard.lifecycle = (w_seed_owner_guard_lifecycle)99;
  CHECK(!w_seed_owner_guard_get_view(&state.guard, &view));
  CHECK(memcmp(&view, &view_snapshot, sizeof(view)) == 0);
  state.guard = valid_guard;

  state.guard.disposition = (w_seed_owner_guard_disposition)-1;
  CHECK(!w_seed_owner_guard_get_view(&state.guard, &view));
  CHECK(memcmp(&view, &view_snapshot, sizeof(view)) == 0);
  state.guard = valid_guard;

  state.staged[0].directory_ordinal = 1u;
  CHECK(!w_seed_owner_guard_get_view(&state.guard, &view));
  CHECK(memcmp(&view, &view_snapshot, sizeof(view)) == 0);
  state.staged[0].directory_ordinal = 0u;
  CHECK(w_seed_owner_guard_revalidate(&state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_OK);
  state.revalidation[state.guard.directory_count - 1u].root_terminal = false;
  CHECK(!w_seed_owner_guard_get_view(&state.guard, &view));
  CHECK(memcmp(&view, &view_snapshot, sizeof(view)) == 0);
  state.revalidation[state.guard.directory_count - 1u].root_terminal = true;
  CHECK(w_seed_owner_guard_get_view(&state.guard, &view));

  w_seed_owner_guard_candidate_ref candidate;
  (void)memset(&candidate, 0x4D, sizeof(candidate));
  const w_seed_owner_guard_candidate_ref candidate_snapshot = candidate;
  CHECK(!w_seed_owner_guard_get_candidate(
      &state.guard, 0u,
      (w_seed_owner_guard_candidate_ref *)(void *)state.published));
  CHECK(!w_seed_owner_guard_get_candidate(&state.guard,
                                           state.guard.candidate_count,
                                           &candidate));
  CHECK(memcmp(&candidate, &candidate_snapshot, sizeof(candidate)) == 0);
  w_seed_owner_guard_destroy(&state.guard);
  return true;
}

static bool test_aliases_and_adjacency(void) {
  fixture state;
  fixture_init(&state, true);
  w_seed_owner_guard_result result_snapshot;
  (void)memset(&result_snapshot, 0, sizeof(result_snapshot));
  state.result = result_snapshot;
  CHECK(w_seed_owner_guard_begin(
            &state.input, &state.guard,
            (w_seed_owner_guard_result *)(void *)&state.guard) ==
        W_SEED_OWNER_GUARD_INVALID);
  CHECK(object_is_zero(&state.guard, sizeof(state.guard)));
  CHECK(state.context.begin_calls == 0u);

  fixture_init(&state, true);
  state.input.storage.revalidation = state.input.storage.staged;
  const w_seed_owner_guard_result alias_result_snapshot = state.result;
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_INVALID);
  CHECK(memcmp(&state.result, &alias_result_snapshot, sizeof(state.result)) ==
        0);
  CHECK(state.context.begin_calls == 0u);

  fixture_init(&state, true);
  state.input.storage.published_candidates =
      (w_seed_owner_guard_candidate_ref *)(void *)state.staged;
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_INVALID);
  CHECK(state.context.begin_calls == 0u);

  fixture_init(&state, true);
  state.input.backend.context = &state.guard;
  state.input.backend_context_size = sizeof(state.guard);
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_INVALID);
  CHECK(state.context.begin_calls == 0u);

  fixture_init(&state, true);
  state.input.source_path.data = (const uint8_t *)(const void *)state.published;
  state.input.source_path.length = sizeof(state.path);
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_INVALID);
  CHECK(state.context.begin_calls == 0u);

  struct {
    w_seed_owner_guard_observation staged[TEST_LEVEL_CAPACITY];
    w_seed_owner_guard_observation revalidation[TEST_LEVEL_CAPACITY];
    w_seed_owner_guard_candidate_ref published[TEST_LEVEL_CAPACITY];
  } adjacent;
  fixture_init(&state, true);
  CHECK((const uint8_t *)(const void *)adjacent.revalidation ==
        (const uint8_t *)(const void *)adjacent.staged +
            sizeof(adjacent.staged));
  CHECK((const uint8_t *)(const void *)adjacent.published ==
        (const uint8_t *)(const void *)adjacent.revalidation +
            sizeof(adjacent.revalidation));
  state.input.storage.staged = adjacent.staged;
  state.input.storage.revalidation = adjacent.revalidation;
  state.input.storage.published_candidates = adjacent.published;
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_OK);
  CHECK(w_seed_owner_guard_revalidate(&state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_OK);

  w_seed_owner_guard_view view_snapshot;
  (void)memset(&view_snapshot, 0x7E, sizeof(view_snapshot));
  CHECK(!w_seed_owner_guard_get_view(
      &state.guard,
      (w_seed_owner_guard_view *)(void *)state.input.storage.published_candidates));
  CHECK(!w_seed_owner_guard_get_view(
      &state.guard, (w_seed_owner_guard_view *)(void *)&state.guard));
  w_seed_owner_guard_destroy(&state.guard);
  return true;
}

static bool test_preflight_and_revalidate_result_alias(void) {
  fixture state;
  fixture_init(&state, false);
  state.input.max_levels = TEST_LEVEL_CAPACITY;
  state.input.storage.staged_capacity = TEST_LEVEL_CAPACITY - 1u;
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_INVALID);
  CHECK(state.context.begin_calls == 0u);

  fixture_init(&state, false);
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_OK);
  const w_seed_owner_guard guard_snapshot = state.guard;
  CHECK(w_seed_owner_guard_revalidate(
            &state.guard,
            (w_seed_owner_guard_result *)(void *)state.revalidation) ==
        W_SEED_OWNER_GUARD_INVALID);
  CHECK(memcmp(&state.guard, &guard_snapshot, sizeof(state.guard)) == 0);
  CHECK(state.context.revalidate_calls == 0u);
  w_seed_owner_guard_destroy(&state.guard);

  fixture_init(&state, false);
  state.context.next_generation = UINT64_MAX;
  CHECK(w_seed_owner_guard_begin(&state.input, &state.guard, &state.result) ==
        W_SEED_OWNER_GUARD_FAULT);
  CHECK(object_is_zero(&state.guard, sizeof(state.guard)));
  CHECK(state.context.destroy_calls == 0u);
  return true;
}

int main(void) {
  if (!test_candidate_lifecycle() || !test_absence_lifecycle() ||
      !test_begin_capacity_and_malformed_cleanup() ||
      !test_revalidation_failure_poisoning() ||
      !test_backend_status_mapping() ||
      !test_impossible_backend_envelopes() ||
      !test_view_preflight_before_record_access() ||
      !test_aliases_and_adjacency() ||
      !test_preflight_and_revalidate_result_alias())
    return 1;
  (void)puts("w_seed_owner_guard_tests: ok");
  return 0;
}
