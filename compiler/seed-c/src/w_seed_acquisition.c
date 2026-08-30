#include "w_seed_acquisition.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
  void *pointer;
  size_t bytes;
  bool allocated;
  bool releasable;
} acquisition_replacement;

enum {
  ACQUISITION_STORAGE_RANGE_COUNT =
      1 + 4 * W_SEED_ACQUISITION_MAX_SOURCES,
  ACQUISITION_BIND_RANGE_COUNT = ACQUISITION_STORAGE_RANGE_COUNT + 3 +
                                 10 * W_SEED_ACQUISITION_MAX_SOURCES,
  ACQUISITION_PIPELINE_RANGE_COUNT = 2048,
};

_Static_assert(
    ACQUISITION_PIPELINE_RANGE_COUNT >=
        33 + 14 * W_SEED_ACQUISITION_MAX_SOURCES,
    "pipeline range table must cover every bounded backing range");

typedef struct {
  uintptr_t start;
  uintptr_t end;
  bool present;
} acquisition_range;

static bool object_is_zero(const void *object, size_t size) {
  if (object == NULL) return false;
  const unsigned char *bytes = (const unsigned char *)object;
  for (size_t index = 0u; index < size; index += 1u) {
    if (bytes[index] != 0u) return false;
  }
  return true;
}

static bool index_valid(size_t index) {
  return index < (size_t)W_SEED_ACQUISITION_MAX_SOURCES;
}

static bool pointer_capacity_valid(const void *pointer, size_t capacity) {
  return (capacity == 0u && pointer == NULL) ||
         (capacity != 0u && pointer != NULL);
}

static bool add_bounded(size_t left, size_t right, size_t maximum,
                        size_t *sum) {
  if (sum == NULL || left > maximum || right > maximum - left) return false;
  *sum = left + right;
  return true;
}

static bool node_bytes(size_t count, size_t *bytes) {
  if (bytes == NULL || count > SIZE_MAX / sizeof(w_seed_cst_node))
    return false;
  *bytes = count * sizeof(w_seed_cst_node);
  return true;
}

static bool make_range(const void *pointer, size_t bytes,
                       acquisition_range *range) {
  if (range == NULL) return false;
  *range = (acquisition_range){0u, 0u, false};
  if (bytes == 0u) return true;
  if (pointer == NULL) return false;
  const uintptr_t start = (uintptr_t)pointer;
  if ((uintmax_t)bytes > (uintmax_t)UINTPTR_MAX - (uintmax_t)start)
    return false;
  range->start = start;
  range->end = start + (uintptr_t)bytes;
  range->present = true;
  return true;
}

static bool make_array_range(const void *pointer, size_t count,
                             size_t element_size,
                             acquisition_range *range) {
  if (element_size != 0u && count > SIZE_MAX / element_size) return false;
  return make_range(pointer, count * element_size, range);
}

static bool ranges_overlap(acquisition_range left, acquisition_range right) {
  return left.present && right.present && left.start < right.end &&
         right.start < left.end;
}

static bool add_range(acquisition_range *ranges, size_t capacity,
                      size_t *count, const void *pointer, size_t elements,
                      size_t element_size) {
  if (ranges == NULL || count == NULL || *count >= capacity) return false;
  acquisition_range range;
  if (!make_array_range(pointer, elements, element_size, &range)) return false;
  if (!range.present) return true;
  ranges[*count] = range;
  *count += 1u;
  return true;
}

static bool add_aligned_range(acquisition_range *ranges, size_t capacity,
                              size_t *count, const void *pointer,
                              size_t elements, size_t element_size,
                              size_t alignment) {
  if (elements != 0u && alignment > 1u && pointer != NULL &&
      (uintptr_t)pointer % (uintptr_t)alignment != 0u)
    return false;
  return add_range(ranges, capacity, count, pointer, elements, element_size);
}

static bool ranges_disjoint(const acquisition_range *ranges, size_t count) {
  if (ranges == NULL) return false;
  for (size_t left = 0u; left < count; left += 1u) {
    for (size_t right = left + 1u; right < count; right += 1u) {
      if (ranges_overlap(ranges[left], ranges[right])) return false;
    }
  }
  return true;
}

static bool storage_ranges(const w_seed_acquisition_storage *storage,
                           acquisition_range *ranges, size_t capacity,
                           size_t *count) {
  if (storage == NULL || ranges == NULL || count == NULL) return false;
  *count = 0u;
  if (!add_range(ranges, capacity, count, storage, 1u, sizeof(*storage)))
    return false;
  for (size_t index = 0u;
       index < (size_t)W_SEED_ACQUISITION_MAX_SOURCES; index += 1u) {
    if (!add_range(ranges, capacity, count, storage->staging_bytes[index],
                   storage->staging_capacity[index], sizeof(uint8_t)) ||
        !add_range(ranges, capacity, count,
                   storage->revalidation_bytes[index],
                   storage->revalidation_capacity[index], sizeof(uint8_t)) ||
        !add_range(ranges, capacity, count, storage->published_bytes[index],
                   storage->published_capacity[index], sizeof(uint8_t)) ||
        !add_range(ranges, capacity, count, storage->nodes[index],
                   storage->node_capacity[index], sizeof(w_seed_cst_node)))
      return false;
  }
  return true;
}

static bool storage_valid(const w_seed_acquisition_storage *storage) {
  size_t staging_total = 0u;
  size_t revalidation_total = 0u;
  size_t published_total = 0u;
  size_t node_total = 0u;
  if (storage == NULL || !storage->initialized || storage->owner != storage ||
      storage->allocate == NULL || storage->deallocate == NULL)
    return false;
  for (size_t index = 0u;
       index < (size_t)W_SEED_ACQUISITION_MAX_SOURCES; index += 1u) {
    if (!pointer_capacity_valid(storage->staging_bytes[index],
                                storage->staging_capacity[index]) ||
        !pointer_capacity_valid(storage->revalidation_bytes[index],
                                storage->revalidation_capacity[index]) ||
        !pointer_capacity_valid(storage->published_bytes[index],
                                storage->published_capacity[index]) ||
        !pointer_capacity_valid(storage->nodes[index],
                                storage->node_capacity[index]) ||
        storage->staging_capacity[index] >
            (size_t)W_SEED_ACQUISITION_MAX_SOURCE_BYTES ||
        storage->revalidation_capacity[index] >
            (size_t)W_SEED_ACQUISITION_MAX_SOURCE_BYTES ||
        storage->published_capacity[index] >
            (size_t)W_SEED_ACQUISITION_MAX_SOURCE_BYTES ||
        storage->node_capacity[index] >
            (size_t)W_SEED_ACQUISITION_MAX_NODES ||
        (storage->nodes[index] != NULL &&
         (uintptr_t)storage->nodes[index] % _Alignof(w_seed_cst_node) != 0u))
      return false;
    if (!add_bounded(staging_total, storage->staging_capacity[index],
                     (size_t)W_SEED_ACQUISITION_MAX_TOTAL_SOURCE_BYTES,
                     &staging_total) ||
        !add_bounded(revalidation_total,
                     storage->revalidation_capacity[index],
                     (size_t)W_SEED_ACQUISITION_MAX_TOTAL_SOURCE_BYTES,
                     &revalidation_total) ||
        !add_bounded(published_total, storage->published_capacity[index],
                     (size_t)W_SEED_ACQUISITION_MAX_TOTAL_SOURCE_BYTES,
                     &published_total) ||
        !add_bounded(node_total, storage->node_capacity[index],
                     (size_t)W_SEED_ACQUISITION_MAX_TOTAL_NODES,
                     &node_total))
      return false;
  }
  if (staging_total != storage->staging_total ||
      revalidation_total != storage->revalidation_total ||
      published_total != storage->published_total ||
      node_total != storage->node_total)
    return false;
  acquisition_range ranges[ACQUISITION_STORAGE_RANGE_COUNT];
  size_t range_count = 0u;
  return storage_ranges(storage, ranges, ACQUISITION_STORAGE_RANGE_COUNT,
                        &range_count) &&
         ranges_disjoint(ranges, range_count);
}

static bool range_disjoint_from(const void *pointer, size_t elements,
                                size_t element_size,
                                const acquisition_range *ranges,
                                size_t range_count) {
  acquisition_range candidate;
  if (!make_array_range(pointer, elements, element_size, &candidate))
    return false;
  if (!candidate.present) return true;
  for (size_t index = 0u; index < range_count; index += 1u) {
    if (ranges_overlap(candidate, ranges[index])) return false;
  }
  return true;
}

static bool target_disjoint_from_storage(
    const w_seed_acquisition_storage *storage, const void *target,
    size_t target_size) {
  acquisition_range ranges[ACQUISITION_STORAGE_RANGE_COUNT];
  size_t range_count = 0u;
  return storage_valid(storage) &&
         storage_ranges(storage, ranges, ACQUISITION_STORAGE_RANGE_COUNT,
                        &range_count) &&
         range_disjoint_from(target, 1u, target_size, ranges, range_count);
}

static bool add_token_buffer_ranges(
    const w_seed_ephemeral_provider_token_buffers *tokens,
    acquisition_range *ranges, size_t capacity, size_t *range_count) {
  return tokens != NULL &&
         add_range(ranges, capacity, range_count, tokens->provider_id,
                   tokens->provider_id_capacity, sizeof(char)) &&
         add_range(ranges, capacity, range_count, tokens->root_token,
                   tokens->root_token_capacity, sizeof(char)) &&
         add_range(ranges, capacity, range_count,
                   tokens->source_provider_owner_token,
                   tokens->source_provider_owner_token_capacity,
                   sizeof(char)) &&
         add_range(ranges, capacity, range_count, tokens->canonical_token,
                   tokens->canonical_token_capacity, sizeof(char));
}

static bool add_preserved_driver_ranges(
    const w_seed_ephemeral_driver_scratch *scratch,
    acquisition_range *ranges, size_t capacity, size_t *range_count) {
  if (scratch == NULL || ranges == NULL || range_count == NULL) return false;
  for (size_t index = 0u; index < scratch->slot_capacity; index += 1u) {
    const w_seed_ephemeral_driver_slot *slot = &scratch->slots[index];
    if (!add_range(ranges, capacity, range_count, slot->source_id_storage,
                   slot->source_id_capacity, sizeof(char)) ||
        !add_range(ranges, capacity, range_count, slot->module_id_storage,
                   slot->module_id_capacity, sizeof(char)))
      return false;
  }
  for (size_t index = 0u; index < scratch->request_capacity; index += 1u) {
    const w_seed_ephemeral_provider_request *request =
        &scratch->requests[index];
    if (!add_token_buffer_ranges(&request->tokens, ranges, capacity,
                                 range_count) ||
        !add_token_buffer_ranges(&request->revalidation_tokens, ranges,
                                 capacity, range_count))
      return false;
  }
  return true;
}

