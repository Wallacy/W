#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif

#include "w_seed_manifest_linux.h"

#include "w_seed_sha256.h"

#include <limits.h>
#include <string.h>

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
#define W_SEED_MANIFEST_LINUX_NATIVE 1
#else
#define W_SEED_MANIFEST_LINUX_NATIVE 0
#endif

#endif

#if defined(__linux__)

enum {
  W_SEED_MANIFEST_LINUX_READ_CHUNK = 4096u,
  W_SEED_MANIFEST_LINUX_CONTEXT_BINDING_CAPACITY = 128u * 1024u,
  W_SEED_MANIFEST_LINUX_CANDIDATE_BINDING_CAPACITY = 1024u,
};

typedef struct {
  uint8_t *bytes;
  size_t capacity;
  size_t length;
} manifest_wire_writer;

static bool wire_append(manifest_wire_writer *writer, const void *bytes,
                        size_t length) {
  if (writer == NULL || writer->length > writer->capacity ||
      length > writer->capacity - writer->length ||
      (length != 0u && bytes == NULL))
    return false;
  if (length != 0u)
    (void)memcpy(writer->bytes + writer->length, bytes, length);
  writer->length += length;
  return true;
}

static bool wire_u32(manifest_wire_writer *writer, size_t value) {
  if (value > (size_t)UINT32_MAX) return false;
  const uint32_t converted = (uint32_t)value;
  const uint8_t encoded[4] = {
      (uint8_t)(converted >> 24), (uint8_t)(converted >> 16),
      (uint8_t)(converted >> 8), (uint8_t)converted};
  return wire_append(writer, encoded, sizeof(encoded));
}

static bool wire_u64(manifest_wire_writer *writer, uint64_t value) {
  uint8_t encoded[8];
  for (size_t index = 0u; index < sizeof(encoded); index += 1u)
    encoded[index] = (uint8_t)(value >> (56u - index * 8u));
  return wire_append(writer, encoded, sizeof(encoded));
}

static bool wire_open_frame(manifest_wire_writer *writer, const char *tag,
                            size_t *payload_start) {
  if (writer == NULL || tag == NULL || payload_start == NULL) return false;
  const size_t tag_length = strlen(tag);
  if (!wire_u32(writer, tag_length) ||
      !wire_append(writer, tag, tag_length))
    return false;
  const size_t length_offset = writer->length;
  if (!wire_u64(writer, 0u)) return false;
  *payload_start = writer->length;
  return length_offset + sizeof(uint64_t) == writer->length;
}

static bool wire_close_frame(manifest_wire_writer *writer,
                             size_t payload_start) {
  if (writer == NULL || payload_start < sizeof(uint64_t) ||
      payload_start > writer->length ||
      payload_start - sizeof(uint64_t) >= writer->capacity)
    return false;
  const size_t payload_length = writer->length - payload_start;
  const uint64_t encoded_length = (uint64_t)payload_length;
  uint8_t encoded[8];
  for (size_t index = 0u; index < sizeof(encoded); index += 1u)
    encoded[index] =
        (uint8_t)(encoded_length >> (56u - index * 8u));
  (void)memcpy(writer->bytes + payload_start - sizeof(encoded), encoded,
               sizeof(encoded));
  return true;
}

static bool wire_frame_bytes(manifest_wire_writer *writer, const char *tag,
                             const void *bytes, size_t length) {
  size_t payload_start = 0u;
  return wire_open_frame(writer, tag, &payload_start) &&
         wire_append(writer, bytes, length) &&
         wire_close_frame(writer, payload_start);
}

static bool wire_frame_u8(manifest_wire_writer *writer, const char *tag,
                          uint8_t value) {
  return wire_frame_bytes(writer, tag, &value, sizeof(value));
}

