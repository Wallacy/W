#include "w_seed_manifest_windows.h"

#include <string.h>

static w_seed_manifest_backend_result windows_unsupported(
    const void *context, uint64_t generation,
    w_seed_owner_guard_candidate_ref candidate, uint8_t *bytes,
    size_t byte_capacity, size_t byte_limit) {
  (void)context;
  (void)bytes;
  (void)byte_capacity;
  (void)byte_limit;
  w_seed_manifest_backend_result result;
  (void)memset(&result, 0, sizeof(result));
  result.status = W_SEED_MANIFEST_BACKEND_UNSUPPORTED;
  result.phase = W_SEED_MANIFEST_BACKEND_PHASE_OPEN_CANDIDATE;
  result.generation = generation;
  result.candidate = candidate;
  return result;
}

bool w_seed_manifest_windows_backend(
    const w_seed_owner_guard *guard,
    const w_seed_owner_guard_windows_context *context,
    w_seed_manifest_backend *backend) {
  if (guard == NULL || context == NULL || backend == NULL) return false;
  *backend = (w_seed_manifest_backend){
      .owner = backend,
      .guard = guard,
      .context = context,
      .context_size = sizeof(*context),
      .generation = guard->generation,
      .read_candidate = windows_unsupported,
  };
  return true;
}
