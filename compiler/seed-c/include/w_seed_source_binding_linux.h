#ifndef W_SEED_SOURCE_BINDING_LINUX_H
#define W_SEED_SOURCE_BINDING_LINUX_H

#include <stdbool.h>

#include "w_seed_owner_guard_linux.h"
#include "w_seed_source_binding.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Configure the adapter-private Linux link over one live OWN0 context. On
 * non-Linux builds this creates a fail-closed stub whose callback returns
 * UNSUPPORTED and has no effects. */
bool w_seed_source_binding_linux_link(
    const w_seed_owner_guard_linux_context *context,
    w_seed_source_binding_link *link);

#ifdef __cplusplus
}
#endif

#endif
