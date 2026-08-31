#if defined(__linux__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#endif

#include "w_seed_ephemeral_provider_linux.h"

#include <limits.h>
#include <string.h>

static const char linux_provider_id[] =
    W_SEED_EPHEMERAL_PROVIDER_LINUX_V2_ID;

enum {
  LINUX_PROVIDER_ID_LENGTH = sizeof(linux_provider_id) - 1u,
  /* prefix + mount id + device major + device minor + inode. */
  LINUX_IDENTITY_TOKEN_LENGTH =
      W_SEED_EPHEMERAL_PROVIDER_LINUX_V2_TOKEN_BYTES,
};

static w_seed_ephemeral_provider_metadata linux_metadata(void) {
  const w_seed_ephemeral_provider_token_capacity provider = {
      LINUX_PROVIDER_ID_LENGTH, LINUX_PROVIDER_ID_LENGTH};
  const w_seed_ephemeral_provider_token_capacity identity = {
      LINUX_IDENTITY_TOKEN_LENGTH, LINUX_IDENTITY_TOKEN_LENGTH};
  return (w_seed_ephemeral_provider_metadata){
      provider, identity, identity, identity};
}

#if defined(__linux__)

#include <errno.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <linux/stat.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#if defined(SYS_openat2) && defined(RESOLVE_BENEATH) && \
    defined(RESOLVE_NO_SYMLINKS) && defined(RESOLVE_NO_XDEV) && \
    defined(SYS_statx) && defined(STATX_MNT_ID_UNIQUE) && \
    defined(AT_EMPTY_PATH)
#define W_SEED_EPHEMERAL_PROVIDER_HAS_OPENAT2 1
#else
#define W_SEED_EPHEMERAL_PROVIDER_HAS_OPENAT2 0
#endif

typedef struct {
  uint64_t mount_id;
  uint64_t device_major;
  uint64_t device_minor;
  uint64_t inode;
} linux_identity;

static w_seed_ephemeral_provider_linux_slot *slot_for_handle(
    w_seed_ephemeral_provider_linux_context *context,
    w_seed_ephemeral_provider_handle handle,
    w_seed_ephemeral_provider_linux_slot_kind expected_kind) {
  if (context == NULL || !context->initialized || handle.value == (uintptr_t)0u)
    return NULL;
  const uintptr_t span =
      (uintptr_t)W_SEED_EPHEMERAL_PROVIDER_LINUX_MAX_HANDLES;
  const uintptr_t normalized = handle.value - (uintptr_t)1u;
  const size_t index = (size_t)(normalized % span);
  const uint64_t generation = (uint64_t)(normalized / span);
  if (index >= W_SEED_EPHEMERAL_PROVIDER_LINUX_MAX_HANDLES)
    return NULL;
  w_seed_ephemeral_provider_linux_slot *slot = &context->slots[index];
  if (!slot->used || slot->generation != generation ||
      slot->kind != expected_kind)
    return NULL;
  return slot;
}

static bool encode_handle(size_t index, uint64_t generation,
                          w_seed_ephemeral_provider_handle *handle) {
  if (handle == NULL || index >= W_SEED_EPHEMERAL_PROVIDER_LINUX_MAX_HANDLES ||
      generation == 0u)
    return false;
  const uintptr_t span =
      (uintptr_t)W_SEED_EPHEMERAL_PROVIDER_LINUX_MAX_HANDLES;
  const uintptr_t slot_number = (uintptr_t)(index + 1u);
  if (generation > (uint64_t)UINTPTR_MAX) return false;
  const uintptr_t generation_value = (uintptr_t)generation;
  if (generation_value > (UINTPTR_MAX - slot_number) / span) return false;
  handle->value = generation_value * span + slot_number;
  return handle->value != (uintptr_t)0u;
}

static bool allocate_slot(w_seed_ephemeral_provider_linux_context *context,
                          int fd, const linux_identity *identity,
                          w_seed_ephemeral_provider_linux_slot_kind kind,
                          w_seed_ephemeral_provider_handle *handle) {
  if (context == NULL || identity == NULL || handle == NULL || fd < 0 ||
      !context->initialized)
    return false;
  uint64_t generation = context->next_generation;
  if (generation == 0u) generation = 1u;
  for (size_t index = 0u;
       index < W_SEED_EPHEMERAL_PROVIDER_LINUX_MAX_HANDLES; index += 1u) {
    w_seed_ephemeral_provider_linux_slot *slot = &context->slots[index];
    if (slot->used) continue;
    if (!encode_handle(index, generation, handle)) return false;
    slot->fd = fd;
    slot->mount_id = identity->mount_id;
    slot->device_major = identity->device_major;
    slot->device_minor = identity->device_minor;
    slot->inode = identity->inode;
    slot->generation = generation;
    slot->kind = kind;
    slot->used = true;
    context->next_generation = generation == UINT64_MAX ? 1u : generation + 1u;
    return true;
  }
  return false;
}

