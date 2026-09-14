#ifndef W_SEED_SCALAR_EVALUATOR0_H
#define W_SEED_SCALAR_EVALUATOR0_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_hir0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One compiler-host authority for the closed pure i64 seed subset. It is not
 * a W runtime interpreter and never owns or publishes Task state. */
bool w_seed_scalar_evaluator0_checked_binary(
    w_seed_hir0_binary_operator operation, int64_t left, int64_t right,
    int64_t *result);

bool w_seed_scalar_evaluator0_evaluate_call(
    const w_seed_hir0_program *program, uint32_t call_index, size_t *budget,
    int64_t *result);

#ifdef __cplusplus
}
#endif

#endif