static bool wire_frame_u32(manifest_wire_writer *writer, const char *tag,
                           size_t value) {
  if (value > (size_t)UINT32_MAX) return false;
  const uint32_t converted = (uint32_t)value;
  const uint8_t encoded[4] = {
      (uint8_t)(converted >> 24), (uint8_t)(converted >> 16),
      (uint8_t)(converted >> 8), (uint8_t)converted};
  return wire_frame_bytes(writer, tag, encoded, sizeof(encoded));
}

static bool wire_frame_u64(manifest_wire_writer *writer, const char *tag,
                           uint64_t value) {
  uint8_t encoded[8];
  for (size_t index = 0u; index < sizeof(encoded); index += 1u)
    encoded[index] = (uint8_t)(value >> (56u - index * 8u));
  return wire_frame_bytes(writer, tag, encoded, sizeof(encoded));
}

static bool wire_identity(manifest_wire_writer *writer,
                          const w_seed_owner_guard_linux_identity *identity) {
  size_t payload_start = 0u;
  return identity != NULL && wire_open_frame(writer, "linux-identity",
                                              &payload_start) &&
         wire_frame_u64(writer, "mount-id-unique", identity->mount_id) &&
         wire_frame_u64(writer, "device-major", identity->device_major) &&
         wire_frame_u64(writer, "device-minor", identity->device_minor) &&
         wire_frame_u64(writer, "inode", identity->inode) &&
         wire_close_frame(writer, payload_start);
}

static bool digest_tagged(const char *tag, const uint8_t *payload,
                          size_t payload_length,
                          uint8_t digest[W_SEED_MANIFEST_DIGEST_BYTES]) {
  if (tag == NULL || digest == NULL ||
      (payload_length != 0u && payload == NULL) ||
      strlen(tag) > (size_t)UINT32_MAX)
    return false;
  const size_t tag_length = strlen(tag);
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  const uint8_t tag_size[4] = {
      (uint8_t)((uint32_t)tag_length >> 24),
      (uint8_t)((uint32_t)tag_length >> 16),
      (uint8_t)((uint32_t)tag_length >> 8), (uint8_t)tag_length};
  uint8_t payload_size[8];
  const uint64_t converted_length = (uint64_t)payload_length;
  for (size_t index = 0u; index < sizeof(payload_size); index += 1u)
    payload_size[index] =
        (uint8_t)(converted_length >> (56u - index * 8u));
  w_seed_sha256_update(&state, tag_size, sizeof(tag_size));
  w_seed_sha256_update(&state, (const uint8_t *)tag, tag_length);
  w_seed_sha256_update(&state, payload_size, sizeof(payload_size));
  w_seed_sha256_update(&state, payload, payload_length);
  w_seed_sha256_final(&state, digest);
  return true;
}

static bool digest_tagged_prefix(const char *tag, size_t payload_length,
                                 w_seed_sha256_state *state) {
  if (tag == NULL || state == NULL || strlen(tag) > (size_t)UINT32_MAX)
    return false;
  const size_t tag_length = strlen(tag);
  const uint8_t tag_size[4] = {
      (uint8_t)((uint32_t)tag_length >> 24),
      (uint8_t)((uint32_t)tag_length >> 16),
      (uint8_t)((uint32_t)tag_length >> 8), (uint8_t)tag_length};
  uint8_t payload_size[8];
  const uint64_t converted_length = (uint64_t)payload_length;
  for (size_t index = 0u; index < sizeof(payload_size); index += 1u)
    payload_size[index] =
        (uint8_t)(converted_length >> (56u - index * 8u));
  w_seed_sha256_init(state);
  w_seed_sha256_update(state, tag_size, sizeof(tag_size));
  w_seed_sha256_update(state, (const uint8_t *)tag, tag_length);
  w_seed_sha256_update(state, payload_size, sizeof(payload_size));
  return true;
}

static bool linux_identity_valid(
    const w_seed_owner_guard_linux_identity *identity) {
  return identity != NULL && identity->mount_id != 0u &&
         identity->inode != 0u;
}

