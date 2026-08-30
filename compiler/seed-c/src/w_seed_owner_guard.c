#include "w_seed_owner_guard.h"

#include <limits.h>
#include <string.h>

typedef struct {
  uintptr_t start;
  size_t size;
} owner_guard_range;

static bool multiply_size(size_t left, size_t right, size_t *result) {
  if (result == NULL) return false;
  if (left != 0u && right > SIZE_MAX / left) return false;
  *result = left * right;
  return true;
}

static bool make_range(const void *pointer, size_t size,
                       owner_guard_range *range) {
  if (range == NULL || (pointer == NULL && size != 0u)) return false;
  if (size == 0u) {
    range->start = (uintptr_t)0u;
    range->size = 0u;
    return pointer == NULL;
  }
  const uintptr_t start = (uintptr_t)pointer;
  if (start == (uintptr_t)0u || size > (size_t)(UINTPTR_MAX - start))
    return false;
  range->start = start;
  range->size = size;
  return true;
}

static bool make_array_range(const void *pointer, size_t count,
                             size_t item_size, owner_guard_range *range) {
  size_t size = 0u;
  return multiply_size(count, item_size, &size) &&
         make_range(pointer, size, range);
}

static bool pointer_aligned(const void *pointer, size_t alignment) {
  return pointer != NULL && alignment != 0u &&
         (uintptr_t)pointer % (uintptr_t)alignment == (uintptr_t)0u;
}

static bool make_typed_array_range(const void *pointer, size_t count,
                                   size_t item_size, size_t alignment,
                                   owner_guard_range *range) {
  if (count != 0u && !pointer_aligned(pointer, alignment)) return false;
  return make_array_range(pointer, count, item_size, range);
}

static bool ranges_overlap(owner_guard_range left, owner_guard_range right) {
  if (left.size == 0u || right.size == 0u) return false;
  return left.start < right.start + right.size &&
         right.start < left.start + left.size;
}

static bool ranges_disjoint(const owner_guard_range *ranges, size_t count) {
  if (ranges == NULL) return false;
  for (size_t left = 0u; left < count; left += 1u) {
    for (size_t right = left + 1u; right < count; right += 1u) {
      if (ranges_overlap(ranges[left], ranges[right])) return false;
    }
  }
  return true;
}

static bool bytes_zero(const void *object, size_t size) {
  if (object == NULL) return false;
  const uint8_t *bytes = (const uint8_t *)object;
  for (size_t index = 0u; index < size; index += 1u) {
    if (bytes[index] != 0u) return false;
  }
  return true;
}

static w_seed_owner_guard_result owner_result(
    w_seed_owner_guard_status status, w_seed_owner_guard_phase phase) {
  w_seed_owner_guard_result result;
  (void)memset(&result, 0, sizeof(result));
  result.status = status;
  result.phase = phase;
  result.backend_status = W_SEED_OWNER_GUARD_BACKEND_INVALID;
  result.backend_phase = W_SEED_OWNER_GUARD_BACKEND_PHASE_NONE;
  result.level_index = W_SEED_OWNER_GUARD_NO_LEVEL;
  return result;
}

static w_seed_owner_guard_status map_backend_status(
    w_seed_owner_guard_backend_status status) {
  switch (status) {
    case W_SEED_OWNER_GUARD_BACKEND_OK:
      return W_SEED_OWNER_GUARD_OK;
    case W_SEED_OWNER_GUARD_BACKEND_CAPACITY:
      return W_SEED_OWNER_GUARD_CAPACITY;
    case W_SEED_OWNER_GUARD_BACKEND_MUTATED:
      return W_SEED_OWNER_GUARD_MUTATED;
    case W_SEED_OWNER_GUARD_BACKEND_BOUNDARY:
      return W_SEED_OWNER_GUARD_BOUNDARY;
    case W_SEED_OWNER_GUARD_BACKEND_REPARSE:
      return W_SEED_OWNER_GUARD_REPARSE;
    case W_SEED_OWNER_GUARD_BACKEND_UNSUPPORTED:
      return W_SEED_OWNER_GUARD_UNSUPPORTED;
    case W_SEED_OWNER_GUARD_BACKEND_IO:
      return W_SEED_OWNER_GUARD_IO;
    case W_SEED_OWNER_GUARD_BACKEND_INVALID:
      return W_SEED_OWNER_GUARD_INVALID;
    case W_SEED_OWNER_GUARD_BACKEND_FAULT:
      return W_SEED_OWNER_GUARD_FAULT;
  }
  return W_SEED_OWNER_GUARD_FAULT;
}

