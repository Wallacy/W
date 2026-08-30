#include "check_storage.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#define CHECK(condition)                                                      \
  do {                                                                        \
    if (!(condition)) {                                                       \
      (void)fprintf(stderr, "check storage test failed: %s (%s:%d)\n",      \
                    #condition, __FILE__, __LINE__);                         \
      return false;                                                           \
    }                                                                         \
  } while (0)

static bool test_initial_request_is_empty(void) {
  w_seed_check_storage storage = {0};
  w_seed_ephemeral_provider_request request;
  (void)memset(&request, 0, sizeof(request));
  CHECK(w_seed_check_storage_init(&storage));
  CHECK(w_seed_check_storage_bind_request(&storage, 0u, &request));
  CHECK(request.staging_bytes == NULL && request.staging_capacity == 0u);
  CHECK(request.revalidation_bytes == NULL &&
        request.revalidation_capacity == 0u);
  CHECK(request.bytes == NULL && request.byte_capacity == 0u);
  CHECK(storage.json_staging == NULL && storage.json_final == NULL &&
        storage.json_staging_capacity == 0u &&
        storage.json_final_capacity == 0u);
  w_seed_check_storage_destroy(&storage);
  return true;
}

static bool test_growth_preserves_each_arena(void) {
  w_seed_check_storage storage = {0};
  w_seed_ephemeral_provider_request request;
  (void)memset(&request, 0, sizeof(request));
  CHECK(w_seed_check_storage_init(&storage));
  CHECK(w_seed_check_storage_grow(&storage, 2u, 7u) ==
        W_SEED_CHECK_STORAGE_OK);
  CHECK(storage.acquisition.staging_capacity[2u] == 8u &&
        storage.acquisition.revalidation_capacity[2u] == 8u &&
        storage.acquisition.published_capacity[2u] == 8u);
  CHECK(storage.acquisition.staging_total == 8u && storage.acquisition.revalidation_total == 8u &&
        storage.acquisition.published_total == 8u);
  for (size_t index = 0u; index < 7u; index += 1u) {
    storage.acquisition.staging_bytes[2u][index] = (uint8_t)(index + 1u);
    storage.acquisition.revalidation_bytes[2u][index] = (uint8_t)(index + 11u);
    storage.acquisition.published_bytes[2u][index] = (uint8_t)(index + 21u);
  }
  CHECK(w_seed_check_storage_grow(&storage, 2u, 9u) ==
        W_SEED_CHECK_STORAGE_OK);
  for (size_t index = 0u; index < 7u; index += 1u) {
    CHECK(storage.acquisition.staging_bytes[2u][index] == (uint8_t)(index + 1u));
    CHECK(storage.acquisition.revalidation_bytes[2u][index] ==
          (uint8_t)(index + 11u));
    CHECK(storage.acquisition.published_bytes[2u][index] == (uint8_t)(index + 21u));
  }
  CHECK(w_seed_check_storage_bind_request(&storage, 2u, &request));
  CHECK(request.staging_bytes == storage.acquisition.staging_bytes[2u] &&
        request.staging_capacity == 16u);
  CHECK(request.revalidation_bytes == storage.acquisition.revalidation_bytes[2u] &&
        request.revalidation_capacity == 16u);
  CHECK(request.bytes == storage.acquisition.published_bytes[2u] &&
        request.byte_capacity == 16u);
  CHECK(storage.acquisition.staging_total == 16u &&
        storage.acquisition.revalidation_total == 16u && storage.acquisition.published_total == 16u);
  uint8_t *stable_staging = storage.acquisition.staging_bytes[2u];
  uint8_t *stable_revalidation = storage.acquisition.revalidation_bytes[2u];
  uint8_t *stable_published = storage.acquisition.published_bytes[2u];
  CHECK(w_seed_check_storage_grow(&storage, 2u, 12u) ==
        W_SEED_CHECK_STORAGE_OK);
  CHECK(storage.acquisition.staging_bytes[2u] == stable_staging &&
        storage.acquisition.revalidation_bytes[2u] == stable_revalidation &&
        storage.acquisition.published_bytes[2u] == stable_published &&
        storage.acquisition.staging_capacity[2u] == 16u &&
        storage.acquisition.revalidation_capacity[2u] == 16u &&
        storage.acquisition.published_capacity[2u] == 16u);
  w_seed_check_storage_destroy(&storage);
  return true;
}

static bool test_growth_keeps_requests_disjoint(void) {
  w_seed_check_storage storage = {0};
  CHECK(w_seed_check_storage_init(&storage));
  CHECK(w_seed_check_storage_grow(&storage, 0u, 4u) ==
        W_SEED_CHECK_STORAGE_OK);
  CHECK(w_seed_check_storage_grow(&storage, 1u, 3u) ==
        W_SEED_CHECK_STORAGE_OK);
  CHECK(storage.acquisition.staging_total == 8u && storage.acquisition.revalidation_total == 8u &&
        storage.acquisition.published_total == 8u);
  CHECK(storage.acquisition.staging_bytes[0u] != storage.acquisition.staging_bytes[1u]);
  CHECK(storage.acquisition.revalidation_bytes[0u] != storage.acquisition.revalidation_bytes[1u]);
  CHECK(storage.acquisition.published_bytes[0u] != storage.acquisition.published_bytes[1u]);
  CHECK(storage.acquisition.staging_bytes[0u] != storage.acquisition.revalidation_bytes[0u] &&
        storage.acquisition.staging_bytes[0u] != storage.acquisition.published_bytes[0u]);
  w_seed_check_storage_destroy(&storage);
  return true;
}

static bool test_json_growth_preserves_pair(void) {
  w_seed_check_storage storage = {0};
  CHECK(w_seed_check_storage_init(&storage));
  CHECK(w_seed_check_storage_grow_json(&storage, 7u) ==
        W_SEED_CHECK_STORAGE_OK);
  CHECK(storage.json_staging_capacity == 8u &&
        storage.json_final_capacity == 8u);
  for (size_t index = 0u; index < 7u; index += 1u) {
    storage.json_staging[index] = (uint8_t)(index + 1u);
    storage.json_final[index] = (uint8_t)(index + 11u);
  }
  uint8_t *old_staging = storage.json_staging;
  uint8_t *old_final = storage.json_final;
  CHECK(w_seed_check_storage_grow_json(&storage, 9u) ==
        W_SEED_CHECK_STORAGE_OK);
  CHECK(storage.json_staging_capacity == 16u &&
        storage.json_final_capacity == 16u);
  for (size_t index = 0u; index < 7u; index += 1u) {
    CHECK(storage.json_staging[index] == (uint8_t)(index + 1u));
    CHECK(storage.json_final[index] == (uint8_t)(index + 11u));
  }
  CHECK(storage.json_staging != old_staging && storage.json_final != old_final);
  old_staging = storage.json_staging;
  old_final = storage.json_final;
  CHECK(w_seed_check_storage_grow_json(&storage, 12u) ==
        W_SEED_CHECK_STORAGE_OK);
  CHECK(storage.json_staging == old_staging && storage.json_final == old_final &&
        storage.json_staging_capacity == storage.json_final_capacity);
  w_seed_check_storage_destroy(&storage);
  return true;
}

static bool test_node_growth_preserves_data_and_binds_slot(void) {
  w_seed_check_storage storage = {0};
  w_seed_ephemeral_driver_slot slot;
  (void)memset(&slot, 0, sizeof(slot));
  CHECK(w_seed_check_storage_init(&storage));
  CHECK(w_seed_check_storage_grow_nodes(&storage, 2u, 7u) ==
        W_SEED_CHECK_STORAGE_OK);
  CHECK(storage.acquisition.node_capacity[2u] == 8u && storage.acquisition.node_total == 8u);
  (void)memset(storage.acquisition.nodes[2u], 0x5Au,
               7u * sizeof(storage.acquisition.nodes[2u][0]));
  CHECK(w_seed_check_storage_grow_nodes(&storage, 2u, 9u) ==
        W_SEED_CHECK_STORAGE_OK);
  CHECK(storage.acquisition.node_capacity[2u] == 16u && storage.acquisition.node_total == 16u);
  for (size_t index = 0u; index < 7u; index += 1u) {
    const uint8_t *bytes = (const uint8_t *)&storage.acquisition.nodes[2u][index];
    for (size_t byte = 0u; byte < sizeof(w_seed_cst_node); byte += 1u)
      CHECK(bytes[byte] == 0x5Au);
  }
  CHECK(w_seed_check_storage_bind_slot(&storage, 2u, &slot));
  CHECK(slot.nodes == storage.acquisition.nodes[2u] && slot.node_capacity == 16u);
  w_seed_check_storage_destroy(&storage);
  return true;
}

static bool test_node_aggregate_capacity_is_bounded(void) {
  w_seed_check_storage storage = {0};
  CHECK(w_seed_check_storage_init(&storage));
  for (size_t index = 0u; index < 8u; index += 1u) {
    CHECK(w_seed_check_storage_grow_nodes(
              &storage, index,
              (size_t)W_SEED_CHECK_STORAGE_MAX_NODES) ==
          W_SEED_CHECK_STORAGE_OK);
  }
  CHECK(storage.acquisition.node_total ==
        (size_t)W_SEED_CHECK_STORAGE_MAX_TOTAL_NODES);
  CHECK(w_seed_check_storage_grow_nodes(&storage, 8u, 1u) ==
        W_SEED_CHECK_STORAGE_CAPACITY);
  CHECK(storage.acquisition.node_total ==
        (size_t)W_SEED_CHECK_STORAGE_MAX_TOTAL_NODES);
  w_seed_check_storage_destroy(&storage);
  return true;
}

static bool test_capacity_is_atomic(void) {
  w_seed_check_storage storage = {0};
  CHECK(w_seed_check_storage_init(&storage));
  CHECK(w_seed_check_storage_grow(&storage, 0u, 5u) ==
        W_SEED_CHECK_STORAGE_OK);
  uint8_t *old_staging = storage.acquisition.staging_bytes[0u];
  uint8_t *old_revalidation = storage.acquisition.revalidation_bytes[0u];
  uint8_t *old_published = storage.acquisition.published_bytes[0u];
  CHECK(w_seed_check_storage_grow(
            &storage, 0u,
            (size_t)W_SEED_CHECK_STORAGE_MAX_SOURCE_BYTES + 1u) ==
        W_SEED_CHECK_STORAGE_CAPACITY);
  CHECK(storage.acquisition.staging_bytes[0u] == old_staging &&
        storage.acquisition.revalidation_bytes[0u] == old_revalidation &&
        storage.acquisition.published_bytes[0u] == old_published);
  CHECK(storage.acquisition.staging_capacity[0u] == 8u &&
        storage.acquisition.revalidation_capacity[0u] == 8u &&
        storage.acquisition.published_capacity[0u] == 8u);
  CHECK(w_seed_check_storage_grow(
            &storage, (size_t)W_SEED_CHECK_STORAGE_MAX_SOURCES, 1u) ==
        W_SEED_CHECK_STORAGE_INVALID);
  w_seed_check_storage_destroy(&storage);
  return true;
}

static bool test_aggregate_capacity_is_separate_and_atomic(void) {
  w_seed_check_storage storage = {0};
  CHECK(w_seed_check_storage_init(&storage));
  CHECK(w_seed_check_storage_grow(
            &storage, 0u,
            (size_t)W_SEED_CHECK_STORAGE_MAX_TOTAL_SOURCE_BYTES) ==
        W_SEED_CHECK_STORAGE_OK);
  uint8_t *old_staging = storage.acquisition.staging_bytes[0u];
  uint8_t *old_revalidation = storage.acquisition.revalidation_bytes[0u];
  uint8_t *old_published = storage.acquisition.published_bytes[0u];
  CHECK(w_seed_check_storage_grow(&storage, 1u, 1u) ==
        W_SEED_CHECK_STORAGE_CAPACITY);
  CHECK(storage.acquisition.staging_bytes[0u] == old_staging &&
        storage.acquisition.revalidation_bytes[0u] == old_revalidation &&
        storage.acquisition.published_bytes[0u] == old_published);
  CHECK(storage.acquisition.staging_total ==
            (size_t)W_SEED_CHECK_STORAGE_MAX_TOTAL_SOURCE_BYTES &&
        storage.acquisition.revalidation_total ==
            (size_t)W_SEED_CHECK_STORAGE_MAX_TOTAL_SOURCE_BYTES &&
        storage.acquisition.published_total ==
            (size_t)W_SEED_CHECK_STORAGE_MAX_TOTAL_SOURCE_BYTES);
  w_seed_check_storage_destroy(&storage);
  return true;
}

static bool test_aggregate_limit_uses_exact_fallback(void) {
  const size_t mebibyte = (size_t)1024u * (size_t)1024u;
  w_seed_check_storage storage = {0};
  CHECK(w_seed_check_storage_init(&storage));
  CHECK(w_seed_check_storage_grow(&storage, 0u, 8u * mebibyte) ==
        W_SEED_CHECK_STORAGE_OK);
  CHECK(w_seed_check_storage_grow(&storage, 1u, 4u * mebibyte) ==
        W_SEED_CHECK_STORAGE_OK);
  CHECK(w_seed_check_storage_grow(&storage, 2u, mebibyte) ==
        W_SEED_CHECK_STORAGE_OK);
  CHECK(storage.acquisition.staging_total == 13u * mebibyte);
  CHECK(w_seed_check_storage_grow(&storage, 3u, 3u * mebibyte) ==
        W_SEED_CHECK_STORAGE_OK);
  CHECK(storage.acquisition.staging_capacity[3u] == 3u * mebibyte &&
        storage.acquisition.revalidation_capacity[3u] == 3u * mebibyte &&
        storage.acquisition.published_capacity[3u] == 3u * mebibyte);
  CHECK(storage.acquisition.staging_total ==
            (size_t)W_SEED_CHECK_STORAGE_MAX_TOTAL_SOURCE_BYTES &&
        storage.acquisition.revalidation_total ==
            (size_t)W_SEED_CHECK_STORAGE_MAX_TOTAL_SOURCE_BYTES &&
        storage.acquisition.published_total ==
            (size_t)W_SEED_CHECK_STORAGE_MAX_TOTAL_SOURCE_BYTES);
  w_seed_check_storage_destroy(&storage);
  return true;
}

static bool test_bind_does_not_replace_non_byte_fields(void) {
  w_seed_check_storage storage = {0};
  w_seed_ephemeral_provider_request request;
  w_seed_source source;
  w_seed_ephemeral_graph_provider_facts facts;
  (void)memset(&request, 0, sizeof(request));
  (void)memset(&source, 0, sizeof(source));
  (void)memset(&facts, 0, sizeof(facts));
  request.source = &source;
  request.facts = &facts;
  request.source_id.data = "root";
  request.source_id.length = 4u;
  CHECK(w_seed_check_storage_init(&storage));
  CHECK(w_seed_check_storage_grow(&storage, 0u, 3u) ==
        W_SEED_CHECK_STORAGE_OK);
  CHECK(w_seed_check_storage_bind_request(&storage, 0u, &request));
  CHECK(request.source == &source && request.facts == &facts);
  CHECK(request.source_id.length == 4u &&
        memcmp(request.source_id.data, "root", 4u) == 0);
  w_seed_check_storage_destroy(&storage);
  return true;
}

typedef struct {
  size_t allocations;
  size_t deallocations;
  size_t fail_after;
} allocator_probe;

static allocator_probe probe;

static void *probe_allocate(size_t size) {
  if (probe.allocations >= probe.fail_after) return NULL;
  probe.allocations += 1u;
  return malloc(size);
}

static void probe_deallocate(void *pointer) {
  probe.deallocations += 1u;
  free(pointer);
}

static bool test_allocation_failure_is_atomic(void) {
  w_seed_check_storage storage = {0};
  probe.allocations = 0u;
  probe.deallocations = 0u;
  probe.fail_after = SIZE_MAX;
  CHECK(w_seed_check_storage_init_with_allocator(
      &storage, probe_allocate, probe_deallocate));
  CHECK(w_seed_check_storage_grow(&storage, 0u, 4u) ==
        W_SEED_CHECK_STORAGE_OK);
  for (size_t index = 0u; index < 4u; index += 1u) {
    storage.acquisition.staging_bytes[0u][index] = (uint8_t)(index + 1u);
    storage.acquisition.revalidation_bytes[0u][index] = (uint8_t)(index + 11u);
    storage.acquisition.published_bytes[0u][index] = (uint8_t)(index + 21u);
  }
  uint8_t *old_staging = storage.acquisition.staging_bytes[0u];
  uint8_t *old_revalidation = storage.acquisition.revalidation_bytes[0u];
  uint8_t *old_published = storage.acquisition.published_bytes[0u];
  const size_t old_allocations = probe.allocations;
  probe.fail_after = old_allocations + 1u;
  CHECK(w_seed_check_storage_grow(&storage, 0u, 5u) ==
        W_SEED_CHECK_STORAGE_ALLOCATION);
  CHECK(storage.acquisition.staging_bytes[0u] == old_staging &&
        storage.acquisition.revalidation_bytes[0u] == old_revalidation &&
        storage.acquisition.published_bytes[0u] == old_published);
  CHECK(storage.acquisition.staging_capacity[0u] == 4u &&
        storage.acquisition.revalidation_capacity[0u] == 4u &&
        storage.acquisition.published_capacity[0u] == 4u);
  CHECK(storage.acquisition.staging_total == 4u && storage.acquisition.revalidation_total == 4u &&
        storage.acquisition.published_total == 4u);
  for (size_t index = 0u; index < 4u; index += 1u) {
    CHECK(storage.acquisition.staging_bytes[0u][index] == (uint8_t)(index + 1u));
    CHECK(storage.acquisition.revalidation_bytes[0u][index] ==
          (uint8_t)(index + 11u));
    CHECK(storage.acquisition.published_bytes[0u][index] == (uint8_t)(index + 21u));
  }
  CHECK(probe.allocations == old_allocations + 1u);
  CHECK(probe.deallocations == 1u);
  w_seed_check_storage_destroy(&storage);
  CHECK(probe.deallocations == 4u);
  return true;
}

static bool test_node_allocation_failure_is_atomic(void) {
  w_seed_check_storage storage = {0};
  probe.allocations = 0u;
  probe.deallocations = 0u;
  probe.fail_after = SIZE_MAX;
  CHECK(w_seed_check_storage_init_with_allocator(
      &storage, probe_allocate, probe_deallocate));
  CHECK(w_seed_check_storage_grow_nodes(&storage, 0u, 4u) ==
        W_SEED_CHECK_STORAGE_OK);
  w_seed_cst_node *old_nodes = storage.acquisition.nodes[0u];
  probe.fail_after = probe.allocations;
  CHECK(w_seed_check_storage_grow_nodes(&storage, 0u, 5u) ==
        W_SEED_CHECK_STORAGE_ALLOCATION);
  CHECK(storage.acquisition.nodes[0u] == old_nodes && storage.acquisition.node_capacity[0u] == 4u &&
        storage.acquisition.node_total == 4u);
  CHECK(probe.deallocations == 0u);
  w_seed_check_storage_destroy(&storage);
  CHECK(probe.deallocations == 1u);
  return true;
}

static bool test_json_allocation_failure_is_atomic(void) {
  for (size_t failed = 0u; failed < 2u; failed += 1u) {
    w_seed_check_storage storage = {0};
    probe.allocations = 0u;
    probe.deallocations = 0u;
    probe.fail_after = SIZE_MAX;
    CHECK(w_seed_check_storage_init_with_allocator(
        &storage, probe_allocate, probe_deallocate));
    CHECK(w_seed_check_storage_grow_json(&storage, 4u) ==
          W_SEED_CHECK_STORAGE_OK);
    storage.json_staging[0u] = 0x31u;
    storage.json_final[0u] = 0x41u;
    uint8_t *old_staging = storage.json_staging;
    uint8_t *old_final = storage.json_final;
    const size_t old_allocations = probe.allocations;
    probe.fail_after = old_allocations + failed;
    CHECK(w_seed_check_storage_grow_json(&storage, 5u) ==
          W_SEED_CHECK_STORAGE_ALLOCATION);
    CHECK(storage.json_staging == old_staging &&
          storage.json_final == old_final &&
          storage.json_staging_capacity == 4u &&
          storage.json_final_capacity == 4u &&
          storage.json_staging[0u] == 0x31u &&
          storage.json_final[0u] == 0x41u);
    CHECK(probe.allocations == old_allocations + failed);
    CHECK(probe.deallocations == failed);
    w_seed_check_storage_destroy(&storage);
    CHECK(probe.deallocations == failed + 2u);
  }
  return true;
}

static bool test_retry_bound_is_explicit(void) {
  const size_t capacity_bits = sizeof(size_t) * (size_t)CHAR_BIT;
  const size_t expected =
      (size_t)1u +
      ((size_t)2u * (size_t)W_SEED_CHECK_STORAGE_MAX_SOURCES +
       (size_t)1u) *
          W_SEED_CHECK_STORAGE_MAX_GROWTH_STEPS;
  CHECK(W_SEED_CHECK_STORAGE_MAX_RETRY_CLASSES ==
        (size_t)2u * (size_t)W_SEED_CHECK_STORAGE_MAX_SOURCES +
            (size_t)1u);
  CHECK(W_SEED_CHECK_STORAGE_MAX_GROWTH_STEPS == capacity_bits + 1u);
  CHECK(W_SEED_CHECK_STORAGE_MAX_RETRIES == expected);
  CHECK(W_SEED_CHECK_STORAGE_MAX_RETRIES >= 1u);
  return true;
}

int main(void) {
  if (!test_initial_request_is_empty() ||
      !test_growth_preserves_each_arena() ||
      !test_growth_keeps_requests_disjoint() || !test_capacity_is_atomic() ||
      !test_json_growth_preserves_pair() ||
      !test_node_growth_preserves_data_and_binds_slot() ||
      !test_node_aggregate_capacity_is_bounded() ||
      !test_aggregate_capacity_is_separate_and_atomic() ||
      !test_aggregate_limit_uses_exact_fallback() ||
      !test_bind_does_not_replace_non_byte_fields() ||
      !test_allocation_failure_is_atomic() ||
      !test_node_allocation_failure_is_atomic() ||
      !test_json_allocation_failure_is_atomic() ||
      !test_retry_bound_is_explicit()) {
    return 1;
  }
  return 0;
}
