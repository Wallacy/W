#ifndef W_SEED_CHECK_H
#define W_SEED_CHECK_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Run one bounded source check with caller-selected output rendering.
 * The path is explicit, and all source and frontend storage is private to
 * this one-shot implementation. The function is not thread-safe. */
int w_seed_check_run(const char *path, bool json);

#ifdef __cplusplus
}
#endif

#endif