static w_seed_owner_guard_result result_from_backend(
    w_seed_owner_guard_phase phase,
    const w_seed_owner_guard_backend_result *backend) {
  w_seed_owner_guard_result result =
      owner_result(map_backend_status(backend->status), phase);
  result.backend_status = backend->status;
  result.backend_phase = backend->phase;
  result.level_index = backend->level_index;
  result.required_level_capacity = backend->required_level_capacity;
  result.generation = backend->generation;
  return result;
}

static bool backend_shape(const w_seed_owner_guard_backend *backend) {
  return backend != NULL && backend->context != NULL &&
         backend->begin != NULL && backend->revalidate != NULL &&
         backend->abort_begin != NULL && backend->destroy != NULL;
}

static bool backend_status_valid(w_seed_owner_guard_backend_status status) {
  return (int)status >= (int)W_SEED_OWNER_GUARD_BACKEND_OK &&
         (int)status <= (int)W_SEED_OWNER_GUARD_BACKEND_FAULT;
}

static bool backend_phase_valid(w_seed_owner_guard_backend_phase phase) {
  return (int)phase >= (int)W_SEED_OWNER_GUARD_BACKEND_PHASE_NONE &&
         (int)phase <= (int)W_SEED_OWNER_GUARD_BACKEND_PHASE_COMMIT;
}

static bool lifecycle_valid(w_seed_owner_guard_lifecycle lifecycle) {
  return (int)lifecycle >= (int)W_SEED_OWNER_GUARD_ZERO &&
         (int)lifecycle <= (int)W_SEED_OWNER_GUARD_FAILED;
}

static bool disposition_valid(w_seed_owner_guard_disposition disposition) {
  return (int)disposition >= (int)W_SEED_OWNER_GUARD_DISPOSITION_NONE &&
         (int)disposition <=
             (int)W_SEED_OWNER_GUARD_NO_CANDIDATE_RECONFIRMED;
}

static bool source_path_valid(w_seed_byte_view path) {
  if (path.data == NULL || path.length == 0u ||
      path.length > (size_t)W_SEED_OWNER_GUARD_MAX_PATH_BYTES)
    return false;
  for (size_t index = 0u; index < path.length; index += 1u) {
    if (path.data[index] == 0u) return false;
  }
  return true;
}

static bool storage_shape(const w_seed_owner_guard_storage *storage,
                          size_t max_levels) {
  if (storage == NULL || max_levels == 0u ||
      max_levels > (size_t)W_SEED_OWNER_GUARD_MAX_LEVELS)
    return false;
  if (storage->staged == NULL || storage->revalidation == NULL ||
      !pointer_aligned(storage->staged,
                       (size_t)_Alignof(w_seed_owner_guard_observation)) ||
      !pointer_aligned(storage->revalidation,
                       (size_t)_Alignof(w_seed_owner_guard_observation)) ||
      storage->staged_capacity < max_levels ||
      storage->revalidation_capacity < max_levels ||
      storage->staged_capacity > (size_t)W_SEED_OWNER_GUARD_MAX_LEVELS ||
      storage->revalidation_capacity >
          (size_t)W_SEED_OWNER_GUARD_MAX_LEVELS ||
      storage->published_candidate_capacity >
          (size_t)W_SEED_OWNER_GUARD_MAX_LEVELS)
    return false;
  if ((storage->published_candidate_capacity == 0u) !=
      (storage->published_candidates == NULL))
    return false;
  if (storage->published_candidate_capacity != 0u &&
      !pointer_aligned(
          storage->published_candidates,
          (size_t)_Alignof(w_seed_owner_guard_candidate_ref)))
    return false;
  return true;
}

