#ifndef W_SEED_CHECK_PIPELINE_H
#define W_SEED_CHECK_PIPELINE_H

#include <stddef.h>

#include "check_retry.h"
#include "w_seed_ephemeral_check.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A caller-owned CHK6 -> CHK7 composition.  This layer has no filesystem,
 * operating-system, stdout, or stderr policy. */
typedef struct {
  const w_seed_ephemeral_driver_input *driver_input;
  w_seed_ephemeral_driver_scratch *driver_scratch;
  w_seed_ephemeral_driver_output *driver_output;
  w_seed_frontend_output *frontend_output;
  w_seed_check_storage *storage;
  const char *instance;
  size_t instance_length;
} w_seed_check_pipeline_input;

typedef enum {
  W_SEED_CHECK_PIPELINE_CLEAN = 0,
  W_SEED_CHECK_PIPELINE_DIAGNOSTICS,
  W_SEED_CHECK_PIPELINE_CAPACITY,
  W_SEED_CHECK_PIPELINE_UNSUPPORTED,
  W_SEED_CHECK_PIPELINE_INVALID,
  W_SEED_CHECK_PIPELINE_IO,
  W_SEED_CHECK_PIPELINE_FAULT,
} w_seed_check_pipeline_status;

typedef struct {
  w_seed_check_pipeline_status status;
  size_t attempts;
  w_seed_ephemeral_check_result check_result;
  w_seed_check_retry_outcome retry;
  /* SIZE_MAX means that no JSON is published.  Zero is a valid clean result;
   * a positive value is valid only with DIAGNOSTICS. */
  size_t json_length;
} w_seed_check_pipeline_result;

/* Bind every caller-provided slot/request before each CHK7 attempt.  A
 * capacity result performs one and only one retry action; RETRY restarts the
 * complete CHK7 composition with the newly bound storage.  The caller keeps
 * ownership of all storage and scratch, including the final JSON bytes. */
w_seed_check_pipeline_status w_seed_check_pipeline_run(
    const w_seed_check_pipeline_input *input,
    w_seed_check_pipeline_result *result);

#ifdef __cplusplus
}
#endif

#endif