static bool preserved_driver_views_disjoint(
    const w_seed_ephemeral_driver_scratch *scratch,
    const acquisition_range *ranges, size_t range_count) {
  if (scratch == NULL || ranges == NULL) return false;
  for (size_t index = 0u; index < scratch->request_capacity; index += 1u) {
    const w_seed_ephemeral_provider_request *request =
        &scratch->requests[index];
    if (!range_disjoint_from(request->source_id.data,
                             request->source_id.length, sizeof(char), ranges,
                             range_count))
      return false;
    if (request->source != NULL &&
        !(index < scratch->slot_capacity &&
          request->source == &scratch->slots[index].source) &&
        !range_disjoint_from(request->source, 1u, sizeof(*request->source),
                             ranges, range_count))
      return false;
    if (request->facts != NULL &&
        !(index < scratch->slot_capacity &&
          request->facts == &scratch->slots[index].facts) &&
        !range_disjoint_from(request->facts, 1u, sizeof(*request->facts),
                             ranges, range_count))
      return false;
  }
  return true;
}

static bool next_total(size_t current_total, size_t old_capacity,
                       size_t next_capacity, size_t maximum,
                       size_t *next) {
  if (current_total < old_capacity || old_capacity > maximum ||
      next_capacity > maximum)
    return false;
  return add_bounded(current_total - old_capacity, next_capacity, maximum,
                     next);
}

static size_t maximum(size_t left, size_t right) {
  return left > right ? left : right;
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
         steps < W_SEED_ACQUISITION_MAX_GROWTH_STEPS) {
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
    const void *old_pointer, size_t old_bytes, size_t next_bytes,
    w_seed_acquisition_allocate_fn allocate,
    acquisition_replacement *replacement) {
  if (replacement == NULL || allocate == NULL ||
      !pointer_capacity_valid(old_pointer, old_bytes))
    return false;
  replacement->pointer = (void *)old_pointer;
  replacement->bytes = old_bytes;
  replacement->allocated = false;
  replacement->releasable = false;
  if (next_bytes == old_bytes) return true;
  replacement->pointer = allocate(next_bytes);
  if (replacement->pointer == NULL) return false;
  replacement->bytes = next_bytes;
  replacement->allocated = true;
  return true;
}

static bool validate_replacement(
    const w_seed_acquisition_storage *storage,
    acquisition_replacement *replacement, size_t next_bytes,
    size_t alignment, const acquisition_replacement *prior,
    size_t prior_count) {
  if (storage == NULL || replacement == NULL) return false;
  if (!replacement->allocated) return true;
  acquisition_range candidate;
  if (!make_range(replacement->pointer, next_bytes, &candidate) ||
      (alignment > 1u &&
       (uintptr_t)replacement->pointer % (uintptr_t)alignment != 0u))
    return false;
  acquisition_range existing[ACQUISITION_STORAGE_RANGE_COUNT];
  size_t existing_count = 0u;
  if (!storage_ranges(storage, existing, ACQUISITION_STORAGE_RANGE_COUNT,
                      &existing_count))
    return false;
  for (size_t index = 0u; index < existing_count; index += 1u) {
    if (ranges_overlap(candidate, existing[index])) return false;
  }
  for (size_t index = 0u; index < prior_count; index += 1u) {
    if (!prior[index].allocated || !prior[index].releasable) continue;
    acquisition_range prior_range;
    if (!make_range(prior[index].pointer, prior[index].bytes, &prior_range) ||
        ranges_overlap(candidate, prior_range))
      return false;
  }
  replacement->releasable = true;
  return true;
}

static void discard_replacement(
    acquisition_replacement *replacement,
    w_seed_acquisition_deallocate_fn deallocate) {
  if (replacement == NULL) return;
  if (replacement->allocated && replacement->releasable &&
      deallocate != NULL)
    deallocate(replacement->pointer);
  replacement->pointer = NULL;
  replacement->bytes = 0u;
  replacement->allocated = false;
  replacement->releasable = false;
}

bool w_seed_acquisition_storage_init_with_allocator(
    w_seed_acquisition_storage *storage,
    w_seed_acquisition_allocate_fn allocate,
    w_seed_acquisition_deallocate_fn deallocate) {
  if (storage == NULL || allocate == NULL || deallocate == NULL ||
      !object_is_zero(storage, sizeof(*storage)))
    return false;
  w_seed_acquisition_storage initialized = {0};
  initialized.allocate = allocate;
  initialized.deallocate = deallocate;
  initialized.owner = storage;
  initialized.initialized = true;
  *storage = initialized;
  return true;
}

bool w_seed_acquisition_storage_init(w_seed_acquisition_storage *storage) {
  return w_seed_acquisition_storage_init_with_allocator(storage, malloc, free);
}

w_seed_acquisition_storage_status w_seed_acquisition_storage_grow_source(
    w_seed_acquisition_storage *storage, size_t request_index,
    size_t required_capacity) {
  if (!index_valid(request_index) || !storage_valid(storage) ||
      storage->pipeline_active)
    return W_SEED_ACQUISITION_STORAGE_INVALID;
  if (required_capacity > (size_t)W_SEED_ACQUISITION_MAX_SOURCE_BYTES)
    return W_SEED_ACQUISITION_STORAGE_CAPACITY;

  const size_t old_staging = storage->staging_capacity[request_index];
  const size_t old_revalidation =
      storage->revalidation_capacity[request_index];
  const size_t old_published = storage->published_capacity[request_index];
  const size_t current =
      maximum(old_staging, maximum(old_revalidation, old_published));
  size_t rounded = 0u;
  if (!round_capacity(current, required_capacity,
                      (size_t)W_SEED_ACQUISITION_MAX_SOURCE_BYTES, &rounded))
    return W_SEED_ACQUISITION_STORAGE_CAPACITY;
  size_t next_staging = maximum(old_staging, rounded);
  size_t next_revalidation = maximum(old_revalidation, rounded);
  size_t next_published = maximum(old_published, rounded);
  size_t next_staging_total = 0u;
  size_t next_revalidation_total = 0u;
  size_t next_published_total = 0u;
  if (!next_total(storage->staging_total, old_staging, next_staging,
                  (size_t)W_SEED_ACQUISITION_MAX_TOTAL_SOURCE_BYTES,
                  &next_staging_total) ||
      !next_total(storage->revalidation_total, old_revalidation,
                  next_revalidation,
                  (size_t)W_SEED_ACQUISITION_MAX_TOTAL_SOURCE_BYTES,
                  &next_revalidation_total) ||
      !next_total(storage->published_total, old_published, next_published,
                  (size_t)W_SEED_ACQUISITION_MAX_TOTAL_SOURCE_BYTES,
                  &next_published_total)) {
    next_staging = maximum(old_staging, required_capacity);
    next_revalidation = maximum(old_revalidation, required_capacity);
    next_published = maximum(old_published, required_capacity);
    if (!next_total(storage->staging_total, old_staging, next_staging,
                    (size_t)W_SEED_ACQUISITION_MAX_TOTAL_SOURCE_BYTES,
                    &next_staging_total) ||
        !next_total(storage->revalidation_total, old_revalidation,
                    next_revalidation,
                    (size_t)W_SEED_ACQUISITION_MAX_TOTAL_SOURCE_BYTES,
                    &next_revalidation_total) ||
        !next_total(storage->published_total, old_published, next_published,
                    (size_t)W_SEED_ACQUISITION_MAX_TOTAL_SOURCE_BYTES,
                    &next_published_total))
      return W_SEED_ACQUISITION_STORAGE_CAPACITY;
  }

  acquisition_replacement replacements[3] = {
      {NULL, 0u, false, false},
      {NULL, 0u, false, false},
      {NULL, 0u, false, false}};
  if (!prepare_replacement(storage->staging_bytes[request_index], old_staging,
                           next_staging, storage->allocate,
                           &replacements[0]) ||
      !validate_replacement(storage, &replacements[0], next_staging, 1u,
                            replacements, 0u) ||
      !prepare_replacement(storage->revalidation_bytes[request_index],
                           old_revalidation, next_revalidation,
                           storage->allocate, &replacements[1]) ||
      !validate_replacement(storage, &replacements[1], next_revalidation, 1u,
                            replacements, 1u) ||
      !prepare_replacement(storage->published_bytes[request_index],
                           old_published, next_published, storage->allocate,
                           &replacements[2]) ||
      !validate_replacement(storage, &replacements[2], next_published, 1u,
                            replacements, 2u)) {
    for (size_t index = 0u; index < 3u; index += 1u)
      discard_replacement(&replacements[index], storage->deallocate);
    return W_SEED_ACQUISITION_STORAGE_ALLOCATION;
  }

  if (replacements[0].allocated && old_staging != 0u)
    (void)memcpy(replacements[0].pointer,
                 storage->staging_bytes[request_index], old_staging);
  if (replacements[1].allocated && old_revalidation != 0u)
    (void)memcpy(replacements[1].pointer,
                 storage->revalidation_bytes[request_index], old_revalidation);
  if (replacements[2].allocated && old_published != 0u)
    (void)memcpy(replacements[2].pointer,
                 storage->published_bytes[request_index], old_published);

  if (replacements[0].allocated &&
      storage->staging_bytes[request_index] != NULL)
    storage->deallocate(storage->staging_bytes[request_index]);
  if (replacements[1].allocated &&
      storage->revalidation_bytes[request_index] != NULL)
    storage->deallocate(storage->revalidation_bytes[request_index]);
  if (replacements[2].allocated &&
      storage->published_bytes[request_index] != NULL)
    storage->deallocate(storage->published_bytes[request_index]);
  storage->staging_bytes[request_index] =
      (uint8_t *)replacements[0].pointer;
  storage->revalidation_bytes[request_index] =
      (uint8_t *)replacements[1].pointer;
  storage->published_bytes[request_index] =
      (uint8_t *)replacements[2].pointer;
  storage->staging_capacity[request_index] = next_staging;
  storage->revalidation_capacity[request_index] = next_revalidation;
  storage->published_capacity[request_index] = next_published;
  storage->staging_total = next_staging_total;
  storage->revalidation_total = next_revalidation_total;
  storage->published_total = next_published_total;
  return W_SEED_ACQUISITION_STORAGE_OK;
}

