#include "check_storage.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
  void *pointer;
  size_t bytes;
  bool allocated;
  bool releasable;
} check_replacement;

typedef struct {
  uintptr_t start;
  uintptr_t end;
} check_storage_range;

enum {
  CHECK_STORAGE_RANGE_COUNT =
      3 + 4 * W_SEED_CHECK_STORAGE_MAX_SOURCES,
};

static bool object_is_zero(const void *object, size_t size) {
  if (object == NULL) return false;
  const unsigned char *bytes = (const unsigned char *)object;
  for (size_t index = 0u; index < size; index += 1u) {
    if (bytes[index] != 0u) return false;
  }
  return true;
}

static bool add_range(check_storage_range *ranges, size_t capacity,
                      size_t *count, const void *pointer, size_t elements,
                      size_t element_size) {
  if (ranges == NULL || count == NULL || element_size == 0u ||
      elements > SIZE_MAX / element_size)
    return false;
  const size_t bytes = elements * element_size;
  if (bytes == 0u) return true;
  if (pointer == NULL || *count >= capacity) return false;
  const uintptr_t start = (uintptr_t)pointer;
  if ((uintmax_t)bytes > (uintmax_t)UINTPTR_MAX - (uintmax_t)start)
    return false;
  ranges[*count] =
      (check_storage_range){start, start + (uintptr_t)bytes};
  *count += 1u;
  return true;
}

static bool ranges_overlap(check_storage_range left,
                           check_storage_range right) {
  return left.start < right.end && right.start < left.end;
}

static bool ranges_disjoint(const check_storage_range *ranges,
                            size_t count) {
  if (ranges == NULL) return false;
  for (size_t left = 0u; left < count; left += 1u) {
    for (size_t right = left + 1u; right < count; right += 1u) {
      if (ranges_overlap(ranges[left], ranges[right])) return false;
    }
  }
  return true;
}

static bool check_storage_ranges(const w_seed_check_storage *storage,
                                 check_storage_range *ranges,
                                 size_t *count) {
  if (storage == NULL || ranges == NULL || count == NULL) return false;
  *count = 0u;
  if (!add_range(ranges, CHECK_STORAGE_RANGE_COUNT, count, storage, 1u,
                 sizeof(*storage)) ||
      !add_range(ranges, CHECK_STORAGE_RANGE_COUNT, count,
                 storage->json_staging, storage->json_staging_capacity,
                 sizeof(uint8_t)) ||
      !add_range(ranges, CHECK_STORAGE_RANGE_COUNT, count,
                 storage->json_final, storage->json_final_capacity,
                 sizeof(uint8_t)))
    return false;
  for (size_t index = 0u;
       index < (size_t)W_SEED_CHECK_STORAGE_MAX_SOURCES; index += 1u) {
    if (!add_range(ranges, CHECK_STORAGE_RANGE_COUNT, count,
                   storage->acquisition.staging_bytes[index],
                   storage->acquisition.staging_capacity[index],
                   sizeof(uint8_t)) ||
        !add_range(ranges, CHECK_STORAGE_RANGE_COUNT, count,
                   storage->acquisition.revalidation_bytes[index],
                   storage->acquisition.revalidation_capacity[index],
                   sizeof(uint8_t)) ||
        !add_range(ranges, CHECK_STORAGE_RANGE_COUNT, count,
                   storage->acquisition.published_bytes[index],
                   storage->acquisition.published_capacity[index],
                   sizeof(uint8_t)) ||
        !add_range(ranges, CHECK_STORAGE_RANGE_COUNT, count,
                   storage->acquisition.nodes[index],
                   storage->acquisition.node_capacity[index],
                   sizeof(w_seed_cst_node)))
      return false;
  }
  return true;
}

/* This is the single parent validator. ACQ0 validates its own child through
 * the non-mutating bind operation. */
static bool check_storage_valid(const w_seed_check_storage *storage) {
  w_seed_ephemeral_provider_request probe = {0};
  if (storage == NULL ||
      !w_seed_acquisition_storage_bind_request(&storage->acquisition, 0u,
                                               &probe) ||
      !((storage->json_staging_capacity == 0u &&
         storage->json_staging == NULL) ||
        (storage->json_staging_capacity != 0u &&
         storage->json_staging != NULL)) ||
      !((storage->json_final_capacity == 0u &&
         storage->json_final == NULL) ||
        (storage->json_final_capacity != 0u &&
         storage->json_final != NULL)) ||
      storage->json_staging_capacity != storage->json_final_capacity ||
      storage->json_staging_capacity >
          (size_t)W_SEED_CHECK_STORAGE_MAX_JSON_BYTES)
    return false;
  check_storage_range ranges[CHECK_STORAGE_RANGE_COUNT];
  size_t count = 0u;
  return check_storage_ranges(storage, ranges, &count) &&
         ranges_disjoint(ranges, count);
}