static bool begin_ranges(const w_seed_owner_guard_input *input,
                         const w_seed_owner_guard *guard,
                         const w_seed_owner_guard_result *result) {
  owner_guard_range ranges[8];
  size_t count = 0u;
  if (!make_range(input, sizeof(*input), &ranges[count++]) ||
      !make_range(guard, sizeof(*guard), &ranges[count++]) ||
      !make_range(result, sizeof(*result), &ranges[count++]) ||
      !make_range(input->backend.context, input->backend_context_size,
                  &ranges[count++]) ||
      !make_range(input->source_path.data, input->source_path.length,
                  &ranges[count++]) ||
      !make_typed_array_range(
          input->storage.staged, input->storage.staged_capacity,
          sizeof(*input->storage.staged),
          (size_t)_Alignof(w_seed_owner_guard_observation),
          &ranges[count++]) ||
      !make_typed_array_range(
          input->storage.revalidation,
          input->storage.revalidation_capacity,
          sizeof(*input->storage.revalidation),
          (size_t)_Alignof(w_seed_owner_guard_observation),
          &ranges[count++]) ||
      !make_typed_array_range(
          input->storage.published_candidates,
          input->storage.published_candidate_capacity,
          sizeof(*input->storage.published_candidates),
          (size_t)_Alignof(w_seed_owner_guard_candidate_ref),
          &ranges[count++]))
    return false;
  return ranges_disjoint(ranges, count);
}

static bool observation_shape_valid(
    const w_seed_owner_guard_observation *observations, size_t level_count,
    size_t candidate_count) {
  if (observations == NULL || level_count == 0u ||
      candidate_count > level_count)
    return false;
  size_t candidate_cursor = 0u;
  for (size_t index = 0u; index < level_count; index += 1u) {
    const w_seed_owner_guard_observation *observation =
        &observations[index];
    if (observation->directory_ordinal != index ||
        observation->root_terminal != (index + 1u == level_count))
      return false;
    if (observation->candidate_index == W_SEED_OWNER_GUARD_NO_CANDIDATE)
      continue;
    if (observation->candidate_index != candidate_cursor) return false;
    candidate_cursor += 1u;
  }
  return candidate_cursor == candidate_count;
}

static bool observations_valid(
    const w_seed_owner_guard_observation *observations, size_t capacity,
    const w_seed_owner_guard_backend_result *backend, size_t max_levels) {
  if (observations == NULL || backend == NULL ||
      !backend_status_valid(backend->status) ||
      !backend_phase_valid(backend->phase) ||
      backend->status != W_SEED_OWNER_GUARD_BACKEND_OK ||
      backend->phase != W_SEED_OWNER_GUARD_BACKEND_PHASE_COMMIT ||
      backend->generation == 0u || backend->level_count == 0u ||
      backend->level_count > capacity || backend->level_count > max_levels ||
      backend->candidate_count > backend->level_count ||
      backend->required_level_capacity != 0u ||
      backend->level_index != backend->level_count - 1u)
    return false;
  return observation_shape_valid(observations, backend->level_count,
                                 backend->candidate_count);
}

