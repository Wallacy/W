#include "check_storage.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
  void *pointer;
  bool allocated;
} storage_replacement;

static size_t storage_max_source_bytes(void) {
  return (size_t)W_SEED_CHECK_STORAGE_MAX_SOURCE_BYTES;
}

static size_t storage_max_total_source_bytes(void) {
  return (size_t)W_SEED_CHECK_STORAGE_MAX_TOTAL_SOURCE_BYTES;
}

static size_t storage_max_nodes(void) {
  return (size_t)W_SEED_CHECK_STORAGE_MAX_NODES;
}

static size_t storage_max_total_nodes(void) {
  return (size_t)W_SEED_CHECK_STORAGE_MAX_TOTAL_NODES;
}

static size_t storage_max_json_bytes(void) {
  return (size_t)W_SEED_CHECK_STORAGE_MAX_JSON_BYTES;
}

static bool storage_index_valid(size_t request_index) {
  return request_index < (size_t)W_SEED_CHECK_STORAGE_MAX_SOURCES;
}

static bool storage_pointer_capacity_valid(const void *pointer,
                                           size_t capacity) {
  return (capacity == 0u && pointer == NULL) ||
         (capacity != 0u && pointer != NULL);
}

static bool storage_add_bounded(size_t left, size_t right, size_t maximum,
                                size_t *sum) {
  if (sum == NULL || left > maximum || right > maximum - left) return false;
  *sum = left + right;
  return true;
}

static bool storage_node_bytes(size_t count, size_t *bytes) {
  if (bytes == NULL || count > SIZE_MAX / sizeof(w_seed_cst_node))
    return false;
  *bytes = count * sizeof(w_seed_cst_node);
  return true;
}

static bool storage_matches_totals(const w_seed_check_storage *storage) {
  const size_t source_maximum = storage_max_source_bytes();
  const size_t source_total_maximum = storage_max_total_source_bytes();
  const size_t node_maximum = storage_max_nodes();
  const size_t node_total_maximum = storage_max_total_nodes();
  size_t staging_total = 0u;
  size_t revalidation_total = 0u;
  size_t published_total = 0u;
  size_t node_total = 0u;

  if (storage == NULL || !storage->initialized || storage->allocate == NULL ||
      storage->deallocate == NULL) {
    return false;
  }
  if (!storage_pointer_capacity_valid(storage->json_staging,
                                      storage->json_staging_capacity) ||
      !storage_pointer_capacity_valid(storage->json_final,
                                      storage->json_final_capacity) ||
      storage->json_staging_capacity != storage->json_final_capacity ||
      storage->json_staging_capacity > storage_max_json_bytes()) {
    return false;
  }
  for (size_t index = 0u;
       index < (size_t)W_SEED_CHECK_STORAGE_MAX_SOURCES; index += 1u) {
    if (!storage_pointer_capacity_valid(storage->staging_bytes[index],
                                        storage->staging_capacity[index]) ||
        !storage_pointer_capacity_valid(
            storage->revalidation_bytes[index],
            storage->revalidation_capacity[index]) ||
        !storage_pointer_capacity_valid(storage->published_bytes[index],
                                        storage->published_capacity[index]) ||
        !storage_pointer_capacity_valid(storage->nodes[index],
                                        storage->node_capacity[index]) ||
        storage->staging_capacity[index] > source_maximum ||
        storage->revalidation_capacity[index] > source_maximum ||
        storage->published_capacity[index] > source_maximum ||
        storage->node_capacity[index] > node_maximum) {
      return false;
    }
    if (!storage_add_bounded(staging_total, storage->staging_capacity[index],
                             source_total_maximum, &staging_total) ||
        !storage_add_bounded(revalidation_total,
                             storage->revalidation_capacity[index],
                             source_total_maximum, &revalidation_total) ||
        !storage_add_bounded(published_total,
                             storage->published_capacity[index],
                             source_total_maximum, &published_total) ||
        !storage_add_bounded(node_total, storage->node_capacity[index],
                             node_total_maximum, &node_total)) {
      return false;
    }
  }
  return staging_total == storage->staging_total &&
         revalidation_total == storage->revalidation_total &&
         published_total == storage->published_total &&
         node_total == storage->node_total;
}

static bool storage_next_total(size_t current_total, size_t old_capacity,
                               size_t next_capacity, size_t maximum,
                               size_t *next_total) {
  if (current_total < old_capacity || old_capacity > maximum ||
      next_capacity > maximum) {
    return false;
  }
  return storage_add_bounded(current_total - old_capacity, next_capacity,
                             maximum, next_total);
}

