#include "w_seed_run0.h"

#include <stdint.h>
#include <string.h>

static bool ranges_overlap(const void *left, size_t left_bytes,
                           const void *right, size_t right_bytes) {
  if (left == NULL || right == NULL || left_bytes == 0u || right_bytes == 0u)
    return false;
  const uintptr_t left_start = (uintptr_t)left;
  const uintptr_t right_start = (uintptr_t)right;
  if (left_bytes > UINTPTR_MAX - left_start ||
      right_bytes > UINTPTR_MAX - right_start)
    return true;
  const uintptr_t left_end = left_start + (uintptr_t)left_bytes;
  const uintptr_t right_end = right_start + (uintptr_t)right_bytes;
  return left_start < right_end && right_start < left_end;
}

w_seed_run0_status w_seed_run0_execute(const w_seed_hlo0_plan *plan,
                                       w_seed_run0_sink sink, void *context,
                                       w_seed_run0_result *result) {
  if (plan == NULL || sink == NULL || result == NULL)
    return W_SEED_RUN0_INVALID_PLAN;
  if (ranges_overlap(plan, sizeof(*plan), result, sizeof(*result)))
    return W_SEED_RUN0_ALIAS;
  if (!w_seed_hlo0_verify_plan(plan)) return W_SEED_RUN0_INVALID_PLAN;

  uint8_t output[W_SEED_RUN0_MAX_OUTPUT_BYTES];
  if (plan->payload_bytes >= sizeof(output)) return W_SEED_RUN0_INVALID_PLAN;
  (void)memcpy(output, plan->payload, plan->payload_bytes);
  output[plan->payload_bytes] = 0x0au;

  const size_t attempted_bytes = plan->stdout_bytes;
  const w_seed_run0_sink_result sink_result =
      sink(context, output, attempted_bytes);
  const bool sink_success =
      sink_result.accepted_bytes == attempted_bytes &&
      sink_result.flush_status == W_SEED_RUN0_FLUSH_SUCCEEDED;
  w_seed_run0_result candidate = {
      sink_success ? W_SEED_RUN0_OK : W_SEED_RUN0_IO,
      attempted_bytes,
      sink_result.accepted_bytes,
      sink_result.flush_status,
      1u,
  };
  *result = candidate;
  return candidate.status;
}
