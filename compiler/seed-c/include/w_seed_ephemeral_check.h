#ifndef W_SEED_EPHEMERAL_CHECK_H
#define W_SEED_EPHEMERAL_CHECK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_diagnostic.h"
#include "w_seed_ephemeral_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/* This is an internal composition boundary for the seed checker. It is not a
 * public `w check` command and it does not perform filesystem or CLI work. */
typedef enum {
  W_SEED_EPHEMERAL_CHECK_OK = 0,
  W_SEED_EPHEMERAL_CHECK_DIAGNOSTICS,
  W_SEED_EPHEMERAL_CHECK_CAPACITY,
  W_SEED_EPHEMERAL_CHECK_INVALID,
  W_SEED_EPHEMERAL_CHECK_UNSUPPORTED,
  W_SEED_EPHEMERAL_CHECK_IO,
} w_seed_ephemeral_check_status;

typedef enum {
  W_SEED_EPHEMERAL_CHECK_FAILURE_NONE = 0,
  W_SEED_EPHEMERAL_CHECK_FAILURE_INSTANCE,
  W_SEED_EPHEMERAL_CHECK_FAILURE_DRIVER,
  W_SEED_EPHEMERAL_CHECK_FAILURE_FRONTEND,
  W_SEED_EPHEMERAL_CHECK_FAILURE_DIAGNOSTIC,
  W_SEED_EPHEMERAL_CHECK_FAILURE_OUTPUT,
} w_seed_ephemeral_check_failure;

typedef enum {
  W_SEED_EPHEMERAL_CHECK_PHASE_NONE = 0,
  W_SEED_EPHEMERAL_CHECK_PHASE_VALIDATE,
  W_SEED_EPHEMERAL_CHECK_PHASE_DRIVER,
  W_SEED_EPHEMERAL_CHECK_PHASE_FRONTEND_MEASURE,
  W_SEED_EPHEMERAL_CHECK_PHASE_FRONTEND_RUN,
  W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_MEASURE,
  W_SEED_EPHEMERAL_CHECK_PHASE_DIAGNOSTIC_WRITE,
  W_SEED_EPHEMERAL_CHECK_PHASE_COMMIT,
} w_seed_ephemeral_check_phase;

typedef struct {
  const w_seed_ephemeral_driver_input *driver_input;
  w_seed_ephemeral_driver_scratch *driver_scratch;
  /* CHK6 graph/documents are scratch for this composition. They may be
   * published by the driver before the frontend succeeds. */
  w_seed_ephemeral_driver_output *driver_staging_output;
  /* Frontend records are caller-owned staging and may change on failure. */
  w_seed_frontend_output *frontend_staging_output;
  /* The first D0 instance. It must be exactly `D` followed by six digits.
   * Each later diagnostic increments the six-digit ordinal. */
  const char *instance;
  size_t instance_length;
  /* D0 JSONL scratch. It is never exposed as the final output. */
  uint8_t *json_staging;
  size_t json_staging_capacity;
} w_seed_ephemeral_check_input;

typedef struct {
  /* Only this buffer and jsonl_length are externally published by a
   * successful call. They remain bitwise unchanged on every failure. */
  uint8_t *jsonl;
  size_t jsonl_capacity;
  size_t jsonl_length;
} w_seed_ephemeral_check_output;

typedef struct {
  w_seed_ephemeral_check_status status;
  w_seed_ephemeral_check_failure failure;
  w_seed_ephemeral_check_phase phase;
  /* Required capacity is phase-relative. For D0 it is the complete JSONL
   * byte count; nested results carry the exact driver/frontend requirements. */
  size_t required_capacity;
  /* SIZE_MAX means that no frontend diagnostic has been selected. */
  size_t diagnostic_index;
  w_seed_ephemeral_driver_status driver_status;
  w_seed_ephemeral_driver_result driver_result;
  w_seed_frontend_status frontend_status;
  w_seed_frontend_result frontend_result;
  w_seed_diagnostic_status diagnostic_status;
  w_seed_diagnostic_result diagnostic_result;
} w_seed_ephemeral_check_result;

/* Run CHK6 discovery, frontend normalization, and the supported D0 JSON-only
 * adapter. The graph, documents, and frontend output are caller-owned scratch;
 * final JSONL is copied once, after every diagnostic has passed preflight. */
w_seed_ephemeral_check_status w_seed_ephemeral_check_run(
    const w_seed_ephemeral_check_input *input,
    w_seed_ephemeral_check_output *output,
    w_seed_ephemeral_check_result *result);

#ifdef __cplusplus
}
#endif

#endif