static void release_slot(w_seed_ephemeral_provider_linux_slot *slot) {
  if (slot == NULL || !slot->used) return;
  if (slot->fd >= 0) (void)close(slot->fd);
  slot->fd = -1;
  slot->mount_id = 0u;
  slot->device_major = 0u;
  slot->device_minor = 0u;
  slot->inode = 0u;
  slot->generation = 0u;
  slot->kind = W_SEED_EPHEMERAL_PROVIDER_LINUX_SLOT_EMPTY;
  slot->used = false;
}

static w_seed_ephemeral_provider_backend_status errno_status(int error_code) {
  switch (error_code) {
    case ENOENT:
    case ENOTDIR:
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_NOT_FOUND;
    case ELOOP:
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_SYMLINK;
    case EXDEV:
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_ESCAPE;
    case ENOSYS:
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
    default:
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO;
  }
}

static bool component_valid(const uint8_t *data, size_t start, size_t end) {
  if (data == NULL || start >= end) return false;
  if ((end - start == 1u && data[start] == (uint8_t)'.') ||
      (end - start == 2u && data[start] == (uint8_t)'.' &&
       data[start + 1u] == (uint8_t)'.'))
    return false;
  for (size_t index = start; index < end; index += 1u) {
    if (data[index] == 0u || data[index] == (uint8_t)'\\') return false;
  }
  return true;
}

static bool root_path_parts(w_seed_byte_view root_path, size_t *parent_length,
                            size_t *leaf_start, bool *absolute) {
  if (parent_length == NULL || leaf_start == NULL || absolute == NULL ||
      root_path.data == NULL || root_path.length == 0u ||
      root_path.length > W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES ||
      root_path.data[root_path.length - 1u] == (uint8_t)'/')
    return false;
  *absolute = root_path.data[0] == (uint8_t)'/';
  size_t last_slash = SIZE_MAX;
  for (size_t index = 0u; index < root_path.length; index += 1u) {
    if (root_path.data[index] == 0u || root_path.data[index] == (uint8_t)'\\')
      return false;
    if (root_path.data[index] == (uint8_t)'/') last_slash = index;
  }
  if (*absolute && root_path.length > 1u && root_path.data[1u] == (uint8_t)'/')
    return false;
  *parent_length = last_slash == SIZE_MAX ? 0u : last_slash;
  *leaf_start = last_slash == SIZE_MAX ? 0u : last_slash + 1u;
  if (!component_valid(root_path.data, *leaf_start, root_path.length))
    return false;
  if (*parent_length == 0u) return true;
  const size_t first_component = *absolute ? 1u : 0u;
  size_t component_start = first_component;
  for (size_t index = first_component; index <= *parent_length; index += 1u) {
    if (index != *parent_length && root_path.data[index] != (uint8_t)'/')
      continue;
    if (index > *parent_length ||
        !component_valid(root_path.data, component_start, index))
      return false;
    component_start = index + 1u;
  }
  return true;
}

static bool copy_component(const uint8_t *data, size_t start, size_t end,
                           char destination[W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES +
                                             1u]) {
  if (!component_valid(data, start, end) || destination == NULL ||
      end - start > W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES)
    return false;
  const size_t length = end - start;
  (void)memcpy(destination, data + start, length);
  destination[length] = '\0';
  return true;
}

static bool copy_source_id(w_seed_frontend_text source_id,
                           char destination[W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES +
                                             1u]) {
  if (destination == NULL || source_id.data == NULL || source_id.length == 0u ||
      source_id.length > W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES)
    return false;
  const uint8_t *data = (const uint8_t *)source_id.data;
  size_t component_start = 0u;
  for (size_t index = 0u; index <= source_id.length; index += 1u) {
    if (index != source_id.length && data[index] != (uint8_t)'/') continue;
    if (!component_valid(data, component_start, index)) return false;
    component_start = index + 1u;
  }
  (void)memcpy(destination, data, source_id.length);
  destination[source_id.length] = '\0';
  return true;
}

