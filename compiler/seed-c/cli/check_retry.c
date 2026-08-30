#include "check_retry.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static w_seed_check_retry_outcome retry_outcome(
    w_seed_check_retry_action action, w_seed_check_retry_detail detail,
    const char *reason) {
  return (w_seed_check_retry_outcome){action, detail, reason};
}

static w_seed_check_retry_outcome retry_fault(
    w_seed_check_retry_detail detail, const char *reason) {
  return retry_outcome(W_SEED_CHECK_RETRY_FAULT, detail, reason);
}

static w_seed_check_retry_outcome retry_terminal(
    w_seed_check_retry_detail detail, const char *reason) {
  return retry_outcome(W_SEED_CHECK_RETRY_TERMINAL_CAPACITY, detail, reason);
}

static w_seed_check_retry_detail map_acquisition_detail(
    w_seed_acquisition_retry_detail detail) {
  switch (detail) {
    case W_SEED_ACQUISITION_RETRY_DETAIL_NONE:
      return W_SEED_CHECK_RETRY_DETAIL_NONE;
    case W_SEED_ACQUISITION_RETRY_DETAIL_NON_RESIZABLE:
      return W_SEED_CHECK_RETRY_DETAIL_NON_RESIZABLE;
    case W_SEED_ACQUISITION_RETRY_DETAIL_INVALID_RESULT:
      return W_SEED_CHECK_RETRY_DETAIL_INVALID_RESULT;
    case W_SEED_ACQUISITION_RETRY_DETAIL_NO_PROGRESS:
      return W_SEED_CHECK_RETRY_DETAIL_NO_PROGRESS;
    case W_SEED_ACQUISITION_RETRY_DETAIL_CAPACITY:
      return W_SEED_CHECK_RETRY_DETAIL_CAPACITY;
    case W_SEED_ACQUISITION_RETRY_DETAIL_ALLOCATION:
      return W_SEED_CHECK_RETRY_DETAIL_ALLOCATION;
    case W_SEED_ACQUISITION_RETRY_DETAIL_STORAGE:
      return W_SEED_CHECK_RETRY_DETAIL_STORAGE;
    case W_SEED_ACQUISITION_RETRY_DETAIL_RETRY_LIMIT:
      return W_SEED_CHECK_RETRY_DETAIL_RETRY_LIMIT;
    default:
      return W_SEED_CHECK_RETRY_DETAIL_INVALID_RESULT;
  }
}

static bool span_empty(w_seed_span span) {
  return span.start_byte == 0u && span.end_byte == 0u;
}

static bool graph_counts_equal(w_seed_ephemeral_graph_counts left,
                               w_seed_ephemeral_graph_counts right) {
  return left.sources == right.sources && left.edges == right.edges &&
         left.total_source_bytes == right.total_source_bytes;
}