static w_seed_check_storage_status map_acquisition_status(
    w_seed_acquisition_storage_status status) {
  switch (status) {
    case W_SEED_ACQUISITION_STORAGE_OK:
      return W_SEED_CHECK_STORAGE_OK;
    case W_SEED_ACQUISITION_STORAGE_CAPACITY:
      return W_SEED_CHECK_STORAGE_CAPACITY;
    case W_SEED_ACQUISITION_STORAGE_ALLOCATION:
      return W_SEED_CHECK_STORAGE_ALLOCATION;
    case W_SEED_ACQUISITION_STORAGE_INVALID:
    default:
      return W_SEED_CHECK_STORAGE_INVALID;
  }
}

static bool round_capacity(size_t current, size_t required, size_t limit,
                           size_t *rounded) {
  if (rounded == NULL || current > limit || required > limit) return false;
  if (required <= current) {
    *rounded = current;
    return true;
  }
  size_t candidate = current == 0u ? 1u : current;
  size_t steps = 0u;
  while (candidate < required &&
         steps < W_SEED_CHECK_STORAGE_MAX_GROWTH_STEPS) {
    steps += 1u;
    if (candidate > limit / 2u) {
      candidate = limit;
      break;
    }
    candidate *= 2u;
  }
  if (candidate < required) return false;
  *rounded = candidate;
  return true;
}

static bool prepare_replacement(
    const void *old_pointer, size_t old_size, size_t next_size,
    w_seed_check_storage_allocate_fn allocate, check_replacement *replacement) {
  if (replacement == NULL || allocate == NULL ||
      ((old_size == 0u && old_pointer != NULL) ||
       (old_size != 0u && old_pointer == NULL)))
    return false;
  replacement->pointer = (void *)old_pointer;
  replacement->bytes = old_size;
  replacement->allocated = false;
  replacement->releasable = false;
  if (next_size == old_size) return true;
  replacement->pointer = allocate(next_size);
  if (replacement->pointer == NULL) return false;
  replacement->bytes = next_size;
  replacement->allocated = true;
  return true;
}

static bool validate_replacement(const w_seed_check_storage *storage,
                                 check_replacement *replacement,
                                 const check_replacement *prior) {
  if (storage == NULL || replacement == NULL) return false;
  if (!replacement->allocated) return true;
  check_storage_range candidate[1];
  size_t candidate_count = 0u;
  if (!add_range(candidate, 1u, &candidate_count, replacement->pointer,
                 replacement->bytes, sizeof(uint8_t)) ||
      candidate_count != 1u)
    return false;
  check_storage_range existing[CHECK_STORAGE_RANGE_COUNT];
  size_t existing_count = 0u;
  if (!check_storage_ranges(storage, existing, &existing_count)) return false;
  for (size_t index = 0u; index < existing_count; index += 1u) {
    if (ranges_overlap(candidate[0], existing[index])) return false;
  }
  if (prior != NULL && prior->allocated && prior->releasable) {
    check_storage_range earlier[1];
    size_t earlier_count = 0u;
    if (!add_range(earlier, 1u, &earlier_count, prior->pointer, prior->bytes,
                   sizeof(uint8_t)) ||
        earlier_count != 1u || ranges_overlap(candidate[0], earlier[0]))
      return false;
  }
  replacement->releasable = true;
  return true;
}

static void discard_replacement(
    check_replacement *replacement,
    w_seed_check_storage_deallocate_fn deallocate) {
  if (replacement == NULL) return;
  if (replacement->allocated && replacement->releasable &&
      deallocate != NULL)
    deallocate(replacement->pointer);
  replacement->pointer = NULL;
  replacement->bytes = 0u;
  replacement->allocated = false;
  replacement->releasable = false;
}

bool w_seed_check_storage_init_with_allocator(
    w_seed_check_storage *storage,
    w_seed_check_storage_allocate_fn allocate,
    w_seed_check_storage_deallocate_fn deallocate) {
  if (storage == NULL || allocate == NULL || deallocate == NULL ||
      !object_is_zero(storage, sizeof(*storage)))
    return false;
  return w_seed_acquisition_storage_init_with_allocator(
      &storage->acquisition, allocate, deallocate);
}

