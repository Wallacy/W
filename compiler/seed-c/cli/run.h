#ifndef W_SEED_RUN_CLI_H
#define W_SEED_RUN_CLI_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define W_SEED_RUN_MAX_ARGUMENTS 256u
#define W_SEED_NATIVE_TARGET_LINUX "x86_64-unknown-linux-gnu"
#define W_SEED_NATIVE_TARGET_WINDOWS "x86_64-pc-windows-msvc"

typedef enum {
  W_SEED_RUN_COMPILE_PROFILE_DEV = 0,
  W_SEED_RUN_COMPILE_PROFILE_RELEASE = 1
} w_seed_run_compile_profile;

/* Zero deliberately preserves the current static-PIE recipe for callers that
 * leave this option uninitialized by older designated initializers. */
typedef enum {
  W_SEED_RUN_COMPILE_PIE_ON = 0,
  W_SEED_RUN_COMPILE_PIE_OFF = 1
} w_seed_run_compile_pie_mode;

typedef struct {
  const char *path;
  size_t argument_count;
  char *const *arguments;
} w_seed_run_request;

typedef struct {
  const char *source_path;
  const char *target;
  const char *directory;
  const char *artifact_path;
  w_seed_run_compile_profile profile;
  w_seed_run_compile_pie_mode pie_mode;
  /* Internal w build audit lane: retain bounded intermediates and manifest in
   * the caller-owned private staging directory. Never set for w run. */
  bool retain_audit_trace;
} w_seed_run_compile_request;

/* Parse only the public run grammar. Argument strings remain borrowed from
 * argv and are forwarded unchanged after an optional -- separator. */
bool w_seed_run_parse(int argc, char **argv, w_seed_run_request *request);

/* Execute one parsed request. Linux uses its pinned native path; an explicitly
 * configured Windows build uses the bounded MLIR0-to-PE path. Other hosts
 * return the documented unsupported status without side effects. */
int w_seed_run_execute(const w_seed_run_request *request);

/* Shared bounded source-to-native compilation used by run and build. Windows
 * accepts both its native PE target and the finite Linux x86-64 cross target;
 * the latter links the authored WRT0 closure with the pinned sibling ld.lld.
 * The caller owns directory and artifact_path; ordinary success keeps only
 * artifact_path, while the internal audit lane keeps its fixed trace inventory
 * until w build copies it to the caller-selected destination. */
int w_seed_run_compile(const w_seed_run_compile_request *request);

/* Remove a compiled artifact and its private directory. */
bool w_seed_run_cleanup_compiled(const char *directory,
                                 const char *artifact_path);

#ifdef __cplusplus
}
#endif

#endif