w_seed_acquisition_storage_status w_seed_acquisition_storage_grow_nodes(
    w_seed_acquisition_storage *storage, size_t request_index,
    size_t required_capacity) {
  if (!index_valid(request_index) || !storage_valid(storage) ||
      storage->pipeline_active)
    return W_SEED_ACQUISITION_STORAGE_INVALID;
  if (required_capacity > (size_t)W_SEED_ACQUISITION_MAX_NODES)
    return W_SEED_ACQUISITION_STORAGE_CAPACITY;

  const size_t old_capacity = storage->node_capacity[request_index];
  size_t next_capacity = 0u;
  if (!round_capacity(old_capacity, required_capacity,
                      (size_t)W_SEED_ACQUISITION_MAX_NODES, &next_capacity))
    return W_SEED_ACQUISITION_STORAGE_CAPACITY;
  size_t next_node_total = 0u;
  if (!next_total(storage->node_total, old_capacity, next_capacity,
                  (size_t)W_SEED_ACQUISITION_MAX_TOTAL_NODES,
                  &next_node_total))
    return W_SEED_ACQUISITION_STORAGE_CAPACITY;

  size_t old_bytes = 0u;
  size_t next_bytes = 0u;
  if (!node_bytes(old_capacity, &old_bytes) ||
      !node_bytes(next_capacity, &next_bytes))
    return W_SEED_ACQUISITION_STORAGE_CAPACITY;
  acquisition_replacement replacement = {NULL, 0u, false, false};
  if (!prepare_replacement(storage->nodes[request_index], old_bytes,
                           next_bytes, storage->allocate, &replacement) ||
      !validate_replacement(storage, &replacement, next_bytes,
                            _Alignof(w_seed_cst_node), &replacement, 0u)) {
    discard_replacement(&replacement, storage->deallocate);
    return W_SEED_ACQUISITION_STORAGE_ALLOCATION;
  }
  if (replacement.allocated && old_bytes != 0u)
    (void)memcpy(replacement.pointer, storage->nodes[request_index], old_bytes);
  if (replacement.allocated && storage->nodes[request_index] != NULL)
    storage->deallocate(storage->nodes[request_index]);
  storage->nodes[request_index] = (w_seed_cst_node *)replacement.pointer;
  storage->node_capacity[request_index] = next_capacity;
  storage->node_total = next_node_total;
  return W_SEED_ACQUISITION_STORAGE_OK;
}

bool w_seed_acquisition_storage_bind_request(
    const w_seed_acquisition_storage *storage, size_t request_index,
    w_seed_ephemeral_provider_request *request) {
  if (request == NULL || !index_valid(request_index) ||
      !storage_valid(storage) || storage->pipeline_active ||
      !target_disjoint_from_storage(storage, request, sizeof(*request)))
    return false;
  w_seed_ephemeral_provider_request candidate;
  (void)memcpy(&candidate, request, sizeof(candidate));
  candidate.staging_bytes = storage->staging_bytes[request_index];
  candidate.staging_capacity = storage->staging_capacity[request_index];
  candidate.revalidation_bytes = storage->revalidation_bytes[request_index];
  candidate.revalidation_capacity =
      storage->revalidation_capacity[request_index];
  candidate.bytes = storage->published_bytes[request_index];
  candidate.byte_capacity = storage->published_capacity[request_index];
  (void)memcpy(request, &candidate, sizeof(candidate));
  return true;
}

bool w_seed_acquisition_storage_bind_slot(
    const w_seed_acquisition_storage *storage, size_t request_index,
    w_seed_ephemeral_driver_slot *slot) {
  if (slot == NULL || !index_valid(request_index) ||
      !storage_valid(storage) || storage->pipeline_active ||
      !target_disjoint_from_storage(storage, slot, sizeof(*slot)))
    return false;
  w_seed_ephemeral_driver_slot candidate;
  (void)memcpy(&candidate, slot, sizeof(candidate));
  candidate.nodes = storage->nodes[request_index];
  candidate.node_capacity = storage->node_capacity[request_index];
  (void)memcpy(slot, &candidate, sizeof(candidate));
  return true;
}

bool w_seed_acquisition_storage_bind_driver(
    const w_seed_acquisition_storage *storage,
    w_seed_ephemeral_driver_scratch *scratch) {
  if (!storage_valid(storage) || storage->pipeline_active || scratch == NULL ||
      scratch->slots == NULL ||
      scratch->requests == NULL ||
      scratch->slot_capacity > (size_t)W_SEED_ACQUISITION_MAX_SOURCES ||
      scratch->request_capacity > (size_t)W_SEED_ACQUISITION_MAX_SOURCES)
    return false;

  acquisition_range ranges[ACQUISITION_BIND_RANGE_COUNT];
  size_t range_count = 0u;
  if (!storage_ranges(storage, ranges, ACQUISITION_BIND_RANGE_COUNT,
                      &range_count) ||
      !add_range(ranges, ACQUISITION_BIND_RANGE_COUNT, &range_count, scratch,
                 1u, sizeof(*scratch)) ||
      !add_range(ranges, ACQUISITION_BIND_RANGE_COUNT, &range_count,
                 scratch->slots, scratch->slot_capacity,
                 sizeof(*scratch->slots)) ||
      !add_range(ranges, ACQUISITION_BIND_RANGE_COUNT, &range_count,
                 scratch->requests, scratch->request_capacity,
                 sizeof(*scratch->requests)) ||
      !preserved_driver_views_disjoint(scratch, ranges, range_count) ||
      !add_preserved_driver_ranges(scratch, ranges,
                                   ACQUISITION_BIND_RANGE_COUNT,
                                   &range_count) ||
      !ranges_disjoint(ranges, range_count))
    return false;

  w_seed_ephemeral_driver_slot
      slot_candidates[W_SEED_ACQUISITION_MAX_SOURCES];
  w_seed_ephemeral_provider_request
      request_candidates[W_SEED_ACQUISITION_MAX_SOURCES];
  for (size_t index = 0u; index < scratch->slot_capacity; index += 1u) {
    (void)memcpy(&slot_candidates[index], &scratch->slots[index],
                 sizeof(slot_candidates[index]));
    slot_candidates[index].nodes = storage->nodes[index];
    slot_candidates[index].node_capacity = storage->node_capacity[index];
  }
  for (size_t index = 0u; index < scratch->request_capacity; index += 1u) {
    (void)memcpy(&request_candidates[index], &scratch->requests[index],
                 sizeof(request_candidates[index]));
    request_candidates[index].staging_bytes = storage->staging_bytes[index];
    request_candidates[index].staging_capacity =
        storage->staging_capacity[index];
    request_candidates[index].revalidation_bytes =
        storage->revalidation_bytes[index];
    request_candidates[index].revalidation_capacity =
        storage->revalidation_capacity[index];
    request_candidates[index].bytes = storage->published_bytes[index];
    request_candidates[index].byte_capacity =
        storage->published_capacity[index];
  }
  for (size_t index = 0u; index < scratch->slot_capacity; index += 1u)
    (void)memcpy(&scratch->slots[index], &slot_candidates[index],
                 sizeof(scratch->slots[index]));
  for (size_t index = 0u; index < scratch->request_capacity; index += 1u)
    (void)memcpy(&scratch->requests[index], &request_candidates[index],
                 sizeof(scratch->requests[index]));
  return true;
}

void w_seed_acquisition_storage_destroy(w_seed_acquisition_storage *storage) {
  if (storage == NULL) return;
  if (object_is_zero(storage, sizeof(*storage))) return;
  if (!storage_valid(storage) || storage->pipeline_active) return;
  for (size_t index = 0u;
       index < (size_t)W_SEED_ACQUISITION_MAX_SOURCES; index += 1u) {
    if (storage->staging_bytes[index] != NULL)
      storage->deallocate(storage->staging_bytes[index]);
    if (storage->revalidation_bytes[index] != NULL)
      storage->deallocate(storage->revalidation_bytes[index]);
    if (storage->published_bytes[index] != NULL)
      storage->deallocate(storage->published_bytes[index]);
    if (storage->nodes[index] != NULL)
      storage->deallocate(storage->nodes[index]);
  }
  (void)memset(storage, 0, sizeof(*storage));
}

static w_seed_acquisition_retry_outcome retry_outcome(
    w_seed_acquisition_retry_status status,
    w_seed_acquisition_retry_action action,
    w_seed_acquisition_retry_detail detail, const char *reason) {
  return (w_seed_acquisition_retry_outcome){status, action, detail, reason};
}

static w_seed_acquisition_retry_outcome invalid_result(void) {
  return retry_outcome(W_SEED_ACQUISITION_RETRY_INVALID,
                       W_SEED_ACQUISITION_RETRY_TERMINAL,
                       W_SEED_ACQUISITION_RETRY_DETAIL_INVALID_RESULT,
                       "driver capacity envelope is invalid");
}

static bool provider_byte_field(
    w_seed_ephemeral_provider_capacity_field field) {
  return field == W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES ||
         field ==
             W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_BYTES ||
         field == W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_OUTPUT_BYTES;
}

static bool provider_non_resizable_field(
    w_seed_ephemeral_provider_capacity_field field) {
  switch (field) {
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_AGGREGATE_SOURCE_BYTES:
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_PROVIDER_ID:
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_ROOT_TOKEN:
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_SOURCE_PROVIDER_OWNER_TOKEN:
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_CANONICAL_TOKEN:
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_PROVIDER_ID:
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_ROOT_TOKEN:
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_SOURCE_PROVIDER_OWNER_TOKEN:
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_CANONICAL_TOKEN:
      return true;
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_NONE:
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES:
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_BYTES:
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_OUTPUT_BYTES:
    default:
      return false;
  }
}

static bool provider_phase_valid(
    const w_seed_ephemeral_provider_result *provider) {
  if (provider == NULL) return false;
  switch (provider->capacity_field) {
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES:
      return provider->phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_READ &&
             provider->observed_byte_count == provider->required_capacity &&
             (provider->backend_status ==
                  W_SEED_EPHEMERAL_PROVIDER_BACKEND_CAPACITY ||
              provider->backend_status ==
                  W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK);
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_BYTES:
      return provider->phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_REVALIDATE &&
             provider->observed_byte_count == provider->required_capacity &&
             provider->backend_status ==
                 W_SEED_EPHEMERAL_PROVIDER_BACKEND_CAPACITY;
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_OUTPUT_BYTES:
      return provider->phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_COMMIT &&
             provider->observed_byte_count == provider->required_capacity &&
             provider->backend_status == W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_AGGREGATE_SOURCE_BYTES:
      return provider->phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_READ &&
             provider->required_byte_capacity == 0u &&
             provider->observed_byte_count != 0u &&
             provider->total_source_bytes <= provider->required_capacity &&
             provider->observed_byte_count ==
                 provider->required_capacity - provider->total_source_bytes &&
             provider->backend_status == W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_PROVIDER_ID:
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_ROOT_TOKEN:
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_SOURCE_PROVIDER_OWNER_TOKEN:
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_CANONICAL_TOKEN:
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_PROVIDER_ID:
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_ROOT_TOKEN:
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_SOURCE_PROVIDER_OWNER_TOKEN:
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_CANONICAL_TOKEN:
      return provider->phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_VALIDATE &&
             provider->total_source_bytes == 0u &&
             provider->required_byte_capacity == 0u &&
             provider->observed_byte_count == 0u &&
             provider->backend_status == W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
    case W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_NONE:
    default:
      return false;
  }
}

