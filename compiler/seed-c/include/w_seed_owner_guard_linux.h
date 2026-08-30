#ifndef W_SEED_OWNER_GUARD_LINUX_H
#define W_SEED_OWNER_GUARD_LINUX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_owner_guard.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W_SEED_OWNER_GUARD_LINUX_MAX_HANDLES \
  (2u * W_SEED_OWNER_GUARD_MAX_LEVELS + 1u)

typedef enum {
  W_SEED_OWNER_GUARD_LINUX_SLOT_EMPTY = 0,
  W_SEED_OWNER_GUARD_LINUX_SLOT_SOURCE,
  W_SEED_OWNER_GUARD_LINUX_SLOT_DIRECTORY,
  W_SEED_OWNER_GUARD_LINUX_SLOT_CANDIDATE,
} w_seed_owner_guard_linux_slot_kind;

typedef struct {
  uint64_t mount_id;
  uint64_t device_major;
  uint64_t device_minor;
  uint64_t inode;
} w_seed_owner_guard_linux_identity;

typedef struct {
  int fd;
  w_seed_owner_guard_linux_identity identity;
  w_seed_owner_guard_linux_slot_kind kind;
  bool used;
} w_seed_owner_guard_linux_slot;

/* The base directory fd is borrowed and is never closed by the adapter. The
 * context is caller-owned, bounded, non-thread-safe, and accepts one live
 * session. Native handles and identities never leave this context. */
typedef struct {
  int base_dir_fd;
  bool initialized;
  bool native_supported;
  bool session_live;
  uint64_t next_generation;
  uint64_t active_generation;
  w_seed_owner_guard_linux_identity base_identity;
  size_t slot_count;
  size_t level_count;
  size_t candidate_count;
  size_t source_slot;
  size_t directory_slots[W_SEED_OWNER_GUARD_MAX_LEVELS];
  size_t candidate_slots[W_SEED_OWNER_GUARD_MAX_LEVELS];
  size_t source_path_length;
  char source_path[W_SEED_OWNER_GUARD_MAX_PATH_BYTES + 1u];
  w_seed_owner_guard_linux_slot
      slots[W_SEED_OWNER_GUARD_LINUX_MAX_HANDLES];
} w_seed_owner_guard_linux_context;

/* On Linux, base_dir_fd must be a live O_PATH or readable directory fd and
 * must not be AT_FDCWD. Outside Linux, initialization creates a fail-closed
 * UNSUPPORTED backend. */
bool w_seed_owner_guard_linux_init(
    w_seed_owner_guard_linux_context *context, int base_dir_fd);

bool w_seed_owner_guard_linux_backend(
    w_seed_owner_guard_linux_context *context,
    w_seed_owner_guard_backend *backend);

#ifdef __cplusplus
}
#endif

#endif
