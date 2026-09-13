#ifndef W_SEED_CHECK_H
#define W_SEED_CHECK_H

#include <stdbool.h>

#include "w_seed_native0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Run one bounded ephemeral local graph check with caller-selected output
 * rendering. The path is an explicit root. The implementation uses private
 * storage for this one-shot call. The function is not thread-safe. */
int w_seed_check_run(const char *path, bool json);

/* Acquire and validate one resolver-complete local source graph, then emit
 * the bounded native subset while the check host's borrowed sources remain
 * alive. This is an internal CLI composition seam, not a public W API. */
w_seed_native0_status w_seed_check_compile_local_graph(
    const char *path, const w_seed_mlir0_target *target,
    w_seed_native0_storage *native_storage,
    const w_seed_native0_output *native_output,
    w_seed_native0_result *native_result);

#ifdef __cplusplus
}
#endif

#endif
