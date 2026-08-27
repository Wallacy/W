#ifndef W_SEED_EPHEMERAL_PROVIDER_WINDOWS_H
#define W_SEED_EPHEMERAL_PROVIDER_WINDOWS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_ephemeral_provider.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W_SEED_EPHEMERAL_PROVIDER_WINDOWS_MAX_HANDLES \
  (W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES + 1u)

typedef enum {
  W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_EMPTY = 0,
  W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_ROOT_DIRECTORY,
  W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_ROOT_SOURCE,
  W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_CHILD_SOURCE,
} w_seed_ephemeral_provider_windows_slot_kind;

/* FILE_ID_INFO is a 64-bit volume serial plus a 128-bit file identifier.
 * The fields are kept as fixed-width values so this header remains free of
 * windows.h and remains usable by a caller-owned non-Windows stub. */
typedef struct {
  uintptr_t native_handle;
  uint64_t volume_serial;
  uint8_t file_id[16];
  uint64_t generation;
  w_seed_ephemeral_provider_windows_slot_kind kind;
  bool used;
} w_seed_ephemeral_provider_windows_slot;

/* All storage is caller-owned. One context accepts one acquisition at a time;
 * it is not thread-safe or reentrant. base_handle is borrowed, is never
 * closed by this adapter, and must remain a valid directory HANDLE for the
 * full context lifetime. Relative roots use this handle. The adapter closes
 * only native handles stored in its own slots. */
typedef struct {
  uintptr_t base_handle;
  bool initialized;
  bool ntcreatefile_supported;
  uint64_t next_generation;
  size_t root_leaf_length;
  uint16_t root_leaf[W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES + 1u];
  w_seed_ephemeral_provider_windows_slot
      slots[W_SEED_EPHEMERAL_PROVIDER_WINDOWS_MAX_HANDLES];
} w_seed_ephemeral_provider_windows_context;

/* Initialize a caller-owned context. On Windows, base_handle must be a valid
 * disk directory handle and the native NtCreateFile feature probe must pass.
 * Outside Windows this initializes a fail-closed UNSUPPORTED backend. */
bool w_seed_ephemeral_provider_windows_init(
    w_seed_ephemeral_provider_windows_context *context,
    uintptr_t base_handle);

/* Fill a valid synchronous backend vtable. No callback performs discovery,
 * path fallback, allocation, or publication. */
bool w_seed_ephemeral_provider_windows_backend(
    w_seed_ephemeral_provider_windows_context *context,
    w_seed_ephemeral_provider_backend *backend);

#ifdef __cplusplus
}
#endif

#endif
