#ifndef W_SEED_RUN0_H
#define W_SEED_RUN0_H

#include <stddef.h>
#include <stdint.h>

#include "w_seed_hlo0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* RUN0 is an internal seed execution adapter for one verified HLO0 plan. It
 * is not a public or general W runner. It has no heap allocation. All staging
 * storage remains on the call stack. */
#define W_SEED_RUN0_MAX_OUTPUT_BYTES (W_SEED_HLO0_MAX_PAYLOAD + 1u)

typedef enum {
  W_SEED_RUN0_OK = 0,
  W_SEED_RUN0_INVALID_PLAN,
  W_SEED_RUN0_ALIAS,
  W_SEED_RUN0_IO,
} w_seed_run0_status;

typedef enum {
  W_SEED_RUN0_FLUSH_NOT_ATTEMPTED = 0,
  W_SEED_RUN0_FLUSH_SUCCEEDED,
  W_SEED_RUN0_FLUSH_FAILED,
} w_seed_run0_flush_status;

/* A sink reports the byte count that it accepted and the flush result. The
 * callback may not retain the byte pointer. Accepted bytes can have an
 * external effect. RUN0 cannot roll back that effect. */
typedef struct {
  size_t accepted_bytes;
  w_seed_run0_flush_status flush_status;
} w_seed_run0_sink_result;

typedef w_seed_run0_sink_result (*w_seed_run0_sink)(
    void *context, const uint8_t *bytes, size_t byte_count);

/* A preflight failure leaves the result untouched. After the sink call, RUN0
 * publishes the sink report. Success requires all attempted bytes and a
 * successful flush. Any other report has I/O status. */
typedef struct {
  w_seed_run0_status status;
  size_t attempted_bytes;
  size_t accepted_bytes;
  w_seed_run0_flush_status flush_status;
  size_t sink_calls;
} w_seed_run0_result;

/* Validate the plan, stage payload plus LF, and call sink exactly once. The
 * result must be distinct from plan. No callback occurs on a preflight fault.
 */
w_seed_run0_status w_seed_run0_execute(const w_seed_hlo0_plan *plan,
                                       w_seed_run0_sink sink, void *context,
                                       w_seed_run0_result *result);

#ifdef __cplusplus
}
#endif

#endif