static bool linux_slot_valid(
    const w_seed_owner_guard_linux_context *context, size_t index,
    w_seed_owner_guard_linux_slot_kind kind,
    const w_seed_owner_guard_linux_slot **slot) {
  if (context == NULL || slot == NULL ||
      context->slot_count > W_SEED_OWNER_GUARD_LINUX_MAX_HANDLES ||
      index >= context->slot_count || index >= W_SEED_OWNER_GUARD_LINUX_MAX_HANDLES)
    return false;
  const w_seed_owner_guard_linux_slot *candidate = &context->slots[index];
  if (!candidate->used || candidate->fd < 0 || candidate->kind != kind ||
      !linux_identity_valid(&candidate->identity))
    return false;
  *slot = candidate;
  return true;
}

static bool linux_find_candidate(
    const w_seed_owner_guard_linux_context *context,
    w_seed_owner_guard_candidate_ref candidate, size_t *level,
    const w_seed_owner_guard_linux_slot **directory,
    const w_seed_owner_guard_linux_slot **candidate_slot) {
  if (context == NULL || level == NULL || directory == NULL ||
      candidate_slot == NULL || candidate.generation == 0u ||
      context->level_count == 0u ||
      context->level_count > W_SEED_OWNER_GUARD_MAX_LEVELS ||
      context->candidate_count == 0u ||
      context->candidate_count > W_SEED_OWNER_GUARD_MAX_LEVELS ||
      candidate.directory_ordinal >= context->level_count ||
      candidate.candidate_index >= context->candidate_count)
    return false;
  size_t dense_index = 0u;
  for (size_t current = 0u; current < context->level_count; current += 1u) {
    const w_seed_owner_guard_linux_slot *directory_value = NULL;
    if (!linux_slot_valid(context, context->directory_slots[current],
                          W_SEED_OWNER_GUARD_LINUX_SLOT_DIRECTORY,
                          &directory_value))
      return false;
    const size_t slot_index = context->candidate_slots[current];
    if (slot_index == SIZE_MAX) continue;
    const w_seed_owner_guard_linux_slot *candidate_value = NULL;
    if (!linux_slot_valid(context, slot_index,
                          W_SEED_OWNER_GUARD_LINUX_SLOT_CANDIDATE,
                          &candidate_value))
      return false;
    if (dense_index == candidate.candidate_index &&
        current == candidate.directory_ordinal) {
      *level = current;
      *directory = directory_value;
      *candidate_slot = candidate_value;
      return true;
    }
    dense_index += 1u;
  }
  return false;
}

