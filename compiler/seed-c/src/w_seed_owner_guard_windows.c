#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0602
#endif

#include "w_seed_owner_guard_windows.h"

#include <limits.h>
#include <string.h>

static w_seed_owner_guard_backend_result windows_result(
    w_seed_owner_guard_backend_status status,
    w_seed_owner_guard_backend_phase phase, size_t level,
    size_t required_capacity, uint64_t generation, size_t level_count,
    size_t candidate_count) {
  const w_seed_owner_guard_backend_result result = {
      status, phase, level, required_capacity, generation, level_count,
      candidate_count};
  return result;
}

#if defined(_WIN32)

#include <fileapi.h>
#include <processthreadsapi.h>
#include <windows.h>
#include <winternl.h>

_Static_assert(sizeof(WCHAR) == sizeof(uint16_t),
               "Windows owner guard requires 16-bit WCHAR");

#define W_STATUS_SUCCESS ((NTSTATUS)0x00000000u)
#define W_STATUS_INVALID_PARAMETER ((NTSTATUS)0xC000000Du)
#define W_STATUS_NOT_IMPLEMENTED ((NTSTATUS)0xC0000002u)
#define W_STATUS_INVALID_DEVICE_REQUEST ((NTSTATUS)0xC0000010u)
#define W_STATUS_OBJECT_NAME_INVALID ((NTSTATUS)0xC0000033u)
#define W_STATUS_OBJECT_NAME_NOT_FOUND ((NTSTATUS)0xC0000034u)
#define W_STATUS_OBJECT_PATH_NOT_FOUND ((NTSTATUS)0xC000003Au)
#define W_STATUS_OBJECT_PATH_SYNTAX_BAD ((NTSTATUS)0xC000003Bu)
#define W_STATUS_NOT_SUPPORTED ((NTSTATUS)0xC00000BBu)
#define W_STATUS_NOT_A_DIRECTORY ((NTSTATUS)0xC0000103u)
#define W_STATUS_STOPPED_ON_SYMLINK ((NTSTATUS)0x8000002Du)
#define W_STATUS_IO_REPARSE_TAG_NOT_HANDLED ((NTSTATUS)0xC0000279u)
#define W_STATUS_DIRECTORY_IS_A_REPARSE_POINT ((NTSTATUS)0xC0000281u)
#define W_STATUS_REPARSE_POINT_ENCOUNTERED ((NTSTATUS)0xC000050Bu)

typedef struct {
  HANDLE handle;
  w_seed_owner_guard_windows_identity identity;
  bool directory;
} windows_entry;

static bool native_handle_valid(HANDLE handle) {
  return handle != NULL && handle != INVALID_HANDLE_VALUE;
}

static HANDLE native_from_value(uintptr_t value) {
  return (HANDLE)value;
}

static uintptr_t value_from_native(HANDLE handle) {
  return (uintptr_t)handle;
}

static bool identity_equal(const w_seed_owner_guard_windows_identity *left,
                           const w_seed_owner_guard_windows_identity *right) {
  return left != NULL && right != NULL &&
         left->volume_serial == right->volume_serial &&
         memcmp(left->file_id, right->file_id, sizeof(left->file_id)) == 0;
}

static bool nt_not_found(NTSTATUS status) {
  return status == W_STATUS_OBJECT_NAME_NOT_FOUND;
}

static w_seed_owner_guard_backend_status ntstatus_status(NTSTATUS status) {
  switch (status) {
    case W_STATUS_REPARSE_POINT_ENCOUNTERED:
    case W_STATUS_STOPPED_ON_SYMLINK:
    case W_STATUS_IO_REPARSE_TAG_NOT_HANDLED:
    case W_STATUS_DIRECTORY_IS_A_REPARSE_POINT:
      return W_SEED_OWNER_GUARD_BACKEND_REPARSE;
    case W_STATUS_NOT_IMPLEMENTED:
    case W_STATUS_INVALID_DEVICE_REQUEST:
    case W_STATUS_NOT_SUPPORTED:
      return W_SEED_OWNER_GUARD_BACKEND_UNSUPPORTED;
    case W_STATUS_INVALID_PARAMETER:
    case W_STATUS_OBJECT_NAME_INVALID:
    case W_STATUS_OBJECT_PATH_SYNTAX_BAD:
      return W_SEED_OWNER_GUARD_BACKEND_INVALID;
    default:
      return W_SEED_OWNER_GUARD_BACKEND_IO;
  }
}

static size_t wide_length(const WCHAR *text) {
  size_t length = 0u;
  if (text == NULL) return 0u;
  while (text[length] != (WCHAR)0) length += 1u;
  return length;
}

static bool initialize_unicode_string(const WCHAR *path,
                                      UNICODE_STRING *name) {
  if (path == NULL || name == NULL) return false;
  const size_t length = wide_length(path);
  if (length > (size_t)(UINT16_MAX / sizeof(WCHAR))) return false;
  name->Length = (USHORT)(length * sizeof(WCHAR));
  name->MaximumLength = name->Length;
  name->Buffer = (PWSTR)path;
  return true;
}

