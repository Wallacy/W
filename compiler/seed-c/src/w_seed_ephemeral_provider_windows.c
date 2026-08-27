#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0602
#endif

#include "w_seed_ephemeral_provider_windows.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

static const char windows_provider_id[] = "windows-ntcreatefile-v1";

enum {
  WINDOWS_PROVIDER_ID_LENGTH = sizeof(windows_provider_id) - 1u,
  WINDOWS_IDENTITY_TOKEN_LENGTH = 50u,
};

static w_seed_ephemeral_provider_metadata windows_metadata(void) {
  const w_seed_ephemeral_provider_token_capacity provider = {
      WINDOWS_PROVIDER_ID_LENGTH, WINDOWS_PROVIDER_ID_LENGTH};
  const w_seed_ephemeral_provider_token_capacity identity = {
      WINDOWS_IDENTITY_TOKEN_LENGTH, WINDOWS_IDENTITY_TOKEN_LENGTH};
  return (w_seed_ephemeral_provider_metadata){
      provider, identity, identity, identity};
}

#if defined(_WIN32)

#include <windows.h>
#include <fileapi.h>
#include <processthreadsapi.h>
#include <winternl.h>

_Static_assert(sizeof(WCHAR) == sizeof(uint16_t),
               "Windows provider requires 16-bit WCHAR");

enum {
  WINDOWS_MAX_WIDE_PATH = W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES + 1u,
  WINDOWS_WIDE_PREFIX_CAPACITY = 8u,
};

/* MinGW does not expose STATUS_REPARSE_POINT_ENCOUNTERED in ntstatus.h and
 * including ntstatus.h together with windows.h creates duplicate definitions
 * on some SDKs. Keep the stable values needed for the closed mapping here. */
#define W_STATUS_SUCCESS ((NTSTATUS)0x00000000u)
#define W_STATUS_INVALID_PARAMETER ((NTSTATUS)0xC000000Du)
#define W_STATUS_NOT_IMPLEMENTED ((NTSTATUS)0xC0000002u)
#define W_STATUS_INVALID_DEVICE_REQUEST ((NTSTATUS)0xC0000010u)
#define W_STATUS_OBJECT_NAME_INVALID ((NTSTATUS)0xC0000033u)
#define W_STATUS_OBJECT_NAME_NOT_FOUND ((NTSTATUS)0xC0000034u)
#define W_STATUS_OBJECT_PATH_NOT_FOUND ((NTSTATUS)0xC000003Au)
#define W_STATUS_OBJECT_PATH_SYNTAX_BAD ((NTSTATUS)0xC000003Bu)
#define W_STATUS_FILE_IS_A_DIRECTORY ((NTSTATUS)0xC00000BAu)
#define W_STATUS_NOT_SUPPORTED ((NTSTATUS)0xC00000BBu)
#define W_STATUS_NOT_A_DIRECTORY ((NTSTATUS)0xC0000103u)
#define W_STATUS_STOPPED_ON_SYMLINK ((NTSTATUS)0x8000002Du)
#define W_STATUS_IO_REPARSE_TAG_NOT_HANDLED ((NTSTATUS)0xC0000279u)
#define W_STATUS_DIRECTORY_IS_A_REPARSE_POINT ((NTSTATUS)0xC0000281u)
#define W_STATUS_REPARSE_POINT_ENCOUNTERED ((NTSTATUS)0xC000050Bu)

typedef struct {
  HANDLE handle;
  uint64_t volume_serial;
  uint8_t file_id[16];
  size_t byte_count;
} windows_identity;

typedef struct {
  WCHAR text[WINDOWS_MAX_WIDE_PATH];
  size_t length;
  size_t first_component;
  size_t leaf_start;
  bool absolute;
} windows_path;

static bool native_handle_valid(HANDLE handle) {
  return handle != NULL && handle != INVALID_HANDLE_VALUE;
}

static HANDLE native_from_value(uintptr_t value) {
  return (HANDLE)value;
}

static uintptr_t value_from_native(HANDLE handle) {
  return (uintptr_t)handle;
}

static w_seed_ephemeral_provider_windows_slot *slot_for_handle(
    w_seed_ephemeral_provider_windows_context *context,
    w_seed_ephemeral_provider_handle handle,
    w_seed_ephemeral_provider_windows_slot_kind expected_kind) {
  if (context == NULL || !context->initialized || handle.value == (uintptr_t)0u)
    return NULL;
  const uintptr_t span =
      (uintptr_t)W_SEED_EPHEMERAL_PROVIDER_WINDOWS_MAX_HANDLES;
  const uintptr_t normalized = handle.value - (uintptr_t)1u;
  const size_t index = (size_t)(normalized % span);
  const uint64_t generation = (uint64_t)(normalized / span);
  if (index >= W_SEED_EPHEMERAL_PROVIDER_WINDOWS_MAX_HANDLES)
    return NULL;
  w_seed_ephemeral_provider_windows_slot *slot = &context->slots[index];
  if (!slot->used || slot->generation != generation ||
      slot->kind != expected_kind)
    return NULL;
  return slot;
}

static bool encode_handle(size_t index, uint64_t generation,
                          w_seed_ephemeral_provider_handle *handle) {
  if (handle == NULL ||
      index >= W_SEED_EPHEMERAL_PROVIDER_WINDOWS_MAX_HANDLES ||
      generation == 0u)
    return false;
  const uintptr_t span =
      (uintptr_t)W_SEED_EPHEMERAL_PROVIDER_WINDOWS_MAX_HANDLES;
  const uintptr_t slot_number = (uintptr_t)(index + 1u);
  if (generation > (uint64_t)UINTPTR_MAX) return false;
  const uintptr_t generation_value = (uintptr_t)generation;
  if (generation_value > (UINTPTR_MAX - slot_number) / span) return false;
  handle->value = generation_value * span + slot_number;
  return handle->value != (uintptr_t)0u;
}