static size_t storage_maximum(size_t left, size_t right) {
  return left > right ? left : right;
}

static bool storage_round_capacity(size_t current_capacity,
                                   size_t required_capacity, size_t maximum,
                                   size_t *rounded_capacity) {
  if (rounded_capacity == NULL || current_capacity > maximum ||
      required_capacity > maximum) {
    return false;
  }
  if (required_capacity <= current_capacity) {
    *rounded_capacity = current_capacity;
    return true;
  }

  size_t candidate = current_capacity == 0u ? 1u : current_capacity;
  size_t steps = 0u;
  while (candidate < required_capacity &&
         steps < W_SEED_CHECK_STORAGE_MAX_GROWTH_STEPS) {
    steps += 1u;
    if (candidate > maximum / 2u) {
      candidate = maximum;
      break;
    }
    candidate *= 2u;
  }
  if (candidate < required_capacity) return false;
  *rounded_capacity = candidate;
  return true;
}

static bool storage_prepare_replacement(
    const void *old_pointer, size_t old_bytes, size_t next_bytes,
    w_seed_check_storage_allocate_fn allocate,
    storage_replacement *replacement) {
  if (replacement == NULL || allocate == NULL ||
      !storage_pointer_capacity_valid(old_pointer, old_bytes)) {
    return false;
  }
  replacement->pointer = (void *)old_pointer;
  replacement->allocated = false;
  if (next_bytes == old_bytes) return true;
  if (next_bytes == 0u) {
    replacement->pointer = NULL;
    return true;
  }
  replacement->pointer = allocate(next_bytes);
  if (replacement->pointer == NULL) return false;
  replacement->allocated = true;
  if (old_bytes != 0u) {
    (void)memcpy(replacement->pointer, old_pointer, old_bytes);
  }
  return true;
}

static void storage_discard_replacement(
    storage_replacement *replacement,
    w_seed_check_storage_deallocate_fn deallocate) {
  if (replacement == NULL) return;
  if (replacement->allocated && deallocate != NULL) {
    deallocate(replacement->pointer);
  }
  replacement->pointer = NULL;
  replacement->allocated = false;
}

bool w_seed_check_storage_init_with_allocator(
    w_seed_check_storage *storage,
    w_seed_check_storage_allocate_fn allocate,
    w_seed_check_storage_deallocate_fn deallocate) {
  if (storage == NULL || allocate == NULL || deallocate == NULL) return false;
  (void)memset(storage, 0, sizeof(*storage));
  storage->allocate = allocate;
  storage->deallocate = deallocate;
  storage->initialized = true;
  return true;
}

bool w_seed_check_storage_init(w_seed_check_storage *storage) {
  return w_seed_check_storage_init_with_allocator(storage, malloc, free);
}