static bool provider_success(
    const w_seed_ephemeral_driver_result *driver) {
  if (driver == NULL) return false;
  const w_seed_ephemeral_provider_result *provider =
      &driver->provider_result;
  return driver->provider_status == W_SEED_EPHEMERAL_PROVIDER_OK &&
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

static bool scan_success(const w_seed_ephemeral_driver_result *driver) {
  if (driver == NULL) return false;
  const w_seed_module_scan_result *scan = &driver->scan_result;
  return driver->scan_status == W_SEED_MODULE_SCAN_OK &&
         scan->status == W_SEED_MODULE_SCAN_OK &&
         scan->required == scan->written &&
         scan->module_header_name_span.start_byte <=
             scan->module_header_name_span.end_byte &&
         scan->module_header_name_span.end_byte <=
             (size_t)W_SEED_ACQUISITION_MAX_SOURCE_BYTES &&
         (scan->has_module_header_name
              ? scan->module_header_name_span.start_byte <
                    scan->module_header_name_span.end_byte
              : span_empty(scan->module_header_name_span));
}

static bool driver_success(const w_seed_ephemeral_check_result *check) {
  if (check == NULL) return false;
  const w_seed_ephemeral_driver_result *driver = &check->driver_result;
  const w_seed_ephemeral_graph_result *graph = &driver->graph_result;
  return check->driver_status == W_SEED_EPHEMERAL_DRIVER_OK &&
         driver->status == W_SEED_EPHEMERAL_DRIVER_OK &&
         driver->failure == W_SEED_EPHEMERAL_DRIVER_FAILURE_NONE &&
         driver->phase == W_SEED_EPHEMERAL_DRIVER_PHASE_COMMIT &&
         driver->round < (size_t)W_SEED_EPHEMERAL_DRIVER_MAX_ROUNDS &&
         driver->candidate_index == SIZE_MAX &&
         driver->origin_index == SIZE_MAX &&
         driver->document_index == SIZE_MAX && span_empty(driver->span) &&
         driver->capacity_field ==
             W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_NONE &&
         driver->required_capacity == 0u && provider_success(driver) &&
         driver->parser_status == W_SEED_PARSE_COMPLETE &&
         driver->parser_issue_kind == W_SEED_PARSE_ISSUE_NONE &&
         scan_success(driver) &&
         driver->graph_status == W_SEED_EPHEMERAL_GRAPH_OK &&
         graph->status == W_SEED_EPHEMERAL_GRAPH_OK &&
         graph->failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_NONE &&
         graph_counts_equal(graph->required, graph->written) &&
         graph->candidate_index == SIZE_MAX &&
         graph->document_ordinal == SIZE_MAX &&
         graph->edge_ordinal == SIZE_MAX && span_empty(graph->span);
}

static bool frontend_inactive(const w_seed_ephemeral_check_result *check) {
  if (check == NULL) return false;
  const w_seed_frontend_result *frontend = &check->frontend_result;
  const w_seed_frontend_counts zero = {0};
  return check->frontend_status == W_SEED_FRONTEND_INVALID &&
         frontend->status == W_SEED_FRONTEND_OK &&
         memcmp(&frontend->required, &zero, sizeof(zero)) == 0 &&
         memcmp(&frontend->written, &zero, sizeof(zero)) == 0 &&
         frontend->schema_version.data == NULL &&
         frontend->schema_version.length == 0u &&
         frontend->barrier_document == 0u && span_empty(frontend->barrier_span) &&
         frontend->primary_diagnostic == 0u && frontend->receipt_bytes == 0u;
}

static bool diagnostic_inactive(
    const w_seed_ephemeral_check_result *check) {
  return check != NULL && check->diagnostic_index == SIZE_MAX &&
         check->diagnostic_status == W_SEED_DIAGNOSTIC_NO_RECORD &&
         check->diagnostic_result.status == W_SEED_DIAGNOSTIC_OK &&
         check->diagnostic_result.required_bytes == 0u &&
         check->diagnostic_result.written_bytes == 0u &&
         check->diagnostic_result.primary_byte == 0u;
}

static bool frontend_diagnostics_success(
    const w_seed_ephemeral_check_result *check) {
  if (check == NULL) return false;
  const w_seed_frontend_result *frontend = &check->frontend_result;
  return check->frontend_status == W_SEED_FRONTEND_DIAGNOSTICS &&
         frontend->status == W_SEED_FRONTEND_DIAGNOSTICS &&
         frontend->schema_version.data != NULL &&
         frontend->schema_version.length != 0u &&
         frontend->barrier_document == SIZE_MAX &&
         frontend->primary_diagnostic < frontend->written.diagnostics &&
         memcmp(&frontend->required, &frontend->written,
                sizeof(frontend->required)) == 0 &&
         frontend->written.diagnostics != 0u &&
         frontend->receipt_bytes == frontend->written.receipt_bytes;
}

static bool frontend_capacity_envelope(
    const w_seed_ephemeral_check_result *check) {
  if (check == NULL) return false;
  const w_seed_frontend_result *frontend = &check->frontend_result;
  const w_seed_frontend_counts zero = {0};
  return check->frontend_status == W_SEED_FRONTEND_CAPACITY &&
         frontend->status == W_SEED_FRONTEND_CAPACITY &&
         frontend->schema_version.data != NULL &&
         frontend->schema_version.length != 0u &&
         frontend->barrier_document == SIZE_MAX &&
         frontend->primary_diagnostic == SIZE_MAX &&
         memcmp(&frontend->written, &zero, sizeof(zero)) == 0 &&
         frontend->required.receipt_bytes == check->required_capacity &&
         check->required_capacity != 0u;
}

static w_seed_check_retry_outcome apply_driver_capacity(
    w_seed_check_storage *storage,
    const w_seed_ephemeral_check_result *check_result) {
  if (check_result->status != W_SEED_EPHEMERAL_CHECK_CAPACITY ||
      check_result->failure != W_SEED_EPHEMERAL_CHECK_FAILURE_DRIVER ||
      check_result->phase != W_SEED_EPHEMERAL_CHECK_PHASE_DRIVER ||
      check_result->driver_status != W_SEED_EPHEMERAL_DRIVER_CAPACITY ||
      check_result->driver_result.status !=
          W_SEED_EPHEMERAL_DRIVER_CAPACITY ||
      check_result->required_capacity !=
          check_result->driver_result.required_capacity ||
      !frontend_inactive(check_result) ||
      !diagnostic_inactive(check_result))
    return retry_fault(W_SEED_CHECK_RETRY_DETAIL_INVALID_RESULT,
                       "retry result fields are incoherent");
  const w_seed_acquisition_retry_outcome acquisition =
      w_seed_acquisition_retry_apply(&storage->acquisition,
                                     check_result->driver_status,
                                     &check_result->driver_result);
  const w_seed_check_retry_detail detail =
      map_acquisition_detail(acquisition.detail);
  if (acquisition.status == W_SEED_ACQUISITION_RETRY_OK &&
      acquisition.action == W_SEED_ACQUISITION_RETRY_RETRY)
    return retry_outcome(W_SEED_CHECK_RETRY_RETRY, detail,
                         acquisition.reason);
  if (acquisition.status == W_SEED_ACQUISITION_RETRY_CAPACITY &&
      acquisition.action == W_SEED_ACQUISITION_RETRY_TERMINAL)
    return retry_terminal(detail, acquisition.reason);
  return retry_fault(detail, acquisition.reason);
}

static w_seed_check_retry_outcome apply_json_capacity(
    w_seed_check_storage *storage,
    const w_seed_ephemeral_check_result *check_result) {
  if (!driver_success(check_result) ||
      !frontend_diagnostics_success(check_result) ||
      check_result->failure != W_SEED_EPHEMERAL_CHECK_FAILURE_OUTPUT ||
      check_result->phase != W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_MEASURE ||
      check_result->required_capacity == 0u ||
      check_result->diagnostic_index !=
          check_result->frontend_result.written.diagnostics - 1u ||
      check_result->diagnostic_status != W_SEED_DIAGNOSTIC_CAPACITY ||
      check_result->diagnostic_result.status != W_SEED_DIAGNOSTIC_CAPACITY ||
      check_result->diagnostic_result.required_bytes == 0u ||
      check_result->diagnostic_result.written_bytes != 0u ||
      check_result->diagnostic_result.required_bytes >=
          check_result->required_capacity)
    return retry_fault(W_SEED_CHECK_RETRY_DETAIL_INVALID_RESULT,
                       "retry result fields are incoherent");
  const size_t old_capacity = storage->json_staging_capacity;
  const w_seed_check_storage_status status = w_seed_check_storage_grow_json(
      storage, check_result->required_capacity);
  if (status == W_SEED_CHECK_STORAGE_ALLOCATION)
    return retry_fault(W_SEED_CHECK_RETRY_DETAIL_ALLOCATION,
                       "retry storage allocation failed");
  if (status == W_SEED_CHECK_STORAGE_CAPACITY)
    return retry_terminal(W_SEED_CHECK_RETRY_DETAIL_CAPACITY,
                          "retry JSON capacity exceeded");
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
  if (check_result->failure == W_SEED_EPHEMERAL_CHECK_FAILURE_DRIVER)
    return apply_driver_capacity(storage, check_result);
  if (check_result->failure == W_SEED_EPHEMERAL_CHECK_FAILURE_OUTPUT)
    return apply_json_capacity(storage, check_result);
  if (check_result->failure == W_SEED_EPHEMERAL_CHECK_FAILURE_FRONTEND ||
      check_result->failure == W_SEED_EPHEMERAL_CHECK_FAILURE_DIAGNOSTIC) {
    if (!driver_success(check_result))
      return retry_fault(W_SEED_CHECK_RETRY_DETAIL_INVALID_RESULT,
                         "retry result fields are incoherent");
    if (check_result->failure == W_SEED_EPHEMERAL_CHECK_FAILURE_FRONTEND) {
      if (!frontend_capacity_envelope(check_result) ||
          check_result->phase !=
              W_SEED_EPHEMERAL_CHECK_PHASE_FRONTEND_RUN ||
          !diagnostic_inactive(check_result))
        return retry_fault(W_SEED_CHECK_RETRY_DETAIL_INVALID_RESULT,
                           "retry result fields are incoherent");
      return retry_terminal(W_SEED_CHECK_RETRY_DETAIL_NON_RESIZABLE,
                            "checker capacity is not resizable");
    }
    return retry_fault(W_SEED_CHECK_RETRY_DETAIL_INVALID_RESULT,
                       "diagnostic capacity is not a retry envelope");
  }
  return retry_fault(W_SEED_CHECK_RETRY_DETAIL_INVALID_RESULT,
                     "retry result failure is invalid");
}
