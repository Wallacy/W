#ifndef W_SEED_CHECK_RETRY_H
#define W_SEED_CHECK_RETRY_H

#include <stddef.h>

#include "check_storage.h"
#include "w_seed_ephemeral_check.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The retry coordinator performs one bounded storage action.  The caller
 * owns the outer retry loop and decides how these outcomes map to the CLI
 * exit codes. */
typedef enum {
  W_SEED_CHECK_RETRY_RETRY = 0,
  W_SEED_CHECK_RETRY_TERMINAL_CAPACITY,
  W_SEED_CHECK_RETRY_FAULT,
} w_seed_check_retry_action;

typedef enum {
  W_SEED_CHECK_RETRY_DETAIL_NONE = 0,
  W_SEED_CHECK_RETRY_DETAIL_NON_RESIZABLE,
  W_SEED_CHECK_RETRY_DETAIL_INVALID_RESULT,
  W_SEED_CHECK_RETRY_DETAIL_NO_PROGRESS,
  W_SEED_CHECK_RETRY_DETAIL_CAPACITY,
  W_SEED_CHECK_RETRY_DETAIL_ALLOCATION,
  W_SEED_CHECK_RETRY_DETAIL_STORAGE,
  W_SEED_CHECK_RETRY_DETAIL_RETRY_LIMIT,
} w_seed_check_retry_detail;

typedef struct {
  w_seed_check_retry_action action;
  w_seed_check_retry_detail detail;
  const char *reason;
} w_seed_check_retry_outcome;

/* Apply exactly one action for one CHK7 CAPACITY result.  The function does
 * not run CHK6/CHK7 and does not allocate anything outside storage's
 * configured allocator.  `reason` points to a static, deterministic string
 * owned by this module. */
w_seed_check_retry_outcome w_seed_check_retry_apply(
    w_seed_check_storage *storage,
    const w_seed_ephemeral_check_result *check_result);

#ifdef __cplusplus
}
#endif

#endif