static bool begin_failure_phase_allowed(
    w_seed_owner_guard_backend_status status,
    w_seed_owner_guard_backend_phase phase) {
  if (status == W_SEED_OWNER_GUARD_BACKEND_CAPACITY)
    return phase == W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_PARENT;
  if (status == W_SEED_OWNER_GUARD_BACKEND_INVALID)
    return phase == W_SEED_OWNER_GUARD_BACKEND_PHASE_VALIDATE ||
           phase == W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_START;
  if (status == W_SEED_OWNER_GUARD_BACKEND_FAULT &&
      phase == W_SEED_OWNER_GUARD_BACKEND_PHASE_VALIDATE)
    return true;
  return phase == W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_START ||
         phase == W_SEED_OWNER_GUARD_BACKEND_PHASE_LOOKUP_CANDIDATE ||
         phase == W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_PARENT;
}

static bool revalidation_failure_phase_allowed(
    w_seed_owner_guard_backend_status status,
    w_seed_owner_guard_backend_phase phase) {
  if (status == W_SEED_OWNER_GUARD_BACKEND_CAPACITY) return false;
  if (status == W_SEED_OWNER_GUARD_BACKEND_INVALID)
    return phase == W_SEED_OWNER_GUARD_BACKEND_PHASE_VALIDATE;
  if (status == W_SEED_OWNER_GUARD_BACKEND_FAULT &&
      phase == W_SEED_OWNER_GUARD_BACKEND_PHASE_VALIDATE)
    return true;
  return phase ==
             W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_DIRECTORY ||
         phase ==
             W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_CANDIDATE ||
         phase == W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_PARENT;
}

static bool backend_failure_valid(
    const w_seed_owner_guard_backend_result *backend, bool revalidation,
    uint64_t generation, size_t observation_capacity) {
  if (backend == NULL || !backend_status_valid(backend->status) ||
      !backend_phase_valid(backend->phase) ||
      backend->status == W_SEED_OWNER_GUARD_BACKEND_OK ||
      backend->level_count != 0u || backend->candidate_count != 0u ||
      backend->generation != (revalidation ? generation : 0u))
    return false;
  if (!(revalidation
            ? revalidation_failure_phase_allowed(backend->status,
                                                 backend->phase)
            : begin_failure_phase_allowed(backend->status,
                                          backend->phase)))
    return false;
  if (backend->status == W_SEED_OWNER_GUARD_BACKEND_CAPACITY) {
    return !revalidation && observation_capacity != SIZE_MAX &&
           backend->level_index == observation_capacity &&
           backend->required_level_capacity == observation_capacity + 1u;
  }
  if (backend->level_index != W_SEED_OWNER_GUARD_NO_LEVEL &&
      backend->level_index >= observation_capacity)
    return false;
  return backend->required_level_capacity == 0u;
}

static bool observations_equal(
    const w_seed_owner_guard_observation *left,
    const w_seed_owner_guard_observation *right, size_t count) {
  if (left == NULL || right == NULL) return false;
  for (size_t index = 0u; index < count; index += 1u) {
    if (left[index].directory_ordinal != right[index].directory_ordinal ||
        left[index].candidate_index != right[index].candidate_index ||
        left[index].root_terminal != right[index].root_terminal)
      return false;
  }
  return true;
}

static bool candidate_refs_valid(const w_seed_owner_guard *guard) {
  if (guard == NULL || guard->candidate_count > guard->directory_count ||
      guard->candidate_count > guard->storage.published_candidate_capacity)
    return false;
  if (guard->candidate_count != 0u &&
      guard->storage.published_candidates == NULL)
    return false;
  size_t candidate_cursor = 0u;
  for (size_t directory = 0u; directory < guard->directory_count;
       directory += 1u) {
    const size_t index = guard->storage.staged[directory].candidate_index;
    if (index == W_SEED_OWNER_GUARD_NO_CANDIDATE) continue;
    if (index != candidate_cursor || candidate_cursor >= guard->candidate_count)
      return false;
    const w_seed_owner_guard_candidate_ref *candidate =
        &guard->storage.published_candidates[candidate_cursor];
    if (candidate->generation != guard->generation ||
        candidate->directory_ordinal != directory ||
        candidate->candidate_index != candidate_cursor)
      return false;
    candidate_cursor += 1u;
  }
  return candidate_cursor == guard->candidate_count;
}

