#include "check_retry.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                     \
  do {                                                                       \
    if (!(condition)) {                                                      \
      (void)fprintf(stderr, "check retry test failed: %s (%s:%d)\n",      \
                    #condition, __FILE__, __LINE__);                        \
      return false;                                                          \
    }                                                                        \
  } while (0)

static w_seed_ephemeral_check_result provider_capacity(
    w_seed_ephemeral_provider_capacity_field field, size_t request_index,
    size_t required_capacity) {
  w_seed_ephemeral_check_result result;
  (void)memset(&result, 0, sizeof(result));
  result.status = W_SEED_EPHEMERAL_CHECK_CAPACITY;
  result.failure = W_SEED_EPHEMERAL_CHECK_FAILURE_DRIVER;
  result.phase = W_SEED_EPHEMERAL_CHECK_PHASE_DRIVER;
  result.required_capacity = required_capacity;
  result.driver_status = W_SEED_EPHEMERAL_DRIVER_CAPACITY;
  result.driver_result.status = W_SEED_EPHEMERAL_DRIVER_CAPACITY;
  result.driver_result.failure = W_SEED_EPHEMERAL_DRIVER_FAILURE_PROVIDER;
  result.driver_result.capacity_field =
      W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PROVIDER;
  result.driver_result.required_capacity = required_capacity;
  result.driver_result.candidate_index = request_index;
  result.driver_result.provider_status = W_SEED_EPHEMERAL_PROVIDER_CAPACITY;
  result.driver_result.provider_result.status =
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY;
  result.driver_result.provider_result.failure =
      W_SEED_EPHEMERAL_PROVIDER_FAILURE_LIMIT;
  result.driver_result.provider_result.request_index = request_index;
  result.driver_result.provider_result.capacity_field = field;
  result.driver_result.provider_result.required_capacity = required_capacity;
  result.driver_result.provider_result.required_byte_capacity =
      required_capacity;
  return result;
}

static w_seed_ephemeral_check_result node_capacity(size_t candidate_index,
                                                   size_t required_capacity) {
  w_seed_ephemeral_check_result result;
  (void)memset(&result, 0, sizeof(result));
  result.status = W_SEED_EPHEMERAL_CHECK_CAPACITY;
  result.failure = W_SEED_EPHEMERAL_CHECK_FAILURE_DRIVER;
  result.phase = W_SEED_EPHEMERAL_CHECK_PHASE_DRIVER;
  result.required_capacity = required_capacity;
  result.driver_status = W_SEED_EPHEMERAL_DRIVER_CAPACITY;
  result.driver_result.status = W_SEED_EPHEMERAL_DRIVER_CAPACITY;
  result.driver_result.failure = W_SEED_EPHEMERAL_DRIVER_FAILURE_PARSE;
  result.driver_result.capacity_field =
      W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_NODE;
  result.driver_result.required_capacity = required_capacity;
  result.driver_result.candidate_index = candidate_index;
  /* Acquisition already succeeded before parsing reports node capacity. */
  result.driver_result.provider_status = W_SEED_EPHEMERAL_PROVIDER_OK;
  result.driver_result.provider_result.status = W_SEED_EPHEMERAL_PROVIDER_OK;
  result.driver_result.provider_result.failure =
      W_SEED_EPHEMERAL_PROVIDER_FAILURE_NONE;
  result.driver_result.provider_result.request_index = SIZE_MAX;
  return result;
}

static w_seed_ephemeral_check_result json_capacity(size_t required_capacity) {
  w_seed_ephemeral_check_result result;
  (void)memset(&result, 0, sizeof(result));
  result.status = W_SEED_EPHEMERAL_CHECK_CAPACITY;
  result.failure = W_SEED_EPHEMERAL_CHECK_FAILURE_OUTPUT;
  result.phase = W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_MEASURE;
  result.required_capacity = required_capacity;
  result.driver_status = W_SEED_EPHEMERAL_DRIVER_OK;
  result.driver_result.status = W_SEED_EPHEMERAL_DRIVER_OK;
  result.driver_result.provider_status = W_SEED_EPHEMERAL_PROVIDER_OK;
  result.driver_result.provider_result.status = W_SEED_EPHEMERAL_PROVIDER_OK;
  result.driver_result.provider_result.failure =
      W_SEED_EPHEMERAL_PROVIDER_FAILURE_NONE;
  return result;
}

static w_seed_ephemeral_check_result frontend_capacity(
    size_t required_capacity) {
  w_seed_ephemeral_check_result result = json_capacity(required_capacity);
  result.failure = W_SEED_EPHEMERAL_CHECK_FAILURE_FRONTEND;
  result.phase = W_SEED_EPHEMERAL_CHECK_PHASE_FRONTEND_MEASURE;
  result.frontend_status = W_SEED_FRONTEND_CAPACITY;
  result.frontend_result.status = W_SEED_FRONTEND_CAPACITY;
  result.frontend_result.required.receipt_bytes = required_capacity;
  return result;
}

static bool test_provider_bytes_retry(void) {
  w_seed_check_storage storage;
  CHECK(w_seed_check_storage_init(&storage));
  const w_seed_ephemeral_check_result result = provider_capacity(
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES, 3u, 7u);
  const w_seed_check_retry_outcome outcome =
      w_seed_check_retry_apply(&storage, &result);
  CHECK(outcome.action == W_SEED_CHECK_RETRY_RETRY);
  CHECK(outcome.detail == W_SEED_CHECK_RETRY_DETAIL_NONE);
  CHECK(outcome.reason != NULL);
  CHECK(storage.staging_capacity[3u] == 8u &&
        storage.revalidation_capacity[3u] == 8u &&
        storage.published_capacity[3u] == 8u);
  w_seed_check_storage_destroy(&storage);
  return true;
}

static bool test_node_retry(void) {
  w_seed_check_storage storage;
  CHECK(w_seed_check_storage_init(&storage));
  const w_seed_ephemeral_check_result result = node_capacity(4u, 7u);
  const w_seed_check_retry_outcome outcome =
      w_seed_check_retry_apply(&storage, &result);
  CHECK(outcome.action == W_SEED_CHECK_RETRY_RETRY);
  CHECK(outcome.detail == W_SEED_CHECK_RETRY_DETAIL_NONE);
  CHECK(storage.node_capacity[4u] == 8u);
  w_seed_check_storage_destroy(&storage);
  return true;
}

static bool test_json_pair_retry(void) {
  w_seed_check_storage storage;
  CHECK(w_seed_check_storage_init(&storage));
  const w_seed_ephemeral_check_result result = json_capacity(7u);
  const w_seed_check_retry_outcome outcome =
      w_seed_check_retry_apply(&storage, &result);
  CHECK(outcome.action == W_SEED_CHECK_RETRY_RETRY);
  CHECK(outcome.detail == W_SEED_CHECK_RETRY_DETAIL_NONE);
  CHECK(storage.json_staging_capacity == 8u &&
        storage.json_final_capacity == 8u);
  CHECK(storage.json_staging != NULL && storage.json_final != NULL &&
        storage.json_staging != storage.json_final);
  w_seed_check_storage_destroy(&storage);
  return true;
}

static bool test_frontend_capacity_is_terminal(void) {
  w_seed_check_storage storage;
  CHECK(w_seed_check_storage_init(&storage));
  w_seed_ephemeral_check_result result = frontend_capacity(7u);
  const w_seed_check_retry_outcome outcome =
      w_seed_check_retry_apply(&storage, &result);
  CHECK(outcome.action == W_SEED_CHECK_RETRY_TERMINAL_CAPACITY);
  CHECK(outcome.detail == W_SEED_CHECK_RETRY_DETAIL_NON_RESIZABLE);
  result.frontend_result.status = W_SEED_FRONTEND_OK;
  CHECK(w_seed_check_retry_apply(&storage, &result).action ==
        W_SEED_CHECK_RETRY_FAULT);
  result = frontend_capacity(7u);
  result.phase = W_SEED_EPHEMERAL_CHECK_PHASE_DRIVER;
  CHECK(w_seed_check_retry_apply(&storage, &result).action ==
        W_SEED_CHECK_RETRY_FAULT);
  w_seed_check_storage_destroy(&storage);
  return true;
}

static bool test_non_resizable_capacity_is_terminal(void) {
  w_seed_check_storage storage;
  CHECK(w_seed_check_storage_init(&storage));
  w_seed_ephemeral_check_result result = provider_capacity(
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_AGGREGATE_SOURCE_BYTES, 0u,
      7u);
  result.driver_result.provider_result.required_byte_capacity = 0u;
  w_seed_check_retry_outcome outcome =
      w_seed_check_retry_apply(&storage, &result);
  CHECK(outcome.action == W_SEED_CHECK_RETRY_TERMINAL_CAPACITY);
  CHECK(outcome.detail == W_SEED_CHECK_RETRY_DETAIL_NON_RESIZABLE);

  result = provider_capacity(
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_PROVIDER_ID, 0u, 7u);
  result.driver_result.provider_result.required_byte_capacity = 0u;
  outcome = w_seed_check_retry_apply(&storage, &result);
  CHECK(outcome.action == W_SEED_CHECK_RETRY_TERMINAL_CAPACITY);
  w_seed_check_storage_destroy(&storage);
  return true;
}

static bool test_invalid_indices_and_fields_fault(void) {
  w_seed_check_storage storage;
  CHECK(w_seed_check_storage_init(&storage));
  w_seed_ephemeral_check_result result = provider_capacity(
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES,
      (size_t)W_SEED_CHECK_STORAGE_MAX_SOURCES, 7u);
  w_seed_check_retry_outcome outcome =
      w_seed_check_retry_apply(&storage, &result);
  CHECK(outcome.action == W_SEED_CHECK_RETRY_FAULT);
  CHECK(outcome.detail == W_SEED_CHECK_RETRY_DETAIL_INVALID_RESULT);

  result = provider_capacity(
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_NONE, 0u, 7u);
  result.driver_result.provider_result.required_byte_capacity = 0u;
  outcome = w_seed_check_retry_apply(&storage, &result);
  CHECK(outcome.action == W_SEED_CHECK_RETRY_FAULT);

  result = provider_capacity(
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES, 0u, 7u);
  result.driver_result.capacity_field =
      W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_NODE;
  outcome = w_seed_check_retry_apply(&storage, &result);
  CHECK(outcome.action == W_SEED_CHECK_RETRY_FAULT);

  result = provider_capacity(
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES, 0u, 7u);
  result.driver_result.provider_result.status =
      W_SEED_EPHEMERAL_PROVIDER_OK;
  outcome = w_seed_check_retry_apply(&storage, &result);
  CHECK(outcome.action == W_SEED_CHECK_RETRY_FAULT);

  result = node_capacity(0u, 7u);
  result.driver_status = W_SEED_EPHEMERAL_DRIVER_OK;
  outcome = w_seed_check_retry_apply(&storage, &result);
  CHECK(outcome.action == W_SEED_CHECK_RETRY_FAULT);

  result = node_capacity(0u, 7u);
  result.driver_result.provider_status = W_SEED_EPHEMERAL_PROVIDER_INVALID;
  outcome = w_seed_check_retry_apply(&storage, &result);
  CHECK(outcome.action == W_SEED_CHECK_RETRY_FAULT);

  result = json_capacity(7u);
  result.driver_result.status = W_SEED_EPHEMERAL_DRIVER_CAPACITY;
  outcome = w_seed_check_retry_apply(&storage, &result);
  CHECK(outcome.action == W_SEED_CHECK_RETRY_FAULT);
  result = json_capacity(7u);
  result.driver_result.failure = W_SEED_EPHEMERAL_DRIVER_FAILURE_PARSE;
  outcome = w_seed_check_retry_apply(&storage, &result);
  CHECK(outcome.action == W_SEED_CHECK_RETRY_FAULT);
  w_seed_check_storage_destroy(&storage);
  return true;
}

static bool test_no_progress_and_bounds(void) {
  w_seed_check_storage storage;
  CHECK(w_seed_check_storage_init(&storage));
  CHECK(w_seed_check_storage_grow(&storage, 0u, 8u) ==
        W_SEED_CHECK_STORAGE_OK);
  w_seed_ephemeral_check_result result = provider_capacity(
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_OUTPUT_BYTES, 0u, 4u);
  w_seed_check_retry_outcome outcome =
      w_seed_check_retry_apply(&storage, &result);
  CHECK(outcome.action == W_SEED_CHECK_RETRY_FAULT);
  CHECK(outcome.detail == W_SEED_CHECK_RETRY_DETAIL_NO_PROGRESS);

  result = provider_capacity(
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_OUTPUT_BYTES, 1u,
      (size_t)W_SEED_CHECK_STORAGE_MAX_SOURCE_BYTES + 1u);
  outcome = w_seed_check_retry_apply(&storage, &result);
  CHECK(outcome.action == W_SEED_CHECK_RETRY_TERMINAL_CAPACITY);

  result = json_capacity((size_t)W_SEED_CHECK_STORAGE_MAX_JSON_BYTES + 1u);
  outcome = w_seed_check_retry_apply(&storage, &result);
  CHECK(outcome.action == W_SEED_CHECK_RETRY_TERMINAL_CAPACITY);
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

static bool test_json_allocation_failure_is_atomic(void) {
  w_seed_check_storage storage;
  probe.allocations = 0u;
  probe.deallocations = 0u;
  probe.fail_after = SIZE_MAX;
  CHECK(w_seed_check_storage_init_with_allocator(
      &storage, probe_allocate, probe_deallocate));
  w_seed_ephemeral_check_result result = json_capacity(4u);
  CHECK(w_seed_check_retry_apply(&storage, &result).action ==
        W_SEED_CHECK_RETRY_RETRY);
  uint8_t *old_staging = storage.json_staging;
  uint8_t *old_final = storage.json_final;
  const size_t old_allocations = probe.allocations;
  probe.fail_after = old_allocations + 1u;
  result = json_capacity(5u);
  const w_seed_check_retry_outcome outcome =
      w_seed_check_retry_apply(&storage, &result);
  CHECK(outcome.action == W_SEED_CHECK_RETRY_FAULT);
  CHECK(outcome.detail == W_SEED_CHECK_RETRY_DETAIL_ALLOCATION);
  CHECK(storage.json_staging == old_staging && storage.json_final == old_final);
  CHECK(storage.json_staging_capacity == 4u &&
        storage.json_final_capacity == 4u);
  CHECK(probe.allocations == old_allocations + 1u);
  CHECK(probe.deallocations == 1u);
  w_seed_check_storage_destroy(&storage);
  CHECK(probe.deallocations == 3u);
  return true;
}

static bool test_allocation_failure_detail_is_distinct(void) {
  w_seed_check_storage storage;
  probe.allocations = 0u;
  probe.deallocations = 0u;
  probe.fail_after = 0u;
  CHECK(w_seed_check_storage_init_with_allocator(
      &storage, probe_allocate, probe_deallocate));
  const w_seed_ephemeral_check_result result = provider_capacity(
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES, 0u, 4u);
  const w_seed_check_retry_outcome outcome =
      w_seed_check_retry_apply(&storage, &result);
  CHECK(outcome.action == W_SEED_CHECK_RETRY_FAULT);
  CHECK(outcome.detail == W_SEED_CHECK_RETRY_DETAIL_ALLOCATION);
  CHECK(storage.staging_capacity[0u] == 0u &&
        storage.revalidation_capacity[0u] == 0u &&
        storage.published_capacity[0u] == 0u);
  w_seed_check_storage_destroy(&storage);
  return true;
}

int main(void) {
  if (!test_provider_bytes_retry() || !test_node_retry() ||
      !test_json_pair_retry() || !test_frontend_capacity_is_terminal() ||
      !test_non_resizable_capacity_is_terminal() ||
      !test_invalid_indices_and_fields_fault() ||
      !test_no_progress_and_bounds() ||
      !test_json_allocation_failure_is_atomic() ||
      !test_allocation_failure_detail_is_distinct())
    return 1;
  return 0;
}