static bool span_is_empty(w_seed_span span) {
  return span.start_byte == 0u && span.end_byte == 0u;
}

static bool counts_are_zero(w_seed_ephemeral_graph_counts counts) {
  return counts.sources == 0u && counts.edges == 0u &&
         counts.total_source_bytes == 0u;
}

static bool provider_clear_envelope(
    w_seed_ephemeral_provider_status status,
    const w_seed_ephemeral_provider_result *provider) {
  return provider != NULL && status == W_SEED_EPHEMERAL_PROVIDER_INVALID &&
         provider->status == W_SEED_EPHEMERAL_PROVIDER_INVALID &&
         provider->failure == W_SEED_EPHEMERAL_PROVIDER_FAILURE_NONE &&
         provider->phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_NONE &&
         provider->request_index == SIZE_MAX &&
         provider->total_source_bytes == 0u &&
         provider->capacity_field ==
             W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_NONE &&
         provider->required_capacity == 0u &&
         provider->required_byte_capacity == 0u &&
         provider->observed_byte_count == 0u &&
         provider->backend_status == W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
}

static bool parser_clear_envelope(
    const w_seed_ephemeral_driver_result *driver) {
  return driver != NULL && driver->parser_status == W_SEED_PARSE_FATAL &&
         driver->parser_issue_kind == W_SEED_PARSE_ISSUE_NONE;
}

static bool parser_success_envelope(
    const w_seed_ephemeral_driver_result *driver) {
  return driver != NULL && driver->parser_status == W_SEED_PARSE_COMPLETE &&
         driver->parser_issue_kind == W_SEED_PARSE_ISSUE_NONE;
}

static bool scan_clear_envelope(
    const w_seed_ephemeral_driver_result *driver) {
  return driver != NULL &&
         driver->scan_status == W_SEED_MODULE_SCAN_INVALID &&
         driver->scan_result.status == W_SEED_MODULE_SCAN_OK &&
         driver->scan_result.required == 0u &&
         driver->scan_result.written == 0u &&
         !driver->scan_result.has_module_header_name &&
         span_is_empty(driver->scan_result.module_header_name_span);
}

static bool scan_success_envelope(
    const w_seed_ephemeral_driver_result *driver) {
  return driver != NULL && driver->scan_status == W_SEED_MODULE_SCAN_OK &&
         driver->scan_result.status == W_SEED_MODULE_SCAN_OK &&
         driver->scan_result.required == driver->scan_result.written &&
         driver->scan_result.module_header_name_span.start_byte <=
             driver->scan_result.module_header_name_span.end_byte &&
         driver->scan_result.module_header_name_span.end_byte <=
             (size_t)W_SEED_ACQUISITION_MAX_SOURCE_BYTES &&
         (driver->scan_result.has_module_header_name
              ? driver->scan_result.module_header_name_span.start_byte <
                    driver->scan_result.module_header_name_span.end_byte
              : span_is_empty(
                    driver->scan_result.module_header_name_span));
}

static bool previous_scan_envelope(
    const w_seed_ephemeral_driver_result *driver) {
  return driver != NULL &&
         (driver->round == 0u ? scan_clear_envelope(driver)
                              : scan_success_envelope(driver));
}

static bool graph_clear_envelope(
    const w_seed_ephemeral_driver_result *driver) {
  if (driver == NULL) return false;
  const w_seed_ephemeral_graph_result *graph = &driver->graph_result;
  return driver->graph_status == W_SEED_EPHEMERAL_GRAPH_INVALID &&
         graph->status == W_SEED_EPHEMERAL_GRAPH_INVALID &&
         counts_are_zero(graph->required) && counts_are_zero(graph->written) &&
         graph->failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_NONE &&
         graph->candidate_index == SIZE_MAX &&
         graph->document_ordinal == SIZE_MAX &&
         graph->edge_ordinal == SIZE_MAX && span_is_empty(graph->span);
}

static bool graph_measure_success_envelope(
    const w_seed_ephemeral_driver_result *driver) {
  if (driver == NULL) return false;
  const w_seed_ephemeral_graph_result *graph = &driver->graph_result;
  return driver->graph_status == W_SEED_EPHEMERAL_GRAPH_OK &&
         graph->status == W_SEED_EPHEMERAL_GRAPH_OK &&
         counts_are_zero(graph->written) &&
         graph->failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_NONE &&
         graph->candidate_index == SIZE_MAX &&
         graph->document_ordinal == SIZE_MAX &&
         graph->edge_ordinal == SIZE_MAX && span_is_empty(graph->span);
}

static bool provider_capacity_envelope(
    w_seed_ephemeral_driver_status status,
    const w_seed_ephemeral_driver_result *driver) {
  if (driver == NULL) return false;
  const w_seed_ephemeral_provider_result *provider = &driver->provider_result;
  return status == W_SEED_EPHEMERAL_DRIVER_CAPACITY &&
         driver->status == W_SEED_EPHEMERAL_DRIVER_CAPACITY &&
         driver->failure == W_SEED_EPHEMERAL_DRIVER_FAILURE_PROVIDER &&
         driver->phase == W_SEED_EPHEMERAL_DRIVER_PHASE_ACQUIRE &&
         driver->round < (size_t)W_SEED_EPHEMERAL_DRIVER_MAX_ROUNDS &&
         driver->origin_index == SIZE_MAX &&
         driver->document_index == SIZE_MAX && driver->span.start_byte == 0u &&
         driver->span.end_byte == 0u &&
         driver->capacity_field ==
             W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PROVIDER &&
         driver->required_capacity != 0u &&
         driver->provider_status == W_SEED_EPHEMERAL_PROVIDER_CAPACITY &&
         provider->status == W_SEED_EPHEMERAL_PROVIDER_CAPACITY &&
         provider->failure == W_SEED_EPHEMERAL_PROVIDER_FAILURE_LIMIT &&
         provider->request_index == driver->candidate_index &&
         index_valid(provider->request_index) &&
         provider->required_capacity == driver->required_capacity &&
         provider->total_source_bytes <=
             (size_t)W_SEED_ACQUISITION_MAX_TOTAL_SOURCE_BYTES &&
         (provider_byte_field(provider->capacity_field)
              ? provider->required_byte_capacity == provider->required_capacity
              : provider->required_byte_capacity == 0u) &&
         provider_phase_valid(provider) &&
         (driver->round == 0u ? parser_clear_envelope(driver)
                              : parser_success_envelope(driver)) &&
         previous_scan_envelope(driver) && graph_clear_envelope(driver);
}

static bool provider_success_envelope(
    const w_seed_ephemeral_provider_result *provider) {
  return provider != NULL &&
         provider->status == W_SEED_EPHEMERAL_PROVIDER_OK &&
         provider->failure == W_SEED_EPHEMERAL_PROVIDER_FAILURE_NONE &&
         provider->phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_COMMIT &&
         provider->request_index == SIZE_MAX &&
         provider->capacity_field ==
             W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_NONE &&
         provider->required_capacity == 0u &&
         provider->required_byte_capacity == 0u &&
         provider->observed_byte_count == 0u &&
         provider->total_source_bytes <=
             (size_t)W_SEED_ACQUISITION_MAX_TOTAL_SOURCE_BYTES &&
         provider->backend_status == W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
}

static bool node_capacity_envelope(
    w_seed_ephemeral_driver_status status,
    const w_seed_ephemeral_driver_result *driver) {
  if (driver == NULL) return false;
  return status == W_SEED_EPHEMERAL_DRIVER_CAPACITY &&
         driver->status == W_SEED_EPHEMERAL_DRIVER_CAPACITY &&
         driver->failure == W_SEED_EPHEMERAL_DRIVER_FAILURE_PARSE &&
         driver->phase == W_SEED_EPHEMERAL_DRIVER_PHASE_PARSE &&
         driver->round < (size_t)W_SEED_EPHEMERAL_DRIVER_MAX_ROUNDS &&
         index_valid(driver->candidate_index) &&
         driver->origin_index == SIZE_MAX &&
         driver->document_index == driver->candidate_index &&
         driver->capacity_field ==
             W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_NODE &&
         driver->required_capacity != 0u &&
         driver->provider_status == W_SEED_EPHEMERAL_PROVIDER_OK &&
         provider_success_envelope(&driver->provider_result) &&
         driver->parser_status == W_SEED_PARSE_FATAL &&
         driver->parser_issue_kind >= W_SEED_PARSE_ISSUE_NONE &&
         driver->parser_issue_kind <= W_SEED_PARSE_ISSUE_CAPACITY &&
         previous_scan_envelope(driver) && graph_clear_envelope(driver);
}

static bool graph_capacity_envelope(
    w_seed_ephemeral_driver_status status,
    const w_seed_ephemeral_driver_result *driver) {
  if (driver == NULL) return false;
  const w_seed_ephemeral_graph_result *graph = &driver->graph_result;
  const size_t required = graph->required.sources != 0u
                              ? graph->required.sources
                              : graph->required.edges;
  return status == W_SEED_EPHEMERAL_DRIVER_CAPACITY &&
         driver->status == W_SEED_EPHEMERAL_DRIVER_CAPACITY &&
         driver->failure == W_SEED_EPHEMERAL_DRIVER_FAILURE_GRAPH &&
         (driver->phase == W_SEED_EPHEMERAL_DRIVER_PHASE_GRAPH_MEASURE ||
          driver->phase == W_SEED_EPHEMERAL_DRIVER_PHASE_GRAPH_WRITE) &&
         driver->round < (size_t)W_SEED_EPHEMERAL_DRIVER_MAX_ROUNDS &&
         driver->candidate_index == graph->candidate_index &&
         driver->origin_index == graph->edge_ordinal &&
         driver->document_index == graph->document_ordinal &&
         driver->span.start_byte == graph->span.start_byte &&
         driver->span.end_byte == graph->span.end_byte &&
         driver->capacity_field ==
             W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_NONE &&
         driver->required_capacity == required &&
         driver->provider_status == W_SEED_EPHEMERAL_PROVIDER_OK &&
         provider_success_envelope(&driver->provider_result) &&
         parser_success_envelope(driver) && scan_success_envelope(driver) &&
         driver->graph_status == W_SEED_EPHEMERAL_GRAPH_CAPACITY &&
         graph->status == W_SEED_EPHEMERAL_GRAPH_CAPACITY &&
         graph->failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_LIMIT &&
         counts_are_zero(graph->written);
}

static bool parser_non_resizable_field(
    w_seed_ephemeral_driver_capacity_field field) {
  return field == W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_TOKEN ||
         field == W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_FRAME ||
         field == W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_ISSUE ||
         field == W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_LEXER_FRAME;
}