static bool identity_copy(const windows_identity *source,
                          w_seed_ephemeral_provider_windows_slot *slot,
                          HANDLE native_handle,
                          w_seed_ephemeral_provider_windows_slot_kind kind,
                          uint64_t generation) {
  if (source == NULL || slot == NULL || !native_handle_valid(native_handle) ||
      generation == 0u)
    return false;
  slot->native_handle = value_from_native(native_handle);
  slot->volume_serial = source->volume_serial;
  (void)memcpy(slot->file_id, source->file_id, sizeof(slot->file_id));
  slot->generation = generation;
  slot->kind = kind;
  slot->used = true;
  return true;
}

static bool allocate_slot(w_seed_ephemeral_provider_windows_context *context,
                          HANDLE native_handle,
                          const windows_identity *identity,
                          w_seed_ephemeral_provider_windows_slot_kind kind,
                          w_seed_ephemeral_provider_handle *handle) {
  if (context == NULL || identity == NULL || handle == NULL ||
      !native_handle_valid(native_handle) || !context->initialized)
    return false;
  uint64_t generation = context->next_generation;
  if (generation == 0u) generation = 1u;
  for (size_t index = 0u;
       index < W_SEED_EPHEMERAL_PROVIDER_WINDOWS_MAX_HANDLES; index += 1u) {
    w_seed_ephemeral_provider_windows_slot *slot = &context->slots[index];
    if (slot->used) continue;
    if (!encode_handle(index, generation, handle) ||
        !identity_copy(identity, slot, native_handle, kind, generation))
      return false;
    context->next_generation = generation == UINT64_MAX ? 1u : generation + 1u;
    return true;
  }
  return false;
}

static void release_slot(w_seed_ephemeral_provider_windows_slot *slot) {
  if (slot == NULL || !slot->used) return;
  const HANDLE native_handle = native_from_value(slot->native_handle);
  slot->native_handle = (uintptr_t)0u;
  slot->volume_serial = 0u;
  (void)memset(slot->file_id, 0, sizeof(slot->file_id));
  slot->generation = 0u;
  slot->kind = W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_EMPTY;
  slot->used = false;
  if (native_handle_valid(native_handle)) (void)NtClose(native_handle);
}

static w_seed_ephemeral_provider_backend_status ntstatus_status(
    NTSTATUS status) {
  switch (status) {
    case W_STATUS_OBJECT_NAME_NOT_FOUND:
    case W_STATUS_OBJECT_PATH_NOT_FOUND:
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_NOT_FOUND;
    case W_STATUS_REPARSE_POINT_ENCOUNTERED:
    case W_STATUS_STOPPED_ON_SYMLINK:
    case W_STATUS_IO_REPARSE_TAG_NOT_HANDLED:
    case W_STATUS_DIRECTORY_IS_A_REPARSE_POINT:
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_SYMLINK;
    case W_STATUS_INVALID_PARAMETER:
    case W_STATUS_OBJECT_NAME_INVALID:
    case W_STATUS_OBJECT_PATH_SYNTAX_BAD:
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
    case W_STATUS_NOT_IMPLEMENTED:
    case W_STATUS_INVALID_DEVICE_REQUEST:
    case W_STATUS_NOT_SUPPORTED:
    case W_STATUS_FILE_IS_A_DIRECTORY:
    case W_STATUS_NOT_A_DIRECTORY:
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
    default:
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO;
  }
}

static w_seed_ephemeral_provider_backend_status win32_error_status(
    DWORD error) {
  switch (error) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_NOT_FOUND;
    case ERROR_INVALID_NAME:
    case ERROR_INVALID_PARAMETER:
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
    case ERROR_INVALID_FUNCTION:
    case ERROR_NOT_SUPPORTED:
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
    case ERROR_CANT_ACCESS_FILE:
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_SYMLINK;
    default:
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO;
  }
}

static size_t wide_length(const WCHAR *text) {
  if (text == NULL) return 0u;
  size_t length = 0u;
  while (text[length] != (WCHAR)0) length += 1u;
  return length;
}

static bool ascii_letter(WCHAR value) {
  return (value >= (WCHAR)L'a' && value <= (WCHAR)L'z') ||
         (value >= (WCHAR)L'A' && value <= (WCHAR)L'Z');
}

static WCHAR ascii_lower(WCHAR value) {
  if (value >= (WCHAR)L'A' && value <= (WCHAR)L'Z')
    return (WCHAR)(value + ((WCHAR)L'a' - (WCHAR)L'A'));
  return value;
}

static bool ascii_component_equals(const WCHAR *text, size_t start, size_t end,
                                   const char *literal) {
  if (text == NULL || literal == NULL || start >= end) return false;
  size_t literal_length = 0u;
  while (literal[literal_length] != '\0') literal_length += 1u;
  if (end - start != literal_length) return false;
  for (size_t index = 0u; index < literal_length; index += 1u) {
    const WCHAR expected = (WCHAR)(unsigned char)literal[index];
    if (ascii_lower(text[start + index]) != ascii_lower(expected)) return false;
  }
  return true;
}

