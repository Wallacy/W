#ifndef W_SEED_OWNER_GUARD_WINDOWS_H
#define W_SEED_OWNER_GUARD_WINDOWS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_owner_guard.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W_SEED_OWNER_GUARD_WINDOWS_MAX_HANDLES \
  (2u * W_SEED_OWNER_GUARD_MAX_LEVELS + 1u)

typedef enum {
  W_SEED_OWNER_GUARD_WINDOWS_SLOT_EMPTY = 0,
  W_SEED_OWNER_GUARD_WINDOWS_SLOT_SOURCE,
  W_SEED_OWNER_GUARD_WINDOWS_SLOT_DIRECTORY,
  W_SEED_OWNER_GUARD_WINDOWS_SLOT_CANDIDATE,
} w_seed_owner_guard_windows_slot_kind;

typedef struct {
  uint64_t volume_serial;
  uint8_t file_id[16];
} w_seed_owner_guard_windows_identity;

typedef struct {
  uintptr_t native_handle;
  w_seed_owner_guard_windows_identity identity;
  w_seed_owner_guard_windows_slot_kind kind;
  bool used;
} w_seed_owner_guard_windows_slot;

/* The base HANDLE is borrowed and is never closed by the adapter. The context
 * is caller-owned, bounded, non-thread-safe, and accepts one live session.
 * Native handles and identities never leave this context. */
typedef struct {
  uintptr_t base_handle;
  bool initialized;
  bool native_supported;
  bool base_local;
  uint32_t locality_status;
  uint32_t parent_probe_status;
  bool session_live;
  uint64_t next_generation;
  uint64_t active_generation;
  w_seed_owner_guard_windows_identity base_identity;
  size_t slot_count;
  size_t level_count;
  size_t candidate_count;
  size_t source_slot;
  size_t directory_slots[W_SEED_OWNER_GUARD_MAX_LEVELS];
  size_t candidate_slots[W_SEED_OWNER_GUARD_MAX_LEVELS];
  size_t source_path_length;
  uint16_t source_path[W_SEED_OWNER_GUARD_MAX_PATH_BYTES + 1u];
  w_seed_owner_guard_windows_slot
      slots[W_SEED_OWNER_GUARD_WINDOWS_MAX_HANDLES];
} w_seed_owner_guard_windows_context;

/* On Windows, base_handle must be a live disk directory handle. Initialization
 * probes a native NtCreateFile parent `..` lookup. Outside Windows, it creates
 * a fail-closed UNSUPPORTED backend. */
bool w_seed_owner_guard_windows_init(
    w_seed_owner_guard_windows_context *context, uintptr_t base_handle);

bool w_seed_owner_guard_windows_backend(
    w_seed_owner_guard_windows_context *context,
    w_seed_owner_guard_backend *backend);

#ifdef __cplusplus
}
#endif

#endif