static bool statx_identity(int fd, linux_identity *identity,
                           uint64_t *size, mode_t *mode) {
  if (fd < 0 || identity == NULL || size == NULL || mode == NULL)
    return false;
#if W_SEED_EPHEMERAL_PROVIDER_HAS_OPENAT2
  struct statx stat_buffer;
  (void)memset(&stat_buffer, 0, sizeof(stat_buffer));
  const unsigned int mask =
      (unsigned int)(STATX_TYPE | STATX_SIZE | STATX_INO |
                     STATX_MNT_ID_UNIQUE | STATX_BASIC_STATS);
  const long result = syscall(SYS_statx, fd, "", AT_EMPTY_PATH,
                              (unsigned int)mask, &stat_buffer);
  if (result < 0L || (stat_buffer.stx_mask & STATX_MNT_ID_UNIQUE) == 0u ||
      (stat_buffer.stx_mask & STATX_INO) == 0u ||
      (stat_buffer.stx_mask & STATX_TYPE) == 0u ||
      (stat_buffer.stx_mask & STATX_SIZE) == 0u)
    return false;
  identity->mount_id = (uint64_t)stat_buffer.stx_mnt_id;
  identity->device_major = (uint64_t)stat_buffer.stx_dev_major;
  identity->device_minor = (uint64_t)stat_buffer.stx_dev_minor;
  identity->inode = (uint64_t)stat_buffer.stx_ino;
  *size = (uint64_t)stat_buffer.stx_size;
  *mode = (mode_t)stat_buffer.stx_mode;
  return identity->mount_id != 0u && identity->inode != 0u;
#else
  (void)identity;
  (void)size;
  (void)mode;
  return false;
#endif
}

static bool identity_equal(const linux_identity *left,
                           const linux_identity *right) {
  return left != NULL && right != NULL && left->mount_id == right->mount_id &&
         left->device_major == right->device_major &&
         left->device_minor == right->device_minor &&
         left->inode == right->inode;
}

static bool write_hex(char *destination, uint64_t value) {
  static const char hex[] = "0123456789abcdef";
  for (size_t index = 0u; index < 16u; index += 1u) {
    const unsigned int shift = (unsigned int)((15u - index) * 4u);
    destination[1u + index] =
        hex[(size_t)((value >> shift) & UINT64_C(0x0f))];
  }
  return true;
}

static bool write_identity_token(char *destination, size_t capacity, char prefix,
                                 const linux_identity *identity,
                                 size_t *length) {
  if (destination == NULL || identity == NULL || length == NULL ||
      capacity < LINUX_IDENTITY_TOKEN_LENGTH)
    return false;
  destination[0] = prefix;
  (void)write_hex(destination, identity->mount_id);
  destination[17u] = '-';
  (void)write_hex(destination + 17u, identity->device_major);
  destination[34u] = '-';
  (void)write_hex(destination + 34u, identity->device_minor);
  destination[51u] = '-';
  (void)write_hex(destination + 51u, identity->inode);
  *length = LINUX_IDENTITY_TOKEN_LENGTH;
  return true;
}

static bool write_tokens(
    w_seed_ephemeral_provider_token_buffers *tokens,
    const linux_identity *root_identity, const linux_identity *source_identity,
    w_seed_ephemeral_provider_observation *observation) {
  if (tokens == NULL || observation == NULL ||
      tokens->provider_id == NULL || tokens->root_token == NULL ||
      tokens->source_provider_owner_token == NULL ||
      tokens->canonical_token == NULL ||
      tokens->provider_id_capacity < LINUX_PROVIDER_ID_LENGTH)
    return false;
  (void)memcpy(tokens->provider_id, linux_provider_id,
               LINUX_PROVIDER_ID_LENGTH);
  if (!write_identity_token(tokens->root_token, tokens->root_token_capacity,
                            'r', root_identity,
                            &observation->root_token_length) ||
      !write_identity_token(
          tokens->source_provider_owner_token,
          tokens->source_provider_owner_token_capacity, 'o', root_identity,
          &observation->source_provider_owner_token_length) ||
      !write_identity_token(tokens->canonical_token,
                            tokens->canonical_token_capacity, 'c',
                            source_identity,
                            &observation->canonical_token_length))
    return false;
  observation->provider_id_length = LINUX_PROVIDER_ID_LENGTH;
  return true;
}

