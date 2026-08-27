#include "check_retry.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static w_seed_check_retry_outcome retry_outcome(
    w_seed_check_retry_action action, w_seed_check_retry_detail detail,
    const char *reason) {
  return (w_seed_check_retry_outcome){action, detail, reason};
}

static w_seed_check_retry_outcome retry_fault(
    w_seed_check_retry_detail detail, const char *reason) {
  return retry_outcome(W_SEED_CHECK_RETRY_FAULT, detail, reason);
}

static w_seed_check_retry_outcome retry_terminal(const char *reason) {
  return retry_outcome(W_SEED_CHECK_RETRY_TERMINAL_CAPACITY,
                       W_SEED_CHECK_RETRY_DETAIL_NON_RESIZABLE, reason);
}

static bool provider_byte_field(
    w_seed_ephemeral_provider_capacity_field field) {
  return field == W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES ||
         field ==
             W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_BYTES ||
         field == W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_OUTPUT_BYTES;
}

static bool source_index_valid(size_t index) {
  return index < (size_t)W_SEED_CHECK_STORAGE_MAX_SOURCES;
}

static bool driver_non_resizable_field(
    w_seed_ephemeral_driver_capacity_field field) {
  switch (field) {
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_SLOT:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_REQUEST:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_SOURCE_ID:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_MODULE_ID:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_TOKEN:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_FRAME:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_ISSUE:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_LEXER_FRAME:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_ORIGIN:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_CANDIDATE_DOCUMENT:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_CANDIDATE_FACT:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_GRAPH_INVENTORY:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_GRAPH_EDGE:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_GRAPH_ORDER:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_GRAPH_RESOLVED:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_DOCUMENT:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_ROUNDS:
      return true;
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_NONE:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_NODE:
    case W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PROVIDER:
    default:
      return false;
  }
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

static w_seed_check_retry_outcome apply_provider_capacity(
    w_seed_check_storage *storage,
    const w_seed_ephemeral_check_result *check_result) {
  const w_seed_ephemeral_driver_result *driver =
      &check_result->driver_result;
  const w_seed_ephemeral_provider_result *provider =
      &driver->provider_result;
  if (check_result->driver_status != W_SEED_EPHEMERAL_DRIVER_CAPACITY ||
      driver->status != W_SEED_EPHEMERAL_DRIVER_CAPACITY ||
      driver->failure != W_SEED_EPHEMERAL_DRIVER_FAILURE_PROVIDER ||
      driver->provider_status != W_SEED_EPHEMERAL_PROVIDER_CAPACITY ||
      provider->status != W_SEED_EPHEMERAL_PROVIDER_CAPACITY ||
      provider->failure != W_SEED_EPHEMERAL_PROVIDER_FAILURE_LIMIT ||
      driver->capacity_field !=
          W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PROVIDER ||
      driver->required_capacity != check_result->required_capacity ||
      check_result->phase != W_SEED_EPHEMERAL_CHECK_PHASE_DRIVER ||
      provider->required_capacity != driver->required_capacity ||
      !source_index_valid(provider->request_index) ||
      driver->candidate_index != provider->request_index ||
      provider->required_capacity == 0u ||
      provider->capacity_field ==
          W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_NONE ||
      (provider_byte_field(provider->capacity_field) &&
       provider->required_byte_capacity != provider->required_capacity))
    return retry_fault(W_SEED_CHECK_RETRY_DETAIL_INVALID_RESULT,
                       "retry result fields are incoherent");
  if (!provider_byte_field(provider->capacity_field)) {
    if (provider_non_resizable_field(provider->capacity_field))
      return retry_terminal("capacity is not resizable");
    return retry_fault(W_SEED_CHECK_RETRY_DETAIL_INVALID_RESULT,
                       "retry result fields are incoherent");
  }

  const size_t index = provider->request_index;
  const size_t old_capacity = storage->staging_capacity[index] >
                                      storage->revalidation_capacity[index]
                                  ? storage->staging_capacity[index]
                                  : storage->revalidation_capacity[index];
  const size_t old_published = storage->published_capacity[index];
  const size_t old_max = old_capacity > old_published ? old_capacity
                                                       : old_published;
  const w_seed_check_storage_status status = w_seed_check_storage_grow(
      storage, index, provider->required_capacity);
  if (status == W_SEED_CHECK_STORAGE_ALLOCATION)
    return retry_fault(W_SEED_CHECK_RETRY_DETAIL_ALLOCATION,
                       "retry storage allocation failed");
  if (status == W_SEED_CHECK_STORAGE_CAPACITY)
    return retry_terminal("retry storage capacity exceeded");
  if (status != W_SEED_CHECK_STORAGE_OK)
    return retry_fault(W_SEED_CHECK_RETRY_DETAIL_STORAGE,
                       "retry storage invariant failed");

  const size_t new_capacity =
      storage->staging_capacity[index] > storage->revalidation_capacity[index]
          ? storage->staging_capacity[index]
          : storage->revalidation_capacity[index];
  const size_t new_max = new_capacity > storage->published_capacity[index]
                             ? new_capacity
                             : storage->published_capacity[index];
  if (new_max <= old_max)
    return retry_fault(W_SEED_CHECK_RETRY_DETAIL_NO_PROGRESS,
                       "retry storage grow made no progress");
  return retry_outcome(W_SEED_CHECK_RETRY_RETRY,
                       W_SEED_CHECK_RETRY_DETAIL_NONE, "provider bytes grown");
}

static w_seed_check_retry_outcome apply_node_capacity(
    w_seed_check_storage *storage,
    const w_seed_ephemeral_check_result *check_result) {
  const w_seed_ephemeral_driver_result *driver =
      &check_result->driver_result;
  if (check_result->driver_status != W_SEED_EPHEMERAL_DRIVER_CAPACITY ||
      driver->status != W_SEED_EPHEMERAL_DRIVER_CAPACITY ||
      driver->failure != W_SEED_EPHEMERAL_DRIVER_FAILURE_PARSE ||
      driver->provider_status != W_SEED_EPHEMERAL_PROVIDER_OK ||
      driver->provider_result.status != W_SEED_EPHEMERAL_PROVIDER_OK ||
      driver->provider_result.failure !=
          W_SEED_EPHEMERAL_PROVIDER_FAILURE_NONE ||
      driver->capacity_field !=
          W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_NODE ||
      check_result->phase != W_SEED_EPHEMERAL_CHECK_PHASE_DRIVER ||
      !source_index_valid(driver->candidate_index) ||
      driver->required_capacity == 0u ||
      driver->required_capacity != check_result->required_capacity)
    return retry_fault(W_SEED_CHECK_RETRY_DETAIL_INVALID_RESULT,
                       "retry result fields are incoherent");

  const size_t index = driver->candidate_index;
  const size_t old_capacity = storage->node_capacity[index];
  const w_seed_check_storage_status status = w_seed_check_storage_grow_nodes(
      storage, index, driver->required_capacity);
  if (status == W_SEED_CHECK_STORAGE_ALLOCATION)
    return retry_fault(W_SEED_CHECK_RETRY_DETAIL_ALLOCATION,
                       "retry storage allocation failed");
  if (status == W_SEED_CHECK_STORAGE_CAPACITY)
    return retry_terminal("retry storage capacity exceeded");
  if (status != W_SEED_CHECK_STORAGE_OK)
    return retry_fault(W_SEED_CHECK_RETRY_DETAIL_STORAGE,
                       "retry storage invariant failed");
  if (storage->node_capacity[index] <= old_capacity)
    return retry_fault(W_SEED_CHECK_RETRY_DETAIL_NO_PROGRESS,
                       "retry storage grow made no progress");
  return retry_outcome(W_SEED_CHECK_RETRY_RETRY,
                       W_SEED_CHECK_RETRY_DETAIL_NONE, "parser nodes grown");
}

static w_seed_check_retry_outcome apply_json_capacity(
    w_seed_check_storage *storage,
    const w_seed_ephemeral_check_result *check_result) {
  const w_seed_ephemeral_driver_result *driver =
      &check_result->driver_result;
  if (check_result->driver_status != W_SEED_EPHEMERAL_DRIVER_OK ||
      driver->status != W_SEED_EPHEMERAL_DRIVER_OK ||
      driver->failure != W_SEED_EPHEMERAL_DRIVER_FAILURE_NONE ||
      driver->capacity_field !=
          W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_NONE ||
      driver->provider_status != W_SEED_EPHEMERAL_PROVIDER_OK ||
      driver->provider_result.status != W_SEED_EPHEMERAL_PROVIDER_OK ||
      driver->provider_result.failure !=
          W_SEED_EPHEMERAL_PROVIDER_FAILURE_NONE ||
      check_result->failure != W_SEED_EPHEMERAL_CHECK_FAILURE_OUTPUT ||
      check_result->phase != W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_MEASURE ||
      check_result->required_capacity == 0u)
    return retry_fault(W_SEED_CHECK_RETRY_DETAIL_INVALID_RESULT,
                       "retry result fields are incoherent");
  const size_t old_capacity = storage->json_staging_capacity;
  const w_seed_check_storage_status status = w_seed_check_storage_grow_json(
      storage, check_result->required_capacity);
  if (status == W_SEED_CHECK_STORAGE_ALLOCATION)
    return retry_fault(W_SEED_CHECK_RETRY_DETAIL_ALLOCATION,
                       "retry storage allocation failed");
  if (status == W_SEED_CHECK_STORAGE_CAPACITY)
    return retry_terminal("retry JSON capacity exceeded");
  if (status != W_SEED_CHECK_STORAGE_OK)
    return retry_fault(W_SEED_CHECK_RETRY_DETAIL_STORAGE,
                       "retry storage invariant failed");
  if (storage->json_staging_capacity <= old_capacity)
    return retry_fault(W_SEED_CHECK_RETRY_DETAIL_NO_PROGRESS,
                       "retry JSON grow made no progress");
  return retry_outcome(W_SEED_CHECK_RETRY_RETRY,
                       W_SEED_CHECK_RETRY_DETAIL_NONE, "JSON buffers grown");
}

w_seed_check_retry_outcome w_seed_check_retry_apply(
    w_seed_check_storage *storage,
    const w_seed_ephemeral_check_result *check_result) {
  if (storage == NULL || check_result == NULL)
    return retry_fault(W_SEED_CHECK_RETRY_DETAIL_INVALID_RESULT,
                       "retry input is invalid");
  if (check_result->status != W_SEED_EPHEMERAL_CHECK_CAPACITY)
    return retry_fault(W_SEED_CHECK_RETRY_DETAIL_INVALID_RESULT,
                       "retry result is not a capacity result");
  if (check_result->failure == W_SEED_EPHEMERAL_CHECK_FAILURE_DRIVER) {
    if (check_result->driver_result.provider_status ==
            W_SEED_EPHEMERAL_PROVIDER_CAPACITY ||
        check_result->driver_result.capacity_field ==
            W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PROVIDER) {
      return apply_provider_capacity(storage, check_result);
    }
    if (check_result->driver_result.capacity_field ==
        W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_NODE) {
      return apply_node_capacity(storage, check_result);
    }
    if (check_result->driver_result.capacity_field ==
            W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_NONE ||
        !driver_non_resizable_field(
            check_result->driver_result.capacity_field) ||
        check_result->driver_status != W_SEED_EPHEMERAL_DRIVER_CAPACITY ||
        check_result->driver_result.status !=
            W_SEED_EPHEMERAL_DRIVER_CAPACITY ||
        check_result->driver_result.required_capacity !=
            check_result->required_capacity ||
        check_result->driver_result.failure ==
            W_SEED_EPHEMERAL_DRIVER_FAILURE_NONE)
      return retry_fault(W_SEED_CHECK_RETRY_DETAIL_INVALID_RESULT,
                         "retry result fields are incoherent");
    return retry_terminal("driver capacity is not resizable");
  }
  if (check_result->failure == W_SEED_EPHEMERAL_CHECK_FAILURE_OUTPUT)
    return apply_json_capacity(storage, check_result);
  if (check_result->failure == W_SEED_EPHEMERAL_CHECK_FAILURE_FRONTEND ||
      check_result->failure == W_SEED_EPHEMERAL_CHECK_FAILURE_DIAGNOSTIC) {
    if (check_result->driver_status != W_SEED_EPHEMERAL_DRIVER_OK ||
        check_result->driver_result.status != W_SEED_EPHEMERAL_DRIVER_OK ||
        check_result->driver_result.failure !=
            W_SEED_EPHEMERAL_DRIVER_FAILURE_NONE)
      return retry_fault(W_SEED_CHECK_RETRY_DETAIL_INVALID_RESULT,
                         "retry result fields are incoherent");
    if (check_result->failure == W_SEED_EPHEMERAL_CHECK_FAILURE_FRONTEND) {
      if (check_result->frontend_status != W_SEED_FRONTEND_CAPACITY ||
          check_result->frontend_result.status != W_SEED_FRONTEND_CAPACITY ||
          (check_result->phase !=
               W_SEED_EPHEMERAL_CHECK_PHASE_FRONTEND_MEASURE &&
           check_result->phase !=
               W_SEED_EPHEMERAL_CHECK_PHASE_FRONTEND_RUN) ||
          check_result->required_capacity !=
              check_result->frontend_result.required.receipt_bytes)
        return retry_fault(W_SEED_CHECK_RETRY_DETAIL_INVALID_RESULT,
                           "retry result fields are incoherent");
      return retry_terminal("checker capacity is not resizable");
    }
    if (check_result->diagnostic_status != W_SEED_DIAGNOSTIC_CAPACITY ||
        check_result->diagnostic_result.status !=
            W_SEED_DIAGNOSTIC_CAPACITY ||
        (check_result->phase !=
             W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_MEASURE &&
         check_result->phase !=
             W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_WRITE))
      return retry_fault(W_SEED_CHECK_RETRY_DETAIL_INVALID_RESULT,
                         "retry result fields are incoherent");
    return retry_terminal("checker capacity is not resizable");
  }
  return retry_fault(W_SEED_CHECK_RETRY_DETAIL_INVALID_RESULT,
                     "retry result failure is invalid");
}