static bool shallow_live_guard_valid(const w_seed_owner_guard *guard) {
  if (guard == NULL || guard->owner != guard || !guard->session_live ||
      guard->generation == 0u || !backend_shape(&guard->backend) ||
      guard->backend_context_size == 0u || guard->max_levels == 0u ||
      guard->max_levels > (size_t)W_SEED_OWNER_GUARD_MAX_LEVELS ||
      guard->directory_count == 0u ||
      guard->directory_count > guard->max_levels ||
      !lifecycle_valid(guard->lifecycle) ||
      !disposition_valid(guard->disposition) ||
      !storage_shape(&guard->storage, guard->max_levels))
    return false;
  if (guard->lifecycle == W_SEED_OWNER_GUARD_LIVE_OBSERVED) {
    return guard->disposition ==
               (guard->candidate_count == 0u
                    ? W_SEED_OWNER_GUARD_NO_CANDIDATE_OBSERVED
                    : W_SEED_OWNER_GUARD_CANDIDATES_OBSERVED);
  }
  if (guard->lifecycle == W_SEED_OWNER_GUARD_LIVE_RECONFIRMED) {
    return guard->disposition ==
               (guard->candidate_count == 0u
                    ? W_SEED_OWNER_GUARD_NO_CANDIDATE_RECONFIRMED
                    : W_SEED_OWNER_GUARD_CANDIDATES_RECONFIRMED);
  }
  return false;
}

static bool live_guard_valid(const w_seed_owner_guard *guard) {
  if (!shallow_live_guard_valid(guard) ||
      !observation_shape_valid(guard->storage.staged,
                               guard->directory_count,
                               guard->candidate_count) ||
      !candidate_refs_valid(guard))
    return false;
  if (guard->lifecycle == W_SEED_OWNER_GUARD_LIVE_RECONFIRMED) {
    return observation_shape_valid(guard->storage.revalidation,
                                   guard->directory_count,
                                   guard->candidate_count) &&
           observations_equal(guard->storage.staged,
                              guard->storage.revalidation,
                              guard->directory_count);
  }
  return true;
}

static bool guard_output_ranges(const w_seed_owner_guard *guard,
                                const void *output, size_t output_size,
                                size_t output_alignment) {
  owner_guard_range ranges[6];
  size_t count = 0u;
  if (!pointer_aligned(output, output_alignment)) return false;
  if (!make_range(guard, sizeof(*guard), &ranges[count++]) ||
      !make_range(output, output_size, &ranges[count++]) ||
      !make_range(guard->backend.context, guard->backend_context_size,
                  &ranges[count++]) ||
      !make_typed_array_range(
          guard->storage.staged, guard->storage.staged_capacity,
          sizeof(*guard->storage.staged),
          (size_t)_Alignof(w_seed_owner_guard_observation),
          &ranges[count++]) ||
      !make_typed_array_range(
          guard->storage.revalidation,
          guard->storage.revalidation_capacity,
          sizeof(*guard->storage.revalidation),
          (size_t)_Alignof(w_seed_owner_guard_observation),
          &ranges[count++]) ||
      !make_typed_array_range(
          guard->storage.published_candidates,
          guard->storage.published_candidate_capacity,
          sizeof(*guard->storage.published_candidates),
          (size_t)_Alignof(w_seed_owner_guard_candidate_ref),
          &ranges[count++]))
    return false;
  return ranges_disjoint(ranges, count);
}

