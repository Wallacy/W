#include "w_seed_manifest_windows.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      (void)fprintf(stderr, "manifest adapter check failed: %s (%s:%d)\n", \
                    #condition, __FILE__, __LINE__);                           \
      return false;                                                             \
    }                                                                           \
  } while (0)

static bool zero_bytes(const uint8_t *bytes, size_t length) {
  if (bytes == NULL) return false;
  for (size_t index = 0u; index < length; index += 1u)
    if (bytes[index] != 0u) return false;
  return true;
}

static bool test_windows_fail_closed_stub(void) {
  w_seed_owner_guard_windows_context owner_context;
  (void)memset(&owner_context, 0, sizeof(owner_context));

  w_seed_owner_guard guard;
  (void)memset(&guard, 0, sizeof(guard));
  guard.owner = &guard;
  guard.backend.context = &owner_context;
  guard.backend_context_size = sizeof(owner_context);
  guard.generation = 7u;

  w_seed_manifest_backend backend;
  CHECK(w_seed_manifest_windows_backend(&guard, &owner_context, &backend));
  CHECK(backend.owner == &backend && backend.guard == &guard &&
        backend.context == &owner_context &&
        backend.context_size == sizeof(owner_context) &&
        backend.generation == guard.generation &&
        backend.read_candidate != NULL);

  uint8_t bytes[16];
  (void)memset(bytes, 0xA5, sizeof(bytes));
  const uint8_t snapshot[sizeof(bytes)] = {
      0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u,
      0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u};
  const w_seed_owner_guard_candidate_ref candidate = {7u, 3u, 1u};
  const w_seed_manifest_backend_result result = backend.read_candidate(
      backend.context, 7u, candidate, bytes, sizeof(bytes), 15u);
  CHECK(result.status == W_SEED_MANIFEST_BACKEND_UNSUPPORTED &&
        result.phase == W_SEED_MANIFEST_BACKEND_PHASE_OPEN_CANDIDATE &&
        result.generation == 7u &&
        memcmp(&result.candidate, &candidate, sizeof(candidate)) == 0 &&
        result.byte_count == 0u && result.required_byte_capacity == 0u &&
        zero_bytes(result.source_digest, sizeof(result.source_digest)) &&
        zero_bytes(result.context_binding, sizeof(result.context_binding)) &&
        zero_bytes(result.candidate_binding,
                   sizeof(result.candidate_binding)) &&
        memcmp(bytes, snapshot, sizeof(bytes)) == 0);
  return true;
}

int main(void) {
  if (!test_windows_fail_closed_stub()) return 1;
  (void)puts("w_seed_manifest_adapters: windows-stub ok");
  return 0;
}