static NTSTATUS nt_open(HANDLE root, const WCHAR *name, ULONG type_options,
                        HANDLE *handle) {
  if (name == NULL || handle == NULL) return W_STATUS_INVALID_PARAMETER;
  *handle = NULL;
  UNICODE_STRING unicode_name;
  if (!initialize_unicode_string(name, &unicode_name))
    return W_STATUS_INVALID_PARAMETER;
  OBJECT_ATTRIBUTES attributes;
  InitializeObjectAttributes(&attributes, &unicode_name,
                             OBJ_CASE_INSENSITIVE | OBJ_DONT_REPARSE, root,
                             NULL);
  IO_STATUS_BLOCK io_status;
  (void)memset(&io_status, 0, sizeof(io_status));
  return NtCreateFile(
      handle, FILE_READ_ATTRIBUTES | SYNCHRONIZE, &attributes, &io_status,
      NULL, FILE_ATTRIBUTE_NORMAL,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN,
      FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_REPARSE_POINT | type_options,
      NULL, 0u);
}

static w_seed_owner_guard_backend_status query_entry(HANDLE handle,
                                                       windows_entry *entry) {
  if (!native_handle_valid(handle) || entry == NULL)
    return W_SEED_OWNER_GUARD_BACKEND_INVALID;
  if (GetFileType(handle) != FILE_TYPE_DISK)
    return W_SEED_OWNER_GUARD_BACKEND_UNSUPPORTED;
  FILE_ATTRIBUTE_TAG_INFO tag;
  (void)memset(&tag, 0, sizeof(tag));
  if (!GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tag,
                                    (DWORD)sizeof(tag)))
    return W_SEED_OWNER_GUARD_BACKEND_IO;
  if ((tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u)
    return W_SEED_OWNER_GUARD_BACKEND_REPARSE;
  FILE_STANDARD_INFO standard;
  (void)memset(&standard, 0, sizeof(standard));
  if (!GetFileInformationByHandleEx(handle, FileStandardInfo, &standard,
                                    (DWORD)sizeof(standard)))
    return W_SEED_OWNER_GUARD_BACKEND_IO;
  FILE_ID_INFO id;
  (void)memset(&id, 0, sizeof(id));
  if (!GetFileInformationByHandleEx(handle, FileIdInfo, &id,
                                    (DWORD)sizeof(id)))
    return W_SEED_OWNER_GUARD_BACKEND_UNSUPPORTED;
  entry->handle = handle;
  entry->identity.volume_serial = (uint64_t)id.VolumeSerialNumber;
  (void)memcpy(entry->identity.file_id, id.FileId.Identifier,
               sizeof(entry->identity.file_id));
  entry->directory = standard.Directory != FALSE;
  return entry->identity.volume_serial != 0u
             ? W_SEED_OWNER_GUARD_BACKEND_OK
             : W_SEED_OWNER_GUARD_BACKEND_UNSUPPORTED;
}

static bool local_disk_handle(HANDLE handle, uint32_t *query_status) {
  if (query_status == NULL) return false;
  *query_status = ERROR_INVALID_HANDLE;
  if (!native_handle_valid(handle) || GetFileType(handle) != FILE_TYPE_DISK)
    return false;
  FILE_REMOTE_PROTOCOL_INFO remote;
  (void)memset(&remote, 0, sizeof(remote));
  if (!GetFileInformationByHandleEx(handle, FileRemoteProtocolInfo, &remote,
                                    (DWORD)sizeof(remote))) {
    *query_status = (uint32_t)GetLastError();
    return false;
  }
  *query_status = ERROR_SUCCESS;
  return remote.Protocol == 0u;
}

static bool ascii_equal_ci(const WCHAR *text, size_t start, size_t end,
                           const char *literal) {
  size_t length = 0u;
  while (literal[length] != '\0') length += 1u;
  if (end - start != length) return false;
  for (size_t index = 0u; index < length; index += 1u) {
    WCHAR observed = text[start + index];
    WCHAR expected = (WCHAR)(unsigned char)literal[index];
    if (observed >= (WCHAR)L'A' && observed <= (WCHAR)L'Z')
      observed = (WCHAR)(observed + ((WCHAR)L'a' - (WCHAR)L'A'));
    if (expected >= (WCHAR)L'A' && expected <= (WCHAR)L'Z')
      expected = (WCHAR)(expected + ((WCHAR)L'a' - (WCHAR)L'A'));
    if (observed != expected) return false;
  }
  return true;
}

static bool reserved_device(const WCHAR *text, size_t start, size_t end) {
  size_t base_end = start;
  while (base_end < end && text[base_end] != (WCHAR)L'.') base_end += 1u;
  if (ascii_equal_ci(text, start, base_end, "con") ||
      ascii_equal_ci(text, start, base_end, "prn") ||
      ascii_equal_ci(text, start, base_end, "aux") ||
      ascii_equal_ci(text, start, base_end, "nul") ||
      ascii_equal_ci(text, start, base_end, "clock$"))
    return true;
  if (base_end - start != 4u) return false;
  const WCHAR digit = text[base_end - 1u];
  return digit >= (WCHAR)L'1' && digit <= (WCHAR)L'9' &&
         (ascii_equal_ci(text, start, base_end - 1u, "com") ||
          ascii_equal_ci(text, start, base_end - 1u, "lpt"));
}