static bool reserved_device_component(const WCHAR *text, size_t start,
                                      size_t end) {
  if (text == NULL || start >= end) return false;
  size_t base_end = start;
  while (base_end < end && text[base_end] != (WCHAR)L'.') base_end += 1u;
  if (ascii_component_equals(text, start, base_end, "CON") ||
      ascii_component_equals(text, start, base_end, "PRN") ||
      ascii_component_equals(text, start, base_end, "AUX") ||
      ascii_component_equals(text, start, base_end, "NUL") ||
      ascii_component_equals(text, start, base_end, "CLOCK$"))
    return true;
  if (base_end - start == 4u &&
      (ascii_component_equals(text, start, base_end, "COM1") ||
       ascii_component_equals(text, start, base_end, "COM2") ||
       ascii_component_equals(text, start, base_end, "COM3") ||
       ascii_component_equals(text, start, base_end, "COM4") ||
       ascii_component_equals(text, start, base_end, "COM5") ||
       ascii_component_equals(text, start, base_end, "COM6") ||
       ascii_component_equals(text, start, base_end, "COM7") ||
       ascii_component_equals(text, start, base_end, "COM8") ||
       ascii_component_equals(text, start, base_end, "COM9") ||
       ascii_component_equals(text, start, base_end, "LPT1") ||
       ascii_component_equals(text, start, base_end, "LPT2") ||
       ascii_component_equals(text, start, base_end, "LPT3") ||
       ascii_component_equals(text, start, base_end, "LPT4") ||
       ascii_component_equals(text, start, base_end, "LPT5") ||
       ascii_component_equals(text, start, base_end, "LPT6") ||
       ascii_component_equals(text, start, base_end, "LPT7") ||
       ascii_component_equals(text, start, base_end, "LPT8") ||
       ascii_component_equals(text, start, base_end, "LPT9")))
    return true;
  return false;
}

static bool component_valid(const WCHAR *text, size_t start, size_t end) {
  if (text == NULL || start >= end) return false;
  if ((end - start == 1u && text[start] == (WCHAR)L'.') ||
      (end - start == 2u && text[start] == (WCHAR)L'.' &&
       text[start + 1u] == (WCHAR)L'.'))
    return false;
  if (text[end - 1u] == (WCHAR)L'.' || text[end - 1u] == (WCHAR)L' ')
    return false;
  if (reserved_device_component(text, start, end)) return false;
  for (size_t index = start; index < end; index += 1u) {
    const WCHAR value = text[index];
    if (value == (WCHAR)0 || value == (WCHAR)L':' || value == (WCHAR)L'/' ||
        value == (WCHAR)L'\\' || value == (WCHAR)L'*' ||
        value == (WCHAR)L'?' || value < (WCHAR)0x20u)
      return false;
  }
  return true;
}

static bool copy_wide_component(const WCHAR *text, size_t start, size_t end,
                                WCHAR destination[WINDOWS_MAX_WIDE_PATH]) {
  if (text == NULL || destination == NULL || !component_valid(text, start, end) ||
      end - start >= WINDOWS_MAX_WIDE_PATH)
    return false;
  const size_t length = end - start;
  if (length != 0u)
    (void)memcpy(destination, text + start, length * sizeof(WCHAR));
  destination[length] = (WCHAR)0;
  return true;
}