static bool linux_context_payload(
    const w_seed_owner_guard_linux_context *context, uint64_t generation,
    uint8_t payload[W_SEED_MANIFEST_LINUX_CONTEXT_BINDING_CAPACITY],
    size_t *payload_length) {
  if (context == NULL || payload == NULL || payload_length == NULL ||
      !context->initialized || !context->native_supported ||
      !context->session_live || context->active_generation != generation ||
      generation == 0u || context->level_count == 0u ||
      context->level_count > W_SEED_OWNER_GUARD_MAX_LEVELS ||
      context->candidate_count == 0u ||
      context->candidate_count > context->level_count ||
      context->slot_count > W_SEED_OWNER_GUARD_LINUX_MAX_HANDLES ||
      !linux_identity_valid(&context->base_identity))
    return false;
  const w_seed_owner_guard_linux_slot *source_slot = NULL;
  if (!linux_slot_valid(context, context->source_slot,
                        W_SEED_OWNER_GUARD_LINUX_SLOT_SOURCE, &source_slot))
    return false;
  manifest_wire_writer writer = {payload,
                                 W_SEED_MANIFEST_LINUX_CONTEXT_BINDING_CAPACITY,
                                 0u};
  if (!wire_frame_bytes(&writer, "schema", W_SEED_MANIFEST_SCHEMA_VERSION,
                        sizeof(W_SEED_MANIFEST_SCHEMA_VERSION) - 1u) ||
      !wire_frame_u64(&writer, "generation", generation))
    return false;
  size_t identity_frame = 0u;
  if (!wire_open_frame(&writer, "base-identity", &identity_frame) ||
      !wire_identity(&writer, &context->base_identity) ||
      !wire_close_frame(&writer, identity_frame) ||
      !wire_open_frame(&writer, "source-identity", &identity_frame) ||
      !wire_identity(&writer, &source_slot->identity) ||
      !wire_close_frame(&writer, identity_frame))
    return false;
  size_t levels_frame = 0u;
  if (!wire_open_frame(&writer, "levels", &levels_frame) ||
      !wire_u32(&writer, context->level_count))
    return false;
  size_t dense_index = 0u;
  for (size_t level = 0u; level < context->level_count; level += 1u) {
    const w_seed_owner_guard_linux_slot *directory_slot = NULL;
    if (!linux_slot_valid(context, context->directory_slots[level],
                          W_SEED_OWNER_GUARD_LINUX_SLOT_DIRECTORY,
                          &directory_slot))
      return false;
    size_t item_frame = 0u;
    if (!wire_open_frame(&writer, "item", &item_frame)) return false;
    size_t level_frame = 0u;
    if (!wire_open_frame(&writer, "level", &level_frame) ||
        !wire_frame_u32(&writer, "directory-ordinal", level))
      return false;
    size_t directory_identity_frame = 0u;
    if (!wire_open_frame(&writer, "directory-identity",
                         &directory_identity_frame) ||
        !wire_identity(&writer, &directory_slot->identity) ||
        !wire_close_frame(&writer, directory_identity_frame))
      return false;
    const size_t candidate_slot_index = context->candidate_slots[level];
    if (candidate_slot_index == SIZE_MAX) {
      if (!wire_frame_u8(&writer, "candidate-present", 0u)) return false;
    } else {
      const w_seed_owner_guard_linux_slot *candidate_slot = NULL;
      if (!linux_slot_valid(context, candidate_slot_index,
                            W_SEED_OWNER_GUARD_LINUX_SLOT_CANDIDATE,
                            &candidate_slot) ||
          dense_index > (size_t)UINT32_MAX ||
          !wire_frame_u8(&writer, "candidate-present", 1u) ||
          !wire_frame_u32(&writer, "candidate-index", dense_index))
        return false;
      size_t candidate_identity_frame = 0u;
      if (!wire_open_frame(&writer, "candidate-identity",
                           &candidate_identity_frame) ||
          !wire_identity(&writer, &candidate_slot->identity) ||
          !wire_close_frame(&writer, candidate_identity_frame))
        return false;
      dense_index += 1u;
    }
    if (!wire_close_frame(&writer, level_frame) ||
        !wire_close_frame(&writer, item_frame))
      return false;
  }
  if (dense_index != context->candidate_count ||
      !wire_close_frame(&writer, levels_frame))
    return false;
  *payload_length = writer.length;
  return true;
}