static bool wide_component_valid(const WCHAR *text, size_t start,
                                 size_t end) {
  if (text == NULL || start >= end) return false;
  if ((end - start == 1u && text[start] == (WCHAR)L'.') ||
      (end - start == 2u && text[start] == (WCHAR)L'.' &&
       text[start + 1u] == (WCHAR)L'.'))
    return false;
  if (text[end - 1u] == (WCHAR)L'.' || text[end - 1u] == (WCHAR)L' ' ||
      reserved_device(text, start, end))
    return false;
  for (size_t index = start; index < end; index += 1u) {
    const WCHAR value = text[index];
    if (value < (WCHAR)0x20u || value == (WCHAR)L':' ||
        value == (WCHAR)L'\\' || value == (WCHAR)L'/' ||
        value == (WCHAR)L'<' || value == (WCHAR)L'>' ||
        value == (WCHAR)L'"' || value == (WCHAR)L'|' ||
        value == (WCHAR)L'?' || value == (WCHAR)L'*')
      return false;
  }
  return true;
}

static bool convert_source_path(w_seed_byte_view source_path,
                                uint16_t *destination, size_t *length) {
  if (destination == NULL || length == NULL || source_path.data == NULL ||
      source_path.length == 0u ||
      source_path.length > W_SEED_OWNER_GUARD_MAX_PATH_BYTES ||
      source_path.length > (size_t)INT_MAX ||
      source_path.data[0] == (uint8_t)'/' ||
      source_path.data[0] == (uint8_t)'\\' ||
      source_path.data[source_path.length - 1u] == (uint8_t)'/' ||
      source_path.data[source_path.length - 1u] == (uint8_t)'\\')
    return false;
  for (size_t index = 0u; index < source_path.length; index += 1u) {
    if (source_path.data[index] == 0u ||
        source_path.data[index] == (uint8_t)'\\' ||
        source_path.data[index] == (uint8_t)':')
      return false;
  }
  WCHAR wide[W_SEED_OWNER_GUARD_MAX_PATH_BYTES + 1u];
  const int converted = MultiByteToWideChar(
      CP_UTF8, MB_ERR_INVALID_CHARS, (LPCCH)source_path.data,
      (int)source_path.length, wide,
      (int)W_SEED_OWNER_GUARD_MAX_PATH_BYTES);
  if (converted <= 0) return false;
  const size_t wide_count = (size_t)converted;
  size_t component_start = 0u;
  for (size_t index = 0u; index <= wide_count; index += 1u) {
    if (index != wide_count && wide[index] != (WCHAR)L'/') continue;
    if (!wide_component_valid(wide, component_start, index)) return false;
    component_start = index + 1u;
  }
  for (size_t index = 0u; index < wide_count; index += 1u)
    destination[index] =
        (uint16_t)(wide[index] == (WCHAR)L'/' ? (WCHAR)L'\\' : wide[index]);
  destination[wide_count] = 0u;
  *length = wide_count;
  return true;
}

static void clear_slot(w_seed_owner_guard_windows_slot *slot) {
  if (slot == NULL) return;
  slot->native_handle = (uintptr_t)0u;
  (void)memset(&slot->identity, 0, sizeof(slot->identity));
  slot->kind = W_SEED_OWNER_GUARD_WINDOWS_SLOT_EMPTY;
  slot->used = false;
}

static bool push_slot(w_seed_owner_guard_windows_context *context,
                      const windows_entry *entry,
                      w_seed_owner_guard_windows_slot_kind kind,
                      size_t *slot_index) {
  if (context == NULL || entry == NULL || slot_index == NULL ||
      !native_handle_valid(entry->handle) ||
      context->slot_count >= W_SEED_OWNER_GUARD_WINDOWS_MAX_HANDLES)
    return false;
  const size_t index = context->slot_count;
  context->slots[index].native_handle = value_from_native(entry->handle);
  context->slots[index].identity = entry->identity;
  context->slots[index].kind = kind;
  context->slots[index].used = true;
  context->slot_count += 1u;
  *slot_index = index;
  return true;
}

static void cleanup_session(w_seed_owner_guard_windows_context *context) {
  if (context == NULL) return;
  for (size_t index = context->slot_count; index > 0u; index -= 1u) {
    w_seed_owner_guard_windows_slot *slot = &context->slots[index - 1u];
    if (slot->used && native_handle_valid(
                          native_from_value(slot->native_handle)))
      (void)NtClose(native_from_value(slot->native_handle));
    clear_slot(slot);
  }
  context->slot_count = 0u;
  context->level_count = 0u;
  context->candidate_count = 0u;
  context->source_slot = SIZE_MAX;
  context->source_path_length = 0u;
  context->source_path[0] = 0u;
  for (size_t index = 0u; index < W_SEED_OWNER_GUARD_MAX_LEVELS;
       index += 1u) {
    context->directory_slots[index] = SIZE_MAX;
    context->candidate_slots[index] = SIZE_MAX;
  }
  context->active_generation = 0u;
  context->session_live = false;
}