static bool scan_capacity_envelope(
    const w_seed_ephemeral_driver_result *driver) {
  return driver != NULL &&
         driver->scan_status == W_SEED_MODULE_SCAN_CAPACITY &&
         driver->scan_result.status == W_SEED_MODULE_SCAN_CAPACITY &&
         driver->scan_result.required == driver->required_capacity &&
         driver->scan_result.written == 0u &&
         driver->scan_result.module_header_name_span.start_byte <=
             driver->scan_result.module_header_name_span.end_byte;
}

static bool fixed_prior_success(
    const w_seed_ephemeral_driver_result *driver) {
  return driver != NULL &&
         driver->provider_status == W_SEED_EPHEMERAL_PROVIDER_OK &&
         provider_success_envelope(&driver->provider_result) &&
         parser_success_envelope(driver);
}

static bool storage_non_resizable_envelope(
    const w_seed_ephemeral_driver_result *driver) {
  if (driver == NULL ||
      driver->failure != W_SEED_EPHEMERAL_DRIVER_FAILURE_STORAGE)
    return false;
  switch (driver->capacity_field) {
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_SLOT:
      return driver->phase == W_SEED_EPHEMERAL_DRIVER_PHASE_DISCOVER &&
             driver->round <
                 (size_t)W_SEED_EPHEMERAL_DRIVER_MAX_ROUNDS &&
             index_valid(driver->candidate_index) &&
             driver->document_index == driver->candidate_index &&
             driver->origin_index < driver->scan_result.written &&
             driver->span.start_byte < driver->span.end_byte &&
             fixed_prior_success(driver) && scan_success_envelope(driver) &&
             graph_clear_envelope(driver);
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_MODULE_ID:
      if (driver->phase != W_SEED_EPHEMERAL_DRIVER_PHASE_DISCOVER ||
          driver->round >=
              (size_t)W_SEED_EPHEMERAL_DRIVER_MAX_ROUNDS ||
          !index_valid(driver->candidate_index) ||
          driver->document_index != driver->candidate_index ||
          !fixed_prior_success(driver) || !scan_success_envelope(driver) ||
          !graph_clear_envelope(driver))
        return false;
      if (driver->origin_index != SIZE_MAX)
        return driver->origin_index < driver->scan_result.written &&
               driver->span.start_byte < driver->span.end_byte &&
               driver->required_capacity ==
                   driver->span.end_byte - driver->span.start_byte;
      return driver->candidate_index == 0u &&
             (driver->scan_result.has_module_header_name
                  ? (driver->span.start_byte ==
                         driver->scan_result.module_header_name_span
                             .start_byte &&
                     driver->span.end_byte ==
                         driver->scan_result.module_header_name_span.end_byte &&
                     driver->required_capacity ==
                         driver->span.end_byte - driver->span.start_byte)
                  : span_is_empty(driver->span));
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_ORIGIN:
      return driver->phase == W_SEED_EPHEMERAL_DRIVER_PHASE_DISCOVER &&
             driver->round <
                 (size_t)W_SEED_EPHEMERAL_DRIVER_MAX_ROUNDS &&
             index_valid(driver->candidate_index) &&
             driver->origin_index == SIZE_MAX &&
             driver->document_index == driver->candidate_index &&
             fixed_prior_success(driver) && scan_capacity_envelope(driver) &&
             graph_clear_envelope(driver);
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_SOURCE_ID:
      return (driver->phase == W_SEED_EPHEMERAL_DRIVER_PHASE_VALIDATE &&
              driver->round == SIZE_MAX && driver->candidate_index == 0u &&
              driver->origin_index == SIZE_MAX &&
              driver->document_index == SIZE_MAX &&
              span_is_empty(driver->span) &&
              provider_clear_envelope(driver->provider_status,
                                      &driver->provider_result) &&
              parser_clear_envelope(driver) && scan_clear_envelope(driver) &&
              graph_clear_envelope(driver)) ||
             (driver->phase == W_SEED_EPHEMERAL_DRIVER_PHASE_DISCOVER &&
              driver->round <
                  (size_t)W_SEED_EPHEMERAL_DRIVER_MAX_ROUNDS &&
              index_valid(driver->candidate_index) &&
              driver->document_index == driver->candidate_index &&
              driver->origin_index < driver->scan_result.written &&
              driver->span.start_byte < driver->span.end_byte &&
              driver->span.end_byte - driver->span.start_byte <=
                  SIZE_MAX - 2u &&
              driver->required_capacity ==
                  driver->span.end_byte - driver->span.start_byte + 2u &&
              fixed_prior_success(driver) && scan_success_envelope(driver) &&
              graph_clear_envelope(driver));
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_CANDIDATE_DOCUMENT:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_CANDIDATE_FACT:
      return driver->phase ==
                 W_SEED_EPHEMERAL_DRIVER_PHASE_GRAPH_MEASURE &&
             driver->round <
                 (size_t)W_SEED_EPHEMERAL_DRIVER_MAX_ROUNDS &&
             driver->candidate_index == SIZE_MAX &&
             driver->origin_index == SIZE_MAX &&
             driver->document_index == SIZE_MAX &&
             span_is_empty(driver->span) && fixed_prior_success(driver) &&
             scan_success_envelope(driver) && graph_clear_envelope(driver);
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_GRAPH_INVENTORY:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_GRAPH_EDGE:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_GRAPH_ORDER:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_GRAPH_RESOLVED:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_DOCUMENT:
      if (driver->phase != W_SEED_EPHEMERAL_DRIVER_PHASE_GRAPH_WRITE ||
          driver->round >=
              (size_t)W_SEED_EPHEMERAL_DRIVER_MAX_ROUNDS ||
          driver->candidate_index != SIZE_MAX ||
          driver->origin_index != SIZE_MAX ||
          driver->document_index != SIZE_MAX ||
          !span_is_empty(driver->span) || !fixed_prior_success(driver) ||
          !scan_success_envelope(driver) ||
          !graph_measure_success_envelope(driver))
        return false;
      if (driver->capacity_field ==
              W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_GRAPH_EDGE ||
          driver->capacity_field ==
              W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_GRAPH_RESOLVED)
        return driver->required_capacity ==
               driver->graph_result.required.edges;
      return driver->required_capacity ==
             driver->graph_result.required.sources;
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_NONE:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_REQUEST:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_NODE:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_TOKEN:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_FRAME:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_ISSUE:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_LEXER_FRAME:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_ROUNDS:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PROVIDER:
    default:
      return false;
  }
}

static bool non_resizable_envelope(
    w_seed_ephemeral_driver_status status,
    const w_seed_ephemeral_driver_result *driver) {
  if (graph_capacity_envelope(status, driver)) return true;
  if (driver == NULL || status != W_SEED_EPHEMERAL_DRIVER_CAPACITY ||
      driver->status != W_SEED_EPHEMERAL_DRIVER_CAPACITY ||
      driver->required_capacity == 0u)
    return false;
  if (parser_non_resizable_field(driver->capacity_field))
    return driver->failure == W_SEED_EPHEMERAL_DRIVER_FAILURE_PARSE &&
           driver->phase == W_SEED_EPHEMERAL_DRIVER_PHASE_PARSE &&
           index_valid(driver->candidate_index) &&
           driver->origin_index == SIZE_MAX &&
           driver->document_index == driver->candidate_index &&
           driver->provider_status == W_SEED_EPHEMERAL_PROVIDER_OK &&
           provider_success_envelope(&driver->provider_result) &&
           driver->parser_status == W_SEED_PARSE_FATAL &&
           driver->parser_issue_kind >= W_SEED_PARSE_ISSUE_NONE &&
           driver->parser_issue_kind <= W_SEED_PARSE_ISSUE_CAPACITY &&
           previous_scan_envelope(driver) && graph_clear_envelope(driver);
  if (driver->capacity_field == W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_ROUNDS)
    return driver->failure == W_SEED_EPHEMERAL_DRIVER_FAILURE_LIMIT &&
           driver->phase == W_SEED_EPHEMERAL_DRIVER_PHASE_DISCOVER &&
           driver->round ==
               (size_t)W_SEED_EPHEMERAL_DRIVER_MAX_ROUNDS &&
           driver->candidate_index == SIZE_MAX &&
           driver->origin_index == SIZE_MAX &&
           driver->document_index == SIZE_MAX &&
           driver->required_capacity ==
               (size_t)W_SEED_EPHEMERAL_DRIVER_MAX_ROUNDS + 1u &&
           span_is_empty(driver->span) && fixed_prior_success(driver) &&
           scan_success_envelope(driver) && graph_clear_envelope(driver);
  return storage_non_resizable_envelope(driver);
}

static w_seed_acquisition_retry_outcome apply_storage_status(
    w_seed_acquisition_storage_status status) {
  if (status == W_SEED_ACQUISITION_STORAGE_ALLOCATION)
    return retry_outcome(W_SEED_ACQUISITION_RETRY_ALLOCATION,
                         W_SEED_ACQUISITION_RETRY_TERMINAL,
                         W_SEED_ACQUISITION_RETRY_DETAIL_ALLOCATION,
                         "acquisition storage allocation failed");
  if (status == W_SEED_ACQUISITION_STORAGE_CAPACITY)
    return retry_outcome(W_SEED_ACQUISITION_RETRY_CAPACITY,
                         W_SEED_ACQUISITION_RETRY_TERMINAL,
                         W_SEED_ACQUISITION_RETRY_DETAIL_CAPACITY,
                         "acquisition storage capacity exceeded");
  return retry_outcome(W_SEED_ACQUISITION_RETRY_FAULT,
                       W_SEED_ACQUISITION_RETRY_TERMINAL,
                       W_SEED_ACQUISITION_RETRY_DETAIL_STORAGE,
                       "acquisition storage invariant failed");
}

