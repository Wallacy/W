#ifndef W_SEED_WRT0_H
#define W_SEED_WRT0_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Implemented seed runtime closures. This is evidence coverage, not the
 * closed universe of W targets. A target is added here only when its exact
 * runtime bytes and executable-link contract are implemented and gated. */
typedef enum {
  W_SEED_WRT0_TARGET_LINUX_X86_64 = 1,
} w_seed_wrt0_target;

/* Target-neutral facts derived from an admitted program. UNKNOWN is zero so
 * zero-initialized or future values conservatively retain the full startup. */
typedef enum {
  W_SEED_RUNTIME_REQUIREMENTS_UNKNOWN = 0,
  W_SEED_RUNTIME_REQUIREMENTS_NONE,
  W_SEED_RUNTIME_REQUIREMENTS_PROCESS_ARGUMENTS,
} w_seed_runtime_requirements;

typedef struct {
  const uint8_t *llvm_ir;
  size_t llvm_ir_length;
} w_seed_wrt0_artifact;

/* Returns immutable authored LLVM IR for one implemented target closure.
 * Only proven NONE selects the no-arguments startup; UNKNOWN and unrecognized
 * values retain the full startup. Failure leaves artifact unchanged. The
 * returned storage has static lifetime and excludes a trailing C NUL from
 * llvm_ir_length. */
bool w_seed_wrt0_get(w_seed_wrt0_target target,
                     w_seed_runtime_requirements requirements,
                     w_seed_wrt0_artifact *artifact);

#endif
