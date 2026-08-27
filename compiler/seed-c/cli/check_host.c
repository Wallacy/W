#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif

#include "check_host.h"

#include <stdint.h>
#include <string.h>

#if defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static bool path_separator(uint8_t byte) {
#if defined(_WIN32)
  return byte == (uint8_t)'/' || byte == (uint8_t)'\\';
#else
  return byte == (uint8_t)'/';
#endif
}

static bool identifier_start(uint8_t byte) {
  return (byte >= (uint8_t)'A' && byte <= (uint8_t)'Z') ||
         (byte >= (uint8_t)'a' && byte <= (uint8_t)'z') ||
         byte == (uint8_t)'_';
}

static bool identifier_tail(uint8_t byte) {
  return identifier_start(byte) ||
         (byte >= (uint8_t)'0' && byte <= (uint8_t)'9');
}

w_seed_check_host_status w_seed_check_root_source_id(
    const char *path, size_t length, w_seed_frontend_text *source_id) {
  if (source_id != NULL) *source_id = (w_seed_frontend_text){NULL, 0u};
  if (path == NULL || length == 0u ||
      length > (size_t)W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES ||
      source_id == NULL)
    return W_SEED_CHECK_HOST_INVALID;

  size_t basename_start = 0u;
  for (size_t index = 0u; index < length; index += 1u) {
    const uint8_t byte = (uint8_t)path[index];
    if (byte == 0u) return W_SEED_CHECK_HOST_INVALID;
    if (path_separator(byte)) basename_start = index + 1u;
  }
  if (basename_start >= length) return W_SEED_CHECK_HOST_INVALID;
  const size_t basename_length = length - basename_start;
  if (basename_length < 3u ||
      (uint8_t)path[length - 2u] != (uint8_t)'.' ||
      (uint8_t)path[length - 1u] != (uint8_t)'w')
    return W_SEED_CHECK_HOST_INVALID;

  const size_t stem_length = basename_length - 2u;
  if (stem_length == 0u) return W_SEED_CHECK_HOST_INVALID;
  for (size_t index = 0u; index < stem_length; index += 1u) {
    const uint8_t byte = (uint8_t)path[basename_start + index];
    if (byte >= 0x80u) return W_SEED_CHECK_HOST_UNSUPPORTED;
    if ((index == 0u && !identifier_start(byte)) ||
        (index != 0u && !identifier_tail(byte)))
      return W_SEED_CHECK_HOST_INVALID;
  }
  *source_id = (w_seed_frontend_text){path + basename_start, basename_length};
  return W_SEED_CHECK_HOST_OK;
}

w_seed_check_host_status w_seed_check_host_open(
    w_seed_check_host *host, w_seed_ephemeral_provider_backend *backend) {
  if (host == NULL || backend == NULL || !host->initialized || host->open)
    return W_SEED_CHECK_HOST_INVALID;
#if defined(__linux__)
  const int base_fd = open(".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (base_fd < 0) return W_SEED_CHECK_HOST_IO;
  if (!w_seed_ephemeral_provider_linux_init(&host->linux_context, base_fd)) {
    (void)close(base_fd);
    return W_SEED_CHECK_HOST_IO;
  }
  if (!host->linux_context.openat2_supported) {
    (void)close(base_fd);
    return W_SEED_CHECK_HOST_UNSUPPORTED;
  }
  if (!w_seed_ephemeral_provider_linux_backend(&host->linux_context,
                                               backend)) {
    (void)close(base_fd);
    return W_SEED_CHECK_HOST_IO;
  }
  host->base_dir_fd = base_fd;
  host->backend = *backend;
  host->open = true;
  return W_SEED_CHECK_HOST_OK;
#elif defined(_WIN32)
  const HANDLE base_handle = CreateFileW(
      L".", GENERIC_READ | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (base_handle == INVALID_HANDLE_VALUE) return W_SEED_CHECK_HOST_IO;
  if (!w_seed_ephemeral_provider_windows_init(
          &host->windows_context, (uintptr_t)base_handle)) {
    (void)CloseHandle(base_handle);
    return W_SEED_CHECK_HOST_IO;
  }
  if (!host->windows_context.ntcreatefile_supported) {
    (void)CloseHandle(base_handle);
    return W_SEED_CHECK_HOST_UNSUPPORTED;
  }
  if (!w_seed_ephemeral_provider_windows_backend(&host->windows_context,
                                                 backend)) {
    (void)CloseHandle(base_handle);
    return W_SEED_CHECK_HOST_IO;
  }
  host->base_handle = (uintptr_t)base_handle;
  host->backend = *backend;
  host->open = true;
  return W_SEED_CHECK_HOST_OK;
#else
  (void)backend;
  return W_SEED_CHECK_HOST_UNSUPPORTED;
#endif
}

w_seed_check_host_status w_seed_check_host_init(w_seed_check_host *host) {
  if (host == NULL || host->initialized) return W_SEED_CHECK_HOST_INVALID;
  (void)memset(host, 0, sizeof(*host));
#if defined(__linux__)
  host->base_dir_fd = -1;
#endif
  host->initialized = true;
  return W_SEED_CHECK_HOST_OK;
}

void w_seed_check_host_close(w_seed_check_host *host) {
  if (host == NULL || !host->initialized || !host->open) return;
#if defined(__linux__)
  if (host->base_dir_fd >= 0) (void)close(host->base_dir_fd);
  host->base_dir_fd = -1;
#elif defined(_WIN32)
  if (host->base_handle != (uintptr_t)0u &&
      host->base_handle != (uintptr_t)INVALID_HANDLE_VALUE)
    (void)CloseHandle((HANDLE)host->base_handle);
  host->base_handle = (uintptr_t)0u;
#endif
  (void)memset(&host->backend, 0, sizeof(host->backend));
  host->open = false;
}