static bool linux_bindings(
    const w_seed_owner_guard_linux_context *context, uint64_t generation,
    w_seed_owner_guard_candidate_ref candidate,
    uint8_t context_binding[W_SEED_MANIFEST_DIGEST_BYTES],
    uint8_t candidate_binding[W_SEED_MANIFEST_DIGEST_BYTES]) {
  uint8_t context_payload[W_SEED_MANIFEST_LINUX_CONTEXT_BINDING_CAPACITY];
  size_t context_payload_length = 0u;
  if (!linux_context_payload(context, generation, context_payload,
                             &context_payload_length) ||
      !digest_tagged(W_SEED_MANIFEST_CONTEXT_TAG, context_payload,
                      context_payload_length, context_binding))
    return false;
  size_t level = 0u;
  const w_seed_owner_guard_linux_slot *directory = NULL;
  const w_seed_owner_guard_linux_slot *candidate_slot = NULL;
  if (!linux_find_candidate(context, candidate, &level, &directory,
                            &candidate_slot) ||
      level != candidate.directory_ordinal ||
      !linux_identity_valid(&directory->identity) ||
      !linux_identity_valid(&candidate_slot->identity))
    return false;
  uint8_t candidate_payload[W_SEED_MANIFEST_LINUX_CANDIDATE_BINDING_CAPACITY];
  manifest_wire_writer writer = {candidate_payload,
                                 sizeof(candidate_payload), 0u};
  size_t ref_frame = 0u;
  if (!wire_frame_bytes(&writer, "schema", W_SEED_MANIFEST_SCHEMA_VERSION,
                        sizeof(W_SEED_MANIFEST_SCHEMA_VERSION) - 1u) ||
      !wire_frame_bytes(&writer, "literal", "build.w", sizeof("build.w") - 1u) ||
      !wire_frame_bytes(&writer, "context-binding", context_binding,
                        W_SEED_MANIFEST_DIGEST_BYTES) ||
      !wire_open_frame(&writer, "candidate-ref", &ref_frame) ||
      !wire_u64(&writer, candidate.generation) ||
      !wire_u32(&writer, candidate.directory_ordinal) ||
      !wire_u32(&writer, candidate.candidate_index) ||
      !wire_close_frame(&writer, ref_frame))
    return false;
  size_t identity_frame = 0u;
  if (!wire_open_frame(&writer, "directory-identity", &identity_frame) ||
      !wire_identity(&writer, &directory->identity) ||
      !wire_close_frame(&writer, identity_frame) ||
      !wire_open_frame(&writer, "candidate-identity", &identity_frame) ||
      !wire_identity(&writer, &candidate_slot->identity) ||
      !wire_close_frame(&writer, identity_frame) ||
      !digest_tagged(W_SEED_MANIFEST_CANDIDATE_TAG, candidate_payload,
                     writer.length, candidate_binding))
    return false;
  return true;
}

#if defined(__linux__)

static bool linux_identity_equal(
    const w_seed_owner_guard_linux_identity *left,
    const w_seed_owner_guard_linux_identity *right) {
  return left != NULL && right != NULL &&
         left->mount_id == right->mount_id &&
         left->device_major == right->device_major &&
         left->device_minor == right->device_minor &&
         left->inode == right->inode;
}

static int linux_openat2(int directory_fd, const char *path, int flags) {
#if W_SEED_MANIFEST_LINUX_NATIVE
  if (directory_fd < 0 || path == NULL) {
    errno = EINVAL;
    return -1;
  }
  struct open_how how;
  (void)memset(&how, 0, sizeof(how));
  how.flags = (uint64_t)(unsigned int)(flags | O_CLOEXEC);
  how.resolve = (uint64_t)(RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS |
                           RESOLVE_NO_XDEV | RESOLVE_BENEATH);
  const long value = syscall(SYS_openat2, directory_fd, path, &how,
                             sizeof(how));
  if (value < 0L || value > (long)INT_MAX) return -1;
  return (int)value;
#else
  (void)directory_fd;
  (void)path;
  (void)flags;
  errno = ENOSYS;
  return -1;
#endif
}

static bool linux_statx_identity(
    int fd, w_seed_owner_guard_linux_identity *identity) {
#if W_SEED_MANIFEST_LINUX_NATIVE
  if (fd < 0 || identity == NULL) return false;
  struct statx value;
  (void)memset(&value, 0, sizeof(value));
  const unsigned int mask = STATX_TYPE | STATX_INO | STATX_MNT_ID_UNIQUE;
  const long status = syscall(SYS_statx, fd, "",
                              AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW, mask,
                              &value);
  if (status != 0L || (value.stx_mask & mask) != mask ||
      ((unsigned int)value.stx_mode & S_IFMT) != S_IFREG)
    return false;
  identity->mount_id = value.stx_mnt_id;
  identity->device_major = value.stx_dev_major;
  identity->device_minor = value.stx_dev_minor;
  identity->inode = value.stx_ino;
  return linux_identity_valid(identity);
#else
  (void)fd;
  (void)identity;
  return false;
#endif
}