static bool retained_slot(
    const w_seed_owner_guard_windows_context *context, size_t slot_index,
    w_seed_owner_guard_windows_slot_kind kind,
    const w_seed_owner_guard_windows_slot **slot) {
  if (context == NULL || slot == NULL || slot_index >= context->slot_count)
    return false;
  const w_seed_owner_guard_windows_slot *value = &context->slots[slot_index];
  if (!value->used || !native_handle_valid(
                          native_from_value(value->native_handle)) ||
      value->kind != kind)
    return false;
  *slot = value;
  return true;
}

static w_seed_owner_guard_backend_status duplicate_base(
    const w_seed_owner_guard_windows_context *context, windows_entry *entry) {
  if (context == NULL || entry == NULL) return W_SEED_OWNER_GUARD_BACKEND_INVALID;
  HANDLE duplicate = NULL;
  if (!DuplicateHandle(GetCurrentProcess(),
                       native_from_value(context->base_handle),
                       GetCurrentProcess(), &duplicate, 0u, FALSE,
                       DUPLICATE_SAME_ACCESS))
    return W_SEED_OWNER_GUARD_BACKEND_IO;
  w_seed_owner_guard_backend_status status = query_entry(duplicate, entry);
  if (status != W_SEED_OWNER_GUARD_BACKEND_OK || !entry->directory ||
      !identity_equal(&entry->identity, &context->base_identity)) {
    (void)NtClose(duplicate);
    if (status != W_SEED_OWNER_GUARD_BACKEND_OK) return status;
    return !entry->directory ? W_SEED_OWNER_GUARD_BACKEND_UNSUPPORTED
                             : W_SEED_OWNER_GUARD_BACKEND_MUTATED;
  }
  return W_SEED_OWNER_GUARD_BACKEND_OK;
}

static w_seed_owner_guard_backend_status open_named(
    HANDLE parent, const WCHAR *name, int expected_directory,
    windows_entry *entry, bool *absent) {
  if (entry == NULL || absent == NULL)
    return W_SEED_OWNER_GUARD_BACKEND_INVALID;
  *absent = false;
  HANDLE handle = NULL;
  const ULONG type_options =
      expected_directory == 1 ? FILE_DIRECTORY_FILE : 0u;
  const NTSTATUS open_status = nt_open(parent, name, type_options, &handle);
  if (open_status != W_STATUS_SUCCESS) {
    if (nt_not_found(open_status)) {
      *absent = true;
      return W_SEED_OWNER_GUARD_BACKEND_OK;
    }
    return ntstatus_status(open_status);
  }
  w_seed_owner_guard_backend_status status = query_entry(handle, entry);
  if (status != W_SEED_OWNER_GUARD_BACKEND_OK ||
      (expected_directory >= 0 &&
       entry->directory != (expected_directory != 0))) {
    (void)NtClose(handle);
    return status == W_SEED_OWNER_GUARD_BACKEND_OK
               ? W_SEED_OWNER_GUARD_BACKEND_UNSUPPORTED
               : status;
  }
  return W_SEED_OWNER_GUARD_BACKEND_OK;
}

static w_seed_owner_guard_backend_status open_source_binding(
    const w_seed_owner_guard_windows_context *context, windows_entry *parent,
    windows_entry *source) {
  if (context == NULL || parent == NULL || source == NULL)
    return W_SEED_OWNER_GUARD_BACKEND_INVALID;
  w_seed_owner_guard_backend_status status = duplicate_base(context, parent);
  if (status != W_SEED_OWNER_GUARD_BACKEND_OK) return status;
  const WCHAR *path = (const WCHAR *)(const void *)context->source_path;
  size_t component_start = 0u;
  size_t leaf_start = 0u;
  for (size_t index = 0u; index <= context->source_path_length; index += 1u) {
    if (index != context->source_path_length && path[index] != (WCHAR)L'\\')
      continue;
    if (index == context->source_path_length) {
      leaf_start = component_start;
      break;
    }
    WCHAR component[W_SEED_OWNER_GUARD_MAX_PATH_BYTES + 1u];
    const size_t length = index - component_start;
    (void)memcpy(component, path + component_start, length * sizeof(WCHAR));
    component[length] = (WCHAR)0;
    windows_entry next;
    bool absent = false;
    status = open_named(parent->handle, component, 1, &next, &absent);
    if (status != W_SEED_OWNER_GUARD_BACKEND_OK || absent) {
      (void)NtClose(parent->handle);
      return absent ? W_SEED_OWNER_GUARD_BACKEND_IO : status;
    }
    (void)NtClose(parent->handle);
    *parent = next;
    component_start = index + 1u;
  }
  bool absent = false;
  status = open_named(parent->handle, path + leaf_start, 0, source, &absent);
  if (status != W_SEED_OWNER_GUARD_BACKEND_OK || absent) {
    (void)NtClose(parent->handle);
    return absent ? W_SEED_OWNER_GUARD_BACKEND_IO : status;
  }
  if (source->identity.volume_serial != parent->identity.volume_serial) {
    (void)NtClose(source->handle);
    (void)NtClose(parent->handle);
    return W_SEED_OWNER_GUARD_BACKEND_BOUNDARY;
  }
  return W_SEED_OWNER_GUARD_BACKEND_OK;
}