bool w_seed_check_storage_init(w_seed_check_storage *storage) {
  return w_seed_check_storage_init_with_allocator(storage, malloc, free);
}

w_seed_check_storage_status w_seed_check_storage_grow(
    w_seed_check_storage *storage, size_t request_index,
    size_t required_capacity) {
  if (!check_storage_valid(storage)) return W_SEED_CHECK_STORAGE_INVALID;
  return map_acquisition_status(w_seed_acquisition_storage_grow_source(
      &storage->acquisition, request_index, required_capacity));
}

w_seed_check_storage_status w_seed_check_storage_grow_nodes(
    w_seed_check_storage *storage, size_t request_index,
    size_t required_capacity) {
  if (!check_storage_valid(storage)) return W_SEED_CHECK_STORAGE_INVALID;
  return map_acquisition_status(w_seed_acquisition_storage_grow_nodes(
      &storage->acquisition, request_index, required_capacity));
}

w_seed_check_storage_status w_seed_check_storage_grow_json(
    w_seed_check_storage *storage, size_t required_capacity) {
  if (!check_storage_valid(storage)) return W_SEED_CHECK_STORAGE_INVALID;
  if (required_capacity > (size_t)W_SEED_CHECK_STORAGE_MAX_JSON_BYTES)
    return W_SEED_CHECK_STORAGE_CAPACITY;
  const size_t old_capacity = storage->json_staging_capacity;
  size_t next_capacity = 0u;
  if (!round_capacity(old_capacity, required_capacity,
                      (size_t)W_SEED_CHECK_STORAGE_MAX_JSON_BYTES,
                      &next_capacity))
    return W_SEED_CHECK_STORAGE_CAPACITY;
  if (next_capacity == old_capacity) return W_SEED_CHECK_STORAGE_OK;

  check_replacement staging = {NULL, 0u, false, false};
  check_replacement final = {NULL, 0u, false, false};
  if (!prepare_replacement(storage->json_staging, old_capacity, next_capacity,
                           storage->acquisition.allocate, &staging) ||
      !validate_replacement(storage, &staging, NULL) ||
      !prepare_replacement(storage->json_final, old_capacity, next_capacity,
                           storage->acquisition.allocate, &final) ||
      !validate_replacement(storage, &final, &staging)) {
    discard_replacement(&staging, storage->acquisition.deallocate);
    discard_replacement(&final, storage->acquisition.deallocate);
    return W_SEED_CHECK_STORAGE_ALLOCATION;
  }
  if (staging.allocated && old_capacity != 0u)
    (void)memcpy(staging.pointer, storage->json_staging, old_capacity);
  if (final.allocated && old_capacity != 0u)
    (void)memcpy(final.pointer, storage->json_final, old_capacity);
  if (staging.allocated && storage->json_staging != NULL)
    storage->acquisition.deallocate(storage->json_staging);
  if (final.allocated && storage->json_final != NULL)
    storage->acquisition.deallocate(storage->json_final);
  storage->json_staging = (uint8_t *)staging.pointer;
  storage->json_final = (uint8_t *)final.pointer;
  storage->json_staging_capacity = next_capacity;
  storage->json_final_capacity = next_capacity;
  return W_SEED_CHECK_STORAGE_OK;
}

bool w_seed_check_storage_bind_request(
    const w_seed_check_storage *storage, size_t request_index,
    w_seed_ephemeral_provider_request *request) {
  return check_storage_valid(storage) &&
         w_seed_acquisition_storage_bind_request(
             &storage->acquisition, request_index, request);
}

bool w_seed_check_storage_bind_slot(const w_seed_check_storage *storage,
                                    size_t request_index,
                                    w_seed_ephemeral_driver_slot *slot) {
  return check_storage_valid(storage) &&
         w_seed_acquisition_storage_bind_slot(
             &storage->acquisition, request_index, slot);
}

void w_seed_check_storage_destroy(w_seed_check_storage *storage) {
  if (storage == NULL) return;
  if (object_is_zero(storage, sizeof(*storage))) return;
  if (!check_storage_valid(storage)) return;
  if (storage->json_staging != NULL)
    storage->acquisition.deallocate(storage->json_staging);
  if (storage->json_final != NULL)
    storage->acquisition.deallocate(storage->json_final);
  w_seed_acquisition_storage_destroy(&storage->acquisition);
  (void)memset(storage, 0, sizeof(*storage));
}
