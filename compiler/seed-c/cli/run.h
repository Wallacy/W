#ifndef W_SEED_RUN_CLI_H
#define W_SEED_RUN_CLI_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define W_SEED_RUN_MAX_ARGUMENTS 256u

typedef struct {
  const char *path;
  size_t argument_count;
  char *const *arguments;
} w_seed_run_request;

/* Parse only the public run grammar. Argument strings remain borrowed from
 * argv and are forwarded unchanged after an optional -- separator. */
bool w_seed_run_parse(int argc, char **argv, w_seed_run_request *request);

/* Execute one parsed request. Linux uses its pinned native path; an explicitly
 * configured Windows build uses the bounded MLIR0-to-PE path. Other hosts
 * return the documented unsupported status without side effects. */
int w_seed_run_execute(const w_seed_run_request *request);

#ifdef __cplusplus
}
#endif

#endif