static w_seed_owner_guard_backend_status open_candidate(
    HANDLE directory, windows_entry *candidate, bool *absent) {
  return open_named(directory, L"build.w", 0, candidate, absent);
}

static w_seed_owner_guard_backend_status open_parent(
    HANDLE directory, windows_entry *parent) {
  bool absent = false;
  const w_seed_owner_guard_backend_status status =
      open_named(directory, L"..", 1, parent, &absent);
  return absent ? W_SEED_OWNER_GUARD_BACKEND_IO : status;
}

static bool identity_seen(const w_seed_owner_guard_windows_context *context,
                          const w_seed_owner_guard_windows_identity *identity,
                          size_t level_count) {
  for (size_t index = 0u; index < level_count; index += 1u) {
    const size_t slot_index = context->directory_slots[index];
    if (slot_index < context->slot_count &&
        identity_equal(&context->slots[slot_index].identity, identity))
      return true;
  }
  return false;
}

static w_seed_owner_guard_backend_result windows_begin(
    void *context_value, w_seed_byte_view source_path,
    w_seed_owner_guard_observation *observations,
    size_t observation_capacity) {
  w_seed_owner_guard_windows_context *context = context_value;
  if (context == NULL || observations == NULL || observation_capacity == 0u)
    return windows_result(W_SEED_OWNER_GUARD_BACKEND_INVALID,
                          W_SEED_OWNER_GUARD_BACKEND_PHASE_VALIDATE,
                          W_SEED_OWNER_GUARD_NO_LEVEL, 0u, 0u, 0u, 0u);
  if (!context->initialized || !context->native_supported)
    return windows_result(W_SEED_OWNER_GUARD_BACKEND_UNSUPPORTED,
                          W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_START,
                          W_SEED_OWNER_GUARD_NO_LEVEL, 0u, 0u, 0u, 0u);
  if (context->session_live ||
      !convert_source_path(source_path, context->source_path,
                           &context->source_path_length))
    return windows_result(W_SEED_OWNER_GUARD_BACKEND_INVALID,
                          W_SEED_OWNER_GUARD_BACKEND_PHASE_VALIDATE,
                          W_SEED_OWNER_GUARD_NO_LEVEL, 0u, 0u, 0u, 0u);
  windows_entry base_now;
  w_seed_owner_guard_backend_status status =
      query_entry(native_from_value(context->base_handle), &base_now);
  if (status != W_SEED_OWNER_GUARD_BACKEND_OK || !base_now.directory ||
      !identity_equal(&base_now.identity, &context->base_identity))
    return windows_result(W_SEED_OWNER_GUARD_BACKEND_MUTATED,
                          W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_START,
                          W_SEED_OWNER_GUARD_NO_LEVEL, 0u, 0u, 0u, 0u);
  if (context->next_generation == 0u ||
      context->next_generation == UINT64_MAX)
    return windows_result(W_SEED_OWNER_GUARD_BACKEND_FAULT,
                          W_SEED_OWNER_GUARD_BACKEND_PHASE_VALIDATE,
                          W_SEED_OWNER_GUARD_NO_LEVEL, 0u, 0u, 0u, 0u);
  context->active_generation = context->next_generation;
  context->next_generation += 1u;
  context->session_live = true;

  windows_entry parent;
  windows_entry source;
  status = open_source_binding(context, &parent, &source);
  if (status != W_SEED_OWNER_GUARD_BACKEND_OK) {
    cleanup_session(context);
    return windows_result(status,
                          W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_START,
                          W_SEED_OWNER_GUARD_NO_LEVEL, 0u, 0u, 0u, 0u);
  }
  size_t parent_slot = SIZE_MAX;
  if (!push_slot(context, &parent,
                 W_SEED_OWNER_GUARD_WINDOWS_SLOT_DIRECTORY, &parent_slot)) {
    (void)NtClose(source.handle);
    (void)NtClose(parent.handle);
    cleanup_session(context);
    return windows_result(W_SEED_OWNER_GUARD_BACKEND_FAULT,
                          W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_START,
                          W_SEED_OWNER_GUARD_NO_LEVEL, 0u, 0u, 0u, 0u);
  }
  context->directory_slots[0] = parent_slot;
  if (!push_slot(context, &source, W_SEED_OWNER_GUARD_WINDOWS_SLOT_SOURCE,
                 &context->source_slot)) {
    (void)NtClose(source.handle);
    cleanup_session(context);
    return windows_result(W_SEED_OWNER_GUARD_BACKEND_FAULT,
                          W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_START,
                          W_SEED_OWNER_GUARD_NO_LEVEL, 0u, 0u, 0u, 0u);
  }

  size_t candidate_count = 0u;
  size_t level = 0u;
  for (;;) {
    w_seed_owner_guard_windows_slot *directory =
        &context->slots[context->directory_slots[level]];
    observations[level] = (w_seed_owner_guard_observation){
        level, W_SEED_OWNER_GUARD_NO_CANDIDATE, false};
    windows_entry candidate;
    bool absent = false;
    status = open_candidate(native_from_value(directory->native_handle),
                            &candidate, &absent);
    if (status != W_SEED_OWNER_GUARD_BACKEND_OK) {
      cleanup_session(context);
      return windows_result(
          status, W_SEED_OWNER_GUARD_BACKEND_PHASE_LOOKUP_CANDIDATE, level,
          0u, 0u, 0u, 0u);
    }
    if (!absent) {
      if (candidate.identity.volume_serial !=
              directory->identity.volume_serial ||
          !push_slot(context, &candidate,
                     W_SEED_OWNER_GUARD_WINDOWS_SLOT_CANDIDATE,
                     &context->candidate_slots[level])) {
        (void)NtClose(candidate.handle);
        cleanup_session(context);
        return windows_result(
            W_SEED_OWNER_GUARD_BACKEND_BOUNDARY,
            W_SEED_OWNER_GUARD_BACKEND_PHASE_LOOKUP_CANDIDATE, level, 0u, 0u,
            0u, 0u);
      }
      observations[level].candidate_index = candidate_count;
      candidate_count += 1u;
    }
    windows_entry next;
    status = open_parent(native_from_value(directory->native_handle), &next);
    if (status != W_SEED_OWNER_GUARD_BACKEND_OK ||
        next.identity.volume_serial != directory->identity.volume_serial) {
      if (status == W_SEED_OWNER_GUARD_BACKEND_OK)
        (void)NtClose(next.handle);
      cleanup_session(context);
      return windows_result(
          status == W_SEED_OWNER_GUARD_BACKEND_OK
              ? W_SEED_OWNER_GUARD_BACKEND_BOUNDARY
              : status,
          W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_PARENT, level, 0u, 0u, 0u,
          0u);
    }
    if (identity_equal(&next.identity, &directory->identity)) {
      (void)NtClose(next.handle);
      observations[level].root_terminal = true;
      context->level_count = level + 1u;
      context->candidate_count = candidate_count;
      return windows_result(W_SEED_OWNER_GUARD_BACKEND_OK,
                            W_SEED_OWNER_GUARD_BACKEND_PHASE_COMMIT, level,
                            0u, context->active_generation,
                            context->level_count, candidate_count);
    }
    if (identity_seen(context, &next.identity, level + 1u)) {
      (void)NtClose(next.handle);
      cleanup_session(context);
      return windows_result(W_SEED_OWNER_GUARD_BACKEND_BOUNDARY,
                            W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_PARENT,
                            level, 0u, 0u, 0u, 0u);
    }
    if (level + 1u >= observation_capacity ||
        level + 1u >= W_SEED_OWNER_GUARD_MAX_LEVELS) {
      (void)NtClose(next.handle);
      cleanup_session(context);
      return windows_result(W_SEED_OWNER_GUARD_BACKEND_CAPACITY,
                            W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_PARENT,
                            observation_capacity, observation_capacity + 1u,
                            0u, 0u, 0u);
    }
    level += 1u;
    size_t next_slot = SIZE_MAX;
    if (!push_slot(context, &next,
                   W_SEED_OWNER_GUARD_WINDOWS_SLOT_DIRECTORY, &next_slot)) {
      (void)NtClose(next.handle);
      cleanup_session(context);
      return windows_result(W_SEED_OWNER_GUARD_BACKEND_FAULT,
                            W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_PARENT,
                            level, 0u, 0u, 0u, 0u);
    }
    context->directory_slots[level] = next_slot;
  }
}