w_seed_acquisition_retry_outcome w_seed_acquisition_retry_apply(
    w_seed_acquisition_storage *storage,
    w_seed_ephemeral_driver_status driver_status,
    const w_seed_ephemeral_driver_result *driver_result) {
  if (storage == NULL || driver_result == NULL) return invalid_result();
  if (!storage_valid(storage) || storage->pipeline_active)
    return retry_outcome(W_SEED_ACQUISITION_RETRY_FAULT,
                         W_SEED_ACQUISITION_RETRY_TERMINAL,
                         W_SEED_ACQUISITION_RETRY_DETAIL_STORAGE,
                         "acquisition storage invariant failed");
  if (driver_result->capacity_field ==
      W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PROVIDER) {
    if (!provider_capacity_envelope(driver_status, driver_result))
      return invalid_result();
    const w_seed_ephemeral_provider_capacity_field field =
        driver_result->provider_result.capacity_field;
    if (!provider_byte_field(field)) {
      if (!provider_non_resizable_field(field)) return invalid_result();
      return retry_outcome(W_SEED_ACQUISITION_RETRY_CAPACITY,
                           W_SEED_ACQUISITION_RETRY_TERMINAL,
                           W_SEED_ACQUISITION_RETRY_DETAIL_NON_RESIZABLE,
                           "provider capacity is not resizable");
    }
    if (field == W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES &&
        driver_result->provider_result.backend_status ==
            W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK)
      return retry_outcome(W_SEED_ACQUISITION_RETRY_CAPACITY,
                           W_SEED_ACQUISITION_RETRY_TERMINAL,
                           W_SEED_ACQUISITION_RETRY_DETAIL_NON_RESIZABLE,
                           "provider source-byte limit is not resizable");
    const size_t index = driver_result->provider_result.request_index;
    const size_t old_capacity =
        maximum(storage->staging_capacity[index],
                maximum(storage->revalidation_capacity[index],
                        storage->published_capacity[index]));
    const w_seed_acquisition_storage_status status =
        w_seed_acquisition_storage_grow_source(
            storage, index, driver_result->required_capacity);
    if (status != W_SEED_ACQUISITION_STORAGE_OK)
      return apply_storage_status(status);
    const size_t new_capacity =
        maximum(storage->staging_capacity[index],
                maximum(storage->revalidation_capacity[index],
                        storage->published_capacity[index]));
    if (new_capacity <= old_capacity)
      return retry_outcome(W_SEED_ACQUISITION_RETRY_FAULT,
                           W_SEED_ACQUISITION_RETRY_TERMINAL,
                           W_SEED_ACQUISITION_RETRY_DETAIL_NO_PROGRESS,
                           "acquisition storage grow made no progress");
    return retry_outcome(W_SEED_ACQUISITION_RETRY_OK,
                         W_SEED_ACQUISITION_RETRY_RETRY,
                         W_SEED_ACQUISITION_RETRY_DETAIL_NONE,
                         "provider byte arenas grown");
  }
  if (driver_result->capacity_field ==
      W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_NODE) {
    if (!node_capacity_envelope(driver_status, driver_result))
      return invalid_result();
    const size_t index = driver_result->candidate_index;
    const size_t old_capacity = storage->node_capacity[index];
    const w_seed_acquisition_storage_status status =
        w_seed_acquisition_storage_grow_nodes(
            storage, index, driver_result->required_capacity);
    if (status != W_SEED_ACQUISITION_STORAGE_OK)
      return apply_storage_status(status);
    if (storage->node_capacity[index] <= old_capacity)
      return retry_outcome(W_SEED_ACQUISITION_RETRY_FAULT,
                           W_SEED_ACQUISITION_RETRY_TERMINAL,
                           W_SEED_ACQUISITION_RETRY_DETAIL_NO_PROGRESS,
                           "acquisition node grow made no progress");
    return retry_outcome(W_SEED_ACQUISITION_RETRY_OK,
                         W_SEED_ACQUISITION_RETRY_RETRY,
                         W_SEED_ACQUISITION_RETRY_DETAIL_NONE,
                         "parser node arena grown");
  }
  if (!non_resizable_envelope(driver_status, driver_result))
    return invalid_result();
  if (driver_result->capacity_field ==
      W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_ROUNDS)
    return retry_outcome(W_SEED_ACQUISITION_RETRY_CAPACITY,
                         W_SEED_ACQUISITION_RETRY_TERMINAL,
                         W_SEED_ACQUISITION_RETRY_DETAIL_RETRY_LIMIT,
                         "driver round limit exhausted");
  return retry_outcome(W_SEED_ACQUISITION_RETRY_CAPACITY,
                       W_SEED_ACQUISITION_RETRY_TERMINAL,
                       W_SEED_ACQUISITION_RETRY_DETAIL_NON_RESIZABLE,
                       "driver capacity is not resizable");
}

typedef enum {
  ACQUISITION_PREFLIGHT_OK = 0,
  ACQUISITION_PREFLIGHT_INVALID,
  ACQUISITION_PREFLIGHT_UNPUBLISHABLE,
} acquisition_preflight_status;

static bool pointer_aligned(const void *pointer, size_t alignment) {
  return pointer != NULL &&
         (alignment <= 1u ||
          (uintptr_t)pointer % (uintptr_t)alignment == 0u);
}

typedef struct {
  const w_seed_acquisition_pipeline_input *input_address;
  w_seed_acquisition_pipeline_input input;
  w_seed_ephemeral_driver_input driver;
  w_seed_ephemeral_driver_scratch scratch;
  w_seed_ephemeral_driver_output output;
  w_seed_acquisition_storage storage;
  w_seed_ephemeral_graph_scratch graph;
  w_seed_ephemeral_driver_slot slots[W_SEED_ACQUISITION_MAX_SOURCES];
  w_seed_ephemeral_provider_request
      requests[W_SEED_ACQUISITION_MAX_SOURCES];
} acquisition_pipeline_snapshot;

static bool result_disjoint_from_ranges(
    acquisition_range result_range, const acquisition_range *ranges,
    size_t count) {
  if (ranges == NULL) return false;
  for (size_t index = 0u; index < count; index += 1u) {
    if (ranges_overlap(result_range, ranges[index])) return false;
  }
  return true;
}

/* Phase 1 proves the top-level containers pairwise disjoint before any one of
 * those containers is dereferenced. A failure leaves the caller result
 * untouched. */
static bool pipeline_top_snapshot(
    const w_seed_acquisition_pipeline_input *input,
    const w_seed_acquisition_pipeline_result *result,
    acquisition_pipeline_snapshot *snapshot,
    acquisition_range *result_range) {
  if (!pointer_aligned(input, _Alignof(w_seed_acquisition_pipeline_input)) ||
      !pointer_aligned(result,
                       _Alignof(w_seed_acquisition_pipeline_result)) ||
      snapshot == NULL || result_range == NULL ||
      !make_range(result, sizeof(*result), result_range))
    return false;

  acquisition_range input_range;
  if (!make_range(input, sizeof(*input), &input_range) ||
      ranges_overlap(*result_range, input_range))
    return false;
  snapshot->input_address = input;
  (void)memcpy(&snapshot->input, input, sizeof(snapshot->input));

  acquisition_range top[5];
  size_t count = 0u;
  if (!add_aligned_range(top, 5u, &count, input, 1u, sizeof(*input),
                         _Alignof(w_seed_acquisition_pipeline_input)) ||
      !add_aligned_range(top, 5u, &count, snapshot->input.driver_input, 1u,
                         sizeof(*snapshot->input.driver_input),
                         _Alignof(w_seed_ephemeral_driver_input)) ||
      !add_aligned_range(top, 5u, &count, snapshot->input.scratch, 1u,
                         sizeof(*snapshot->input.scratch),
                         _Alignof(w_seed_ephemeral_driver_scratch)) ||
      !add_aligned_range(top, 5u, &count, snapshot->input.output, 1u,
                         sizeof(*snapshot->input.output),
                         _Alignof(w_seed_ephemeral_driver_output)) ||
      !add_aligned_range(top, 5u, &count, snapshot->input.storage, 1u,
                         sizeof(*snapshot->input.storage),
                         _Alignof(w_seed_acquisition_storage)) ||
      !ranges_disjoint(top, count) ||
      !result_disjoint_from_ranges(*result_range, top, count))
    return false;

  (void)memcpy(&snapshot->driver, snapshot->input.driver_input,
               sizeof(snapshot->driver));
  (void)memcpy(&snapshot->scratch, snapshot->input.scratch,
               sizeof(snapshot->scratch));
  (void)memcpy(&snapshot->output, snapshot->input.output,
               sizeof(snapshot->output));
  (void)memcpy(&snapshot->storage, snapshot->input.storage,
               sizeof(snapshot->storage));
  return true;
}

static bool bounded_pointer(const void *pointer, size_t capacity,
                            size_t maximum) {
  return capacity == 0u || (pointer != NULL && capacity <= maximum);
}

