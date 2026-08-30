#include "w_seed_acquisition.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                     \
  do {                                                                       \
    if (!(condition)) {                                                      \
      (void)fprintf(stderr, "acquisition test failed: %s (%s:%d)\n",       \
                    #condition, __FILE__, __LINE__);                         \
      return false;                                                          \
    }                                                                        \
  } while (0)

typedef struct {
  size_t allocations;
  size_t deallocations;
  size_t fail_after;
} allocator_probe;

static allocator_probe probe;

typedef struct {
  size_t allocations;
  size_t deallocations;
  size_t duplicate_at;
  void *last_allocation;
} malicious_probe;

static malicious_probe malicious;

static void reset_malicious(size_t duplicate_at) {
  malicious = (malicious_probe){0u, 0u, duplicate_at, NULL};
}

static void *malicious_allocate(size_t size) {
  const size_t call = malicious.allocations;
  malicious.allocations += 1u;
  if (call == malicious.duplicate_at) return malicious.last_allocation;
  malicious.last_allocation = malloc(size);
  return malicious.last_allocation;
}

static void malicious_deallocate(void *pointer) {
  malicious.deallocations += 1u;
  free(pointer);
}

typedef union {
  max_align_t alignment;
  unsigned char bytes[128u];
} adjacent_arena;

static adjacent_arena adjacent_storage;
static size_t adjacent_offset;
static size_t adjacent_deallocations;

static void *adjacent_allocate(size_t size) {
  if (size == 0u || size > sizeof(adjacent_storage.bytes) - adjacent_offset)
    return NULL;
  void *result = &adjacent_storage.bytes[adjacent_offset];
  adjacent_offset += size;
  return result;
}

static void adjacent_deallocate(void *pointer) {
  if (pointer != NULL) adjacent_deallocations += 1u;
}

static void reset_probe(size_t fail_after) {
  probe.allocations = 0u;
  probe.deallocations = 0u;
  probe.fail_after = fail_after;
}

static void *probe_allocate(size_t size) {
  if (probe.allocations >= probe.fail_after) return NULL;
  probe.allocations += 1u;
  return malloc(size);
}

static void probe_deallocate(void *pointer) {
  probe.deallocations += 1u;
  free(pointer);
}

static bool bytes_are_zero(const void *object, size_t size) {
  const unsigned char *bytes = (const unsigned char *)object;
  for (size_t index = 0u; index < size; index += 1u) {
    if (bytes[index] != 0u) return false;
  }
  return true;
}

static bool test_init_live_destroy_reinit(void) {
  w_seed_acquisition_storage storage = {0};
  CHECK(w_seed_acquisition_storage_init(&storage));
  w_seed_acquisition_storage before = storage;
  CHECK(!w_seed_acquisition_storage_init(&storage));
  CHECK(memcmp(&storage, &before, sizeof(storage)) == 0);
  CHECK(!w_seed_acquisition_storage_init_with_allocator(
      &storage, probe_allocate, probe_deallocate));
  CHECK(memcmp(&storage, &before, sizeof(storage)) == 0);
  w_seed_acquisition_storage_destroy(&storage);
  CHECK(bytes_are_zero(&storage, sizeof(storage)));
  w_seed_acquisition_storage_destroy(&storage);
  CHECK(bytes_are_zero(&storage, sizeof(storage)));
  CHECK(w_seed_acquisition_storage_init(&storage));
  w_seed_acquisition_storage_destroy(&storage);

  (void)memset(&storage, 0xA5, sizeof(storage));
  before = storage;
  CHECK(!w_seed_acquisition_storage_init(&storage));
  CHECK(memcmp(&storage, &before, sizeof(storage)) == 0);
  return true;
}

static bool test_source_allocation_rollback(void) {
  for (size_t failed = 0u; failed < 3u; failed += 1u) {
    w_seed_acquisition_storage storage = {0};
    reset_probe(failed);
    CHECK(w_seed_acquisition_storage_init_with_allocator(
        &storage, probe_allocate, probe_deallocate));
    CHECK(w_seed_acquisition_storage_grow_source(&storage, 0u, 4u) ==
          W_SEED_ACQUISITION_STORAGE_ALLOCATION);
    CHECK(storage.staging_bytes[0u] == NULL &&
          storage.revalidation_bytes[0u] == NULL &&
          storage.published_bytes[0u] == NULL);
    CHECK(storage.staging_capacity[0u] == 0u &&
          storage.revalidation_capacity[0u] == 0u &&
          storage.published_capacity[0u] == 0u);
    CHECK(storage.staging_total == 0u && storage.revalidation_total == 0u &&
          storage.published_total == 0u);
    CHECK(probe.allocations == failed && probe.deallocations == failed);
    w_seed_acquisition_storage_destroy(&storage);
    CHECK(bytes_are_zero(&storage, sizeof(storage)));
  }

  for (size_t failed = 0u; failed < 3u; failed += 1u) {
    w_seed_acquisition_storage storage = {0};
    reset_probe(SIZE_MAX);
    CHECK(w_seed_acquisition_storage_init_with_allocator(
        &storage, probe_allocate, probe_deallocate));
    CHECK(w_seed_acquisition_storage_grow_source(&storage, 0u, 4u) ==
          W_SEED_ACQUISITION_STORAGE_OK);
    for (size_t index = 0u; index < 4u; index += 1u) {
      storage.staging_bytes[0u][index] = (uint8_t)(index + 1u);
      storage.revalidation_bytes[0u][index] = (uint8_t)(index + 11u);
      storage.published_bytes[0u][index] = (uint8_t)(index + 21u);
    }
    uint8_t *old_staging = storage.staging_bytes[0u];
    uint8_t *old_revalidation = storage.revalidation_bytes[0u];
    uint8_t *old_published = storage.published_bytes[0u];
    const size_t base_allocations = probe.allocations;
    const size_t base_deallocations = probe.deallocations;
    probe.fail_after = base_allocations + failed;
    CHECK(w_seed_acquisition_storage_grow_source(&storage, 0u, 5u) ==
          W_SEED_ACQUISITION_STORAGE_ALLOCATION);
    CHECK(storage.staging_bytes[0u] == old_staging &&
          storage.revalidation_bytes[0u] == old_revalidation &&
          storage.published_bytes[0u] == old_published);
    CHECK(storage.staging_capacity[0u] == 4u &&
          storage.revalidation_capacity[0u] == 4u &&
          storage.published_capacity[0u] == 4u);
    CHECK(probe.deallocations == base_deallocations + failed);
    for (size_t index = 0u; index < 4u; index += 1u) {
      CHECK(storage.staging_bytes[0u][index] == (uint8_t)(index + 1u));
      CHECK(storage.revalidation_bytes[0u][index] ==
            (uint8_t)(index + 11u));
      CHECK(storage.published_bytes[0u][index] ==
            (uint8_t)(index + 21u));
    }
    w_seed_acquisition_storage_destroy(&storage);
    CHECK(probe.deallocations == base_deallocations + failed + 3u);
  }
  return true;
}

static bool test_node_rollback_and_bounds(void) {
  w_seed_acquisition_storage storage = {0};
  reset_probe(SIZE_MAX);
  CHECK(w_seed_acquisition_storage_init_with_allocator(
      &storage, probe_allocate, probe_deallocate));
  CHECK(w_seed_acquisition_storage_grow_nodes(&storage, 0u, 4u) ==
        W_SEED_ACQUISITION_STORAGE_OK);
  (void)memset(storage.nodes[0u], 0x5A,
               4u * sizeof(storage.nodes[0u][0]));
  w_seed_cst_node *old_nodes = storage.nodes[0u];
  probe.fail_after = probe.allocations;
  CHECK(w_seed_acquisition_storage_grow_nodes(&storage, 0u, 5u) ==
        W_SEED_ACQUISITION_STORAGE_ALLOCATION);
  CHECK(storage.nodes[0u] == old_nodes && storage.node_capacity[0u] == 4u &&
        storage.node_total == 4u);
  for (size_t index = 0u; index < 4u * sizeof(w_seed_cst_node); index += 1u)
    CHECK(((const uint8_t *)storage.nodes[0u])[index] == 0x5Au);
  CHECK(w_seed_acquisition_storage_grow_nodes(
            &storage, 0u, (size_t)W_SEED_ACQUISITION_MAX_NODES + 1u) ==
        W_SEED_ACQUISITION_STORAGE_CAPACITY);
  CHECK(storage.nodes[0u] == old_nodes && storage.node_total == 4u);
  w_seed_acquisition_storage_destroy(&storage);
  return true;
}

static bool invalid_storage_is_preserved(
    w_seed_acquisition_storage *storage) {
  if (storage == NULL) return false;
  const w_seed_acquisition_storage before = *storage;
  const size_t deallocations = probe.deallocations;
  if (w_seed_acquisition_storage_grow_source(storage, 0u, 8u) !=
          W_SEED_ACQUISITION_STORAGE_INVALID ||
      memcmp(storage, &before, sizeof(*storage)) != 0)
    return false;
  w_seed_acquisition_storage_destroy(storage);
  return memcmp(storage, &before, sizeof(*storage)) == 0 &&
         probe.deallocations == deallocations;
}

static bool test_owner_alias_and_invalid_destroy(void) {
  w_seed_acquisition_storage storage = {0};
  reset_probe(SIZE_MAX);
  CHECK(w_seed_acquisition_storage_init_with_allocator(
      &storage, probe_allocate, probe_deallocate));
  CHECK(w_seed_acquisition_storage_grow_source(&storage, 0u, 4u) ==
        W_SEED_ACQUISITION_STORAGE_OK);
  CHECK(w_seed_acquisition_storage_grow_source(&storage, 1u, 4u) ==
        W_SEED_ACQUISITION_STORAGE_OK);
  CHECK(w_seed_acquisition_storage_grow_nodes(&storage, 0u, 2u) ==
        W_SEED_ACQUISITION_STORAGE_OK);

  w_seed_acquisition_storage copied = storage;
  const w_seed_acquisition_storage copied_before = copied;
  const size_t copied_deallocations = probe.deallocations;
  CHECK(w_seed_acquisition_storage_grow_nodes(&copied, 0u, 3u) ==
        W_SEED_ACQUISITION_STORAGE_INVALID);
  w_seed_acquisition_storage_destroy(&copied);
  CHECK(memcmp(&copied, &copied_before, sizeof(copied)) == 0 &&
        probe.deallocations == copied_deallocations);

  uint8_t *saved = storage.revalidation_bytes[0u];
  storage.revalidation_bytes[0u] = storage.staging_bytes[0u];
  CHECK(invalid_storage_is_preserved(&storage));
  storage.revalidation_bytes[0u] = saved;

  saved = storage.revalidation_bytes[0u];
  storage.revalidation_bytes[0u] = storage.staging_bytes[0u] + 1u;
  CHECK(invalid_storage_is_preserved(&storage));
  storage.revalidation_bytes[0u] = saved;

  saved = storage.staging_bytes[1u];
  storage.staging_bytes[1u] = storage.staging_bytes[0u];
  CHECK(invalid_storage_is_preserved(&storage));
  storage.staging_bytes[1u] = saved;

  saved = storage.published_bytes[0u];
  storage.published_bytes[0u] = (uint8_t *)storage.nodes[0u];
  CHECK(invalid_storage_is_preserved(&storage));
  storage.published_bytes[0u] = saved;

  saved = storage.published_bytes[0u];
  storage.published_bytes[0u] = (uint8_t *)(UINTPTR_MAX - (uintptr_t)1u);
  CHECK(invalid_storage_is_preserved(&storage));
  storage.published_bytes[0u] = saved;

  saved = storage.published_bytes[0u];
  storage.published_bytes[0u] = (uint8_t *)&storage;
  CHECK(invalid_storage_is_preserved(&storage));
  storage.published_bytes[0u] = saved;

  w_seed_acquisition_storage_destroy(&storage);
  CHECK(bytes_are_zero(&storage, sizeof(storage)));
  return true;
}

static bool test_allocator_overlap_and_adjacency(void) {
  w_seed_acquisition_storage storage = {0};
  reset_malicious(SIZE_MAX);
  CHECK(w_seed_acquisition_storage_init_with_allocator(
      &storage, malicious_allocate, malicious_deallocate));
  CHECK(w_seed_acquisition_storage_grow_source(&storage, 0u, 4u) ==
        W_SEED_ACQUISITION_STORAGE_OK);
  uint8_t *old_staging = storage.staging_bytes[0u];
  uint8_t *old_revalidation = storage.revalidation_bytes[0u];
  uint8_t *old_published = storage.published_bytes[0u];
  malicious.duplicate_at = malicious.allocations + 1u;
  const size_t old_deallocations = malicious.deallocations;
  CHECK(w_seed_acquisition_storage_grow_source(&storage, 0u, 5u) ==
        W_SEED_ACQUISITION_STORAGE_ALLOCATION);
  CHECK(storage.staging_bytes[0u] == old_staging &&
        storage.revalidation_bytes[0u] == old_revalidation &&
        storage.published_bytes[0u] == old_published &&
        storage.staging_capacity[0u] == 4u &&
        malicious.deallocations == old_deallocations + 1u);
  w_seed_acquisition_storage_destroy(&storage);

  storage = (w_seed_acquisition_storage){0};
  adjacent_offset = 0u;
  adjacent_deallocations = 0u;
  CHECK(w_seed_acquisition_storage_init_with_allocator(
      &storage, adjacent_allocate, adjacent_deallocate));
  CHECK(w_seed_acquisition_storage_grow_source(&storage, 0u, 9u) ==
        W_SEED_ACQUISITION_STORAGE_OK);
  CHECK(storage.revalidation_bytes[0u] ==
            storage.staging_bytes[0u] + storage.staging_capacity[0u] &&
        storage.published_bytes[0u] ==
            storage.revalidation_bytes[0u] +
                storage.revalidation_capacity[0u]);
  w_seed_ephemeral_provider_request request = {0};
  CHECK(w_seed_acquisition_storage_bind_request(&storage, 0u, &request));
  w_seed_acquisition_storage_destroy(&storage);
  CHECK(adjacent_deallocations == 3u);
  return true;
}

static bool test_bind_preserves_fields_and_rebinds(void) {
  w_seed_acquisition_storage storage = {0};
  w_seed_ephemeral_driver_slot slots[2] = {0};
  w_seed_ephemeral_provider_request requests[2] = {0};
  char source_ids[2][8] = {{0}};
  char module_ids[2][8] = {{0}};
  w_seed_source sources[2] = {0};
  w_seed_ephemeral_graph_provider_facts facts[2] = {0};
  for (size_t index = 0u; index < 2u; index += 1u) {
    slots[index].source_id_storage = source_ids[index];
    slots[index].source_id_capacity = sizeof(source_ids[index]);
    slots[index].module_id_storage = module_ids[index];
    slots[index].module_id_capacity = sizeof(module_ids[index]);
    requests[index].source_id =
        (w_seed_frontend_text){source_ids[index], index + 1u};
    requests[index].source = &sources[index];
    requests[index].facts = &facts[index];
  }
  w_seed_ephemeral_driver_scratch scratch = {0};
  scratch.slots = slots;
  scratch.slot_capacity = 2u;
  scratch.requests = requests;
  scratch.request_capacity = 2u;
  CHECK(w_seed_acquisition_storage_init(&storage));
  CHECK(w_seed_acquisition_storage_grow_source(&storage, 0u, 3u) ==
        W_SEED_ACQUISITION_STORAGE_OK);
  CHECK(w_seed_acquisition_storage_grow_nodes(&storage, 0u, 3u) ==
        W_SEED_ACQUISITION_STORAGE_OK);
  w_seed_ephemeral_driver_slot expected_slots[2];
  w_seed_ephemeral_provider_request expected_requests[2];
  (void)memcpy(expected_slots, slots, sizeof(expected_slots));
  (void)memcpy(expected_requests, requests, sizeof(expected_requests));
  for (size_t index = 0u; index < 2u; index += 1u) {
    expected_slots[index].nodes = storage.nodes[index];
    expected_slots[index].node_capacity = storage.node_capacity[index];
    expected_requests[index].staging_bytes = storage.staging_bytes[index];
    expected_requests[index].staging_capacity =
        storage.staging_capacity[index];
    expected_requests[index].revalidation_bytes =
        storage.revalidation_bytes[index];
    expected_requests[index].revalidation_capacity =
        storage.revalidation_capacity[index];
    expected_requests[index].bytes = storage.published_bytes[index];
    expected_requests[index].byte_capacity =
        storage.published_capacity[index];
  }
  CHECK(w_seed_acquisition_storage_bind_driver(&storage, &scratch));
  CHECK(memcmp(slots, expected_slots, sizeof(slots)) == 0 &&
        memcmp(requests, expected_requests, sizeof(requests)) == 0);
  CHECK(slots[0u].nodes == storage.nodes[0u] &&
        slots[0u].node_capacity == storage.node_capacity[0u]);
  CHECK(requests[0u].staging_bytes == storage.staging_bytes[0u] &&
        requests[0u].revalidation_bytes == storage.revalidation_bytes[0u] &&
        requests[0u].bytes == storage.published_bytes[0u]);
  CHECK(requests[0u].source == &sources[0u] &&
        requests[0u].facts == &facts[0u] &&
        requests[0u].source_id.data == source_ids[0u]);
  CHECK(slots[0u].source_id_storage == source_ids[0u] &&
        slots[0u].module_id_storage == module_ids[0u]);

  uint8_t *old_published = storage.published_bytes[0u];
  w_seed_cst_node *old_nodes = storage.nodes[0u];
  CHECK(w_seed_acquisition_storage_grow_source(&storage, 0u, 5u) ==
        W_SEED_ACQUISITION_STORAGE_OK);
  CHECK(w_seed_acquisition_storage_grow_nodes(&storage, 0u, 5u) ==
        W_SEED_ACQUISITION_STORAGE_OK);
  CHECK(storage.published_bytes[0u] != old_published &&
        storage.nodes[0u] != old_nodes);
  CHECK(requests[0u].bytes == old_published && slots[0u].nodes == old_nodes);
  (void)memcpy(expected_slots, slots, sizeof(expected_slots));
  (void)memcpy(expected_requests, requests, sizeof(expected_requests));
  for (size_t index = 0u; index < 2u; index += 1u) {
    expected_slots[index].nodes = storage.nodes[index];
    expected_slots[index].node_capacity = storage.node_capacity[index];
    expected_requests[index].staging_bytes = storage.staging_bytes[index];
    expected_requests[index].staging_capacity =
        storage.staging_capacity[index];
    expected_requests[index].revalidation_bytes =
        storage.revalidation_bytes[index];
    expected_requests[index].revalidation_capacity =
        storage.revalidation_capacity[index];
    expected_requests[index].bytes = storage.published_bytes[index];
    expected_requests[index].byte_capacity =
        storage.published_capacity[index];
  }
  CHECK(w_seed_acquisition_storage_bind_driver(&storage, &scratch));
  CHECK(memcmp(slots, expected_slots, sizeof(slots)) == 0 &&
        memcmp(requests, expected_requests, sizeof(requests)) == 0);
  CHECK(requests[0u].bytes == storage.published_bytes[0u] &&
        slots[0u].nodes == storage.nodes[0u]);

  uint8_t *stable_published = storage.published_bytes[0u];
  CHECK(w_seed_acquisition_storage_grow_source(&storage, 0u, 6u) ==
        W_SEED_ACQUISITION_STORAGE_OK);
  CHECK(storage.published_bytes[0u] == stable_published);
  CHECK(w_seed_acquisition_storage_grow_source(
            &storage, 0u,
            (size_t)W_SEED_ACQUISITION_MAX_SOURCE_BYTES + 1u) ==
        W_SEED_ACQUISITION_STORAGE_CAPACITY);
  CHECK(storage.published_bytes[0u] == stable_published);
  w_seed_acquisition_storage_destroy(&storage);
  return true;
}

static bool test_bind_preflight_aliases(void) {
  w_seed_acquisition_storage storage = {0};
  CHECK(w_seed_acquisition_storage_init(&storage));
  CHECK(w_seed_acquisition_storage_grow_source(&storage, 0u, 64u) ==
        W_SEED_ACQUISITION_STORAGE_OK);
  CHECK(w_seed_acquisition_storage_grow_nodes(&storage, 0u, 2u) ==
        W_SEED_ACQUISITION_STORAGE_OK);

  w_seed_ephemeral_driver_slot slots[2] = {0};
  w_seed_ephemeral_provider_request requests[2] = {0};
  char source_ids[2][8] = {{0}};
  char module_ids[2][8] = {{0}};
  w_seed_source sources[2] = {0};
  w_seed_ephemeral_graph_provider_facts facts[2] = {0};
  for (size_t index = 0u; index < 2u; index += 1u) {
    slots[index].source_id_storage = source_ids[index];
    slots[index].source_id_capacity = sizeof(source_ids[index]);
    slots[index].module_id_storage = module_ids[index];
    slots[index].module_id_capacity = sizeof(module_ids[index]);
    requests[index].source_id =
        (w_seed_frontend_text){source_ids[index], 1u};
    requests[index].source = &sources[index];
    requests[index].facts = &facts[index];
  }
  w_seed_ephemeral_driver_scratch scratch = {0};
  scratch.slots = slots;
  scratch.slot_capacity = 2u;
  scratch.requests = requests;
  scratch.request_capacity = 2u;

  const w_seed_ephemeral_driver_slot slots_before[2] = {slots[0], slots[1]};
  const w_seed_ephemeral_provider_request requests_before[2] = {
      requests[0], requests[1]};
  slots[0].source_id_storage = (char *)&slots[1];
  slots[0].source_id_capacity = 1u;
  const w_seed_ephemeral_driver_slot forged_slots[2] = {slots[0], slots[1]};
  CHECK(!w_seed_acquisition_storage_bind_driver(&storage, &scratch));
  CHECK(memcmp(slots, forged_slots, sizeof(slots)) == 0 &&
        memcmp(requests, requests_before, sizeof(requests)) == 0);
  (void)memcpy(slots, slots_before, sizeof(slots));

  slots[0].module_id_storage = slots[0].source_id_storage;
  slots[0].module_id_capacity = slots[0].source_id_capacity;
  w_seed_ephemeral_driver_slot preserved_alias_slots[2] = {slots[0],
                                                            slots[1]};
  CHECK(!w_seed_acquisition_storage_bind_driver(&storage, &scratch));
  CHECK(memcmp(slots, preserved_alias_slots, sizeof(slots)) == 0 &&
        memcmp(requests, requests_before, sizeof(requests)) == 0);
  (void)memcpy(slots, slots_before, sizeof(slots));

  slots[1].source_id_storage = slots[0].module_id_storage;
  slots[1].source_id_capacity = slots[0].module_id_capacity;
  preserved_alias_slots[0] = slots[0];
  preserved_alias_slots[1] = slots[1];
  CHECK(!w_seed_acquisition_storage_bind_driver(&storage, &scratch));
  CHECK(memcmp(slots, preserved_alias_slots, sizeof(slots)) == 0 &&
        memcmp(requests, requests_before, sizeof(requests)) == 0);
  (void)memcpy(slots, slots_before, sizeof(slots));

  requests[0].tokens.provider_id = (char *)&requests[1];
  requests[0].tokens.provider_id_capacity = 1u;
  w_seed_ephemeral_provider_request forged_requests[2] = {
      requests[0], requests[1]};
  CHECK(!w_seed_acquisition_storage_bind_driver(&storage, &scratch));
  CHECK(memcmp(slots, slots_before, sizeof(slots)) == 0 &&
        memcmp(requests, forged_requests, sizeof(requests)) == 0);
  (void)memcpy(requests, requests_before, sizeof(requests));

  requests[0].tokens.provider_id = source_ids[0];
  requests[0].tokens.provider_id_capacity = 1u;
  forged_requests[0] = requests[0];
  forged_requests[1] = requests[1];
  CHECK(!w_seed_acquisition_storage_bind_driver(&storage, &scratch));
  CHECK(memcmp(slots, slots_before, sizeof(slots)) == 0 &&
        memcmp(requests, forged_requests, sizeof(requests)) == 0);
  (void)memcpy(requests, requests_before, sizeof(requests));

  char shared_token[2] = {0};
  requests[0].tokens.root_token = shared_token;
  requests[0].tokens.root_token_capacity = sizeof(shared_token);
  requests[1].revalidation_tokens.canonical_token = shared_token;
  requests[1].revalidation_tokens.canonical_token_capacity =
      sizeof(shared_token);
  forged_requests[0] = requests[0];
  forged_requests[1] = requests[1];
  CHECK(!w_seed_acquisition_storage_bind_driver(&storage, &scratch));
  CHECK(memcmp(slots, slots_before, sizeof(slots)) == 0 &&
        memcmp(requests, forged_requests, sizeof(requests)) == 0);
  (void)memcpy(requests, requests_before, sizeof(requests));

  union {
    w_seed_ephemeral_driver_slot slots[2];
    w_seed_ephemeral_provider_request requests[2];
  } same_array = {0};
  w_seed_ephemeral_driver_scratch same_scratch = {0};
  same_scratch.slots = same_array.slots;
  same_scratch.slot_capacity = 2u;
  same_scratch.requests = same_array.requests;
  same_scratch.request_capacity = 2u;
  CHECK(!w_seed_acquisition_storage_bind_driver(&storage, &same_scratch));

  w_seed_ephemeral_driver_scratch self_scratch = {0};
  self_scratch.slots = (w_seed_ephemeral_driver_slot *)&self_scratch;
  self_scratch.slot_capacity = 1u;
  self_scratch.requests = requests;
  self_scratch.request_capacity = 1u;
  const w_seed_ephemeral_driver_scratch self_before = self_scratch;
  CHECK(!w_seed_acquisition_storage_bind_driver(&storage, &self_scratch));
  CHECK(memcmp(&self_scratch, &self_before, sizeof(self_scratch)) == 0);

  w_seed_ephemeral_driver_scratch arena_scratch = scratch;
  arena_scratch.requests = (w_seed_ephemeral_provider_request *)
      storage.staging_bytes[0u];
  arena_scratch.request_capacity = 1u;
  CHECK(!w_seed_acquisition_storage_bind_driver(&storage, &arena_scratch));
  CHECK(!w_seed_acquisition_storage_bind_request(
      &storage, 0u, (w_seed_ephemeral_provider_request *)
                        storage.staging_bytes[0u]));

  CHECK(w_seed_acquisition_storage_bind_driver(&storage, &scratch));
  CHECK(slots[0].nodes == storage.nodes[0u] &&
        requests[0].bytes == storage.published_bytes[0u]);
  w_seed_acquisition_storage_destroy(&storage);
  return true;
}

static w_seed_ephemeral_driver_result clear_driver_result(void) {
  w_seed_ephemeral_driver_result result = {0};
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
  result.scan_result.status = W_SEED_MODULE_SCAN_OK;
  result.graph_status = W_SEED_EPHEMERAL_GRAPH_INVALID;
  result.graph_result.status = W_SEED_EPHEMERAL_GRAPH_INVALID;
  result.graph_result.candidate_index = SIZE_MAX;
  result.graph_result.document_ordinal = SIZE_MAX;
  result.graph_result.edge_ordinal = SIZE_MAX;
  return result;
}

static void set_prior_success(w_seed_ephemeral_driver_result *result) {
  if (result == NULL) return;
  result->provider_status = W_SEED_EPHEMERAL_PROVIDER_OK;
  result->provider_result.status = W_SEED_EPHEMERAL_PROVIDER_OK;
  result->provider_result.failure = W_SEED_EPHEMERAL_PROVIDER_FAILURE_NONE;
  result->provider_result.phase = W_SEED_EPHEMERAL_PROVIDER_PHASE_COMMIT;
  result->provider_result.request_index = SIZE_MAX;
  result->provider_result.capacity_field =
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_NONE;
  result->provider_result.backend_status =
      W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
  result->parser_status = W_SEED_PARSE_COMPLETE;
  result->parser_issue_kind = W_SEED_PARSE_ISSUE_NONE;
  result->scan_status = W_SEED_MODULE_SCAN_OK;
  result->scan_result.status = W_SEED_MODULE_SCAN_OK;
  result->scan_result.required = 0u;
  result->scan_result.written = 0u;
}

static w_seed_ephemeral_driver_result provider_capacity(
    w_seed_ephemeral_provider_capacity_field field, size_t request_index,
    size_t required) {
  w_seed_ephemeral_driver_result result = clear_driver_result();
  result.status = W_SEED_EPHEMERAL_DRIVER_CAPACITY;
  result.failure = W_SEED_EPHEMERAL_DRIVER_FAILURE_PROVIDER;
  result.phase = W_SEED_EPHEMERAL_DRIVER_PHASE_ACQUIRE;
  result.round = 0u;
  result.candidate_index = request_index;
  result.origin_index = SIZE_MAX;
  result.document_index = SIZE_MAX;
  result.capacity_field = W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PROVIDER;
  result.required_capacity = required;
  result.provider_status = W_SEED_EPHEMERAL_PROVIDER_CAPACITY;
  result.provider_result.status = W_SEED_EPHEMERAL_PROVIDER_CAPACITY;
  result.provider_result.failure = W_SEED_EPHEMERAL_PROVIDER_FAILURE_LIMIT;
  result.provider_result.phase = W_SEED_EPHEMERAL_PROVIDER_PHASE_VALIDATE;
  result.provider_result.request_index = request_index;
  result.provider_result.capacity_field = field;
  result.provider_result.required_capacity = required;
  result.provider_result.backend_status =
      W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
  if (field == W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES) {
    result.provider_result.phase = W_SEED_EPHEMERAL_PROVIDER_PHASE_READ;
    result.provider_result.required_byte_capacity = required;
    result.provider_result.observed_byte_count = required;
    result.provider_result.backend_status =
        W_SEED_EPHEMERAL_PROVIDER_BACKEND_CAPACITY;
  } else if (field ==
             W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_BYTES) {
    result.provider_result.phase = W_SEED_EPHEMERAL_PROVIDER_PHASE_REVALIDATE;
    result.provider_result.required_byte_capacity = required;
    result.provider_result.observed_byte_count = required;
    result.provider_result.backend_status =
        W_SEED_EPHEMERAL_PROVIDER_BACKEND_CAPACITY;
  } else if (field ==
             W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_OUTPUT_BYTES) {
    result.provider_result.phase = W_SEED_EPHEMERAL_PROVIDER_PHASE_COMMIT;
    result.provider_result.required_byte_capacity = required;
    result.provider_result.observed_byte_count = required;
  } else if (field ==
             W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_AGGREGATE_SOURCE_BYTES) {
    result.provider_result.phase = W_SEED_EPHEMERAL_PROVIDER_PHASE_READ;
    result.provider_result.observed_byte_count = required;
  }
  return result;
}

static w_seed_ephemeral_driver_result node_capacity(size_t index,
                                                    size_t required) {
  w_seed_ephemeral_driver_result result = clear_driver_result();
  result.status = W_SEED_EPHEMERAL_DRIVER_CAPACITY;
  result.failure = W_SEED_EPHEMERAL_DRIVER_FAILURE_PARSE;
  result.phase = W_SEED_EPHEMERAL_DRIVER_PHASE_PARSE;
  result.round = 0u;
  result.candidate_index = index;
  result.origin_index = SIZE_MAX;
  result.document_index = index;
  result.capacity_field =
      W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_NODE;
  result.required_capacity = required;
  set_prior_success(&result);
  result.parser_status = W_SEED_PARSE_FATAL;
  result.scan_status = W_SEED_MODULE_SCAN_INVALID;
  result.scan_result = (w_seed_module_scan_result){0};
  return result;
}

static bool outcome_is_invalid(w_seed_acquisition_retry_outcome outcome) {
  return outcome.status == W_SEED_ACQUISITION_RETRY_INVALID &&
         outcome.action == W_SEED_ACQUISITION_RETRY_TERMINAL &&
         outcome.detail == W_SEED_ACQUISITION_RETRY_DETAIL_INVALID_RESULT;
}

static bool test_retry_provider_and_forged_envelopes(void) {
  w_seed_acquisition_storage storage = {0};
  CHECK(w_seed_acquisition_storage_init(&storage));
  const w_seed_ephemeral_provider_capacity_field fields[3] = {
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES,
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_BYTES,
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_OUTPUT_BYTES};
  w_seed_acquisition_retry_outcome outcome;
  for (size_t index = 0u; index < 3u; index += 1u) {
    const w_seed_ephemeral_driver_result direct =
        provider_capacity(fields[index], index, 7u);
    outcome = w_seed_acquisition_retry_apply(
        &storage, W_SEED_EPHEMERAL_DRIVER_CAPACITY, &direct);
    CHECK(outcome.status == W_SEED_ACQUISITION_RETRY_OK &&
          outcome.action == W_SEED_ACQUISITION_RETRY_RETRY &&
          storage.staging_capacity[index] == 8u &&
          storage.revalidation_capacity[index] == 8u &&
          storage.published_capacity[index] == 8u);
  }

  w_seed_ephemeral_driver_result result = provider_capacity(
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES, 1u, 9u);

  result = provider_capacity(
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES, 1u, 9u);
  result.provider_result.required_byte_capacity = 8u;
  CHECK(outcome_is_invalid(w_seed_acquisition_retry_apply(
      &storage, W_SEED_EPHEMERAL_DRIVER_CAPACITY, &result)));
  result = provider_capacity(
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES, 1u, 9u);
  result.provider_result.observed_byte_count = 8u;
  CHECK(outcome_is_invalid(w_seed_acquisition_retry_apply(
      &storage, W_SEED_EPHEMERAL_DRIVER_CAPACITY, &result)));
  result = provider_capacity(
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES, 1u, 9u);
  result.provider_result.phase = W_SEED_EPHEMERAL_PROVIDER_PHASE_VALIDATE;
  CHECK(outcome_is_invalid(w_seed_acquisition_retry_apply(
      &storage, W_SEED_EPHEMERAL_DRIVER_CAPACITY, &result)));
  result = provider_capacity(
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES, 1u, 9u);
  result.provider_result.request_index = 0u;
  CHECK(outcome_is_invalid(w_seed_acquisition_retry_apply(
      &storage, W_SEED_EPHEMERAL_DRIVER_CAPACITY, &result)));
  result = provider_capacity(
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES, 1u, 9u);
  result.provider_result.total_source_bytes =
      (size_t)W_SEED_ACQUISITION_MAX_TOTAL_SOURCE_BYTES + 1u;
  CHECK(outcome_is_invalid(w_seed_acquisition_retry_apply(
      &storage, W_SEED_EPHEMERAL_DRIVER_CAPACITY, &result)));
  result = provider_capacity(
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES, 1u, 9u);
  CHECK(outcome_is_invalid(w_seed_acquisition_retry_apply(
      &storage, W_SEED_EPHEMERAL_DRIVER_OK, &result)));

  result = provider_capacity(
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_BYTES, 1u, 9u);
  result.provider_result.backend_status =
      W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
  CHECK(outcome_is_invalid(w_seed_acquisition_retry_apply(
      &storage, W_SEED_EPHEMERAL_DRIVER_CAPACITY, &result)));

  result = provider_capacity(
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES, 4u, 7u);
  result.provider_result.backend_status =
      W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
  outcome = w_seed_acquisition_retry_apply(
      &storage, W_SEED_EPHEMERAL_DRIVER_CAPACITY, &result);
  CHECK(outcome.status == W_SEED_ACQUISITION_RETRY_CAPACITY &&
        outcome.action == W_SEED_ACQUISITION_RETRY_TERMINAL &&
        outcome.detail == W_SEED_ACQUISITION_RETRY_DETAIL_NON_RESIZABLE &&
        storage.staging_capacity[4u] == 0u);

  result = provider_capacity(
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_OUTPUT_BYTES, 2u, 4u);
  outcome = w_seed_acquisition_retry_apply(
      &storage, W_SEED_EPHEMERAL_DRIVER_CAPACITY, &result);
  CHECK(outcome.status == W_SEED_ACQUISITION_RETRY_FAULT &&
        outcome.detail == W_SEED_ACQUISITION_RETRY_DETAIL_NO_PROGRESS);
  result = provider_capacity(
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_PROVIDER_ID, 0u, 4u);
  outcome = w_seed_acquisition_retry_apply(
      &storage, W_SEED_EPHEMERAL_DRIVER_CAPACITY, &result);
  CHECK(outcome.status == W_SEED_ACQUISITION_RETRY_CAPACITY &&
        outcome.detail == W_SEED_ACQUISITION_RETRY_DETAIL_NON_RESIZABLE);
  result = provider_capacity(
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_AGGREGATE_SOURCE_BYTES, 0u,
      4u);
  outcome = w_seed_acquisition_retry_apply(
      &storage, W_SEED_EPHEMERAL_DRIVER_CAPACITY, &result);
  CHECK(outcome.status == W_SEED_ACQUISITION_RETRY_CAPACITY);
  w_seed_acquisition_storage_destroy(&storage);
  return true;
}

static bool test_retry_nodes_fixed_and_limit(void) {
  w_seed_acquisition_storage storage = {0};
  CHECK(w_seed_acquisition_storage_init(&storage));
  w_seed_ephemeral_driver_result result = node_capacity(2u, 7u);
  w_seed_acquisition_retry_outcome outcome =
      w_seed_acquisition_retry_apply(&storage,
                                     W_SEED_EPHEMERAL_DRIVER_CAPACITY,
                                     &result);
  CHECK(outcome.status == W_SEED_ACQUISITION_RETRY_OK &&
        storage.node_capacity[2u] == 8u);
  result = node_capacity(2u, 9u);
  result.provider_result.phase = W_SEED_EPHEMERAL_PROVIDER_PHASE_VALIDATE;
  CHECK(outcome_is_invalid(w_seed_acquisition_retry_apply(
      &storage, W_SEED_EPHEMERAL_DRIVER_CAPACITY, &result)));
  result = node_capacity(2u, 9u);
  result.document_index = 1u;
  CHECK(outcome_is_invalid(w_seed_acquisition_retry_apply(
      &storage, W_SEED_EPHEMERAL_DRIVER_CAPACITY, &result)));
  result = node_capacity(2u, 9u);
  result.parser_issue_kind = W_SEED_PARSE_ISSUE_LEXER;
  outcome = w_seed_acquisition_retry_apply(
      &storage, W_SEED_EPHEMERAL_DRIVER_CAPACITY, &result);
  CHECK(outcome.status == W_SEED_ACQUISITION_RETRY_OK &&
        outcome.action == W_SEED_ACQUISITION_RETRY_RETRY);
  result = node_capacity(2u, 9u);
  result.parser_issue_kind = (w_seed_parse_issue_kind)-1;
  CHECK(outcome_is_invalid(w_seed_acquisition_retry_apply(
      &storage, W_SEED_EPHEMERAL_DRIVER_CAPACITY, &result)));

  result = clear_driver_result();
  result.status = W_SEED_EPHEMERAL_DRIVER_CAPACITY;
  result.failure = W_SEED_EPHEMERAL_DRIVER_FAILURE_STORAGE;
  result.phase = W_SEED_EPHEMERAL_DRIVER_PHASE_GRAPH_WRITE;
  result.round = 0u;
  result.capacity_field = W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_GRAPH_EDGE;
  result.required_capacity = 1u;
  result.candidate_index = SIZE_MAX;
  result.origin_index = SIZE_MAX;
  result.document_index = SIZE_MAX;
  set_prior_success(&result);
  result.graph_status = W_SEED_EPHEMERAL_GRAPH_OK;
  result.graph_result.status = W_SEED_EPHEMERAL_GRAPH_OK;
  result.graph_result.required.edges = 1u;
  outcome = w_seed_acquisition_retry_apply(
      &storage, W_SEED_EPHEMERAL_DRIVER_CAPACITY, &result);
  CHECK(outcome.status == W_SEED_ACQUISITION_RETRY_CAPACITY &&
        outcome.detail == W_SEED_ACQUISITION_RETRY_DETAIL_NON_RESIZABLE);
  result.graph_result.required.edges = 2u;
  CHECK(outcome_is_invalid(w_seed_acquisition_retry_apply(
      &storage, W_SEED_EPHEMERAL_DRIVER_CAPACITY, &result)));
  result.graph_result.required.edges = 1u;
  result.phase = W_SEED_EPHEMERAL_DRIVER_PHASE_DISCOVER;
  CHECK(outcome_is_invalid(w_seed_acquisition_retry_apply(
      &storage, W_SEED_EPHEMERAL_DRIVER_CAPACITY, &result)));

  /* A conforming CHK6 producer exhausts its own round limit before the larger
   * ACQ0 attempt budget. Exercise that terminal retry-limit authority with
   * the complete real-shaped driver envelope instead of mutating storage from
   * a callback to manufacture thousands of pipeline attempts. */
  result = clear_driver_result();
  result.status = W_SEED_EPHEMERAL_DRIVER_CAPACITY;
  result.failure = W_SEED_EPHEMERAL_DRIVER_FAILURE_LIMIT;
  result.phase = W_SEED_EPHEMERAL_DRIVER_PHASE_DISCOVER;
  result.round = W_SEED_EPHEMERAL_DRIVER_MAX_ROUNDS;
  result.candidate_index = SIZE_MAX;
  result.origin_index = SIZE_MAX;
  result.document_index = SIZE_MAX;
  result.capacity_field = W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_ROUNDS;
  result.required_capacity =
      (size_t)W_SEED_EPHEMERAL_DRIVER_MAX_ROUNDS + 1u;
  set_prior_success(&result);
  outcome = w_seed_acquisition_retry_apply(
      &storage, W_SEED_EPHEMERAL_DRIVER_CAPACITY, &result);
  CHECK(outcome.status == W_SEED_ACQUISITION_RETRY_CAPACITY &&
        outcome.detail == W_SEED_ACQUISITION_RETRY_DETAIL_RETRY_LIMIT);
  w_seed_acquisition_storage_destroy(&storage);
  return true;
}

static bool test_retry_allocation_failure(void) {
  w_seed_acquisition_storage storage = {0};
  reset_probe(0u);
  CHECK(w_seed_acquisition_storage_init_with_allocator(
      &storage, probe_allocate, probe_deallocate));
  const w_seed_ephemeral_driver_result result = provider_capacity(
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES, 0u, 4u);
  const w_seed_acquisition_retry_outcome outcome =
      w_seed_acquisition_retry_apply(&storage,
                                     W_SEED_EPHEMERAL_DRIVER_CAPACITY,
                                     &result);
  CHECK(outcome.status == W_SEED_ACQUISITION_RETRY_ALLOCATION &&
        outcome.action == W_SEED_ACQUISITION_RETRY_TERMINAL &&
        outcome.detail == W_SEED_ACQUISITION_RETRY_DETAIL_ALLOCATION);
  CHECK(storage.staging_capacity[0u] == 0u &&
        storage.revalidation_capacity[0u] == 0u &&
        storage.published_capacity[0u] == 0u);
  w_seed_acquisition_storage_destroy(&storage);
  return true;
}

enum {
  PIPELINE_SOURCES = 3,
  PIPELINE_PATH_BYTES = 64,
  PIPELINE_TOKEN_BYTES = 32,
  PIPELINE_LEXER_FRAMES = 128,
  PIPELINE_TOKENS = 1024,
  PIPELINE_PARSE_FRAMES = 256,
  PIPELINE_ISSUES = 64,
  PIPELINE_ORIGINS = 8,
  PIPELINE_EDGES = 4,
};

typedef struct {
  const char *root_text;
  const char *child_text;
  const char *mutated_child_text;
  bool root_containment_inside;
  bool child_containment_inside;
  bool mutate_revalidation;
  bool unsupported_root;
  bool io_read;
  bool io_after_first_attempt;
  bool canonical_alias_child;
  size_t root_open_calls;
  size_t source_open_calls;
  size_t read_calls;
  size_t revalidate_calls;
  size_t source_close_calls;
  size_t root_close_calls;
} pipeline_backend;

typedef struct {
  char root_path[PIPELINE_PATH_BYTES];
  char root_source_id[PIPELINE_PATH_BYTES];
  char identities[PIPELINE_SOURCES][2u * PIPELINE_PATH_BYTES];
  char primary_tokens[PIPELINE_SOURCES][4u][PIPELINE_TOKEN_BYTES];
  char revalidation_tokens[PIPELINE_SOURCES][4u][PIPELINE_TOKEN_BYTES];
  w_seed_ephemeral_driver_slot slots[PIPELINE_SOURCES];
  w_seed_ephemeral_provider_request requests[PIPELINE_SOURCES];
  w_seed_lexer_frame lexer_frames[PIPELINE_LEXER_FRAMES];
  w_seed_parse_token tokens[PIPELINE_TOKENS];
  w_seed_parse_frame parse_frames[PIPELINE_PARSE_FRAMES];
  w_seed_parse_issue issues[PIPELINE_ISSUES];
  w_seed_module_origin origins[PIPELINE_ORIGINS];
  w_seed_frontend_document candidate_documents[PIPELINE_SOURCES];
  w_seed_ephemeral_graph_provider_facts candidate_facts[PIPELINE_SOURCES];
  w_seed_ephemeral_graph_scratch_node graph_nodes[PIPELINE_SOURCES];
  w_seed_ephemeral_graph_scratch_edge graph_edges[PIPELINE_EDGES];
  size_t sorted_nodes[PIPELINE_SOURCES];
  size_t node_ordinals[PIPELINE_SOURCES];
  size_t sorted_edges[PIPELINE_EDGES];
  size_t sorted_resolved_edges[PIPELINE_EDGES];
  w_seed_module_origin graph_origins[PIPELINE_EDGES];
  uint32_t indegree[PIPELINE_SOURCES];
  uint32_t queue[PIPELINE_SOURCES];
  uint32_t depths[PIPELINE_SOURCES];
  w_seed_ephemeral_graph_scratch graph_scratch;
  w_seed_frontend_document documents[PIPELINE_SOURCES];
  w_seed_ephemeral_graph_inventory_item inventory[PIPELINE_SOURCES];
  w_seed_ephemeral_graph_edge edges[PIPELINE_EDGES];
  uint32_t document_order[PIPELINE_SOURCES];
  w_seed_frontend_resolved_import resolved[PIPELINE_EDGES];
  w_seed_ephemeral_driver_scratch scratch;
  w_seed_ephemeral_driver_input driver_input;
  w_seed_ephemeral_driver_output output;
} pipeline_fixture;

typedef struct {
  pipeline_backend backend;
  pipeline_fixture fixture;
} pipeline_adjacent_context;

_Static_assert(offsetof(pipeline_adjacent_context, fixture) ==
                   sizeof(pipeline_backend),
               "backend context must be adjacent to the fixture");
_Static_assert(_Alignof(w_seed_ephemeral_graph_inventory_item) >=
                   _Alignof(pipeline_backend),
               "inventory storage must align an overlap backend");
_Static_assert(sizeof(pipeline_backend) <=
                   sizeof(((pipeline_fixture *)0)->inventory),
               "inventory storage must contain an overlap backend");

typedef struct {
  w_seed_ephemeral_driver_output descriptor;
  w_seed_frontend_document documents[PIPELINE_SOURCES];
  w_seed_ephemeral_graph_inventory_item inventory[PIPELINE_SOURCES];
  w_seed_ephemeral_graph_edge edges[PIPELINE_EDGES];
  uint32_t document_order[PIPELINE_SOURCES];
  w_seed_frontend_resolved_import resolved[PIPELINE_EDGES];
} pipeline_output_snapshot;

static const char restaurant_source[] =
    "module restaurant;\n"
    "import { value } from kitchen.menu\n"
    "fn order(): i64 { return value() }\n";
static const char kitchen_source[] =
    "module menu;\n"
    "export fn value(): i64 { return 42 }\n";
static const char mutated_kitchen_source[] =
    "module menu;\n"
    "export fn value(): i64 { return 43 }\n";
static const char root_only_source[] =
    "module restaurant;\n"
    "fn order(): i64 { return 42 }\n";
static const char mutated_root_only_source[] =
    "module restaurant;\n"
    "fn order(): i64 { return 43 }\n";

_Static_assert(sizeof(kitchen_source) == sizeof(mutated_kitchen_source),
               "revalidation mutation must preserve byte length");
_Static_assert(sizeof(root_only_source) == sizeof(mutated_root_only_source),
               "reuse mutation must preserve byte length");

static bool pipeline_copy_text(char *destination, size_t capacity,
                               const char *source) {
  if (destination == NULL || source == NULL) return false;
  const size_t length = strlen(source);
  if (length > capacity) return false;
  if (length != 0u) (void)memcpy(destination, source, length);
  return true;
}

static void pipeline_fill_observation(
    pipeline_backend *backend,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_observation *observation,
    const char *canonical, bool containment_inside) {
  static const char provider[] = "local";
  static const char root[] = "root-token";
  static const char owner[] = "owner";
  (void)memset(observation, 0, sizeof(*observation));
  if (backend == NULL ||
      !pipeline_copy_text(tokens->provider_id, tokens->provider_id_capacity,
                          provider) ||
      !pipeline_copy_text(tokens->root_token, tokens->root_token_capacity,
                          root) ||
      !pipeline_copy_text(tokens->source_provider_owner_token,
                          tokens->source_provider_owner_token_capacity,
                          owner) ||
      !pipeline_copy_text(tokens->canonical_token,
                          tokens->canonical_token_capacity, canonical))
    return;
  observation->opened = true;
  observation->containment_inside = containment_inside;
  observation->symlink = W_SEED_EPHEMERAL_GRAPH_SYMLINK_NONE;
  observation->provider_id_length = sizeof(provider) - 1u;
  observation->root_token_length = sizeof(root) - 1u;
  observation->source_provider_owner_token_length = sizeof(owner) - 1u;
  observation->canonical_token_length = strlen(canonical);
}

static bool pipeline_text_equal(w_seed_frontend_text text,
                                const char *expected) {
  const size_t length = strlen(expected);
  return text.length == length && text.data != NULL &&
         memcmp(text.data, expected, length) == 0;
}

static bool pipeline_source_is_child(w_seed_frontend_text source_id) {
  return pipeline_text_equal(source_id, "kitchen/menu.w");
}

static const char *pipeline_handle_text(
    const pipeline_backend *backend,
    w_seed_ephemeral_provider_handle handle) {
  if (backend == NULL) return NULL;
  if (handle.value == (uintptr_t)2u) return backend->root_text;
  if (handle.value == (uintptr_t)3u) return backend->child_text;
  return NULL;
}

static w_seed_ephemeral_provider_backend_status pipeline_open_root(
    void *context, w_seed_byte_view root_path,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_handle *root_handle,
    w_seed_ephemeral_provider_handle *root_source_handle,
    w_seed_ephemeral_provider_observation *observation) {
  pipeline_backend *backend = (pipeline_backend *)context;
  if (backend == NULL || root_path.data == NULL || root_path.length == 0u ||
      tokens == NULL || root_handle == NULL || root_source_handle == NULL ||
      observation == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  backend->root_open_calls += 1u;
  if (backend->unsupported_root)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
  *root_handle = (w_seed_ephemeral_provider_handle){1u};
  *root_source_handle = (w_seed_ephemeral_provider_handle){2u};
  pipeline_fill_observation(backend, tokens, observation, "canonical-root",
                            backend->root_containment_inside);
  return observation->opened ? W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK
                             : W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
}

static w_seed_ephemeral_provider_backend_status pipeline_open_source(
    void *context, w_seed_ephemeral_provider_handle root_handle,
    w_seed_frontend_text source_id,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_handle *source_handle,
    w_seed_ephemeral_provider_observation *observation) {
  pipeline_backend *backend = (pipeline_backend *)context;
  if (backend == NULL || root_handle.value != (uintptr_t)1u ||
      tokens == NULL || source_handle == NULL || observation == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  backend->source_open_calls += 1u;
  if (!pipeline_source_is_child(source_id))
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_NOT_FOUND;
  *source_handle = (w_seed_ephemeral_provider_handle){3u};
  pipeline_fill_observation(
      backend, tokens, observation,
      backend->canonical_alias_child ? "canonical-root" : "canonical-child",
      backend->child_containment_inside);
  return observation->opened ? W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK
                             : W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
}

static w_seed_ephemeral_provider_backend_status pipeline_write_source(
    pipeline_backend *backend, const char *text, uint8_t *bytes,
    size_t capacity, size_t *written) {
  if (backend == NULL || text == NULL || written == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  const size_t length = strlen(text);
  *written = length;
  if (backend->io_read ||
      (backend->io_after_first_attempt && backend->root_open_calls > 1u))
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO;
  if (length > capacity)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_CAPACITY;
  if (length != 0u && bytes == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  if (length != 0u) (void)memcpy(bytes, text, length);
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
}

static w_seed_ephemeral_provider_backend_status pipeline_read_source(
    void *context, w_seed_ephemeral_provider_handle source_handle,
    uint8_t *bytes, size_t capacity, size_t *written) {
  pipeline_backend *backend = (pipeline_backend *)context;
  if (backend != NULL) backend->read_calls += 1u;
  return pipeline_write_source(backend,
                               pipeline_handle_text(backend, source_handle),
                               bytes, capacity, written);
}

static w_seed_ephemeral_provider_backend_status pipeline_revalidate_source(
    void *context, w_seed_ephemeral_provider_handle root_handle,
    w_seed_ephemeral_provider_handle source_handle,
    w_seed_frontend_text source_id,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_observation *observation, uint8_t *bytes,
    size_t capacity, size_t *written) {
  pipeline_backend *backend = (pipeline_backend *)context;
  if (backend == NULL || root_handle.value != (uintptr_t)1u ||
      tokens == NULL || observation == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  backend->revalidate_calls += 1u;
  const bool child = source_handle.value == (uintptr_t)3u;
  if ((child && !pipeline_source_is_child(source_id)) ||
      (!child && !pipeline_text_equal(source_id, "restaurant.w")))
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_NOT_FOUND;
  const char *text = child && backend->mutate_revalidation
                         ? backend->mutated_child_text
                         : pipeline_handle_text(backend, source_handle);
  const w_seed_ephemeral_provider_backend_status status =
      pipeline_write_source(backend, text, bytes, capacity, written);
  if (status != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) return status;
  pipeline_fill_observation(
      backend, tokens, observation,
      child && !backend->canonical_alias_child ? "canonical-child"
                                                : "canonical-root",
      child ? backend->child_containment_inside
            : backend->root_containment_inside);
  return observation->opened ? W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK
                             : W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
}

static void pipeline_close_source(
    void *context, w_seed_ephemeral_provider_handle source_handle) {
  pipeline_backend *backend = (pipeline_backend *)context;
  if (backend != NULL && source_handle.value != (uintptr_t)0u)
    backend->source_close_calls += 1u;
}

static void pipeline_close_root(
    void *context, w_seed_ephemeral_provider_handle root_handle) {
  pipeline_backend *backend = (pipeline_backend *)context;
  if (backend != NULL && root_handle.value != (uintptr_t)0u)
    backend->root_close_calls += 1u;
}

static w_seed_ephemeral_provider_backend pipeline_backend_vtable(
    pipeline_backend *backend) {
  return (w_seed_ephemeral_provider_backend){
      .context = backend,
      .open_root = pipeline_open_root,
      .open_source = pipeline_open_source,
      .read_source = pipeline_read_source,
      .revalidate_source = pipeline_revalidate_source,
      .close_source = pipeline_close_source,
      .close_root = pipeline_close_root,
      .metadata = {{16u, 16u}, {16u, 16u}, {16u, 16u}, {16u, 16u}}};
}

static void pipeline_fixture_init(pipeline_fixture *fixture,
                                  pipeline_backend *backend) {
  (void)memset(fixture, 0, sizeof(*fixture));
  (void)pipeline_copy_text(fixture->root_path, sizeof(fixture->root_path),
                           "virtual-root");
  (void)pipeline_copy_text(fixture->root_source_id,
                           sizeof(fixture->root_source_id), "restaurant.w");
  for (size_t index = 0u; index < PIPELINE_SOURCES; index += 1u) {
    fixture->slots[index].source_id_storage = fixture->identities[index];
    fixture->slots[index].source_id_capacity = PIPELINE_PATH_BYTES;
    fixture->slots[index].module_id_storage =
        fixture->identities[index] + PIPELINE_PATH_BYTES;
    fixture->slots[index].module_id_capacity = PIPELINE_PATH_BYTES;
    fixture->requests[index].tokens =
        (w_seed_ephemeral_provider_token_buffers){
            fixture->primary_tokens[index][0u], PIPELINE_TOKEN_BYTES,
            fixture->primary_tokens[index][1u], PIPELINE_TOKEN_BYTES,
            fixture->primary_tokens[index][2u], PIPELINE_TOKEN_BYTES,
            fixture->primary_tokens[index][3u], PIPELINE_TOKEN_BYTES};
    fixture->requests[index].revalidation_tokens =
        (w_seed_ephemeral_provider_token_buffers){
            fixture->revalidation_tokens[index][0u], PIPELINE_TOKEN_BYTES,
            fixture->revalidation_tokens[index][1u], PIPELINE_TOKEN_BYTES,
            fixture->revalidation_tokens[index][2u], PIPELINE_TOKEN_BYTES,
            fixture->revalidation_tokens[index][3u], PIPELINE_TOKEN_BYTES};
  }
  fixture->graph_scratch = (w_seed_ephemeral_graph_scratch){
      fixture->graph_nodes,
      PIPELINE_SOURCES,
      fixture->graph_edges,
      PIPELINE_EDGES,
      fixture->sorted_nodes,
      PIPELINE_SOURCES,
      fixture->node_ordinals,
      PIPELINE_SOURCES,
      fixture->sorted_edges,
      PIPELINE_EDGES,
      fixture->sorted_resolved_edges,
      PIPELINE_EDGES,
      fixture->graph_origins,
      PIPELINE_EDGES,
      fixture->indegree,
      PIPELINE_SOURCES,
      fixture->queue,
      PIPELINE_SOURCES,
      fixture->depths,
      PIPELINE_SOURCES};
  fixture->scratch = (w_seed_ephemeral_driver_scratch){
      fixture->slots,
      PIPELINE_SOURCES,
      fixture->requests,
      PIPELINE_SOURCES,
      fixture->lexer_frames,
      PIPELINE_LEXER_FRAMES,
      fixture->tokens,
      PIPELINE_TOKENS,
      fixture->parse_frames,
      PIPELINE_PARSE_FRAMES,
      fixture->issues,
      PIPELINE_ISSUES,
      fixture->origins,
      PIPELINE_ORIGINS,
      fixture->candidate_documents,
      PIPELINE_SOURCES,
      fixture->candidate_facts,
      PIPELINE_SOURCES,
      &fixture->graph_scratch};
  fixture->output = (w_seed_ephemeral_driver_output){
      {fixture->inventory, PIPELINE_SOURCES, fixture->edges, PIPELINE_EDGES,
       fixture->document_order, PIPELINE_SOURCES, fixture->resolved,
       PIPELINE_EDGES},
      fixture->documents,
      PIPELINE_SOURCES,
      0u};
  fixture->driver_input = (w_seed_ephemeral_driver_input){
      {(const uint8_t *)fixture->root_path, strlen(fixture->root_path)},
      {fixture->root_source_id, strlen(fixture->root_source_id)},
      {PIPELINE_SOURCES, W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCE_BYTES,
       W_SEED_EPHEMERAL_PROVIDER_MAX_TOTAL_SOURCE_BYTES,
       W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES,
       W_SEED_EPHEMERAL_PROVIDER_MAX_TOKEN_BYTES},
      PIPELINE_EDGES,
      W_SEED_EPHEMERAL_GRAPH_MAX_DEPTH,
      {65536u, 256u},
      pipeline_backend_vtable(backend)};
}

static w_seed_acquisition_pipeline_input pipeline_input(
    pipeline_fixture *fixture, w_seed_acquisition_storage *storage) {
  return (w_seed_acquisition_pipeline_input){
      &fixture->driver_input, &fixture->scratch, &fixture->output, storage,
      sizeof(pipeline_backend)};
}

static void pipeline_poison_output(pipeline_fixture *fixture) {
  (void)memset(fixture->documents, 0xA5, sizeof(fixture->documents));
  (void)memset(fixture->inventory, 0xA6, sizeof(fixture->inventory));
  (void)memset(fixture->edges, 0xA7, sizeof(fixture->edges));
  (void)memset(fixture->document_order, 0xA8,
               sizeof(fixture->document_order));
  (void)memset(fixture->resolved, 0xA9, sizeof(fixture->resolved));
  fixture->output.document_count = 77u;
}

static pipeline_output_snapshot pipeline_snapshot_output(
    const pipeline_fixture *fixture) {
  pipeline_output_snapshot snapshot;
  snapshot.descriptor = fixture->output;
  (void)memcpy(snapshot.documents, fixture->documents,
               sizeof(snapshot.documents));
  (void)memcpy(snapshot.inventory, fixture->inventory,
               sizeof(snapshot.inventory));
  (void)memcpy(snapshot.edges, fixture->edges, sizeof(snapshot.edges));
  (void)memcpy(snapshot.document_order, fixture->document_order,
               sizeof(snapshot.document_order));
  (void)memcpy(snapshot.resolved, fixture->resolved,
               sizeof(snapshot.resolved));
  return snapshot;
}

static bool pipeline_output_matches(
    const pipeline_fixture *fixture,
    const pipeline_output_snapshot *snapshot) {
  return memcmp(&fixture->output, &snapshot->descriptor,
                sizeof(fixture->output)) == 0 &&
         memcmp(fixture->documents, snapshot->documents,
                sizeof(fixture->documents)) == 0 &&
         memcmp(fixture->inventory, snapshot->inventory,
                sizeof(fixture->inventory)) == 0 &&
         memcmp(fixture->edges, snapshot->edges,
                sizeof(fixture->edges)) == 0 &&
         memcmp(fixture->document_order, snapshot->document_order,
                sizeof(fixture->document_order)) == 0 &&
         memcmp(fixture->resolved, snapshot->resolved,
                sizeof(fixture->resolved)) == 0;
}

static bool pipeline_result_output_is_empty(
    const w_seed_acquisition_pipeline_result *result) {
  return result != NULL && result->document_count == 0u &&
         result->graph_written.sources == 0u &&
         result->graph_written.edges == 0u &&
         result->graph_written.total_source_bytes == 0u;
}

static bool pipeline_pregrow(w_seed_acquisition_storage *storage,
                             size_t source_count, size_t byte_capacity,
                             size_t node_capacity) {
  if (storage == NULL || source_count > PIPELINE_SOURCES) return false;
  for (size_t index = 0u; index < source_count; index += 1u) {
    if (w_seed_acquisition_storage_grow_source(storage, index,
                                               byte_capacity) !=
            W_SEED_ACQUISITION_STORAGE_OK ||
        w_seed_acquisition_storage_grow_nodes(storage, index,
                                              node_capacity) !=
            W_SEED_ACQUISITION_STORAGE_OK)
      return false;
  }
  return true;
}

static bool test_pipeline_restaurant(void) {
  pipeline_backend backend = {
      .root_text = restaurant_source,
      .child_text = kitchen_source,
      .mutated_child_text = mutated_kitchen_source,
      .root_containment_inside = true,
      .child_containment_inside = true};
  pipeline_fixture fixture;
  pipeline_fixture_init(&fixture, &backend);
  pipeline_poison_output(&fixture);
  const pipeline_output_snapshot before = pipeline_snapshot_output(&fixture);
  w_seed_acquisition_storage storage = {0};
  CHECK(w_seed_acquisition_storage_init(&storage));
  const w_seed_acquisition_pipeline_input input =
      pipeline_input(&fixture, &storage);
  w_seed_acquisition_pipeline_result result;
  (void)memset(&result, 0xB4, sizeof(result));
  const w_seed_acquisition_pipeline_status pipeline_status =
      w_seed_acquisition_pipeline_run(&input, &result);
  CHECK(pipeline_status == W_SEED_ACQUISITION_PIPELINE_OK);
  CHECK(result.status == W_SEED_ACQUISITION_PIPELINE_OK &&
        result.attempts >= 3u &&
        result.driver_result.status == W_SEED_EPHEMERAL_DRIVER_OK &&
        result.document_count == 2u && result.graph_written.sources == 2u &&
        result.graph_written.edges == 1u &&
        result.graph_written.total_source_bytes ==
            sizeof(restaurant_source) - 1u + sizeof(kitchen_source) - 1u &&
        result.driver_result.graph_result.written.sources == 2u &&
        result.driver_result.graph_result.written.edges == 1u &&
        fixture.output.document_count == 2u);
  CHECK(result.retry.status == W_SEED_ACQUISITION_RETRY_OK &&
        result.retry.action == W_SEED_ACQUISITION_RETRY_RETRY);
  CHECK(result.attempts <= (size_t)W_SEED_ACQUISITION_MAX_ATTEMPTS &&
        storage.staging_capacity[0u] >= sizeof(restaurant_source) - 1u &&
        storage.staging_capacity[1u] >= sizeof(kitchen_source) - 1u &&
        storage.node_capacity[0u] >= 49u &&
        storage.node_capacity[1u] >= 33u);
  CHECK(backend.root_open_calls > result.attempts &&
        backend.root_close_calls == backend.root_open_calls &&
        backend.source_close_calls == backend.read_calls);
  CHECK(pipeline_text_equal(fixture.documents[0u].logical_source_id,
                            "restaurant.w") &&
        pipeline_text_equal(fixture.documents[0u].module_id,
                            "restaurant") &&
        pipeline_text_equal(fixture.documents[1u].logical_source_id,
                            "kitchen/menu.w") &&
        pipeline_text_equal(fixture.documents[1u].module_id,
                            "kitchen.menu"));
  CHECK(fixture.documents[0u].source == &fixture.slots[0u].source &&
        fixture.documents[1u].source == &fixture.slots[1u].source &&
        fixture.slots[0u].source.bytes.data ==
            storage.published_bytes[0u] &&
        fixture.slots[1u].source.bytes.data ==
            storage.published_bytes[1u] &&
        fixture.documents[0u].nodes == storage.nodes[0u] &&
        fixture.documents[1u].nodes == storage.nodes[1u] &&
        fixture.documents[0u].node_count == 49u &&
        fixture.documents[1u].node_count == 33u &&
        fixture.documents[0u].node_count <= storage.node_capacity[0u] &&
        fixture.documents[1u].node_count <= storage.node_capacity[1u]);
  const w_seed_parse_result root_parse = fixture.documents[0u].parse;
  const w_seed_parse_result child_parse = fixture.documents[1u].parse;
  CHECK(root_parse.status == W_SEED_PARSE_COMPLETE &&
        root_parse.root < root_parse.node_count &&
        storage.nodes[0u][root_parse.root].kind == W_SEED_CST_DOCUMENT &&
        storage.nodes[0u][root_parse.root].raw_span.start_byte == 0u &&
        storage.nodes[0u][root_parse.root].raw_span.end_byte ==
            sizeof(restaurant_source) - 1u &&
        child_parse.status == W_SEED_PARSE_COMPLETE &&
        child_parse.root < child_parse.node_count &&
        storage.nodes[1u][child_parse.root].kind == W_SEED_CST_DOCUMENT &&
        storage.nodes[1u][child_parse.root].raw_span.start_byte == 0u &&
        storage.nodes[1u][child_parse.root].raw_span.end_byte ==
            sizeof(kitchen_source) - 1u);
  CHECK(fixture.slots[0u].source.bytes.length ==
            sizeof(restaurant_source) - 1u &&
        memcmp(fixture.slots[0u].source.bytes.data, restaurant_source,
               sizeof(restaurant_source) - 1u) == 0 &&
        fixture.slots[1u].source.bytes.length == sizeof(kitchen_source) - 1u &&
        memcmp(fixture.slots[1u].source.bytes.data, kitchen_source,
               sizeof(kitchen_source) - 1u) == 0);
  CHECK(fixture.document_order[0u] == 0u &&
        fixture.document_order[1u] == 1u &&
        pipeline_text_equal(fixture.inventory[0u].source_id,
                            "restaurant.w") &&
        pipeline_text_equal(fixture.inventory[0u].module_id,
                            "restaurant") &&
        pipeline_text_equal(fixture.inventory[1u].source_id,
                            "kitchen/menu.w") &&
        pipeline_text_equal(fixture.inventory[1u].module_id,
                            "kitchen.menu") &&
        pipeline_text_equal(fixture.inventory[1u].local_module_name,
                            "menu") &&
        fixture.inventory[0u].candidate_index == 0u &&
        fixture.inventory[0u].depth == 0u &&
        fixture.inventory[1u].candidate_index == 1u &&
        fixture.inventory[1u].depth == 1u &&
        fixture.edges[0u].source_ordinal == 0u &&
        fixture.edges[0u].target_ordinal == 1u &&
        fixture.edges[0u].direct_import_ordinal == 0u &&
        fixture.edges[0u].logical_origin == W_SEED_MODULE_ORIGIN_IMPORT &&
        fixture.resolved[0u].source_document_index == 0u &&
        fixture.resolved[0u].direct_import_ordinal == 0u &&
        fixture.resolved[0u].target_kind ==
            W_SEED_FRONTEND_RESOLVED_IMPORT_LOCAL_DOCUMENT &&
        fixture.resolved[0u].target_index == 1u);
  const w_seed_span path = fixture.edges[0u].path_span;
  CHECK(path.start_byte <= sizeof(restaurant_source) - 1u &&
        path.end_byte >= path.start_byte &&
        path.end_byte <= sizeof(restaurant_source) - 1u &&
        path.end_byte - path.start_byte == sizeof("kitchen.menu") - 1u &&
        memcmp(restaurant_source + path.start_byte, "kitchen.menu",
               sizeof("kitchen.menu") - 1u) == 0 &&
        fixture.resolved[0u].import_declaration_span.start_byte ==
            fixture.edges[0u].declaration_span.start_byte &&
        fixture.resolved[0u].import_declaration_span.end_byte ==
            fixture.edges[0u].declaration_span.end_byte);
  uint8_t root_digest[W_SEED_EPHEMERAL_GRAPH_SHA256_BYTES];
  uint8_t child_digest[W_SEED_EPHEMERAL_GRAPH_SHA256_BYTES];
  CHECK(w_seed_ephemeral_graph_source_digest(&fixture.slots[0u].source,
                                             root_digest) &&
        w_seed_ephemeral_graph_source_digest(&fixture.slots[1u].source,
                                             child_digest) &&
        memcmp(root_digest, fixture.inventory[0u].digest,
               sizeof(root_digest)) == 0 &&
        memcmp(child_digest, fixture.inventory[1u].digest,
               sizeof(child_digest)) == 0 &&
        memcmp(root_digest, child_digest, sizeof(root_digest)) != 0);
  CHECK(memcmp(&fixture.documents[2u], &before.documents[2u],
               sizeof(fixture.documents[2u])) == 0 &&
        memcmp(&fixture.inventory[2u], &before.inventory[2u],
               sizeof(fixture.inventory[2u])) == 0 &&
        fixture.document_order[2u] == before.document_order[2u]);
  for (size_t index = 1u; index < PIPELINE_EDGES; index += 1u) {
    CHECK(memcmp(&fixture.edges[index], &before.edges[index],
                 sizeof(fixture.edges[index])) == 0 &&
          memcmp(&fixture.resolved[index], &before.resolved[index],
                 sizeof(fixture.resolved[index])) == 0);
  }
  w_seed_acquisition_storage_destroy(&storage);
  return true;
}

static bool test_pipeline_root_retry_and_reuse(void) {
  pipeline_backend backend = {
      .root_text = root_only_source,
      .child_text = kitchen_source,
      .mutated_child_text = mutated_kitchen_source,
      .root_containment_inside = true,
      .child_containment_inside = true};
  pipeline_fixture fixture;
  pipeline_fixture_init(&fixture, &backend);
  pipeline_poison_output(&fixture);
  w_seed_acquisition_storage storage = {0};
  CHECK(w_seed_acquisition_storage_init(&storage));
  const w_seed_acquisition_pipeline_input input =
      pipeline_input(&fixture, &storage);
  w_seed_acquisition_pipeline_result first;
  CHECK(w_seed_acquisition_pipeline_run(&input, &first) ==
        W_SEED_ACQUISITION_PIPELINE_OK);
  CHECK(first.attempts >= 3u && backend.root_open_calls == first.attempts &&
        backend.root_close_calls == backend.root_open_calls &&
        backend.source_close_calls == backend.root_open_calls &&
        fixture.output.document_count == 1u &&
        first.graph_written.sources == 1u && first.graph_written.edges == 0u);
  const uint8_t *old_view = fixture.slots[0u].source.bytes.data;
  const size_t old_root_calls = backend.root_open_calls;
  const size_t old_root_closes = backend.root_close_calls;
  const size_t old_source_closes = backend.source_close_calls;
  backend.root_text = mutated_root_only_source;
  pipeline_poison_output(&fixture);
  w_seed_acquisition_pipeline_result second;
  CHECK(w_seed_acquisition_pipeline_run(&input, &second) ==
        W_SEED_ACQUISITION_PIPELINE_OK);
  CHECK(second.attempts == 1u &&
        second.retry.status == W_SEED_ACQUISITION_RETRY_NOT_RUN &&
        second.retry.action == W_SEED_ACQUISITION_RETRY_ACTION_NOT_RUN &&
        backend.root_open_calls == old_root_calls + 1u &&
        backend.root_close_calls == old_root_closes + 1u &&
        backend.source_close_calls == old_source_closes + 1u &&
        fixture.slots[0u].source.bytes.data == old_view &&
        fixture.slots[0u].source.bytes.length ==
            sizeof(mutated_root_only_source) - 1u &&
        memcmp(fixture.slots[0u].source.bytes.data, mutated_root_only_source,
               sizeof(mutated_root_only_source) - 1u) == 0);
  w_seed_acquisition_storage_destroy(&storage);
  return true;
}

static bool test_pipeline_barriers_and_fixed_output(void) {
  pipeline_backend backend = {
      .root_text = restaurant_source,
      .child_text = kitchen_source,
      .mutated_child_text = mutated_kitchen_source,
      .root_containment_inside = true,
      .child_containment_inside = false};
  pipeline_fixture fixture;
  pipeline_fixture_init(&fixture, &backend);
  pipeline_poison_output(&fixture);
  pipeline_output_snapshot before = pipeline_snapshot_output(&fixture);
  w_seed_acquisition_storage storage = {0};
  CHECK(w_seed_acquisition_storage_init(&storage));
  CHECK(pipeline_pregrow(&storage, 2u, 128u, 256u));
  w_seed_acquisition_pipeline_input input = pipeline_input(&fixture, &storage);
  w_seed_acquisition_pipeline_result result;
  CHECK(w_seed_acquisition_pipeline_run(&input, &result) ==
        W_SEED_ACQUISITION_PIPELINE_INVALID);
  CHECK(result.attempts == 1u && pipeline_result_output_is_empty(&result) &&
        result.driver_result.failure ==
            W_SEED_EPHEMERAL_DRIVER_FAILURE_PROVIDER &&
        result.driver_result.phase == W_SEED_EPHEMERAL_DRIVER_PHASE_ACQUIRE &&
        result.driver_result.provider_result.failure ==
            W_SEED_EPHEMERAL_PROVIDER_FAILURE_CONTAINMENT &&
        result.driver_result.provider_result.phase ==
            W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_SOURCE &&
        result.driver_result.provider_result.request_index == 1u &&
        result.driver_result.provider_result.backend_status ==
            W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK &&
        backend.source_open_calls == 1u &&
        pipeline_output_matches(&fixture, &before));
  w_seed_acquisition_storage_destroy(&storage);

  backend = (pipeline_backend){
      .root_text = restaurant_source,
      .child_text = kitchen_source,
      .mutated_child_text = mutated_kitchen_source,
      .root_containment_inside = true,
      .child_containment_inside = true,
      .mutate_revalidation = true};
  pipeline_fixture_init(&fixture, &backend);
  pipeline_poison_output(&fixture);
  before = pipeline_snapshot_output(&fixture);
  storage = (w_seed_acquisition_storage){0};
  CHECK(w_seed_acquisition_storage_init(&storage));
  CHECK(pipeline_pregrow(&storage, 2u, 128u, 256u));
  input = pipeline_input(&fixture, &storage);
  CHECK(w_seed_acquisition_pipeline_run(&input, &result) ==
        W_SEED_ACQUISITION_PIPELINE_INVALID);
  CHECK(result.attempts == 1u && pipeline_result_output_is_empty(&result) &&
        result.driver_result.failure ==
            W_SEED_EPHEMERAL_DRIVER_FAILURE_PROVIDER &&
        result.driver_result.provider_result.failure ==
            W_SEED_EPHEMERAL_PROVIDER_FAILURE_SNAPSHOT &&
        result.driver_result.provider_result.phase ==
            W_SEED_EPHEMERAL_PROVIDER_PHASE_REVALIDATE &&
        result.driver_result.provider_result.request_index == 1u &&
        result.driver_result.provider_result.backend_status ==
            W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK &&
        backend.source_open_calls == 1u && backend.revalidate_calls == 3u &&
        pipeline_output_matches(&fixture, &before));
  w_seed_acquisition_storage_destroy(&storage);

  backend = (pipeline_backend){
      .root_text = restaurant_source,
      .child_text = kitchen_source,
      .mutated_child_text = mutated_kitchen_source,
      .root_containment_inside = true,
      .child_containment_inside = true};
  pipeline_fixture_init(&fixture, &backend);
  fixture.output.graph.edge_capacity = 0u;
  pipeline_poison_output(&fixture);
  before = pipeline_snapshot_output(&fixture);
  storage = (w_seed_acquisition_storage){0};
  CHECK(w_seed_acquisition_storage_init(&storage));
  CHECK(pipeline_pregrow(&storage, 2u, 128u, 256u));
  input = pipeline_input(&fixture, &storage);
  CHECK(w_seed_acquisition_pipeline_run(&input, &result) ==
        W_SEED_ACQUISITION_PIPELINE_CAPACITY);
  CHECK(result.attempts == 1u && pipeline_result_output_is_empty(&result) &&
        result.retry.status == W_SEED_ACQUISITION_RETRY_CAPACITY &&
        result.retry.detail ==
            W_SEED_ACQUISITION_RETRY_DETAIL_NON_RESIZABLE &&
        result.driver_result.capacity_field ==
            W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_GRAPH_EDGE &&
        pipeline_output_matches(&fixture, &before));
  w_seed_acquisition_storage_destroy(&storage);
  return true;
}

static bool test_pipeline_backend_statuses(void) {
  const w_seed_acquisition_pipeline_status expected[2] = {
      W_SEED_ACQUISITION_PIPELINE_UNSUPPORTED,
      W_SEED_ACQUISITION_PIPELINE_IO};
  for (size_t index = 0u; index < 2u; index += 1u) {
    pipeline_backend backend = {
        .root_text = root_only_source,
        .child_text = kitchen_source,
        .mutated_child_text = mutated_kitchen_source,
        .root_containment_inside = true,
        .child_containment_inside = true,
        .unsupported_root = index == 0u,
        .io_read = index == 1u};
    pipeline_fixture fixture;
    pipeline_fixture_init(&fixture, &backend);
    pipeline_poison_output(&fixture);
    const pipeline_output_snapshot before = pipeline_snapshot_output(&fixture);
    w_seed_acquisition_storage storage = {0};
    CHECK(w_seed_acquisition_storage_init(&storage));
    const w_seed_acquisition_pipeline_input input =
        pipeline_input(&fixture, &storage);
    w_seed_acquisition_pipeline_result result;
    CHECK(w_seed_acquisition_pipeline_run(&input, &result) == expected[index]);
    CHECK(result.status == expected[index] && result.attempts == 1u &&
          result.retry.status == W_SEED_ACQUISITION_RETRY_NOT_RUN &&
          pipeline_result_output_is_empty(&result) &&
          pipeline_output_matches(&fixture, &before));
    w_seed_acquisition_storage_destroy(&storage);
  }
  return true;
}

static bool test_pipeline_retry_then_terminal(void) {
  pipeline_backend backend = {
      .root_text = root_only_source,
      .child_text = kitchen_source,
      .mutated_child_text = mutated_kitchen_source,
      .root_containment_inside = true,
      .child_containment_inside = true,
      .io_after_first_attempt = true};
  pipeline_fixture fixture;
  pipeline_fixture_init(&fixture, &backend);
  pipeline_poison_output(&fixture);
  const pipeline_output_snapshot before = pipeline_snapshot_output(&fixture);
  w_seed_acquisition_storage storage = {0};
  CHECK(w_seed_acquisition_storage_init(&storage));
  CHECK(w_seed_acquisition_storage_grow_nodes(&storage, 0u, 256u) ==
        W_SEED_ACQUISITION_STORAGE_OK);
  const w_seed_acquisition_pipeline_input input =
      pipeline_input(&fixture, &storage);
  w_seed_acquisition_pipeline_result result;
  CHECK(w_seed_acquisition_pipeline_run(&input, &result) ==
        W_SEED_ACQUISITION_PIPELINE_IO);
  CHECK(result.attempts == 2u && backend.root_open_calls == 2u &&
        result.driver_result.status == W_SEED_EPHEMERAL_DRIVER_IO &&
        result.retry.status == W_SEED_ACQUISITION_RETRY_OK &&
        result.retry.action == W_SEED_ACQUISITION_RETRY_RETRY &&
        result.retry.detail == W_SEED_ACQUISITION_RETRY_DETAIL_NONE &&
        storage.staging_capacity[0u] >= sizeof(root_only_source) - 1u &&
        pipeline_result_output_is_empty(&result) &&
        pipeline_output_matches(&fixture, &before));
  w_seed_acquisition_storage_destroy(&storage);
  return true;
}

static bool test_pipeline_canonical_alias(void) {
  pipeline_backend backend = {
      .root_text = restaurant_source,
      .child_text = kitchen_source,
      .mutated_child_text = mutated_kitchen_source,
      .root_containment_inside = true,
      .child_containment_inside = true,
      .canonical_alias_child = true};
  pipeline_fixture fixture;
  pipeline_fixture_init(&fixture, &backend);
  pipeline_poison_output(&fixture);
  const pipeline_output_snapshot before = pipeline_snapshot_output(&fixture);
  w_seed_acquisition_storage storage = {0};
  CHECK(w_seed_acquisition_storage_init(&storage));
  CHECK(pipeline_pregrow(&storage, 2u, 128u, 256u));
  const w_seed_acquisition_pipeline_input input =
      pipeline_input(&fixture, &storage);
  w_seed_acquisition_pipeline_result result;
  CHECK(w_seed_acquisition_pipeline_run(&input, &result) ==
        W_SEED_ACQUISITION_PIPELINE_INVALID);
  CHECK(result.attempts == 1u &&
        result.driver_result.failure ==
            W_SEED_EPHEMERAL_DRIVER_FAILURE_PROVIDER &&
        result.driver_result.provider_result.failure ==
            W_SEED_EPHEMERAL_PROVIDER_FAILURE_CANONICAL_ALIAS &&
        result.driver_result.provider_result.request_index == 1u &&
        backend.source_open_calls == 1u &&
        pipeline_result_output_is_empty(&result) &&
        pipeline_output_matches(&fixture, &before));
  w_seed_acquisition_storage_destroy(&storage);
  return true;
}

static bool test_pipeline_active_storage_guards(void) {
  pipeline_backend backend = {
      .root_text = root_only_source,
      .child_text = kitchen_source,
      .mutated_child_text = mutated_kitchen_source,
      .root_containment_inside = true,
      .child_containment_inside = true};
  pipeline_fixture fixture;
  pipeline_fixture_init(&fixture, &backend);
  pipeline_poison_output(&fixture);
  const pipeline_output_snapshot before = pipeline_snapshot_output(&fixture);
  w_seed_acquisition_storage storage = {0};
  CHECK(w_seed_acquisition_storage_init(&storage));
  storage.pipeline_active = true;
  CHECK(w_seed_acquisition_storage_grow_source(&storage, 0u, 8u) ==
            W_SEED_ACQUISITION_STORAGE_INVALID &&
        w_seed_acquisition_storage_grow_nodes(&storage, 0u, 8u) ==
            W_SEED_ACQUISITION_STORAGE_INVALID);
  w_seed_ephemeral_driver_slot slots_before[PIPELINE_SOURCES];
  w_seed_ephemeral_provider_request requests_before[PIPELINE_SOURCES];
  (void)memcpy(slots_before, fixture.slots, sizeof(slots_before));
  (void)memcpy(requests_before, fixture.requests, sizeof(requests_before));
  CHECK(!w_seed_acquisition_storage_bind_driver(&storage, &fixture.scratch) &&
        memcmp(fixture.slots, slots_before, sizeof(slots_before)) == 0 &&
        memcmp(fixture.requests, requests_before,
               sizeof(requests_before)) == 0);
  const w_seed_acquisition_storage active_before_destroy = storage;
  w_seed_acquisition_storage_destroy(&storage);
  CHECK(memcmp(&storage, &active_before_destroy, sizeof(storage)) == 0);
  const w_seed_acquisition_pipeline_input input =
      pipeline_input(&fixture, &storage);
  w_seed_acquisition_pipeline_result result;
  CHECK(w_seed_acquisition_pipeline_run(&input, &result) ==
        W_SEED_ACQUISITION_PIPELINE_INVALID);
  CHECK(result.attempts == 0u && backend.root_open_calls == 0u &&
        pipeline_result_output_is_empty(&result) &&
        pipeline_output_matches(&fixture, &before));
  storage.pipeline_active = false;
  w_seed_acquisition_storage_destroy(&storage);
  return true;
}

static bool test_pipeline_shallow_preflight(void) {
  pipeline_backend backend = {
      .root_text = root_only_source,
      .child_text = kitchen_source,
      .mutated_child_text = mutated_kitchen_source,
      .root_containment_inside = true,
      .child_containment_inside = true};
  pipeline_fixture fixture;
  pipeline_fixture_init(&fixture, &backend);
  w_seed_acquisition_storage storage = {0};
  CHECK(w_seed_acquisition_storage_init(&storage));
  w_seed_acquisition_pipeline_input good = pipeline_input(&fixture, &storage);
  union {
    max_align_t alignment;
    unsigned char bytes[sizeof(w_seed_acquisition_pipeline_input) + 1u];
  } input_bytes;
  (void)memcpy(input_bytes.bytes + 1u, &good, sizeof(good));
  w_seed_acquisition_pipeline_result result;
  (void)memset(&result, 0x61, sizeof(result));
  const w_seed_acquisition_pipeline_result result_before = result;
  CHECK(w_seed_acquisition_pipeline_run(
            (const w_seed_acquisition_pipeline_input *)(const void *)(
                input_bytes.bytes + 1u),
            &result) == W_SEED_ACQUISITION_PIPELINE_INVALID &&
        memcmp(&result, &result_before, sizeof(result)) == 0 &&
        backend.root_open_calls == 0u);

  union {
    max_align_t alignment;
    unsigned char bytes[sizeof(w_seed_acquisition_pipeline_result) + 1u];
  } result_bytes;
  (void)memset(&result_bytes, 0x62, sizeof(result_bytes));
  unsigned char result_bytes_before[sizeof(result_bytes.bytes)];
  (void)memcpy(result_bytes_before, result_bytes.bytes,
               sizeof(result_bytes_before));
  CHECK(w_seed_acquisition_pipeline_run(
            &good,
            (w_seed_acquisition_pipeline_result *)(void *)(
                result_bytes.bytes + 1u)) ==
            W_SEED_ACQUISITION_PIPELINE_INVALID &&
        memcmp(result_bytes.bytes, result_bytes_before,
               sizeof(result_bytes_before)) == 0);

  for (size_t variant = 0u; variant < 7u; variant += 1u) {
    backend = (pipeline_backend){
        .root_text = root_only_source,
        .child_text = kitchen_source,
        .mutated_child_text = mutated_kitchen_source,
        .root_containment_inside = true,
        .child_containment_inside = true};
    pipeline_fixture_init(&fixture, &backend);
    good = pipeline_input(&fixture, &storage);
    switch (variant) {
      case 0u:
        good.driver_input =
            (const w_seed_ephemeral_driver_input *)(const void *)(
                (const unsigned char *)&fixture.driver_input + 1u);
        break;
      case 1u:
        good.scratch = (w_seed_ephemeral_driver_scratch *)(void *)(
            (unsigned char *)&fixture.scratch + 1u);
        break;
      case 2u:
        good.output = (w_seed_ephemeral_driver_output *)(void *)(
            (unsigned char *)&fixture.output + 1u);
        break;
      case 3u:
        good.storage = (w_seed_acquisition_storage *)(void *)(
            (unsigned char *)&storage + 1u);
        break;
      case 4u:
        fixture.scratch.graph_scratch =
            (w_seed_ephemeral_graph_scratch *)(void *)(
                (unsigned char *)&fixture.graph_scratch + 1u);
        break;
      case 5u:
        fixture.scratch.slots = (w_seed_ephemeral_driver_slot *)(void *)(
            (unsigned char *)fixture.slots + 1u);
        break;
      case 6u:
        fixture.scratch.requests =
            (w_seed_ephemeral_provider_request *)(void *)(
                (unsigned char *)fixture.requests + 1u);
        break;
      default:
        return false;
    }
    (void)memset(&result, 0x63, sizeof(result));
    const w_seed_acquisition_pipeline_result nested_before = result;
    CHECK(w_seed_acquisition_pipeline_run(&good, &result) ==
              W_SEED_ACQUISITION_PIPELINE_INVALID &&
          memcmp(&result, &nested_before, sizeof(result)) == 0 &&
          backend.root_open_calls == 0u);
  }

  pipeline_fixture_init(&fixture, &backend);
  const uintptr_t slot_alignment =
      (uintptr_t)_Alignof(w_seed_ephemeral_driver_slot);
  fixture.scratch.slots = (w_seed_ephemeral_driver_slot *)(uintptr_t)(
      UINTPTR_MAX - UINTPTR_MAX % slot_alignment);
  fixture.scratch.slot_capacity = 2u;
  good = pipeline_input(&fixture, &storage);
  (void)memset(&result, 0x64, sizeof(result));
  const w_seed_acquisition_pipeline_result overflow_before = result;
  CHECK(w_seed_acquisition_pipeline_run(&good, &result) ==
            W_SEED_ACQUISITION_PIPELINE_INVALID &&
        memcmp(&result, &overflow_before, sizeof(result)) == 0);

  for (size_t alias = 0u; alias < 3u; alias += 1u) {
    backend = (pipeline_backend){
        .root_text = root_only_source,
        .child_text = kitchen_source,
        .mutated_child_text = mutated_kitchen_source,
        .root_containment_inside = true,
        .child_containment_inside = true};
    pipeline_fixture_init(&fixture, &backend);
    pipeline_poison_output(&fixture);
    const pipeline_output_snapshot alias_output_before =
        pipeline_snapshot_output(&fixture);
    good = pipeline_input(&fixture, &storage);
    switch (alias) {
      case 0u:
        good.driver_input =
            (const w_seed_ephemeral_driver_input *)(const void *)&fixture.scratch;
        break;
      case 1u:
        fixture.scratch.slots =
            (w_seed_ephemeral_driver_slot *)(void *)&fixture.scratch;
        break;
      case 2u:
        fixture.scratch.requests =
            (w_seed_ephemeral_provider_request *)(void *)fixture.slots;
        break;
      default:
        return false;
    }
    (void)memset(&result, (int)(0x65u + alias), sizeof(result));
    const w_seed_acquisition_pipeline_result alias_result_before = result;
    CHECK(w_seed_acquisition_pipeline_run(&good, &result) ==
              W_SEED_ACQUISITION_PIPELINE_INVALID &&
          memcmp(&result, &alias_result_before, sizeof(result)) == 0 &&
          pipeline_output_matches(&fixture, &alias_output_before) &&
          backend.root_open_calls == 0u);
  }
  w_seed_acquisition_storage_destroy(&storage);
  return true;
}

static bool test_pipeline_context_ranges(void) {
  pipeline_backend backend = {
      .root_text = root_only_source,
      .child_text = kitchen_source,
      .mutated_child_text = mutated_kitchen_source,
      .root_containment_inside = true,
      .child_containment_inside = true};
  pipeline_fixture fixture;
  pipeline_fixture_init(&fixture, &backend);
  pipeline_poison_output(&fixture);
  pipeline_output_snapshot output_before = pipeline_snapshot_output(&fixture);
  w_seed_acquisition_storage storage = {0};
  CHECK(w_seed_acquisition_storage_init(&storage));
  w_seed_acquisition_pipeline_input input = pipeline_input(&fixture, &storage);
  w_seed_acquisition_pipeline_result result;

  input.backend_context_size = 0u;
  (void)memset(&result, 0x71, sizeof(result));
  const w_seed_acquisition_pipeline_result nonnull_zero_before = result;
  CHECK(w_seed_acquisition_pipeline_run(&input, &result) ==
            W_SEED_ACQUISITION_PIPELINE_INVALID &&
        memcmp(&result, &nonnull_zero_before, sizeof(result)) == 0 &&
        backend.root_open_calls == 0u);

  fixture.driver_input.backend.context = NULL;
  input = pipeline_input(&fixture, &storage);
  (void)memset(&result, 0x72, sizeof(result));
  const w_seed_acquisition_pipeline_result null_nonzero_before = result;
  CHECK(w_seed_acquisition_pipeline_run(&input, &result) ==
            W_SEED_ACQUISITION_PIPELINE_INVALID &&
        memcmp(&result, &null_nonzero_before, sizeof(result)) == 0);
  input.backend_context_size = 0u;
  CHECK(w_seed_acquisition_pipeline_run(&input, &result) ==
            W_SEED_ACQUISITION_PIPELINE_INVALID &&
        result.attempts == 1u && pipeline_output_matches(&fixture, &output_before));

  pipeline_fixture_init(&fixture, &backend);
  fixture.driver_input.backend.context =
      (void *)(uintptr_t)(UINTPTR_MAX - (uintptr_t)3u);
  input = pipeline_input(&fixture, &storage);
  input.backend_context_size = 8u;
  (void)memset(&result, 0x73, sizeof(result));
  const w_seed_acquisition_pipeline_result context_overflow_before = result;
  CHECK(w_seed_acquisition_pipeline_run(&input, &result) ==
            W_SEED_ACQUISITION_PIPELINE_INVALID &&
        memcmp(&result, &context_overflow_before, sizeof(result)) == 0);

  pipeline_fixture_init(&fixture, &backend);
  pipeline_poison_output(&fixture);
  pipeline_backend *output_backend =
      (pipeline_backend *)(void *)fixture.inventory;
  *output_backend = (pipeline_backend){
      .root_text = root_only_source,
      .child_text = kitchen_source,
      .mutated_child_text = mutated_kitchen_source,
      .root_containment_inside = true,
      .child_containment_inside = true,
      .io_read = true};
  fixture.driver_input.backend = pipeline_backend_vtable(output_backend);
  output_before = pipeline_snapshot_output(&fixture);
  input = pipeline_input(&fixture, &storage);
  (void)memset(&result, 0x74, sizeof(result));
  const w_seed_acquisition_pipeline_result output_overlap_before = result;
  CHECK(w_seed_acquisition_pipeline_run(&input, &result) ==
            W_SEED_ACQUISITION_PIPELINE_INVALID &&
        memcmp(&result, &output_overlap_before, sizeof(result)) == 0 &&
        output_backend->root_open_calls == 0u &&
        pipeline_output_matches(&fixture, &output_before));

  union {
    max_align_t alignment;
    pipeline_backend backend;
    w_seed_acquisition_pipeline_result result;
    unsigned char bytes[512u];
  } context_result;
  (void)memset(&context_result, 0, sizeof(context_result));
  context_result.backend = (pipeline_backend){
      .root_text = root_only_source,
      .child_text = kitchen_source,
      .mutated_child_text = mutated_kitchen_source,
      .root_containment_inside = true,
      .child_containment_inside = true};
  pipeline_fixture_init(&fixture, &context_result.backend);
  context_result.result.status = W_SEED_ACQUISITION_PIPELINE_INVALID;
  unsigned char context_result_before[sizeof(context_result)];
  (void)memcpy(context_result_before, &context_result,
               sizeof(context_result_before));
  input = pipeline_input(&fixture, &storage);
  CHECK(w_seed_acquisition_pipeline_run(&input, &context_result.result) ==
            W_SEED_ACQUISITION_PIPELINE_INVALID &&
        memcmp(&context_result, context_result_before,
               sizeof(context_result_before)) == 0);

  pipeline_fixture_init(&fixture, &backend);
  pipeline_poison_output(&fixture);
  output_before = pipeline_snapshot_output(&fixture);
  fixture.driver_input.backend.context = &storage;
  input = pipeline_input(&fixture, &storage);
  const w_seed_acquisition_storage storage_before = storage;
  (void)memset(&result, 0x75, sizeof(result));
  const w_seed_acquisition_pipeline_result storage_overlap_before = result;
  CHECK(w_seed_acquisition_pipeline_run(&input, &result) ==
            W_SEED_ACQUISITION_PIPELINE_INVALID &&
        memcmp(&result, &storage_overlap_before, sizeof(result)) == 0 &&
        memcmp(&storage, &storage_before, sizeof(storage)) == 0 &&
        pipeline_output_matches(&fixture, &output_before));
  w_seed_acquisition_storage_destroy(&storage);

  pipeline_adjacent_context adjacent = {0};
  adjacent.backend = (pipeline_backend){
      .root_text = root_only_source,
      .child_text = kitchen_source,
      .mutated_child_text = mutated_kitchen_source,
      .root_containment_inside = true,
      .child_containment_inside = true};
  pipeline_fixture_init(&adjacent.fixture, &adjacent.backend);
  storage = (w_seed_acquisition_storage){0};
  CHECK(w_seed_acquisition_storage_init(&storage));
  CHECK(pipeline_pregrow(&storage, 1u, 128u, 256u));
  input = pipeline_input(&adjacent.fixture, &storage);
  CHECK(w_seed_acquisition_pipeline_run(&input, &result) ==
            W_SEED_ACQUISITION_PIPELINE_OK &&
        result.attempts == 1u && adjacent.backend.root_open_calls == 1u);
  w_seed_acquisition_storage_destroy(&storage);
  return true;
}

static bool test_pipeline_allocation_failure(void) {
  pipeline_backend backend = {
      .root_text = root_only_source,
      .child_text = kitchen_source,
      .mutated_child_text = mutated_kitchen_source,
      .root_containment_inside = true,
      .child_containment_inside = true};
  pipeline_fixture fixture;
  pipeline_fixture_init(&fixture, &backend);
  pipeline_poison_output(&fixture);
  pipeline_output_snapshot before = pipeline_snapshot_output(&fixture);
  w_seed_acquisition_storage storage = {0};
  reset_probe(1u);
  CHECK(w_seed_acquisition_storage_init_with_allocator(
      &storage, probe_allocate, probe_deallocate));
  w_seed_acquisition_pipeline_input input = pipeline_input(&fixture, &storage);
  w_seed_acquisition_pipeline_result result;
  CHECK(w_seed_acquisition_pipeline_run(&input, &result) ==
        W_SEED_ACQUISITION_PIPELINE_ALLOCATION);
  CHECK(result.attempts == 1u &&
        result.driver_result.status == W_SEED_EPHEMERAL_DRIVER_CAPACITY &&
        result.driver_result.capacity_field ==
            W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PROVIDER &&
        result.driver_result.failure ==
            W_SEED_EPHEMERAL_DRIVER_FAILURE_PROVIDER &&
        result.driver_result.provider_result.capacity_field ==
            W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES &&
        result.driver_result.required_capacity ==
            sizeof(root_only_source) - 1u &&
        result.retry.status == W_SEED_ACQUISITION_RETRY_ALLOCATION &&
        result.retry.action == W_SEED_ACQUISITION_RETRY_TERMINAL &&
        result.retry.detail == W_SEED_ACQUISITION_RETRY_DETAIL_ALLOCATION &&
        pipeline_result_output_is_empty(&result) &&
        probe.allocations == 1u && probe.deallocations == 1u &&
        storage.staging_total == 0u && storage.revalidation_total == 0u &&
        storage.published_total == 0u &&
        pipeline_output_matches(&fixture, &before));
  w_seed_acquisition_storage_destroy(&storage);

  return true;
}

typedef union {
  max_align_t alignment;
  w_seed_acquisition_pipeline_result result;
  w_seed_ephemeral_graph_inventory_item inventory[PIPELINE_SOURCES];
  unsigned char bytes[512u];
} pipeline_result_output_alias;

static bool test_pipeline_preflight_aliases(void) {
  pipeline_backend backend = {
      .root_text = root_only_source,
      .child_text = kitchen_source,
      .mutated_child_text = mutated_kitchen_source,
      .root_containment_inside = true,
      .child_containment_inside = true};
  pipeline_fixture fixture;
  pipeline_fixture_init(&fixture, &backend);
  pipeline_poison_output(&fixture);
  w_seed_acquisition_storage storage = {0};
  CHECK(w_seed_acquisition_storage_init(&storage));
  pipeline_result_output_alias alias;
  (void)memset(&alias, 0x6B, sizeof(alias));
  alias.result.status = W_SEED_ACQUISITION_PIPELINE_INVALID;
  const pipeline_result_output_alias alias_before = alias;
  fixture.output.graph.inventory = alias.inventory;
  const w_seed_ephemeral_driver_output descriptor_before = fixture.output;
  const w_seed_acquisition_pipeline_input input =
      pipeline_input(&fixture, &storage);
  CHECK(w_seed_acquisition_pipeline_run(&input, &alias.result) ==
        W_SEED_ACQUISITION_PIPELINE_INVALID);
  CHECK(memcmp(&alias, &alias_before, sizeof(alias)) == 0 &&
        memcmp(&fixture.output, &descriptor_before,
               sizeof(fixture.output)) == 0 &&
        backend.root_open_calls == 0u);

  pipeline_fixture_init(&fixture, &backend);
  pipeline_poison_output(&fixture);
  const pipeline_output_snapshot before = pipeline_snapshot_output(&fixture);
  fixture.output.documents = fixture.candidate_documents;
  const w_seed_ephemeral_driver_output overlapping_descriptor_before =
      fixture.output;
  const w_seed_acquisition_pipeline_input overlapping =
      pipeline_input(&fixture, &storage);
  w_seed_acquisition_pipeline_result result;
  (void)memset(&result, 0x5C, sizeof(result));
  const w_seed_acquisition_pipeline_result overlapping_result_before = result;
  CHECK(w_seed_acquisition_pipeline_run(&overlapping, &result) ==
        W_SEED_ACQUISITION_PIPELINE_INVALID);
  CHECK(memcmp(&result, &overlapping_result_before, sizeof(result)) == 0 &&
        backend.root_open_calls == 0u &&
        memcmp(&fixture.output, &overlapping_descriptor_before,
               sizeof(fixture.output)) == 0 &&
        memcmp(fixture.documents, before.documents,
               sizeof(fixture.documents)) == 0 &&
        memcmp(fixture.inventory, before.inventory,
               sizeof(fixture.inventory)) == 0 &&
        memcmp(fixture.edges, before.edges, sizeof(fixture.edges)) == 0 &&
        memcmp(fixture.document_order, before.document_order,
               sizeof(fixture.document_order)) == 0 &&
        memcmp(fixture.resolved, before.resolved,
               sizeof(fixture.resolved)) == 0);
  w_seed_acquisition_storage_destroy(&storage);
  return true;
}

int main(void) {
  if (!test_init_live_destroy_reinit() ||
      !test_source_allocation_rollback() ||
      !test_node_rollback_and_bounds() ||
      !test_owner_alias_and_invalid_destroy() ||
      !test_allocator_overlap_and_adjacency() ||
      !test_bind_preserves_fields_and_rebinds() ||
      !test_bind_preflight_aliases() ||
      !test_retry_provider_and_forged_envelopes() ||
      !test_retry_nodes_fixed_and_limit() ||
      !test_retry_allocation_failure() ||
      !test_pipeline_restaurant() ||
      !test_pipeline_root_retry_and_reuse() ||
      !test_pipeline_barriers_and_fixed_output() ||
      !test_pipeline_backend_statuses() ||
      !test_pipeline_retry_then_terminal() ||
      !test_pipeline_canonical_alias() ||
      !test_pipeline_active_storage_guards() ||
      !test_pipeline_shallow_preflight() ||
      !test_pipeline_context_ranges() ||
      !test_pipeline_allocation_failure() ||
      !test_pipeline_preflight_aliases())
    return 1;
  (void)puts("w_seed_acquisition_tests: ok");
  return 0;
}