static w_seed_owner_guard_backend_result windows_revalidate(
    void *context_value, uint64_t generation,
    w_seed_owner_guard_observation *observations,
    size_t observation_capacity) {
  w_seed_owner_guard_windows_context *context = context_value;
  if (context == NULL || !context->initialized || !context->native_supported ||
      !context->session_live || generation == 0u ||
      generation != context->active_generation || observations == NULL ||
      observation_capacity < context->level_count)
    return windows_result(W_SEED_OWNER_GUARD_BACKEND_INVALID,
                          W_SEED_OWNER_GUARD_BACKEND_PHASE_VALIDATE,
                          W_SEED_OWNER_GUARD_NO_LEVEL, 0u, generation, 0u, 0u);
  windows_entry base_now;
  if (query_entry(native_from_value(context->base_handle), &base_now) !=
          W_SEED_OWNER_GUARD_BACKEND_OK ||
      !base_now.directory ||
      !identity_equal(&base_now.identity, &context->base_identity))
    return windows_result(
        W_SEED_OWNER_GUARD_BACKEND_MUTATED,
        W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_DIRECTORY, 0u, 0u,
        generation, 0u, 0u);
  windows_entry parent;
  windows_entry source;
  w_seed_owner_guard_backend_status status =
      open_source_binding(context, &parent, &source);
  const w_seed_owner_guard_windows_slot *retained_source = NULL;
  const w_seed_owner_guard_windows_slot *retained_start = NULL;
  if (status != W_SEED_OWNER_GUARD_BACKEND_OK ||
      !retained_slot(context, context->source_slot,
                     W_SEED_OWNER_GUARD_WINDOWS_SLOT_SOURCE,
                     &retained_source) ||
      !retained_slot(context, context->directory_slots[0],
                     W_SEED_OWNER_GUARD_WINDOWS_SLOT_DIRECTORY,
                     &retained_start) ||
      !identity_equal(&source.identity, &retained_source->identity) ||
      !identity_equal(&parent.identity, &retained_start->identity)) {
    if (status == W_SEED_OWNER_GUARD_BACKEND_OK) {
      (void)NtClose(source.handle);
      (void)NtClose(parent.handle);
    }
    return windows_result(
        status == W_SEED_OWNER_GUARD_BACKEND_OK
            ? W_SEED_OWNER_GUARD_BACKEND_MUTATED
            : status,
        W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_DIRECTORY, 0u, 0u,
        generation, 0u, 0u);
  }
  (void)NtClose(source.handle);
  (void)NtClose(parent.handle);

  size_t candidate_count = 0u;
  for (size_t level = 0u; level < context->level_count; level += 1u) {
    const w_seed_owner_guard_windows_slot *directory = NULL;
    if (!retained_slot(context, context->directory_slots[level],
                       W_SEED_OWNER_GUARD_WINDOWS_SLOT_DIRECTORY,
                       &directory))
      return windows_result(
          W_SEED_OWNER_GUARD_BACKEND_FAULT,
          W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_DIRECTORY, level, 0u,
          generation, 0u, 0u);
    windows_entry directory_now;
    if (query_entry(native_from_value(directory->native_handle),
                    &directory_now) != W_SEED_OWNER_GUARD_BACKEND_OK ||
        !directory_now.directory ||
        !identity_equal(&directory_now.identity, &directory->identity))
      return windows_result(
          W_SEED_OWNER_GUARD_BACKEND_MUTATED,
          W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_DIRECTORY, level, 0u,
          generation, 0u, 0u);
    observations[level] = (w_seed_owner_guard_observation){
        level, W_SEED_OWNER_GUARD_NO_CANDIDATE, false};
    windows_entry candidate;
    bool absent = false;
    status = open_candidate(native_from_value(directory->native_handle),
                            &candidate, &absent);
    const size_t expected_candidate = context->candidate_slots[level];
    if (status != W_SEED_OWNER_GUARD_BACKEND_OK)
      return windows_result(
          status, W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_CANDIDATE,
          level, 0u, generation, 0u, 0u);
    if (expected_candidate == SIZE_MAX) {
      if (!absent) {
        (void)NtClose(candidate.handle);
        return windows_result(
            W_SEED_OWNER_GUARD_BACKEND_MUTATED,
            W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_CANDIDATE, level, 0u,
            generation, 0u, 0u);
      }
    } else {
      const w_seed_owner_guard_windows_slot *retained_candidate = NULL;
      if (absent || !retained_slot(
                        context, expected_candidate,
                        W_SEED_OWNER_GUARD_WINDOWS_SLOT_CANDIDATE,
                        &retained_candidate) ||
          !identity_equal(&candidate.identity,
                          &retained_candidate->identity)) {
        if (!absent) (void)NtClose(candidate.handle);
        return windows_result(
            W_SEED_OWNER_GUARD_BACKEND_MUTATED,
            W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_CANDIDATE, level, 0u,
            generation, 0u, 0u);
      }
      (void)NtClose(candidate.handle);
      observations[level].candidate_index = candidate_count;
      candidate_count += 1u;
    }
    windows_entry next;
    status = open_parent(native_from_value(directory->native_handle), &next);
    if (status != W_SEED_OWNER_GUARD_BACKEND_OK)
      return windows_result(
          status, W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_PARENT, level,
          0u, generation, 0u, 0u);
    bool parent_ok = false;
    if (level + 1u == context->level_count) {
      parent_ok = identity_equal(&next.identity, &directory->identity);
      observations[level].root_terminal = parent_ok;
    } else {
      const w_seed_owner_guard_windows_slot *retained_parent = NULL;
      parent_ok = retained_slot(
                      context, context->directory_slots[level + 1u],
                      W_SEED_OWNER_GUARD_WINDOWS_SLOT_DIRECTORY,
                      &retained_parent) &&
                  identity_equal(&next.identity, &retained_parent->identity);
    }
    const bool same_volume =
        next.identity.volume_serial == directory->identity.volume_serial;
    (void)NtClose(next.handle);
    if (!parent_ok || !same_volume)
      return windows_result(
          W_SEED_OWNER_GUARD_BACKEND_MUTATED,
          W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_PARENT, level, 0u,
          generation, 0u, 0u);
  }
  if (candidate_count != context->candidate_count)
    return windows_result(
        W_SEED_OWNER_GUARD_BACKEND_MUTATED,
        W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_CANDIDATE,
        W_SEED_OWNER_GUARD_NO_LEVEL, 0u, generation, 0u, 0u);
  return windows_result(W_SEED_OWNER_GUARD_BACKEND_OK,
                        W_SEED_OWNER_GUARD_BACKEND_PHASE_COMMIT,
                        context->level_count - 1u, 0u, generation,
                        context->level_count, candidate_count);
}

