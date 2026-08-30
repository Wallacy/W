#include "check_pipeline.h"

#include <stdint.h>
#include <string.h>

static w_seed_check_retry_outcome retry_not_run(void) {
  return (w_seed_check_retry_outcome){
      W_SEED_CHECK_RETRY_FAULT, W_SEED_CHECK_RETRY_DETAIL_NONE,
      "retry not run"};
}

static w_seed_check_pipeline_status map_terminal_check_status(
    w_seed_ephemeral_check_status status) {
  switch (status) {
    case W_SEED_EPHEMERAL_CHECK_UNSUPPORTED:
      return W_SEED_CHECK_PIPELINE_UNSUPPORTED;
    case W_SEED_EPHEMERAL_CHECK_INVALID:
      return W_SEED_CHECK_PIPELINE_INVALID;
    case W_SEED_EPHEMERAL_CHECK_IO:
      return W_SEED_CHECK_PIPELINE_IO;
    case W_SEED_EPHEMERAL_CHECK_OK:
    case W_SEED_EPHEMERAL_CHECK_DIAGNOSTICS:
    case W_SEED_EPHEMERAL_CHECK_CAPACITY:
    default:
      return W_SEED_CHECK_PIPELINE_FAULT;
  }
}

static void result_clear(w_seed_check_pipeline_result *result) {
  (void)memset(result, 0, sizeof(*result));
  result->status = W_SEED_CHECK_PIPELINE_INVALID;
  result->retry = retry_not_run();
  result->json_length = SIZE_MAX;
}

static w_seed_check_pipeline_status finish_success(
    w_seed_check_pipeline_result *result,
    const w_seed_ephemeral_check_output *output,
    const w_seed_check_storage *storage,
    w_seed_ephemeral_check_status status) {
  if (result == NULL || output == NULL || storage == NULL ||
      output->jsonl != storage->json_final ||
      output->jsonl_length > storage->json_final_capacity ||
      (status == W_SEED_EPHEMERAL_CHECK_OK && output->jsonl_length != 0u) ||
      (status == W_SEED_EPHEMERAL_CHECK_DIAGNOSTICS &&
       output->jsonl_length == 0u)) {
    if (result != NULL) {
      result->status = W_SEED_CHECK_PIPELINE_FAULT;
      result->json_length = SIZE_MAX;
    }
    return W_SEED_CHECK_PIPELINE_FAULT;
  }
  result->json_length = output->jsonl_length;
  result->status = status == W_SEED_EPHEMERAL_CHECK_OK
                       ? W_SEED_CHECK_PIPELINE_CLEAN
                       : W_SEED_CHECK_PIPELINE_DIAGNOSTICS;
  return result->status;
}

w_seed_check_pipeline_status w_seed_check_pipeline_run(
    const w_seed_check_pipeline_input *input,
    w_seed_check_pipeline_result *result) {
  if (result == NULL) return W_SEED_CHECK_PIPELINE_INVALID;
  result_clear(result);
  if (input == NULL || input->driver_input == NULL ||
      input->driver_scratch == NULL || input->driver_output == NULL ||
      input->frontend_output == NULL || input->storage == NULL ||
      !input->storage->acquisition.initialized || input->instance == NULL ||
      input->instance_length == 0u)
    return W_SEED_CHECK_PIPELINE_INVALID;

  for (size_t attempt = 0u;
       attempt < W_SEED_CHECK_STORAGE_MAX_RETRIES; attempt += 1u) {
    if (!w_seed_acquisition_storage_bind_driver(
            &input->storage->acquisition, input->driver_scratch)) {
      result->status = W_SEED_CHECK_PIPELINE_FAULT;
      return result->status;
    }

    result->attempts += 1u;
    const w_seed_ephemeral_check_input check_input = {
        input->driver_input,
        input->driver_scratch,
        input->driver_output,
        input->frontend_output,
        input->instance,
        input->instance_length,
        input->storage->json_staging,
        input->storage->json_staging_capacity};
    w_seed_ephemeral_check_output check_output = {
        input->storage->json_final, input->storage->json_final_capacity,
        SIZE_MAX};
    (void)memset(&result->check_result, 0, sizeof(result->check_result));
    const w_seed_ephemeral_check_status check_status =
        w_seed_ephemeral_check_run(&check_input, &check_output,
                                   &result->check_result);
    if (check_status == W_SEED_EPHEMERAL_CHECK_OK ||
        check_status == W_SEED_EPHEMERAL_CHECK_DIAGNOSTICS)
      return finish_success(result, &check_output, input->storage,
                            check_status);
    if (check_output.jsonl_length != SIZE_MAX) {
      result->status = W_SEED_CHECK_PIPELINE_FAULT;
      return result->status;
    }
    if (check_status != W_SEED_EPHEMERAL_CHECK_CAPACITY) {
      result->status = map_terminal_check_status(check_status);
      return result->status;
    }

    result->retry = w_seed_check_retry_apply(input->storage,
                                             &result->check_result);
    if (result->retry.action == W_SEED_CHECK_RETRY_TERMINAL_CAPACITY) {
      result->status = W_SEED_CHECK_PIPELINE_CAPACITY;
      return result->status;
    }
    if (result->retry.action != W_SEED_CHECK_RETRY_RETRY) {
      result->status = W_SEED_CHECK_PIPELINE_FAULT;
      return result->status;
    }
    if (attempt + 1u >= W_SEED_CHECK_STORAGE_MAX_RETRIES) {
      result->retry = (w_seed_check_retry_outcome){
          W_SEED_CHECK_RETRY_FAULT,
          W_SEED_CHECK_RETRY_DETAIL_RETRY_LIMIT,
          "retry attempt limit exhausted"};
      result->status = W_SEED_CHECK_PIPELINE_FAULT;
      return result->status;
    }
  }
  result->status = W_SEED_CHECK_PIPELINE_FAULT;
  result->retry = (w_seed_check_retry_outcome){
      W_SEED_CHECK_RETRY_FAULT, W_SEED_CHECK_RETRY_DETAIL_RETRY_LIMIT,
      "retry attempt limit exhausted"};
  return result->status;
}