w_seed_owner_guard_status w_seed_owner_guard_begin(
    const w_seed_owner_guard_input *input, w_seed_owner_guard *guard,
    w_seed_owner_guard_result *result) {
  if (input == NULL || guard == NULL || result == NULL)
    return W_SEED_OWNER_GUARD_INVALID;
  if (!pointer_aligned(input, (size_t)_Alignof(w_seed_owner_guard_input)) ||
      !pointer_aligned(guard, (size_t)_Alignof(w_seed_owner_guard)) ||
      !pointer_aligned(result, (size_t)_Alignof(w_seed_owner_guard_result)))
    return W_SEED_OWNER_GUARD_INVALID;
  owner_guard_range basic[3];
  if (!make_range(input, sizeof(*input), &basic[0]) ||
      !make_range(guard, sizeof(*guard), &basic[1]) ||
      !make_range(result, sizeof(*result), &basic[2]) ||
      !ranges_disjoint(basic, 3u))
    return W_SEED_OWNER_GUARD_INVALID;

  w_seed_owner_guard_result local =
      owner_result(W_SEED_OWNER_GUARD_INVALID,
                   W_SEED_OWNER_GUARD_PHASE_VALIDATE);
  if (!begin_ranges(input, guard, result))
    return W_SEED_OWNER_GUARD_INVALID;
  if (!bytes_zero(guard, sizeof(*guard)) ||
      !source_path_valid(input->source_path) ||
      !storage_shape(&input->storage, input->max_levels) ||
      !backend_shape(&input->backend) || input->backend_context_size == 0u) {
    *result = local;
    return local.status;
  }

  uint8_t path_copy[W_SEED_OWNER_GUARD_MAX_PATH_BYTES];
  (void)memcpy(path_copy, input->source_path.data, input->source_path.length);
  const w_seed_byte_view copied_path = {path_copy, input->source_path.length};
  const w_seed_owner_guard_backend_result backend = input->backend.begin(
      input->backend.context, copied_path, input->storage.staged,
      input->max_levels);
  local = result_from_backend(W_SEED_OWNER_GUARD_PHASE_BEGIN, &backend);
  if (backend.status != W_SEED_OWNER_GUARD_BACKEND_OK) {
    if (!backend_failure_valid(&backend, false, 0u,
                               input->max_levels)) {
      local = owner_result(W_SEED_OWNER_GUARD_FAULT,
                           W_SEED_OWNER_GUARD_PHASE_BEGIN);
      local.backend_status = backend.status;
      local.backend_phase = backend.phase;
    }
    *result = local;
    return local.status;
  }
  if (!observations_valid(input->storage.staged,
                          input->max_levels, &backend,
                          input->max_levels)) {
    input->backend.abort_begin(input->backend.context);
    local = owner_result(W_SEED_OWNER_GUARD_FAULT,
                         W_SEED_OWNER_GUARD_PHASE_BEGIN);
    local.backend_status = backend.status;
    local.backend_phase = backend.phase;
    *result = local;
    return local.status;
  }
  if (backend.candidate_count >
      input->storage.published_candidate_capacity) {
    input->backend.abort_begin(input->backend.context);
    local = owner_result(W_SEED_OWNER_GUARD_CAPACITY,
                         W_SEED_OWNER_GUARD_PHASE_COMMIT);
    local.backend_status = backend.status;
    local.backend_phase = backend.phase;
    local.level_index = backend.level_index;
    local.required_candidate_capacity = backend.candidate_count;
    local.generation = backend.generation;
    *result = local;
    return local.status;
  }

  w_seed_owner_guard_candidate_ref
      candidates[W_SEED_OWNER_GUARD_MAX_LEVELS];
  size_t candidate_cursor = 0u;
  for (size_t directory = 0u; directory < backend.level_count;
       directory += 1u) {
    if (input->storage.staged[directory].candidate_index ==
        W_SEED_OWNER_GUARD_NO_CANDIDATE)
      continue;
    candidates[candidate_cursor].generation = backend.generation;
    candidates[candidate_cursor].directory_ordinal = directory;
    candidates[candidate_cursor].candidate_index = candidate_cursor;
    candidate_cursor += 1u;
  }

  w_seed_owner_guard committed;
  (void)memset(&committed, 0, sizeof(committed));
  committed.owner = guard;
  committed.backend = input->backend;
  committed.storage = input->storage;
  committed.generation = backend.generation;
  committed.directory_count = backend.level_count;
  committed.candidate_count = backend.candidate_count;
  committed.max_levels = input->max_levels;
  committed.backend_context_size = input->backend_context_size;
  committed.lifecycle = W_SEED_OWNER_GUARD_LIVE_OBSERVED;
  committed.disposition =
      backend.candidate_count == 0u
          ? W_SEED_OWNER_GUARD_NO_CANDIDATE_OBSERVED
          : W_SEED_OWNER_GUARD_CANDIDATES_OBSERVED;
  committed.session_live = true;

  local = owner_result(W_SEED_OWNER_GUARD_OK,
                       W_SEED_OWNER_GUARD_PHASE_COMMIT);
  local.backend_status = backend.status;
  local.backend_phase = backend.phase;
  local.level_index = backend.level_index;
  local.generation = backend.generation;
  if (candidate_cursor != 0u)
    (void)memcpy(input->storage.published_candidates, candidates,
                 candidate_cursor * sizeof(candidates[0]));
  *guard = committed;
  *result = local;
  return W_SEED_OWNER_GUARD_OK;
}