static void windows_abort_begin(void *context_value) {
  cleanup_session((w_seed_owner_guard_windows_context *)context_value);
}

static void windows_destroy(void *context_value, uint64_t generation) {
  w_seed_owner_guard_windows_context *context = context_value;
  if (context == NULL || !context->session_live || generation == 0u ||
      generation != context->active_generation)
    return;
  cleanup_session(context);
}

static bool parent_probe(HANDLE base,
                         const w_seed_owner_guard_windows_identity *base_id,
                         uint32_t *native_status) {
  if (native_status == NULL) return false;
  HANDLE handle = NULL;
  const NTSTATUS status = nt_open(base, L"..", FILE_DIRECTORY_FILE, &handle);
  *native_status = (uint32_t)(ULONG)status;
  if (status != W_STATUS_SUCCESS) return false;
  windows_entry parent;
  const w_seed_owner_guard_backend_status query = query_entry(handle, &parent);
  const bool valid = query == W_SEED_OWNER_GUARD_BACKEND_OK &&
                     parent.directory &&
                     parent.identity.volume_serial == base_id->volume_serial;
  (void)NtClose(handle);
  return valid;
}

bool w_seed_owner_guard_windows_init(
    w_seed_owner_guard_windows_context *context, uintptr_t base_handle) {
  if (context == NULL) return false;
  (void)memset(context, 0, sizeof(*context));
  context->base_handle = base_handle;
  context->locality_status = UINT32_MAX;
  context->parent_probe_status = UINT32_MAX;
  context->next_generation = 1u;
  context->source_slot = SIZE_MAX;
  for (size_t index = 0u; index < W_SEED_OWNER_GUARD_MAX_LEVELS;
       index += 1u) {
    context->directory_slots[index] = SIZE_MAX;
    context->candidate_slots[index] = SIZE_MAX;
  }
  for (size_t index = 0u; index < W_SEED_OWNER_GUARD_WINDOWS_MAX_HANDLES;
       index += 1u)
    clear_slot(&context->slots[index]);
  if (!native_handle_valid(native_from_value(base_handle))) return false;
  windows_entry base;
  if (query_entry(native_from_value(base_handle), &base) !=
          W_SEED_OWNER_GUARD_BACKEND_OK ||
      !base.directory)
    return false;
  context->base_identity = base.identity;
  context->initialized = true;
  context->base_local = local_disk_handle(native_from_value(base_handle),
                                          &context->locality_status);
  (void)parent_probe(native_from_value(base_handle), &base.identity,
                     &context->parent_probe_status);
  /* Probes remain diagnostic only. The Windows capability is not promoted in
   * this bundle, even on a host where both probes happen to succeed. */
  context->native_supported = false;
  return true;
}