static bool pipeline_direct_shape_valid(
    const acquisition_pipeline_snapshot *snapshot) {
  if (snapshot == NULL || snapshot->input.driver_input == NULL ||
      snapshot->input.scratch == NULL || snapshot->input.output == NULL ||
      snapshot->input.storage == NULL || snapshot->scratch.slots == NULL ||
      snapshot->scratch.requests == NULL ||
      snapshot->scratch.graph_scratch == NULL ||
      snapshot->scratch.slot_capacity >
          (size_t)W_SEED_ACQUISITION_MAX_SOURCES ||
      snapshot->scratch.request_capacity >
          (size_t)W_SEED_ACQUISITION_MAX_SOURCES ||
      snapshot->scratch.candidate_document_capacity >
          (size_t)W_SEED_ACQUISITION_MAX_SOURCES ||
      snapshot->scratch.candidate_fact_capacity >
          (size_t)W_SEED_ACQUISITION_MAX_SOURCES ||
      snapshot->scratch.origin_capacity >
          (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES ||
      snapshot->output.document_capacity >
          (size_t)W_SEED_FRONTEND_MAX_DOCUMENTS ||
      snapshot->output.graph.inventory_capacity >
          (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES ||
      snapshot->output.graph.edge_capacity >
          (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES ||
      snapshot->output.graph.document_order_capacity >
          (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES ||
      snapshot->output.graph.resolved_import_capacity >
          (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES)
    return false;

  const w_seed_ephemeral_driver_input *driver = &snapshot->driver;
  const w_seed_ephemeral_driver_scratch *scratch = &snapshot->scratch;
  const w_seed_ephemeral_driver_output *output = &snapshot->output;
  if ((driver->backend.context == NULL) !=
          (snapshot->input.backend_context_size == 0u) ||
      !bounded_pointer(driver->root_path.data, driver->root_path.length,
                       (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES) ||
      !bounded_pointer(driver->root_source_id.data,
                       driver->root_source_id.length,
                       (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES) ||
      !bounded_pointer(scratch->lexer_frames, scratch->lexer_frame_capacity,
                       (size_t)W_SEED_FRONTEND_MAX_CST_NODES) ||
      !bounded_pointer(scratch->tokens, scratch->token_capacity,
                       (size_t)(4u * W_SEED_FRONTEND_MAX_CST_NODES)) ||
      !bounded_pointer(scratch->parse_frames, scratch->parse_frame_capacity,
                       (size_t)W_SEED_FRONTEND_MAX_CST_NODES) ||
      !bounded_pointer(scratch->issues, scratch->issue_capacity,
                       (size_t)W_SEED_FRONTEND_MAX_CST_NODES) ||
      !bounded_pointer(scratch->origins, scratch->origin_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES) ||
      !bounded_pointer(scratch->candidate_documents,
                       scratch->candidate_document_capacity,
                       (size_t)W_SEED_ACQUISITION_MAX_SOURCES) ||
      !bounded_pointer(scratch->candidate_facts,
                       scratch->candidate_fact_capacity,
                       (size_t)W_SEED_ACQUISITION_MAX_SOURCES) ||
      !bounded_pointer(output->documents, output->document_capacity,
                       (size_t)W_SEED_FRONTEND_MAX_DOCUMENTS) ||
      !bounded_pointer(output->graph.inventory,
                       output->graph.inventory_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES) ||
      !bounded_pointer(output->graph.edges, output->graph.edge_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES) ||
      !bounded_pointer(output->graph.document_order,
                       output->graph.document_order_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES) ||
      !bounded_pointer(output->graph.resolved_imports,
                       output->graph.resolved_import_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES))
    return false;
  return true;
}

static bool pipeline_nested_snapshot_valid(
    const acquisition_pipeline_snapshot *snapshot) {
  if (snapshot == NULL) return false;
  const w_seed_ephemeral_graph_scratch *graph = &snapshot->graph;
  if (!bounded_pointer(graph->nodes, graph->node_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES) ||
      !bounded_pointer(graph->edges, graph->edge_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES) ||
      !bounded_pointer(graph->sorted_nodes, graph->sorted_nodes_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES) ||
      !bounded_pointer(graph->node_ordinals, graph->node_ordinals_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES) ||
      !bounded_pointer(graph->sorted_edges, graph->sorted_edges_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES) ||
      !bounded_pointer(graph->sorted_resolved_edges,
                       graph->sorted_resolved_edges_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES) ||
      !bounded_pointer(graph->origins, graph->origin_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES) ||
      !bounded_pointer(graph->indegree, graph->indegree_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES) ||
      !bounded_pointer(graph->queue, graph->queue_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES) ||
      !bounded_pointer(graph->depths, graph->depths_capacity,
                       (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES))
    return false;

  for (size_t index = 0u; index < snapshot->scratch.slot_capacity;
       index += 1u) {
    const w_seed_ephemeral_driver_slot *slot = &snapshot->slots[index];
    if (!bounded_pointer(slot->source_id_storage, slot->source_id_capacity,
                         (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES) ||
        !bounded_pointer(slot->module_id_storage, slot->module_id_capacity,
                         (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES))
      return false;
  }
  for (size_t index = 0u; index < snapshot->scratch.request_capacity;
       index += 1u) {
    const w_seed_ephemeral_provider_request *request =
        &snapshot->requests[index];
#define ACQUISITION_TOKEN_BOUNDED(pointer, capacity)                         \
  if (!bounded_pointer((pointer), (capacity),                                \
                       (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_TOKEN_BYTES))    \
  return false
    ACQUISITION_TOKEN_BOUNDED(request->tokens.provider_id,
                              request->tokens.provider_id_capacity);
    ACQUISITION_TOKEN_BOUNDED(request->tokens.root_token,
                              request->tokens.root_token_capacity);
    ACQUISITION_TOKEN_BOUNDED(
        request->tokens.source_provider_owner_token,
        request->tokens.source_provider_owner_token_capacity);
    ACQUISITION_TOKEN_BOUNDED(request->tokens.canonical_token,
                              request->tokens.canonical_token_capacity);
    ACQUISITION_TOKEN_BOUNDED(request->revalidation_tokens.provider_id,
                              request->revalidation_tokens.provider_id_capacity);
    ACQUISITION_TOKEN_BOUNDED(request->revalidation_tokens.root_token,
                              request->revalidation_tokens.root_token_capacity);
    ACQUISITION_TOKEN_BOUNDED(
        request->revalidation_tokens.source_provider_owner_token,
        request->revalidation_tokens.source_provider_owner_token_capacity);
    ACQUISITION_TOKEN_BOUNDED(
        request->revalidation_tokens.canonical_token,
        request->revalidation_tokens.canonical_token_capacity);
#undef ACQUISITION_TOKEN_BOUNDED
  }
  return true;
}

static bool pipeline_limits_valid(
    const w_seed_ephemeral_driver_input *input,
    const w_seed_ephemeral_driver_scratch *scratch) {
  return input != NULL && scratch != NULL && scratch->slot_capacity != 0u &&
         scratch->request_capacity != 0u && scratch->origins != NULL &&
         scratch->origin_capacity != 0u &&
         input->provider_limits.max_sources != 0u &&
         input->provider_limits.max_sources <=
             (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES &&
         input->provider_limits.max_source_bytes != 0u &&
         input->provider_limits.max_source_bytes <=
             (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCE_BYTES &&
         input->provider_limits.max_total_source_bytes != 0u &&
         input->provider_limits.max_total_source_bytes <=
             (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_TOTAL_SOURCE_BYTES &&
         input->provider_limits.max_path_bytes != 0u &&
         input->provider_limits.max_path_bytes <=
             (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES &&
         input->provider_limits.max_token_bytes != 0u &&
         input->provider_limits.max_token_bytes <=
             (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_TOKEN_BYTES &&
         input->max_edges <= (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_EDGES &&
         input->max_depth <= (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_DEPTH &&
         input->root_path.data != NULL && input->root_path.length != 0u &&
         input->root_path.length <= input->provider_limits.max_path_bytes &&
         input->root_source_id.data != NULL &&
         input->root_source_id.length != 0u &&
         input->root_source_id.length <=
             input->provider_limits.max_path_bytes;
}

static bool add_storage_snapshot_ranges(
    const acquisition_pipeline_snapshot *snapshot,
    acquisition_range *ranges, size_t *count) {
  if (snapshot == NULL || ranges == NULL || count == NULL ||
      !add_range(ranges, ACQUISITION_PIPELINE_RANGE_COUNT, count,
                 snapshot->input.storage, 1u,
                 sizeof(*snapshot->input.storage)))
    return false;
  for (size_t index = 0u;
       index < (size_t)W_SEED_ACQUISITION_MAX_SOURCES; index += 1u) {
    if (!add_range(ranges, ACQUISITION_PIPELINE_RANGE_COUNT, count,
                   snapshot->storage.staging_bytes[index],
                   snapshot->storage.staging_capacity[index],
                   sizeof(uint8_t)) ||
        !add_range(ranges, ACQUISITION_PIPELINE_RANGE_COUNT, count,
                   snapshot->storage.revalidation_bytes[index],
                   snapshot->storage.revalidation_capacity[index],
                   sizeof(uint8_t)) ||
        !add_range(ranges, ACQUISITION_PIPELINE_RANGE_COUNT, count,
                   snapshot->storage.published_bytes[index],
                   snapshot->storage.published_capacity[index],
                   sizeof(uint8_t)) ||
        !add_range(ranges, ACQUISITION_PIPELINE_RANGE_COUNT, count,
                   snapshot->storage.nodes[index],
                   snapshot->storage.node_capacity[index],
                   sizeof(w_seed_cst_node)))
      return false;
  }
  return true;
}

static bool add_pipeline_direct_ranges(
    const acquisition_pipeline_snapshot *snapshot,
    acquisition_range *ranges, size_t *count) {
  const w_seed_ephemeral_driver_input *driver = &snapshot->driver;
  const w_seed_ephemeral_driver_scratch *scratch = &snapshot->scratch;
  const w_seed_ephemeral_driver_output *output = &snapshot->output;
#define ADD_PIPELINE_OBJECT(pointer, type)                                  \
  if (!add_aligned_range(ranges, ACQUISITION_PIPELINE_RANGE_COUNT, count,   \
                         (pointer), 1u, sizeof(type), _Alignof(type)))       \
  return false
#define ADD_PIPELINE_ARRAY(pointer, length, type)                            \
  if (!add_aligned_range(ranges, ACQUISITION_PIPELINE_RANGE_COUNT, count,    \
                         (pointer), (length), sizeof(type), _Alignof(type)))  \
  return false
  ADD_PIPELINE_OBJECT(snapshot->input_address,
                      w_seed_acquisition_pipeline_input);
  ADD_PIPELINE_OBJECT(snapshot->input.driver_input,
                      w_seed_ephemeral_driver_input);
  ADD_PIPELINE_OBJECT(snapshot->input.scratch,
                      w_seed_ephemeral_driver_scratch);
  ADD_PIPELINE_OBJECT(snapshot->input.output, w_seed_ephemeral_driver_output);
  ADD_PIPELINE_ARRAY(driver->backend.context,
                     snapshot->input.backend_context_size,
                     unsigned char);
  ADD_PIPELINE_ARRAY(driver->root_path.data, driver->root_path.length,
                     uint8_t);
  ADD_PIPELINE_ARRAY(driver->root_source_id.data,
                     driver->root_source_id.length, char);
  ADD_PIPELINE_ARRAY(scratch->slots, scratch->slot_capacity,
                     w_seed_ephemeral_driver_slot);
  ADD_PIPELINE_ARRAY(scratch->requests, scratch->request_capacity,
                     w_seed_ephemeral_provider_request);
  ADD_PIPELINE_ARRAY(scratch->lexer_frames, scratch->lexer_frame_capacity,
                     w_seed_lexer_frame);
  ADD_PIPELINE_ARRAY(scratch->tokens, scratch->token_capacity,
                     w_seed_parse_token);
  ADD_PIPELINE_ARRAY(scratch->parse_frames, scratch->parse_frame_capacity,
                     w_seed_parse_frame);
  ADD_PIPELINE_ARRAY(scratch->issues, scratch->issue_capacity,
                     w_seed_parse_issue);
  ADD_PIPELINE_ARRAY(scratch->origins, scratch->origin_capacity,
                     w_seed_module_origin);
  ADD_PIPELINE_ARRAY(scratch->candidate_documents,
                     scratch->candidate_document_capacity,
                     w_seed_frontend_document);
  ADD_PIPELINE_ARRAY(scratch->candidate_facts,
                     scratch->candidate_fact_capacity,
                     w_seed_ephemeral_graph_provider_facts);
  ADD_PIPELINE_OBJECT(scratch->graph_scratch,
                      w_seed_ephemeral_graph_scratch);
  ADD_PIPELINE_ARRAY(output->documents, output->document_capacity,
                     w_seed_frontend_document);
  ADD_PIPELINE_ARRAY(output->graph.inventory,
                     output->graph.inventory_capacity,
                     w_seed_ephemeral_graph_inventory_item);
  ADD_PIPELINE_ARRAY(output->graph.edges, output->graph.edge_capacity,
                     w_seed_ephemeral_graph_edge);
  ADD_PIPELINE_ARRAY(output->graph.document_order,
                     output->graph.document_order_capacity, uint32_t);
  ADD_PIPELINE_ARRAY(output->graph.resolved_imports,
                     output->graph.resolved_import_capacity,
                     w_seed_frontend_resolved_import);
#undef ADD_PIPELINE_ARRAY
#undef ADD_PIPELINE_OBJECT
  return true;
}

static bool add_pipeline_nested_ranges(
    const acquisition_pipeline_snapshot *snapshot,
    acquisition_range *ranges, size_t *count) {
#define ADD_NESTED_ARRAY(pointer, length, type)                              \
  if (!add_aligned_range(ranges, ACQUISITION_PIPELINE_RANGE_COUNT, count,    \
                         (pointer), (length), sizeof(type), _Alignof(type)))  \
  return false
  const w_seed_ephemeral_graph_scratch *graph = &snapshot->graph;
  ADD_NESTED_ARRAY(graph->nodes, graph->node_capacity,
                   w_seed_ephemeral_graph_scratch_node);
  ADD_NESTED_ARRAY(graph->edges, graph->edge_capacity,
                   w_seed_ephemeral_graph_scratch_edge);
  ADD_NESTED_ARRAY(graph->sorted_nodes, graph->sorted_nodes_capacity, size_t);
  ADD_NESTED_ARRAY(graph->node_ordinals, graph->node_ordinals_capacity,
                   size_t);
  ADD_NESTED_ARRAY(graph->sorted_edges, graph->sorted_edges_capacity, size_t);
  ADD_NESTED_ARRAY(graph->sorted_resolved_edges,
                   graph->sorted_resolved_edges_capacity, size_t);
  ADD_NESTED_ARRAY(graph->origins, graph->origin_capacity,
                   w_seed_module_origin);
  ADD_NESTED_ARRAY(graph->indegree, graph->indegree_capacity, uint32_t);
  ADD_NESTED_ARRAY(graph->queue, graph->queue_capacity, uint32_t);
  ADD_NESTED_ARRAY(graph->depths, graph->depths_capacity, uint32_t);
#undef ADD_NESTED_ARRAY

  w_seed_ephemeral_driver_scratch scratch = snapshot->scratch;
  scratch.slots = (w_seed_ephemeral_driver_slot *)snapshot->slots;
  scratch.requests =
      (w_seed_ephemeral_provider_request *)snapshot->requests;
  return add_preserved_driver_ranges(
      &scratch, ranges, ACQUISITION_PIPELINE_RANGE_COUNT, count);
}

static acquisition_preflight_status pipeline_preflight(
    const w_seed_acquisition_pipeline_input *input,
    const w_seed_acquisition_pipeline_result *result) {
  acquisition_pipeline_snapshot snapshot;
  (void)memset(&snapshot, 0, sizeof(snapshot));
  acquisition_range result_range;
  if (!pipeline_top_snapshot(input, result, &snapshot, &result_range))
    return ACQUISITION_PREFLIGHT_UNPUBLISHABLE;
  if (!pipeline_direct_shape_valid(&snapshot))
    return ACQUISITION_PREFLIGHT_UNPUBLISHABLE;

  acquisition_range ranges[ACQUISITION_PIPELINE_RANGE_COUNT];
  size_t count = 0u;
  if (!add_storage_snapshot_ranges(&snapshot, ranges, &count) ||
      !add_pipeline_direct_ranges(&snapshot, ranges, &count) ||
      !ranges_disjoint(ranges, count) ||
      !result_disjoint_from_ranges(result_range, ranges, count))
    return ACQUISITION_PREFLIGHT_UNPUBLISHABLE;

  (void)memcpy(&snapshot.graph, snapshot.scratch.graph_scratch,
               sizeof(snapshot.graph));
  if (snapshot.scratch.slot_capacity != 0u)
    (void)memcpy(snapshot.slots, snapshot.scratch.slots,
                 snapshot.scratch.slot_capacity * sizeof(snapshot.slots[0]));
  if (snapshot.scratch.request_capacity != 0u)
    (void)memcpy(snapshot.requests, snapshot.scratch.requests,
                 snapshot.scratch.request_capacity *
                     sizeof(snapshot.requests[0]));
  if (!pipeline_nested_snapshot_valid(&snapshot) ||
      !add_pipeline_nested_ranges(&snapshot, ranges, &count) ||
      !ranges_disjoint(ranges, count) ||
      !result_disjoint_from_ranges(result_range, ranges, count))
    return ACQUISITION_PREFLIGHT_UNPUBLISHABLE;

  if (!storage_valid(snapshot.input.storage) ||
      snapshot.input.storage->pipeline_active ||
      !pipeline_limits_valid(&snapshot.driver, &snapshot.scratch))
    return ACQUISITION_PREFLIGHT_INVALID;
  return ACQUISITION_PREFLIGHT_OK;
}

static w_seed_ephemeral_driver_result driver_not_run(void) {
  w_seed_ephemeral_driver_result result;
  (void)memset(&result, 0, sizeof(result));
  result.status = W_SEED_EPHEMERAL_DRIVER_INVALID;
  result.failure = W_SEED_EPHEMERAL_DRIVER_FAILURE_NONE;
  result.phase = W_SEED_EPHEMERAL_DRIVER_PHASE_NONE;
  result.round = SIZE_MAX;
  result.candidate_index = SIZE_MAX;
  result.origin_index = SIZE_MAX;
  result.document_index = SIZE_MAX;
  result.capacity_field = W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_NONE;
  result.provider_status = W_SEED_EPHEMERAL_PROVIDER_INVALID;
  result.provider_result.status = W_SEED_EPHEMERAL_PROVIDER_INVALID;
  result.provider_result.request_index = SIZE_MAX;
  result.parser_status = W_SEED_PARSE_FATAL;
  result.scan_status = W_SEED_MODULE_SCAN_INVALID;
  result.graph_status = W_SEED_EPHEMERAL_GRAPH_INVALID;
  result.graph_result.status = W_SEED_EPHEMERAL_GRAPH_INVALID;
  result.graph_result.candidate_index = SIZE_MAX;
  result.graph_result.document_ordinal = SIZE_MAX;
  result.graph_result.edge_ordinal = SIZE_MAX;
  return result;
}

static w_seed_acquisition_retry_outcome retry_not_run(void) {
  return retry_outcome(W_SEED_ACQUISITION_RETRY_NOT_RUN,
                       W_SEED_ACQUISITION_RETRY_ACTION_NOT_RUN,
                       W_SEED_ACQUISITION_RETRY_DETAIL_NONE,
                       "retry not run");
}

static w_seed_acquisition_pipeline_result pipeline_result_initial(void) {
  w_seed_acquisition_pipeline_result result;
  (void)memset(&result, 0, sizeof(result));
  result.status = W_SEED_ACQUISITION_PIPELINE_INVALID;
  result.driver_result = driver_not_run();
  result.retry = retry_not_run();
  return result;
}

static w_seed_acquisition_pipeline_status map_driver_status(
    w_seed_ephemeral_driver_status status) {
  switch (status) {
    case W_SEED_EPHEMERAL_DRIVER_OK:
      return W_SEED_ACQUISITION_PIPELINE_OK;
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY:
      return W_SEED_ACQUISITION_PIPELINE_CAPACITY;
    case W_SEED_EPHEMERAL_DRIVER_UNSUPPORTED:
      return W_SEED_ACQUISITION_PIPELINE_UNSUPPORTED;
    case W_SEED_EPHEMERAL_DRIVER_IO:
      return W_SEED_ACQUISITION_PIPELINE_IO;
    case W_SEED_EPHEMERAL_DRIVER_INVALID:
    default:
      return W_SEED_ACQUISITION_PIPELINE_INVALID;
  }
}

static w_seed_acquisition_pipeline_status publish_pipeline_result(
    w_seed_acquisition_pipeline_result *result,
    w_seed_acquisition_pipeline_result value) {
  (void)memcpy(result, &value, sizeof(value));
  return value.status;
}

w_seed_acquisition_pipeline_status w_seed_acquisition_pipeline_run(
    const w_seed_acquisition_pipeline_input *input,
    w_seed_acquisition_pipeline_result *result) {
  w_seed_acquisition_pipeline_result local = pipeline_result_initial();
  for (;;) {
    const acquisition_preflight_status preflight =
        pipeline_preflight(input, result);
    if (preflight == ACQUISITION_PREFLIGHT_UNPUBLISHABLE)
      return W_SEED_ACQUISITION_PIPELINE_INVALID;
    if (preflight == ACQUISITION_PREFLIGHT_INVALID)
      return publish_pipeline_result(result, local);
    if (!w_seed_acquisition_storage_bind_driver(input->storage,
                                                 input->scratch)) {
      local.status = W_SEED_ACQUISITION_PIPELINE_FAULT;
      return publish_pipeline_result(result, local);
    }

    w_seed_ephemeral_driver_result driver_result = driver_not_run();
    input->storage->pipeline_active = true;
    const w_seed_ephemeral_driver_status driver_status =
        w_seed_ephemeral_driver_run(input->driver_input, input->scratch,
                                    input->output, &driver_result);
    input->storage->pipeline_active = false;
    local.attempts += 1u;
    local.driver_result = driver_result;
    if (driver_status == W_SEED_EPHEMERAL_DRIVER_OK) {
      local.status = W_SEED_ACQUISITION_PIPELINE_OK;
      local.document_count = input->output->document_count;
      local.graph_written = driver_result.graph_result.written;
      return publish_pipeline_result(result, local);
    }
    if (driver_status != W_SEED_EPHEMERAL_DRIVER_CAPACITY) {
      local.status = map_driver_status(driver_status);
      return publish_pipeline_result(result, local);
    }
    if (local.attempts >= (size_t)W_SEED_ACQUISITION_MAX_ATTEMPTS) {
      local.retry = retry_outcome(
          W_SEED_ACQUISITION_RETRY_CAPACITY,
          W_SEED_ACQUISITION_RETRY_TERMINAL,
          W_SEED_ACQUISITION_RETRY_DETAIL_RETRY_LIMIT,
          "acquisition retry limit exhausted");
      local.status = W_SEED_ACQUISITION_PIPELINE_CAPACITY;
      return publish_pipeline_result(result, local);
    }
    local.retry = w_seed_acquisition_retry_apply(
        input->storage, driver_status, &driver_result);
    if (local.retry.status == W_SEED_ACQUISITION_RETRY_OK &&
        local.retry.action == W_SEED_ACQUISITION_RETRY_RETRY)
      continue;
    if (local.retry.status == W_SEED_ACQUISITION_RETRY_ALLOCATION)
      local.status = W_SEED_ACQUISITION_PIPELINE_ALLOCATION;
    else if (local.retry.status == W_SEED_ACQUISITION_RETRY_CAPACITY)
      local.status = W_SEED_ACQUISITION_PIPELINE_CAPACITY;
    else
      local.status = W_SEED_ACQUISITION_PIPELINE_FAULT;
    return publish_pipeline_result(result, local);
  }
}