w_seed_check_storage_status w_seed_check_storage_grow(
    w_seed_check_storage *storage, size_t request_index,
    size_t required_capacity) {
  if (!storage_index_valid(request_index) ||
      !storage_matches_totals(storage)) {
    return W_SEED_CHECK_STORAGE_INVALID;
  }
  if (required_capacity > storage_max_source_bytes()) {
    return W_SEED_CHECK_STORAGE_CAPACITY;
  }

  const size_t old_staging = storage->staging_capacity[request_index];
  const size_t old_revalidation =
      storage->revalidation_capacity[request_index];
  const size_t old_published = storage->published_capacity[request_index];
  const size_t current_capacity =
      storage_maximum(old_staging,
                      storage_maximum(old_revalidation, old_published));
  size_t rounded_capacity = 0u;
  if (!storage_round_capacity(current_capacity, required_capacity,
                              storage_max_source_bytes(), &rounded_capacity)) {
    return W_SEED_CHECK_STORAGE_CAPACITY;
  }
  size_t next_staging = storage_maximum(old_staging, rounded_capacity);
  size_t next_revalidation =
      storage_maximum(old_revalidation, rounded_capacity);
  size_t next_published = storage_maximum(old_published, rounded_capacity);
  size_t next_staging_total = 0u;
  size_t next_revalidation_total = 0u;
  size_t next_published_total = 0u;
  if (!storage_next_total(storage->staging_total, old_staging, next_staging,
                          storage_max_total_source_bytes(),
                          &next_staging_total) ||
      !storage_next_total(storage->revalidation_total, old_revalidation,
                          next_revalidation,
                          storage_max_total_source_bytes(),
                          &next_revalidation_total) ||
      !storage_next_total(storage->published_total, old_published,
                          next_published, storage_max_total_source_bytes(),
                          &next_published_total)) {
    /* A geometric target can exceed the remaining aggregate budget while the
     * exact provider requirement still fits.  Keep the hard bound without
     * turning that valid request into a false capacity failure. */
    next_staging = storage_maximum(old_staging, required_capacity);
    next_revalidation = storage_maximum(old_revalidation, required_capacity);
    next_published = storage_maximum(old_published, required_capacity);
    if (!storage_next_total(storage->staging_total, old_staging,
                            next_staging, storage_max_total_source_bytes(),
                            &next_staging_total) ||
        !storage_next_total(storage->revalidation_total, old_revalidation,
                            next_revalidation,
                            storage_max_total_source_bytes(),
                            &next_revalidation_total) ||
        !storage_next_total(storage->published_total, old_published,
                            next_published, storage_max_total_source_bytes(),
                            &next_published_total)) {
      return W_SEED_CHECK_STORAGE_CAPACITY;
    }
  }

  storage_replacement staging = {NULL, false};
  storage_replacement revalidation = {NULL, false};
  storage_replacement published = {NULL, false};
  if (!storage_prepare_replacement(storage->staging_bytes[request_index],
                                   old_staging, next_staging,
                                   storage->allocate, &staging) ||
      !storage_prepare_replacement(
          storage->revalidation_bytes[request_index], old_revalidation,
          next_revalidation, storage->allocate, &revalidation) ||
      !storage_prepare_replacement(storage->published_bytes[request_index],
                                   old_published, next_published,
                                   storage->allocate, &published)) {
    storage_discard_replacement(&staging, storage->deallocate);
    storage_discard_replacement(&revalidation, storage->deallocate);
    storage_discard_replacement(&published, storage->deallocate);
    return W_SEED_CHECK_STORAGE_ALLOCATION;
  }

  if (staging.allocated && storage->staging_bytes[request_index] != NULL) {
    storage->deallocate(storage->staging_bytes[request_index]);
  }
  if (revalidation.allocated &&
      storage->revalidation_bytes[request_index] != NULL) {
    storage->deallocate(storage->revalidation_bytes[request_index]);
  }
  if (published.allocated && storage->published_bytes[request_index] != NULL) {
    storage->deallocate(storage->published_bytes[request_index]);
  }
  storage->staging_bytes[request_index] =
      (uint8_t *)staging.pointer;
  storage->revalidation_bytes[request_index] =
      (uint8_t *)revalidation.pointer;
  storage->published_bytes[request_index] =
      (uint8_t *)published.pointer;
  storage->staging_capacity[request_index] = next_staging;
  storage->revalidation_capacity[request_index] = next_revalidation;
  storage->published_capacity[request_index] = next_published;
  storage->staging_total = next_staging_total;
  storage->revalidation_total = next_revalidation_total;
  storage->published_total = next_published_total;
  return W_SEED_CHECK_STORAGE_OK;
}

w_seed_check_storage_status w_seed_check_storage_grow_nodes(
    w_seed_check_storage *storage, size_t request_index,
    size_t required_capacity) {
  if (!storage_index_valid(request_index) ||
      !storage_matches_totals(storage)) {
    return W_SEED_CHECK_STORAGE_INVALID;
  }
  if (required_capacity > storage_max_nodes()) {
    return W_SEED_CHECK_STORAGE_CAPACITY;
  }

  const size_t old_capacity = storage->node_capacity[request_index];
  size_t rounded_capacity = 0u;
  if (!storage_round_capacity(old_capacity, required_capacity,
                              storage_max_nodes(), &rounded_capacity)) {
    return W_SEED_CHECK_STORAGE_CAPACITY;
  }
  const size_t next_capacity =
      storage_maximum(old_capacity, rounded_capacity);
  size_t next_total = 0u;
  if (!storage_next_total(storage->node_total, old_capacity, next_capacity,
                          storage_max_total_nodes(), &next_total)) {
    return W_SEED_CHECK_STORAGE_CAPACITY;
  }

  size_t old_bytes = 0u;
  size_t next_bytes = 0u;
  if (!storage_node_bytes(old_capacity, &old_bytes) ||
      !storage_node_bytes(next_capacity, &next_bytes)) {
    return W_SEED_CHECK_STORAGE_CAPACITY;
  }
  storage_replacement replacement = {NULL, false};
  if (!storage_prepare_replacement(storage->nodes[request_index], old_bytes,
                                   next_bytes, storage->allocate,
                                   &replacement)) {
    storage_discard_replacement(&replacement, storage->deallocate);
    return W_SEED_CHECK_STORAGE_ALLOCATION;
  }
  if (replacement.allocated && storage->nodes[request_index] != NULL) {
    storage->deallocate(storage->nodes[request_index]);
  }
  storage->nodes[request_index] = (w_seed_cst_node *)replacement.pointer;
  storage->node_capacity[request_index] = next_capacity;
  storage->node_total = next_total;
  return W_SEED_CHECK_STORAGE_OK;
}

