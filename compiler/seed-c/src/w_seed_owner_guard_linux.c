#if defined(__linux__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#endif

#include "w_seed_owner_guard_linux.h"

#include <limits.h>
#include <string.h>

static w_seed_owner_guard_backend_result linux_result(
    w_seed_owner_guard_backend_status status,
    w_seed_owner_guard_backend_phase phase, size_t level,
    size_t required_capacity, uint64_t generation, size_t level_count,
    size_t candidate_count) {
  const w_seed_owner_guard_backend_result result = {
      status, phase, level, required_capacity, generation, level_count,
      candidate_count};
  return result;
}

#if defined(__linux__)

#include <errno.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <linux/stat.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#if defined(SYS_openat2) && defined(SYS_statx) && \
    defined(RESOLVE_BENEATH) && defined(RESOLVE_NO_SYMLINKS) && \
    defined(RESOLVE_NO_MAGICLINKS) && defined(RESOLVE_NO_XDEV) && \
    defined(STATX_MNT_ID_UNIQUE)
#define W_SEED_OWNER_GUARD_LINUX_NATIVE 1
#else
#define W_SEED_OWNER_GUARD_LINUX_NATIVE 0
#endif

static bool identity_equal(const w_seed_owner_guard_linux_identity *left,
                           const w_seed_owner_guard_linux_identity *right) {
  return left != NULL && right != NULL &&
         left->mount_id == right->mount_id &&
         left->device_major == right->device_major &&
         left->device_minor == right->device_minor &&
         left->inode == right->inode;
}