static w_seed_ephemeral_provider_backend_status convert_root_path(
    w_seed_byte_view root_path, windows_path *converted) {
  if (converted == NULL || root_path.data == NULL || root_path.length == 0u ||
      root_path.length > W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  for (size_t index = 0u; index < root_path.length; index += 1u) {
    if (root_path.data[index] == 0u)
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  }
  if (root_path.length > (size_t)INT_MAX)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  const int input_length = (int)root_path.length;
  const int converted_length = MultiByteToWideChar(
      CP_UTF8, MB_ERR_INVALID_CHARS, (LPCCH)root_path.data, input_length,
      converted->text, (int)(WINDOWS_MAX_WIDE_PATH - 1u));
  if (converted_length <= 0) return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  converted->length = (size_t)converted_length;
  converted->text[converted->length] = (WCHAR)0;
  for (size_t index = 0u; index < converted->length; index += 1u) {
    if (converted->text[index] == (WCHAR)L'/')
      converted->text[index] = (WCHAR)L'\\';
  }

  converted->absolute = false;
  converted->first_component = 0u;
  if (converted->length >= 2u && converted->text[0] == (WCHAR)L'\\' &&
      converted->text[1] == (WCHAR)L'\\') {
    if (converted->length >= 3u &&
        (converted->text[2] == (WCHAR)L'?' ||
         converted->text[2] == (WCHAR)L'.'))
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
  }
  if (converted->length >= 2u && ascii_letter(converted->text[0]) &&
      converted->text[1] == (WCHAR)L':') {
    if (converted->length < 4u || converted->text[2] != (WCHAR)L'\\')
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
    converted->absolute = true;
    converted->first_component = 3u;
  } else {
    if (converted->text[0] == (WCHAR)L'\\' ||
        (converted->length >= 2u && converted->text[1] == (WCHAR)L':'))
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  }
  if (converted->text[converted->length - 1u] == (WCHAR)L'\\')
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  size_t component_start = converted->first_component;
  size_t last_separator = SIZE_MAX;
  for (size_t index = converted->first_component;
       index <= converted->length; index += 1u) {
    if (index != converted->length && converted->text[index] != (WCHAR)L'\\')
      continue;
    if (!component_valid(converted->text, component_start, index))
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
    if (index != converted->length) last_separator = index;
    component_start = index + 1u;
  }
  if (last_separator == SIZE_MAX) {
    converted->leaf_start = converted->first_component;
  } else {
    converted->leaf_start = last_separator + 1u;
  }
  if (converted->leaf_start >= converted->length) return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  /* A colon is valid only at the drive prefix. This also rejects ADS and
   * namespace-like syntax hidden in a non-prefix component. */
  for (size_t index = converted->absolute ? 2u : 0u;
       index < converted->length; index += 1u) {
    if (converted->text[index] == (WCHAR)L':')
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  }
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
}

static bool copy_source_id(w_seed_frontend_text source_id,
                           WCHAR destination[WINDOWS_MAX_WIDE_PATH]) {
  if (destination == NULL || source_id.data == NULL || source_id.length == 0u ||
      source_id.length >= WINDOWS_MAX_WIDE_PATH ||
      source_id.length > W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES)
    return false;
  const uint8_t *data = (const uint8_t *)source_id.data;
  size_t component_start = 0u;
  for (size_t index = 0u; index <= source_id.length; index += 1u) {
    if (index != source_id.length && data[index] != (uint8_t)'/') continue;
    if (component_start == index) return false;
    if (index - component_start == 1u && data[component_start] == '.')
      return false;
    if (index - component_start == 2u && data[component_start] == '.' &&
        data[component_start + 1u] == '.')
      return false;
    if (data[index == 0u ? 0u : index - 1u] == (uint8_t)'.' ||
        data[index == 0u ? 0u : index - 1u] == (uint8_t)' ')
      return false;
    for (size_t component = component_start; component < index; component += 1u) {
      if (data[component] == 0u || data[component] >= 0x80u ||
          data[component] == (uint8_t)'\\' || data[component] == (uint8_t)':')
        return false;
    }
    component_start = index + 1u;
  }
  for (size_t index = 0u; index < source_id.length; index += 1u)
    destination[index] = (WCHAR)(data[index] == (uint8_t)'/'
                                     ? (uint8_t)'\\'
                                     : data[index]);
  destination[source_id.length] = (WCHAR)0;
  return true;
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

static NTSTATUS nt_create(HANDLE root_directory, const WCHAR *path,
                          ACCESS_MASK desired_access, ULONG create_options,
                          HANDLE *handle) {
  if (handle == NULL || path == NULL)
    return W_STATUS_INVALID_PARAMETER;
  *handle = NULL;
  UNICODE_STRING name;
  if (!initialize_unicode_string(path, &name)) return W_STATUS_INVALID_PARAMETER;
  OBJECT_ATTRIBUTES attributes;
  InitializeObjectAttributes(&attributes, &name,
                             OBJ_CASE_INSENSITIVE | OBJ_DONT_REPARSE,
                             root_directory, NULL);
  IO_STATUS_BLOCK io_status;
  (void)memset(&io_status, 0, sizeof(io_status));
  return NtCreateFile(
      handle, desired_access, &attributes, &io_status, NULL,
      FILE_ATTRIBUTE_NORMAL,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN,
      create_options | FILE_OPEN_REPARSE_POINT, NULL, 0u);
}

static w_seed_ephemeral_provider_backend_status query_entry(
    HANDLE handle, bool expect_directory, windows_identity *identity) {
  if (!native_handle_valid(handle) || identity == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  if (GetFileType(handle) != FILE_TYPE_DISK)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
  FILE_ATTRIBUTE_TAG_INFO tag_info;
  (void)memset(&tag_info, 0, sizeof(tag_info));
  if (!GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tag_info,
                                    (DWORD)sizeof(tag_info)))
    return win32_error_status(GetLastError());
  if ((tag_info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_SYMLINK;
  FILE_STANDARD_INFO standard_info;
  (void)memset(&standard_info, 0, sizeof(standard_info));
  if (!GetFileInformationByHandleEx(handle, FileStandardInfo, &standard_info,
                                    (DWORD)sizeof(standard_info)))
    return win32_error_status(GetLastError());
  const bool is_directory = standard_info.Directory != FALSE;
  if (is_directory != expect_directory)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
  FILE_ID_INFO file_id_info;
  (void)memset(&file_id_info, 0, sizeof(file_id_info));
  if (!GetFileInformationByHandleEx(handle, FileIdInfo, &file_id_info,
                                    (DWORD)sizeof(file_id_info)))
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
  if (file_id_info.VolumeSerialNumber > UINT64_MAX)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
  identity->handle = handle;
  identity->volume_serial = (uint64_t)file_id_info.VolumeSerialNumber;
  (void)memcpy(identity->file_id, file_id_info.FileId.Identifier,
               sizeof(identity->file_id));
  if (is_directory) {
    identity->byte_count = 0u;
  } else {
    if (standard_info.EndOfFile.QuadPart < 0)
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
    const uint64_t raw_size = (uint64_t)standard_info.EndOfFile.QuadPart;
    if (raw_size > (uint64_t)SIZE_MAX)
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
    identity->byte_count = (size_t)raw_size;
  }
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
}

static bool identity_equal(const windows_identity *left,
                           const windows_identity *right) {
  return left != NULL && right != NULL &&
         left->volume_serial == right->volume_serial &&
         memcmp(left->file_id, right->file_id, sizeof(left->file_id)) == 0;
}

static bool write_identity_token(char *destination, size_t capacity, char prefix,
                                 const windows_identity *identity,
                                 size_t *length) {
  static const char hex[] = "0123456789abcdef";
  if (destination == NULL || identity == NULL || length == NULL ||
      capacity < WINDOWS_IDENTITY_TOKEN_LENGTH)
    return false;
  destination[0] = prefix;
  for (size_t index = 0u; index < 16u; index += 1u) {
    const unsigned int shift = (unsigned int)((15u - index) * 4u);
    destination[1u + index] =
        hex[(size_t)((identity->volume_serial >> shift) & UINT64_C(0x0f))];
    destination[18u + (2u * index)] =
        hex[(size_t)((identity->file_id[index] >> 4u) & 0x0fu)];
    destination[19u + (2u * index)] =
        hex[(size_t)(identity->file_id[index] & 0x0fu)];
  }
  destination[17u] = '-';
  *length = WINDOWS_IDENTITY_TOKEN_LENGTH;
  return true;
}

static bool write_tokens(w_seed_ephemeral_provider_token_buffers *tokens,
                         const windows_identity *root_identity,
                         const windows_identity *source_identity,
                         w_seed_ephemeral_provider_observation *observation) {
  if (tokens == NULL || root_identity == NULL || source_identity == NULL ||
      observation == NULL || tokens->provider_id == NULL ||
      tokens->root_token == NULL || tokens->source_provider_owner_token == NULL ||
      tokens->canonical_token == NULL ||
      tokens->provider_id_capacity < WINDOWS_PROVIDER_ID_LENGTH)
    return false;
  (void)memcpy(tokens->provider_id, windows_provider_id,
               WINDOWS_PROVIDER_ID_LENGTH);
  if (!write_identity_token(tokens->root_token, tokens->root_token_capacity, 'r',
                            root_identity, &observation->root_token_length) ||
      !write_identity_token(tokens->source_provider_owner_token,
                            tokens->source_provider_owner_token_capacity, 'o',
                            root_identity,
                            &observation->source_provider_owner_token_length) ||
      !write_identity_token(tokens->canonical_token,
                            tokens->canonical_token_capacity, 'c',
                            source_identity,
                            &observation->canonical_token_length))
    return false;
  observation->provider_id_length = WINDOWS_PROVIDER_ID_LENGTH;
  return true;
}

static w_seed_ephemeral_provider_backend_status close_raw(HANDLE handle) {
  if (!native_handle_valid(handle)) return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  return NtClose(handle) == W_STATUS_SUCCESS
             ? W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK
             : W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO;
}

static w_seed_ephemeral_provider_backend_status open_volume_root(
    WCHAR drive, HANDLE *handle, windows_identity *identity) {
  WCHAR volume_path[WINDOWS_WIDE_PREFIX_CAPACITY] = {
      (WCHAR)L'\\', (WCHAR)L'?', (WCHAR)L'?', (WCHAR)L'\\', drive,
      (WCHAR)L':', (WCHAR)L'\\', (WCHAR)0};
  const NTSTATUS status = nt_create(
      NULL, volume_path, FILE_READ_ATTRIBUTES | SYNCHRONIZE,
      FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, handle);
  if (status != W_STATUS_SUCCESS)
    return ntstatus_status(status);
  const w_seed_ephemeral_provider_backend_status query =
      query_entry(*handle, true, identity);
  if (query != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) {
    (void)close_raw(*handle);
    *handle = NULL;
  }
  return query;
}

static w_seed_ephemeral_provider_backend_status duplicate_base(
    const w_seed_ephemeral_provider_windows_context *context, HANDLE *duplicate,
    windows_identity *identity) {
  if (context == NULL || duplicate == NULL || identity == NULL ||
      !context->initialized || !native_handle_valid(native_from_value(context->base_handle)))
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  *duplicate = NULL;
  if (!DuplicateHandle(GetCurrentProcess(), native_from_value(context->base_handle),
                       GetCurrentProcess(), duplicate, 0u, FALSE,
                       DUPLICATE_SAME_ACCESS))
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO;
  const w_seed_ephemeral_provider_backend_status query =
      query_entry(*duplicate, true, identity);
  if (query != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) {
    (void)close_raw(*duplicate);
    *duplicate = NULL;
  }
  return query;
}

static w_seed_ephemeral_provider_backend_status open_parent(
    w_seed_ephemeral_provider_windows_context *context,
    const windows_path *path, HANDLE *parent, windows_identity *identity) {
  if (context == NULL || path == NULL || parent == NULL || identity == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  *parent = NULL;
  w_seed_ephemeral_provider_backend_status status =
      path->absolute
          ? open_volume_root(path->text[0], parent, identity)
          : duplicate_base(context, parent, identity);
  if (status != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) return status;
  size_t component_start = path->first_component;
  for (size_t index = path->first_component; index <= path->leaf_start;
       index += 1u) {
    if (index != path->leaf_start && path->text[index] != (WCHAR)L'\\')
      continue;
    if (index == path->leaf_start) break;
    WCHAR component[WINDOWS_MAX_WIDE_PATH];
    if (!copy_wide_component(path->text, component_start, index, component)) {
      (void)close_raw(*parent);
      *parent = NULL;
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
    }
    HANDLE next = NULL;
    const NTSTATUS open_status = nt_create(
        *parent, component, FILE_READ_ATTRIBUTES | SYNCHRONIZE,
        FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, &next);
    if (open_status != W_STATUS_SUCCESS) {
      status = ntstatus_status(open_status);
      (void)close_raw(*parent);
      *parent = NULL;
      return status;
    }
    windows_identity next_identity;
    status = query_entry(next, true, &next_identity);
    if (status != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) {
      (void)close_raw(next);
      (void)close_raw(*parent);
      *parent = NULL;
      return status;
    }
    (void)close_raw(*parent);
    *parent = next;
    *identity = next_identity;
    component_start = index + 1u;
  }
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
}

static w_seed_ephemeral_provider_backend_status open_file(
    HANDLE parent, const WCHAR *path, HANDLE *handle, windows_identity *identity) {
  if (handle == NULL || identity == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  *handle = NULL;
  NTSTATUS status = nt_create(
      parent, path, FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
      FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, handle);
  if (status == W_STATUS_FILE_IS_A_DIRECTORY) {
    /* A final directory reparse point can report FILE_IS_A_DIRECTORY before
     * its tag is observable with the file type constraint. Reopen only this
     * case as a directory so the defensive tag query can classify it as
     * SYMLINK. Ordinary directories still map to UNSUPPORTED. */
    HANDLE reparse_probe = NULL;
    const NTSTATUS probe_status = nt_create(
        parent, path, FILE_READ_ATTRIBUTES | SYNCHRONIZE,
        FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, &reparse_probe);
    if (probe_status == W_STATUS_SUCCESS) {
      windows_identity probe_identity;
      const w_seed_ephemeral_provider_backend_status probe_query =
          query_entry(reparse_probe, true, &probe_identity);
      (void)close_raw(reparse_probe);
      return probe_query == W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK
                 ? W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED
                 : probe_query;
    }
    status = probe_status;
  }
  if (status != W_STATUS_SUCCESS) return ntstatus_status(status);
  const w_seed_ephemeral_provider_backend_status query =
      query_entry(*handle, false, identity);
  if (query != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) {
    (void)close_raw(*handle);
    *handle = NULL;
  }
  return query;
}

static w_seed_ephemeral_provider_backend_status read_handle(
    HANDLE handle, const windows_identity *expected, uint8_t *bytes,
    size_t capacity, size_t *written) {
  if (written == NULL || expected == NULL || !native_handle_valid(handle))
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  *written = 0u;
  windows_identity before;
  w_seed_ephemeral_provider_backend_status status =
      query_entry(handle, false, &before);
  if (status != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK)
    return status;
  if (!identity_equal(&before, expected))
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO;
  const size_t length = before.byte_count;
  if (length > capacity) {
    *written = length;
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_CAPACITY;
  }
  if (length != 0u && bytes == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  LARGE_INTEGER offset;
  offset.QuadPart = 0;
  if (!SetFilePointerEx(handle, offset, NULL, FILE_BEGIN))
    return win32_error_status(GetLastError());
  size_t cursor = 0u;
  while (cursor < length) {
    const size_t remaining = length - cursor;
    const DWORD request = (DWORD)(remaining > (size_t)UINT32_MAX
                                      ? (size_t)UINT32_MAX
                                      : remaining);
    DWORD count = 0u;
    if (!ReadFile(handle, bytes + cursor, request, &count, NULL))
      return win32_error_status(GetLastError());
    if (count == 0u) return W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO;
    if ((size_t)count > remaining)
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
    cursor += (size_t)count;
  }
  windows_identity after;
  status = query_entry(handle, false, &after);
  if (status != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) return status;
  if (!identity_equal(&after, expected) || after.byte_count != length)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO;
  *written = length;
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
}

static bool write_root_leaf(
    w_seed_ephemeral_provider_windows_context *context,
    const windows_path *path) {
  if (context == NULL || path == NULL || path->length - path->leaf_start >=
                                      WINDOWS_MAX_WIDE_PATH)
    return false;
  const size_t length = path->length - path->leaf_start;
  (void)memcpy(context->root_leaf, path->text + path->leaf_start,
               length * sizeof(uint16_t));
  context->root_leaf[length] = 0u;
  context->root_leaf_length = length;
  return true;
}

static void clear_root_leaf(
    w_seed_ephemeral_provider_windows_context *context) {
  if (context == NULL) return;
  context->root_leaf_length = 0u;
  context->root_leaf[0] = 0u;
}

static w_seed_ephemeral_provider_backend_status windows_open_root(
    void *context_value, w_seed_byte_view root_path,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_handle *root_handle,
    w_seed_ephemeral_provider_handle *root_source_handle,
    w_seed_ephemeral_provider_observation *observation) {
  w_seed_ephemeral_provider_windows_context *context = context_value;
  if (root_handle != NULL) root_handle->value = (uintptr_t)0u;
  if (root_source_handle != NULL) root_source_handle->value = (uintptr_t)0u;
  if (observation != NULL) (void)memset(observation, 0, sizeof(*observation));
  if (context == NULL || tokens == NULL || root_handle == NULL ||
      root_source_handle == NULL || observation == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  if (!context->initialized || !context->ntcreatefile_supported)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
  windows_path path;
  w_seed_ephemeral_provider_backend_status status =
      convert_root_path(root_path, &path);
  if (status != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) return status;
  HANDLE parent = NULL;
  windows_identity parent_identity;
  status = open_parent(context, &path, &parent, &parent_identity);
  if (status != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) return status;
  WCHAR leaf[WINDOWS_MAX_WIDE_PATH];
  if (!copy_wide_component(path.text, path.leaf_start, path.length, leaf)) {
    (void)close_raw(parent);
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  }
  HANDLE source = NULL;
  windows_identity source_identity;
  status = open_file(parent, leaf, &source, &source_identity);
  if (status != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) {
    (void)close_raw(parent);
    return status;
  }
  w_seed_ephemeral_provider_handle local_root = {(uintptr_t)0u};
  w_seed_ephemeral_provider_handle local_source = {(uintptr_t)0u};
  if (!allocate_slot(context, parent, &parent_identity,
                     W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_ROOT_DIRECTORY,
                     &local_root)) {
    (void)close_raw(source);
    (void)close_raw(parent);
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO;
  }
  if (!allocate_slot(context, source, &source_identity,
                     W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_ROOT_SOURCE,
                     &local_source)) {
    w_seed_ephemeral_provider_windows_slot *root_slot = slot_for_handle(
        context, local_root,
        W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_ROOT_DIRECTORY);
    release_slot(root_slot);
    (void)close_raw(source);
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO;
  }
  if (!write_tokens(tokens, &parent_identity, &source_identity, observation)) {
    w_seed_ephemeral_provider_windows_slot *source_slot = slot_for_handle(
        context, local_source,
        W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_ROOT_SOURCE);
    w_seed_ephemeral_provider_windows_slot *root_slot = slot_for_handle(
        context, local_root,
        W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_ROOT_DIRECTORY);
    release_slot(source_slot);
    release_slot(root_slot);
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  }
  if (!write_root_leaf(context, &path)) {
    w_seed_ephemeral_provider_windows_slot *source_slot = slot_for_handle(
        context, local_source,
        W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_ROOT_SOURCE);
    w_seed_ephemeral_provider_windows_slot *root_slot = slot_for_handle(
        context, local_root,
        W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_ROOT_DIRECTORY);
    release_slot(source_slot);
    release_slot(root_slot);
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  }
  observation->opened = true;
  observation->containment_inside = true;
  observation->symlink = W_SEED_EPHEMERAL_GRAPH_SYMLINK_NONE;
  *root_handle = local_root;
  *root_source_handle = local_source;
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
}

static w_seed_ephemeral_provider_backend_status windows_open_source(
    void *context_value, w_seed_ephemeral_provider_handle root_handle,
    w_seed_frontend_text source_id,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_handle *source_handle,
    w_seed_ephemeral_provider_observation *observation) {
  w_seed_ephemeral_provider_windows_context *context = context_value;
  if (source_handle != NULL) source_handle->value = (uintptr_t)0u;
  if (observation != NULL) (void)memset(observation, 0, sizeof(*observation));
  if (context == NULL || tokens == NULL || source_handle == NULL ||
      observation == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  if (!context->initialized || !context->ntcreatefile_supported)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
  w_seed_ephemeral_provider_windows_slot *root_slot = slot_for_handle(
      context, root_handle,
      W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_ROOT_DIRECTORY);
  if (root_slot == NULL) return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  WCHAR path[WINDOWS_MAX_WIDE_PATH];
  if (!copy_source_id(source_id, path))
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  HANDLE source = NULL;
  windows_identity source_identity;
  const w_seed_ephemeral_provider_backend_status status = open_file(
      native_from_value(root_slot->native_handle), path, &source,
      &source_identity);
  if (status != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) return status;
  w_seed_ephemeral_provider_handle local_source = {(uintptr_t)0u};
  if (!allocate_slot(context, source, &source_identity,
                     W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_CHILD_SOURCE,
                     &local_source)) {
    (void)close_raw(source);
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO;
  }
  windows_identity root_identity;
  root_identity.handle = native_from_value(root_slot->native_handle);
  root_identity.volume_serial = root_slot->volume_serial;
  (void)memcpy(root_identity.file_id, root_slot->file_id,
               sizeof(root_identity.file_id));
  root_identity.byte_count = 0u;
  if (!write_tokens(tokens, &root_identity, &source_identity, observation)) {
    w_seed_ephemeral_provider_windows_slot *source_slot = slot_for_handle(
        context, local_source,
        W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_CHILD_SOURCE);
    release_slot(source_slot);
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  }
  observation->opened = true;
  observation->containment_inside = true;
  observation->symlink = W_SEED_EPHEMERAL_GRAPH_SYMLINK_NONE;
  *source_handle = local_source;
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
}

static w_seed_ephemeral_provider_backend_status windows_read_source(
    void *context_value, w_seed_ephemeral_provider_handle source_handle,
    uint8_t *bytes, size_t capacity, size_t *written) {
  w_seed_ephemeral_provider_windows_context *context = context_value;
  if (context == NULL || written == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  w_seed_ephemeral_provider_windows_slot *source_slot = slot_for_handle(
      context, source_handle,
      W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_ROOT_SOURCE);
  if (source_slot == NULL)
    source_slot = slot_for_handle(
        context, source_handle,
        W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_CHILD_SOURCE);
  if (source_slot == NULL) return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  windows_identity expected;
  expected.handle = native_from_value(source_slot->native_handle);
  expected.volume_serial = source_slot->volume_serial;
  (void)memcpy(expected.file_id, source_slot->file_id, sizeof(expected.file_id));
  expected.byte_count = 0u;
  return read_handle(expected.handle, &expected, bytes, capacity, written);
}

static w_seed_ephemeral_provider_backend_status windows_revalidate_source(
    void *context_value, w_seed_ephemeral_provider_handle root_handle,
    w_seed_ephemeral_provider_handle source_handle,
    w_seed_frontend_text source_id,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_observation *observation, uint8_t *bytes,
    size_t capacity, size_t *written) {
  w_seed_ephemeral_provider_windows_context *context = context_value;
  if (written != NULL) *written = 0u;
  if (context == NULL || tokens == NULL || observation == NULL ||
      written == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  (void)memset(observation, 0, sizeof(*observation));
  if (!context->initialized || !context->ntcreatefile_supported)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED;
  w_seed_ephemeral_provider_windows_slot *root_slot = slot_for_handle(
      context, root_handle,
      W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_ROOT_DIRECTORY);
  if (root_slot == NULL) return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  windows_identity current_root;
  w_seed_ephemeral_provider_backend_status status = query_entry(
      native_from_value(root_slot->native_handle), true, &current_root);
  if (status != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) return status;
  windows_identity expected_root;
  expected_root.handle = current_root.handle;
  expected_root.volume_serial = root_slot->volume_serial;
  (void)memcpy(expected_root.file_id, root_slot->file_id,
               sizeof(expected_root.file_id));
  expected_root.byte_count = 0u;
  if (!identity_equal(&current_root, &expected_root))
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO;
  const bool root_source =
      slot_for_handle(context, source_handle,
                      W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_ROOT_SOURCE) != NULL;
  w_seed_ephemeral_provider_windows_slot *source_slot = root_source
      ? slot_for_handle(context, source_handle,
                        W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_ROOT_SOURCE)
      : slot_for_handle(context, source_handle,
                        W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_CHILD_SOURCE);
  if (source_slot == NULL) return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  WCHAR path[WINDOWS_MAX_WIDE_PATH];
  if (root_source) {
    if (context->root_leaf_length == 0u ||
        context->root_leaf_length >= WINDOWS_MAX_WIDE_PATH)
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
    (void)memcpy(path, context->root_leaf,
                 (context->root_leaf_length + 1u) * sizeof(uint16_t));
  } else if (!copy_source_id(source_id, path)) {
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  }
  HANDLE reopened = NULL;
  windows_identity reopened_identity;
  status = open_file(native_from_value(root_slot->native_handle), path,
                      &reopened, &reopened_identity);
  if (status != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) return status;
  status = read_handle(reopened, &reopened_identity, bytes, capacity, written);
  if (status != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) {
    (void)close_raw(reopened);
    return status;
  }
  if (!write_tokens(tokens, &current_root, &reopened_identity, observation)) {
    (void)close_raw(reopened);
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  }
  (void)close_raw(reopened);
  observation->opened = true;
  observation->containment_inside = true;
  observation->symlink = W_SEED_EPHEMERAL_GRAPH_SYMLINK_NONE;
  (void)source_slot;
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
}

static void windows_close_source(
    void *context_value, w_seed_ephemeral_provider_handle source_handle) {
  w_seed_ephemeral_provider_windows_context *context = context_value;
  if (context == NULL) return;
  w_seed_ephemeral_provider_windows_slot *slot = slot_for_handle(
      context, source_handle,
      W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_ROOT_SOURCE);
  if (slot == NULL)
    slot = slot_for_handle(context, source_handle,
                           W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_CHILD_SOURCE);
  release_slot(slot);
}

static void windows_close_root(
    void *context_value, w_seed_ephemeral_provider_handle root_handle) {
  w_seed_ephemeral_provider_windows_context *context = context_value;
  if (context == NULL) return;
  w_seed_ephemeral_provider_windows_slot *slot = slot_for_handle(
      context, root_handle,
      W_SEED_EPHEMERAL_PROVIDER_WINDOWS_SLOT_ROOT_DIRECTORY);
  release_slot(slot);
  if (slot != NULL) clear_root_leaf(context);
}

static bool probe_ntcreatefile(w_seed_ephemeral_provider_windows_context *context) {
  if (context == NULL) return false;
  HANDLE probe = NULL;
  const NTSTATUS status = nt_create(
      native_from_value(context->base_handle), L"",
      FILE_READ_ATTRIBUTES | SYNCHRONIZE,
      FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, &probe);
  if (status != W_STATUS_SUCCESS) return false;
  windows_identity identity;
  const w_seed_ephemeral_provider_backend_status query =
      query_entry(probe, true, &identity);
  const w_seed_ephemeral_provider_backend_status close_status = close_raw(probe);
  return query == W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK &&
         close_status == W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
}

bool w_seed_ephemeral_provider_windows_init(
    w_seed_ephemeral_provider_windows_context *context, uintptr_t base_handle) {
  if (context == NULL) return false;
  (void)memset(context, 0, sizeof(*context));
  context->base_handle = base_handle;
  context->next_generation = 1u;
  if (!native_handle_valid(native_from_value(base_handle))) return false;
  windows_identity base_identity;
  if (query_entry(native_from_value(base_handle), true, &base_identity) !=
      W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK)
    return false;
  context->initialized = true;
  context->ntcreatefile_supported = probe_ntcreatefile(context);
  return true;
}

bool w_seed_ephemeral_provider_windows_backend(
    w_seed_ephemeral_provider_windows_context *context,
    w_seed_ephemeral_provider_backend *backend) {
  if (context == NULL || backend == NULL || !context->initialized)
    return false;
  (void)memset(backend, 0, sizeof(*backend));
  backend->context = context;
  backend->open_root = windows_open_root;
  backend->open_source = windows_open_source;
  backend->read_source = windows_read_source;
  backend->revalidate_source = windows_revalidate_source;
  backend->close_source = windows_close_source;
  backend->close_root = windows_close_root;
  backend->metadata = windows_metadata();
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
  if (root_source_handle != NULL) root_source_handle->value = (uintptr_t)0u;
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

static void unsupported_close_root(
    void *context, w_seed_ephemeral_provider_handle root_handle) {
  (void)context;
  (void)root_handle;
}

bool w_seed_ephemeral_provider_windows_init(
    w_seed_ephemeral_provider_windows_context *context, uintptr_t base_handle) {
  if (context == NULL) return false;
  (void)memset(context, 0, sizeof(*context));
  context->base_handle = base_handle;
  context->next_generation = 1u;
  context->initialized = true;
  context->ntcreatefile_supported = false;
  return true;
}

bool w_seed_ephemeral_provider_windows_backend(
    w_seed_ephemeral_provider_windows_context *context,
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
  backend->metadata = windows_metadata();
  return true;
}

#endif
