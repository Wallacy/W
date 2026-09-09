#ifndef W_SEED_BUILD_CLI_H
#define W_SEED_BUILD_CLI_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const char *path;
  const char *target;
  const char *output;
} w_seed_build_request;

/* Parse only the bounded seed build grammar. All strings remain borrowed from
 * argv and no filesystem access occurs during parsing. */
bool w_seed_build_parse(int argc, char **argv,
                        w_seed_build_request *request);

/* Compile one explicit seed source to one new caller-owned executable. */
int w_seed_build_execute(const w_seed_build_request *request);

#ifdef __cplusplus
}
#endif

#endif
