#ifndef W_SEED_MANIFEST_WINDOWS_H
#define W_SEED_MANIFEST_WINDOWS_H

#include <stdbool.h>

#include "w_seed_manifest.h"
#include "w_seed_owner_guard_windows.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MAN0 has no Windows read capability in this cut. The factory exposes a
 * fail-closed descriptor for direct adapter tests; its callback always returns
 * UNSUPPORTED without opening or reading a file and without changing
 * destination bytes. Guarded run returns INVALID/VALIDATE for the non-live
 * OWN0 Windows guard during preflight and never reaches this callback. */
bool w_seed_manifest_windows_backend(
    const w_seed_owner_guard *guard,
    const w_seed_owner_guard_windows_context *context,
    w_seed_manifest_backend *backend);

#ifdef __cplusplus
}
#endif

#endif