#else

static w_seed_owner_guard_backend_result windows_begin(
    void *context, w_seed_byte_view source_path,
    w_seed_owner_guard_observation *observations,
    size_t observation_capacity) {
  (void)context;
  (void)source_path;
  (void)observations;
  (void)observation_capacity;
  return windows_result(W_SEED_OWNER_GUARD_BACKEND_UNSUPPORTED,
                        W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_START,
                        W_SEED_OWNER_GUARD_NO_LEVEL, 0u, 0u, 0u, 0u);
}

static w_seed_owner_guard_backend_result windows_revalidate(
    void *context, uint64_t generation,
    w_seed_owner_guard_observation *observations,
    size_t observation_capacity) {
  (void)context;
  (void)observations;
  (void)observation_capacity;
  return windows_result(W_SEED_OWNER_GUARD_BACKEND_UNSUPPORTED,
                        W_SEED_OWNER_GUARD_BACKEND_PHASE_VALIDATE,
                        W_SEED_OWNER_GUARD_NO_LEVEL, 0u, generation, 0u, 0u);
}

static void windows_abort_begin(void *context) { (void)context; }

static void windows_destroy(void *context, uint64_t generation) {
  (void)context;
  (void)generation;
}

bool w_seed_owner_guard_windows_init(
    w_seed_owner_guard_windows_context *context, uintptr_t base_handle) {
  if (context == NULL) return false;
  (void)memset(context, 0, sizeof(*context));
  context->base_handle = base_handle;
  context->next_generation = 1u;
  context->initialized = true;
  context->native_supported = false;
  return true;
}

#endif

bool w_seed_owner_guard_windows_backend(
    w_seed_owner_guard_windows_context *context,
    w_seed_owner_guard_backend *backend) {
  if (context == NULL || backend == NULL || !context->initialized)
    return false;
  *backend = (w_seed_owner_guard_backend){
      .context = context,
      .begin = windows_begin,
      .revalidate = windows_revalidate,
      .abort_begin = windows_abort_begin,
      .destroy = windows_destroy,
  };
  return true;
}