static int openat2_path(int directory_fd, const char *path, int flags) {
#if W_SEED_EPHEMERAL_PROVIDER_HAS_OPENAT2
  struct open_how how;
  (void)memset(&how, 0, sizeof(how));
  /* RESOLVE_NO_SYMLINKS rejects every symlink component. Do not add
   * O_NOFOLLOW here: with O_PATH|O_DIRECTORY it can turn a final symlink into
   * ENOTDIR instead of the required symlink diagnostic. */
  how.flags = (unsigned long long)(unsigned int)(flags | O_CLOEXEC);
  how.resolve =
      (unsigned long long)(RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS |
                           RESOLVE_NO_XDEV);
  const long result = syscall(SYS_openat2, directory_fd, path, &how,
                              sizeof(how));
  if (result < 0L) return -1;
  return (int)result;
#else
  (void)directory_fd;
  (void)path;
  (void)flags;
  errno = ENOSYS;
  return -1;
#endif
}

static bool openat2_probe(int directory_fd) {
#if W_SEED_EPHEMERAL_PROVIDER_HAS_OPENAT2
  const int probe_fd = openat2_path(directory_fd, ".", O_PATH | O_DIRECTORY);
  if (probe_fd < 0) return false;
  (void)close(probe_fd);
  return true;
#else
  (void)directory_fd;
  return false;
#endif
}

static bool duplicate_anchor(int source_fd, int *duplicate_fd) {
  if (duplicate_fd == NULL || source_fd < 0) return false;
  const int copy_fd = dup(source_fd);
  if (copy_fd < 0) return false;
  const int descriptor_flags = fcntl(copy_fd, F_GETFD);
  if (descriptor_flags < 0 ||
      fcntl(copy_fd, F_SETFD, descriptor_flags | FD_CLOEXEC) < 0) {
    (void)close(copy_fd);
    return false;
  }
  *duplicate_fd = copy_fd;
  return true;
}

