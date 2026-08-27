#ifndef W_SEED_CHECK_HOST_H
#define W_SEED_CHECK_HOST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_ephemeral_provider.h"

#if defined(__linux__)
#include "w_seed_ephemeral_provider_linux.h"
#elif defined(_WIN32)
#include "w_seed_ephemeral_provider_windows.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  W_SEED_CHECK_HOST_OK = 0,
  W_SEED_CHECK_HOST_INVALID,
  W_SEED_CHECK_HOST_UNSUPPORTED,
  W_SEED_CHECK_HOST_IO,
} w_seed_check_host_status;

/* The logical identity is a view into the caller's physical path.  The
 * directory portion is never canonicalized or validated as an identifier. */
w_seed_check_host_status w_seed_check_root_source_id(
    const char *path, size_t length, w_seed_frontend_text *source_id);

/* Owns only the base directory opened by this module.  The platform adapter
 * borrows that descriptor/handle and closes only the handles it opens for
 * individual sources. */
typedef struct {
  w_seed_ephemeral_provider_backend backend;
  bool initialized;
  bool open;
#if defined(__linux__)
  int base_dir_fd;
  w_seed_ephemeral_provider_linux_context linux_context;
#elif defined(_WIN32)
  uintptr_t base_handle;
  w_seed_ephemeral_provider_windows_context windows_context;
#endif
} w_seed_check_host;

/* The first call must receive a zero-initialized object. A live initialized
 * object is rejected without being overwritten. */
w_seed_check_host_status w_seed_check_host_init(w_seed_check_host *host);

w_seed_check_host_status w_seed_check_host_open(
    w_seed_check_host *host, w_seed_ephemeral_provider_backend *backend);

/* Idempotent. A repeated close performs no native close and is safe. */
void w_seed_check_host_close(w_seed_check_host *host);

#ifdef __cplusplus
}
#endif

#endif
