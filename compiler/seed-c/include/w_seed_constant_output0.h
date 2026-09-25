#ifndef W_SEED_CONSTANT_OUTPUT0_H
#define W_SEED_CONSTANT_OUTPUT0_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_hir0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A successful evaluation certifies one complete default-Unit-entry
 * execution. The supported evaluator subset is deliberately smaller than
 * NativeSubset0: fixed i64 literals, checked arithmetic and comparisons;
 * Bool literals and selected acyclic branches; String literals/interpolation
 * over i64, Bool, and String; immutable bindings; direct synchronous local
 * calls; and flat value-copy tuple/value-struct products with i64 fields.
 * The only accepted effect is the exact `native-process@1` Console
 * `print(String): Unit` output. Entry adapters, runtime inputs, unsupported
 * calls/values, selected paths that repeat a block, joins/block arguments,
 * faults, and cleanup obligations decline evaluation. Unreached blocks are not
 * interpreted. */
#define W_SEED_CONSTANT_OUTPUT0_MAX_BYTES (4u * 1024u)

typedef struct {
  uint8_t stdout_bytes[W_SEED_CONSTANT_OUTPUT0_MAX_BYTES];
  size_t stdout_length;
  int32_t success_exit_status;
  /* True if the selected reachable execution consumed at least one flat
   * tuple/value-struct construction or projection. MLIR adopts only such
   * evaluations in this bounded optimization slice. */
  bool evaluated_flat_product;
  uint8_t hir_semantic_digest[32];
  uint8_t hir_provenance_digest[32];
} w_seed_constant_output0_result;

/* Evaluate the whole reachable default-entry root from independently verified
 * HIR. False means unsupported or not statically provable; in that case the
 * caller-owned result is left byte-for-byte unchanged. */
bool w_seed_constant_output0_evaluate(const w_seed_hir0_program *program,
                                      const w_seed_hir0_result *hir_result,
                                      w_seed_constant_output0_result *result);

#ifdef __cplusplus
}
#endif

#endif