static w_seed_ephemeral_provider_backend_status open_root_parent(
    w_seed_ephemeral_provider_linux_context *context,
    w_seed_byte_view root_path, int *parent_fd, linux_identity *parent_identity,
    size_t *leaf_start) {
  if (context == NULL || parent_fd == NULL || parent_identity == NULL ||
      leaf_start == NULL || !context->initialized ||
      !context->openat2_supported)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
  size_t parent_length = 0u;
  bool absolute = false;
  if (!root_path_parts(root_path, &parent_length, leaf_start, &absolute))
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  int current_fd = -1;
  if (absolute) {
    current_fd = open("/", O_PATH | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  } else if (!duplicate_anchor(context->base_dir_fd, &current_fd)) {
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO;
  }
  if (current_fd < 0)
    return errno_status(errno);
  const size_t first_component = absolute ? 1u : 0u;
  size_t component_start = first_component;
  if (absolute || parent_length != 0u) {
    for (size_t index = first_component; index <= parent_length;
         index += 1u) {
      if (index != parent_length && root_path.data[index] != (uint8_t)'/')
        continue;
      if (!component_valid(root_path.data, component_start, index)) {
        (void)close(current_fd);
        return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
      }
      char component[W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES + 1u];
      if (!copy_component(root_path.data, component_start, index, component)) {
        (void)close(current_fd);
        return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
      }
      const int next_fd = openat2_path(current_fd, component,
                                       O_PATH | O_DIRECTORY);
      if (next_fd < 0) {
        const w_seed_ephemeral_provider_backend_status status =
            errno_status(errno);
        (void)close(current_fd);
        return status;
      }
      (void)close(current_fd);
      current_fd = next_fd;
      component_start = index + 1u;
    }
  }
  uint64_t parent_size = 0u;
  mode_t parent_mode = 0;
  if (!statx_identity(current_fd, parent_identity, &parent_size,
                      &parent_mode) || !S_ISDIR(parent_mode)) {
    (void)close(current_fd);
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
  }
  *parent_fd = current_fd;
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
}

static bool source_size(uint64_t raw_size, size_t *size) {
  if (size == NULL || raw_size > (uint64_t)SIZE_MAX) return false;
  *size = (size_t)raw_size;
  return true;
}

static bool offset_value(size_t value, off_t *offset) {
  if (offset == NULL || (uintmax_t)value > (uintmax_t)INT64_MAX)
    return false;
  *offset = (off_t)value;
  return true;
}

static w_seed_ephemeral_provider_backend_status read_fd(
    int fd, const linux_identity *expected_identity, uint8_t *bytes,
    size_t capacity, size_t *written) {
  if (fd < 0 || written == NULL) return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  *written = 0u;
  linux_identity before_identity;
  uint64_t before_size = 0u;
  mode_t before_mode = 0;
  if (!statx_identity(fd, &before_identity, &before_size, &before_mode))
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO;
  if (!S_ISREG(before_mode) ||
      !identity_equal(&before_identity, expected_identity))
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
  size_t length = 0u;
  if (!source_size(before_size, &length))
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
  if (length > capacity) {
    *written = length;
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_CAPACITY;
  }
  if (length != 0u && bytes == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  size_t offset = 0u;
  while (offset < length) {
    off_t file_offset;
    if (!offset_value(offset, &file_offset))
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
    const ssize_t count = pread(fd, bytes + offset, length - offset, file_offset);
    if (count < (ssize_t)0) {
      if (errno == EINTR) continue;
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO;
    }
    if (count == (ssize_t)0) return W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO;
    const size_t count_size = (size_t)count;
    if (count_size > length - offset)
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
    offset += count_size;
  }
  linux_identity after_identity;
  uint64_t after_size = 0u;
  mode_t after_mode = 0;
  if (!statx_identity(fd, &after_identity, &after_size, &after_mode))
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO;
  size_t after_length = 0u;
  if (!S_ISREG(after_mode) || !source_size(after_size, &after_length) ||
      after_length != length ||
      !identity_equal(&after_identity, expected_identity))
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO;
  *written = length;
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
}

static w_seed_ephemeral_provider_backend_status linux_open_root(
    void *context_value, w_seed_byte_view root_path,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_handle *root_handle,
    w_seed_ephemeral_provider_handle *root_source_handle,
    w_seed_ephemeral_provider_observation *observation) {
  w_seed_ephemeral_provider_linux_context *context = context_value;
  if (root_handle != NULL) root_handle->value = (uintptr_t)0u;
  if (root_source_handle != NULL)
    root_source_handle->value = (uintptr_t)0u;
  if (observation != NULL) (void)memset(observation, 0, sizeof(*observation));
  if (context == NULL || root_handle == NULL || root_source_handle == NULL ||
      observation == NULL || tokens == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  if (!context->initialized || !context->openat2_supported)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
  int parent_fd = -1;
  linux_identity parent_identity;
  (void)memset(&parent_identity, 0, sizeof(parent_identity));
  size_t leaf_start = 0u;
  w_seed_ephemeral_provider_backend_status status = open_root_parent(
      context, root_path, &parent_fd, &parent_identity, &leaf_start);
  if (status != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) return status;
  const size_t leaf_length = root_path.length - leaf_start;
  if (leaf_length > W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES) {
    (void)close(parent_fd);
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  }
  (void)memcpy(context->root_leaf, root_path.data + leaf_start, leaf_length);
  context->root_leaf[leaf_length] = '\0';
  context->root_leaf_length = leaf_length;
  const int source_fd =
      openat2_path(parent_fd, context->root_leaf, O_RDONLY | O_NONBLOCK);
  if (source_fd < 0) {
    status = errno_status(errno);
    (void)close(parent_fd);
    context->root_leaf_length = 0u;
    context->root_leaf[0] = '\0';
    return status;
  }
  linux_identity source_identity;
  uint64_t source_size_value = 0u;
  mode_t source_mode = 0;
  if (!statx_identity(source_fd, &source_identity, &source_size_value,
                      &source_mode) ||
      !S_ISREG(source_mode)) {
    (void)close(source_fd);
    (void)close(parent_fd);
    context->root_leaf_length = 0u;
    context->root_leaf[0] = '\0';
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
  }
  w_seed_ephemeral_provider_handle local_root = {(uintptr_t)0u};
  w_seed_ephemeral_provider_handle local_source = {(uintptr_t)0u};
  if (!allocate_slot(context, parent_fd, &parent_identity,
                     W_SEED_EPHEMERAL_PROVIDER_LINUX_SLOT_ROOT_DIRECTORY,
                     &local_root)) {
    (void)close(parent_fd);
    (void)close(source_fd);
    context->root_leaf_length = 0u;
    context->root_leaf[0] = '\0';
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO;
  }
  if (!allocate_slot(context, source_fd, &source_identity,
                     W_SEED_EPHEMERAL_PROVIDER_LINUX_SLOT_ROOT_SOURCE,
                     &local_source)) {
    w_seed_ephemeral_provider_linux_slot *root_slot =
        slot_for_handle(context, local_root,
                        W_SEED_EPHEMERAL_PROVIDER_LINUX_SLOT_ROOT_DIRECTORY);
    if (root_slot != NULL)
      release_slot(root_slot);
    else
      (void)close(parent_fd);
    (void)close(source_fd);
    context->root_leaf_length = 0u;
    context->root_leaf[0] = '\0';
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO;
  }
  if (!write_tokens(tokens, &parent_identity, &source_identity, observation)) {
    w_seed_ephemeral_provider_linux_slot *source_slot =
        slot_for_handle(context, local_source,
                        W_SEED_EPHEMERAL_PROVIDER_LINUX_SLOT_ROOT_SOURCE);
    w_seed_ephemeral_provider_linux_slot *root_slot =
        slot_for_handle(context, local_root,
                        W_SEED_EPHEMERAL_PROVIDER_LINUX_SLOT_ROOT_DIRECTORY);
    if (source_slot != NULL)
      release_slot(source_slot);
    else
      (void)close(source_fd);
    if (root_slot != NULL)
      release_slot(root_slot);
    else
      (void)close(parent_fd);
    context->root_leaf_length = 0u;
    context->root_leaf[0] = '\0';
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  }
  observation->opened = true;
  observation->containment_inside = true;
  observation->symlink = W_SEED_EPHEMERAL_GRAPH_SYMLINK_NONE;
  *root_handle = local_root;
  *root_source_handle = local_source;
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
}

static w_seed_ephemeral_provider_backend_status linux_open_source(
    void *context_value, w_seed_ephemeral_provider_handle root_handle,
    w_seed_frontend_text source_id,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_handle *source_handle,
    w_seed_ephemeral_provider_observation *observation) {
  w_seed_ephemeral_provider_linux_context *context = context_value;
  if (source_handle != NULL) source_handle->value = (uintptr_t)0u;
  if (observation != NULL) (void)memset(observation, 0, sizeof(*observation));
  if (context == NULL || source_handle == NULL || observation == NULL ||
      tokens == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  if (!context->initialized || !context->openat2_supported)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
  w_seed_ephemeral_provider_linux_slot *root_slot = slot_for_handle(
      context, root_handle,
      W_SEED_EPHEMERAL_PROVIDER_LINUX_SLOT_ROOT_DIRECTORY);
  if (root_slot == NULL) return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  char path[W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES + 1u];
  if (!copy_source_id(source_id, path))
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  const int source_fd = openat2_path(root_slot->fd, path,
                                     O_RDONLY | O_NONBLOCK);
  if (source_fd < 0) return errno_status(errno);
  linux_identity source_identity;
  uint64_t source_size_value = 0u;
  mode_t source_mode = 0;
  if (!statx_identity(source_fd, &source_identity, &source_size_value,
                      &source_mode) ||
      !S_ISREG(source_mode)) {
    (void)close(source_fd);
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
  }
  w_seed_ephemeral_provider_handle local_source = {(uintptr_t)0u};
  if (!allocate_slot(context, source_fd, &source_identity,
                     W_SEED_EPHEMERAL_PROVIDER_LINUX_SLOT_CHILD_SOURCE,
                     &local_source)) {
    (void)close(source_fd);
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO;
  }
  const linux_identity root_identity = {
      root_slot->mount_id, root_slot->device_major, root_slot->device_minor,
      root_slot->inode};
  if (!write_tokens(tokens, &root_identity, &source_identity, observation)) {
    w_seed_ephemeral_provider_linux_slot *source_slot = slot_for_handle(
        context, local_source,
        W_SEED_EPHEMERAL_PROVIDER_LINUX_SLOT_CHILD_SOURCE);
    if (source_slot != NULL) release_slot(source_slot);
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  }
  observation->opened = true;
  observation->containment_inside = true;
  observation->symlink = W_SEED_EPHEMERAL_GRAPH_SYMLINK_NONE;
  *source_handle = local_source;
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
}

static w_seed_ephemeral_provider_backend_status linux_read_source(
    void *context_value, w_seed_ephemeral_provider_handle source_handle,
    uint8_t *bytes, size_t capacity, size_t *written) {
  w_seed_ephemeral_provider_linux_context *context = context_value;
  if (context == NULL || written == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  w_seed_ephemeral_provider_linux_slot *source_slot = slot_for_handle(
      context, source_handle,
      W_SEED_EPHEMERAL_PROVIDER_LINUX_SLOT_ROOT_SOURCE);
  if (source_slot == NULL)
    source_slot = slot_for_handle(
        context, source_handle,
        W_SEED_EPHEMERAL_PROVIDER_LINUX_SLOT_CHILD_SOURCE);
  if (source_slot == NULL) return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  const linux_identity source_identity = {
      source_slot->mount_id, source_slot->device_major,
      source_slot->device_minor, source_slot->inode};
  return read_fd(source_slot->fd, &source_identity, bytes, capacity, written);
}

static w_seed_ephemeral_provider_backend_status linux_revalidate_source(
    void *context_value, w_seed_ephemeral_provider_handle root_handle,
    w_seed_ephemeral_provider_handle source_handle,
    w_seed_frontend_text source_id,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_observation *observation, uint8_t *bytes,
    size_t capacity, size_t *written) {
  w_seed_ephemeral_provider_linux_context *context = context_value;
  if (written != NULL) *written = 0u;
  if (context == NULL || tokens == NULL || observation == NULL ||
      written == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  (void)memset(observation, 0, sizeof(*observation));
  if (!context->initialized || !context->openat2_supported)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
  w_seed_ephemeral_provider_linux_slot *root_slot = slot_for_handle(
      context, root_handle,
      W_SEED_EPHEMERAL_PROVIDER_LINUX_SLOT_ROOT_DIRECTORY);
  if (root_slot == NULL) return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  w_seed_ephemeral_provider_linux_slot *source_slot = slot_for_handle(
      context, source_handle,
      W_SEED_EPHEMERAL_PROVIDER_LINUX_SLOT_ROOT_SOURCE);
  const bool root_source = source_slot != NULL;
  if (!root_source)
    source_slot = slot_for_handle(
        context, source_handle,
        W_SEED_EPHEMERAL_PROVIDER_LINUX_SLOT_CHILD_SOURCE);
  if (source_slot == NULL) return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  const linux_identity root_identity = {
      root_slot->mount_id, root_slot->device_major, root_slot->device_minor,
      root_slot->inode};
  linux_identity current_root_identity;
  uint64_t current_root_size = 0u;
  mode_t current_root_mode = 0;
  if (!statx_identity(root_slot->fd, &current_root_identity, &current_root_size,
                      &current_root_mode) ||
      !S_ISDIR(current_root_mode) ||
      !identity_equal(&current_root_identity, &root_identity))
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO;
  char path[W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES + 1u];
  if (root_source) {
    if (context->root_leaf_length == 0u ||
        context->root_leaf_length > W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES)
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
    (void)memcpy(path, context->root_leaf, context->root_leaf_length + 1u);
  } else if (!copy_source_id(source_id, path)) {
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  }
  const int reopened_fd = openat2_path(root_slot->fd, path,
                                       O_RDONLY | O_NONBLOCK);
  if (reopened_fd < 0) return errno_status(errno);
  linux_identity reopened_identity;
  uint64_t reopened_size = 0u;
  mode_t reopened_mode = 0;
  if (!statx_identity(reopened_fd, &reopened_identity, &reopened_size,
                      &reopened_mode) ||
      !S_ISREG(reopened_mode)) {
    (void)close(reopened_fd);
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
  }
  (void)reopened_size;
  const w_seed_ephemeral_provider_backend_status read_status = read_fd(
      reopened_fd, &reopened_identity, bytes, capacity, written);
  if (read_status != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) {
    (void)close(reopened_fd);
    return read_status;
  }
  if (!write_tokens(tokens, &root_identity, &reopened_identity, observation)) {
    (void)close(reopened_fd);
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  }
  (void)close(reopened_fd);
  observation->opened = true;
  observation->containment_inside = true;
  observation->symlink = W_SEED_EPHEMERAL_GRAPH_SYMLINK_NONE;
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
}

static void linux_close_source(
    void *context_value, w_seed_ephemeral_provider_handle source_handle) {
  w_seed_ephemeral_provider_linux_context *context = context_value;
  if (context == NULL) return;
  w_seed_ephemeral_provider_linux_slot *slot = slot_for_handle(
      context, source_handle,
      W_SEED_EPHEMERAL_PROVIDER_LINUX_SLOT_ROOT_SOURCE);
  if (slot == NULL)
    slot = slot_for_handle(context, source_handle,
                           W_SEED_EPHEMERAL_PROVIDER_LINUX_SLOT_CHILD_SOURCE);
  release_slot(slot);
}

static void linux_close_root(void *context_value,
                             w_seed_ephemeral_provider_handle root_handle) {
  w_seed_ephemeral_provider_linux_context *context = context_value;
  if (context == NULL) return;
  w_seed_ephemeral_provider_linux_slot *slot = slot_for_handle(
      context, root_handle,
      W_SEED_EPHEMERAL_PROVIDER_LINUX_SLOT_ROOT_DIRECTORY);
  release_slot(slot);
  if (slot == NULL) return;
  context->root_leaf_length = 0u;
  context->root_leaf[0] = '\0';
}

bool w_seed_ephemeral_provider_linux_init(
    w_seed_ephemeral_provider_linux_context *context, int base_dir_fd) {
  if (context == NULL) return false;
  (void)memset(context, 0, sizeof(*context));
  context->base_dir_fd = base_dir_fd;
  context->next_generation = 1u;
  if (base_dir_fd == AT_FDCWD) return false;
#if W_SEED_EPHEMERAL_PROVIDER_HAS_OPENAT2
  struct stat base_stat;
  if (fstat(base_dir_fd, &base_stat) < 0 || !S_ISDIR(base_stat.st_mode))
    return false;
  context->openat2_supported = openat2_probe(base_dir_fd);
#else
  (void)base_dir_fd;
  context->openat2_supported = false;
#endif
  context->initialized = true;
  return true;
}

bool w_seed_ephemeral_provider_linux_backend(
    w_seed_ephemeral_provider_linux_context *context,
    w_seed_ephemeral_provider_backend *backend) {
  if (context == NULL || backend == NULL || !context->initialized)
    return false;
  (void)memset(backend, 0, sizeof(*backend));
  backend->context = context;
  backend->open_root = linux_open_root;
  backend->open_source = linux_open_source;
  backend->read_source = linux_read_source;
  backend->revalidate_source = linux_revalidate_source;
  backend->close_source = linux_close_source;
  backend->close_root = linux_close_root;
  backend->metadata = linux_metadata();
  return true;
}

#else

static w_seed_ephemeral_provider_backend_status unsupported_open_root(
    void *context, w_seed_byte_view root_path,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_handle *root_handle,
    w_seed_ephemeral_provider_handle *root_source_handle,
    w_seed_ephemeral_provider_observation *observation) {
  (void)context;
  (void)root_path;
  (void)tokens;
  if (root_handle != NULL) root_handle->value = (uintptr_t)0u;
  if (root_source_handle != NULL)
    root_source_handle->value = (uintptr_t)0u;
  if (observation != NULL) (void)memset(observation, 0, sizeof(*observation));
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
}

static w_seed_ephemeral_provider_backend_status unsupported_open_source(
    void *context, w_seed_ephemeral_provider_handle root_handle,
    w_seed_frontend_text source_id,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_handle *source_handle,
    w_seed_ephemeral_provider_observation *observation) {
  (void)context;
  (void)root_handle;
  (void)source_id;
  (void)tokens;
  if (source_handle != NULL) source_handle->value = (uintptr_t)0u;
  if (observation != NULL) (void)memset(observation, 0, sizeof(*observation));
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
}

static w_seed_ephemeral_provider_backend_status unsupported_read_source(
    void *context, w_seed_ephemeral_provider_handle source_handle,
    uint8_t *bytes, size_t capacity, size_t *written) {
  (void)context;
  (void)source_handle;
  (void)bytes;
  (void)capacity;
  if (written != NULL) *written = 0u;
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
}

static w_seed_ephemeral_provider_backend_status unsupported_revalidate_source(
    void *context, w_seed_ephemeral_provider_handle root_handle,
    w_seed_ephemeral_provider_handle source_handle,
    w_seed_frontend_text source_id,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_observation *observation, uint8_t *bytes,
    size_t capacity, size_t *written) {
  (void)context;
  (void)root_handle;
  (void)source_handle;
  (void)source_id;
  (void)tokens;
  (void)bytes;
  (void)capacity;
  if (observation != NULL) (void)memset(observation, 0, sizeof(*observation));
  if (written != NULL) *written = 0u;
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
}

static void unsupported_close_source(
    void *context, w_seed_ephemeral_provider_handle source_handle) {
  (void)context;
  (void)source_handle;
}

static void unsupported_close_root(void *context,
                                   w_seed_ephemeral_provider_handle root_handle) {
  (void)context;
  (void)root_handle;
}

bool w_seed_ephemeral_provider_linux_init(
    w_seed_ephemeral_provider_linux_context *context, int base_dir_fd) {
  if (context == NULL) return false;
  (void)memset(context, 0, sizeof(*context));
  context->base_dir_fd = base_dir_fd;
  context->next_generation = 1u;
  context->initialized = true;
  context->openat2_supported = false;
  return true;
}

bool w_seed_ephemeral_provider_linux_backend(
    w_seed_ephemeral_provider_linux_context *context,
    w_seed_ephemeral_provider_backend *backend) {
  if (context == NULL || backend == NULL || !context->initialized)
    return false;
  (void)memset(backend, 0, sizeof(*backend));
  backend->context = context;
  backend->open_root = unsupported_open_root;
  backend->open_source = unsupported_open_source;
  backend->read_source = unsupported_read_source;
  backend->revalidate_source = unsupported_revalidate_source;
  backend->close_source = unsupported_close_source;
  backend->close_root = unsupported_close_root;
  backend->metadata = linux_metadata();
  return true;
}

#endif