static bool linux_file_size(int fd, size_t *size) {
  if (fd < 0 || size == NULL) return false;
  struct stat value;
  if (fstat(fd, &value) != 0 || !S_ISREG(value.st_mode) ||
      value.st_size < (off_t)0)
    return false;
  const uintmax_t converted = (uintmax_t)value.st_size;
  if (converted > (uintmax_t)SIZE_MAX) return false;
  *size = (size_t)converted;
  return true;
}

static w_seed_manifest_backend_status linux_errno_status(int error_code) {
  if (error_code == ELOOP) return W_SEED_MANIFEST_BACKEND_REPARSE;
  if (error_code == EXDEV) return W_SEED_MANIFEST_BACKEND_BOUNDARY;
  if (error_code == ENOENT) return W_SEED_MANIFEST_BACKEND_MUTATED;
  if (error_code == ENOSYS) return W_SEED_MANIFEST_BACKEND_UNSUPPORTED;
  if (error_code == EINVAL || error_code == EBADF)
    return W_SEED_MANIFEST_BACKEND_INVALID;
  return W_SEED_MANIFEST_BACKEND_IO;
}

#endif

#endif

static w_seed_manifest_backend_result linux_backend_result(
    w_seed_manifest_backend_status status,
    w_seed_manifest_backend_phase phase, uint64_t generation,
    w_seed_owner_guard_candidate_ref candidate) {
  w_seed_manifest_backend_result result;
  (void)memset(&result, 0, sizeof(result));
  result.status = status;
  result.phase = phase;
  result.generation = generation;
  result.candidate = candidate;
  return result;
}

#if !defined(__linux__)

static w_seed_manifest_backend_result linux_unsupported(
    const void *context, uint64_t generation,
    w_seed_owner_guard_candidate_ref candidate, uint8_t *bytes,
    size_t byte_capacity, size_t byte_limit) {
  (void)context;
  (void)bytes;
  (void)byte_capacity;
  (void)byte_limit;
  return linux_backend_result(W_SEED_MANIFEST_BACKEND_UNSUPPORTED,
                              W_SEED_MANIFEST_BACKEND_PHASE_OPEN_CANDIDATE,
                              generation, candidate);
}

#endif

#if defined(__linux__)

