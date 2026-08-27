#ifndef W_SEED_CHECK_H
#define W_SEED_CHECK_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Run one bounded ephemeral local graph check with caller-selected output
 * rendering. The path is an explicit root. The implementation uses private
 * storage for this one-shot call. The function is not thread-safe. */
int w_seed_check_run(const char *path, bool json);

#ifdef __cplusplus
}
#endif

#endif
