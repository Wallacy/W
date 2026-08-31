#ifndef W_SEED_MANIFEST_LINUX_H
#define W_SEED_MANIFEST_LINUX_H

#include <stdbool.h>

#include "w_seed_manifest.h"
#include "w_seed_owner_guard_linux.h"

#ifdef __cplusplus
extern "C" {
#endif

/* On Linux, bind MAN0 read-only to the original live OWN0 context and guard.
 * The descriptor reads literal build.w entries through retained handles.
 * Outside Linux, the factory creates only a fail-closed direct-test descriptor;
 * guarded run rejects the non-live guard before it can call the callback. */
bool w_seed_manifest_linux_backend(
    const w_seed_owner_guard *guard,
    const w_seed_owner_guard_linux_context *context,
    w_seed_manifest_backend *backend);

#ifdef __cplusplus
}
#endif

#endif