w_seed_check_storage_status w_seed_check_storage_grow_json(
    w_seed_check_storage *storage, size_t required_capacity) {
  if (!storage_matches_totals(storage))
    return W_SEED_CHECK_STORAGE_INVALID;
  if (required_capacity > storage_max_json_bytes())
    return W_SEED_CHECK_STORAGE_CAPACITY;

  const size_t old_capacity = storage->json_staging_capacity;
  if (storage->json_final_capacity != old_capacity)
    return W_SEED_CHECK_STORAGE_INVALID;
  size_t next_capacity = 0u;
  if (!storage_round_capacity(old_capacity, required_capacity,
                             storage_max_json_bytes(), &next_capacity))
    return W_SEED_CHECK_STORAGE_CAPACITY;
  if (next_capacity == old_capacity) return W_SEED_CHECK_STORAGE_OK;

  storage_replacement staging = {NULL, false};
  storage_replacement final = {NULL, false};
  if (!storage_prepare_replacement(storage->json_staging, old_capacity,
                                   next_capacity, storage->allocate,
                                   &staging) ||
      !storage_prepare_replacement(storage->json_final, old_capacity,
                                   next_capacity, storage->allocate, &final)) {
    storage_discard_replacement(&staging, storage->deallocate);
    storage_discard_replacement(&final, storage->deallocate);
    return W_SEED_CHECK_STORAGE_ALLOCATION;
  }
  if (staging.allocated && storage->json_staging != NULL)
    storage->deallocate(storage->json_staging);
  if (final.allocated && storage->json_final != NULL)
    storage->deallocate(storage->json_final);
  storage->json_staging = (uint8_t *)staging.pointer;
  storage->json_final = (uint8_t *)final.pointer;
  storage->json_staging_capacity = next_capacity;
  storage->json_final_capacity = next_capacity;
  return W_SEED_CHECK_STORAGE_OK;
}

bool w_seed_check_storage_bind_request(
    const w_seed_check_storage *storage, size_t request_index,
    w_seed_ephemeral_provider_request *request) {
  if (request == NULL || !storage_index_valid(request_index) ||
      !storage_matches_totals(storage)) {
    return false;
  }
  request->staging_bytes = storage->staging_bytes[request_index];
  request->staging_capacity = storage->staging_capacity[request_index];
  request->revalidation_bytes = storage->revalidation_bytes[request_index];
  request->revalidation_capacity =
      storage->revalidation_capacity[request_index];
  request->bytes = storage->published_bytes[request_index];
  request->byte_capacity = storage->published_capacity[request_index];
  return true;
}

bool w_seed_check_storage_bind_slot(const w_seed_check_storage *storage,
                                    size_t request_index,
                                    w_seed_ephemeral_driver_slot *slot) {
  if (slot == NULL || !storage_index_valid(request_index) ||
      !storage_matches_totals(storage)) {
    return false;
  }
  slot->nodes = storage->nodes[request_index];
  slot->node_capacity = storage->node_capacity[request_index];
  return true;
}

void w_seed_check_storage_destroy(w_seed_check_storage *storage) {
  if (storage == NULL) return;
  for (size_t index = 0u;
       index < (size_t)W_SEED_CHECK_STORAGE_MAX_SOURCES; index += 1u) {
    if (storage->deallocate != NULL) {
      if (storage->staging_bytes[index] != NULL) {
        storage->deallocate(storage->staging_bytes[index]);
      }
      if (storage->revalidation_bytes[index] != NULL) {
        storage->deallocate(storage->revalidation_bytes[index]);
      }
      if (storage->published_bytes[index] != NULL) {
        storage->deallocate(storage->published_bytes[index]);
      }
      if (storage->nodes[index] != NULL) {
        storage->deallocate(storage->nodes[index]);
      }
    }
  }
  if (storage->deallocate != NULL) {
    if (storage->json_staging != NULL)
      storage->deallocate(storage->json_staging);
    if (storage->json_final != NULL)
      storage->deallocate(storage->json_final);
  }
  (void)memset(storage, 0, sizeof(*storage));
}
