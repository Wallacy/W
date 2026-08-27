#ifndef W_SEED_EPHEMERAL_PROVIDER_LINUX_H
#define W_SEED_EPHEMERAL_PROVIDER_LINUX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_ephemeral_provider.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W_SEED_EPHEMERAL_PROVIDER_LINUX_MAX_HANDLES \
  (W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES + 1u)

typedef enum {
  W_SEED_EPHEMERAL_PROVIDER_LINUX_SLOT_EMPTY = 0,
  W_SEED_EPHEMERAL_PROVIDER_LINUX_SLOT_ROOT_DIRECTORY,
  W_SEED_EPHEMERAL_PROVIDER_LINUX_SLOT_ROOT_SOURCE,
  W_SEED_EPHEMERAL_PROVIDER_LINUX_SLOT_CHILD_SOURCE,
} w_seed_ephemeral_provider_linux_slot_kind;

typedef struct {
  int fd;
  uint64_t device;
  uint64_t inode;
  uint64_t generation;
  w_seed_ephemeral_provider_linux_slot_kind kind;
  bool used;
} w_seed_ephemeral_provider_linux_slot;

/* All storage is caller-owned. One context accepts one acquisition at a time;
 * it is not thread-safe or reentrant. base_dir_fd is borrowed, is never
 * closed, and must remain a valid directory fd for the full context lifetime.
 * Relative roots use this fd; AT_FDCWD is rejected on Linux. The adapter
 * closes only fds stored in its own slots and requires the openat2 resolver. */
typedef struct {
  int base_dir_fd;
  bool initialized;
  bool openat2_supported;
  uint64_t next_generation;
  size_t root_leaf_length;
  char root_leaf[W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES + 1u];
  w_seed_ephemeral_provider_linux_slot
      slots[W_SEED_EPHEMERAL_PROVIDER_LINUX_MAX_HANDLES];
} w_seed_ephemeral_provider_linux_context;

/* Initialize a caller-owned context with no live adapter handles. On Linux,
 * base_dir_fd must be a valid directory fd and must not be AT_FDCWD; it must
 * remain valid until the context is no longer used. Outside Linux this
 * initializes the context for the fail-closed UNSUPPORTED backend. */
bool w_seed_ephemeral_provider_linux_init(
    w_seed_ephemeral_provider_linux_context *context, int base_dir_fd);

/* Fill a valid synchronous backend vtable. No callback performs discovery,
 * path fallback, allocation, or publication. */
bool w_seed_ephemeral_provider_linux_backend(
    w_seed_ephemeral_provider_linux_context *context,
    w_seed_ephemeral_provider_backend *backend);

#ifdef __cplusplus
}
#endif

#endif