bool w_seed_owner_guard_get_view(const w_seed_owner_guard *guard,
                                 w_seed_owner_guard_view *view) {
  if (guard == NULL || view == NULL ||
      !pointer_aligned(guard, (size_t)_Alignof(w_seed_owner_guard)) ||
      !pointer_aligned(view, (size_t)_Alignof(w_seed_owner_guard_view)) ||
      !shallow_live_guard_valid(guard) ||
      !guard_output_ranges(guard, view, sizeof(*view),
                           (size_t)_Alignof(w_seed_owner_guard_view)) ||
      !live_guard_valid(guard))
    return false;
  const w_seed_owner_guard_view local = {
      guard->lifecycle,
      guard->disposition,
      guard->generation,
      guard->directory_count,
      guard->storage.published_candidates,
      guard->candidate_count,
      true,
  };
  *view = local;
  return true;
}

bool w_seed_owner_guard_get_candidate(
    const w_seed_owner_guard *guard, size_t candidate_index,
    w_seed_owner_guard_candidate_ref *candidate) {
  if (guard == NULL || candidate == NULL ||
      !pointer_aligned(guard, (size_t)_Alignof(w_seed_owner_guard)) ||
      !pointer_aligned(
          candidate, (size_t)_Alignof(w_seed_owner_guard_candidate_ref)) ||
      !shallow_live_guard_valid(guard) ||
      !guard_output_ranges(
          guard, candidate, sizeof(*candidate),
          (size_t)_Alignof(w_seed_owner_guard_candidate_ref)) ||
      !live_guard_valid(guard) || candidate_index >= guard->candidate_count)
    return false;
  *candidate = guard->storage.published_candidates[candidate_index];
  return true;
}

