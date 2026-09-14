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

typedef struct {
  const uint8_t *llvm_ir;
  size_t llvm_ir_length;
} w_seed_wrt0_artifact;

/* Returns immutable authored LLVM IR for one implemented target closure.
 * Failure leaves artifact unchanged. The returned storage has static
 * lifetime and excludes a trailing C NUL from llvm_ir_length. */
bool w_seed_wrt0_get(w_seed_wrt0_target target,
                     w_seed_wrt0_artifact *artifact);

#endif