static w_seed_owner_guard_backend_status errno_status(int error_code) {
  if (error_code == ELOOP) return W_SEED_OWNER_GUARD_BACKEND_REPARSE;
  if (error_code == EXDEV) return W_SEED_OWNER_GUARD_BACKEND_BOUNDARY;
  if (error_code == ENOSYS) return W_SEED_OWNER_GUARD_BACKEND_UNSUPPORTED;
  return W_SEED_OWNER_GUARD_BACKEND_IO;
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

static bool source_path_valid(w_seed_byte_view path) {
  if (path.data == NULL || path.length == 0u ||
      path.length > W_SEED_OWNER_GUARD_MAX_PATH_BYTES ||
      path.data[0] == (uint8_t)'/' ||
      path.data[path.length - 1u] == (uint8_t)'/')
    return false;
  w_seed_source utf8;
  w_seed_source_error utf8_error;
  if (!w_seed_source_init(path, &utf8, &utf8_error)) return false;
  size_t component_start = 0u;
  for (size_t index = 0u; index <= path.length; index += 1u) {
    if (index != path.length && path.data[index] != (uint8_t)'/') continue;
    if (!component_valid(path.data, component_start, index)) return false;
    component_start = index + 1u;
  }
  return true;
}

static int openat2_component(int directory_fd, const char *path, int flags,
                             bool downward) {
#if W_SEED_OWNER_GUARD_LINUX_NATIVE
  struct open_how how;
  (void)memset(&how, 0, sizeof(how));
  how.flags = (uint64_t)(unsigned int)(flags | O_CLOEXEC);
  how.resolve = (uint64_t)(RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS |
                           RESOLVE_NO_XDEV |
                           (downward ? RESOLVE_BENEATH : 0u));
  const long value = syscall(SYS_openat2, directory_fd, path, &how,
                             sizeof(how));
  return value < 0L ? -1 : (int)value;
#else
  (void)directory_fd;
  (void)path;
  (void)flags;
  (void)downward;
  errno = ENOSYS;
  return -1;
#endif
}

static bool statx_identity(int fd, bool directory,
                           w_seed_owner_guard_linux_identity *identity) {
#if W_SEED_OWNER_GUARD_LINUX_NATIVE
  if (fd < 0 || identity == NULL) return false;
  struct statx value;
  (void)memset(&value, 0, sizeof(value));
  const unsigned int mask = STATX_TYPE | STATX_INO | STATX_MNT_ID_UNIQUE;
  const long status = syscall(SYS_statx, fd, "",
                              AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW, mask,
                              &value);
  if (status != 0L || (value.stx_mask & mask) != mask) return false;
  const unsigned int type = (unsigned int)value.stx_mode & S_IFMT;
  if ((directory && type != S_IFDIR) || (!directory && type != S_IFREG))
    return false;
  identity->mount_id = value.stx_mnt_id;
  identity->device_major = value.stx_dev_major;
  identity->device_minor = value.stx_dev_minor;
  identity->inode = value.stx_ino;
  return identity->mount_id != 0u && identity->inode != 0u;
#else
  (void)fd;
  (void)directory;
  (void)identity;
  return false;
#endif
}

static bool duplicate_fd(int source, int *destination) {
  if (source < 0 || destination == NULL) return false;
  const int value = fcntl(source, F_DUPFD_CLOEXEC, 0);
  if (value < 0) return false;
  *destination = value;
  return true;
}

static void clear_slot(w_seed_owner_guard_linux_slot *slot) {
  if (slot == NULL) return;
  slot->fd = -1;
  (void)memset(&slot->identity, 0, sizeof(slot->identity));
  slot->kind = W_SEED_OWNER_GUARD_LINUX_SLOT_EMPTY;
  slot->used = false;
}

static bool push_slot(w_seed_owner_guard_linux_context *context, int fd,
                      const w_seed_owner_guard_linux_identity *identity,
                      w_seed_owner_guard_linux_slot_kind kind,
                      size_t *slot_index) {
  if (context == NULL || identity == NULL || slot_index == NULL || fd < 0 ||
      context->slot_count >= W_SEED_OWNER_GUARD_LINUX_MAX_HANDLES)
    return false;
  const size_t index = context->slot_count;
  context->slots[index].fd = fd;
  context->slots[index].identity = *identity;
  context->slots[index].kind = kind;
  context->slots[index].used = true;
  context->slot_count += 1u;
  *slot_index = index;
  return true;
}

static void cleanup_session(w_seed_owner_guard_linux_context *context) {
  if (context == NULL) return;
  for (size_t index = context->slot_count; index > 0u; index -= 1u) {
    w_seed_owner_guard_linux_slot *slot = &context->slots[index - 1u];
    if (slot->used && slot->fd >= 0) (void)close(slot->fd);
    clear_slot(slot);
  }
  context->slot_count = 0u;
  context->level_count = 0u;
  context->candidate_count = 0u;
  context->source_slot = SIZE_MAX;
  context->source_path_length = 0u;
  context->source_path[0] = '\0';
  for (size_t index = 0u; index < W_SEED_OWNER_GUARD_MAX_LEVELS;
       index += 1u) {
    context->directory_slots[index] = SIZE_MAX;
    context->candidate_slots[index] = SIZE_MAX;
  }
  context->active_generation = 0u;
  context->session_live = false;
}

static bool retained_slot(
    const w_seed_owner_guard_linux_context *context, size_t slot_index,
    w_seed_owner_guard_linux_slot_kind kind,
    const w_seed_owner_guard_linux_slot **slot) {
  if (context == NULL || slot == NULL || slot_index >= context->slot_count)
    return false;
  const w_seed_owner_guard_linux_slot *value = &context->slots[slot_index];
  if (!value->used || value->fd < 0 || value->kind != kind) return false;
  *slot = value;
  return true;
}

static w_seed_owner_guard_backend_status open_source_binding(
    const w_seed_owner_guard_linux_context *context, const char *path,
    size_t path_length, int *parent_fd,
    w_seed_owner_guard_linux_identity *parent_identity, int *source_fd,
    w_seed_owner_guard_linux_identity *source_identity) {
  if (context == NULL || path == NULL || path_length == 0u ||
      parent_fd == NULL || parent_identity == NULL || source_fd == NULL ||
      source_identity == NULL)
    return W_SEED_OWNER_GUARD_BACKEND_INVALID;
  *parent_fd = -1;
  *source_fd = -1;
  int current = -1;
  if (!duplicate_fd(context->base_dir_fd, &current))
    return W_SEED_OWNER_GUARD_BACKEND_IO;
  w_seed_owner_guard_linux_identity duplicate_identity;
  if (!statx_identity(current, true, &duplicate_identity) ||
      !identity_equal(&duplicate_identity, &context->base_identity)) {
    (void)close(current);
    return W_SEED_OWNER_GUARD_BACKEND_MUTATED;
  }
  size_t component_start = 0u;
  size_t leaf_start = 0u;
  for (size_t index = 0u; index <= path_length; index += 1u) {
    if (index != path_length && path[index] != '/') continue;
    if (index == path_length) {
      leaf_start = component_start;
      break;
    }
    char component[W_SEED_OWNER_GUARD_MAX_PATH_BYTES + 1u];
    const size_t length = index - component_start;
    (void)memcpy(component, path + component_start, length);
    component[length] = '\0';
    const int next = openat2_component(current, component,
                                       O_PATH | O_DIRECTORY, true);
    if (next < 0) {
      const w_seed_owner_guard_backend_status status = errno_status(errno);
      (void)close(current);
      return status;
    }
    (void)close(current);
    current = next;
    component_start = index + 1u;
  }
  if (!statx_identity(current, true, parent_identity)) {
    (void)close(current);
    return W_SEED_OWNER_GUARD_BACKEND_UNSUPPORTED;
  }
  const int source = openat2_component(current, path + leaf_start, O_PATH, true);
  if (source < 0) {
    const w_seed_owner_guard_backend_status status = errno_status(errno);
    (void)close(current);
    return status;
  }
  if (!statx_identity(source, false, source_identity)) {
    (void)close(source);
    (void)close(current);
    return W_SEED_OWNER_GUARD_BACKEND_UNSUPPORTED;
  }
  if (parent_identity->mount_id != source_identity->mount_id) {
    (void)close(source);
    (void)close(current);
    return W_SEED_OWNER_GUARD_BACKEND_BOUNDARY;
  }
  *parent_fd = current;
  *source_fd = source;
  return W_SEED_OWNER_GUARD_BACKEND_OK;
}

static w_seed_owner_guard_backend_status open_candidate(
    int directory_fd, int *candidate_fd,
    w_seed_owner_guard_linux_identity *identity, bool *absent) {
  if (candidate_fd == NULL || identity == NULL || absent == NULL)
    return W_SEED_OWNER_GUARD_BACKEND_INVALID;
  *candidate_fd = -1;
  *absent = false;
  const int value = openat2_component(directory_fd, "build.w", O_PATH, true);
  if (value < 0) {
    if (errno == ENOENT) {
      *absent = true;
      return W_SEED_OWNER_GUARD_BACKEND_OK;
    }
    return errno_status(errno);
  }
  if (!statx_identity(value, false, identity)) {
    (void)close(value);
    return W_SEED_OWNER_GUARD_BACKEND_UNSUPPORTED;
  }
  *candidate_fd = value;
  return W_SEED_OWNER_GUARD_BACKEND_OK;
}

static w_seed_owner_guard_backend_status open_parent(
    int directory_fd, int *parent_fd,
    w_seed_owner_guard_linux_identity *identity) {
  if (parent_fd == NULL || identity == NULL)
    return W_SEED_OWNER_GUARD_BACKEND_INVALID;
  *parent_fd = openat2_component(directory_fd, "..", O_PATH | O_DIRECTORY,
                                 false);
  if (*parent_fd < 0) return errno_status(errno);
  if (!statx_identity(*parent_fd, true, identity)) {
    (void)close(*parent_fd);
    *parent_fd = -1;
    return W_SEED_OWNER_GUARD_BACKEND_UNSUPPORTED;
  }
  return W_SEED_OWNER_GUARD_BACKEND_OK;
}

static bool identity_seen(const w_seed_owner_guard_linux_context *context,
                          const w_seed_owner_guard_linux_identity *identity,
                          size_t level_count) {
  for (size_t index = 0u; index < level_count; index += 1u) {
    const size_t slot_index = context->directory_slots[index];
    if (slot_index < context->slot_count &&
        identity_equal(&context->slots[slot_index].identity, identity))
      return true;
  }
  return false;
}

static w_seed_owner_guard_backend_result linux_begin(
    void *context_value, w_seed_byte_view source_path,
    w_seed_owner_guard_observation *observations,
    size_t observation_capacity) {
  w_seed_owner_guard_linux_context *context = context_value;
  if (context == NULL || observations == NULL || observation_capacity == 0u)
    return linux_result(W_SEED_OWNER_GUARD_BACKEND_INVALID,
                        W_SEED_OWNER_GUARD_BACKEND_PHASE_VALIDATE,
                        W_SEED_OWNER_GUARD_NO_LEVEL, 0u, 0u, 0u, 0u);
  if (!context->initialized || !context->native_supported)
    return linux_result(W_SEED_OWNER_GUARD_BACKEND_UNSUPPORTED,
                        W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_START,
                        W_SEED_OWNER_GUARD_NO_LEVEL, 0u, 0u, 0u, 0u);
  if (context->session_live || !source_path_valid(source_path))
    return linux_result(W_SEED_OWNER_GUARD_BACKEND_INVALID,
                        W_SEED_OWNER_GUARD_BACKEND_PHASE_VALIDATE,
                        W_SEED_OWNER_GUARD_NO_LEVEL, 0u, 0u, 0u, 0u);
  w_seed_owner_guard_linux_identity base_now;
  if (!statx_identity(context->base_dir_fd, true, &base_now) ||
      !identity_equal(&base_now, &context->base_identity))
    return linux_result(W_SEED_OWNER_GUARD_BACKEND_MUTATED,
                        W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_START,
                        W_SEED_OWNER_GUARD_NO_LEVEL, 0u, 0u, 0u, 0u);
  if (context->next_generation == 0u ||
      context->next_generation == UINT64_MAX)
    return linux_result(W_SEED_OWNER_GUARD_BACKEND_FAULT,
                        W_SEED_OWNER_GUARD_BACKEND_PHASE_VALIDATE,
                        W_SEED_OWNER_GUARD_NO_LEVEL, 0u, 0u, 0u, 0u);
  context->active_generation = context->next_generation;
  context->next_generation += 1u;
  context->session_live = true;
  (void)memcpy(context->source_path, source_path.data, source_path.length);
  context->source_path[source_path.length] = '\0';
  context->source_path_length = source_path.length;

  int parent_fd = -1;
  int source_fd = -1;
  w_seed_owner_guard_linux_identity parent_identity;
  w_seed_owner_guard_linux_identity source_identity;
  w_seed_owner_guard_backend_status status = open_source_binding(
      context, context->source_path, context->source_path_length, &parent_fd,
      &parent_identity, &source_fd, &source_identity);
  if (status != W_SEED_OWNER_GUARD_BACKEND_OK) {
    cleanup_session(context);
    return linux_result(status, W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_START,
                        W_SEED_OWNER_GUARD_NO_LEVEL, 0u, 0u, 0u, 0u);
  }
  size_t parent_slot = SIZE_MAX;
  if (!push_slot(context, parent_fd, &parent_identity,
                 W_SEED_OWNER_GUARD_LINUX_SLOT_DIRECTORY, &parent_slot)) {
    (void)close(parent_fd);
    (void)close(source_fd);
    cleanup_session(context);
    return linux_result(W_SEED_OWNER_GUARD_BACKEND_FAULT,
                        W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_START,
                        W_SEED_OWNER_GUARD_NO_LEVEL, 0u, 0u, 0u, 0u);
  }
  context->directory_slots[0] = parent_slot;
  if (!push_slot(context, source_fd, &source_identity,
                 W_SEED_OWNER_GUARD_LINUX_SLOT_SOURCE,
                 &context->source_slot)) {
    (void)close(source_fd);
    cleanup_session(context);
    return linux_result(W_SEED_OWNER_GUARD_BACKEND_FAULT,
                        W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_START,
                        W_SEED_OWNER_GUARD_NO_LEVEL, 0u, 0u, 0u, 0u);
  }

  size_t candidate_count = 0u;
  size_t level = 0u;
  for (;;) {
    const size_t directory_slot_index = context->directory_slots[level];
    w_seed_owner_guard_linux_slot *directory_slot =
        &context->slots[directory_slot_index];
    observations[level].directory_ordinal = level;
    observations[level].candidate_index = W_SEED_OWNER_GUARD_NO_CANDIDATE;
    observations[level].root_terminal = false;
    int candidate_fd = -1;
    bool absent = false;
    w_seed_owner_guard_linux_identity candidate_identity;
    status = open_candidate(directory_slot->fd, &candidate_fd,
                            &candidate_identity, &absent);
    if (status != W_SEED_OWNER_GUARD_BACKEND_OK) {
      cleanup_session(context);
      return linux_result(status,
                          W_SEED_OWNER_GUARD_BACKEND_PHASE_LOOKUP_CANDIDATE,
                          level, 0u, 0u, 0u, 0u);
    }
    if (!absent) {
      if (candidate_identity.mount_id != directory_slot->identity.mount_id ||
          !push_slot(context, candidate_fd, &candidate_identity,
                     W_SEED_OWNER_GUARD_LINUX_SLOT_CANDIDATE,
                     &context->candidate_slots[level])) {
        (void)close(candidate_fd);
        cleanup_session(context);
        return linux_result(W_SEED_OWNER_GUARD_BACKEND_BOUNDARY,
                            W_SEED_OWNER_GUARD_BACKEND_PHASE_LOOKUP_CANDIDATE,
                            level, 0u, 0u, 0u, 0u);
      }
      observations[level].candidate_index = candidate_count;
      candidate_count += 1u;
    }
    int next_parent = -1;
    w_seed_owner_guard_linux_identity next_identity;
    status = open_parent(directory_slot->fd, &next_parent, &next_identity);
    if (status != W_SEED_OWNER_GUARD_BACKEND_OK ||
        next_identity.mount_id != directory_slot->identity.mount_id) {
      if (next_parent >= 0) (void)close(next_parent);
      cleanup_session(context);
      return linux_result(
          status == W_SEED_OWNER_GUARD_BACKEND_OK
              ? W_SEED_OWNER_GUARD_BACKEND_BOUNDARY
              : status,
          W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_PARENT, level, 0u, 0u, 0u,
          0u);
    }
    if (identity_equal(&next_identity, &directory_slot->identity)) {
      (void)close(next_parent);
      observations[level].root_terminal = true;
      context->level_count = level + 1u;
      context->candidate_count = candidate_count;
      return linux_result(W_SEED_OWNER_GUARD_BACKEND_OK,
                          W_SEED_OWNER_GUARD_BACKEND_PHASE_COMMIT, level, 0u,
                          context->active_generation, context->level_count,
                          candidate_count);
    }
    if (identity_seen(context, &next_identity, level + 1u)) {
      (void)close(next_parent);
      cleanup_session(context);
      return linux_result(W_SEED_OWNER_GUARD_BACKEND_BOUNDARY,
                          W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_PARENT, level,
                          0u, 0u, 0u, 0u);
    }
    if (level + 1u >= observation_capacity ||
        level + 1u >= W_SEED_OWNER_GUARD_MAX_LEVELS) {
      (void)close(next_parent);
      cleanup_session(context);
      return linux_result(W_SEED_OWNER_GUARD_BACKEND_CAPACITY,
                          W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_PARENT,
                          observation_capacity, observation_capacity + 1u,
                          0u, 0u, 0u);
    }
    level += 1u;
    size_t next_slot = SIZE_MAX;
    if (!push_slot(context, next_parent, &next_identity,
                   W_SEED_OWNER_GUARD_LINUX_SLOT_DIRECTORY, &next_slot)) {
      (void)close(next_parent);
      cleanup_session(context);
      return linux_result(W_SEED_OWNER_GUARD_BACKEND_FAULT,
                          W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_PARENT, level,
                          0u, 0u, 0u, 0u);
    }
    context->directory_slots[level] = next_slot;
  }
}

static w_seed_owner_guard_backend_result linux_revalidate(
    void *context_value, uint64_t generation,
    w_seed_owner_guard_observation *observations,
    size_t observation_capacity) {
  w_seed_owner_guard_linux_context *context = context_value;
  if (context == NULL || !context->initialized || !context->native_supported ||
      !context->session_live || generation == 0u ||
      generation != context->active_generation || observations == NULL ||
      observation_capacity < context->level_count)
    return linux_result(W_SEED_OWNER_GUARD_BACKEND_INVALID,
                        W_SEED_OWNER_GUARD_BACKEND_PHASE_VALIDATE,
                        W_SEED_OWNER_GUARD_NO_LEVEL, 0u, generation, 0u, 0u);
  w_seed_owner_guard_linux_identity base_now;
  if (!statx_identity(context->base_dir_fd, true, &base_now) ||
      !identity_equal(&base_now, &context->base_identity))
    return linux_result(W_SEED_OWNER_GUARD_BACKEND_MUTATED,
                        W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_DIRECTORY,
                        0u, 0u, generation, 0u, 0u);

  int parent_fd = -1;
  int source_fd = -1;
  w_seed_owner_guard_linux_identity parent_identity;
  w_seed_owner_guard_linux_identity source_identity;
  w_seed_owner_guard_backend_status status = open_source_binding(
      context, context->source_path, context->source_path_length, &parent_fd,
      &parent_identity, &source_fd, &source_identity);
  const w_seed_owner_guard_linux_slot *retained_source = NULL;
  const w_seed_owner_guard_linux_slot *retained_start = NULL;
  if (status != W_SEED_OWNER_GUARD_BACKEND_OK ||
      !retained_slot(context, context->source_slot,
                     W_SEED_OWNER_GUARD_LINUX_SLOT_SOURCE,
                     &retained_source) ||
      !retained_slot(context, context->directory_slots[0],
                     W_SEED_OWNER_GUARD_LINUX_SLOT_DIRECTORY,
                     &retained_start) ||
      !identity_equal(&source_identity, &retained_source->identity) ||
      !identity_equal(&parent_identity, &retained_start->identity)) {
    if (source_fd >= 0) (void)close(source_fd);
    if (parent_fd >= 0) (void)close(parent_fd);
    return linux_result(
        status == W_SEED_OWNER_GUARD_BACKEND_OK
            ? W_SEED_OWNER_GUARD_BACKEND_MUTATED
            : status,
        W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_DIRECTORY, 0u, 0u,
        generation, 0u, 0u);
  }
  (void)close(source_fd);
  (void)close(parent_fd);

  size_t candidate_count = 0u;
  for (size_t level = 0u; level < context->level_count; level += 1u) {
    const w_seed_owner_guard_linux_slot *directory = NULL;
    if (!retained_slot(context, context->directory_slots[level],
                       W_SEED_OWNER_GUARD_LINUX_SLOT_DIRECTORY, &directory))
      return linux_result(W_SEED_OWNER_GUARD_BACKEND_FAULT,
                          W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_DIRECTORY,
                          level, 0u, generation, 0u, 0u);
    w_seed_owner_guard_linux_identity directory_now;
    if (!statx_identity(directory->fd, true, &directory_now) ||
        !identity_equal(&directory_now, &directory->identity))
      return linux_result(W_SEED_OWNER_GUARD_BACKEND_MUTATED,
                          W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_DIRECTORY,
                          level, 0u, generation, 0u, 0u);
    observations[level] = (w_seed_owner_guard_observation){
        level, W_SEED_OWNER_GUARD_NO_CANDIDATE, false};

    int candidate_fd = -1;
    bool absent = false;
    w_seed_owner_guard_linux_identity candidate_identity;
    status = open_candidate(directory->fd, &candidate_fd,
                            &candidate_identity, &absent);
    const size_t expected_candidate = context->candidate_slots[level];
    if (status != W_SEED_OWNER_GUARD_BACKEND_OK) {
      return linux_result(status,
                          W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_CANDIDATE,
                          level, 0u, generation, 0u, 0u);
    }
    if (expected_candidate == SIZE_MAX) {
      if (!absent) {
        (void)close(candidate_fd);
        return linux_result(
            W_SEED_OWNER_GUARD_BACKEND_MUTATED,
            W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_CANDIDATE, level, 0u,
            generation, 0u, 0u);
      }
    } else {
      const w_seed_owner_guard_linux_slot *retained_candidate = NULL;
      if (absent || !retained_slot(
                        context, expected_candidate,
                        W_SEED_OWNER_GUARD_LINUX_SLOT_CANDIDATE,
                        &retained_candidate) ||
          !identity_equal(&candidate_identity,
                          &retained_candidate->identity)) {
        if (candidate_fd >= 0) (void)close(candidate_fd);
        return linux_result(
            W_SEED_OWNER_GUARD_BACKEND_MUTATED,
            W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_CANDIDATE, level, 0u,
            generation, 0u, 0u);
      }
      (void)close(candidate_fd);
      observations[level].candidate_index = candidate_count;
      candidate_count += 1u;
    }

    int next_parent = -1;
    w_seed_owner_guard_linux_identity next_identity;
    status = open_parent(directory->fd, &next_parent, &next_identity);
    if (status != W_SEED_OWNER_GUARD_BACKEND_OK) {
      return linux_result(status,
                          W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_PARENT,
                          level, 0u, generation, 0u, 0u);
    }
    bool parent_ok = false;
    if (level + 1u == context->level_count) {
      parent_ok = identity_equal(&next_identity, &directory->identity);
      observations[level].root_terminal = parent_ok;
    } else {
      const w_seed_owner_guard_linux_slot *retained_parent = NULL;
      parent_ok = retained_slot(
                      context, context->directory_slots[level + 1u],
                      W_SEED_OWNER_GUARD_LINUX_SLOT_DIRECTORY,
                      &retained_parent) &&
                  identity_equal(&next_identity, &retained_parent->identity);
    }
    (void)close(next_parent);
    if (!parent_ok || next_identity.mount_id != directory->identity.mount_id)
      return linux_result(W_SEED_OWNER_GUARD_BACKEND_MUTATED,
                          W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_PARENT,
                          level, 0u, generation, 0u, 0u);
  }
  if (candidate_count != context->candidate_count)
    return linux_result(W_SEED_OWNER_GUARD_BACKEND_MUTATED,
                        W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_CANDIDATE,
                        W_SEED_OWNER_GUARD_NO_LEVEL, 0u, generation, 0u, 0u);
  return linux_result(W_SEED_OWNER_GUARD_BACKEND_OK,
                      W_SEED_OWNER_GUARD_BACKEND_PHASE_COMMIT,
                      context->level_count - 1u, 0u, generation,
                      context->level_count, candidate_count);
}

static void linux_abort_begin(void *context_value) {
  cleanup_session((w_seed_owner_guard_linux_context *)context_value);
}

static void linux_destroy(void *context_value, uint64_t generation) {
  w_seed_owner_guard_linux_context *context = context_value;
  if (context == NULL || !context->session_live || generation == 0u ||
      generation != context->active_generation)
    return;
  cleanup_session(context);
}

static bool native_probe(int base_dir_fd) {
#if W_SEED_OWNER_GUARD_LINUX_NATIVE
  const int down = openat2_component(base_dir_fd, ".", O_PATH | O_DIRECTORY,
                                     true);
  if (down < 0) return false;
  (void)close(down);
  const int parent = openat2_component(base_dir_fd, "..",
                                       O_PATH | O_DIRECTORY, false);
  if (parent < 0) return false;
  (void)close(parent);
  return true;
#else
  (void)base_dir_fd;
  return false;
#endif
}

bool w_seed_owner_guard_linux_init(
    w_seed_owner_guard_linux_context *context, int base_dir_fd) {
  if (context == NULL) return false;
  (void)memset(context, 0, sizeof(*context));
  context->base_dir_fd = base_dir_fd;
  context->next_generation = 1u;
  context->source_slot = SIZE_MAX;
  for (size_t index = 0u; index < W_SEED_OWNER_GUARD_MAX_LEVELS;
       index += 1u) {
    context->directory_slots[index] = SIZE_MAX;
    context->candidate_slots[index] = SIZE_MAX;
  }
  for (size_t index = 0u; index < W_SEED_OWNER_GUARD_LINUX_MAX_HANDLES;
       index += 1u)
    clear_slot(&context->slots[index]);
  if (base_dir_fd < 0 || base_dir_fd == AT_FDCWD ||
      !statx_identity(base_dir_fd, true, &context->base_identity))
    return false;
  context->initialized = true;
  context->native_supported = native_probe(base_dir_fd);
  return true;
}

#else

static w_seed_owner_guard_backend_result linux_begin(
    void *context, w_seed_byte_view source_path,
    w_seed_owner_guard_observation *observations,
    size_t observation_capacity) {
  (void)context;
  (void)source_path;
  (void)observations;
  (void)observation_capacity;
  return linux_result(W_SEED_OWNER_GUARD_BACKEND_UNSUPPORTED,
                      W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_START,
                      W_SEED_OWNER_GUARD_NO_LEVEL, 0u, 0u, 0u, 0u);
}

static w_seed_owner_guard_backend_result linux_revalidate(
    void *context, uint64_t generation,
    w_seed_owner_guard_observation *observations,
    size_t observation_capacity) {
  (void)context;
  (void)observations;
  (void)observation_capacity;
  return linux_result(W_SEED_OWNER_GUARD_BACKEND_UNSUPPORTED,
                      W_SEED_OWNER_GUARD_BACKEND_PHASE_VALIDATE,
                      W_SEED_OWNER_GUARD_NO_LEVEL, 0u, generation, 0u, 0u);
}

static void linux_abort_begin(void *context) { (void)context; }

static void linux_destroy(void *context, uint64_t generation) {
  (void)context;
  (void)generation;
}

bool w_seed_owner_guard_linux_init(
    w_seed_owner_guard_linux_context *context, int base_dir_fd) {
  if (context == NULL) return false;
  (void)memset(context, 0, sizeof(*context));
  context->base_dir_fd = base_dir_fd;
  context->next_generation = 1u;
  context->initialized = true;
  context->native_supported = false;
  return true;
}

#endif

bool w_seed_owner_guard_linux_backend(
    w_seed_owner_guard_linux_context *context,
    w_seed_owner_guard_backend *backend) {
  if (context == NULL || backend == NULL || !context->initialized)
    return false;
  *backend = (w_seed_owner_guard_backend){
      .context = context,
      .begin = linux_begin,
      .revalidate = linux_revalidate,
      .abort_begin = linux_abort_begin,
      .destroy = linux_destroy,
  };
  return true;
}