w_seed_owner_guard_status w_seed_owner_guard_revalidate(
    w_seed_owner_guard *guard, w_seed_owner_guard_result *result) {
  if (guard == NULL || result == NULL) return W_SEED_OWNER_GUARD_INVALID;
  if (!pointer_aligned(guard, (size_t)_Alignof(w_seed_owner_guard)) ||
      !pointer_aligned(result, (size_t)_Alignof(w_seed_owner_guard_result)))
    return W_SEED_OWNER_GUARD_INVALID;
  owner_guard_range basic[2];
  if (!make_range(guard, sizeof(*guard), &basic[0]) ||
      !make_range(result, sizeof(*result), &basic[1]) ||
      !ranges_disjoint(basic, 2u))
    return W_SEED_OWNER_GUARD_INVALID;
  w_seed_owner_guard_result local =
      owner_result(W_SEED_OWNER_GUARD_INVALID,
                   W_SEED_OWNER_GUARD_PHASE_VALIDATE);
  if (!shallow_live_guard_valid(guard) ||
      !guard_output_ranges(guard, result, sizeof(*result),
                           (size_t)_Alignof(w_seed_owner_guard_result)))
    return W_SEED_OWNER_GUARD_INVALID;
  if (!live_guard_valid(guard)) {
    *result = local;
    return local.status;
  }
  if (guard->lifecycle != W_SEED_OWNER_GUARD_LIVE_OBSERVED) {
    guard->lifecycle = W_SEED_OWNER_GUARD_FAILED;
    guard->disposition = W_SEED_OWNER_GUARD_DISPOSITION_NONE;
    *result = local;
    return local.status;
  }

  const w_seed_owner_guard_backend_result backend = guard->backend.revalidate(
      guard->backend.context, guard->generation,
      guard->storage.revalidation, guard->max_levels);
  local = result_from_backend(W_SEED_OWNER_GUARD_PHASE_REVALIDATE, &backend);
  if (backend.status != W_SEED_OWNER_GUARD_BACKEND_OK) {
    if (!backend_failure_valid(&backend, true, guard->generation,
                               guard->max_levels)) {
      local = owner_result(W_SEED_OWNER_GUARD_FAULT,
                           W_SEED_OWNER_GUARD_PHASE_REVALIDATE);
      local.backend_status = backend.status;
      local.backend_phase = backend.phase;
    }
    guard->lifecycle = W_SEED_OWNER_GUARD_FAILED;
    guard->disposition = W_SEED_OWNER_GUARD_DISPOSITION_NONE;
    *result = local;
    return local.status;
  }
  if (!observations_valid(guard->storage.revalidation,
                          guard->max_levels, &backend,
                          guard->max_levels) ||
      backend.generation != guard->generation) {
    guard->lifecycle = W_SEED_OWNER_GUARD_FAILED;
    guard->disposition = W_SEED_OWNER_GUARD_DISPOSITION_NONE;
    local = owner_result(W_SEED_OWNER_GUARD_FAULT,
                         W_SEED_OWNER_GUARD_PHASE_REVALIDATE);
    local.backend_status = backend.status;
    local.backend_phase = backend.phase;
    *result = local;
    return local.status;
  }
  if (backend.level_count != guard->directory_count ||
      backend.candidate_count != guard->candidate_count ||
      !observations_equal(guard->storage.staged,
                          guard->storage.revalidation,
                          guard->directory_count)) {
    guard->lifecycle = W_SEED_OWNER_GUARD_FAILED;
    guard->disposition = W_SEED_OWNER_GUARD_DISPOSITION_NONE;
    local.status = W_SEED_OWNER_GUARD_MUTATED;
    *result = local;
    return local.status;
  }

  guard->lifecycle = W_SEED_OWNER_GUARD_LIVE_RECONFIRMED;
  guard->disposition =
      guard->candidate_count == 0u
          ? W_SEED_OWNER_GUARD_NO_CANDIDATE_RECONFIRMED
          : W_SEED_OWNER_GUARD_CANDIDATES_RECONFIRMED;
  local = owner_result(W_SEED_OWNER_GUARD_OK,
                       W_SEED_OWNER_GUARD_PHASE_COMMIT);
  local.backend_status = backend.status;
  local.backend_phase = backend.phase;
  local.level_index = backend.level_index;
  local.generation = backend.generation;
  *result = local;
  return local.status;
}

void w_seed_owner_guard_destroy(w_seed_owner_guard *guard) {
  if (guard == NULL || guard->owner == NULL) return;
  if (guard->owner != guard) return;
  if (guard->session_live && guard->backend.destroy != NULL &&
      guard->backend.context != NULL)
    guard->backend.destroy(guard->backend.context, guard->generation);
  (void)memset(guard, 0, sizeof(*guard));
}