static w_seed_manifest_backend_result linux_read_candidate(
    const void *context_value, uint64_t generation,
    w_seed_owner_guard_candidate_ref candidate, uint8_t *bytes,
    size_t byte_capacity, size_t byte_limit) {
  const w_seed_owner_guard_linux_context *context = context_value;
  w_seed_manifest_backend_result result = linux_backend_result(
      W_SEED_MANIFEST_BACKEND_INVALID,
      W_SEED_MANIFEST_BACKEND_PHASE_VALIDATE, generation, candidate);
  if (context == NULL || !context->initialized || !context->native_supported ||
      !context->session_live || generation == 0u ||
      context->active_generation != generation ||
      (byte_capacity != 0u && bytes == NULL) || byte_limit == SIZE_MAX ||
      candidate.generation != generation)
    return context != NULL && context->initialized &&
                   context->native_supported
               ? result
               : linux_backend_result(
                     W_SEED_MANIFEST_BACKEND_UNSUPPORTED,
                     W_SEED_MANIFEST_BACKEND_PHASE_OPEN_CANDIDATE, generation,
                     candidate);

  size_t level = 0u;
  const w_seed_owner_guard_linux_slot *directory = NULL;
  const w_seed_owner_guard_linux_slot *candidate_slot = NULL;
  if (!linux_find_candidate(context, candidate, &level, &directory,
                            &candidate_slot))
    return result;
  if (directory->identity.mount_id != candidate_slot->identity.mount_id)
    return linux_backend_result(W_SEED_MANIFEST_BACKEND_BOUNDARY,
                                W_SEED_MANIFEST_BACKEND_PHASE_VALIDATE,
                                generation, candidate);

  uint8_t context_binding[W_SEED_MANIFEST_DIGEST_BYTES];
  uint8_t candidate_binding[W_SEED_MANIFEST_DIGEST_BYTES];
  if (!linux_bindings(context, generation, candidate, context_binding,
                      candidate_binding))
    return result;

  const int file = linux_openat2(directory->fd, "build.w",
                                 O_RDONLY | O_NONBLOCK);
  if (file < 0)
    return linux_backend_result(linux_errno_status(errno),
                                W_SEED_MANIFEST_BACKEND_PHASE_OPEN_CANDIDATE,
                                generation, candidate);
  w_seed_owner_guard_linux_identity opened_identity;
  if (!linux_statx_identity(file, &opened_identity)) {
    (void)close(file);
    return linux_backend_result(W_SEED_MANIFEST_BACKEND_MUTATED,
                                W_SEED_MANIFEST_BACKEND_PHASE_VERIFY_IDENTITY,
                                generation, candidate);
  }
  if (!linux_identity_equal(&opened_identity, &candidate_slot->identity)) {
    (void)close(file);
    return linux_backend_result(W_SEED_MANIFEST_BACKEND_MUTATED,
                                W_SEED_MANIFEST_BACKEND_PHASE_VERIFY_IDENTITY,
                                generation, candidate);
  }

  size_t initial_size = 0u;
  if (!linux_file_size(file, &initial_size)) {
    (void)close(file);
    return linux_backend_result(W_SEED_MANIFEST_BACKEND_IO,
                                W_SEED_MANIFEST_BACKEND_PHASE_READ, generation,
                                candidate);
  }
  const size_t limit_plus_one = byte_limit + 1u;
  uint8_t chunk[W_SEED_MANIFEST_LINUX_READ_CHUNK];
  size_t total = 0u;
  bool exceeded = false;
  const bool hash_source = initial_size <= byte_limit;
  w_seed_sha256_state hash;
  if (hash_source &&
      !digest_tagged_prefix(W_SEED_MANIFEST_DOCUMENT_SOURCE_TAG, initial_size,
                            &hash)) {
    (void)close(file);
    return linux_backend_result(W_SEED_MANIFEST_BACKEND_FAULT,
                                W_SEED_MANIFEST_BACKEND_PHASE_VALIDATE,
                                generation, candidate);
  }
  while (total < limit_plus_one) {
    const size_t remaining = limit_plus_one - total;
    const size_t request = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
    const ssize_t read_count = read(file, chunk, request);
    if (read_count < (ssize_t)0) {
      if (errno == EINTR) continue;
      (void)close(file);
      return linux_backend_result(
          W_SEED_MANIFEST_BACKEND_IO, W_SEED_MANIFEST_BACKEND_PHASE_READ,
          generation, candidate);
    }
    if (read_count == (ssize_t)0) break;
    const size_t amount = (size_t)read_count;
    if (amount > request || total > SIZE_MAX - amount) {
      (void)close(file);
      return linux_backend_result(
          W_SEED_MANIFEST_BACKEND_FAULT, W_SEED_MANIFEST_BACKEND_PHASE_READ,
          generation, candidate);
    }
    if (bytes != NULL && total < byte_capacity) {
      const size_t within_limit =
          total < byte_limit ? byte_limit - total : 0u;
      const size_t writable_capacity =
          (amount < byte_capacity - total) ? amount : byte_capacity - total;
      const size_t writable =
          (writable_capacity < within_limit) ? writable_capacity : within_limit;
      if (writable != 0u) (void)memcpy(bytes + total, chunk, writable);
    }
    if (hash_source && !exceeded) {
      if (amount > byte_limit - total) {
        exceeded = true;
      } else {
        w_seed_sha256_update(&hash, chunk, amount);
      }
    }
    total += amount;
    if (total > byte_limit) {
      exceeded = true;
      break;
    }
  }

  size_t final_size = 0u;
  w_seed_owner_guard_linux_identity final_identity;
  const bool final_identity_valid =
      linux_statx_identity(file, &final_identity);
  const bool final_size_valid = linux_file_size(file, &final_size);
  const int close_status = close(file);
  if (!final_identity_valid || !final_size_valid || close_status != 0)
    return linux_backend_result(
        close_status != 0 && final_identity_valid && final_size_valid
            ? W_SEED_MANIFEST_BACKEND_IO
            : W_SEED_MANIFEST_BACKEND_MUTATED,
        close_status != 0 && final_identity_valid && final_size_valid
            ? W_SEED_MANIFEST_BACKEND_PHASE_CLOSE
            : W_SEED_MANIFEST_BACKEND_PHASE_VERIFY_EOF,
        generation, candidate);
  if (!linux_identity_equal(&final_identity, &candidate_slot->identity))
    return linux_backend_result(W_SEED_MANIFEST_BACKEND_MUTATED,
                                W_SEED_MANIFEST_BACKEND_PHASE_VERIFY_EOF,
                                generation, candidate);
  if (total > byte_limit)
    return (w_seed_manifest_backend_result){
        W_SEED_MANIFEST_BACKEND_LIMIT,
        W_SEED_MANIFEST_BACKEND_PHASE_VERIFY_EOF,
        generation,
        candidate,
        0u,
        byte_limit + 1u,
        {0u},
        {0u},
        {0u}};
  if (final_size != total || (initial_size > byte_limit && final_size != initial_size))
    return linux_backend_result(W_SEED_MANIFEST_BACKEND_MUTATED,
                                W_SEED_MANIFEST_BACKEND_PHASE_VERIFY_EOF,
                                generation, candidate);
  if (total > byte_capacity)
    return (w_seed_manifest_backend_result){
        W_SEED_MANIFEST_BACKEND_CAPACITY,
        W_SEED_MANIFEST_BACKEND_PHASE_VERIFY_EOF,
        generation,
        candidate,
        0u,
        total,
        {0u},
        {0u},
        {0u}};
  if (!hash_source)
    return linux_backend_result(W_SEED_MANIFEST_BACKEND_MUTATED,
                                W_SEED_MANIFEST_BACKEND_PHASE_VERIFY_EOF,
                                generation, candidate);
  uint8_t source_digest[W_SEED_MANIFEST_DIGEST_BYTES];
  w_seed_sha256_final(&hash, source_digest);
  result = (w_seed_manifest_backend_result){
      W_SEED_MANIFEST_BACKEND_OK,
      W_SEED_MANIFEST_BACKEND_PHASE_CLOSE,
      generation,
      candidate,
      total,
      total,
      {0u},
      {0u},
      {0u}};
  (void)memcpy(result.source_digest, source_digest,
               sizeof(result.source_digest));
  (void)memcpy(result.context_binding, context_binding,
               sizeof(result.context_binding));
  (void)memcpy(result.candidate_binding, candidate_binding,
               sizeof(result.candidate_binding));
  return result;
}

#endif

bool w_seed_manifest_linux_backend(
    const w_seed_owner_guard *guard,
    const w_seed_owner_guard_linux_context *context,
    w_seed_manifest_backend *backend) {
  if (guard == NULL || context == NULL || backend == NULL) return false;
  *backend = (w_seed_manifest_backend){
      .owner = backend,
      .guard = guard,
      .context = context,
      .context_size = sizeof(*context),
      .generation = guard->generation,
#if defined(__linux__)
      .read_candidate = linux_read_candidate,
#else
      .read_candidate = linux_unsupported,
#endif
  };
  return true;
}
